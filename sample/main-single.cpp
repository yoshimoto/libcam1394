/*
 * $Id: main-single.cpp,v 1.9 2004-07-14 09:26:23 yosimoto Exp $
 * main-single.cc - 1394�����1�����ѥ���ץ�ץ����
 *
 * By Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>
 * Thu Mar 15 17:53:25 2001  YOSHIMOTO Hiromasa 
 * Sun Sep 23 18:10:50 2001  YOSHIMOTO Hiromasa 
 */
#include <stdio.h>
#include <errno.h>

#include <libcam1394/1394cam.h> /* libcam1394���� */
#include <libcam1394/yuv.h>     /* ���Ѵ� */

#include "xview.h"              /* X��ɽ�� */

/* �����γƼ����
   �������ͤ��ѹ����뤳�Ȥ��͡��ʷ����ǲ���������Ǥ���Ϥ��Ǥ���
   ����Ū�ʿ��ͤˤĤ��Ƥ�1394-based Digital Camera Spec��
   �ºݤ˻��Ѥ���1394�����λ��ͽ����ǳ�ǧ���Ƥ���������*/

/* ������320x240(YUV422)@15fps �����ꤹ���� */
const int format=0; 
const int mode=1;    
const int frame_rate=3;

const int channel   = 7;      /* ����ͥ��7�֤���� */
const int buf_count = 4;      /* 4�ե졼��ʬ�μ����ѥХåե������ */

bool use_auto_mode = false; // �����ѥ�᡼����ưĴ������ʤ�� true

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
	  fprintf(stderr, " couldn't get the list of cameras.");
	  return -1;
     }

     /* select one camera */
     CCameraList::iterator camera;
     camera = CameraList.begin();
     if (camera == CameraList.end()){
	  fprintf(stderr, " there's no camera?? .");
	  return -2;
     }

     /* set camera params */
     camera->StopIsoTx();
     camera->SetIsoChannel(channel);
     camera->SetFormat((FORMAT)format,(VMODE)mode,(FRAMERATE)frame_rate);
     camera->AllocateFrameBuffer();

     if (use_auto_mode){
	  camera->AutoModeOn_All();
     }else{
	  camera->SetParameter(BRIGHTNESS,    0x90);
	  camera->SetParameter(AUTO_EXPOSURE, 0x80);
	  camera->SetParameter(SHARPNESS,     0x80);
	  camera->SetParameter(WHITE_BALANCE, (0x80<<12)|0xe0);
	  camera->SetParameter(HUE,           0x80);
	  camera->SetParameter(SATURATION,    0x80);
	  // 0x82=liner
	  camera->SetParameter(GAMMA,         0x82); 
	  // shutter speed  
	  // 1/30sec  2048
	  // 1/50sec  2258 
	  // 1/75sec  2363
	  // 1/100sec 2416
	  camera->SetParameter(SHUTTER,       2048); 
	  camera->SetParameter(GAIN,          0x02);
     }

     /* make a window */
     char tmp[256];
     sprintf(tmp,"-- Live image from #%8lx/ %2dch --",
	     (long int)camera->m_ChipID, channel );
     
     int width  = camera->GetImageWidth();
     int height = camera->GetImageHeight();
     CXview xview;
     if (!xview.CreateWindow(width, height, tmp)){
	  fprintf(stderr, " couldn't create X window.");
	  return -1;
     }

     /* create look-up table for color space conversion */
     CreateYUVtoRGBAMap();

     /* start */
     camera->StartIsoTx();
     
     RGBA* image = new RGBA[width*height];
     /* show live images  */
     while (1){
	  camera->UpDateFrameBuffer();
	  camera->CopyRGBAImage(image);
	  xview.UpDate(image);
     }

     return 0;
}
  
