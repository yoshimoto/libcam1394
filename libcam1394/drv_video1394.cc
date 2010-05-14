/**
 * @file   drv_video1394.cc
 * @author Hiromasa YOSHIMOTO
 * @date   Thu May  6 23:06:45 2010
 * 
 * @brief  a driver for video1394 kernel module
 * 
 * 
 */

#include "config.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>

#include "common.h"
#include "1394cam_drv.h"

#if defined HAVE_VIDEO1394
#include "video1394.h"

struct drv_video1394_data {
     int fd;                        // file descriptor of the driver
     char *buffer;                  // pointer to the mmaped buffer
     int buffer_size;
     int num_frame;
     int last_read_frame;
     int channel;
};

#define CHECK_CTX(ctx) do { if (!(ctx)) return -1; } while(0)
#define GETDATA(ctx) (drv_video1394_data*)((char*)(ctx) + sizeof(libcam1394_driver))


static int drv_video1394_close(libcam1394_driver *ctx)
{
     CHECK_CTX(ctx);
     drv_video1394_data *d = GETDATA(ctx);

     if (d->buffer) {
	  // !!FIXME!!
	  //munmap();
	  d->buffer = NULL;
     }
 
     if (d->fd) {
	  close(d->fd);
	  d->fd = 0;
     }

     return 0;
}


static int
drv_video1394_mmap(libcam1394_driver *ctx,
		   int port_no, const char *devicename,
		   int channel,
		   int sz_packet, int num_packet, 
		   int num_frame,
		   int *header_size)
{
     CHECK_CTX(ctx);
     drv_video1394_data *d = GETDATA(ctx);

     unsigned int i;
     void *buffer=NULL;
     if (0 != strcmp(devicename, "ohci1394")){
	  // this module supports ohci1394 only
	  return -1;
     }

     *header_size = 8;

     char devname[1024];
     snprintf(devname, sizeof(devname), "/dev/video1394/%d", port_no);
     LOG("trying to open("<<devname<<")");
     d->fd = open(devname, O_RDONLY);
     // quick hack for backward-compatibility
     if (d->fd<0){
	  snprintf(devname, sizeof(devname), "/dev/video1394%c", 
		   char('a'+port_no));
	  LOG("trying to open("<<devname<<")");
	  d->fd = open(devname, O_RDONLY);	
     }
     if (d->fd<0){
	  snprintf(devname, sizeof(devname), "/dev/video1394-%d", 
		   port_no);
	  LOG("trying to open("<<devname<<")");
	  d->fd = open(devname, O_RDONLY);	
     }
     // check whether the driver is loaded or not.
     if (d->fd<0){
	  ERR("Failed to open video1394 device ");
	  ERR("The video1394 module must be loaded, "
	      "and you must have read and write permission to its device file.");
	  goto err;
     }
    
     struct video1394_mmap vmmap;
     struct video1394_wait vwait;

     vmmap.channel = channel;  
     vmmap.sync_tag = 1;
     vmmap.nb_buffers = num_frame;
     vmmap.buf_size =   (sz_packet + *header_size)*num_packet;
     vmmap.fps = 0;
     vmmap.syt_offset = 0;
     vmmap.flags = VIDEO1394_SYNC_FRAMES | VIDEO1394_INCLUDE_ISO_HEADERS;
    
     /* tell the video1394 system that we want to listen to the given channel */
     if (ioctl(d->fd, VIDEO1394_IOC_LISTEN_CHANNEL, &vmmap) < 0)
     {
	  ERR("VIDEO1394_IOC_LISTEN_CHANNEL ioctl failed");
	  goto err;
     }

     channel = vmmap.channel;
     d->buffer_size = vmmap.buf_size;
     d->num_frame = vmmap.nb_buffers;

     /* QUEUE the buffers */
     for (i = 0; i < vmmap.nb_buffers; i++){
	  vwait.channel = channel;
	  vwait.buffer = i;
	  if (ioctl(d->fd,VIDEO1394_IOC_LISTEN_QUEUE_BUFFER,&vwait) < 0)
	  {
	       LOG("unlisten channel " << channel);
	       ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER ioctl failed");
	       ioctl(d->fd,VIDEO1394_IOC_UNLISTEN_CHANNEL,&channel);
	       goto err;
	  }
     }
    
     /* allocate ring buffer  */
     vwait.channel= vmmap.channel;
     d->buffer = (char*)mmap(0, vmmap.nb_buffers * vmmap.buf_size,
			     PROT_READ,MAP_SHARED, d->fd, 0);
     if (d->buffer == MAP_FAILED) {
	  ERR("video1394_mmap failed");
	  ioctl(d->fd, VIDEO1394_IOC_UNLISTEN_CHANNEL, &vmmap.channel);
	  goto err;
     }
     return 0;

err:
     drv_video1394_close(ctx);
     return -1;
}

static void *
drv_video1394_updateFrameBuffer(libcam1394_driver *ctx,
				C1394CameraNode::BUFFER_OPTION opt,
				BufferInfo *info)
{
     if (!ctx)
	  return NULL;
     drv_video1394_data *d = GETDATA(ctx);

     int result;
     struct video1394_wait vwait;
     vwait.channel = d->channel;
     vwait.buffer = d->last_read_frame % d->num_frame;
     result = ioctl(d->fd, VIDEO1394_IOC_LISTEN_WAIT_BUFFER, &vwait);
     //result = ioctl(fd, VIDEO1394_IOC_LISTEN_POLL_BUFFER, &vwait);
    
     if (result!=0){
	  if (errno == EINTR){
	       // !!FIXME!!
	  }
	  return 0;
     }

     int drop_frames = vwait.buffer;
     while (drop_frames-->0){
	  vwait.channel = d->channel;
	  vwait.buffer = d->last_read_frame++%d->num_frame;
	  if (ioctl(d->fd, VIDEO1394_IOC_LISTEN_QUEUE_BUFFER, &vwait) < 0){
	       ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER failed.");
	  }
     }

     vwait.channel = d->channel;
     vwait.buffer = d->last_read_frame++ % d->num_frame;
     if (ioctl(d->fd, VIDEO1394_IOC_LISTEN_QUEUE_BUFFER, &vwait) < 0){
	  ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER failed.");
     }
     char * p =
	  d->buffer + d->buffer_size*((d->last_read_frame-1)%d->num_frame);
     return p;
}

static int 
drv_video1394_getFrameCount(libcam1394_driver *ctx,
			    int *counter)
{
     ERR("video1394 doesn't supporte this function");
     return -1;
}

static int
drv_video1394_setFrameCount(libcam1394_driver *ctx,
			    int counter)
{
     ERR("video1394 doesn't supporte this function");
     return -1;
}

#endif //#if defined HAVE_VIDEO1394


libcam1394_driver*
drv_video1394_new()
{
#if defined HAVE_VIDEO1394
     const int sz = sizeof(libcam1394_driver) + sizeof(drv_video1394_data);
     libcam1394_driver * drv;
     drv = (libcam1394_driver*)malloc(sz);
     memset(drv, 0, sz);

     drv->close =  drv_video1394_close;
     drv->mmap = drv_video1394_mmap;
     drv->getFrameCount = drv_video1394_getFrameCount;
     drv->setFrameCount = drv_video1394_setFrameCount;
     drv->updateFrameBuffer = drv_video1394_updateFrameBuffer;

     return drv;
#else
     LOG("video1394 support is disabled");
     return NULL;
#endif
}
