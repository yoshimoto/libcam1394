//
// 1394cam.cc - 1394-based Digital Camera control class
//
// Copyright (C) 2000,2001 by Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp> 
//
// class C1394Node
//  +- class C1394CameraNode
//
// 1394-based Digital Camera Specification (Version1.20)
// 
// Sat Dec 11 07:01:01 1999 by hiromasa yoshimoto 
// Tue Feb  8 21:13:45 2000 By hiromasa yoshimoto 
// Thu Sep 27 08:21:43 2001  YOSHIMOTO Hiromasa 
// Sat Oct 20 10:15:35 2001  YOSHIMOTO Hiromasa 
// Thu Nov  1 10:20:59 2001  YOSHIMOTO Hiromasa  code clean up
 
#include <stdio.h>
#include <unistd.h>
#include <string.h> // for bzero()
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <popt.h>
#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>
using namespace std;
#include <iostream>
#include <iomanip>
#include <list>

#include <linux/ohci1394_iso.h>
#include "common.h"
#include "1394cam.h"
#include "yuv.h"


#define QUAD(h,l)  0x##h##l
#define CHK_PARAM(exp) {if (!(exp)) MSG( "illegal param passed. " << __STRING(exp)); }
#define EXCEPT_FOR_FORMAT_6_ONLY  {if (is_format6) return false;}

#define Addr(name) (m_command_regs_base+OFFSET_##name)

#define HEX(var)     hex<<setw(8)<<var<<dec
#define VAR32(var)   #var" := 0x"<<HEX(var)
#define VAR(var)     #var" :="<<var
#define CHR(var)    CHR_( ((var)&0xff) )
#define CHR_(c)     ((0x20<=(c) && (c)<='z')?(char)(c):'.')
#define EQU(val,mask,pattern)  (0==(((val)&(mask))^(pattern)))

#define ADDR_CONFIGURATION_ROM        (CSR_REGISTER_BASE + CSR_CONFIG_ROM)
// for root directory
#define ADDR_ROOT_DIR                 (ADDR_CONFIGURATION_ROM + 0x14)
#define ADDR_INDIRECT_OFFSET          (ADDR_CONFIGURATION_ROM + 0x20)
#define ADDR_UNIT_DIRECTORY_OFFSET    (ADDR_CONFIGURATION_ROM + 0x24)
// for unit directory
#define OFFSET_UNIT_DEPENDENT_DIRECTORY_OFFSET   0x000c
// for unit dependent directory
#define OFFSET_COMMAND_REGS_BASE                 0x0004       

// Thu Nov  1 10:20:42 2001  YOSHIMOTO Hiromasa 
//

static const char *feature_hi_table[]=
{
  "brightness" , 
  "auto_exposure",   
  "sharpness" ,
  "white_balance" ,
  "hue" ,  
  "saturation" ,
  "gamma" ,       
  "shutter",         
  "gain" ,         
  "iris" ,        
  "focus" ,  
  "temperature" , 
  "triger"         ,
};

struct VideoPixelInfo{
  char* name;
  union {
      int  R_weight;
      int  G_weight;
      int  B_weight;
    } rgb;
    struct YUVinfo{
      int  Y_weight;
      int  U_weight;
      int  V_weight;
    } yuv; 
} info;

struct VideoImageInfo{
  PIXEL_FORMAT pixel_format;
  int  w;
  int  h;
};
struct VideoPacketInfo{
  int packet_sz;
  int num_packets;
  SPD required_speed;
};

static struct VideoPixelInfo video_pixel_info[]=
{
  {"YUV(4:4:4)", 4,4,4   }, // VFMT_YUV444
  {"YUV(4:2:2)", 4,2,2   }, // VFMT_VUV422
  {"YUV(4:1:1)", 4,1,1   }, // VFMT_VUV411
  {"RGB(8:8:8)", 8,8,8   },  // VFMT_RGB888
  {" Y (Mono) ", 8,0,0   }, // VFMT_Y8

  {" unknown  ",-1,-1,-1 },
};

#define RESERVED { VFMT_NOT_SUPPORTED, -1,-1}
static struct VideoImageInfo video_image_info[][8]=  // format / mode
{
  // format_0
  {   //  pixel info       width hight
    {    VFMT_YUV444,       160,  120}, // format_0/mode_0
    {    VFMT_YUV422,       320,  240}, // format_0/mode_1
    {    VFMT_YUV411,       640,  480}, // format_0/mode_2
    {    VFMT_YUV422,       640,  480}, // format_0/mode_3
    {    VFMT_RGB888,       640,  480}, // format_0/mode_4
    {    VFMT_Y8    ,       640,  480}, // format_0/mode_5
    RESERVED,
    RESERVED,
  },
  // format_1
  // format_2
};
#undef RESERVED

