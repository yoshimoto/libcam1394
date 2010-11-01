/*!
  @file  cam1394.cc
  @brief cam1394 main 
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: cam1394.cc,v 1.37 2008-12-15 12:00:55 yosimoto Exp $
  @date    $Date: 2008-12-15 12:00:55 $
 */
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
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

#include <iostream>
#include <iomanip>

#if defined HAVE_CV_H || defined HAVE_OPENCV
#include <cv.h>
#include <highgui.h>
#define IPL_IMG_SUPPORTED
#elif defined HAVE_OPENCV_CV_H
#define HAVE_CV_H
#include <opencv/cv.h>
#include <opencv/highgui.h>
#define IPL_IMG_SUPPORTED
#endif

#include <sys/time.h>
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>                /* 色変換 */


#include "common.h"
#include "xview.h"              /* Xの表示オブジェクト*/

using namespace std;

#define MAKE_CAMERA_ID(x, off) ((x)-(off))
#define MAKE_CHIP_ID(x, off)   ((x)+(off))
uint64_t magic_number = 0ULL;

// const 
const int NUM_PORT=16;  // number of 1394 interfaces 

int opt_debug_level = 1;

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

  cout << " camera id: " << MAKE_CAMERA_ID(camera.GetID(), magic_number);
  cout << " ch: "  << channel <<" ";

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

//  cout << fmt <<endl;
//  cout << mode <<endl;
//  cout << rate <<endl;


  return 0;
}

void
set_camera_feature(C1394CameraNode *cam, const char *cp[])
{
    C1394CAMERA_FEATURE feat;
    for (feat=BRIGHTNESS;
	 feat<END_OF_FEATURE;
	 feat=(C1394CAMERA_FEATURE)((int)feat+1)){
	    
	//    LOG(cam->GetFeatureName(feat)<<" "<<cp[feat]);
	if (!cp[feat])
	    continue;
	    
	if (!strcasecmp("off",cp[feat])){
	    LOG("off "<<cam->GetFeatureName(feat)<<" feat ");
	    if (!cam->DisableFeature(feat))
		ERR(cam->GetFeatureName(feat)
		    <<" has no capability of switching to OFF");
	} else if (!strcasecmp("on",cp[feat])){
	    if (!cam->EnableFeature(feat))
		ERR(cam->GetFeatureName(feat)
		    <<" has no capability of switching to ON");
	} else if (!strcasecmp("manual",cp[feat])){
	    if (!cam->SetFeatureState(feat, MANUAL))
		ERR(cam->GetFeatureName(feat)
		    <<" has no capability of manual mode");
	} else if (!strcasecmp("auto",cp[feat])) {
	    if (!cam->SetFeatureState(feat, AUTO))
		ERR(cam->GetFeatureName(feat)
		    <<" has no capability of auto mode");
	} else if (!strcasecmp("one_push",cp[feat])) {
	    if (!cam->SetFeatureState(feat, ONE_PUSH))
		ERR(cam->GetFeatureName(feat)
		    <<" has no capability of one_push mode");
	} else {
	    int val;
	    char *end=NULL;
	    val=strtol(cp[feat],&end,0);
	    if (*end=='\0'){
		// set camera value (non-abs control)
		LOG("set "<<cam->GetFeatureName(feat)<<" feat "<<val);
		if (!cam->SetParameter(feat, val)){
		    ERR("specified parameter "<<cp[feat]
			<<" is out of "<< cam->GetFeatureName(feat) 
			<< " range, skip..");
		}
	    } else {
		// try to abs control
		if (!cam->HasAbsControl(feat)){
		    ERR("the abs control capability for "
			<< cam->GetFeatureName(feat) << " is not available.");
		    exit (-1);
		}
		double abs_val;
		abs_val = strtod(cp[feat], &end);
		if (*end != '\0'){
		    // verify unit string.
		    if (strncasecmp(end, cam->GetAbsParameterUnit(feat),10)){
			ERR(cam->GetFeatureName(feat) 
			    << ": invalid unit string ["
			    << end <<"] is specified.");
			exit(-1);
		    }
		}
		
		LOG("set "<<cam->GetFeatureName(feat)
		    <<" abs feat "<<val);
		if (!cam->SetAbsParameter(feat,  abs_val)){
		    ERR("invalied abs value is specified.");
		}
	    }
	}
    } // for (feat=BRIGHTNESS;
}

