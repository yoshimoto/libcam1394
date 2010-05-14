/**
 * @file   drv_isofb.cc
 * @author Hiromasa YOSHIMOTO
 * @date   Thu May  6 23:05:35 2010
 * 
 * @brief  a driver for isofb kernel module
 * 
 * 
 */

#include "config.h"
#include "1394cam_drv.h"

#if defined HAVE_ISOFB
# include <linux/ohci1394_iso.h>

static void* mmap_isofb(int port_no, int channel,
			int m_packet_sz, int m_num_packet,
			int *m_num_frame,
			int *fd, int *m_BufferSize)
{
    void *buffer=NULL;

    char devname[1024];
    snprintf(devname, sizeof(devname), "/dev/isofb%d", port_no);
    LOG("open("<<devname<<")");
    *fd=open(devname,O_RDONLY);
    if (*fd < 0){
	ERR("Failed to open isofb device (" 
	    << devname << ") : " << strerror(errno));
	ERR("The ohci1394_fb module must be loaded, "
	    "and you must have read and write permission to "<<devname<<".");
	goto err;
    }
  
    ISO1394_RxParam rxparam;
    if (0>ioctl(*fd, IOCTL_INIT_RXPARAM, &rxparam)){
	ERR("IOCTL_INIT_RXPARAM failed");
	goto err;
    }
  
    rxparam.sz_packet      = m_packet_sz ;
    rxparam.num_packet     = m_num_packet ;
    rxparam.num_frames     = *m_num_frame;
    rxparam.packet_per_buf = 0;
    rxparam.wait           = 1;
    rxparam.sync           = 1;
    rxparam.channelNum     = channel ;

    if (0 > ioctl(*fd, IOCTL_CREATE_RXBUF, &rxparam) ){
	ERR("IOCTL_CREATE_RXBUF failed");
	goto err;
    }
    
    buffer = mmap(NULL,
		  rxparam.sz_allocated,
		  PROT_READ,
		  MAP_SHARED, 
		  *fd,0);
    if (buffer == MAP_FAILED){
	ERR("isofb_mmap failed");
	goto err;
    }


    if (0 >  ioctl(*fd, IOCTL_START_RX) ){
	ERR("IOCTL_START_RX failed");
	goto err;
    }

    *m_BufferSize = m_packet_sz*m_num_packet;

    return buffer;

err:
    if (*fd>0) close(*fd);
    *fd=-1;
    return NULL;
}


static void *
drv_video1394_updateFrameBuffer(libcam1394_driver *ctx,
				BUFFER_OPTION opt, BufferInfo *info)
{
     ISO1394_Chunk chunk;
     if (0 > ioctl(fd, IOCTL_GET_RXBUF, &chunk) ){
	  ERR("ISOFB_IOCTL_GET_RXBUF failed:  " << strerror(errno) );
	  return NULL;
     }
     void *m_lpFrameBuffer=(pMaped+chunk.offset);
     return m_lpFrameBuffer;
}

#endif // #if defined HAVE_ISOFB

libcam1394_driver*
drv_isofb_new()
{
     return NULL;
}

