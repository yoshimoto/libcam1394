/*!
  @file  cam1394.cc
  @brief cam1394 main 
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: cam1394.cc,v 1.3 2002-03-13 13:59:50 yosimoto Exp $
  @date    $Date: 2002-03-13 13:59:50 $
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
#include <netinet/in.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <popt.h>
#include <string.h>

#include <libraw1394/raw1394.h> 
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> /* ドライバとのインタフェイス関連 */
#include "1394cam.h"
#include "yuv.h"                /* 色変換 */
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

  cout << fmt <<endl;
  cout << mode <<endl;
  cout << rate <<endl;


  return 0;
}

int
savetofile(C1394CameraNode& camera,char *fname)
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

  
     int last_frame;
     camera.GetFrameCount(&last_frame);
     int total_drop=0;
  
     int f_count=0;
     while (true)
     {
	  char str[1024];
	  sprintf(str, fname, f_count);
	  cout << str << endl;
       
	  int flag=0;
	  flag |= O_CREAT|O_WRONLY|O_EXCL;
//        flag |= O_SYNC;
//        flag |= O_LARGEFILE;
         
	  int fd=open(str, flag ,S_IRUSR|S_IWUSR);
	  if ( 0 > fd ){
	       ERR( "can't open file : "<<str);
	       return -2;
	  }
         
	  const int NUM_SEG=1500;

	  int i=0;
	  for (i=0; i<NUM_SEG; i++){
              
	       char* p=(char*)camera.UpDateFrameBuffer();

	       int frame_no;
	       camera.GetFrameCount(&frame_no);
	       if ( last_frame+1 != frame_no ){
		    total_drop++;
		    ERR(" drop "
			<<"(rate "
			<< (float)total_drop/(f_count*NUM_SEG + i)
			<<"%)");
	       }
	       last_frame = frame_no;
	       //cout << last_frame << endl;

	       int result = write( fd , p , frame_size );
	       if ( result != frame_size ){
		    ERR(" write() says " << strerror(errno) );
	       }
	  }
	  close(fd);
	  f_count++;
     }

     /* release buffer */
     /* release  channel */
     camera.QueryIsoChannel(&channel);
     return 0;
}


int display_live_image_on_X(C1394CameraNode &cam)
{
  int channel;

  cam.QueryIsoChannel(&channel);

  /* force release !! FIXME! */

  /* allocate receive buffer */
  if (cam.AllocateFrameBuffer()){
    LOG("err: failure @ AllocateFrameBuffer() ");
    return -1;
  }

  const int w = cam.GetImageWidth();
  const int h = cam.GetImageHeight();


  /* make a Window */
  char tmp[256];
  sprintf(tmp,"-- Live image from #%5d/ %2dch --",
	  (int)MAKE_CAMERA_ID(cam.m_ChipID),channel );

  CXview xview;
  if (!xview.CreateWindow(w,h,tmp)){
    LOG(" failure @ create X window");
    return -2;
  }
  
  while (1){
    RGBA tmp[w*h];
    cam.UpDateFrameBuffer();
    cam.CopyRGBAImage(tmp);
    xview.UpDate(tmp);
  }
}