#define RESERVED  { -1,-1,SPD_100M}
static struct VideoPacketInfo video_packet_info[][8][6]= // format / mode / fps
{
  // format_0
  {
    { // mode_0 
      RESERVED,                     // 1.875fps
      RESERVED,                     // 3.75 fps
      {  15*4, 120*8, SPD_100M  },  // 7.5  fps
      {  30*4, 120*4, SPD_100M  },  // 15   fps
      {  60*4, 120*2, SPD_100M  },  // 30   fps
      RESERVED,                     // 60   fps
    },
    { // mode_1 
      RESERVED,
      {  20*4, 240*8 ,SPD_100M},
      {  40*4, 240*4 ,SPD_100M},
      {  80*4, 240*2 ,SPD_100M},
      { 160*4, 240   ,SPD_100M},
      RESERVED,
    },
    { // mode_2
      RESERVED,
      {  60*4, 480*4 ,SPD_100M},
      { 120*4, 480*2 ,SPD_100M},
      { 240*4, 480/1 ,SPD_100M},
      { 480*4, 480/2 ,SPD_200M},
      RESERVED,
    },
    {  // mode_3
      RESERVED,
      {  80*4, 480*4 ,SPD_100M},
      { 160*4, 480*2 ,SPD_100M},
      { 320*4, 480/1 ,SPD_200M},
      { 640*4, 480/2 ,SPD_400M},
      RESERVED,
    },
    { // mode_4
      RESERVED,
      { 120*4, 480*4 ,SPD_100M},
      { 240*4, 480*2 ,SPD_100M},
      { 480*4, 480*1 ,SPD_200M},
      { 960*4, 480/2 ,SPD_400M},
      RESERVED,
    },
    { // mode_5
      RESERVED,
      {  40*4, 480*4 ,SPD_100M},
      {  80*4, 480*2 ,SPD_100M},
      { 160*4, 480/1 ,SPD_100M},
      { 320*4, 480/2 ,SPD_200M},
      { 320*4, 480/4 ,SPD_400M},
    },  
  },
  // format_1
  // format_2
};
#undef RESERVED


static char __buffer__[32+4+1];
static char* BIN32(quadlet_t value)
{
  int mask;
  char* p=__buffer__;
  for (mask=31;mask>=0;mask--){
    *p++=(value&(1<<mask))?'1':'0';
    if ((mask!=0)&&((mask&0x3)==0)){
      *p++='-';
    }
  }
  return __buffer__;
}

//
//
//
static int 
callback_1394Camera(raw1394_handle* handle,nodeid_t node_id,
		    C1394CameraNode* pNode,void* arg)
{
  nodeaddr_t addr;
  quadlet_t tmp;
  addr=ADDR_INDIRECT_OFFSET;
  WAIT;
  TRY( raw1394_read(handle, node_id,
		    addr, 4, &tmp) );
  tmp=ntohl(tmp);
  //  LOG(VAR32(indirect_offset));
  if (!EQU(tmp,0xff000000,0x8d000000)){
    return false;
  }
  tmp&=~0x8d000000;
  tmp*=4;
  addr=ADDR_INDIRECT_OFFSET+tmp+4;
//  LOG(VAR32(addr));
  quadlet_t chip_id_hi,chip_id_lo;
  WAIT;
  TRY( raw1394_read(handle, node_id,
		    addr, 4, 
		    &chip_id_hi));
  chip_id_hi=ntohl(chip_id_hi);
  WAIT;
  addr+=4;
  TRY( raw1394_read(handle, node_id,
		    addr, 4, 
		    &chip_id_lo));
  chip_id_lo=ntohl(chip_id_lo);
  pNode->m_VenderID =chip_id_hi>>8;
  pNode->m_ChipID   =(((int64_t)chip_id_hi&0xff)<<32)+(chip_id_lo);

  
  addr=ADDR_UNIT_DIRECTORY_OFFSET;   /* addr<=RootDirectory の先頭 */
  WAIT;
  /* RootDirectory.unit_directory_offset の取得 */
  TRY( raw1394_read(handle, node_id,
		    addr,sizeof(tmp),&tmp));
  tmp=ntohl(tmp);
  if (0xd1000000!=(tmp&0xff000000)){
    return false;
  }
  addr+=(tmp&~0xff000000)*4;        /* addr <= Unit directory の先頭 */
  WAIT;
  /* Unit Directory.unit_dependent_directory_offset の取得 */
  addr+=OFFSET_UNIT_DEPENDENT_DIRECTORY_OFFSET;
  TRY( raw1394_read(handle, node_id,
		    addr,sizeof(tmp),&tmp));
  tmp=ntohl(tmp);
  if (0xD4000000!=(tmp&0xff000000)){
    return false;
  }
  addr+=(tmp&~0xff000000)*4;        /* addr <= Unit directory の先頭 */
  WAIT;
  /* Unit Dependent Directory.command_regs_base の取得 */
  addr+=OFFSET_COMMAND_REGS_BASE;
  TRY( raw1394_read(handle, node_id,
		    addr,sizeof(tmp),&tmp));
  tmp=ntohl(tmp);
  if (0x40000000!=(tmp&0xff000000)){
    return false;
  }
  /* command_regs_base の取得 */
  // save infomation of this device
  pNode->m_handle=handle;
  pNode->m_node_id=node_id;
  pNode->m_command_regs_base=CSR_REGISTER_BASE+(tmp&~0xff000000)*4;
  return true;
}