void
show_camera_feature(C1394CameraNode *cam)
{
    C1394CAMERA_FEATURE feat;
	    
    printf("#     feature      value     abs value    supported-state \n");

    for (feat=BRIGHTNESS;feat<END_OF_FEATURE;
	 feat=(C1394CAMERA_FEATURE)((int)feat+1)){

	if (!cam->HasFeature(feat)){
	    continue;
	}


	const char *fname = cam->GetFeatureName(feat) ;
	printf("%15s  ", fname);

	unsigned int value;
	if (cam->GetParameter(feat, &value)){
	    printf("0x%08X  ", value);
	} else {
	    printf("%12s","-");
	} 

	float abs_value;
	if (cam->HasAbsControl(feat) && cam->GetAbsParameter(feat, &abs_value)){
	    printf("% 7.3f%-5s", abs_value,
		   cam->GetAbsParameterUnit(feat));
	}else{
	    printf("     -      ");
	}

	C1394CAMERA_FSTATE st,cur_st;
	cam->GetFeatureState(feat, &cur_st);
	for (st=OFF; st<END_OF_FSTATE;st=(C1394CAMERA_FSTATE)(st+1)){
	    if (cam->HasCapability(feat,st)){
		printf("%s%s ", (st==cur_st)?"*":"",
		       cam->GetFeatureStateName(st));
	    }
	}
/*	if (cam->HasAbsControl(feat)){
	    printf("abs ");
	}
*/
	printf("\n");
    }
}


int
savetofile(C1394CameraNode& camera,char *fname)
{
    int channel;

    camera.QueryIsoChannel(&channel);
    /* force release !! */

    /* allocate receive buffer */
    if (camera.AllocateFrameBuffer()){
	cerr << "Failed AllocateFrameBuffer() "<<strerror(errno) << endl;
	return -2;
    }

    //const int w =camera.GetImageWidth();
    //const int h =camera.GetImageHeight();

    const int frame_size= camera.GetFrameBufferSize();
    LOG("frame_size: "<<frame_size);

    int last_frame;
    camera.UpdateFrameBuffer();
    camera.GetFrameCount(&last_frame);
    int total_drop=0;
  
    int f_count=0;
    while (true)
    {
	char str[1024];
	snprintf(str, sizeof(str), fname, f_count);
	cout << str << endl;
       
	int flag=0;
	flag |= O_CREAT|O_WRONLY|O_EXCL;
//        flag |= O_SYNC;
//        flag |= O_LARGEFILE;
         
	int fd=open(str, flag ,S_IRUSR|S_IWUSR);
	if ( 0 > fd ){
	    ERR( "can't open file (" << str << ") :" << strerror(errno));
	    return -2;
	}
         
	const int NUM_SEG=1500;

        
	quadlet_t value = 0;
	camera.ReadReg(camera.m_command_regs_base + 0x830, &value);
	LOG(hex << value );
	camera.WriteReg(camera.m_command_regs_base + 0x830, &value);


	int i=0;
	uint64_t v_prev=0;
	for (i=0; 1 ; i++){
              
	    char* p=(char*)camera.UpdateFrameBuffer();

	    int frame_no;
	    camera.GetFrameCount(&frame_no);
	    if ( last_frame+1 != frame_no ){
		total_drop++;
		ERR(" drop a frame"
		    <<"(rate "
		    << (float)total_drop/(f_count*NUM_SEG + i+1)
		    <<"%)");
	    }
	    last_frame = frame_no;
	    //cout << last_frame << endl;


	    uint64_t v = ntohl(*((u_int32_t *)(p+4)));
	    v = ((v >> 25) & 0x7f) * 1000000 + ((v >> 12) & 0x1fff) * 125 + (int)(((v) & 0xfff) * (1.0/24.576));
	    fprintf(stderr, "%lld = %lld - %lld\n", (128000000 + v - v_prev) % 128000000, v, v_prev);
	    v_prev = v;
	    
	    int result = write( fd , p , frame_size );
/*	    int result = write( fd , p+4, 4 );
	    if ( result != 4 ){
		ERR(" write() failed : " << strerror(errno) );
	    }
*/
	}
	close(fd);
	f_count++;
    }

    /* release buffer */
    /* release  channel */
    camera.QueryIsoChannel(&channel);
    return 0;
}

