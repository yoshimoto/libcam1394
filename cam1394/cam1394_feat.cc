// main.cc - cam1394 main routine
//
// YOSHIMOTO Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
//
// Thu Sep 27 10:09:24 2001  YOSHIMOTO Hiromasa 

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
#include <netinet/in.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <popt.h>
#include <string.h>

#include <libraw1394/raw1394.h> 
#include <libraw1394/csr.h>
#include <libraw1394/kernel-raw1394.h>
#include <libraw1394/raw1394_private.h>

#include "ohci1394_iso.h" 

#include "common.h"
#include "yuv.h"   
#include "xview.h" 
#include "1394cam.h"

// const 
const int NUM_PORT=16;  // number of 1394 interfaces 

void usage(poptContext optCon, int exitcode, char *error, char *addl) 
{
	poptPrintUsage(optCon, stderr, 0);
	if (error) 
	  cerr << error<<" : "<<addl << endl;
	exit(exitcode);
}

int main(int argc, char *argv[]){

  poptContext optCon;   /* context for parsing command-line options */
  char c;             /* for parse options */
  int channel=0;      /* iso channel */
  int card_no=0;      /* oh1394 interface number */
  int spd=2;          /* bus speed 0=100M 1=200M 2=400M */

  char *target_cameras=NULL; /* target camera(s) */
  char *save_filename=NULL;
  char *save_pattern=NULL;

  char *cp[TRIGGER+1]; /* camera's parameter. 
			  "NULL"  means the value isn't set. */

  FORMAT    cp_format = Format_X;
  VMODE     cp_mode   = Mode_X;
  FRAMERATE cp_rate   = FrameRate_X;
  
//  int r_value=-1;   /* white balance param */
//  int b_value=-1;

  enum {
    SM_DEFAULT,
    SM_START,
    SM_STOP
  } start_stop = SM_DEFAULT;

  enum {
    MD_DEFAULT,
    MD_LIST = 'L',
    MD_DISP = 'D',
    MD_INFO = 'I',
  } mode = MD_DEFAULT;

  struct poptOption optionsTable[] = {
    { "target",  'T', POPT_ARG_STRING, &target_cameras, 't',
      "set target camera(s)", "{CAM_ID|all}" },
    
    { "list", 'L',  POPT_ARG_NONE, &mode, MD_LIST,
      " list up all cameras on the bus.", NULL } ,    
    { "info", 'I',  POPT_ARG_NONE, &mode, MD_INFO,
      " show infomation camera", NULL } ,   
    { "disp", 'D',  POPT_ARG_NONE, &mode, MD_DISP,
      " display live image ", NULL } ,    
    { "start", 'S',  POPT_ARG_NONE, &start_stop, SM_START,
      " start camera(s) ", NULL } ,    
    { "stop",  'P',  POPT_ARG_NONE, &start_stop, SM_STOP,
      " stop  camera(s) ", NULL } ,    

    { "save", '\0',  POPT_ARG_STRING, &save_filename, 's',
      "save stream to a binary file. ", "FILENAME" } ,   
    { "save_ppm", '\0',  POPT_ARG_STRING, &save_pattern, 's',
      "save each image  to *.ppm. "
      "param can take printf format, like NAME%04d.ppm ", "FILENAME" } ,   

    
    { "channel", 'c',  POPT_ARG_INT, &channel, 'c',
      "channel", "CHANNEL" } ,
    { "format", 'f',  POPT_ARG_INT, &cp_format, 'f',
      "format",  "FMT"},
    { "mode", 'm',  POPT_ARG_INT, &cp_mode, 'm',
      "mode",  "MODE"},
    { "rate", 'r',  POPT_ARG_INT, &cp_rate, 'r',
      "rate",  "RATE"},

    { "speed",    's',  POPT_ARG_INT, &spd, 's',
      "bus speed (0=100M,1=200M,2=400M)", "SPD" } ,

    { "dev",      'd',  POPT_ARG_INT, &card_no, 'd',
      "device no", "DEVICE" } ,

    { "brightness", '\0', POPT_ARG_STRING, &cp[BRIGHTNESS], 'b',
      "brightness", "{BRIGHTNESS|off|auto|one_push}" } ,
    { "auto_exposure", '\0', POPT_ARG_STRING, &cp[AUTO_EXPOSURE], 'a',
      "auto exposure", "{AUTO_EXPOSURE|off|auto|one_push}" } ,
    { "sharpness", '\0', POPT_ARG_STRING, &cp[SHARPNESS], 's',
      "sharpness", "{SHARPNESS|off|auto|one_push}" } ,
    { "white_balance", '\0', POPT_ARG_STRING, &cp[WHITE_BALANCE], 'w',
      "white balance", "{WHITE_BALANCE|off|auto|one_push}" } ,
    { "hue", '\0', POPT_ARG_STRING, &cp[HUE], 'h',
      "hue", "{HUE|off|auto|one_push}" } ,
    { "saturation", '\0', POPT_ARG_STRING, &cp[SATURATION], 's',
      "saturation", "{SATURATION|off|auto|one_push}" } ,
    { "gamma", '\0', POPT_ARG_STRING, &cp[GAMMA], 'g',
      "gamma", "{GAMMA|off|auto|one_push}" } ,
    { "shutter", '\0', POPT_ARG_STRING, &cp[SHUTTER], 's',
      "shutter", "{SHUTTER|off|auto|one_push}" } ,
    { "gain", '\0', POPT_ARG_STRING, &cp[GAIN], 'g',
      "gain", "{GAIN|off|auto|one_push}" } ,
    { "iris", '\0', POPT_ARG_STRING, &cp[IRIS], 'i',
      "iris", "{IRIS|off|auto|one_push}" } ,
    { "focus", '\0', POPT_ARG_STRING, &cp[FOCUS], 'f',
      "focus", "{FOCUS|off|auto|one_push}" } ,
    { "temperature", '\0', POPT_ARG_STRING, &cp[TEMPERATURE], 't',
      "temperature", "{TEMPERATURE|off|auto|one_push}" } ,
    { "trigger", '\0', POPT_ARG_STRING, &cp[TRIGGER], 't',
      "trigger", "{TRIGGER|off|auto|one_push}" } ,
    
    POPT_AUTOHELP
    { NULL, 0, 0, NULL, 0 }
  };

  memset(cp,0,sizeof(cp));

  optCon = poptGetContext(NULL, argc,(const char**)argv, 
			  optionsTable, 0);
	
  if (argc < 2) {
    poptPrintUsage(optCon, stderr, 0);
    exit(1);
  }
	
  while ((c = poptGetNextOpt(optCon)) >= 0) {
  }

  if (c < -1) {
    /* an error occurred during option processing */
    cerr << poptBadOption(optCon, POPT_BADOPTION_NOALIAS)
	 <<" : "<< poptStrerror(c) <<endl;
    exit(1);
  }
  poptFreeContext(optCon);
  

  // get handle 
  raw1394handle_t handle;
  handle = raw1394_new_handle();
  if (!handle) {
    if (!errno) {
      ERR("not_compatible");
    } else {
      ERR("driver is not loaded");
    }
    return -3;
  }

  // list up  all cameras on the bus 
  //
  CCameraList CameraList;
  if (! GetCameraList(handle,&CameraList) ){
    ERR(" there's no camera.");
    return -1;
  }

  CCameraList TargetList;
  
  // select an camera if camera_id is specified.
  //
  if ( target_cameras == NULL ){
    CCameraList::iterator camera=CameraList.begin();
    if (camera==CameraList.end()){
      ERR(" no camera found ");
      return -2;
    }
    TargetList.insert( camera );
  } else if (!strcasecmp("all",target_cameras)){
    TargetList = CameraList;
  } else {
    char *end=NULL;
    int camera_id=strtol(target_cameras, &end, 0);
    if (*end!='\0'){
      ERR(" not a number " << target_cameras );
      exit(-1);      
    }
    CCameraList::iterator camera;
    camera=find_camera_by_id(CameraList,MAKE_CHIP_ID(camera_id) );
    if (camera==CameraList.end()){
      ERR(" not found camera (id:="<<camera_id<<") ");
      return -2;
    }
    TargetList.insert( camera );
  }


  CCameraList::iterator cam;
  //
  // set camera register for each feature.
  //
  for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
    C1394CAMERA_FEATURE feat;
    for (feat=BRIGHTNESS;
	 feat<TRIGGER;
	 feat=(C1394CAMERA_FEATURE)((int)feat+1)){

//    LOG(cam->GetFeatureName(feat)<<" "<<cp[feat]);

      if (!cp[feat])
	continue;

      if (!strcasecmp("off",cp[feat])){
	LOG("off "<<cam->GetFeatureName(feat)<<" feat ");
	cam->DisableFeature(feat);
      } else if (!strcasecmp("on",cp[feat])){
	cam->EnableFeature(feat);
//    } else if (!strcasecmp("manual",cp[feat])){
//      cam->AutoModeOff(feat);
      } else if (!strcasecmp("auto",cp[feat])) {
	LOG("set "<<cam->GetFeatureName(feat)<<" auto mode");
	cam->AutoModeOn(feat);
      } else if (!strcasecmp("one_push",cp[feat])) {
	LOG("set "<<cam->GetFeatureName(feat)<<" one_push mode");
	cam->OnePush(feat);
      } else {
	int val;
	char *end=NULL;
	val=strtol(cp[feat],&end,0);
	if (*end!='\0'){
	  ERR(cam->GetFeatureName(feat) 
	      << ": invalid param, " << cp[feat] );
	  exit(-1);
	}
	LOG("set "<<cam->GetFeatureName(feat)<<" feat "<<val);
	if (!cam->SetParameter(feat, val)){
	  ERR("specified parameter "<<cp[feat]
	      <<" is out of "<< cam->GetFeatureName(feat) 
	      << " range, skip..");
	}
      }
    }
  }  
  switch ( mode ){
  case MD_LIST:
    break;
  case MD_DISP:
    break;
  case MD_INFO:
    break;
  }

  //
  //
  switch ( start_stop ){
  case SM_START:
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
      cam->StartIsoTx();
    }
    break;
  case SM_STOP:
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
      cam->StopIsoTx();
    }
    break;
  }
  return 0;
}
