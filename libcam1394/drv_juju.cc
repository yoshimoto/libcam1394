/**
 * @file   drv_juju.cc
 * @author Hiromasa YOSHIMOTO
 * @date   Thu May  6 23:06:45 2010
 * 
 * @brief  a driver for juju kernel module
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
#include <poll.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "1394cam_drv.h"
#include "common.h"

#if defined HAVE_JUJU
#include <linux/firewire-cdev.h>

#define ptr_to_u64(p) ((__u64)(unsigned long)(p))
#define u64_to_ptr(p) ((void *)(unsigned long)(p))

#define CHECK_CTX(ctx) do { if (!(ctx)) return -1; } while(0)
#define GETDATA(ctx) (drv_juju_data*)((char*)(ctx) + sizeof(libcam1394_driver))


//
//  mmaped[]                packets[]
//  +------------+           +---+
//  |            |           +---+
//  | index=0    |             :
//  |            |           +---+ 
//  +------------+           +---+
//  |            |           +---+
//  | index=1    |             :
//  |            |           +---+
//  +------------+           +---+
//        :                 :
//  +------------+           +---+
//  |            |           +---+
//  | num_frame-1|             :
//  |            |           +---+
//  +------------+           +---+
//
//
//
//
//
//
struct drv_juju_data {

     int fd;                        // file descriptor of the driver
     int abi_version;
     int isorxhandle;              // a handle for isoRx

     fw_cdev_iso_packet *packets;   //
     char *mmaped;                  //

     int sz_packet;
     int num_packet;
     int buffer_size;               // (sz_packet+get_header_size(d))*num_packet

     int num_frame;

     int index;                     // point to receiving image

     int channel;
};


static int
get_cardno(int portno, struct raw1394_portinfo *portinfo)
{
    static const char *devname = "/dev/fw";
    const int len = sizeof(devname);
    if (0 == strncmp(devname, portinfo->name, len-1)) {
	return portinfo->name[len-1] - '0';
    } else {
	return portno;
    }
}

static inline int
get_header_size(drv_juju_data *d)
{
     return d->abi_version >= 2 ? 8 : 4;
}

static int 
setup_dma_desc(drv_juju_data *d, int index)
{
     fw_cdev_iso_packet *pkt = d->packets;
     int i;
     for (i=0; i<d->num_packet; ++i) {
	  pkt[i].control = 
	       FW_CDEV_ISO_PAYLOAD_LENGTH(d->sz_packet) |
	       FW_CDEV_ISO_HEADER_LENGTH(get_header_size(d));
     }
     pkt[0].control |= FW_CDEV_ISO_SYNC;
     pkt[d->num_packet-1].control |= FW_CDEV_ISO_INTERRUPT;

     return 0;
err:
     return -1;
}

static int
queue_dma_desc(drv_juju_data *d, int index)
{
     int retval;
     fw_cdev_queue_iso  q;
     fw_cdev_iso_packet *pkt = d->packets;
     memset(&q, 0, sizeof(q));
     q.packets = ptr_to_u64(pkt);
     q.data = ptr_to_u64(d->mmaped + d->buffer_size*index);
     q.size = d->num_packet * sizeof( pkt[0] );
     q.handle = d->isorxhandle;

     LOG(q.size);

     retval = ioctl(d->fd, FW_CDEV_IOC_QUEUE_ISO, &q);
     if (retval < 0) {
	  ERR("FW_CDEV_IOC_QUEUE_ISO failed");
	  goto err;
     }
     LOG("queued "<<retval << "  " << q.size);
     return 0;
err:
     return -1;
}

static int
start_dma_desc(drv_juju_data *d)
{
     int retval;
     struct fw_cdev_start_iso start_iso;
     memset(&start_iso, 0, sizeof(start_iso));
     start_iso.cycle = -1;
     start_iso.tags = FW_CDEV_ISO_CONTEXT_MATCH_ALL_TAGS;
     start_iso.sync = 1;
     start_iso.handle = d->isorxhandle;
     retval = ioctl(d->fd, FW_CDEV_IOC_START_ISO, &start_iso);
     if (retval < 0) {
	  ERR("FW_CDEV_IOC_START_ISO failed");
	  goto err;
     }
     return 0;
err:
     return -1;
}

static int
drv_juju_close(libcam1394_driver *ctx)
{
     CHECK_CTX(ctx);
     drv_juju_data *d = GETDATA(ctx);

     if (d->mmaped) {
	  munmap(d->mmaped, d->buffer_size * d->num_frame);
	  d->mmaped = NULL;
     }

     if (d->packets) {
	  d->packets = NULL;
     }
 
     if (0 < d->fd) {
	  close(d->fd);
	  d->fd = 0;
     }

     return 0;
}


static int
drv_juju_mmap(libcam1394_driver *ctx,
	      int port_no, const char *devicename,
	      int channel,
	      int sz_packet, int num_packet, 
	      int num_frame,
	      int *header_size)
{
     int i;
     drv_juju_data *d;
     int retval;
     struct fw_cdev_create_iso_context create;

     CHECK_CTX(ctx);
     d = GETDATA(ctx);
     assert( d );

     static const char *dname = "/dev/fw";
     if (0 != strncmp(devicename, dname, sizeof(dname))) {
	  // this module supports juju stack only
	  return -1;
     }

     LOG("trying to open("<<devicename<<")");
     d->fd = open(devicename, O_RDWR);
     // check whether the driver is loaded or not.
     if (d->fd < 0){
	  ERR("Failed to open firewire device, "<<devicename);
	  ERR("The firewire module (a.k.a juju stack) must be loaded, "
	      "and you must have read/write permissions to its device file."
	      << devicename);
	  goto err;
     }

     struct fw_cdev_get_info get_info;
     struct fw_cdev_event_bus_reset reset;
     memset(&get_info, 0, sizeof(get_info));
     memset(&reset, 0, sizeof(reset));
     get_info.version = FW_CDEV_VERSION;
     get_info.rom = 0;
     get_info.rom_length = 0;
     get_info.bus_reset = ptr_to_u64(&reset);
     retval = ioctl(d->fd, FW_CDEV_IOC_GET_INFO, &get_info);
     if (retval < 0) {
	  ERR("FW_CDEV_IOC_GET_INFO failed");
	  goto err;
     }
     d->abi_version = get_info.version;
     LOG("abi version "<<d->abi_version);

     assert( NULL == d->packets );
     d->packets = (fw_cdev_iso_packet*)malloc(num_frame * num_packet 
					      * sizeof(d->packets[0]));
     if (!d->packets)
	  goto err;

     memset(&create, 0, sizeof(create));
     create.type = FW_CDEV_ISO_CONTEXT_RECEIVE;
     create.header_size = get_header_size(d);
     create.channel = channel;
     retval = ioctl(d->fd, FW_CDEV_IOC_CREATE_ISO_CONTEXT, &create);
     if (retval < 0) {
	  ERR("FW_CDEV_IOC_CREATE_ISO_CONTEXT failed");
	  goto err;
     }
     d->isorxhandle = create.handle;
     LOG("isorxhandle "<< d->isorxhandle);

     d->sz_packet = sz_packet;
     d->num_packet = num_packet;
     d->buffer_size = (sz_packet + get_header_size(d)) * num_packet;
     d->num_frame = num_frame;
     d->index = 0;
     d->channel = channel;

     d->mmaped = (char*)mmap(NULL, d->buffer_size * d->num_frame, 
			     PROT_READ, MAP_SHARED, 
			     d->fd, 0);
     if (MAP_FAILED == d->mmaped) {
	  ERR("mmap() failed");
	  goto err;
     }

     for (i=0; i<d->num_frame; ++i) {
	  retval = setup_dma_desc(d, i);
	  if (retval < 0) {
	       ERR("setup_dma_desc() failed.");
	       goto err;
	  }
     }

    for (i=0; i<d->num_frame-1; ++i) {
	  retval = queue_dma_desc(d, i);
	  if (retval < 0) {
	       ERR("queue_dma_desc() failed.");
	       goto err;
	  }
     }

     retval = start_dma_desc(d);
     if (retval < 0) {
	  ERR("start_dma() failed");
	  goto err;
     }

     *header_size = get_header_size(d);
     return 0;
err:
     drv_juju_close(ctx);
     return -1;
}

static void *
drv_juju_updateFrameBuffer(libcam1394_driver *ctx,
			   C1394CameraNode::BUFFER_OPTION opt,
			   BufferInfo *info)
{
     struct pollfd fds[1];
     drv_juju_data *d = GETDATA(ctx);
     void *frame = NULL;

     assert(d);
     fds[0].fd = d->fd;
     fds[0].events = POLLIN;

     while (1) {
	  int retval;
	  int len;
	  union fw_cdev_event evt;

	  retval = poll(fds, 1, -1);
	  if (retval < 0) {
	       ERR("poll() failed.");
	       goto err;
	  } if (0==retval) {
	       LOG("poll() timedout");
	       continue;
	  } else {
	       LOG("poll() done");
	  }

	  len = read(d->fd, &evt, sizeof(evt));
	  if (len < sizeof(evt)) {
	       ERR("read() failed.");
	       goto err;
	  }

	  if (FW_CDEV_EVENT_ISO_INTERRUPT == evt.common.type) {
	       int prev = (d->index + d->num_frame - 1) % d->num_frame;

	       frame = d->mmaped + d->buffer_size*d->index;

	       retval = queue_dma_desc(d, prev);
	       if (retval < 0) {
		    ERR("queue_dma_desc() failed.");
		    goto err;
	       }

	       d->index = (d->index + 1)%d->num_frame;
	       break;
	  } else {
	       LOG("unknown event");
	  }
     }

     return d->mmaped + d->buffer_size*d->index;
err:
     return NULL;
}

static int 
drv_juju_getFrameCount(libcam1394_driver *ctx,
			    int *counter)
{
     ERR("juju doesn't support this function");
     return -1;
}

static int
drv_juju_setFrameCount(libcam1394_driver *ctx,
			    int counter)
{
     ERR("juju doesn't support this function");
     return -1;
}

#endif //  #if defined HAVE_JUJU

libcam1394_driver*
drv_juju_new()
{
#if defined HAVE_JUJU
     libcam1394_driver * drv;
     const int sz = sizeof(libcam1394_driver) + sizeof(drv_juju_data);
     drv_juju_data *d;

     drv = (libcam1394_driver*)malloc(sz);
     memset(drv, 0, sz);

     d = GETDATA(drv);
     d->fd = -1;

     drv->close =  drv_juju_close;
     drv->mmap = drv_juju_mmap;
     drv->getFrameCount = drv_juju_getFrameCount;
     drv->setFrameCount = drv_juju_setFrameCount;
     drv->updateFrameBuffer = drv_juju_updateFrameBuffer;

     return drv;
#else
     LOG("juju support is disabled");
     return NULL;
#endif 
}
