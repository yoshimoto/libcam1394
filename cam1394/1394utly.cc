//
// 1394utly.cc
//
// Copyright (C) 2000 by Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>
//
// for libraw1394-0.x
//
// Wed Dec  8 21:14:36 1999 by yosimoto 1st
// Mon May 29 10:47:52 2000 By hiromasa yoshimoto 
//
#include <iostream>
#include <iomanip>
using namespace std;
#include <stdio.h>
#include <unistd.h>
#include <string.h> // for bzero()
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <features.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
#include <libraw1394/kernel-raw1394.h>
#include <libraw1394/raw1394_private.h>

#include <ohci1394_iso.h>
#include <1394cam.h>
#include <yuv.h>
#include <xview.h>

#include "common.h"
#include "1394utly.h"


#define WAIT usleep(100)

#define HEX(var)     hex<<setw(8)<<var<<dec
#define VAR32(var)   #var" := 0x"<<HEX(var)
#define VAR(var)     #var" :="<<var
#define CHR(var)    CHR_( ((var)&0xff) )
#define CHR_(c)     ((0x20<=(c) && (c)<='z')?(char)(c):'.')
#define EQU(val,mask,pattern)  (0==(((val)&(mask))^(pattern)))
#define SZ(v)  (sizeof(v)>>2)

static char usage[] = 
" \n 1394-based Digital Video Camera Controller Version 0.5c \n"
" by hiromasa yoshimoto / yosimoto@limu.is.kyushu-u.ac.jp \n"
" \n"
" options: \n"
"  -info    [camera's-id]  \n"
"  -set      camera's-id  [format [mode [rate]]] \n"  
"  -stop    [camera's-id]  \n"
"  -start    camera's-id   \n"
"  -display  camera's-id   \n" 
"   \n"; 



int 
display_current_params(C1394CameraNode& cam)
{
  int i;
  for (i=0;i<12;i++){ // FIX ME. 
    unsigned int tmp;
    if (cam.GetParameter((C1394CAMERA_FEATURE)i,&tmp)){
      char str[20];
      snprintf(str,sizeof(str),
	       "%15s",cam.GetFeatureName((C1394CAMERA_FEATURE)i));
      if ( i==WHITE_BALANCE || i==TEMPERATURE )
	cout << str << ": " << (tmp>>12) <<"/"<< (tmp&((1<<12)-1)) << endl;
      else
	cout << str << ": " << tmp << endl;
    }else{
      char str[20];
      snprintf(str,sizeof(str),
	       "%15s",cam.GetFeatureName((C1394CAMERA_FEATURE)i));
      cout << str << ": not available."<<endl;
    }
  }
  return 0;
}
  
int 
display_current_status(C1394CameraNode& camera)
{
  int channel;
  camera.QueryIsoChannel(&channel);
  cout << " camera id: " << MAKE_CAMERA_ID(camera.m_ChipID);
  cout << " ch: " << channel <<" ";

//  cout << camera->GetImageWidth() <<"x"<< camera->GetImageHeight() ;

  FORMAT fmt;
  VMODE  mode;
  FRAMERATE rate;
  camera.QueryFormat(&fmt,&mode,&rate);
  cout << ::GetImageWidth(fmt,mode) <<"x"<< ::GetImageHeight(fmt,mode) 
       <<" "<< ::GetVideoFormatString(fmt,mode) 
       <<"@"<< 1.875f*(1<<rate) <<"fps";
  SPD speed;
  camera.QueryIsoSpeed(&speed);
  cout << " "<<::GetSpeedString(speed) <<endl;
  return 0;
}

void
start_transmit(C1394CameraNode& camera)
{
  /* allocate Isochronus resources */

  /* start transmit video data.*/
  camera.StartIsoTx();
}

void
stop_transmit(C1394CameraNode& camera)
{
  /* stop transmit video data.*/
  camera.StopIsoTx();  
  /* release Isochronus resources */
}
void
reset_camera(C1394CameraNode& camera)
{
  camera.ResetToInitialState();
}
void
preset_camera(C1394CameraNode& camera)
{
  camera.PreSet_All();
}


