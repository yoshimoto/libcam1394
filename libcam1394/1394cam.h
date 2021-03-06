/**
 * @file    1394cam.h
 * @brief   1394-based Digital Camera control class
 * @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
 * @version $Id: 1394cam.h,v 1.27 2008-12-15 07:23:03 yosimoto Exp $
 */

#if !defined(_1394cam_h_included_)
#define _1394cam_h_included_

#include <list>
#include <netinet/in.h>
#include <libraw1394/raw1394.h>
#include <libcam1394/1394cam_registers.h>

#if !defined _OHCI1394_ISO_H_ 
enum SPD {
    SPD_100M  =0x00,
    SPD_200M  =0x01,
    SPD_400M  =0x02,
    SPD_800M  =0x03,
    SPD_1600M =0x04,
    SPD_3200M =0x05,
};
#endif

typedef struct _IplImage IplImage;

//! pixel format codes
enum PIXEL_FORMAT {
    VFMT_YUV444  =0,  //!< YUV444  = 24 bit/pixel
    VFMT_YUV422  =1,  //!< YUV422  = 16 bit/pixel
    VFMT_YUV411  =2,  //!< YUV411  = 12 bit/pixel
    VFMT_RGB888  =3,  //!< RGB888  = 24 bit/pixel
    VFMT_Y8      =4,  //!< Y8      =  8 bit/pixel, gray image
    VFMT_Y16     =5,  //!< Y16     = 16 bit/pixel, gray image
    VFMT_NOT_SUPPORTED ,
};

//! video format codes. This library supports only Format_0 to Format_2 now.
// @todo Format_6, Format_7 is not supported yet.
enum FORMAT {
    Format_0 = 0, //!< VGA non-compressed format 
    Format_1 = 1, //!< Super VGA non-compressed format(1)
    Format_2 = 2, //!< Super VGA non-compressed format(2)
    Format_3 = 3, //!< reserved for other format
    Format_4 = 4, //!< reserved for other format
    Format_5 = 5, //!< reserved for other format
    Format_6 = 6, //!< Still Image Format    (not supported yet)
    Format_7 = 7, //!< Scalable Image Format (not supported yet)

    Format_X=-1,
};

//! video mode codes.
enum VMODE {  
    Mode_0= 0, //!<  video mode_0
    Mode_1= 1, //!<  video mode_1
    Mode_2= 2, //!<  video mode_2
    Mode_3= 3, //!<  video mode_3
    Mode_4= 4, //!<  video mode_4
    Mode_5= 5, //!<  video mode_5
    Mode_6= 6, //!<  video mode_6
    Mode_7= 7, //!<  video mode_7

    Mode_X=-1,
};


//! video frame rate codes
enum FRAMERATE {
    FrameRate_0  = 0x00,  //!< 1.875fps 
    FrameRate_1  = 0x01,  //!< 3.75fps  
    FrameRate_2  = 0x02,  //!< 7.5fps   
    FrameRate_3  = 0x03,  //!< 15fps    
    FrameRate_4  = 0x04,  //!< 30fps    
    FrameRate_5  = 0x05,  //!< 60fps    

    FrameRate_X=-1, 
};

//! camera feature codes.
enum C1394CAMERA_FEATURE {
    BRIGHTNESS      = 0,      //!< brightness control
    AUTO_EXPOSURE      ,      //!< auto exposure control
    SHARPNESS          ,      //!< sharpness control
    WHITE_BALANCE      ,      //!< white balance control
    HUE                ,      //!< hue control
    SATURATION         ,      //!< saturation control
    GAMMA              ,      //!< gamma control
    SHUTTER            ,      //!< shutter control
    GAIN               ,      //!< gain control
    IRIS               ,      //!< iris control
    FOCUS              ,      //!< forcus control
    TEMPERATURE        ,      //!< temperature control
    TRIGGER            ,      //!< trigger control

