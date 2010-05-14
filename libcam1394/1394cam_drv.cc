/**
 * @file   1394cam_drv.cc
 * @author Hiromasa YOSHIMOTO
 * @date   Thu May  6 23:07:02 2010
 * 
 * @brief  
 * 
 * 
 */

#include "config.h"
#include <malloc.h>
#include <assert.h>
#include "1394cam_drv.h"

libcam1394_driver *
open_1394_driver(int port_no, const char *devicename, 
		 int channel,
		 int sz_packet, int num_packet,
		 int num_frame)
{

     extern libcam1394_driver * drv_juju_new();
     extern libcam1394_driver * drv_video1394_new();
     extern libcam1394_driver * drv_isofb_new();

     typedef libcam1394_driver* (*func_t)();
     static const func_t constructor_tabel[] = {
	  drv_juju_new,
	  drv_video1394_new,
	  drv_isofb_new,
	  NULL,
     };
     const func_t* func = constructor_tabel;

     libcam1394_driver * drv = NULL;

     for (; *func; ++func) {
	  drv = (*func)();
	  int r;
	  r = drv->mmap(drv, port_no, devicename,
			channel, 
			sz_packet, num_packet, num_frame);
	  if (0 == r) {
	       // found suitable one, break
	       break;
	  }
	  close_1394_driver(&drv);
	  assert(0 == drv);
     }

     return drv;
}

void 
close_1394_driver(libcam1394_driver **p)
{
    if (*p) {
	free( *p );
	*p = NULL;
    }
}