CCameraList::iterator
find_camera_by_id(CCameraList& CameraList,int64_t id)
{
  CCameraList::iterator ite;
  for (ite=CameraList.begin();ite!=CameraList.end();ite++){
    if ((*ite).m_ChipID == id){
      return ite;
    }
  } 
  return CameraList.end();
}

const int NUM_PORT = 16;  /*  port   */

bool
GetCameraList(raw1394handle_t handle,CCameraList* pList)
{
  int i, numcards;
  struct raw1394_portinfo portinfo[NUM_PORT];
  
  cerr<<setfill('0');

/*
  printf("successfully got handle\n");
  printf("current generation number: %d\n",
	 raw1394_get_generation(handle));
	 */

  numcards = raw1394_get_port_info(handle, portinfo, NUM_PORT);
  if (numcards < 0) {
    perror("couldn't get card info");
    return false;
  } else {
/*
    printf("%d card(s) found\n", numcards);
    */
  }

  if (!numcards) {
    return false;
  }

/*
  for (i = 0; i < numcards; i++) {
    printf("  nodes on bus: %2d, card name: %s\n", portinfo[i].nodes,
	   portinfo[i].name);
  }
  */      
  if (raw1394_set_port(handle, 0) < 0) {
    perror("couldn't set port");
    return false;
  }


/*  printf("using first card found: %d nodes on bus, local ID is %d\n",
	 raw1394_get_nodecount(handle),
	 raw1394_get_local_id(handle) & 0x3f);

	 */
  Enum1394Node(handle,&portinfo[0],
	       pList,callback_1394Camera,NULL);

  return true;
}

// ------------------------------------------------------------

bool C1394CameraNode::ReadReg(nodeaddr_t addr,quadlet_t* value)
{
  *value=0x12345678;
  TRY(raw1394_read(m_handle, m_node_id,
		   addr, 4,
		   value));
  WAIT;    WAIT;
  *value=(quadlet_t)ntohl((unsigned long int)*value);

  return true;
}
bool C1394CameraNode::WriteReg(nodeaddr_t addr,quadlet_t* value)
{
  quadlet_t tmp=htonl(*value);
    
  TRY(raw1394_write(m_handle, m_node_id,
		    addr,4, 
		    &tmp));
  WAIT;
  return true;
}


C1394CameraNode::C1394CameraNode()
{
  m_channel=-1;
  m_iso_speed=SPD_400M;
  is_format6=0;
  m_lpModelName=NULL;
  m_lpVecderName=NULL;

#if defined(_WITH_ISO_FRAME_BUFFER_)
  m_lpFrameBuffer=NULL;
  m_BufferSize=0;
#endif //#if defined(_WITH_ISO_FRAME_BUFFER_)
//  m_last_read_frame=0;
}

C1394CameraNode::~C1394CameraNode()
{
#if defined(_WITH_ISO_FRAME_BUFFER_)
    m_BufferSize=0;
#endif //#if defined(_WITH_ISO_FRAME_BUFFER_)

}

