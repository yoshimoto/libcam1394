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

#include <linux/ohci1394_iso.h> /* ドライバとのインタフェイス関連 */
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>     /* 色変換 */
#include "common.h"
#include "xview.h"              /* Xの表示オブジェクト*/


// const 
const int NUM_PORT=16;  // number of 1394 interfaces 

void usage(poptContext optCon, int exitcode, char *error, char *addl) 
{
	poptPrintUsage(optCon, stderr, 0);
	if (error) 
	  cerr << error<<" : "<<addl << endl;
	exit(exitcode);
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


int main(int argc, char *argv[]){

  poptContext optCon;   /* context for parsing command-line options */
  char c;             /* for parse options */
  int channel=0;      /* iso channel */
  int card_no=0;      /* oh1394 interface number */
  int spd=2;          /* bus speed 0=100M 1=200M 2=400M */

  const char *target_cameras=NULL; /* target camera(s) */
  const char *save_filename=NULL;
  const char *save_pattern=NULL;

  const char *cp[TRIGGER+1]; /* camera's parameter. 
			  "NULL"  means the value isn't set. */

  FORMAT    cp_format = Format_X;
  VMODE     cp_mode   = Mode_X;
  FRAMERATE cp_rate   = FrameRate_X;
  
//  int r_value=-1;   /* white balance param */
//  int b_value=-1;
  
  int  do_list=-1;
  int  do_info=-1;
  int  do_disp=-1;
  int  do_save_bin=-1;
  int  do_save_ppm=-1;
  int  do_oneshot=-1;

  int do_start=-1;
  int do_stop=-1;

  struct poptOption optionsTable[] = {
      
    { "list", 'L',  POPT_ARG_NONE, &do_list, 'L',
      " list up all cameras on the bus.", NULL } ,    
    { "info", 'I',  POPT_ARG_NONE, &do_info, 'I',
      " show infomation camera", NULL } ,   
    { "disp", 'D',  POPT_ARG_NONE, &do_disp, 'D',
      " display live image ", NULL } ,    
    { "start", 'S',  POPT_ARG_NONE, &do_start, 'S',
      " start camera(s) ", NULL } ,    
    { "stop",  'P',  POPT_ARG_NONE, &do_stop, 'P',
      " stop  camera(s) ", NULL } ,    

    { "oneshot", 'O',  POPT_ARG_NONE, &do_oneshot, 'B',
      "save stream to a binary file. ",NULL } ,   
    { "save",  'X',    POPT_ARG_NONE, &do_save_bin, 'B',
      "save stream to a binary file. ",NULL } ,   
    { "save_ppm", 'Y', POPT_ARG_NONE, &do_save_ppm, 'P',
      "save each image", NULL},

    
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

  poptSetOtherOptionHelp(optCon, "{[CAMERA_ID]..|all}");
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
  target_cameras = poptGetArg(optCon);
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
    CCameraList::iterator cam=CameraList.begin();
    if (cam==CameraList.end()){
      ERR(" no camera found ");
      return -2;
    }
    TargetList.push_back( *cam );
  } else if (!strcasecmp("all",target_cameras)){
    TargetList = CameraList;
  } else {
    char *end=NULL;
    int camera_id=strtol(target_cameras, &end, 0);
    if (*end!='\0'){
      ERR(" please set CAMERA_ID or all " << target_cameras );
      exit(-1);      
    }
    CCameraList::iterator cam;
    cam=find_camera_by_id(CameraList,MAKE_CHIP_ID(camera_id) );
    if (cam==CameraList.end()){
      ERR(" not found camera which id is "<<camera_id);
      return -2;
    }
    TargetList.push_back( *cam );
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

  // start camere(s)
  if (do_info!=-1){
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
     display_current_status(*cam);
    }    
  }

  // start camere(s)
  if (do_start!=-1){
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
      LOG("start");
      cam->StartIsoTx();
    }
  }

  // stop camere(s)
  if (do_stop!=-1){
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
      LOG("stop");
      cam->StopIsoTx();
    }
  }

  // save camere(s) image
  if (do_save_bin!=-1){
    cam=TargetList.begin();
    char fname[1024];
    snprintf(fname,sizeof(fname),
	     "%d.yuv",MAKE_CAMERA_ID(cam->m_ChipID));
    LOG("save to " << fname);
    
    int fd=creat(fname,  S_IRUSR|S_IWUSR);
    if (0==fd){
      ERR( "can't open file : "<<fname);
    }else{
      savetofile(*cam,fd);
    }
  }

  return 0;
}
