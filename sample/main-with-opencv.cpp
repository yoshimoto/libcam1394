/*!
  @file    main-with-opencv.cpp
  @brief   sample code ( with OpenCV )
  @version $Id: main-with-opencv.cpp,v 1.4 2003-01-09 15:17:23 yosimoto Exp $
  @author  $Author: yosimoto $
  @date    $Date: 2003-01-09 15:17:23 $
 */


#include <stdio.h>
#include <errno.h>

#include <opencv/cv.h>          // for OpenCV
#include <opencv/highgui.h> 

#include <libraw1394/raw1394.h> // for libraw1394
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> // for libcam1394
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>


using namespace std;

const int format=0;    
const int mode=1;    
const int frame_rate=3;     
const int channel=7;   

int 
main(int argc, char **argv)
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
	  return false;
     }

     /* list up  all cameras on bus */
     CCameraList CameraList;
     if (! GetCameraList(handle,&CameraList) ){
	  fprintf(stderr, "couldn't get the list of cameras.\n");
	  return -1;
     }
     CCameraList::iterator camera;
     camera=CameraList.begin();
     if (camera==CameraList.end()){
	  fprintf(stderr, "There's no camera??.\n");
	  return -2;
     }

     /* set camera params */
     camera->StopIsoTx();
     camera->SetIsoChannel(channel);
     camera->SetFormat((FORMAT)format,(VMODE)mode,(FRAMERATE)frame_rate);
     camera->AllocateFrameBuffer();
     
     /* make a buffer and window */
     cvvInitSystem(argc, argv);
     cvvNamedWindow( "win", 0 );
     
     CvSize sz={camera->GetImageWidth(),
		camera->GetImageHeight()};
     IplImage *image = cvCreateImage( sz, IPL_DEPTH_8U, 3);

     /* create look-up table */
     ::CreateYUVtoRGBAMap();
     
     /* start */
     camera->StartIsoTx();

     /* show live images  */
     while (1){
	  camera->UpDateFrameBuffer();

	  camera->CopyIplImage(image);
	  
 	  cvvShowImage("win", image);
     }

     return 0;
}  

