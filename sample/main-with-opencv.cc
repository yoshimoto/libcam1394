/*!
  @file    
  @brief   
  @version $Id: main-with-opencv.cc,v 1.2 2002-02-15 20:15:04 yosimoto Exp $
  @author  $Author: yosimoto $
  @date    $Date: 2002-02-15 20:15:04 $
 */

#include <cv.h>
#include <cvvis.h>
#include <stdio.h>
#include <errno.h>
#include <libraw1394/raw1394.h>  /* libraw1394関連 */
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> /* ドライバとのインタフェイス関連 */
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>


/* カメラのIDなど */
const int format=0; 
const int mode=1;    
const int frame_rate=3;
const int camera_id=166647; /* 使用するカメラのID */
const int channel=7;        /* チャネルは6番を使用 */

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
	  cout<<" there's no camera?? ."<<endl;
	  exit(-1);
     }
     CCameraList::iterator camera;
//  camera=find_camera_by_id(CameraList,MAKE_CHIP_ID(camera_id) );
     camera=CameraList.begin();
     if (camera==CameraList.end()){
	  cerr << " not found camera (id:="<<camera_id<<")" << endl;
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
     int loop=0;
     while (1){
	  camera->UpDateFrameBuffer();
	  camera->CopyIplImage(image);

	  cvvShowImage( "win", image );
     }

     exit(0);
}  