// to re-set camera to initial(factory setting value) state
//
// Sun Dec 12 19:04:14 1999 by hiromasa yoshimoto 
//
bool
C1394CameraNode::ResetToInitialState()
{
  quadlet_t tmp;

  WriteReg(
    (CSR_REGISTER_BASE + CSR_CONFIG_ROM)+CSR_STATE_SET, 
    &tmp);
  WAIT;
  
  tmp=SetParam(INITIALIZE,Initialize,1);
  LOG("reset camera..."<<tmp);

  WriteReg(Addr(INITIALIZE),&tmp);

  WAIT;  
  return true;
}


// power up/down camera
// Sun Dec 12 19:08:52 1999 by hiromasa yoshimoto 
// DFW-V500 seems not to support these functions...
//
bool
C1394CameraNode::PowerDown()
{
  quadlet_t tmp=SetParam(Camera_Power,,0);
  TRY( raw1394_write(m_handle, m_node_id, 
		     Addr(Camera_Power),4,
		     &tmp) );  
  WAIT;
  return true;
}

bool
C1394CameraNode::PowerUp()
{
  quadlet_t tmp=SetParam(Camera_Power,,1);
  TRY( raw1394_write(m_handle, m_node_id, 
		     Addr(Camera_Power),4,
		     &tmp) );  
  WAIT;
  return true;
}

//--------------------------------------------------------------------------
const char*
C1394CameraNode::GetFeatureName(C1394CAMERA_FEATURE feat)
{
  return feature_hi_table[feat];
}

bool
C1394CameraNode::SetParameter(C1394CAMERA_FEATURE feat,unsigned int value)
{
  quadlet_t tmp;
  WAIT;
  WAIT;
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);

//  ERR( "tmp:" <<  hex << tmp << dec <<(int)feat );
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }

  switch(feat){
  case WHITE_BALANCE:
    if ( GetParam(WHITE_BALANCE,U_Value,value) < GetParam(BRIGHTNESS_INQ,MIN_Value,tmp) ||
	 GetParam(WHITE_BALANCE,U_Value,value) > GetParam(BRIGHTNESS_INQ,MAX_Value,tmp)){
      ERR(" bad parameter ");
      return false;
    }
    if ( GetParam(WHITE_BALANCE,V_Value,value) < GetParam(BRIGHTNESS_INQ,MIN_Value,tmp) ||
	 GetParam(WHITE_BALANCE,V_Value,value) > GetParam(BRIGHTNESS_INQ,MAX_Value,tmp)){
      ERR(" bad parameter ");
      return false;
    }
    break;
  default:
    if ( value< GetParam(BRIGHTNESS_INQ,MIN_Value,tmp) ||
	 value> GetParam(BRIGHTNESS_INQ,MAX_Value,tmp)){
      ERR(" bad parameter ");
      return false;
    }
  }

  WAIT;
  WAIT;
  tmp=0;
  tmp|=SetParam(BRIGHTNESS,Value,value);
  tmp|=SetParam(BRIGHTNESS,ON_OFF,1);
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  WAIT;
  WAIT;
  return true;
}

bool
C1394CameraNode::GetParameter(C1394CAMERA_FEATURE feat,unsigned int *value)
{
  quadlet_t tmp;
  WAIT;
  WAIT;
  ReadReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  if (!GetParam(BRIGHTNESS,Presence_Inq,tmp)){
    //ERR("the feature "<<feature_hi_table[feat]<<" is not available");
//    return false;
  }
  *value=tmp&((1<<24)-1);////GetParam(BRIGHTNESS,Value,tmp);
  return true;
}

bool
C1394CameraNode::DisableFeature(C1394CAMERA_FEATURE feat)
{
  quadlet_t tmp=0;
  WAIT; 
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }
  WAIT;
  tmp&=~SetParam(BRIGHTNESS,ON_OFF,0);
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  WAIT;
  return true;
}

bool
C1394CameraNode::EnableFeature(C1394CAMERA_FEATURE feat)
{
  quadlet_t tmp=0;
  WAIT; 
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }
  WAIT;
  tmp&=~SetParam(BRIGHTNESS,ON_OFF,1);
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  WAIT;
  return true;
}

bool
C1394CameraNode::OnePush(C1394CAMERA_FEATURE feat)
{
  quadlet_t tmp=0;
  WAIT; 
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }
/*  if  (GetParam(BRIGHTNESS,One_Push,tmp)!=0){
    return true;
  }*/
  WAIT;
  tmp&=~SetParam(BRIGHTNESS,One_Push,1);
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  WAIT;
  return true;
}

