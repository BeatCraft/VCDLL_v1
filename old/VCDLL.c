#include "VCDLL.h"
/**
	History:
	
	2015/02/18: Ver 1.2.0 
	Windows版とインターフェスを合わせた最初のバージョン。
	静止画キャプチャには本バージョンでは対応しない。
*/
#define DEBUG_FLAG 0

#define DISABLE_PREVIEW

/* 静止画キャプチャのサポート：UVCドライバが静止画キャプチャをサポートしている場合 */
#define SUPPORT_STILL_CAPTURE

#ifdef DISABLE_PREVIEW
#define NUM_BUFFERS       2
#else
#define NUM_BUFFERS       4    	/* 通常のキャプチャ用のバッファ数 */
#endif
#define NUM_STILL_BUFFERS 4     /* still image capture用のバッファ数 */

/* カメラボード専用 Extension Unit制御用定数 */
#define BEAT_T4K37_UNIT_ID 3    /* Extension UnitのUnitID */
#define BEAT_QUERY_SIZE    2    /* Extension Unitに送るデータのサイズ(バイト) */

/** カメラボード用 selector */
#define BEAT_CTRL_LASER     0x01 /* LASERの制御 */
#define BEAT_CTRL_POWER		0x02 /* センサーON/OFF制御 */
#define BEAT_CTRL_tRDOUT    0x03 /* tRDOUTの設定 */
#define BEAT_CTRL_FLIP      0x04 /* 画像の反転の制御 */
#define BEAT_CTRL_EXPOSURE  0x05 /* 露出時間の設定 */
#define BEAT_CTRL_CAN_STILL 0x06 /* STILL可能かどうかの確認 */
#define BEAT_CTRL_STL_CNT   0x07 /* STILLキャプチャ数 */
#define BEAT_CTRL_SERIAL	0x08 /* シリアルナンバー設定 */
// 2.2.0
#define BEAT_CTRL_SENSOR_DETECTED	0x09
#define BEAT_CTRL_SENSOR_SELECT		0x0A
#define BEAT_CTRL_SENSOR_SETTING	0x0B
#define BEAT_CTRL_LASER_ONOFF		0x0C
#define BEAT_CTRL_SENSOR_REG_ADDR	0x0D
#define BEAT_CTRL_SENSOR_REG_VALUE	0x0E

/* コントロールがxuに対するものか判定 */
#define IS_BEAT_CTRL(a) \
  ((a) == BEAT_CTRL_LASER     ||                \
   (a) == BEAT_CTRL_POWER     ||                \
   (a) == BEAT_CTRL_tRDOUT    ||                \
   (a) == BEAT_CTRL_FLIP      ||                \
   (a) == BEAT_CTRL_EXPOSURE  ||                \
   (a) == BEAT_CTRL_CAN_STILL ||                \
   (a) == BEAT_CTRL_STL_CNT   ||                \
   (a) == BEAT_CTRL_SERIAL	  ||				\
   (a) == BEAT_CTRL_SENSOR_DETECTED ||			\
   (a) == BEAT_CTRL_SENSOR_SELECT	||			\
   (a) == BEAT_CTRL_SENSOR_SETTING	||			\
   (a) == BEAT_CTRL_LASER_ONOFF		||			\
   (a) == BEAT_CTRL_SENSOR_REG_ADDR	||			\
   (a) == BEAT_CTRL_SENSOR_REG_VALUE)
   

#define UVC_STILL_MAGIC 0xF03E5335 /* Still用バッファに付加する情報 */
#define UVC_STILL_IDX_MASK		(1 << 30)
#define UVC_STILL_IDX(idx)		((idx) & ~UVC_STILL_IDX_MASK)
#define UVC_STILL_REQBUF_CNT_MASK	(1 << 30)
#define UVC_STILL_REQBUF_CNT(cnt)	((cnt) & ~UVC_STILL_REQBUF_CNT_MASK)
#define UVC_STILL_FMT_FIELD_MASK	(1 << 30)

#define MIN_STILL_BUFFER_TIMEOUT	2000

static SensorDeviceInfo		_deviceInfo = { 0 };

#define v4l2_open			open
#define v4l2_close			close

#define COMPILE

typedef struct {
	pthread_t			thread_id;
	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	bool				tear_down;
	int					current_buffer_index;
} thread_param;

typedef struct {
	thread_param	preview_thread;
	uint8_t*		preview_buffer[NUM_BUFFERS];
	uint8_t*		previewRaw8_buffer;
	//uint8_t*		previewRaw10_buffer;
	
	uint8_t*		still_buffer[NUM_STILL_BUFFERS];
	uint8_t*		stillRaw10_buffer;
} vcdll_priv;

static int xioctl(int fd, int request, void * arg);

static void cond_init(thread_param* param)
{
	pthread_mutex_init(&param->mutex, NULL);
	pthread_cond_init(&param->cond, NULL);
}

static void cond_destroy(thread_param* param)
{
	pthread_mutex_destroy(&param->mutex);
	pthread_cond_destroy(&param->cond);
}

static void toTimespec(uint64_t timeout, struct timespec* spec)
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	int usec = tv.tv_usec + (timeout % 1000000);
	if (usec >= 1000000) {
		tv.tv_sec++;
		tv.tv_usec -= 1000000;
	}
	
	timeout /= 1000 *1000;
	timeout = (timeout > LONG_MAX) ? LONG_MAX : timeout;
	
	if (timeout >= LONG_MAX)
		spec->tv_sec = LONG_MAX;
	else
		spec->tv_sec = tv.tv_sec + (long)timeout;
		
	spec->tv_nsec = usec * 1000;
}

static uint64_t system_time()
{
	struct timeval date;
	
	gettimeofday(&date, NULL);
	
	return (uint64_t)((uint64_t)((uint32_t)date.tv_sec) * 1000 * 1000 +
					  (uint64_t)date.tv_usec);
	
}

static void snooze(uint64_t micro_second)
{
	struct timespec ts;
	
	ts.tv_sec  = micro_second / (1000 * 1000);
	ts.tv_nsec = (micro_second % (1000 * 1000)) * 1000;
	
	nanosleep(&ts, NULL);
}

static void makeGUID(unsigned long type, GUID* guid)
{
	static uint8_t data4[8] = { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71, };
	
	guid->Data1		= type;
	guid->Data2		= 0x0000;
	guid->Data3		= 0x0010;
	memcpy(&guid->Data4, data4, 8);
}

static unsigned long getFormatType(GUID* guid)
{
	return guid->Data1;
}

static void printGUID(GUID* guid)
{
	int i;
	fprintf(stdout, "%08lX-%04X-%04X-%02X%02X-", 
					guid->Data1, guid->Data2, guid->Data3,
					guid->Data4[0], guid->Data4[1]);
	for (i = 2; i < 8; i++) {
		fprintf(stdout, "%02X", guid->Data4[i]);
	}
	fprintf(stdout, "\n");
}

// OV24 RAW10 formar to 8bits grayscale
static void raw10to8bit(uint8_t* srcBuffer, uint8_t* dstBuffer, uint32_t width, uint32_t height)
{
	// Preview: copy higher(MSB) 8bit. remove LSB's byte
    uint8_t* srcPtr = srcBuffer;
	uint8_t *dstPtr = dstBuffer;
    
    for (int y = 0; y < height; y++) {
		dstPtr = dstBuffer + (width * y);
		srcPtr = srcBuffer + (width * 2 * y);
		for (int x = 0; x < width; x++) {
			dstPtr[x] = (uint8_t)(((srcPtr[0] >> 2) & 0x3F) | ((srcPtr[1] << 6) & 0xC0));
			srcPtr += 2;
		}
	}
}

#if 0
// IMX350 RAW10 formar to 16bits grayscale
static void raw10to16bit(uint8_t* srcBuffer, uint8_t* dstBuffer, uint32_t width, uint32_t height)
{
	uint8_t* srcPtr;
	uint16_t* dstPtr;
	uint32_t srcSpan = width + (width / 4);
	uint32_t dstSpan = width * 2;

	for (uint32_t y = 0; y < height; y++) {
		srcPtr = srcBuffer + (srcSpan * y);
		dstPtr = (uint16_t*)((uint8_t*)dstBuffer + (dstSpan * y));

		for (uint32_t x = 0; x < width / 4; x++) {
			uint8_t lsbs = srcPtr[4];
			dstPtr[0] = (srcPtr[0] << 8) | (lsbs & 0xC0);
			dstPtr[1] = (srcPtr[1] << 8) | ((lsbs << 2) & 0xC0);
			dstPtr[2] = (srcPtr[2] << 8) | ((lsbs << 4) & 0xC0);
			dstPtr[3] = (srcPtr[3] << 8) | ((lsbs << 6) & 0xC0);
			dstPtr += 4;
			srcPtr += 5;
		}
	}
}
#endif

static void* _preview_thread(void* data);
static int start_preview_thread(DevObject* self)
{
	int result;
	vcdll_priv* priv = (vcdll_priv*)self->priv;
	thread_param* param = (thread_param*)&priv->preview_thread;
	
	if (param->thread_id > 0) {
		return -EBUSY;
	}
	
	param->tear_down = false;
	
	result = pthread_create(&param->thread_id, NULL, _preview_thread, self);
	if (result != 0) {
		fprintf(stderr, "preview thread create faild. err=%d\n", result);
		return -result;
	}
	
	return 0;
}

