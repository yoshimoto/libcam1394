/*
 * main-dual.cc - 1394�����2�����ѥ���ץ�ץ����
 *
 * By Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>
// Thu Mar 15 18:23:53 2001 By YOSHIMOTO Hiromasa 
 */
#include <stdio.h>
#include <errno.h>

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

#define DWFV500_MAGICNUMBER 8589965664ULL
#define MAKE_CAMERA_ID(x) ((int64_t)(x)-DWFV500_MAGICNUMBER)
#define MAKE_CHIP_ID(x)   ((int64_t)(x)+DWFV500_MAGICNUMBER)


/* ������320x240(YUV422)@15fps �����ꤹ���� */
const int format=0; 
const int mode=1;    
const int frame_rate=3;

bool use_auto_mode = false; // �����ѥ�᡼����ưĴ������ʤ�� true

/* �����θ��̾��� */

struct camera_info {
     int id;
     int channel;
     int buf_count;
};

const int NUM_CAM=2;

struct camera_info cinfo[NUM_CAM]={
     // {camera's ID, channel, number of buffer}
     {165561, 4, 4},
     {100692, 3, 4},
};

CCameraList::iterator camera[NUM_CAM];  
CXview xview[NUM_CAM];

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
	  return false;
     }

     /* list up  all cameras on bus */
     CCameraList CameraList;
     if (! GetCameraList(handle,&CameraList) ){
	  fprintf(stderr, "couldn't get the list of cameras.\n");
	  return -1;
     }

     int i;
     for (i=0;i<NUM_CAM;i++){
	  camera[i]=find_camera_by_id(CameraList,
				      MAKE_CHIP_ID(cinfo[i].id) );
	  if (camera[i]==CameraList.end()){
	       fprintf(stderr, "there's no camera which id is %ld\n",
		       cinfo[i].id);
	       return -2;
	  }
	  camera[i]->StopIsoTx();
     }
 
     for (i=0;i<NUM_CAM;i++){
	  if (use_auto_mode){
	       camera[i]->AutoModeOn_All();
	  }else{
	       camera[i]->SetParameter(BRIGHTNESS,    0x90);
	       camera[i]->SetParameter(AUTO_EXPOSURE, 0x80);
	       camera[i]->SetParameter(SHARPNESS,     0x80);
	       camera[i]->SetParameter(WHITE_BALANCE, (0x80<<12)|0xe0);
	       camera[i]->SetParameter(HUE,           0x80);
	       camera[i]->SetParameter(SATURATION,    0x80);
	       // 0x82=liner
	       camera[i]->SetParameter(GAMMA,         0x82);
	       // shutter speed 1/30sec=0x800 1/20,000sec=0xa0d
	       camera[i]->SetParameter(SHUTTER,       0x800); 
	       camera[i]->SetParameter(GAIN,          0x02); // def=0x00
	  }
     }

     for (i=0;i<NUM_CAM;i++){
	  /* set camera params */
	  camera[i]->SetIsoChannel(cinfo[i].channel);
	  camera[i]->SetFormat((FORMAT)format,(VMODE)mode,(FRAMERATE)frame_rate);
	  camera[i]->AllocateFrameBuffer();

	  /* make a Window */
	  char tmp[256];
	  sprintf(tmp,"-- Live image from #%5d/ %2dch --",
		  (int)MAKE_CAMERA_ID(camera[i]->m_ChipID),cinfo[i].channel );
	  
	  if (!xview[i].CreateWindow(camera[i]->GetImageWidth(),
				     camera[i]->GetImageHeight(), tmp))
	  {
	       fprintf(stderr, " couldn't create X window.");
	       return -1;
	  }
     }
     

     /* create look-up table */
     ::CreateYUVtoRGBAMap();
  
     /* start */
     for (i=0;i<NUM_CAM;i++)
	  camera[i]->StartIsoTx();
     
     /* show live images  */
     RGBA *image[NUM_CAM];
     for (i=0;i<NUM_CAM;i++){
	  int sz = camera[i]->GetImageWidth() * camera[i]->GetImageHeight();
	  image[i] = new RGBA[sz];
     }

     while (1){
	  /* UpDateFrameBuffer() �ϲ��������Ȥꤪ����ޤ�
	     return ���Ƥ��ޤ�������ʤ��̥���åɤȤ���٤��Ǥ���
	     ���Υ���ץ�ϲ���ͤ�����ߤ�ɽ���򷫤��֤��ޤ���*/
	  camera[0]->UpDateFrameBuffer();
	  camera[0]->CopyRGBAImage(image[0]);
	  xview[0].UpDate(image[0]);

	  camera[1]->UpDateFrameBuffer();
	  camera[1]->CopyRGBAImage(image[1]);
	  xview[1].UpDate(image[1]);
     }

     exit(0);
} 