#ifdef IPL_IMG_SUPPORTED

static int 
get_bayer_code(const char *string)
{
    if (NULL==string)
	return -1;
    if ('\0'==*string)
	return -1;

    static const char *tbl[]={
	"BG","GB","RG","GR",NULL,
    };
    int code = 46;
    const char **cur = tbl;
    while (NULL!=*cur){
	if (0==strcasecmp(string, *cur)){
	    return code;
	}
	code++;
	cur++;
    }
    return -1;
}

#endif /* #ifdef IPL_IMG_SUPPORTED */

int 
display_live_image_on_X(C1394CameraNode &cam, const char *fmt, 
			double scale, int draw_fps)
{
  int channel;

  cam.QueryIsoChannel(&channel);

  /* force release !! FIXME! */

  /* allocate receive buffer */
  if (cam.AllocateFrameBuffer()){
    LOG("err: failure @ AllocateFrameBuffer() ");
    return -2;
  }

  const int w = cam.GetImageWidth();
  const int h = cam.GetImageHeight();

#ifndef IPL_IMG_SUPPORTED
  /* make a Window */
  char tmp[256];
  snprintf(tmp,sizeof(tmp),"-- Live image from #%llu/ %2dch --",
	   MAKE_CAMERA_ID(cam.GetID(), magic_number), channel );

  CXview xview;
  if (!xview.CreateWindow(w,h,tmp)){
    LOG(" failure @ create X window");
    return -2;
  }
  
  while (1){
    RGBA tmp[w*h];
    cam.UpdateFrameBuffer();
    cam.CopyRGBAImage(tmp);
    xview.UpDate(tmp);
  }
#else  /* #ifndef IPL_IMG_SUPPORTED */

  CvFont font;
  if (draw_fps) {
      font = cvFont(1);
  }

  int argc = 1;
  char tmp[]="dummy";
  char *argv[]={tmp};
  cvInitSystem(argc, argv);
  cvNamedWindow("disp", !0);
  int ch=3;
  IplImage *img = NULL;
  IplImage *buf = NULL;
  IplImage *resize = NULL;

  int buf_option=0;

  if (fmt){
      int code = get_bayer_code(fmt);
      if (code>0){
	  buf = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, 3);     
	  buf_option = code;
      }
  }

  if (!buf){
      ch = 3;
  } else {      
      // bayer conversion.
      ch = 1;
  }
  img = cvCreateImage(cvSize(w,h), IPL_DEPTH_8U, ch);
  if (scale != 1.) {
      int channel = (buf)?buf->nChannels:ch;
      resize = cvCreateImage(cvSize(cvRound(w*scale), cvRound(h*scale)),
			     IPL_DEPTH_8U, channel);
  }

  unsigned int lastcycle = 0;
  timeval last;
  gettimeofday(&last, NULL);
  while ('q' != (((unsigned int)cvWaitKey(3))&0xff) ){

      BufferInfo info;
      memset(&info, 0, sizeof(info));
      void *p = cam.UpdateFrameBuffer(C1394CameraNode::BUFFER_DEFAULT, &info);
      if (!p) {
	  ABORT("UpdateFrameBuffer() failed.");
      }

      switch (ch){
      case 3:
	  cam.CopyIplImage(img);
	  break;
      case 1:
	  cam.CopyIplImageGray(img);
	  break;
      }
      if (buf){
	  cvCvtColor(img, buf, buf_option);
      }
      if (resize) {
	  cvResize(buf?buf:img, resize);
      }

      int diff = (info.timestamp - lastcycle)&0xffff;
      DBG("diff: "<< setw(5) << diff << " cycle " 
	  << setw(5) << setprecision(3) << diff*0.125 << " msec "
	  << setw(5) << setprecision(3) << 1000./(diff*0.125) << " fps");
      lastcycle = info.timestamp;

      IplImage *tmp = resize?resize:(buf?buf:img);
      if (draw_fps) {
	  timeval current;
	  gettimeofday(&current, NULL);

	  double diff = (current.tv_sec - last.tv_sec) + 
	      (current.tv_usec - last.tv_usec)/(1000.*1000.);

	  last = current;

	  char str[1024];
	  snprintf(str, sizeof(str), "% 3.1ffps", 1./diff);
	  cvPutText(tmp, str, cvPoint(tmp->width-100, tmp->height-10), 
		    &font, CV_RGB(255,255,255));
      }
      cvShowImage("disp", tmp);
      
  }
  cvReleaseImage(&img);
  cvReleaseImage(&buf);
  cvReleaseImage(&resize);