static int stop_preview_thread(DevObject* self)
{
	int result;
	vcdll_priv* priv = (vcdll_priv*)self->priv;
	thread_param* param = (thread_param*)&priv->preview_thread;
	
	if (param->thread_id <= 0) {
		return -EPERM;
	}
	
	param->tear_down = true;
	
	result = pthread_join(param->thread_id, NULL);
	if (result != 0) {
		fprintf(stderr, "preview thread join faild. err=%d\n", result);
		return -result;
	}
	
	return 0;
}

static void* _preview_thread(void* data)
{
	DevObject* self = (DevObject*)data;
	vcdll_priv* priv = self->priv;
	thread_param* thread = &priv->preview_thread;
	
	fd_set fds;
  	struct timeval tv;
  	struct v4l2_buffer buf;
  	
#if DEBUG_FLAG
  	printf("Preview thread start\n");
#endif
	while (!thread->tear_down) {
		FD_ZERO(&fds);
  		FD_SET(self->devfd, &fds);
  		
  		tv.tv_sec  = 1;
  		tv.tv_usec = 0;
  		
  		CLEAR(buf);
  		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  		buf.memory = V4L2_MEMORY_MMAP;
  		
  		int r = select(self->devfd + 1, &fds, NULL, NULL, &tv);
  		if (r <= 0) {
  			fprintf(stderr, "Preview thread:timeout\n");
  			continue;
  		}
  		
  		r = ioctl(self->devfd, VIDIOC_DQBUF, &buf);
  		if (r < 0) {
  			fprintf(stderr, "Preview thread:VIDIOC_DQBUF failed(%d)\n", errno);
  			continue;
  		}
  		
  		if (self->currentImageSize == buf.bytesused) {
  			memcpy(priv->preview_buffer[buf.index], self->buffers[buf.index].start, self->currentImageSize);
  			thread->current_buffer_index = buf.index;
  			pthread_cond_signal(&thread->cond);
  			//fprintf(stdout, "Preview received: %d\n", buf.bytesused);
  		} else {
  			/*
  			fprintf(stderr, "Preview thread: Bad image size: %d/%lu\n", 
  								buf.bytesused, self->currentImageSize);
  			*/
  		}
  		
  		if(-1 == ioctl(self->devfd, VIDIOC_QBUF, &buf)) {
    		fprintf(stderr, "Preview thread: VIDIOC_QBUF failed(%d)\n", errno);
    	}
	}
#if DEBUG_FLAG
	printf("Preview thread exit\n");
#endif
	return (void*)0;
}

static int stream_on(DevObject* self)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	return xioctl(self->devfd, VIDIOC_STREAMON, &type);
}

static int stream_off(DevObject* self)
{
	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	xioctl(self->devfd, VIDIOC_STREAMOFF, &type);
}
/****************************************************
  static な関数の実装 (ライブラリ内でのみ使用)
 ***************************************************/