bool
C1394CameraNode::AutoModeOn(C1394CAMERA_FEATURE feat)
{
  quadlet_t tmp=0;
  WAIT; 
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }
  WAIT;
  tmp&=~(SetParam(BRIGHTNESS,A_M_Mode,1));
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  WAIT;
  return true;
}

bool
C1394CameraNode::PreSet_All()
{
  quadlet_t tmp;
  this->SetParameter(BRIGHTNESS,    0x80);
  this->SetParameter(AUTO_EXPOSURE, 0x80);
  this->SetParameter(SHARPNESS,     0x80);
  // default 0x80080
  this->SetParameter(WHITE_BALANCE, (0x80<<12)|0xe0);
  this->SetParameter(HUE,           0x80);
  this->SetParameter(SATURATION,    0x80);
  this->SetParameter(GAMMA,         0x82); // 0x82=liner
  // shutter speed 1/30sec=0x800 1/20,000sec=0xa0d
  this->SetParameter(SHUTTER,       0x800); 
  this->SetParameter(GAIN,          0x01); // def=0x00

  return true;
}

/*
    WAIT;
    WAIT;
    char str[30];
    snprintf(str,sizeof(str),"%20s",feature_hi_table[i]);
    if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
      LOG(str
	  <<(GetParam(BRIGHTNESS_INQ,Auto_Inq,tmp)    ?"  auto ":"  ---  ")
	  <<(GetParam(BRIGHTNESS_INQ,Manual_Inq,tmp)  ?" manual":"  ---  ")
	  <<"["<<GetParam(BRIGHTNESS_INQ,MIN_Value,tmp)<<"-"
	  <<GetParam(BRIGHTNESS_INQ,MAX_Value,tmp)<<"]"
	  <<(GetParam(BRIGHTNESS_INQ,On_Off_Inq,tmp)  ?" on/off":"  ---  ")
	);
    }else{
      LOG(str<< " not available.");
    }
*/

// Sun Dec 12 20:10:33 1999 by hiromasa yoshimoto 
bool 
C1394CameraNode::AutoModeOn_All()
{
  quadlet_t tmp=0;
  for (int i=0;i<12;i++){ // FIXME
    WAIT;WAIT;
    ReadReg(Addr(BRIGHTNESS_INQ)+4*i,&tmp);
    
    if ( GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp) && 
	 GetParam(BRIGHTNESS_INQ,Auto_Inq,tmp) ){
      LOG(" auto on "<< GetFeatureName(( C1394CAMERA_FEATURE)i ));

      ReadReg(Addr(BRIGHTNESS)+4*i,&tmp);

      tmp|=SetParam(BRIGHTNESS,ON_OFF,1);
      tmp|=SetParam(BRIGHTNESS,A_M_Mode,1);
      WriteReg(Addr(BRIGHTNESS)+4*i,&tmp);
      WAIT;
      WAIT;
    }
  }
  return true;
}
bool 
C1394CameraNode::AutoModeOff_All()
{
  for (int i=0;i<12;i++){  // FIXME
    quadlet_t tmp;
    ReadReg(Addr(BRIGHTNESS_INQ)+4*i,&tmp);
    WAIT;
    if ( GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp) &&
	 GetParam(BRIGHTNESS_INQ,Auto_Inq,tmp) ){
      ReadReg(Addr(BRIGHTNESS)+4*i,&tmp);
      WAIT;
      LOG(" auto off "<< GetFeatureName( (C1394CAMERA_FEATURE )i) );
      tmp&=~SetParam(BRIGHTNESS,A_M_Mode,1);
      WriteReg(Addr(BRIGHTNESS)+4*i,&tmp);
      WAIT;
    }
  }
  return true;
}
bool 
C1394CameraNode::OnePush_All()
{
  for (int i=0;i<12;i++){  // FIXME
    quadlet_t tmp;
    ReadReg(Addr(BRIGHTNESS_INQ)+4*i,&tmp);
    WAIT;
    if ( GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp) &&
	 GetParam(BRIGHTNESS_INQ,One_Push_Inq,tmp) ){
      ReadReg(Addr(BRIGHTNESS)+4*i,&tmp);
      WAIT;
      tmp=SetParam(BRIGHTNESS,One_Push,1);
      ReadReg(Addr(BRIGHTNESS)+4*i,&tmp);
      WAIT;
    }
  }
  WAIT;
  return true;
}