int 
disp_help(char* str)
{
  cout << endl
       << str 
       << usage <<endl;
  return 0;
}

void
display_on_x(C1394CameraNode& camera)
{
  int channel;

  camera.QueryIsoChannel(&channel);

  /* force release !! FIXME! */

  /* allocate receive buffer */
  if (camera.AllocateFrameBuffer()){
    LOG("err: failure @ AllocateFrameBuffer() ");
    return ;
  }

  const int w = camera.GetImageWidth();
  const int h = camera.GetImageHeight();


  /* make a Window */
  char tmp[256];
  sprintf(tmp,"-- Live image from #%5d/ %2dch --",
	  (int)MAKE_CAMERA_ID(camera.m_ChipID),channel );

  CXview xview;
  if (!xview.CreateWindow(w,h,tmp)){
    LOG(" failure @ create X window");
    return ;
  }
  
  while (1){
    RGBA tmp[w*h];
    camera.UpDateFrameBuffer();
    camera.CopyRGBAImage(tmp);
    xview.UpDate(tmp);
  }

  /* release buffer */
  /* release  channel */
  camera.QueryIsoChannel(&channel);
}


void
display_bgs_on_x(C1394CameraNode& camera)
{
  int channel;

  camera.QueryIsoChannel(&channel);

  /* force release !! FIXME! */

  /* allocate receive buffer */

  const int w = camera.GetImageWidth();
  const int h = camera.GetImageHeight();


  /* make a Window */
  char tmp[256];
  sprintf(tmp,"-- Live image (bgs) from #%5d/ %2dch --",
	  (int)MAKE_CAMERA_ID(camera.m_ChipID),channel );

  CXview xview;
  if (!xview.CreateWindow(w,h,tmp)){
    LOG(" failure @ create X window");
    return ;
  }

  RGBA buf[2][w*h];
  RGBA 
    *pri=buf[0],
    *back=buf[1];
  
  RGBA th(20,20,20,0);
  while (1){
    RGBA tmp[w*h];
    
    camera.UpDateFrameBuffer();

    camera.CopyRGBAImage(pri);

    printf("%d\n",
	   mmx_bgs_ex(tmp,pri,back,w*h,th));

    xview.UpDate(tmp);
    swap(back,pri);
  }

  /* release buffer */
  /* release  channel */
  camera.QueryIsoChannel(&channel);
}

int
savetofile(C1394CameraNode& camera,int fd)
{
  int channel;

  camera.QueryIsoChannel(&channel);

  /* force release !! */


  /* allocate receive buffer */
  if (camera.AllocateFrameBuffer()){
    cerr<<": failure @ AllocateFrameBuffer() "<<endl;
    return -2;
  }

  const int w =camera.GetImageWidth();
  const int h =camera.GetImageHeight();

  const int frame_size= camera.GetFrameBufferSize();

  int counter=0;
  while (1){
    char* p=(char*)camera.UpDateFrameBuffer();
    write( fd , p , frame_size );
  }
  
  /* release buffer */
  /* release  channel */
  camera.QueryIsoChannel(&channel);
  return 0;
}



int 
savetoppm(C1394CameraNode& camera,char *fname)
{
  int channel;

  camera.QueryIsoChannel(&channel);

  /* force release !! */


  /* allocate receive buffer */
  if (camera.AllocateFrameBuffer()){
    cerr<<": failure @ AllocateFrameBuffer() "<<endl;
    return -2;
  }

  int counter=0;
  while (!0){
    char tmp[MAX_FILENAME];
    snprintf(tmp,MAX_FILENAME,fname,counter++);
    camera.UpDateFrameBuffer();
    camera.SaveToFile(tmp);
  }
  
  /* release buffer */
  /* release  channel */
  camera.QueryIsoChannel(&channel);
  return 0;
}



//end of [ camera-tools.cc ]