int main(int argc, char *argv[]){

     poptContext optCon;   /* context for parsing command-line options */
     char c;             /* for parse options */
     int channel=-1;      /* iso channel */
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

     bool is_all=false; // if target cameras are all camera, then set true

     struct poptOption control_optionsTable[] = {
	  { "start", 'S',  POPT_ARG_NONE, &do_start, 'S',
	    " start camera(s) ", NULL } ,    
	  { "stop",  'P',  POPT_ARG_NONE, &do_stop, 'P',
	    " stop  camera(s) ", NULL } , 
	  { "oneshot", 'O',  POPT_ARG_NONE, &do_oneshot, 'B',
	    " take one picture. ",NULL } ,   
	  { "disp", 'D',  POPT_ARG_NONE, &do_disp, 'D',
	    " display live image on X", NULL } ,
	  { NULL, 0, 0, NULL, 0 }
     };

     struct poptOption query_optionsTable[] = {
	  { "list", 'L',  POPT_ARG_NONE, &do_list, 'L',
	    " list up all cameras on the bus.", NULL } ,    
	  { "info", 'I',  POPT_ARG_NONE, &do_info, 'I',
	    " show infomation camera", NULL } ,   
	  { NULL, 0, 0, NULL, 0 }
     };

     struct poptOption format_optionsTable[] = {
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
	  { NULL, 0, 0, NULL, 0 }
     };

     struct poptOption save_optionsTable[] = {
	  { "save",  'X',    POPT_ARG_NONE, &do_save_bin, 'B',
	    "save stream to a binary file. ",NULL } ,   
	  { "save_ppm", 'Y', POPT_ARG_NONE, &do_save_ppm, 'P',
	    "save each image", NULL},
	  { NULL, 0, 0, NULL, 0 }
     };

     struct poptOption quality_optionsTable[] = {
	  { "brightness", '\0', POPT_ARG_STRING, &cp[BRIGHTNESS], 0,
	    "brightness", "{BRIGHTNESS|off|auto|one_push}" } ,
	  { "auto_exposure", '\0', POPT_ARG_STRING, &cp[AUTO_EXPOSURE], 0,
	    "auto exposure", "{AUTO_EXPOSURE|off|auto|one_push}" } ,
	  { "sharpness", '\0', POPT_ARG_STRING, &cp[SHARPNESS], 0,
	    "sharpness", "{SHARPNESS|off|auto|one_push}" } ,
	  { "white_balance", '\0', POPT_ARG_STRING, &cp[WHITE_BALANCE], 0,
	    "white balance", "{WHITE_BALANCE|off|auto|one_push}" } ,
	  { "hue", '\0', POPT_ARG_STRING, &cp[HUE], 0,
	    "hue", "{HUE|off|auto|one_push}" } ,
	  { "saturation", '\0', POPT_ARG_STRING, &cp[SATURATION], 0,
	    "saturation", "{SATURATION|off|auto|one_push}" } ,
	  { "gamma", '\0', POPT_ARG_STRING, &cp[GAMMA], 0,
	    "gamma", "{GAMMA|off|auto|one_push}" } ,
	  { "shutter", '\0', POPT_ARG_STRING, &cp[SHUTTER], 0,
	    "shutter", "{SHUTTER|off|auto|one_push}" } ,
	  { "gain", '\0', POPT_ARG_STRING, &cp[GAIN], 0,
	    "gain", "{GAIN|off|auto|one_push}" } ,
	  { "iris", '\0', POPT_ARG_STRING, &cp[IRIS], 0,
	    "iris", "{IRIS|off|auto|one_push}" } ,
	  { "focus", '\0', POPT_ARG_STRING, &cp[FOCUS], 0,
	    "focus", "{FOCUS|off|auto|one_push}" } ,
	  { "temperature", '\0', POPT_ARG_STRING, &cp[TEMPERATURE], 0,
	    "temperature", "{TEMPERATURE|off|auto|one_push}" } ,
	  { "trigger", '\0', POPT_ARG_STRING, &cp[TRIGGER], 0,
	    "trigger", "{TRIGGER|off|auto|one_push}" } ,

	  { "zoom", '\0', POPT_ARG_STRING, &cp[ZOOM], 0,
	    "zoom", "{ZOOM|off|auto|one_push}" } ,
	  { "pan", '\0', POPT_ARG_STRING, &cp[PAN], 0,
	    "pan",  "{PAN|off|auto|one_push}" } ,
	  { "tilt", '\0', POPT_ARG_STRING, &cp[TILT], 0,
	    "tilt", "{TILT|off|auto|one_push}" } ,

	  { NULL, 0, 0, NULL, 0 }
     };

     struct poptOption optionsTable[] = {

	  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, control_optionsTable, 0,
	    "Control options",NULL},
    
//    { "dev",      'd',  POPT_ARG_INT, &card_no, 'd',
//      "device no", "DEVICE" } ,

	  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, query_optionsTable, 0,
	    "Query options",NULL},

	  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, format_optionsTable, 0,
	    "Image format options",NULL},

	  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, save_optionsTable, 0,
	    "Save options",NULL},

	  { NULL, '\0', POPT_ARG_INCLUDE_TABLE, quality_optionsTable, 0,
	    "Image quality options",NULL},

    
	  POPT_AUTOHELP
	  { NULL, 0, 0, NULL, 0 }
     };

     memset(cp,0,sizeof(cp));

     optCon = poptGetContext(NULL, argc,(const char**)argv, 
			     optionsTable, 0);

     poptSetOtherOptionHelp(optCon, "[{CAMERA_ID|all}]");
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
	  TargetList = CameraList;
     } else if (!strcasecmp("all",target_cameras)){
	  is_all = true;
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

     // set camera channel
     if (channel!=-1){
	  if (is_all){
	       MSG("The same channel can not set to two or more cameras.");
	  }
    
	  for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++)
	       cam->SetIsoChannel(channel);
     }

     // set camera format
     if ((cp_format != Format_X)
	 ||(cp_mode   != Mode_X)
	 ||(cp_rate   != FrameRate_X)){
	  for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++)
	       cam->SetFormat(cp_format,cp_mode,cp_rate);    
     }
      
     // stop camere(s)
     if (do_stop!=-1){
	  for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	       LOG("stop");
	       cam->StopIsoTx();
	  }
     }

     if (do_disp!=-1 || do_save_bin!=-1)
	  CreateYUVtoRGBAMap();

     // disp infomation camere(s)
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

     // disp
     if (do_disp!=-1){
	  for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	       display_live_image_on_X(*cam);
	  }
     }

     // save camere(s) image
     if (do_save_bin!=-1){
	  cam=TargetList.begin();
	  char fname[1024];
	  snprintf(fname,sizeof(fname),
		   "%d_%%0d.yuv",MAKE_CAMERA_ID(cam->m_ChipID));
	  LOG("save to " << fname);
    
	  savetofile(*cam, fname);
     }

     return 0;
}

/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