    TRIGGER_DELAY      ,      //!< trigger delay control
    WHITE_SHD          ,      //!< white shading  control
    FRAME_RATE         ,      //!< frame rate control

    // reserved for other FEATURE_HI

    ZOOM           = 32,      //!< zoom control
    PAN                ,      //!< pan control
    TILT               ,      //!< tilt control
    OPTICAL_FILTER     ,      //!< optical filter control 

    // reserved for other FEATURE_LO

    CAPTURE_SIZE   = 48,      //!< capture size control
    CAPTURE_QUALITY    ,      //!< capture quality control

    END_OF_FEATURE   ,
};

//! each camera's feature has four states OFF, AUTO, MANUAL and ONE_PUSH.
enum C1394CAMERA_FSTATE
{
    OFF          = 0,           //!< will be fixed value.
    AUTO            ,		//!< control automatically by itself
				//   continuously.
    MANUAL          ,		//!< control manually by written value
				//   by SetParamter().
    ONE_PUSH        ,		//!< control automatically by itself
				//   only once and returns MANUAL control
				//   state with adjusted value.
    END_OF_FSTATE
};

//-------------------------------------------------------

class C1394Node {
public:
    unsigned int  m_VenderID;  //!< vender id
    uint64_t      m_ChipID;    //!< chip id
};

struct BufferInfo {
    unsigned int timestamp;
};

class C1394CameraNode : public C1394Node {
private:
    enum {
	MAX_COUNT_NUMBER=0xffffffff,
    };
protected:

    unsigned is_format6:1;           // '1':format_6 /  '0':other format

    int   m_channel;                 // -1 or "isochronus channel"
    int   m_iso_speed;               // isochronus speed
public:
    char* m_lpModelName;             // camera name
    char* m_lpVenderName;            // camera vender name

    int   m_port_no;		     // port no of 1394 I/F
    char  m_devicename[32];          // device file name
    raw1394handle_t m_handle;        // handle of 1394 I/F
    nodeid_t m_node_id;              // node_id of this node
    nodeaddr_t m_command_regs_base;  // base address of camera's cmd reg

    C1394CameraNode();
    virtual ~C1394CameraNode();

    char* GetModelName(char* buffer, size_t length);
    char* GetVenderName(char* buffer, size_t length);

    bool ReadReg(nodeaddr_t addr,quadlet_t* value);
    bool WriteReg(nodeaddr_t addr,quadlet_t* value);

    bool ResetToInitialState();
    bool PowerDown();
    bool PowerUp();  

    bool SetFeatureState(C1394CAMERA_FEATURE feat, C1394CAMERA_FSTATE state);
    bool GetFeatureState(C1394CAMERA_FEATURE feat, C1394CAMERA_FSTATE *state);

    bool SetParameter(C1394CAMERA_FEATURE feat, unsigned int value);
    bool GetParameter(C1394CAMERA_FEATURE feat, unsigned int* value);
    bool GetParameterRange(C1394CAMERA_FEATURE feat,
			   unsigned int *min,
			   unsigned int *max);

    bool SetAbsParameter(C1394CAMERA_FEATURE feat, float value);
    bool GetAbsParameter(C1394CAMERA_FEATURE feat, float* value);
    const char* GetAbsParameterUnit(C1394CAMERA_FEATURE feat);
    bool GetAbsParameterRange(C1394CAMERA_FEATURE feat, 
			      float* min,
			      float* max);

    const char* GetFeatureName(C1394CAMERA_FEATURE feat);
    const char* GetFeatureStateName(C1394CAMERA_FSTATE fstate);

    bool HasFeature(C1394CAMERA_FEATURE feat);
    bool HasCapability(C1394CAMERA_FEATURE feat, C1394CAMERA_FSTATE fstate);
    bool HasAbsControl(C1394CAMERA_FEATURE feat);

