/*!
  @file    
  @brief   
  @version $Id: main-with-opencv.cc,v 1.1 2002-02-15 19:50:24 yosimoto Exp $
  @author  $Author: yosimoto $
  @date    $Date: 2002-02-15 19:50:24 $
 */

#include <cv.h>
#include <cvvis.h>
#include <stdio.h>
#include <errno.h>
#include <libraw1394/raw1394.h>  /* libraw1394��Ϣ */
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> /* �ɥ饤�ФȤΥ��󥿥ե�������Ϣ */
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>


/* ������ID�ʤ� */
const int format=0; 
const int mode=1;    
const int frame_rate=3;
const int camera_id=166647; /* ���Ѥ��륫����ID */
const int channel=7;        /* ����ͥ��6�֤���� */

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

#if !0
     camera->AutoModeOn_All();
#else
     camera->SetParameter(BRIGHTNESS,    0x80);
     camera->SetParameter(AUTO_EXPOSURE, 0x80);
     camera->SetParameter(SHARPNESS,     0x80);
     // default 0x80080
     camera->SetParameter(WHITE_BALANCE, (0x80<<12)|0xe0);
     camera->SetParameter(HUE,           0x80);
     camera->SetParameter(SATURATION,    0x80);
     camera->SetParameter(GAMMA,         0x82); // 0x82=liner
     // shutter speed 1/30sec=0x800 1/20,000sec=0xa0d
     camera->SetParameter(SHUTTER,       0x800); 
     camera->SetParameter(GAIN,          0x02); // def=0x00
#endif


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
