//
// 1394utly.h - 1394-based Digital Camera control class
//
// Copyright (C) 2000,2001 by YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp> 
//
// Sat Dec 11 05:50:55 1999 1st
// Wed Aug  1 13:44:09 2001 

#if !defined(_1394utly_h_include_)
#define _1394utly_h_include_

#define WAIT usleep(100)
#define MAX_FILENAME 512
enum CMD {
  CMD_DISPLAY_INFO  =1<<0,
  CMD_DISPLAY_X     =1<<1,
  CMD_PARAM_SET     =1<<2,
  CMD_CAMERA_START  =1<<3,
  CMD_CAMERA_STOP   =1<<4,
  CMD_AUTOMODE_ON   =1<<5,
  CMD_AUTOMODE_OFF  =1<<6,
  CMD_SET_IMAGE_FEATURE=1<<7,
  CMD_SAVE  =1<<10,
  CMD_TAKE_PICTURE  =1<<11,
  CMD_RESET        =1<<12,
  CMD_TRIGGER_ON    =1<<13,
  CMD_TRIGGER_OFF   =1<<14,
  CMD_BGS_X         =1<<15,
  CMD_PPM           =1<<16,
  CMD_DUMP_FEAT    =1<<17,
  CMD_PRESET_FEAT    =1<<18,
};

void
reset_camera(C1394CameraNode& camera);
void
preset_camera(C1394CameraNode& camera);

int 
display_current_status(C1394CameraNode& camera);
int 
display_current_params(C1394CameraNode& camera);
int 
disp_help(char* str);
void
start_transmit(C1394CameraNode& camera);
void
stop_transmit(C1394CameraNode& camera);
void
display_on_x(C1394CameraNode& camera);
void
display_bgs_on_x(C1394CameraNode& camera);
int
savetofile(C1394CameraNode& camera,int fd);
int
savetoppm(C1394CameraNode& camera,char *);

#endif //#if !defined(_1394utly_h_include_)