    bool EnableFeature(C1394CAMERA_FEATURE feat);
    bool DisableFeature(C1394CAMERA_FEATURE feat);
    bool AutoModeOn(C1394CAMERA_FEATURE feat);
    bool AutoModeOff(C1394CAMERA_FEATURE feat);
    bool OnePush(C1394CAMERA_FEATURE feat);

    uint64_t GetID(void) const;

    bool PreSet_All(void);
    bool AutoModeOn_All(void);
    bool AutoModeOff_All(void);
    bool OnePush_All(void);

    bool SetTriggerOn();
    bool SetTriggerOff();

    bool SetTriggerMode(int mode);
    bool QueryTriggerMode(int *mode);

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
    struct libcam1394_driver* driver;

    char *m_lpFrameBuffer;
    PIXEL_FORMAT m_pixel_format;   // the format of the buffered image 
    int  m_Image_W;                // image width (in pixels)
    int  m_Image_H;                // image height (in pixels)
    int  m_packet_sz;              // the packet size per a image
    int  m_num_packet;             // the number of packets per a image

    bool  m_bIsInitalized; // true means this instance has been initalized

    int  m_remove_header;
public:

    //! buffer option. \sa UpdateFrameBuffer()
    //
    //              LAST   AS_FIFO    WAIT_NEW_FRAME
    // blocking      NO      YES       YES
    // frame drop    YES     NO        YES
    //
    enum BUFFER_OPTION {
	BUFFER_DEFAULT    = 0 ,
	LAST              ,   //!< read last captured image.
	AS_FIFO           ,   //!< read first captured image. 
	WAIT_NEW_FRAME    ,   //!< block till new frame will be caputerd.
    };
    //! file type. \sa SaveToFile()
    enum FILE_TYPE {
	FILETYPE_PPM =0x000,  //!< ppm file.
	FILETYPE_PNM =0x001,  //!< pnm file.
	FILETYPE_PGM =0x002,  //!< pgm file.
	FILETYPE_JPG =0x003,  //!< jpg file.
    };

public:
    int  AllocateFrameBuffer(int       channel = -1         ,
			     FORMAT    fmt     = Format_X   ,
			     VMODE     mode    = Mode_X     ,
			     FRAMERATE rate    = FrameRate_X);
  
    int    GetFrameCount(int*);
    int    SetFrameCount(int);
    void*  UpdateFrameBuffer(BUFFER_OPTION opt=BUFFER_DEFAULT,
			     BufferInfo* info=0);
    int    GetFrameBufferSize();
    int    GetImageWidth();
    int    GetImageHeight();
  
    int    CopyRGBAImage(void* dest);
    int    CopyIplImage(IplImage* dest);
    int    CopyIplImageGray(IplImage* dest);

    int    SaveToFile(char* filename,FILE_TYPE type=FILETYPE_PPM); 

protected:
    int    AllocateBuffer(); 
    int    ReleaseBuffer();

    int  m_num_frame; // number of frames in frame buffer.
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
// those functions are obsolated since libcam1394-0.2.3
//int SetFrameCounter(int fd,int counter);
//int GetFrameCounter(int fd,int* ounter);

int GetPacketSize(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetNumPackets(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetImageWidth(FORMAT fmt,VMODE mode);
int GetImageHeight(FORMAT fmt,VMODE mode);
SPD GetRequiredSpeed(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
const char* GetVideoFormatString(FORMAT fmt,VMODE mode);
PIXEL_FORMAT GetPixelFormat(FORMAT fmt, VMODE mode);
const char* GetSpeedString(SPD rate);

typedef std::list<C1394CameraNode> CCameraList; //!< camera list
bool GetCameraList(raw1394handle_t,CCameraList *);
CCameraList::iterator find_camera_by_id(CCameraList& CameraList,uint64_t id);

void libcam1394_set_debug_level(int level);
const char *libcam1394_get_version(void);

#endif // #if !defined(_1394cam_h_included_)
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
