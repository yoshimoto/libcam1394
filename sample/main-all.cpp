/**
 * @file   main-all.cpp
 * @author Hiromasa YOSHIMOTO
 * @date   Fri Dec 19 06:41:37 2003
 * 
 * @brief  
 * 
 * 
 */

#include <vector>
#include <stdio.h>
#include <errno.h>

#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>     /* for conversion yuv to rgb */

#include "xview.h"    

std::vector<CXview*> xview;

int main(int argc, char **argv)
{
     /* get handle*/
     raw1394handle_t handle;
     handle = raw1394_new_handle();
     if (!handle) {
	  if (!errno) {
	       printf("not_compatible");
	  } else {
	       perror("couldn't get handle");
	       printf("driver is not loaded");
	  }
	  return -1;
     }

     /* list up  all cameras on bus */
     CCameraList CameraList;
     if (! GetCameraList(handle,&CameraList) ){
	  fprintf(stderr, "couldn't get the list of cameras.\n");
	  return -1;
     }
     
     CCameraList::iterator cam;
     // stop all cameras on 1394-bus.
     for (cam=CameraList.begin(); cam!=CameraList.end(); cam++){
	  cam->StopIsoTx();
     }

     // set up camera 
     int channel=0;
     for (cam=CameraList.begin(); cam!=CameraList.end(); cam++){

	  // at least, you have to set the iso channel.
	  cam->SetIsoChannel( channel );

	  cam->AllocateFrameBuffer();

	  /* make window to show live images. */
	  char tmp[256];
	  sprintf(tmp,"Live image from ch.#%d", channel );

	  CXview *view = new CXview;
	  if (!view->CreateWindow(cam->GetImageWidth(),
				 cam->GetImageHeight(), tmp))
	  {
	      fprintf(stderr, " couldn't create X window.");
	      return -1;
	  }	  

	  xview.push_back( view );

	  // increment channel. 
	  channel++;
     }
     
     
     /* create look-up table */
     ::CreateYUVtoRGBAMap();
  

     // kick cameras to start sending images.
     for (cam=CameraList.begin(); cam!=CameraList.end(); cam++)
	  cam->StartIsoTx();
     
     /* show live images  */
     int n=0;
     RGBA *image[ xview.size() ];
     for (cam=CameraList.begin(); cam!=CameraList.end(); cam++){
	  int sz = cam->GetImageWidth() * cam->GetImageHeight();
	  image[n++] = new RGBA[sz];
     }

     while (1){
	  int n=0;
	  for (cam=CameraList.begin(); cam!=CameraList.end(); cam++){	
	       cam->UpdateFrameBuffer();
	       cam->CopyRGBAImage(image[n]);
	       xview[n]->UpDate(image[n]);
	       n++;
	  }
     }

     return 0;
} 