//--------------------------------------------------------------------------

bool
C1394CameraNode::SetFormat(FORMAT    fmt,
			   VMODE      mode,
			   FRAMERATE frame_rate)
{
  quadlet_t tmp;
  // check fmt,mode,frame_rate
  if (fmt!=Format_X){
    tmp=SetParam(Cur_V_Format,,fmt);
    WriteReg(Addr(Cur_V_Format),&tmp);
    WAIT;
  }
  if (mode!=Mode_X){
    tmp=SetParam(Cur_V_Mode,,mode); 
    WriteReg(Addr(Cur_V_Mode),&tmp);
    WAIT;
  }
  if (frame_rate!=FrameRate_X){
    tmp=SetParam(Cur_V_Frm_Rate,,frame_rate);
    WriteReg(Addr(Cur_V_Frm_Rate),&tmp);
    WAIT;
  }
  return true;  
}

bool
C1394CameraNode::QueryFormat(FORMAT*    fmt,
			   VMODE*      mode,
			   FRAMERATE* frame_rate)
{
  quadlet_t tmp;
  if (fmt){
    ReadReg(
		       Addr(Cur_V_Format),
		       &tmp);
    WAIT;
    *fmt=(FORMAT)GetParam(Cur_V_Format,,tmp);
  }
  if (mode){
    ReadReg(
			Addr(Cur_V_Mode),
			&tmp);
    WAIT;
    *mode=(VMODE)GetParam(Cur_V_Mode,,tmp); 
  }
  if (frame_rate){
    ReadReg(
			Addr(Cur_V_Frm_Rate),
			&tmp);
    WAIT;
    *frame_rate=(FRAMERATE)GetParam(Cur_V_Frm_Rate,,tmp);
  }
  return true;  
}

bool
C1394CameraNode::SetIsoChannel(int channel)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  quadlet_t tmp;
  // ask IRM ???
  tmp=SetParam(ISO_Channel,,channel)|SetParam(ISO_Speed,,m_iso_speed);
  WriteReg( Addr(ISO_Speed), &tmp);  
  WAIT;
  m_channel=channel;
  return true;  
}

bool
C1394CameraNode::QueryInfo()
{
  int channel;
  QueryIsoChannel(&channel);
  SPD spd;
  QueryIsoSpeed(&spd);
  FORMAT fmt;
  VMODE mode;
  FRAMERATE rate;
  QueryFormat(&fmt,&mode,&rate);

  return true;
}

bool
C1394CameraNode::QueryIsoChannel(int* channel)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  CHK_PARAM(channel!=NULL);
  quadlet_t tmp;
  WAIT; WAIT;
  ReadReg(   Addr(ISO_Speed),    &tmp);  
  WAIT;
  m_channel=
  *channel=GetParam(ISO_Channel,,tmp);
  return true;
}

bool  // SPD
C1394CameraNode::QueryIsoSpeed(SPD* spd)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  CHK_PARAM(spd!=NULL);
  quadlet_t tmp;
  WAIT; WAIT;
  ReadReg(    Addr(ISO_Speed),    &tmp);  
  WAIT;
  *spd=(SPD)GetParam(ISO_Speed,,tmp);
  return true;
}

bool
C1394CameraNode::SetIsoSpeed(SPD iso_speed)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  quadlet_t tmp;
  tmp=SetParam(ISO_Speed,,iso_speed)|SetParam(ISO_Channel,,m_channel);
  ReadReg(
		      Addr(ISO_Speed),
		     &tmp); 
  WAIT;
  return true;
}


bool 
C1394CameraNode::OneShot()
{
//  LOG("one shot.");
  quadlet_t tmp=SetParam(One_Shot,,1);
  WriteReg(
		     Addr(One_Shot),
		     &tmp) ;  
  WAIT;
  return true;
}
bool
C1394CameraNode:: StartIsoTx(unsigned int count_number)
{
  EXCEPT_FOR_FORMAT_6_ONLY;

  if (MAX_COUNT_NUMBER==count_number){
    /* start Iso transmission of video data */
    quadlet_t tmp;
    tmp=SetParam(ISO_EN,,1);
    WriteReg(
			Addr(ISO_EN),
			&tmp);
    WAIT;
  }else{
    quadlet_t tmp;
    tmp =SetParam(Multi_Shot,,1);
    tmp|=SetParam(Count_Number,,count_number);
    WriteReg(
			Addr(Multi_Shot),
			&tmp);
    WAIT;
  }
  return true;
}

