/*!
  @file  1394cam.h
  @brief 1394-based Digital Camera control class
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: 1394cam.h,v 1.3 2002-02-15 19:50:24 yosimoto Exp $
  @date    $Date: 2002-02-15 19:50:24 $
 */
#if !defined(_1394cam_h_included_)
#define _1394cam_h_included_

#include <list>
#include <netinet/in.h>

#if !defined IPLAPI
class IplImage { };
#endif // #if defined IPLAPI

#define WAIT usleep(1)

#define CAT(a,b)  a##b
#define RegInfo(offset,name) \
 const quadlet_t CAT(OFFSET,_##name) = offset; 
#define BitInfo(name,field,start,end) \
 const quadlet_t CAT(MASK_##name,_##field) = \
 (((int64_t)1<<(end-start+1))-1)<<(31-(end)); \
 const quadlet_t CAT(WAIT_##name,_##field) = (31-(end)) ;

//=========================================================
RegInfo(0x0000,INITIALIZE)    // *camera initialize register
RegInfo(0x0100,V_FORMAT_INQ)  // *inquiry registers for video fomrat
RegInfo(0x0180,V_FORMAT_INQ_0)
RegInfo(0x0184,V_FORMAT_INQ_1)
RegInfo(0x0188,V_FORMAT_INQ_2)
//RegInfo(0x018c,V_FORMAT_INQ_3)  // not defined at spec 1.20
//RegInfo(0x0190,V_FORMAT_INQ_4)
//RegInfo(0x0194,V_FORMAT_INQ_5)
RegInfo(0x0198,V_FORMAT_INQ_6)
RegInfo(0x019c,V_FORMAT_INQ_7)
RegInfo(0x0400,BASIC_FUNC_INQ)  // *inquiry register for basic function
RegInfo(0x0404,Feature_Hi_Inq)  // *inquiry register for feature presence
RegInfo(0x0408,Feature_Lo_Inq) 
RegInfo(0x0480,Advanced_Feature_Inq) 
RegInfo(0x0500,BRIGHTNESS_INQ)  // * inquiry register for feature elements 
RegInfo(0x0504,AUTO_EXPOSURE_INQ)
RegInfo(0x0508,SHARPNESS_INQ)
RegInfo(0x050c,WHITE_BAL_INQ)
RegInfo(0x0510,HUE_INQ)
RegInfo(0x0514,SATURATION_INQ)
RegInfo(0x0518,GAMMA_INQ)
RegInfo(0x051c,SHUTTER_INQ)
RegInfo(0x0520,GAIN_INQ)
RegInfo(0x0524,IRIS_INQ)
RegInfo(0x0528,FOCUS_INQ)
RegInfo(0x052c,TEMPERATURE_INQ)
RegInfo(0x0530,TRIGGER_INQ)
RegInfo(0x0580,ZOOM_INQ)
RegInfo(0x0584,PAN_INQ)
RegInfo(0x0500,TILT_INQ)

RegInfo(0x0600,Cur_V_Frm_Rate)
RegInfo(0x0600,Revision)
RegInfo(0x0604,Cur_V_Mode)
RegInfo(0x0608,Cur_V_Format)
RegInfo(0x060C,ISO_Channel)
RegInfo(0x060C,ISO_Speed)
RegInfo(0x0610,Camera_Power)
RegInfo(0x0614,ISO_EN)
RegInfo(0x0614,Continous_Shot)
RegInfo(0x0618,Memory_Save)
RegInfo(0x061c,One_Shot)
RegInfo(0x061c,Multi_Shot)
RegInfo(0x061c,Count_Number)
RegInfo(0x0620,Mem_Save_Ch)
RegInfo(0x0624,Cur_Mem_Ch)
// strage media CSR (only for format_6)
// strage image CSR (only for format_6)
RegInfo(0x0800,BRIGHTNESS)  // * status and control register for feature
RegInfo(0x0804,AUTO_EXPOSURE)
RegInfo(0x0808,SHARPNESS)
RegInfo(0x080c,WHITE_BALANCE)
RegInfo(0x0810,HUE)
RegInfo(0x0814,SATURATION)
RegInfo(0x0818,GAMMA)
RegInfo(0x081c,SHUTTER)
RegInfo(0x0820,GAIN)
RegInfo(0x0824,IRIS)
RegInfo(0x0828,FOCUS)
RegInfo(0x082c,TEMPERATURE)
RegInfo(0x0830,TRIGGER_MODE)
RegInfo(0x0880,ZOOM)
RegInfo(0x0884,PAN)
RegInfo(0x0888,TILT)
//  video mode csr for format_7

//=========================================================

//         name          field        bit 
BitInfo(INITIALIZE      , Initialize    ,  0, 0)
BitInfo(V_FORMAT_INQ    , Format_0      ,  0, 0)
BitInfo(V_FORMAT_INQ    , Format_1      ,  1, 1)
BitInfo(V_FORMAT_INQ    , Format_2      ,  2, 2)
BitInfo(V_FORMAT_INQ    , Format_3      ,  3, 3)
BitInfo(V_FORMAT_INQ    , Format_4      ,  4, 4)
BitInfo(V_FORMAT_INQ    , Format_5      ,  5, 5)
BitInfo(V_FORMAT_INQ    , Format_6      ,  6, 6)
BitInfo(V_FORMAT_INQ    , Format_7      ,  7, 7)
BitInfo(BASIC_FUNC_INQ  ,Advanced_Feature_Inq , 0, 0)
BitInfo(BASIC_FUNC_INQ  ,Cam_Power_Cntl       ,16,16)
BitInfo(BASIC_FUNC_INQ  ,One_Shot_Inq         ,19,19)
BitInfo(BASIC_FUNC_INQ  ,Multi_Shot_Inq       ,20,20)
BitInfo(BASIC_FUNC_INQ  ,Memory_Channel       ,28,31)
BitInfo(Feature_Hi_Inq  ,Brightness           , 0, 0)
BitInfo(Feature_Hi_Inq  ,Auto_Exposure        , 1, 1)
BitInfo(Feature_Hi_Inq  ,Sharpness            , 2, 3)
BitInfo(Feature_Hi_Inq  ,White_Balance        , 3, 3)
BitInfo(Feature_Hi_Inq  ,Hue                  , 4, 4)
BitInfo(Feature_Hi_Inq  ,Saturation           , 5, 5)
BitInfo(Feature_Hi_Inq  ,Gamma                , 6, 6)
BitInfo(Feature_Hi_Inq  ,Shutter              , 7, 7)
BitInfo(Feature_Hi_Inq  ,Gain                 , 8, 8)
BitInfo(Feature_Hi_Inq  ,Iris                 , 9, 9)
BitInfo(Feature_Hi_Inq  ,Focus                ,10,10)
BitInfo(Feature_Hi_Inq  ,Temperture           ,11,11)
BitInfo(Feature_Hi_Inq  ,Trigger              ,12,12)
BitInfo(Feature_Lo_Inq  ,Zoom                 , 0, 0)
BitInfo(Feature_Lo_Inq  ,Pan                  , 1, 1)
BitInfo(Feature_Lo_Inq  ,Tilt                 , 2, 2)
BitInfo(Feature_Lo_Inq  ,Optical_Filter       , 3, 3)
BitInfo(Feature_Lo_Inq  ,Capture_Size         ,16,16)
BitInfo(Feature_Lo_Inq  ,Capture_Quality      ,17,17)
BitInfo(Advanced_Feature_Inq,Advanced_Feature_Quadlet_Offset,0,31)

#define BitInfo_feature_element(name) \
BitInfo(name,  Presence_Inq,           0, 0) \
BitInfo(name,  One_Push_Inq,          3, 3) \
BitInfo(name,  ReadOut_Inq,           4, 4) \
BitInfo(name,  On_Off_Inq,             5, 5) \
BitInfo(name,  Auto_Inq,              6, 6) \
BitInfo(name,  Manual_Inq,            7, 7) \
BitInfo(name,  MIN_Value,             8,19) \
BitInfo(name,  MAX_Value,            20,31)

BitInfo_feature_element(BRIGHTNESS_INQ)
BitInfo_feature_element(AUTO_EXPOSURE_INQ)
BitInfo_feature_element(SHARPNESS_INQ)
BitInfo_feature_element(WHITE_BAL_INQ)
BitInfo_feature_element(HUE_INQ)
BitInfo_feature_element(SATURATION_INQ)
BitInfo_feature_element(GAMMA_INQ)
BitInfo_feature_element(SHUTTER_INQ)
BitInfo_feature_element(GAIN_INQ)
BitInfo_feature_element(IRIS_INQ)
BitInfo_feature_element(FOCUS_INQ)
BitInfo_feature_element(TEMPERATURE_INQ)
BitInfo(TRIGGER_INQ,  Presece_Inq,           0, 0) //
BitInfo(TRIGGER_INQ,  ReadOut_Inq,           4, 4)
BitInfo(TRIGGER_INQ,  On_Off_Inq,             5, 5)
BitInfo(TRIGGER_INQ,  Polarity,              6, 6)
BitInfo(TRIGGER_INQ,  Trigger_Mode0_Inq,    16,16)
BitInfo(TRIGGER_INQ,  Trigger_Mode1_Inq,    17,17)
BitInfo(TRIGGER_INQ,  Trigger_Mode2_Inq,    18,18)
BitInfo(TRIGGER_INQ,  Trigger_Mode3_Inq,    19,19)
BitInfo_feature_element(ZOOM_INQ)
BitInfo_feature_element(PAN_INQ)
BitInfo_feature_element(TILT_INQ)
BitInfo_feature_element(OPTICAL_FILTER_INQ)
BitInfo_feature_element(CAPTURE_SIZE_INQ)
BitInfo_feature_element(CAPTURE_QUALITY_INQ)
#undef BitInfo_feature_element

BitInfo(Cur_V_Frm_Rate,, 0, 2)
BitInfo(Revision,      , 0, 2)
BitInfo(Cur_V_Mode,    , 0, 2)
BitInfo(Cur_V_Format,  , 0, 2)
BitInfo(ISO_Channel,   , 0, 3)
BitInfo(ISO_Speed,     , 6, 7)
BitInfo(Camera_Power,  , 0, 0)
BitInfo(ISO_EN,        , 0, 0)
BitInfo(Continous_Shot,, 0, 0)
BitInfo(Memory_Save,   , 0, 0)
BitInfo(One_Shot,      , 0, 0)
BitInfo(Multi_Shot,    , 1, 1)
BitInfo(Count_Number,  ,16,31)
BitInfo(Mem_Save_Ch,   , 0, 3)
BitInfo(Cur_Mem_Ch,    , 0, 3)

#define BitInfo_ctrl_reg_for_feat(x) \
BitInfo(x,Presence_Inq, 0, 0) \
BitInfo(x,One_Push    , 5, 5) \
BitInfo(x,ON_OFF      , 6, 6) \
BitInfo(x,A_M_Mode    , 7, 7) \
BitInfo(x,Value       , 8,31)

BitInfo_ctrl_reg_for_feat(BRIGHTNESS)
BitInfo_ctrl_reg_for_feat(AUTO_EXPOSURE)
BitInfo_ctrl_reg_for_feat(SHARPNESS)
BitInfo(WHITE_BALANCE,Presence_Inq, 0, 0) //
BitInfo(WHITE_BALANCE,One_Push    , 5, 5)
BitInfo(WHITE_BALANCE,ON_OFF      , 6, 6)
BitInfo(WHITE_BALANCE,A_M_Mode    , 7, 7)
BitInfo(WHITE_BALANCE,U_Value     , 8,19)
BitInfo(WHITE_BALANCE,B_Value     , 8,19)
BitInfo(WHITE_BALANCE,V_Value     ,20,31)
BitInfo(WHITE_BALANCE,R_Value     ,20,31)
BitInfo_ctrl_reg_for_feat(HUE)
BitInfo_ctrl_reg_for_feat(SATURATION)
BitInfo_ctrl_reg_for_feat(GAMMA)
BitInfo_ctrl_reg_for_feat(SHUTTER)
BitInfo_ctrl_reg_for_feat(GAIN)
BitInfo_ctrl_reg_for_feat(IRIS)
BitInfo_ctrl_reg_for_feat(FOCUS)
BitInfo(TEMPERATURE,Presence_Inq      , 0, 0) //
BitInfo(TEMPERATURE,One_Push          , 5, 5)
BitInfo(TEMPERATURE,ON_OFF            , 6, 6)
BitInfo(TEMPERATURE,A_M_Mode          , 7, 7)
BitInfo(TEMPERATURE,Terget_Temperature, 8,19)
BitInfo(TEMPERATURE,Temperature       ,20,31)
BitInfo(TRIGGER_MODE,Presence_Inq     , 0, 0) //
BitInfo(TRIGGER_MODE,ON_OFF           , 6, 6)
BitInfo(TRIGGER_MODE,Trigger_Polarity , 7, 7)
BitInfo(TRIGGER_MODE,Trigger_Mode     ,12,15)
BitInfo(TRIGGER_MODE,Parameter        ,20,31)
BitInfo_ctrl_reg_for_feat(ZOOM)
BitInfo_ctrl_reg_for_feat(PAN)
BitInfo_ctrl_reg_for_feat(TILT)
BitInfo_ctrl_reg_for_feat(OPTICAL_FILTER)
BitInfo_ctrl_reg_for_feat(CAPTURE_SIZE)
BitInfo_ctrl_reg_for_feat(CAPTURE_QUALITY)
#undef BitInfo_ctrl_reg_for_feat
#undef RegInfo
#undef BitInfo

//---------------------------------------------------------------

enum PIXEL_FORMAT {
  VFMT_YUV444  =0,
  VFMT_YUV422  =1,
  VFMT_YUV411  =2,
  VFMT_RGB888  =3,
  VFMT_Y8      =4,

  VFMT_NOT_SUPPORTED ,
};

enum FORMAT {
  Format_0 = 0,
  Format_1 = 1,
  Format_2 = 2,
  Format_3 = 3,
  Format_4 = 4,
  Format_5 = 5,
  Format_6 = 6,
  Format_7 = 7,

  Format_X=-1,
};
enum VMODE {  
  Mode_0= 0,
  Mode_1= 1,
  Mode_2= 2,
  Mode_3= 3,
  Mode_4= 4,
  Mode_5= 5,
  Mode_6= 6,

  Mode_X=-1,
};

enum FRAMERATE {
  FrameRate_0  =0x00,  /* 1.875fps */
  FrameRate_1  =0x01,  /* 3.75fps  */
  FrameRate_2  =0x02,  /* 7.5fps   */
  FrameRate_3  =0x03,  /* 15fps    */
  FrameRate_4  =0x04,  /* 30fps    */
  FrameRate_5  =0x05,  /* 30fps    */

  FrameRate_X=-1,
};

enum C1394CAMERA_FEATURE {
  BRIGHTNESS          =0,
  AUTO_EXPOSURE      ,
  SHARPNESS          ,
  WHITE_BALANCE      ,
  HUE                ,
  SATURATION         , 
  GAMMA              ,
  SHUTTER             ,
  GAIN                 ,
  IRIS               ,
  FOCUS              ,
  TEMPERATURE         ,
  TRIGGER            ,
  // reserved for other FEATURE_HI
  ZOOM                =32,
  PAN   ,
  TILT  ,
  OPTICAL_FILTER ,
  // reserved for other FEATURE_LO
  CAPTURE_SIZE = 48,
  CAPTURE_QUALITY,
};

//-------------------------------------------------------

class C1394Node {
public:
  unsigned int m_VenderID;
  uint64_t  m_ChipID;
#if 0
  virutal int On_BusReset()
  {
    return 0;
  }
#endif 
};

struct BufferInfo {
  // FIXME -- not implemented yet
};

class C1394CameraNode : public C1394Node {

//  typedef quadlet_t CAMERA_ID; // uniq id 
//  CAMERA_ID m_CameraID;        // uniq id of camera
private:
  enum {
    MAX_COUNT_NUMBER=0xffffffff,
  };
protected:
  unsigned is_format6:1;           // '1':format_6 /  '0':other format
//  FORMAT m_Format;
//  VMODE   m_mode;
//  FRAMERATE m_rate;
  int   m_channel;                 // -1 or "isochronus channel"
  int   m_iso_speed;               // isochronus speed

  char* m_lpModelName;             // camera name
  char* m_lpVecderName;            // camera vender name
  nodeaddr_t CMD(nodeaddr_t reg) ;
//  int On_BusReset();

public:

  raw1394handle_t m_handle;        // handle of 1394 I/F
  nodeid_t m_node_id;              // node_id of this node
  nodeaddr_t m_command_regs_base;  // base address of camera's cmd reg

  int fd;
  char *pMaped;

  C1394CameraNode();
  virtual ~C1394CameraNode();
  
  char* GetModelName(char* lpBuffer,size_t* lpLength);
  char* GetVenderName(char* lpBuffer,size_t* lpLength);

  bool ReadReg(nodeaddr_t addr,quadlet_t* value);
  bool WriteReg(nodeaddr_t addr,quadlet_t* value);

  bool ResetToInitialState();
  bool PowerDown();
  bool PowerUp();  

  bool SetParameter(C1394CAMERA_FEATURE feat, unsigned int value);
  bool GetParameter(C1394CAMERA_FEATURE feat, unsigned int* value);
  const char* GetFeatureName(C1394CAMERA_FEATURE feat);

  // Thu Sep 27 08:26:16 2001  YOSHIMOTO Hiromasa add
  bool HasFeature(C1394CAMERA_FEATURE feat);
  bool EnableFeature(C1394CAMERA_FEATURE feat);
  bool DisableFeature(C1394CAMERA_FEATURE feat);
  bool AutoModeOn(C1394CAMERA_FEATURE feat);
  bool AutoModeOff(C1394CAMERA_FEATURE feat);
  bool OnePush(C1394CAMERA_FEATURE feat);

  bool PreSet_All(void);
  bool AutoModeOn_All(void);
  bool AutoModeOff_All(void);
  bool OnePush_All(void);

  bool SetTriggerOn();
  bool SetTriggerOff();

  bool  QueryInfo();
  bool  QueryIsoChannel(int* channel);
  bool  QueryIsoSpeed(SPD* iso_speed);
  bool  QueryFormat(FORMAT* fmt,VMODE* mode, FRAMERATE* rate);

  bool  SetFormat(FORMAT  fmt,VMODE  mode, FRAMERATE  rate);
  bool  SetIsoChannel(int  channel);
  bool  SetIsoSpeed(SPD  iso_speed);

  bool  OneShot();
  bool  StartIsoTx(unsigned int count_number =MAX_COUNT_NUMBER);
  bool  StopIsoTx();
  
  int   GetChannel(){return m_channel;}

private:
  PIXEL_FORMAT m_pixel_format;
  int   m_BufferSize;
  char*  m_lpFrameBuffer;     // point to frame buffer.
  int  m_Image_W;
  int  m_Image_H;
  int  m_packet_sz;
  int  m_num_packet;
  
  bool  m_bIsInitalized;
//  FrameBufferInfo m_BufInfo;
public:
  int m_last_read_frame;
  enum BUFFER_OPTION {
    BUFFER_DEFAULT    = 0 ,
    LAST              ,   // read last  captured image.
    AS_FIFO           ,   // read first captured image.   
    WAIT_NEW_FRAME    ,   // block till new frame will be caputerd.
  };
  enum FILE_TYPE {
    FILETYPE_PPM =0x000,  
    FILETYPE_PNM =0x001,  
    FILETYPE_PGM =0x002,  
  };
public:
  int  AllocateFrameBuffer(int       channel = -1         ,
			   FORMAT    fmt     = Format_X   ,
			   VMODE     mode    = Mode_X     ,
			   FRAMERATE rate    = FrameRate_X);
  
  int    GetFrameCount(int*);
  int    SetFrameCount(int);
  void*  UpDateFrameBuffer(BUFFER_OPTION opt=BUFFER_DEFAULT,
			   BufferInfo* info=0);
  int    GetFrameBufferSize();
  int    GetImageWidth();
  int    GetImageHeight();
  
  int    CopyRGBAImage(void* dest);
  int    CopyIplImage(IplImage* dest);

//  int    WriteRGBAImage(int fd);
//  int    WriteRawImage(int fd);
  int    SaveToFile(char* filename,FILE_TYPE type=FILETYPE_PPM);  // save current frame

protected:
  int    AllocateBuffer();  // (re)allocate buffer for current-mode
  int    ReleaseBuffer();

};

#define ISORX_ISOHEADER 0x000001
int EnableCyclemaster(raw1394handle_t handle);
int DisableCyclemaster(raw1394handle_t handle);
int CopyFrameBuffer(raw1394handle_t handle,
		    int channel,
		    void* buf,int size);
int AllocateIsoChannel(raw1394handle_t handle,
		       int channel,SPD spd);
int ReleaseIsoChannel(raw1394handle_t handle,
		      int channel);
int ReleaseIsoChannelAll(raw1394handle_t handle);
int AllocateFrameBuffer(raw1394handle_t handle,
			int channel,
			SPD spd,
			int packet_sz,
			int num_packets,
			int buf_count,
			int flag);
int SetFrameCounter(int fd,int counter);
int GetFrameCounter(int fd,int* ounter);

//
int GetPacketSize(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetNumPackets(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetImageWidth(FORMAT fmt,VMODE mode);
int GetImageHeight(FORMAT fmt,VMODE mode);
const char* GetVideoFormatString(FORMAT fmt,VMODE mode);
const char* GetSpeedString(SPD rate);

//typedef list<C1394CameraNode> C1394CameraNodeList;
//typedef int (*ENUM1394_CALLBACK)(T* node,void* id);
template <typename T> int
Enum1394Node(raw1394_handle* handle,
	     raw1394_portinfo* pPort,
	     list<T>* pList,
	     int 
	     (*func)(raw1394_handle* handle,nodeid_t node_id,
		     T* pNode,void* arg),
	     void* arg)
{
  T the_node;
  int i;
  for (i = 0; i < pPort->nodes; i++)
    if ((*func)(handle, 0xffc0 | i,&the_node,arg))
      pList->push_back(the_node);
  return 0;
}
typedef list<C1394CameraNode> CCameraList;

bool GetCameraList(raw1394handle_t,CCameraList *);
CCameraList::iterator find_camera_by_id(CCameraList& CameraList,int64_t id);

// helper macro
#define DWFV500_MAGICNUMBER 8589965664ULL
#define MAKE_CAMERA_ID(x) ((int64_t)(x)-DWFV500_MAGICNUMBER)
#define MAKE_CHIP_ID(x)   ((int64_t)(x)+DWFV500_MAGICNUMBER)

#define SetParam(name,field,value)  \
( CAT(MASK_##name,_##field ) & ((value)<< CAT(WAIT_##name,_##field)))
#define GetParam(name,field,value)  \
(( CAT(MASK_##name,_##field) & (value))>> CAT(WAIT_##name,_##field) )

#endif // #if !defined(_1394cam_h_included_)
// end of [1394cam.h]
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