#ifndef COMPILE
{
#endif
/**
   ioctl()を返り値がEINTRの間リトライ

   @param fd デバイス(カメラ)のファイルディスクリプタ
   @param request デバイスに対するリクエスト番号
   @param arg デバイスに渡すデータ
*/
static int xioctl(int fd, int request, void * arg) 
{
	int r;

  	do {
    	r = ioctl(fd, request, arg);
  	} while(-1 == r && EINTR == errno);

  	return r;
}

/**
   errnoの内容を出力して異常終了
   @param s エラー内容の文字列
 */
static void errno_exit(const char * s) 
{
  	fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
  	exit(EXIT_FAILURE);
}

/**
   Extension Unitから設定値を取得する

   @param data 取得した設定値を格納
   @param selector 取得する設定項目のインデックス (BEAT_CTRL_LASERなど)
   @param query 取得する値の種類 (UVC_GET_CURなど)
*/
static bool get_xu_value(int fd, long* data,
                         const unsigned char selector,
                         const unsigned char query) 
{
  	struct uvc_xu_control_query xqry;

  	if(!IS_BEAT_CTRL(selector))
    	return false;

  	if(query != UVC_GET_CUR  &&
       query != UVC_GET_MIN  &&
       query != UVC_GET_MAX  &&
       query != UVC_GET_RES  &&
       query != UVC_GET_LEN  &&
       /* query != UVC_GET_INFO &&  */
       query != UVC_GET_DEF)
    		return false;

  	memset(&xqry, 0, sizeof(xqry));

  	xqry.unit     = BEAT_T4K37_UNIT_ID;
  	xqry.selector = selector;
  	xqry.query    = query;
  	xqry.size     = BEAT_QUERY_SIZE;
  	xqry.data     = (__u8*)malloc(BEAT_QUERY_SIZE);

  	if(-1 == ioctl(fd, UVCIOC_CTRL_QUERY, &xqry)) {
  		fprintf(stderr, "UVCIOC_CTRL_QUERY failed. err=%d\n", errno);
  		free(xqry.data);
    	return false;
  	}

  	*data = (*(xqry.data + 1) << 8) | *xqry.data;
  	free(xqry.data);
  	return true;
}

/**
   Extension Unitに値を設定する

   @param data 設定する値
   @param selector 設定する項目 (BEAT_CTRL_LASERなど)
 */
static bool set_xu_value(int fd,
						 long data,
                         const unsigned char selector) 
{
  	struct uvc_xu_control_query xqry;
  	long min, max;
#if 0
  	get_xu_value(fd, &min, selector, UVC_GET_MIN);
  	if(data < min) return false;

  	get_xu_value(fd, &max, selector, UVC_GET_MAX);
  	if(max < data) return false;
#endif
  	memset(&xqry, 0, sizeof(xqry));

  	xqry.unit     = BEAT_T4K37_UNIT_ID;
  	xqry.selector = selector;
  	xqry.query    = UVC_SET_CUR;
  	xqry.size     = BEAT_QUERY_SIZE;

  	xqry.data        = (__u8*)malloc(BEAT_QUERY_SIZE);
  	*(xqry.data + 1) = data >> 8;
  	*xqry.data       = data;

  	if(-1 == ioctl(fd, UVCIOC_CTRL_QUERY, &xqry)) {
  		fprintf(stderr, "UVCIOC_CTRL_QUERY failed. err=%d\n", errno);
    	return false;
  	}
  	return true;
}

/**
   Extension Unitから設定値を取得する

   @param data 取得した設定値を格納
   @param selector 取得する設定項目のインデックス (BEAT_CTRL_LASERなど)
   @param query 取得する値の種類 (UVC_GET_CURなど)
*/
static bool get_xu_value2(int fd, unsigned char* data, unsigned short size,
                         const unsigned char selector,
                         const unsigned char query) 
{
  	struct uvc_xu_control_query xqry;

  	if(!IS_BEAT_CTRL(selector))
    	return false;

  	if(query != UVC_GET_CUR  &&
       query != UVC_GET_MIN  &&
       query != UVC_GET_MAX  &&
       query != UVC_GET_RES  &&
       query != UVC_GET_LEN  &&
       /* query != UVC_GET_INFO &&  */
       query != UVC_GET_DEF)
       	return false;

  	memset(&xqry, 0, sizeof(xqry));

  	xqry.unit     = BEAT_T4K37_UNIT_ID;
  	xqry.selector = selector;
  	xqry.query    = query;
  	xqry.size     = size;	
  	xqry.data     = data;	

  	if(-1 == ioctl(fd, UVCIOC_CTRL_QUERY, &xqry))
    	return false;

  return true;
}

/**
   Extension Unitに値を設定する

   @param data 設定する値
   @param selector 設定する項目 (BEAT_CTRL_LASERなど)
 */
static bool set_xu_value2(int fd,
						 unsigned char* data, const short size,
                         const unsigned char selector) 
{
  	struct uvc_xu_control_query xqry;

  	memset(&xqry, 0, sizeof(xqry));

  	xqry.unit     = BEAT_T4K37_UNIT_ID;
  	xqry.selector = selector;
  	xqry.query    = UVC_SET_CUR;
  	xqry.size     = size;
  	xqry.data        = data;


  	if(-1 == ioctl(fd, UVCIOC_CTRL_QUERY, &xqry))
    	return false;

  	return true;
}
/**
   mmap の解除
*/

static void unmap_mmap(DevObject* self) 
{
  	unsigned int i;

//printf("unmap_mmap\n");
	if (self->buffers == NULL) {
		goto STILL;
	}

  	for (i = 0; i < self->n_buffers; ++i) {
  		if (self->buffers[i].start != NULL && self->buffers[i].start != MAP_FAILED) {
    		if (-1 == munmap(self->buffers[i].start, self->buffers[i].length)) {
      			fprintf(stderr, "munmap failed\n");
      		}
      		self->buffers[i].start = NULL;
      	}
  	}


	free(self->buffers);
	self->buffers = NULL;

STILL:
#ifdef SUPPORT_STILL_CAPTURE
  	/* STILL */
  	if (Dev_IsSupportStillCapture(self)) {
  		if (self->still_buffers == NULL) {
  			goto DONE;
  		}
  	
  		for(i = 0; i < self->still_n_buffers; ++i) {
  			if (self->still_buffers[i].start != NULL && self->still_buffers[i].start != MAP_FAILED) {
    			if(-1 == munmap(self->still_buffers[i].start, self->still_buffers[i].length)) {
      				fprintf(stderr, "munmap still failed\n");
      			}
      			self->still_buffers[i].start = NULL;
      		}
  		}
  
  		free(self->still_buffers);
  		self->still_buffers = NULL;
  	}
#endif
DONE:
//printf("unmap_mmap done\n");
	return;
}


#ifdef SUPPORT_STILL_CAPTURE
/**
   Still用のmmap
 */
static bool init_mmap_still(DevObject* self)
{
  	int i;
  	struct v4l2_requestbuffers req;
  	struct v4l2_buffer buf;

  	CLEAR(req);

	/* Still識別用にバッファカウントにUVC_STILL_REQBUF_CNT_MASKを設定 */
  	req.count       = NUM_STILL_BUFFERS | UVC_STILL_REQBUF_CNT_MASK;
  	req.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	req.memory      = V4L2_MEMORY_MMAP;

  	if(-1 == xioctl(self->devfd, VIDIOC_REQBUFS, &req)) {
    	if(EINVAL == errno) {
      		fprintf(stderr, "device does not support memory mapping\n");
      		return false;
    	} else {
      		fprintf(stderr, "VIDIOC_REQBUFS STILL\n");
      		return false;
    	}
  	}

  	if(req.count < 1) {
    	fprintf(stderr, "Insufficient buffer memory on device \n");
    	return false;
  	}

  	self->still_n_buffers = req.count;

  	self->still_buffers = (VideoBuffer*)calloc(req.count, sizeof(*self->still_buffers));
  	memset(self->still_buffers, 0, sizeof(self->still_buffers));

  	if(!self->still_buffers) {
    	fprintf(stderr, "Out of memory\n");
    	return false;
  	}

  	for(i = 0; i < self->still_n_buffers; ++i) {
    	CLEAR(buf);

    	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	buf.memory = V4L2_MEMORY_MMAP;
    	buf.index  = i;
    	/* Still識別用にflagsにUVC_STILL_MAGICを設定 */
    	buf.flags  = UVC_STILL_MAGIC;

    	if(-1 == xioctl(self->devfd, VIDIOC_QUERYBUF, &buf)) {
      		fprintf(stderr,"VIDIOC_QUERYBUF STILL failed\n");
      		return false;
      	}
      	
		//printf("still buf.m.offset 0x%X\n", buf.m.offset);
		
    	self->still_buffers[i].length = buf.length;
    	self->still_buffers[i].start  = (unsigned char*)mmap(NULL,
                                  		buf.length,
                                  		PROT_READ | PROT_WRITE,
                                  		MAP_SHARED,
                                  		self->devfd, buf.m.offset);

    	if(MAP_FAILED == self->still_buffers[i].start) {
      		fprintf(stderr, "mmap still failed\n");
      		return false;
    	}
  	}
  
  	return true;
}
#endif	// !SUPPORT_STILL_CAPTURE

/**
   メモリマップの初期化
   I/Oの対応の確認，バッファの確保，バッファの初期化
*/
static bool init_mmap(DevObject* self) 
{
  	unsigned int i;
  	struct v4l2_requestbuffers req;
  	struct v4l2_buffer buf;

  	CLEAR(req);
  	req.count  = NUM_BUFFERS;
  	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	req.memory = V4L2_MEMORY_MMAP;

  	/* 指定したI/O方式が対応しているか */
  	if(-1 == xioctl(self->devfd, VIDIOC_REQBUFS, &req)) {
    	if(EINVAL == errno) {
      		fprintf(stderr, "device does not support memory mapping\n");
      		return false;
    	} else {
    		fprintf(stderr, "VIDIOC_REQBUFS failed\n");
      		return false;
		}
  	}

  	//fprintf(stderr, "VIDIOC_REQBUFS: %u buffers\n", req.count);
  	/* バッファを確保できない */
  	if(req.count < 2) {
    	fprintf(stderr, "Insufficient buffer memory on device\n");
    	return false;
  	}

  	self->n_buffers = req.count;

  	self->buffers = (VideoBuffer *)calloc(self->n_buffers, sizeof(VideoBuffer));

  	if(!self->buffers) {
    	fprintf(stderr, "out of memory: buffers\n");
    	//free(self->buffers);
    	return false;
  	}

  	/* バッファの初期化 */
  	for(i=0; i<self->n_buffers; ++i) {
    	CLEAR(buf);

    	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	buf.memory = V4L2_MEMORY_MMAP;
    	buf.index  = i;

    	if(-1 == xioctl(self->devfd, VIDIOC_QUERYBUF, &buf)) {
      		fprintf(stderr, "VIDIOC_QUERYBUF\n");
      		return false;
    	}

    	self->buffers[i].length = buf.length;
    	self->buffers[i].start  = (unsigned char*) mmap(NULL, buf.length,
                            		PROT_READ | PROT_WRITE,
                            		MAP_SHARED,
                            		self->devfd,
                            		buf.m.offset);

    	if(MAP_FAILED == self->buffers[i].start) {
      		fprintf(stderr, "mmap\n");
      		return false;
    	}
  	}

#ifdef SUPPORT_STILL_CAPTURE
	if (Dev_IsSupportStillCapture(self))
  		return init_mmap_still(self);
  	
  	return true;
#else
	return true;
#endif
}

/**
   一時退避用メモリの確保
*/
static bool init_buffer(DevObject* self)
{
	int i, ret;
	long index;
	vcdll_priv* priv = (vcdll_priv*)self->priv;
	//thread_param* param = &priv->preview_thread;

	/* プレビュー一時確保用  */
	for (i = 0; i < self->n_buffers; i++) {
		priv->preview_buffer[i] = malloc(self->currentImageSize);
		if (priv->preview_buffer[i] == NULL) {
			return false;
		}
	}

	index = self->videoFormatInfo.currentFormatIndex;
	size_t size = self->videoFormatInfo.formats[index].width * 
				  self->videoFormatInfo.formats[index].height;
	index = self->videoFormatInfo.currentFormatIndex;
	//printf("init_buffer: index=%ld\n", index);
	//printf("init_buffer: size=%lu\n", size);
	//printf("init_buffer: bitPerPixel=%ld\n", self->videoFormatInfo.formats[index].bitPerPixel);
#if 0	
	if (self->videoFormatInfo.formats[index].bitPerPixel == 10) {
		/* プレビュー 10bit(2byte)一時確保用 */
		priv->previewRaw10_buffer = malloc(size * 2);
		if (priv->previewRaw10_buffer == NULL) {
			return false;
		}
	}
#endif
#ifndef DISABLE_PREVIEW
	/* プレビュー 8bit(1byte)一時確保用 */
	priv->previewRaw8_buffer = malloc(size);
	if (priv->previewRaw8_buffer == NULL) {
		return false;
	}
#endif
#ifdef SUPPORT_STILL_CAPTURE
	if (Dev_IsSupportStillCapture(self)) {
		/* スチル一時確保用 */
		index = self->stillFormatInfo.currentFormatIndex;
		size_t size = self->stillFormatInfo.formats[index].width * 
				  	  self->stillFormatInfo.formats[index].height;
		for (i = 0; i < self->still_n_buffers; i++) {
			priv->still_buffer[i] = malloc(self->currentStillImageSize);
			if (priv->still_buffer[i] == NULL) {
				return false;
			}
		}
#if 0	
		/* スチル 10bit(2byte)一時確保用 */
		index = self->stillFormatInfo.currentFormatIndex;
		if (self->stillFormatInfo.formats[index].bitPerPixel == 10) {
			priv->stillRaw10_buffer = malloc(self->currentStillImageSize);
			if (priv->stillRaw10_buffer == NULL) {
				return false;
			}
		}
#endif
	}
#endif
	
	return true;
}

/**
   一時退避用メモリの解放
*/
static void uninit_buffer(DevObject* self)
{
	int i, ret;
	vcdll_priv* priv = (vcdll_priv*)self->priv;

	for (i = 0; i < self->n_buffers; i++) {
		free(priv->preview_buffer[i]);
		priv->preview_buffer[i] = NULL;
	}
#if 0	
	free(priv->previewRaw10_buffer);
	priv->previewRaw10_buffer = NULL;
#endif	
	free(priv->previewRaw8_buffer);
	priv->previewRaw8_buffer = NULL;

#ifdef SUPPORT_STILL_CAPTURE
	if (Dev_IsSupportStillCapture(self)) {
		for (i = 0; i < self->still_n_buffers; i++) {
			free(priv->still_buffer[i]);
			priv->still_buffer[i] = NULL;
		}
	
		free(priv->stillRaw10_buffer);
		priv->stillRaw10_buffer = NULL;
	}
#endif

}

bool open_device(DevObject* self)
{
	SensorDevice* dev = &_deviceInfo.devices[self->currentDeviceIndex];
	
	/* デバイスのオープン */
	self->devfd = v4l2_open(dev->devicePath, O_RDWR | O_NONBLOCK, 0);
	if (self->devfd < 0) {
		fprintf(stderr, "Cannot open %s: %d\n",
            dev->devicePath, errno);
        return false;
	}
	
	return true;
}

bool close_device(DevObject* self)
{
	if(-1 == close(self->devfd)) {
        // switch to stderr
    	// printf("ERROR: close(fd)\n");
        fprintf(stderr, "ERROR: close(fd)\n");
    	return false;
  	}
  	
  	self->devfd = -1;
  	return true;
}

/**
	解像度の取得
*/
static void enum_format(DevObject* self, CapFormatInfo* outInfo, unsigned long stillMagic)
{
	int ret;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_frmivalenum fival;
	int fmtind = 0;
	int fsizeind = 0;
	int list_fps=0;
	
	memset(&fmt, 0, sizeof(fmt));
	outInfo->numFormat = 0;
	
	fmt.index = 0;
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	
	while ((ret = xioctl(self->devfd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
		long bitPerPixel;
		long bytesPerPixel;
		
		switch (fmt.pixelformat) {
		case V4L2_PIX_FMT_GREY:
			//printf("V4L2_PIX_FMT_GREY\n");
			bitPerPixel = 8;
			bytesPerPixel = 1;
			break;
			
		case V4L2_PIX_FMT_Y10:
			//printf("V4L2_PIX_FMT_Y10\n");
			bitPerPixel = 10;
			bytesPerPixel = 2;
			break;
			
		default:
			//printf("UNKNOWN: %X\n", fmt.pixelformat);
			fmt.index++;
			//fmt.flags = stillMagic;
			continue;
		}
		
		/* 解像度の取得 */
		fsize.index = 0;
		if (stillMagic == UVC_STILL_MAGIC) {
			/* Still識別用にインデックスにUVC_STILL_IDX_MASKを設定 */
			fsize.index |= UVC_STILL_IDX_MASK;
		}
		fsize.pixel_format = fmt.pixelformat;
		
		while ((ret = xioctl(self->devfd, VIDIOC_ENUM_FRAMESIZES, &fsize)) == 0) {
			unsigned long width;
			unsigned long height;
			
			if (fsize.type != V4L2_FRMSIZE_TYPE_DISCRETE) {
				fsize.index++;
				continue;
			}
			width = fsize.discrete.width;
			height = fsize.discrete.height;
#if 1			
			/* Still フォーマットの取得  */
			if (stillMagic == UVC_STILL_MAGIC) {
				CapFormat* info = &outInfo->formats[outInfo->numFormat];
				
				if (fsize.reserved[0] != 0xFFFFFFFF) {
					/* LFI用カスタムドライバはreserved[0]に0xFFFFFFFFをセットしてくる */
					outInfo->numFormat = 0;
					fprintf(stderr, "uvcvideo-driver does not support still-images ! 0x%X\n", fsize.reserved[0]);
					return;
				}
				
				makeGUID(fsize.pixel_format, &info->formatType);
				//printGUID(&info->formatType);
				info->width       = width;
				info->height      = height;
				info->bitPerPixel = bitPerPixel;
				info->imageSize   = width * height * bytesPerPixel;
				info->index       = UVC_STILL_IDX(fsize.index);
				
				outInfo->numFormat++;
				fsize.index++;
				continue;
			}
#endif			
			/* フレームレートの取得 */
			memset(&fival, 0, sizeof(fival));
			fival.index = 0;
			fival.pixel_format = fsize.pixel_format;
			fival.width = width;
			fival.height = height;
			fival.reserved[0] = stillMagic;
			while ((ret = xioctl(self->devfd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
				if (fival.type != V4L2_FRMIVAL_TYPE_DISCRETE) {
					fival.index++;
					fival.reserved[0] = stillMagic;
					continue;
				}

				CapFormat* info = &outInfo->formats[outInfo->numFormat];
						
				makeGUID(fsize.pixel_format, &info->formatType);
				//printGUID(&info->formatType);
				info->width       = width;
				info->height      = height;
				info->fps         = fival.discrete.denominator / fival.discrete.numerator;
				info->bitPerPixel = bitPerPixel;
				info->imageSize   = width * height * bytesPerPixel;
				info->index       = fival.index;
				
				outInfo->numFormat++;
				fival.index++;
				fival.reserved[0] = stillMagic;
			}
			fsize.index++;
			fival.reserved[0] = stillMagic;
		}
		fmt.index++;
		fmt.flags = stillMagic;
	}
	
}
/**
	オブジェクトの初期化
*/
bool initialize(DevObject* self, int index)
{
	int ret;
	struct v4l2_capability cap;
	struct v4l2_fmtdesc fmt;
	struct v4l2_frmsizeenum fsize;
	struct v4l2_frmivalenum fival;
	int fmtind = 0;
	int fsizeind = 0;
	int list_fps=0;
	
//printf("Dev initialize\n");

	self->currentDeviceIndex = index;
	SensorDevice* dev = &_deviceInfo.devices[self->currentDeviceIndex];	

	/* デバイスのオープン */
	if (!open_device(self)) {
		return false;
	}
	
	/* 機能の確認 */
	memset (&cap, 0, sizeof(struct v4l2_capability));
	if (xioctl(self->devfd , VIDIOC_QUERYCAP, &cap) < 0) {
		if (errno == EINVAL) {
			fprintf(stderr, "%s is no V4L2 device\n", dev->devicePath);
		} else {
			fprintf(stderr, "%s: VIDIOC_QUERYCAP failed\n", dev->devicePath);
		}
		return false;
	}
	/* キャプチャ非対応 */
    if(!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
      	fprintf(stderr, "%s is no video capture device \n", dev->devicePath);
      	return false;
    }
    /* ストリーミング非対応 */
    if(!(cap.capabilities & V4L2_CAP_STREAMING)) {
      	fprintf(stderr, "%s does not support streaming i/o\n", dev->devicePath);
      	return false;
    }

	/*  対応フォーマットの取得 */
	enum_format(self, &self->videoFormatInfo, 0);
	if (self->videoFormatInfo.numFormat == 0) {
		fprintf(stderr, "No valid formats\n");
		return false;
	}
	
#ifdef SUPPORT_STILL_CAPTURE	
	/* Still フォーマットの取得 */
	enum_format(self, &self->stillFormatInfo, UVC_STILL_MAGIC);
	
	if (self->stillFormatInfo.numFormat > 0) {
		if (memcmp(&self->stillFormatInfo, &self->videoFormatInfo, sizeof(CapFormatInfo)) == 0) {
			fprintf(stderr, "Still Caputure non-supported driver!\n");
			self->stillFormatInfo.numFormat = 0;
		}
	}
#endif	// SUPPORT_STILL_CAPTURE

	/* デフォルトフォーマット設定  */
	if (!Dev_SetFormatIndex(self, 0)) {
		return false;
	}
#ifdef SUPPORT_STILL_CAPTURE
	if (self->stillFormatInfo.numFormat > 0) {
		if (!Dev_SetStillFormatIndex(self, 0)) {
			return false;
		}
	}
#endif	
	
	return true;
}


#ifndef COMPILE
}
#endif

/***************************************************
   ライブラリ関数郡
 ***************************************************/
#ifndef COMPILE
{
#endif

void Dev_Initialize(void)
{
	memset(&_deviceInfo, 0, sizeof(_deviceInfo));
}

void Dev_Terminate(void)
{
}

long Dev_EnumDevice(void)
{
	struct udev *udev = udev_new();
	struct udev_enumerate *enumerate = NULL;
    struct udev_list_entry *devices = NULL;;
    struct udev_list_entry *dev_list_entry = NULL;
    
    int num_dev = 0;
    struct v4l2_capability v4l2_cap;
    
    /* Create a list of the devices in the 'v4l2' subsystem. */
    enumerate = udev_enumerate_new(udev);
    udev_enumerate_add_match_subsystem(enumerate, "video4linux");
    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
  
    udev_list_entry_foreach(dev_list_entry, devices)
	{
		const char *path;
		
		/*
         * Get the filename of the /sys entry for the device
         * and create a udev_device object (dev) representing it
         */
        path = udev_list_entry_get_name(dev_list_entry);
        struct udev_device *dev = udev_device_new_from_syspath(udev, path);
         
        /* usb_device_get_devnode() returns the path to the device node itself in /dev. */
        const char *v4l2_device = udev_device_get_devnode(dev);
#if DEBUG_FLAG
        printf("V4L2_CORE: Device Node Path: %s\n", v4l2_device);
#endif
        int fd = 0;
        /* open the device and query the capabilities */
        if ((fd = v4l2_open(v4l2_device, O_RDWR | O_NONBLOCK, 0)) < 0)
        {
            fprintf(stderr, "V4L2: ERROR opening V4L2 interface for %s\n", v4l2_device);
            v4l2_close(fd);
            continue; /*next dir entry*/
        }
     
        if (xioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) < 0)
        {
            fprintf(stderr, "V4L2: VIDIOC_QUERYCAP error: %s\n", strerror(errno));
            fprintf(stderr, "V4L2: couldn't query device %s\n", v4l2_device);
            v4l2_close(fd);
            continue; /*next dir entry*/
        }

		v4l2_close(fd);

		
		/* check the BC LFI Sensor */
		if (strncmp(v4l2_cap.card, DEVICE_NAME_PREFIX, strlen(DEVICE_NAME_PREFIX)) != 0)
		{
			continue;
		}
		
		strcpy(_deviceInfo.devices[num_dev].deviceName, v4l2_cap.card);
		strcpy(_deviceInfo.devices[num_dev].devicePath, v4l2_device);
		sscanf(v4l2_cap.card, DEVICE_NAME_PREFIX " %s", _deviceInfo.devices[num_dev].serialNumber);
		
		//printf("name:%s\n", _deviceInfo.devices[num_dev].deviceName);
		//printf("path:%s\n", _deviceInfo.devices[num_dev].devicePath);
		//printf("serial:%s\n", _deviceInfo.devices[num_dev].serialNumber);
		
		num_dev++;
		if (num_dev >= MAX_DEVICE_NUM) {
			break;
		}
	}
	
	/* Free the enumerator object */
    udev_enumerate_unref(enumerate);
    udev_unref(udev);
    
	_deviceInfo.numDevice = num_dev;
	
	if (_deviceInfo.numDevice == 0) {
		fprintf(stderr, "Device not found!\n");
	}
	
	return num_dev;
}

bool Dev_GetDeviceNameByIndex(int index, SensorDeviceName* name)
{
	if (!name) return false;
	if (index >= _deviceInfo.numDevice) return false;
	
	SensorDevice* dev = &_deviceInfo.devices[index];
	
	strcpy(name->deviceName, dev->deviceName);
	strcpy(name->serialNumber, dev->serialNumber);
	
	return true;
}

/**
   カメラデバイスのオブジェクトを生成

   ライブラリの初期化
   動画，静止画の解像度の一覧を取得・保持 (フォーマットの指定で使用)

   @param device_name オープンするデバイスの絶対パス
   @param format_id ストリーミング解像度のインデックス
*/
DevObject* Dev_NewObject(int index) 
{
//printf("Dev_NewObject %d\n", index);
	if (index >= _deviceInfo.numDevice) {
		return NULL;
	}

	DevObject* self = malloc(sizeof(DevObject));
	if (self == NULL) return NULL;

	memset(self, 0, sizeof(DevObject));
	
	vcdll_priv* priv = malloc(sizeof(vcdll_priv));
	if (priv == NULL) goto fail;
	
	memset(priv, 0, sizeof(vcdll_priv));
	
	cond_init(&priv->preview_thread);
	self->priv = (void*)priv;
	
	/*  デバイスの初期化 */
	if (initialize(self, index) == false) {
		fprintf(stderr, "Dev initialize failed\n");
		free(self);
		return NULL;
	}
//printf("Dev_NewObject done. %p\n", self);  
  	return self;
  	
fail:
	free(self);
	return NULL;
}

/**
   カメラデバイスオブジェクトの破棄
*/
void Dev_Dealloc(DevObject* self) {
  	unsigned int i;
  	struct v4l2_buffer buf;

	//printf("Dev_Dealloc\n");
	if (self == NULL) return;

	if (self->streaming == true) {
		Dev_Stop(self);
	} else {
  		unmap_mmap(self);
  	}
  	
	close_device(self);
	uninit_buffer(self);
	free(self->priv);
  	free(self);

	//printf("Dev_Dealloc done\n");  
}

/**
   接続されたカメラがサポートする動画の解像度の数を返す

   @return カメラがサポートする動画の解像度数
   @note エラー時には-1を返す
   @attention カメラの対応解像度の配列はDev_NewObject()で作成されているものとする
*/
long Dev_FormatCount(DevObject* self) 
{
	if (self == NULL) return 0;
	
	return self->videoFormatInfo.numFormat;  
}

/**
   サポートするフォーマットの詳細情報を取得

   @param index 取得するフォーマットのインデックス
   @param format 取得したフォーマットの情報を格納
*/
bool Dev_GetFormatbyIndex(DevObject* self, int index, CapFormat* info) 
{
	if (self == NULL) return false;
	
	if (index >= self->videoFormatInfo.numFormat) {
		return false;
	}
	
	*info = self->videoFormatInfo.formats[index];
	return true;
}

/**
   プレビュー(動画)で使用する解像度を指定

   @param index 指定する解像度のインデックス

   @return 解像度の設定ができたかどうか
*/
bool Dev_SetFormatIndex(DevObject* self, int index) 
{	
//printf("Dev_SetFormatbyIndex %d/%p\n", index, self);
	if (self == NULL) return false;

	if (index >= self->videoFormatInfo.numFormat) {
		return false;
	}

	/* ドライバ内の既存のバッファを解放 */
	struct v4l2_requestbuffers req;
	// release drivers buffer
	CLEAR(req);
  	req.count  = 0;
  	req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	req.memory = V4L2_MEMORY_MMAP;

  	if(-1 == xioctl(self->devfd, VIDIOC_REQBUFS, &req)) {
  		fprintf(stderr, "VIDIOC_REQBUFS err %d\n", errno);
  		return false;
  	}
	
	struct v4l2_format fmt;
	CapFormat* cap = &self->videoFormatInfo.formats[index];
		
	CLEAR(fmt);
	
	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	fmt.fmt.pix.width       = cap->width;
  	fmt.fmt.pix.height      = cap->height;
  	fmt.fmt.pix.pixelformat = getFormatType(&cap->formatType);
  	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
	
  	if(xioctl(self->devfd, VIDIOC_S_FMT, &fmt) < 0)
  	{
  		fprintf(stderr, "VIDIOC_S_FMT failed err=%d\n", errno);
    	return false;
    }
    
    self->videoFormatInfo.currentFormatIndex = index;
    
    if (cap->bitPerPixel > 8) {
    	// RAW10
    	self->currentImageSize = cap->width * cap->height * 2;
    } else {
    	// RAW8
    	self->currentImageSize = cap->width * cap->height;
    }
#if DEBUG_FLAG
    fprintf(stdout, "Dev_SetFormatIndex: idx:%d %ldx%ld %ldbit size=%ld\n",
			index, cap->width, cap->height, cap->bitPerPixel, self->currentImageSize);
#endif
	//printf("Dev_SetFormatbyIndex done\n");
  	return true;
}

/**
   現在の動画フォーマットの設定を取得

   @param *index 現在の解像度のインデックスを格納

   @return 現在の解像度の設定が取得できたかどうか
*/
bool Dev_GetCurrentFormatIndex(DevObject* self, int* index) 
{
	if (self == NULL) return false;

  	*index = self->videoFormatInfo.currentFormatIndex;

  	return true;
}

/**
   センサが静止画キャプチャをサポートしているかを取得

   @return センサの静止画キャプチャの可否
*/
bool Dev_IsSupportStillCapture(DevObject* self)
{
#ifdef SUPPORT_STILL_CAPTURE
	return (self->stillFormatInfo.numFormat > 0) ? true : false;
#else
	return false;
#endif
}

#if 0	// Old
bool Dev_IsSupportStillCapture(DevObject* self)
{
#if 1 //def SUPPORT_STILL_CAPTURE
  libusb_context *ctx;
  libusb_device **list;
  struct libusb_device_descriptor desc;
  ssize_t num_devs;
  int err, i, k, m, o;
  
  if (self == NULL) return false;

  if(err = libusb_init(&ctx)) {
    fprintf(stderr, "unable to initialize libusb: %i\n", err);
    return false;
  }

  num_devs = libusb_get_device_list(ctx, &list);

  if(num_devs < 0) {
    fprintf(stderr, "ERROR: no dev\n");
    return false;
  }

  for(o=0; o < num_devs; ++o) {
    libusb_device *dev = list[o];

    libusb_get_device_descriptor(dev, &desc);

    if(desc.bNumConfigurations) {
      struct libusb_config_descriptor *config;

      err = libusb_get_config_descriptor(dev, 0, &config);
      if(err) {
        fprintf(stderr, "Couldn't get configuration descriptor 0, "
                "some information will be missing \n");
      }
      else {
        libusb_free_config_descriptor(config);
      }

      for(i=0; i<desc.bNumConfigurations; ++i) {

        err = libusb_get_config_descriptor(dev, i, &config);
        if(err) {
          fprintf(stderr, "Coludn't get configuration descriptor 0, "
                  "some information will missing\n");
        }

        else {
          for(k=0; k < config->bNumInterfaces; ++k) {
            for(m=0; m < config->interface[k].num_altsetting; ++m) {
              struct libusb_interface_descriptor interface =
                config->interface[k].altsetting[m];

              if(interface.extra_length) {
                unsigned size = interface.extra_length;
                const unsigned char *buf;

                buf = interface.extra;

                while(size >= 2 * sizeof(u_int8_t)) {
                  if(buf[0] < 2) {
                    break;
                  }
                  if((buf[1] == USB_DT_CS_DEVICE ||
                      buf[1] == USB_DT_CS_INTERFACE) &&
                     interface.bInterfaceClass == USB_CLASS_VIDEO &&
                     interface.bInterfaceSubClass == 2 &&
                     buf[2] == 0x01) {
                    /* bStillCaptureMethod */
                    //printf("bStillCaptureMethod\n");
                    if(buf[9] == 2) {
                      return true;
                    }
                  }
                  size -= buf[0];
                  buf += buf[0];
                } /* while */
              } /* if interface.extra_length */
            } /* for m */
          } /* for k */
        } /* else */
        libusb_free_config_descriptor(config);
      } /* for i */
    } /* if desc.bNumConfigurations */
  } /* o */

  return false;
#else // !SUPPORT_STILL_CAPTURE
  return false;
#endif
}
#endif
/**
   センサがサポートする静止画解像度の数を取得

   @return センサがサポートする静止画解像度の数
   @note エラー時には-1を返す
   @attention カメラの対応解像度の配列はDev_NewObjectで作成されているものとする
*/
long Dev_StillFormatCount(DevObject* self) 
{
	if (self == NULL) return 0;
	
#ifdef SUPPORT_STILL_CAPTURE
  	return self->stillFormatInfo.numFormat;
#else
	return 0;
#endif
}

/**
   静止画キャプチャのフォマットの詳細情報を取得する
*/
bool Dev_GetStillFormatbyIndex(DevObject* self, int index,
                               CapFormat* info) 
{
	if (self == NULL) return false;
	
	if (!Dev_IsSupportStillCapture(self))
		return false;
	
#ifdef SUPPORT_STILL_CAPTURE
	if (index >= self->stillFormatInfo.numFormat) {
		return false;
	}
	
	*info = self->stillFormatInfo.formats[index];
	
	return true;
	
#else // !SUPPORT_STILL_CAPTURE
  return false;
#endif
}
/**
   ストリーミング開始
   @
*/
bool Dev_Start(DevObject* self) 
{
  	enum v4l2_buf_type type;
  	struct v4l2_cropcap    cropcap;
  	struct v4l2_crop       crop;
  	struct v4l2_buffer     buf;
  	int i;
 //printf("Dev_Start\n"); 
 	if (self == NULL) return false;
 	
  	if (self->streaming == true) {
  		fprintf(stderr, "Streaming already started\n");
  		return false;
  	}
  	/* クリッピング領域の設定 */
  	CLEAR(cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    
    if(0 == xioctl(self->devfd, VIDIOC_CROPCAP, &cropcap)) {
      	/* クリッピング領域の設定 */
      	crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      	crop.c = cropcap.defrect;
      	if(-1 == xioctl(self->devfd, VIDIOC_S_CROP, &crop)) {
        	switch(errno) {
        		case EINVAL:
          		/* クリッピング非対応 */
          		break;
        		default: /* Errors ignored */
          		break;
        	}
      	}
    }
    else {} /* Errors ignored */

    if (init_mmap(self) == false) {
    	fprintf(stderr, "Dev_Start: init_mmap failed\n");
    	return false;
    }

    /* バッファを積む */
  	for(i=0; i<self->n_buffers; ++i) {
    	CLEAR(buf);

    	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    	buf.memory = V4L2_MEMORY_MMAP;
    	buf.index  = i;

    	if(-1 == xioctl(self->devfd, VIDIOC_QBUF, &buf)) {
    		fprintf(stderr, "Dev_Start: VIDIOC_QBUF failed\n");
      		return false;
    	}
  	}
#ifdef SUPPORT_STILL_CAPTURE
	if (Dev_IsSupportStillCapture(self)) {
  		/* STILL */
  		for(i=0; i<self->still_n_buffers; ++i) {
    		CLEAR(buf);

    		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    		buf.memory = V4L2_MEMORY_MMAP;
    		buf.index  = i;
    		/* Still識別用にflagsにUVC_STILL_MAGICをセット */
    		buf.flags  = UVC_STILL_MAGIC;

    		if(-1 == xioctl(self->devfd, VIDIOC_QBUF, &buf))
      			return false;
  		}
  	}
#endif	// !SUPPORT_STILL_CAPTURE  	

	if (init_buffer(self) == false) {
		fprintf(stderr, "Dev_Start: init_buffer failed\n");
		return false;
	} 	
	
	// Stream ON
  	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	if (-1 == xioctl(self->devfd, VIDIOC_STREAMON, &type)) {
  		fprintf(stderr, "VIDIOC_STREAMON failed\n");
    	return false;
    }
#ifndef DISABLE_PREVIEW
	// thread start
	if (start_preview_thread(self) < 0) {
		xioctl(self->devfd, VIDIOC_STREAMOFF, &type);
		return false;
	}
#endif
    self->streaming = true;
    self->streamingOn = true;
//printf("Dev_Start done\n");
  	return true;
}

/**
   ストリーミング終了
   @
*/
bool Dev_Stop(DevObject* self) 
{
  	enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	//printf("Dev_Stop\n");

	if (self == NULL) return false;

	if (self->streaming == false) {
		fprintf(stderr, "Streaming not started\n");
		return false;
	}
#ifndef DISABLE_PREVIEW
	stop_preview_thread(self);
#endif
  	if (-1 == xioctl(self->devfd, VIDIOC_STREAMOFF, &type)) {
    	printf("ERROR: VIDIOC_STREAMOFF\n");
    	//return false;
	}

	unmap_mmap(self);
	uninit_buffer(self);

	//printf("Dev_Stop done\n");

	self->streaming = false;
  	return true;
}

/**
   静止画キャプチャのフォーマットを指定

   @param index 設定する解像度のインデックス(配列formatsの)

   @return 解像度の設定ができたかどうか
*/
bool Dev_SetStillFormatIndex(DevObject* self, int index) 
{
	if (self == NULL) return false;
	
	if (!Dev_IsSupportStillCapture(self))
		return false;
	
#ifdef SUPPORT_STILL_CAPTURE
	if (index >= self->stillFormatInfo.numFormat) {
		return false;
	}
	
	/* 既存のバッファを解放 */
	struct v4l2_requestbuffers req;
	
	req.count       = 0 | UVC_STILL_REQBUF_CNT_MASK;
  	req.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	req.memory      = V4L2_MEMORY_MMAP;
  	
  	if(-1 == xioctl(self->devfd, VIDIOC_REQBUFS, &req)) {
  		fprintf(stderr, "VIDIOC_REQBUFS err %d\n", errno);
  		return false;
  	}
	
	CapFormat* cap = &self->stillFormatInfo.formats[index];
  	struct v4l2_format fmt;

  	CLEAR(fmt);
  	
  	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	fmt.fmt.pix.width       = cap->width;
  	fmt.fmt.pix.height      = cap->height;
  	fmt.fmt.pix.pixelformat = getFormatType(&cap->formatType);
  	fmt.fmt.pix.field		= UVC_STILL_FMT_FIELD_MASK;

  	if(xioctl(self->devfd, VIDIOC_S_FMT, &fmt) < 0) {
  		fprintf(stderr, "VIDIOC_S_FMT failed\n");
    	return false;
    }
    
    self->stillFormatInfo.currentFormatIndex = index;
    if (cap->bitPerPixel > 8) {
    	// RAW10
    	self->currentStillImageSize = cap->width * cap->height * 2;
    } else {
    	// RAW8
    	self->currentStillImageSize = cap->width * cap->height;
    }
#if DEBUG_FLAG
    fprintf(stdout, "Dev_SetStillFormatIndex: idx:%d %ldx%ld %ldbit size=%ld\n", 
			index, cap->width, cap->height, cap->bitPerPixel, self->currentStillImageSize);
#endif
    return true;

#else  // !SUPPORT_STILL_CAPTURE
  return false;
#endif
}

/**
   動画のイメージデータを取得

   @param timeout_ms タイムアウトまでの時間 [ms]
*/
void* Dev_GetBuffer(DevObject* self, int milsec) {
#ifdef DISABLE_PREVIEW
    return NULL;
#else
#if 1
	int ret , buffer_index;
	struct timespec ts;
	vcdll_priv* prev = (vcdll_priv*)self->priv;
	thread_param* thread = &prev->preview_thread;
	
	
	toTimespec(milsec * 1000, &ts);
	
	/*  バッファ受信待機 */
	pthread_mutex_lock(&thread->mutex);
	
	ret = pthread_cond_timedwait(&thread->cond, &thread->mutex, &ts);
	if (ret != 0) {
		pthread_mutex_unlock(&thread->mutex);
		return NULL;
	}
	
	buffer_index = thread->current_buffer_index;
	
	pthread_mutex_unlock(&thread->mutex);
#if 0	
	int format_index = self->videoFormatInfo.currentFormatIndex;
	if (self->videoFormatInfo.formats[format_index].bitPerPixel == 10) {
		raw10to16bit(prev->preview_buffer[buffer_index], prev->previewRaw10_buffer,
					 self->videoFormatInfo.formats[format_index].width,
					 self->videoFormatInfo.formats[format_index].height);
		return prev->previewRaw10_buffer;
	}
#endif	
	return prev->preview_buffer[buffer_index];
	
#else
  	struct v4l2_buffer buf;

  	fd_set fds;
  	struct timeval tv;
  	int r;
  	void* ret = NULL;
//printf("Dev_GetBuffer\n");

	if (self == NULL) return false;

  	FD_ZERO(&fds);
  	FD_SET(self->devfd, &fds);

  	/* タイムアウトの設定 */
  	tv.tv_sec  = milsec / 1000;
  	tv.tv_usec = (milsec % 1000) * 1000;

  	/* I/O の監視 */
  	r = select(self->devfd + 1, &fds, NULL, NULL, &tv);

  	if(r == 0) {
    	fprintf(stderr, "ERROR: select timeout\n");
    	goto failure;
  	}
  	if(r== -1) {
    	fprintf(stderr, "ERROR: select error %d,\n",
            	errno);
    	goto failure;
  	}

  	CLEAR(buf);
  	buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  	buf.memory = V4L2_MEMORY_MMAP;

  	/* バッファを取り出す */
  	if(-1 == xioctl(self->devfd, VIDIOC_DQBUF, &buf)) {
    	//printf("%s: %d\n", __func__, errno);
    	switch(errno) {
    	case EAGAIN:
    	case EIO:
    	default:
      		goto failure;
    	}
  	}

  	assert(buf.index < self->n_buffers);
  	
	if (self->currentImageSize == buf.bytesused) {
  		/* バッファを別のポインタへ退避 */
  		memcpy(self->gp, self->buffers[buf.index].start, self->currentImageSize);
  		ret = self->gp;
  	}

  	/* バッファを戻す */
  	if(-1 == xioctl(self->devfd, VIDIOC_QBUF, &buf)) {
    	fprintf(stderr, "VIDIOC_QBUF\n");
	}
	
//printf("Dev_GetBuffer done\n");
  return ret;
#endif
failure:
//printf("Dev_GetBuffer failure\n");
  return NULL;
#endif // DISABLE_PREVIEW
}

/**
   動画のイメージデータを8bitで取得

   @param timeout_ms タイムアウトまでの時間 [ms]
*/
void* Dev_GetBufferRaw8(DevObject* self, int milsec)
{
#ifdef DISABLE_PREVIEW
    return NULL;
#else
	int ret , buffer_index;
	struct timespec ts;
	vcdll_priv* prev = (vcdll_priv*)self->priv;
	thread_param* thread = &prev->preview_thread;
	
	
	toTimespec(milsec * 1000, &ts);
	
	/*  バッファ受信待機 */
	pthread_mutex_lock(&thread->mutex);
	
	ret = pthread_cond_timedwait(&thread->cond, &thread->mutex, &ts);
	if (ret != 0) {
		pthread_mutex_unlock(&thread->mutex);
		return NULL;
	}
	
	buffer_index = thread->current_buffer_index;
	
	pthread_mutex_unlock(&thread->mutex);
	
	int format_index = self->videoFormatInfo.currentFormatIndex;
	
	if (self->videoFormatInfo.formats[format_index].bitPerPixel == 10) {
		raw10to8bit(prev->preview_buffer[buffer_index], prev->previewRaw8_buffer,
					 self->videoFormatInfo.formats[format_index].width,
					 self->videoFormatInfo.formats[format_index].height);
		return prev->previewRaw8_buffer;
	}
	
	return prev->preview_buffer[buffer_index];
#endif // DISABLE_PREVIEW
}

/**
   静止画イメージデータ取得のためにセンサにトリガを送信
*/
bool Dev_StillTrigger(DevObject* self) 
{
	if (self == NULL) return false;
	
	if (!Dev_IsSupportStillCapture(self))
		return false;
	
#if DEBUG_FLAG
	fprintf(stdout, "Dev_StillTrigger: format index = %ld\n", self->stillFormatInfo.currentFormatIndex);
#endif
#ifdef SUPPORT_STILL_CAPTURE
  	struct v4l2_control ctrl;
  	CLEAR(ctrl);
  	ctrl.id = 98765;

	if(-1 == xioctl(self->devfd, VIDIOC_S_CTRL, &ctrl)) {
    	switch(errno) {
    	case EAGAIN:
    		fprintf(stderr, "VIDIOC_S_CTRL: still trigger error EAGAIN\n");
      		goto failure;

    	case EIO:
    	default:
      		fprintf(stderr, "VIDIOC_S_CTRL: still trigger error\n");
      		goto failure;
    	}
  	}

  return true;

 failure:
  return false;

#else	// !SUPPORT_STILL_CAPTURE
  return false;
#endif
}

/**
   静止画イメージデータを取得
*/
void* Dev_GetStillBuffer(DevObject* self, int milsec) 
{
	vcdll_priv* priv = self->priv;
	
	
	if (self == NULL) return false;
	
	if (!Dev_IsSupportStillCapture(self))
		return false;
	
#ifdef SUPPORT_STILL_CAPTURE
  	struct v4l2_buffer buf;
	int ret;
  	uint64_t end_time;
  	void* ret_buffer = NULL;
  	
  	if (milsec < MIN_STILL_BUFFER_TIMEOUT) {
  		milsec = MIN_STILL_BUFFER_TIMEOUT;
  	}

  	end_time = system_time() + (milsec * 1000) + (10 *1000);

	do {
		CLEAR(buf);
  		buf.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  		buf.memory = V4L2_MEMORY_MMAP;
  		buf.flags  = UVC_STILL_MAGIC;
  	
		ret = xioctl(self->devfd, VIDIOC_DQBUF, &buf);
		if (ret == 0) {
			break;
		}
		//fprintf(stderr, "Dev_GetStillBuffer VIDIOC_DQBUF %d\n", errno); 
		if (errno == EAGAIN) {
			if (system_time() > end_time) {
				fprintf(stderr, "Dev_GetStillBuffer timedout\n"); 
				return NULL;
			}
			snooze(200000);
		} else {
			return NULL;
		}
	} while (ret != 0);

	if (self->currentStillImageSize != buf.bytesused) {
		fprintf(stderr, "Dev_GetStillBuffer: Bad image size: %lu/%d\n", 
				self->currentStillImageSize, buf.bytesused);
		ret_buffer = NULL;
		goto out;
	}
#if DEBUG_FLAG
  	fprintf(stdout, "Dev_GetStillBuffer: Bbuffer received size: %d/%lu\n", 
				buf.bytesused, self->currentStillImageSize);
#endif
  	/* バッファを別のポインタへ退避 */
  	long index = self->stillFormatInfo.currentFormatIndex;
  	CapFormat* cap = &self->stillFormatInfo.formats[index];
#if 0  	
  	if (cap->bitPerPixel == 10) {
  		raw10to16bit(self->still_buffers[buf.index].start , 
  					 priv->previewRaw10_buffer,
  					 cap->width, cap->height);
  		ret_buffer =  priv->previewRaw10_buffer;
  		goto out;
  	}
#endif  	
  	memcpy(priv->still_buffer[buf.index], self->still_buffers[buf.index].start, 
  				self->currentStillImageSize);
  	ret_buffer =  priv->still_buffer[buf.index];

out:	
  	/* バッファを戻す */
  	buf.flags = UVC_STILL_MAGIC;
  	if(-1 == xioctl(self->devfd, VIDIOC_QBUF, &buf)){
    	//errno_exit("VIDOC_QBUF STILL");
    }
      	
  	return ret_buffer;
#else // !SUPPORT_STILL_CAPTURE
  return NULL;
#endif
}

/**
   センサの露出コントロールのパラメータを取得

   @param min 取得した露出時間の最小値を格納
   @param max 取得した露出時間の最大値を格納
   @param step 取得した露出時間の最小変化幅を格納
   @param def 取得した露出時間の初期値を格納
*/
bool Dev_GetExposureRange(DevObject* self,
						  long* min, long* max,
                          long* step, long* def) {
  	return get_xu_value(self->devfd,min, BEAT_CTRL_EXPOSURE, UVC_GET_MIN) &&
    	   get_xu_value(self->devfd, max, BEAT_CTRL_EXPOSURE, UVC_GET_MAX) &&
    	   get_xu_value(self->devfd, step, BEAT_CTRL_EXPOSURE, UVC_GET_RES) &&
    	   get_xu_value(self->devfd, def, BEAT_CTRL_EXPOSURE, UVC_GET_DEF);
}

/**
   センサの露出コントロールの現在の値を取得

   @param exposure 現在のカメラの露出時間を格納
*/
bool Dev_GetExposure(DevObject* self, long* exposure) 
{
	if (self == NULL) return false;

  	return get_xu_value(self->devfd,
  					    exposure,
                        BEAT_CTRL_EXPOSURE,
                        UVC_GET_CUR);
}

/**
   センサの露出コントロールの値を設定

   @param exposure 設定する露出コントロールの値
*/
bool Dev_SetExposure(DevObject* self, long exposure) 
{
	if (self == NULL) return false;
//printf("Dev_SetExposure: %ld\n", exposure);
  	return set_xu_value(self->devfd, 
  					    exposure,
                        BEAT_CTRL_EXPOSURE);
}

/**
   センサのゲインコントロールのパラメータを取得

   @param min  設定可能な最小値を格納
   @param max  設定可能な最大値を格納
   @param step 設定値のステップを格納
   @param def  初期値を格納
*/
bool Dev_GetGainRange(DevObject* self,
					  long* min, long* max,
                      long* step, long* def) 
{
  	struct v4l2_control   control;
  	struct v4l2_queryctrl queryctrl;
  
  	if (self == NULL) return false;

  	memset(&queryctrl, 0, sizeof(queryctrl));
  	queryctrl.id = V4L2_CID_GAIN;

  	/* get value range */
  	if(-1 == ioctl(self->devfd, VIDIOC_QUERYCTRL, &queryctrl))
    	return false;

  	*min  = queryctrl.minimum;
  	*max  = queryctrl.maximum;
  	*step = queryctrl.step;
  	*def  = queryctrl.default_value;

  	return true;
}

/**
   センサのゲインコントロールの値を取得

   @param gain センサのゲインコントロールの値を格納
*/
bool Dev_GetGain(DevObject* self, long* gain) 
{
  	struct v4l2_control   control;
  
  	if (self == NULL) return false;

  	control.id = V4L2_CID_GAIN;

  	if(-1 == xioctl(self->devfd, VIDIOC_G_CTRL, &control))
    	return false;

  	*gain = control.value;

  	return true;
}

/**
   センサのゲインコントロールの値を取得

   @param gain 設定するゲインコントロールの値
*/
bool Dev_SetGain(DevObject* self, long gain) 
{
  	struct v4l2_control control;
  	long min, max, step, def;
#if DEBUG_FLAG
    printf("Dev_SetGain: %ld\n", gain);
#endif
  	if (self == NULL) return false;

  	Dev_GetGainRange(self, &min, &max, &step, &def);

  	if(gain < min || max < gain)
    	return false;

  	control.id = V4L2_CID_GAIN;
  	control.value = gain;

  	if(-1 == xioctl(self->devfd, VIDIOC_S_CTRL, &control))
    	return false;

  	return true;
}

/**
   静止画キャプチャ時に発行させるレーザー番号を取得

   @param number レーザー番号を格納
*/
bool Dev_GetCurrentLaserNumber(DevObject* self, long* number) 
{
	if (self == NULL) return false;
	
  	return get_xu_value(self->devfd,
  					  	number,
                      	BEAT_CTRL_LASER,
                      	UVC_GET_CUR);
}

/**
   静止画キャプチャ時に発行させるレーザー番号を設定する

   @param number 設定するレーザー番号
*/
bool Dev_SetCurrentLaserNumber(DevObject* self, long number) 
{
	if (self == NULL) return false;
	
  	return set_xu_value(self->devfd, number, BEAT_CTRL_LASER);
}

/**
   センサの静止画キャプチャ時のReadOutが開始されるまでの
   ディレイ(tRDOUT)の現在の値を取得

   @param delay 取得したディレイの値を格納
*/
bool Dev_GetSensorReadoutDelay(DevObject* self, long* delay) 
{
	if (self == NULL) return false;
	
  	return get_xu_value(self->devfd,
  					    delay,
                        BEAT_CTRL_tRDOUT,
                        UVC_GET_CUR);
}

/**
   センサの静止画キャプチャ時のReadOutが開始されるまでの
   ディレイ(tRDOUT)を設定

   @param delay 設定するディレイの値
*/
bool Dev_SetSensorReadoutDelay(DevObject* self, long delay) 
{
	if (self == NULL) return false;
	
  	return set_xu_value(self->devfd, delay, BEAT_CTRL_tRDOUT);
}

/**
   センサのキャプチャイメージの反転状態を取得
*/
bool Dev_GetSensorFlip(DevObject* self,
					  long* horizontalMirror,
                      long* verticalFlip) {
  	long temp;
  
  	if (self == NULL) return false;

  	bool retval = get_xu_value(self->devfd, 
  							   &temp,
                               BEAT_CTRL_FLIP,
                               UVC_GET_CUR);
#if 1
	*horizontalMirror = temp & 1;
  	*verticalFlip     = temp >> 1;
#else
  	*horizontalMirror = temp >> 1;
  	*verticalFlip     = temp & 1;
#endif

  	return retval;
}

/**
   センサのキャプチャイメージの反転状態を設定
*/
bool Dev_SetSensorFlip(DevObject* self,
					   long horizontalMirror,
                       long verticalFlip) 
{
	if (self == NULL) return false;
#if 1
		return set_xu_value(self->devfd,
  					  horizontalMirror | (verticalFlip << 1),
                      BEAT_CTRL_FLIP);
#else                       
  	return set_xu_value(self->devfd,
  					  	horizontalMirror << 1 | verticalFlip,
                      	BEAT_CTRL_FLIP);
#endif
}

/**
   センサが静止画キャプチャが可能な状態かを問い合わせる

   @param canStillCapture 静止画キャプチャの可否を格納
 */
bool Dev_GetCanStillCapture(DevObject* self,
							long* canStillCapture) 
{
	if (self == NULL) return false;
	
#ifdef SUPPORT_STILL_CAPTURE
#if DEBUG_FLAG
  	printf("Dev_GetCanStillCapture:\n");
#endif
  	return get_xu_value(self->devfd,
  					 	canStillCapture,
                      	BEAT_CTRL_CAN_STILL,
                      	UVC_GET_CUR);
#else
  	return false;
#endif
}

/**
   センサの電源のOn/Offの状態を取得

   @param onOff 電源の状態を格納
 */
bool Dev_GetSensorPower(DevObject* self, long* onOff)
{
	if (self == NULL) return false;
	
	return get_xu_value(self->devfd,
  					    onOff,
                        BEAT_CTRL_POWER,
                        UVC_GET_CUR);
}

/**
   センサの電源のOn/Offの状態を設定

   @param onOff 設定する電源の状態
 */
bool Dev_SetSensorPower(DevObject* self, long onOff)
{
	if (self == NULL) return false;
	
	return set_xu_value(self->devfd,
  					    onOff,
                        BEAT_CTRL_POWER);
}

//
// 2.2.0
//

/**
	センサの接続状態の取得
	
	@param detected 取得する状態の取得
 */
bool Dev_GetSensorDetected(DevObject* self, long* detected)
{
	if (self == NULL || detected == NULL) return false;
	
	return get_xu_value(self->devfd,
  					    detected,
                        BEAT_CTRL_SENSOR_DETECTED,
                        UVC_GET_CUR);
}

/**
	センサの切り替え状況の取得
	
	@param number 取得するセンサ番号
 */
bool Dev_GetCurrentSensorNumber(DevObject* self, long* number)
{
	if (self == NULL || number == NULL) return false;
	
	return get_xu_value(self->devfd,
  					    number,
                        BEAT_CTRL_SENSOR_SELECT,
                        UVC_GET_CUR);
}

/**
	センサの切り替え設定
	
	@param number 設定するセンサ番号
 */
bool Dev_SetCurrentSensorNumber(DevObject* self, long number)
{
	if (self == NULL) return false;
	
	return set_xu_value(self->devfd,
  					    number,
                        BEAT_CTRL_SENSOR_SELECT);
}

/**
	レーザーの設定状況の取得
	
	@param current 電流(uA)
	@param duration 発光時間(uSec)
 */
bool Dev_GetCurrentLaserSetting(DevObject* self, long* current, long* duration)
{
	unsigned char data[8];
	
	if (self == NULL || current == NULL || duration == NULL) {
		return false;
	}
	
	bool ret = get_xu_value2(self->devfd, data, 8, 
							 BEAT_CTRL_SENSOR_SETTING, UVC_GET_CUR);
	if (ret == false) {
		return false;
	}

	*current  = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
	*duration = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
	
	return true;
}

/**
	レーザーの設定
	
	@param current 電流(uA)
	@param duration 発光時間(uSec)
 */
bool Dev_SetCurrentLaserSetting(DevObject* self, long current, long duration)
{
	unsigned char data[8];
	
	if (self == NULL) {
		return false;
	}
	
	data[0] = (current >> 24) & 0xFF;
	data[1] = (current >> 16) & 0xFF;
	data[2] = (current >>  8) & 0xFF;
	data[3] = (current >>  0) & 0xFF;
	data[4] = (duration >> 24) & 0xFF;
	data[5] = (duration >> 16) & 0xFF;
	data[6] = (duration >>  8) & 0xFF;
	data[7] = (duration >>  0) & 0xFF;
	
	return set_xu_value2(self->devfd, data, 8, BEAT_CTRL_SENSOR_SETTING);
}

/**
	レーザーのOn/Off状況の取得
	
	@param onOff 
 */
bool Dev_GetLaserOnOff(DevObject* self, long* onOff)
{
	if (self == NULL || onOff == NULL) {
		return false;
	}
	
	return get_xu_value(self->devfd,
  					    onOff,
                        BEAT_CTRL_LASER_ONOFF,
                        UVC_GET_CUR);
}

/**
	レーザーのOn/Off設定
	
	@param onOff 
 */
bool Dev_SetLaserOnOff(DevObject* self, long onOff)
{
	if (self == NULL) return false;
	
	return set_xu_value(self->devfd,
  					    onOff,
                        BEAT_CTRL_LASER_ONOFF);
}

bool Dev_GetSensorRegister(DevObject* self, unsigned short addr, unsigned short length, unsigned short* value)
{
	bool ret;
	unsigned char data[3];
	
	if (self == NULL || value == NULL) {
		return false;
	}
	
	data[0] = (addr >> 8) & 0xFF;
	data[1] = addr & 0xFF;
	data[2] = length & 0xFF;
	
	ret = set_xu_value2(self->devfd, data, 3, BEAT_CTRL_SENSOR_REG_ADDR);
	if (ret == false) {
		return false;
	}
	
	return get_xu_value(self->devfd,
  					    (long*)value,
                        BEAT_CTRL_SENSOR_REG_VALUE,
                        UVC_GET_CUR); 
}

bool Dev_SetSensorRegister(DevObject* self, unsigned short addr, unsigned short length, unsigned short value)
{
	bool ret;
	unsigned char data[3];
	
	if (self == NULL) {
		return false;
	}
	
	data[0] = (addr >> 8) & 0xFF;
	data[1] = addr & 0xFF;
	data[2] = length & 0xFF;
	
	ret = set_xu_value2(self->devfd, data, 3, BEAT_CTRL_SENSOR_REG_ADDR);
	if (ret == false) {
		return false;
	}
	
	return set_xu_value(self->devfd,
  					    value,
                        BEAT_CTRL_SENSOR_REG_VALUE);
}

bool Dev_GetSerialNumber(DevObject* self, void* buff, long length)
{
	if (self == NULL || buff == NULL || length != 8) {
		return false;
	}
	
	return get_xu_value2(self->devfd, (unsigned char*)buff, 8,
						 BEAT_CTRL_SERIAL, UVC_GET_CUR);
}

bool Dev_SetSerialNumber(DevObject* self, void* buff, long length)
{
	if (self == NULL || buff == NULL || length != 8) {
		return false;
	}
	
	return set_xu_value2(self->devfd, (unsigned char*)buff, 8, BEAT_CTRL_SERIAL);
}

#ifndef COMPILE
}
#endif