bool
C1394CameraNode:: SetTriggerOn()
{
  quadlet_t tmp;

  ReadReg(
		     Addr(TRIGGER_INQ),
		     &tmp);  
  WAIT;
  ReadReg(
		      Addr(TRIGGER_MODE),
		     &tmp);  
  WAIT;

  LOG(" SetTriggerOn"<<tmp);
  
  tmp=0x82010000;

/*
    SetParam(TRIGGER_MODE,  Presence_Inq, 1)
    |SetParam(TRIGGER_MODE, ON_OFF, 1)
    |SetParam(TRIGGER_MODE, Trigger_Polarity, 1)
    |SetParam(TRIGGER_MODE, Trigger_Mode,1<<4); */

  LOG(" SetTriggerOn "<<tmp);
  
  WriteReg(
		     Addr(TRIGGER_MODE),
		     &tmp);
  WAIT; 

  return true;
}

bool
C1394CameraNode:: SetTriggerOff()
{
  LOG("SetTriggerOff");
  quadlet_t tmp;
  ReadReg(Addr(TRIGGER_INQ),&tmp);  
  WAIT;
  ReadReg(Addr(TRIGGER_MODE),&tmp);  
  WAIT;
  tmp=0x80010000;
  
/*  SetParam(TRIGGER_MODE,  Presence_Inq, 1)
    |SetParam(TRIGGER_MODE, ON_OFF, 1)
    |SetParam(TRIGGER_MODE, Trigger_Polarity, 1)
    |SetParam(TRIGGER_MODE, Trigger_Mode,1<<4); */
  
  WriteReg(Addr(TRIGGER_MODE),&tmp);
  WAIT; 
  return true;
}


bool
C1394CameraNode:: StopIsoTx()
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  quadlet_t tmp;
  tmp=SetParam(ISO_EN,,0);
  WriteReg(Addr(ISO_EN),&tmp);
  WAIT;
  return true;
}

int
C1394CameraNode::AllocateFrameBuffer(int channel,
			  FORMAT fmt,
			  VMODE mode,
			  FRAMERATE rate)
{
  if (channel==-1){
    QueryIsoChannel(&channel);
    LOG("query iso channel "<<channel);
  }else{
    LOG("set iso channel  "<<channel);
    SetIsoChannel(channel);
    QueryIsoChannel(&channel);
  }
  
  LOG("channel "<<channel);
  
  if (!(0<=channel)&&(channel<64)){
    LOG(" can't determine channel.");
    return -1;
  }


  SetFormat(fmt,mode,rate);
  if ((fmt==Format_X)||(mode==Mode_X)||(rate==FrameRate_X)){
    FORMAT f; VMODE m; FRAMERATE r;
    QueryFormat(&f,&m,&r);
    if (fmt ==Format_X)    fmt=f;
    if (mode==Mode_X)      mode=m;
    if (rate==FrameRate_X) rate=r;
  }
  SPD spd=::GetRequiredSpeed(fmt,mode,rate);


  m_packet_sz  = ::GetPacketSize(fmt,mode,rate);
  m_packet_sz += 8;
  m_num_packet = ::GetNumPackets(fmt,mode,rate);

  fd=open("/dev/isofb0", O_RDWR);
  if (-1==fd){
    LOG( "can't open "<<"/dev/isofb0"<<" "<<strerror(errno) );
    return -1;
  }
  
  ISO1394_RxParam rxparam;
  TRY( ioctl(fd, IOCTL_INIT_RXPARAM, &rxparam) );
  
  rxparam.sz_packet      = m_packet_sz ;
  rxparam.num_packet     = m_num_packet ;
  rxparam.num_frames     = 3 ;
  rxparam.packet_per_buf = 0;
  rxparam.wait           = 1;
  rxparam.sync           = 1;
  rxparam.channelNum     = channel ;


  TRY( ioctl(fd, IOCTL_CREATE_RXBUF, &rxparam) );

  pMaped=(char*)mmap(NULL,
		     rxparam.sz_allocated,
		     PROT_READ|PROT_WRITE,
		     MAP_SHARED, 
		     fd,0);

  TRY( ioctl(fd, IOCTL_START_RX) );
  
  m_Image_W=::GetImageWidth(fmt,mode);
  m_Image_H=::GetImageHeight(fmt,mode);
  
  m_BufferSize=m_packet_sz*m_num_packet;
  m_pixel_format=video_image_info[fmt][mode].pixel_format;
 
  return 0;
}