#endif   /* #ifndef IPL_IMG_SUPPORTED */
  return 0;
}


struct reg_and_value {
    quadlet_t reg;
    quadlet_t val;
};

const int MAX_REG_QUEUE = 128;
static reg_and_value reg_read_queue[MAX_REG_QUEUE];
static int num_reg_read = 0;
static reg_and_value reg_write_queue[MAX_REG_QUEUE];
static int num_reg_write = 0;

static int
parse_reg_and_value(const char *str, quadlet_t *reg, quadlet_t *value)
{
    char *endp = NULL;
    quadlet_t tmp;

    tmp = strtoll(str, &endp, 0);
    if (str==endp) {
	return -1;
    }

    if (reg) {
	*reg = tmp;
    }

    if (value) {
	str = endp + 1;
	tmp = strtoll(str, &endp, 0);
	if (str==endp) {
	    return -1;
	}
	*value = tmp;
    }
    return 0;
}

int main(int argc, char *argv[]){

    poptContext optCon;  /* context for parsing command-line options */
    int c;              /* for parse options */
    int channel=-1;      /* iso channel */
    //int card_no=0;     /* oh1394 interface number */
    int spd=-1;          /* bus speed 0=100M 1=200M 2=400M */

    const char *opt_filename=NULL;   /* filename to save frame(s). */
    const char *target_cameras=NULL; /* target camera(s) */
    const char *opt_power = NULL;    /**< power "on" or "off"  */

    const char *cp[END_OF_FEATURE]; /* camera's parameter. 
				       "NULL"  means the value isn't set. */

    enum {
	CMD_SET_REG  = 0x1000,
	CMD_SHOW_REG,
    };

    memset(cp, 0, sizeof(cp));

    const char *opt_magic_string = NULL ;  /* magic number for camera id. */
    FORMAT    cp_format = Format_X;
    VMODE     cp_mode   = Mode_X;
    FRAMERATE cp_rate   = FrameRate_X;
  
//  int r_value=-1;   /* white balance param */
//  int b_value=-1;
  
    int  do_info    =-1;
    int  do_disp    =-1;
    int  do_save_bin=-1;
    int  do_save_ppm=-1;
    int  do_oneshot =-1;
    int  do_start   =-1;
    int  do_stop    =-1;
    int  do_query   =-1;
    int  do_show_version =-1;
    double display_scale = 1.;
    int  draw_fps = 0;
    int  do_query_vender =-1;
    int  do_query_model  =-1;

    int  trigger_mode = -1;

    const char *opt_bayer_string=NULL;

    bool is_all=false; // if target cameras are all camera, then set true

    struct poptOption control_optionsTable[] = {
	{ "start", 'S',  POPT_ARG_NONE, &do_start, 'S',
	  "start camera(s) ", NULL } ,    
	{ "stop",  'P',  POPT_ARG_NONE, &do_stop, 'P',
	  "stop  camera(s) ", NULL } , 
	{ "disp", 'D',  POPT_ARG_NONE, &do_disp, 'D',
	  "display live image on X", NULL } ,
#ifdef IPL_IMG_SUPPORTED
	{ "scale", 0 , POPT_ARG_DOUBLE, &display_scale, 0,
	  "scale the live image", "SCALE"}, 
	{ "drawfps", 0, POPT_ARG_NONE, &draw_fps, 0,
	  "draw frame rate on the live image",},
#endif
	{ "power", 0, POPT_ARG_STRING, &opt_power, 0,
	  "power on/off", "{on|off}"},
	{ NULL, 0, 0, NULL, 0 }
    };

    struct poptOption query_optionsTable[] = {
	{ "info", 'I',  POPT_ARG_NONE, &do_info, 'I',
	  "show infomation camera", NULL } ,   
	{ "query", 'Q', POPT_ARG_NONE, &do_query, 'Q',
	  "query setting (EXPERIMENTAL)", NULL},
	{ "vender", 0, POPT_ARG_NONE, &do_query_vender, 0,
	  "show vender name", NULL},
	{ "model", 0, POPT_ARG_NONE, &do_query_model, 0,
	  "show model name", NULL},
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
	{ "oneshot", 'O',  POPT_ARG_NONE, &do_oneshot, 'O',
	  "take one picture. ",NULL } ,   
	{ "save",  'X',    POPT_ARG_NONE, &do_save_bin, 'X',
	  "save stream to a binary file. ",NULL } ,   
	{ "save_ppm", 'Y', POPT_ARG_NONE, &do_save_ppm, 'Y',
	  "save each frame to files.", NULL},
	{ "filename", 'f', POPT_ARG_STRING, &opt_filename, 'f',
	  "filename or filename pattern.(EXPERIMENTAL)", NULL},
	{ NULL, 0, 0, NULL, 0 }
    };

    struct poptOption quality_optionsTable[] = {
	{ "brightness", '\0', POPT_ARG_STRING, &cp[BRIGHTNESS], 0,
	  "brightness", "{BRIGHTNESS|on|off|manual|auto|one_push}" } ,
	{ "auto_exposure", '\0', POPT_ARG_STRING, &cp[AUTO_EXPOSURE], 0,
	  "auto exposure", "{AUTO_EXPOSURE|on|off|manual|auto|one_push}" } ,
	{ "sharpness", '\0', POPT_ARG_STRING, &cp[SHARPNESS], 0,
	  "sharpness", "{SHARPNESS|on|off|manual|auto|one_push}" } ,
	{ "white_balance", '\0', POPT_ARG_STRING, &cp[WHITE_BALANCE], 0,
	  "white balance", "{WHITE_BALANCE|on|off|manual|auto|one_push}" } ,
	{ "hue", '\0', POPT_ARG_STRING, &cp[HUE], 0,
	  "hue", "{HUE|on|off|manual|auto|one_push}" } ,
	{ "saturation", '\0', POPT_ARG_STRING, &cp[SATURATION], 0,
	  "saturation", "{SATURATION|on|off|manual|auto|one_push}" } ,
	{ "gamma", '\0', POPT_ARG_STRING, &cp[GAMMA], 0,
	  "gamma", "{GAMMA|on|off|manual|auto|one_push}" } ,
	{ "shutter", '\0', POPT_ARG_STRING, &cp[SHUTTER], 0,
	  "shutter", "{SHUTTER|on|off|manual|auto|one_push}" } ,
	{ "gain", '\0', POPT_ARG_STRING, &cp[GAIN], 0,
	  "gain", "{GAIN|on|off|manual|auto|one_push}" } ,
	{ "iris", '\0', POPT_ARG_STRING, &cp[IRIS], 0,
	  "iris", "{IRIS|on|off|manual|auto|one_push}" } ,
	{ "focus", '\0', POPT_ARG_STRING, &cp[FOCUS], 0,
	  "focus", "{FOCUS|on|off|manual|auto|one_push}" } ,
	{ "temperature", '\0', POPT_ARG_STRING, &cp[TEMPERATURE], 0,
	  "temperature", "{TEMPERATURE|on|off|manual|auto|one_push}" } ,
	{ "trigger", '\0', POPT_ARG_STRING, &cp[TRIGGER], 0,
	  "trigger", "{on|off}" } ,
	{ "trigger_delay", '\0', POPT_ARG_STRING, &cp[TRIGGER_DELAY], 0,
	  "trigger delay", "{TRIGGER_DELAY|on|off|manual|auto|one_push}" } ,
	{ "white_shd", '\0', POPT_ARG_STRING, &cp[WHITE_SHD], 0,
	  "white shading", "{WHITE_SHD|on|off|manual|auto|one_push}" } ,
	{ "frame_rate", '\0', POPT_ARG_STRING, &cp[FRAME_RATE], 0,
	  "frame rate", "{FRAME_RATE|on|off|manual|auto|one_push}" } ,

	{ "zoom", '\0', POPT_ARG_STRING, &cp[ZOOM], 0,
	  "zoom", "{ZOOM|on|off|manual|auto|one_push}" } ,
	{ "pan", '\0', POPT_ARG_STRING, &cp[PAN], 0,
	  "pan",  "{PAN|on|off|manual|auto|one_push}" } ,
	{ "tilt", '\0', POPT_ARG_STRING, &cp[TILT], 0,
	  "tilt", "{TILT|on|off|manual|auto|one_push}" } ,
	{ "optical_filter", '\0', POPT_ARG_STRING, &cp[OPTICAL_FILTER], 0,
	  "optical filter", "{OPTICAL_FILTER|on|off|manual|auto|one_push}" } ,

	{ "capture_size", '\0', POPT_ARG_STRING, &cp[CAPTURE_SIZE], 0,
	  "capture size", "{CAPTURE_SIZE|on|off|manual|auto|one_push}" } ,
	{ "capture_quality", '\0', POPT_ARG_STRING, &cp[CAPTURE_QUALITY], 0,
	  "capture quality", "{CAPTURE_QUALITY|on|off|manual|auto|one_push}" } ,
	{ "trigger_mode", '\0', POPT_ARG_INT, &trigger_mode, 0,
	  "trigger mode (0..15)"},

	{ NULL, 0, 0, NULL, 0 }
    };
    
    struct poptOption common_optionsTable[] = {
	{ "magic", 0,  POPT_ARG_STRING, &opt_magic_string, 0,
	  "Set magic number for camera id", "MAGIC" } ,   
#ifdef IPL_IMG_SUPPORTED
	{ "bayer", 0,  POPT_ARG_STRING, &opt_bayer_string, 0,
	  "bayer code.", "{BG,GB,RG,GR}"},
#endif
	{ "verbose", 'v', POPT_ARG_NONE, NULL, 'v',
	  "Provide more detailed output.",0},
	{ "version", 'V', POPT_ARG_NONE, &do_show_version, 'V',
	  "Print the version string of libcam1394 library and exit.",0},
	{ NULL, 0, 0, NULL, 0 }
    };

    struct poptOption expert_optionsTable[] = {
	{ "setreg",  0, POPT_ARG_STRING, NULL,  CMD_SET_REG,
	  "write register", "OFFSET,VALUE"},
	{ "showreg", 0, POPT_ARG_STRING, NULL, CMD_SHOW_REG,
	  "print register value", "OFFSET"},
	{ NULL, 0, 0, NULL, 0 }
    };


    struct poptOption optionsTable[] = {
	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, control_optionsTable, 0,
	  "Control options:",NULL},
    
	// { "dev",      'd',  POPT_ARG_INT, &card_no, 'd',
	//      "device no", "DEVICE" } ,

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, query_optionsTable, 0,
	  "Query options:",NULL},

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, format_optionsTable, 0,
	  "Image format options:",NULL},

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, save_optionsTable, 0,
	  "Save options:",NULL},

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, quality_optionsTable, 0,
	  "Image quality options:",NULL},

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, expert_optionsTable, 0,
	  "Direct access options:",NULL},   

	{ NULL, '\0', POPT_ARG_INCLUDE_TABLE, common_optionsTable, 0,
	  "Common options options:",NULL},   
	
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
	const char *str;
	quadlet_t reg, val;
	int result ;
	switch (c){
	case 'v':
	    opt_debug_level ++;
	    break;
	case CMD_SHOW_REG:	   
	    if (num_reg_read>MAX_REG_QUEUE){
		ERR("too many --showreg option");
	    }
	    str = poptGetOptArg(optCon);
	    result = parse_reg_and_value(str,
					 &reg_read_queue[num_reg_read].reg, 
					 NULL);
	    if (result) {
		ERR("bad address "<< str);
	    }
	    ++num_reg_read;
	    break;
	case CMD_SET_REG:
	    if (num_reg_write>MAX_REG_QUEUE){
		ERR("too many --setreg option");
	    }
	    str = poptGetOptArg(optCon);
	    result = parse_reg_and_value(str,
					 &reg_write_queue[num_reg_write].reg, 
					 &reg_write_queue[num_reg_write].val);
	    if (result) {
		ERR("bad address or value "<< str);
	    }
	    ++num_reg_write;
	    break;
	}
    }
  
    if (c < -1) {
	/* an error occurred during option processing */
	cerr << poptBadOption(optCon, POPT_BADOPTION_NOALIAS)
	     <<" : "<< poptStrerror(c) <<endl;
	exit(1);
    }

    target_cameras = poptGetArg(optCon);
    poptFreeContext(optCon);
    
    libcam1394_set_debug_level( opt_debug_level );

    if ( do_show_version != -1 ) {
	printf("libcam1394-%s\n",libcam1394_get_version());
    }

  
    if ( opt_magic_string ){
	char *end=NULL;
	magic_number = strtoll( opt_magic_string, &end, 0 );
	if (*end!='\0'){
	    // failure parse the opt_magic_string
	    magic_number = 0;
	}
    }
    // *** initialize 1394 port

    // get handle 
    raw1394handle_t handle;
    handle = raw1394_new_handle();
    if (!handle) {
	if (!errno) {
	    ERR("not_compatible");
	} else {
	    ERR("Failed to open raw1394 device "
		" "<< strerror(errno));
	    ERR("The raw1394 module must be loaded, "
		"and you must have read and write permission "
		"to /dev/raw1394.");
	}
	return -3;
    }

    // *** list up  all cameras on the bus 

    CCameraList CameraList;
    if (! GetCameraList(handle,&CameraList) ){
	ERR(" there's no camera.");
	return -1;
    }

    //  *** select an camera if camera_id is specified.
    CCameraList TargetList;
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
	uint64_t camera_id=strtoll(target_cameras, &end, 0);
	if (*end!='\0'){
	    ERR(" please set CAMERA_ID or all " << target_cameras );
	    exit(-1);      
	}
	CCameraList::iterator cam;
	cam=find_camera_by_id(CameraList,MAKE_CHIP_ID(camera_id,magic_number));
	if (cam==CameraList.end()){
	    ERR(" not found camera which id is "<<camera_id);
	    return -2;
	}
	TargetList.push_back( *cam );
    }

    CCameraList::iterator cam;


    if (opt_power){
	int mode = 0;
	if (0==strcasecmp(opt_power, "on"))
	    mode = 1;
	else if (0==strcasecmp(opt_power, "off"))
	    mode = 2;

	if (mode){
	    for (cam=TargetList.begin(); cam!=TargetList.end(); cam++){
		switch(mode){
		case 1:
		    if (!cam->PowerUp()){
			ERR("failed to PowerUp()");
		    }
		    break;
		case 2:
		    if (!cam->PowerDown()){
			ERR("failed to PowerDown()");
		    }
		    break;
		}
	    }
	}
    }

    if (num_reg_read>0) {
	cout << "    cam id           reg      value" <<endl;
	cout << setfill('0');
	for (int i=0; i<num_reg_read; ++i) {
	    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
		quadlet_t val;
		bool r;
		r = cam->ReadReg(cam->m_command_regs_base
				 + reg_read_queue[i].reg,
				 &val);
		cout << MAKE_CAMERA_ID(cam->GetID(), magic_number)
		     << "  " 
		     << hex << setw(8) << reg_read_queue[i].reg << dec;
		if (r)
		    cout << " " << hex << setw(8) << val << dec << endl;
		else
		    cout << "  N/A " << endl;
	    }
	}
    }
    if (num_reg_write>0) {
	for (int i=0; i<num_reg_write; ++i) {
	    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
		bool r;
		r = cam->WriteReg(cam->m_command_regs_base
				  + reg_write_queue[i].reg,
				  &reg_write_queue[i].val);
		if (!r) {
		    cout << " setreg " << hex << reg_write_queue[i].reg 
			 << " " << reg_write_queue[i].val 
			 << " to " 
			 << MAKE_CAMERA_ID(cam->GetID(), magic_number)
			 << " failed ";
                }
	    }
	}
    }

    // set camera register for each feature.
    for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	set_camera_feature( &(*cam), cp);
	if (-1 != trigger_mode) {
	    (*cam).SetTriggerMode( trigger_mode );
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
	||(cp_mode != Mode_X)
	||(cp_rate != FrameRate_X)){
	for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    bool result = cam->SetFormat(cp_format,cp_mode,cp_rate);    
	    if (!result) {
		FORMAT f;
		VMODE m;
		FRAMERATE r;
		cam->QueryFormat(&f, &m, &r);
		MSG("your camera doesn't support " 
		    << " Format_"<< ((cp_format==Format_X)?f:cp_format) 
		    << ",Mode_"<< ((cp_mode==Mode_X)?m:cp_mode)
		    << ",FrameRate_"<< ((cp_rate==FrameRate_X)?r:cp_rate));
	    }
        }
    }
    // set iso speed
    if (spd!=-1) {
	for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    FORMAT f; VMODE m; FRAMERATE r;
	    SPD req_speed;
	    cam->QueryFormat(&f,&m,&r);
	    req_speed=GetRequiredSpeed(f,m,r);
	    if (spd >= req_speed ){
		cam->SetIsoSpeed((SPD)spd);
	    }else{
		ERR("camera "<< MAKE_CAMERA_ID(cam->GetID(), magic_number) 
		    << " requires " << GetSpeedString(req_speed) << ".");
	    }
	}
    }
      
    // stop camere(s)
    if (do_stop!=-1){
	for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    LOG("stop");
	    cam->StopIsoTx();
	}
    }

    if (do_disp!=-1 || do_save_bin!=-1 || do_oneshot!=-1 )
	CreateYUVtoRGBAMap();

    // disp infomation camere(s)

    if ( do_query        != -1 ||
	 do_info         != -1 ||
	 do_query_model  != -1 || 
	 do_query_vender != -1 ) {
	for (cam=TargetList.begin(); cam!=TargetList.end(); cam++) {
	    char buf[1024];
	    if (do_query_vender != -1) {
		char *name = (*cam).GetVenderName(buf, sizeof(buf));
		cout << name << endl;
	    }
	    if ( do_query_model != -1 ) {
		char *name = (*cam).GetModelName(buf, sizeof(buf));
		cout << name << endl;
	    }
	    if (do_info!=-1){
		display_current_status(*cam);
	    }
	    if ( do_query != -1 ) {
		show_camera_feature( &(*cam) );
	    }
	}
    }

    // start camere(s)
    if (do_start!=-1 || do_oneshot!=-1 ){
	for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    LOG("start");
	    cam->StartIsoTx();
	}
    }

    // disp
    if (do_disp!=-1){
	for ( cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    display_live_image_on_X(*cam, opt_bayer_string, 
				    display_scale, draw_fps);
	}
    }

    // save camere(s) image
    if (do_save_bin!=-1){	
	if ( TargetList.size() != 1){
	    cerr << "please specify camera id." << endl;
	    exit(-1);
	}

	cam=TargetList.begin();
	char fname[1024];
	if (NULL==opt_filename)
	    snprintf(fname,sizeof(fname),"%llu_%%02d.raw",
		     MAKE_CAMERA_ID(cam->GetID(),magic_number));
	else
	    snprintf(fname,sizeof(fname),opt_filename, 
		     MAKE_CAMERA_ID(cam->GetID(),magic_number));
	savetofile(*cam, fname);
    }

    // save a frame to file.
    if (do_oneshot!=-1){
	for (cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    if (cam->AllocateFrameBuffer()){
		cerr << "Failed to AllocateFrameBuffer() "<<strerror(errno)<<endl;
		return -2;
	    }
	}

	for (cam=TargetList.begin(); cam!=TargetList.end(); cam++){
	    char fname[1024];
	    
	    if (NULL==opt_filename)
		snprintf(fname,sizeof(fname),"%llu.ppm",
			 MAKE_CAMERA_ID(cam->GetID(),magic_number));
	    else
		snprintf(fname,sizeof(fname),opt_filename, 
			 MAKE_CAMERA_ID(cam->GetID(),magic_number));
	    

	    cam->UpdateFrameBuffer();
	    cam->SaveToFile(fname);
	}

    }
    return 0;
}

/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
