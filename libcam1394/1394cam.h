/*!
  @file  1394cam.h
  @brief 1394-based Digital Camera control class
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: 1394cam.h,v 1.2 2002-06-09 08:29:16 yosimoto Exp $
 */

#if !defined(_1394cam_h_included_)
#define _1394cam_h_included_

#include <list>
#include <netinet/in.h>
#include <libcam1394/1394cam_registers.h>

#if !defined IPLAPI
struct IplImage;
#endif // #if defined IPLAPI


//! pixel format codes
enum PIXEL_FORMAT {
    VFMT_YUV444  =0, 
    VFMT_YUV422  =1,
    VFMT_YUV411  =2,
    VFMT_RGB888  =3,
    VFMT_Y8      =4,
    
    VFMT_NOT_SUPPORTED ,
};

//! video format codes. This library supports only Format_0 now.
enum FORMAT {
    Format_0 = 0, //!< VGA non-compressed format 
    Format_1 = 1, //!< Super VGA non-compressed format(1)
    Format_2 = 2, //!< Super VGA non-compressed format(2)
    Format_3 = 3, //!< reserved for other format
    Format_4 = 4, //!< reserved for other format
    Format_5 = 5, //!< reserved for other format
    Format_6 = 6, //!< Still Image Format
    Format_7 = 7, //!< Scalable Image Format

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
    TRIGGER            ,      //!< triger control

    // reserved for other FEATURE_HI

    ZOOM           = 32,      //!< zoom control
    PAN                ,      //!< pan control
    TILT               ,      //!< tilt control
    OPTICAL_FILTER     ,      //!< optical filter control 

    // reserved for other FEATURE_LO

    CAPTURE_SIZE   = 48,      //!< capture size control
    CAPTURE_QUALITY    ,      //!< capture quality control
};

//-------------------------------------------------------

//! information of the node
class C1394Node {
public:
    unsigned int m_VenderID;  //!< vender id
    uint64_t     m_ChipID;    //!< chip id
};

//! frame buffer parameters
struct BufferInfo {
    //! @todo  !!FIXME!! -- not implemented yet
};

//! class C1394CameraNode 
class C1394CameraNode : public C1394Node {
private:
    enum {
	MAX_COUNT_NUMBER=0xffffffff,
    };
protected:

    unsigned is_format6:1;           //!< '1':format_6 /  '0':other format
//  FORMAT m_Format;
//  VMODE   m_mode;
//  FRAMERATE m_rate;
    int   m_channel;                 //!< -1 or "isochronus channel"
    int   m_iso_speed;               //!< isochronus speed

    char* m_lpModelName;             //!< camera name
    char* m_lpVecderName;            //!< camera vender name

    nodeaddr_t CMD(nodeaddr_t reg) ;

public:

    raw1394handle_t m_handle;        //!< handle of 1394 I/F
    nodeid_t m_node_id;              //!< node_id of this node
    nodeaddr_t m_command_regs_base;  //!< base address of camera's cmd reg

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
    int fd;                        //!< file descriptor of the driver
    char *pMaped;                  //!< pointer to the mmaped buffer

    PIXEL_FORMAT m_pixel_format;   //!< the format of the buffered image 
    int   m_BufferSize;            //!< size of buffer
    char*  m_lpFrameBuffer;        //!< pointer to the latest updated image
    int  m_Image_W;                //!< image width (in pixels)
    int  m_Image_H;                //!< image height (in pixels)
    int  m_packet_sz;              //!< the packet size per a image
    int  m_num_packet;             //!< the number of packets per a image
  
    bool  m_bIsInitalized; //!< true means this instance has been initalized

public:
    int m_last_read_frame;    //!< the counter of the last read frame number.

    enum BUFFER_OPTION {
	BUFFER_DEFAULT    = 0 ,
	LAST              ,   //!< read last  captured image.
	AS_FIFO           ,   //!< read first captured image.   
	WAIT_NEW_FRAME    ,   //!< block till new frame will be caputerd.
    };
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
    void*  UpDateFrameBuffer(BUFFER_OPTION opt=BUFFER_DEFAULT,
			     BufferInfo* info=0);
    int    GetFrameBufferSize();
    int    GetImageWidth();
    int    GetImageHeight();
  
    int    CopyRGBAImage(void* dest);
    int    CopyIplImage(IplImage* dest);

    int    SaveToFile(char* filename,FILE_TYPE type=FILETYPE_PPM); 

protected:
    int    AllocateBuffer(); 
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

int GetPacketSize(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetNumPackets(FORMAT fmt,VMODE mode,FRAMERATE frame_rate);
int GetImageWidth(FORMAT fmt,VMODE mode);
int GetImageHeight(FORMAT fmt,VMODE mode);
const char* GetVideoFormatString(FORMAT fmt,VMODE mode);
const char* GetSpeedString(SPD rate);

typedef list<C1394CameraNode> CCameraList; //!< camera list
bool GetCameraList(raw1394handle_t,CCameraList *);
CCameraList::iterator find_camera_by_id(CCameraList& CameraList,uint64_t id);

#endif // #if !defined(_1394cam_h_included_)
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