int    
C1394CameraNode::GetFrameCount(int* count)
{
  return GetFrameCounter(fd,count);
}
int    
C1394CameraNode::SetFrameCount(int tmp)
{
  return SetFrameCounter(fd,tmp);
}

void*
C1394CameraNode::UpDateFrameBuffer(BUFFER_OPTION opt,BufferInfo* info)
{
  ISO1394_Chunk chunk;
  TRY( ioctl(fd, IOCTL_GET_RXBUF, &chunk) );
  return m_lpFrameBuffer=(pMaped+chunk.offset);
}

int
C1394CameraNode::GetFrameBufferSize()
{
  return m_BufferSize;
}
int    
C1394CameraNode::GetImageWidth()
{
  return m_Image_W;
}
int    
C1394CameraNode::GetImageHeight()
{
  return m_Image_H;
}

int
C1394CameraNode::CopyRGBAImage(void* dest)
{

  switch  ( m_pixel_format ){
  case VFMT_YUV422:
    ::copy_YUV422toRGBA((RGBA*)dest,m_lpFrameBuffer,
			m_packet_sz,m_num_packet,
			REMOVE_HEADER);
    break;
  case VFMT_YUV411:
    ::copy_YUV411toRGBA((RGBA*)dest,m_lpFrameBuffer,
			m_packet_sz,m_num_packet,
			REMOVE_HEADER);
    break;
  case VFMT_YUV444:
    ::copy_YUV444toRGBA((RGBA*)dest,m_lpFrameBuffer,
			m_packet_sz,m_num_packet,
			REMOVE_HEADER);
    break;
  default:
    LOG("not support this pixel format yet.");
    break;
  }
  
  return 0;
}


// save current frame to file
int  
C1394CameraNode::SaveToFile(char* filename,FILE_TYPE type)
{
//  switch (type){
  {
    bool result=false;
    FILE* fp=fopen(filename,"w");
    if (fp){
      int i;
      int w=GetImageWidth();
      int h=GetImageHeight();
      fprintf(fp,"P6\n %d %d\n 255\n",w,h);
      RGBA buf[w*h];
      CopyRGBAImage(buf);
      RGBA* img=buf;
      for (i=0;i<w*h;i++){
	fwrite(&img->r,1,1,fp);
	fwrite(&img->g,1,1,fp);
	fwrite(&img->b,1,1,fp);
	img++;
      }
      fclose(fp);
    }else{
      fprintf(stderr,"can't create file %s \n",filename);
    }
    return result;
  }
  return 0;
}

//--------------------------------------------------------------------------
int
SetFrameCounter(int fd, int counter)
{
  return  (0!=ioctl(fd,IOCTL_SET_COUNT,counter));
}

int
GetFrameCounter(int fd, int* counter)
{
  if (NULL==counter)
    return false;
  return  (0!=ioctl(fd,IOCTL_GET_COUNT,counter));
}

//--------------------------------------------------------------------------
const char*
GetVideoFormatString(FORMAT fmt,VMODE mode)
{
  if (!(0<=fmt&&fmt<1))
    return NULL;
  
  PIXEL_FORMAT pixel=video_image_info[fmt][mode].pixel_format;
  return video_pixel_info[ pixel ].name;
}
const char*
GetSpeedString(SPD spd)
{
  static char* speed_string[]=
  {
    "100Mbps",
    "200Mbps",
    "400Mbps",
  };
  return speed_string[spd];
}
int
GetPacketSize(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
  return video_packet_info[fmt][mode][frame_rate].packet_sz;
}
int 
GetNumPackets(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
  return video_packet_info[fmt][mode][frame_rate].num_packets;
}
int
GetImageWidth(FORMAT fmt,VMODE mode)
{
  return video_image_info[fmt][mode].w;
}
int
GetImageHeight(FORMAT fmt,VMODE mode)
{
  return video_image_info[fmt][mode].h;
}

SPD
GetRequiredSpeed(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
  return video_packet_info[fmt][mode][frame_rate].required_speed;
}

// Sat Dec 11 07:00:47 1999 by hiromasa yoshimoto 
// end of [1394cam.cc ]
