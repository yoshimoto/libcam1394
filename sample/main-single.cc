/*
 * main-single.cc - 1394�����1�����ѥ���ץ�ץ����
 *
 * By Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>

// Thu Mar 15 17:53:25 2001  YOSHIMOTO Hiromasa 1st
// Mon Dec 10 21:19:32 2001  YOSHIMOTO Hiromasa 
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <features.h>
#include <netinet/in.h> /* �Х��ȥ��������Ѵ� */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <libraw1394/raw1394.h>  /* libraw1394��Ϣ */
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> /* �ɥ饤�ФȤΥ��󥿥ե�������Ϣ */
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>     /* ���Ѵ� */

#include "xview.h"              /* X��ɽ�����֥�������*/

/* �����γƼ����
   �������ͤ��ѹ����뤳�Ȥ��͡��ʷ����ǲ���������Ǥ���Ϥ��Ǥ���
   ����Ū�ʿ��ͤˤĤ��Ƥ�1394-based Digital Camera Spec��
   �ºݤ˻��Ѥ���1394�����λ��ͽ����ǳ�ǧ���Ƥ���������*/

/* ������320x240(YUV422)@30fps �����ꤹ���� */
const int W=320;
const int H=240;
const int format=0; 
const int mode=1;    
const int frame_rate=4;

/* ������ID�ʤ� */
const int camera_id=100692; /* ���Ѥ��륫����ID */
const int channel=6;        /* ����ͥ��6�֤���� */
const int buf_count=4;      /* 4�ե졼��ʬ�����ѥХåե������ */

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
  camera=find_camera_by_id(CameraList,MAKE_CHIP_ID(camera_id) );
  if (camera==CameraList.end()){
    cerr << " not found camera (id:="<<camera_id<<")" << endl;
    return -2;
  }

  /* set camera params */
  camera->StopIsoTx();
  camera->SetIsoChannel(channel);
  camera->SetFormat((FORMAT)format,(VMODE)mode,(FRAMERATE)frame_rate);
  camera->AllocateFrameBuffer();

#if 0
  camera->AutoModeOn();
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

  /* make a Window */
  char title[256];
  sprintf(title,"-- Live image from #%5d/ %2dch --",
	  (int)MAKE_CAMERA_ID(camera->m_ChipID),channel );
  CXview xview;
  if (!xview.CreateWindow(W,H,title)){
    cerr << " failure @ create X window" << endl;
    return -1;
  }

  /* create look-up table */
  ::CreateYUVtoRGBAMap();

  /* start */
  camera->StartIsoTx();

  /* show live images  */
  int loop=0;
  while (1){
    RGBA tmp[W*H];
    camera->UpDateFrameBuffer();
    camera->CopyRGBAImage(tmp);


    xview.UpDate(tmp);
  }

  exit(0);
}  
