/**
 * @file    1394cam.cc
 * @brief   1394-based Digital Camera control class
 * @date    Sat Dec 11 07:01:01 1999
 * @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
 * @version $Id: 1394cam.cc,v 1.25 2003-12-02 15:34:31 yosimoto Exp $
 */

// Copyright (C) 1999-2003 by YOSHIMOTO Hiromasa
//
// class C1394Node
//  +- class C1394CameraNode
//
// 1394-based Digital Camera Specification (Version1.30)
//
// Sat Dec 11 07:01:01 1999 by hiromasa yoshimoto 
// Sun Jun  9 16:43:47 2002 by YOSHIMOTO Hiromasa 

#include "config.h"
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
#include <linux/ohci1394_iso.h>

#if defined HAVE_CV_H
#include <cv.h>
#endif
#if defined HAVE_OPENCV_CV_H
#include <opencv/cv.h>
#endif

#include "common.h"
#include "1394cam_registers.h"
#include "1394cam.h"
#include "yuv.h"

using namespace std;

#define CHK_PARAM(exp) {if (!(exp)) MSG( "illegal param passed. " << __STRING(exp)); }
#define EXCEPT_FOR_FORMAT_6_ONLY  {if (is_format6) return false;}

#define Addr(name) (m_command_regs_base+OFFSET_##name)
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

#define WAIT usleep(10000)

// string-table for feature codes
static const char *feature_hi_table[]={
    "brightness" ,   // +0
    "auto_exposure" ,   
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
    "trigger"         ,  //+12

    "reserved","reserved","reserved","reserved","reserved",
    "reserved","reserved","reserved","reserved","reserved",
    "reserved","reserved","reserved","reserved","reserved",
    "reserved","reserved","reserved","reserved",
    
    "zoom", // +32
    "pan",
    "tilt",
    "optical_filter", // +35

    "reserved","reserved","reserved","reserved","reserved",
    "reserved","reserved","reserved","reserved","reserved",
    "reserved","reserved","reserved",
    
    "capture_size",  // +48
    "capture_quality",
    NULL
};

static const char *featurestate_table[]=
{
  "off",
  "auto",
  "manual",
  "one_push",
  NULL
};

/*
 * callback function for enumerating each node.
 * 
 * @param handle 
 * @param node_id 
 * @param pNode 
 * @param arg 
 * 
 * @return   true if the node is 1394-based camera.
 */
static int 
callback_1394Camera(raw1394_handle* handle, nodeid_t node_id,
		    C1394CameraNode* pNode, void* arg)
{
    int port_no = (int)arg;

    nodeaddr_t addr;
    quadlet_t tmp;
    addr=ADDR_INDIRECT_OFFSET;
    
    WAIT;    
    TRY( raw1394_read(handle, node_id,
		      addr, 4, &tmp) );
    tmp=ntohl(tmp);
    // LOG(VAR32(indirect_offset));
    if (!EQU(tmp,0xff000000,0x8d000000)){
	return false;
    }
    tmp &= ~0x8d000000;
    tmp *= 4;
    addr = ADDR_INDIRECT_OFFSET+tmp+4;
    //  LOG(VAR32(addr));
    quadlet_t chip_id_hi, chip_id_lo;

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

    pNode->m_VenderID = chip_id_hi>>8;
    pNode->m_ChipID   = (((uint64_t)chip_id_hi&0xff)<<32)+(uint64_t)chip_id_lo;

  
    addr=ADDR_UNIT_DIRECTORY_OFFSET;   /* addr <= the addr of RootDirectory */
    
    WAIT;    
    /* getting RootDirectory.unit_directory_offset  */
    TRY( raw1394_read(handle, node_id,
		      addr,sizeof(tmp),&tmp));
    tmp=ntohl(tmp);
    if (0xd1000000!=(tmp&0xff000000)){
	return false;
    }
    addr+=(tmp&~0xff000000)*4;        /* addr <= the addr of Unit directory  */
    
    WAIT;    
    /* getting Unit Directory.unit_dependent_directory_offset  */
    addr+=OFFSET_UNIT_DEPENDENT_DIRECTORY_OFFSET;
    TRY( raw1394_read(handle, node_id,
		      addr,sizeof(tmp),&tmp));
    tmp=ntohl(tmp);
    if (0xD4000000!=(tmp&0xff000000)){
	return false;
    }
    addr+=(tmp&~0xff000000)*4;        /* addr <= the top of Unit directory  */
    
    WAIT;    
    /* getting Unit Dependent Directory.command_regs_base  */
    addr+=OFFSET_COMMAND_REGS_BASE;
    TRY( raw1394_read(handle, node_id,
		      addr,sizeof(tmp),&tmp));
    tmp=ntohl(tmp);
    if (0x40000000!=(tmp&0xff000000)){
	return false;
    }
    /* getting command_regs_base  */

    // store the infomation of this camera-device.
    pNode->m_handle  = handle;
    pNode->m_node_id = node_id;
    pNode->m_command_regs_base = CSR_REGISTER_BASE+(tmp&~0xff000000)*4;
    pNode->m_port_no = port_no;
    return true;
}

/**
 *  find camera by camera-id .
 * 
 * @param CameraList 
 * @param id 
 * 
 * @return  iterator of the found camera,  or  CCameraList::end().
 */
CCameraList::iterator
find_camera_by_id(CCameraList& CameraList, uint64_t id)
{
    CCameraList::iterator ite;
    for (ite=CameraList.begin();ite!=CameraList.end();ite++){
	if ((*ite).GetID() == id){
	    return ite;
	}
    } 
    return CameraList.end();
}


/* 
 * @fn  enumrate 1394 nodes
 * 
 * @param handle      
 * @param pPort
 * @param pList       
 * @param func        pointer to the function 
 * 
 * @return returns zero on success, or -1 if an error occurred.
 */
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



/** 
 * retrieve  the list of cameras.
 * 
 * @param handle 
 * @param pList 
 * 
 * @return returns true on success, or false if an error occurred.
 */
bool
GetCameraList(raw1394handle_t handle, CCameraList* pList)
{
    int  numcards;
    const int NUM_PORT = 16;  /*  port   */
    struct raw1394_portinfo portinfo[NUM_PORT];
  

    numcards = raw1394_get_port_info(handle, portinfo, NUM_PORT);
    if (numcards < 0) {
	perror("couldn't get card info");
	return false;
    } else {
	LOG(numcards << "card(s) found");
    }

    if (!numcards) {
	return false;
    }

    pList->clear();

    int i;
    for (i = 0; i< numcards; i++) {
	raw1394handle_t new_handle = raw1394_new_handle();

	if (raw1394_set_port(new_handle, i) < 0) {
	    perror("couldn't set port");
	    return false;
	}	
	int pre = pList->size();
	Enum1394Node(new_handle, &portinfo[i],
		     pList, callback_1394Camera, (void*)i);
	if ( pList->size() == pre )
	    raw1394_destroy_handle(new_handle);
    }
    return true;
}

// ------------------------------------------------------------

/**
 * @class C1394Node 1394cam.h
 * @brief container for 1394-node
 *
 */

/**
 * @class C1394CameraNode 1394cam.h
 * @brief container for 1394-based digital camera.
 *
 * 
 */

/**
 * @struct BufferInfo  1394cam.h
 * @brief  frame buffer parameters
 */ 

/** 
 * Read the configuratoin register directory.
 *
 * @param addr 
 * @param value 
 * 
 * @return True on succeed, or false otherwise.
 */
bool C1394CameraNode::ReadReg(nodeaddr_t addr,quadlet_t* value)
{
    WAIT;
    *value=0x12345678;
    TRY(raw1394_read(m_handle, m_node_id, addr, 4, value));
    *value=(quadlet_t)ntohl((unsigned long int)*value);    
    return true;
}
/** 
 * Write the configuratoin register directory.
 * 
 * @param addr 
 * @param value 
 * 
 * @return True on succeed, or false otherwise.
 */
bool C1394CameraNode::WriteReg(nodeaddr_t addr,quadlet_t* value)
{
    quadlet_t tmp=htonl(*value);
    WAIT;
    TRY(raw1394_write(m_handle, m_node_id, addr, 4, &tmp));
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
  m_last_read_frame=0;
}

C1394CameraNode::~C1394CameraNode()
{
#if defined(_WITH_ISO_FRAME_BUFFER_)
    m_BufferSize=0;
#endif //#if defined(_WITH_ISO_FRAME_BUFFER_)
}



/** 
 * Get model name.
 * 
 * @todo  FIXME, not implemented yet
 *
 * @param lpBuffer 
 * @param lpLength 
 * 
 * @return returns a pointer to the destination string lpBuffer.
 */
const char* 
GetModelName(char* lpBuffer,size_t* lpLength)
{
    return NULL;
}


/** 
 * Get vender name.
 * 
 * @todo  FIXME, not implemented yet
 *
 * @param lpBuffer 
 * @param lpLength 
 * 
 * @return returns a pointer to the destination string lpBuffer.
 */
const char* 
GetVenderName(char* lpBuffer,size_t* lpLength)
{
    return 0;
}


/** 
 * Get camera's id.
 * 
 * 
 * @return returns camera's id.
 */
uint64_t C1394CameraNode::GetID(void) const
{
    return ((uint64_t)m_VenderID<<40) | m_ChipID;
}

/** 
 * Re-set camera to initial (factory setting value) state.
 * 
 * @since   Dec 12 19:04:14 1999 by hiromasa yoshimoto 
 * 
 * @return  returns true on success, or false otherwise.
 */
bool
C1394CameraNode::ResetToInitialState()
{
    quadlet_t tmp;

    WriteReg((CSR_REGISTER_BASE + CSR_CONFIG_ROM)+CSR_STATE_SET, 
	     &tmp);
    tmp=SetParam(INITIALIZE,Initialize,1);
    LOG("reset camera..."<<tmp);
    WriteReg(Addr(INITIALIZE),&tmp);
    return true;
}

/** 
 * Power-down camera.
 *
 * @return returns true on success, or false otherwise.
 *
 * @note Some cameras such as sony's DFW-V500 seems not to support
 * these functions.
 * 
 * @since Sun Dec 12 19:08:52 1999
 */
bool
C1394CameraNode::PowerDown()
{
    quadlet_t tmp=SetParam(Camera_Power,,0);
    TRY( raw1394_write(m_handle, m_node_id, 
		       Addr(Camera_Power),4,
		       &tmp) );  
  
    return true;
}

/** 
 * Power-up camera.
 *
 * @note Some cameras seem not to support these functions.
 *
 * @return returns true on success, or false otherwise.
 */
bool
C1394CameraNode::PowerUp()
{
    quadlet_t tmp=SetParam(Camera_Power,,1);
    TRY( raw1394_write(m_handle, m_node_id, 
		       Addr(Camera_Power),4,
		       &tmp) );  
  
    return true;
}


//--------------------------------------------------------------------------


/** 
 * Checks the presence of feature.
 * 
 * @param feat 
 * 
 * @return Returns true if the camera has the given feature.
 */
bool 
C1394CameraNode::HasFeature(C1394CAMERA_FEATURE feat)
{
  quadlet_t tmp=0;
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  return (1 == GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp));
}

/** 
 * Checks whether the feature has capability for the state.
 * 
 * @note  Each feature can have four states in C1394CAMERA_FSTATE.
 * 
 * @param feat    feature
 * @param fstate  state
 * 
 * @return Returns true if the feature supports the given state.
 */
bool
C1394CameraNode::HasCapability(C1394CAMERA_FEATURE feat, 
			       C1394CAMERA_FSTATE fstate)
{
  quadlet_t inq=0;
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&inq);
  bool r=false;
 
  if (GetParam(BRIGHTNESS_INQ, Presence_Inq, inq)){
    switch (fstate){
    case OFF:
      r = (0!=GetParam(BRIGHTNESS_INQ,On_Off_Inq,inq));
      break;
    case MANUAL:
      r = (0!=GetParam(BRIGHTNESS_INQ,Manual_Inq,inq));
      break;
    case AUTO:
      r = (0!=GetParam(BRIGHTNESS_INQ,Auto_Inq,inq));
      break;
    case ONE_PUSH:
      r = (0!=GetParam(BRIGHTNESS_INQ,One_Push_Inq,inq));
      break;
    }
  }

  return r;
}

/** 
 * Sets state of feature .
 * 
 * @param feat 
 * @param fstate 
 * 
 * @return True on success.
 */
bool C1394CameraNode::SetFeatureState(C1394CAMERA_FEATURE feat, 
				      C1394CAMERA_FSTATE  fstate)
{
  bool r=false;
  quadlet_t tmp=0;
  quadlet_t inq=0;

  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&inq);
  if ( 0==GetParam(BRIGHTNESS_INQ,Presence_Inq,inq) )
    return false;

  ReadReg(Addr(BRIGHTNESS)+4*feat,&tmp);
  switch (fstate){
  case OFF:
    if (GetParam(BRIGHTNESS_INQ,On_Off_Inq,inq)){      
      tmp |=  SetParam(BRIGHTNESS,Presence_Inq,1);
      tmp &= ~SetParam(BRIGHTNESS,ON_OFF,1);  
      WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);
      r = true;
    }
    break;
  case MANUAL:
    if (GetParam(BRIGHTNESS_INQ,Manual_Inq,inq)){      
      ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
      tmp |=  SetParam(BRIGHTNESS,Presence_Inq,1);
      tmp |=  SetParam(BRIGHTNESS,ON_OFF,1);  
      tmp &= ~SetParam(BRIGHTNESS,A_M_Mode,1);    
      WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
      r = true;
    }
    break;
  case AUTO:
    if (GetParam(BRIGHTNESS_INQ,Auto_Inq,inq)){      
      ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
      tmp |=  SetParam(BRIGHTNESS,Presence_Inq,1);
      tmp |=  SetParam(BRIGHTNESS,ON_OFF,1);  
      tmp |=  SetParam(BRIGHTNESS,A_M_Mode,1);    
      WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
      r = true;
    }
    break;
  case ONE_PUSH:
    if (GetParam(BRIGHTNESS_INQ,One_Push_Inq,inq)){      
      ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
      tmp |=  SetParam(BRIGHTNESS,Presence_Inq,1);
      tmp |=  SetParam(BRIGHTNESS,One_Push, 1);  
      tmp |=  SetParam(BRIGHTNESS,ON_OFF,1);  
      tmp &=  ~SetParam(BRIGHTNESS,A_M_Mode,1);    
      WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
      r = true;
    }
    break;
  }
  return r;
}

/** 
 * Retrieves the current state of the feature.
 * 
 * @param feat      a C1394CAMERA_FEATURE
 * @param fstate    pointer to store the state.
 * 
 * @return True if the state was stored in fstate, false otherwise.
 */
bool C1394CameraNode::GetFeatureState(C1394CAMERA_FEATURE feat, 
				      C1394CAMERA_FSTATE  *fstate)
{
    quadlet_t tmp=0;

    if (!fstate)
	return false;
  
    // we must check not Inquiry register but status register.
    ReadReg(Addr(BRIGHTNESS)+4*feat,&tmp);

    if (0==GetParam(BRIGHTNESS,Presence_Inq,tmp) ||
	0==GetParam(BRIGHTNESS,ON_OFF,tmp)){
	*fstate = OFF;
    } else if (1==GetParam(BRIGHTNESS,A_M_Mode,tmp)){
	*fstate = AUTO;
    } else if (1==GetParam(BRIGHTNESS,One_Push,tmp)){
	*fstate = ONE_PUSH;
    } else {	
	*fstate = MANUAL;
    }

    return true;
}

/** 
 * Gets the name for the given feature.
 * 
 * @param   feat a C1394CAMERA_FEATURE
 * 
 * @return  pointer to the string for the given feature, or NULL.
 */
const char*
C1394CameraNode::GetFeatureName(C1394CAMERA_FEATURE feat)
{
    if (0<=feat && feat < END_OF_FEATURE)
	return feature_hi_table[feat];
    else
	return NULL;
}

/** 
 * Gets the name for the given state.
 * 
 * @param fstate a C1394CAMERA_FSTATE
 * 
 * @return pointer to the string for the given feature, or NULL.
 */
const char* 
C1394CameraNode::GetFeatureStateName(C1394CAMERA_FSTATE fstate)
{
    if (OFF<= fstate && fstate < END_OF_FSTATE)
	return featurestate_table[fstate];
    else
	return NULL;
}

/** 
 * Sets the parameter of the camera.
 *
 * @note After this method succeed, the camera state will be set to
 * the manual state.
 * 
 * @param feat     a C1394CAMERA_FEATURE to write 
 * @param value    value
 * 
 * @return True on succeed, or false otherwise.
 */
bool
C1394CameraNode::SetParameter(C1394CAMERA_FEATURE feat,unsigned int value)
{
  quadlet_t tmp;
    
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);

//  ERR( "tmp:" <<  hex << tmp << dec <<(int)feat );
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available.");
    return false;
  }
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
  
  tmp = 0;
  tmp |= SetParam(BRIGHTNESS,Presence_Inq, 1);
  tmp |= SetParam(BRIGHTNESS,ON_OFF,1);
  
  tmp |= SetParam(BRIGHTNESS,Value,value);
  WriteReg(Addr(BRIGHTNESS)+4*feat,&tmp);
    
  return true;
}

/** 
 * Retrieves the current parameter of feature.
 * 
 * @param feat   a C1394CAMERA_FEATURE to read
 * @param value  pointer to stored the parameter.
 * 
 * @return True if the parameter was stored in fstate, or false otherwise.
 */
bool
C1394CameraNode::GetParameter(C1394CAMERA_FEATURE feat,unsigned int *value)
{
  quadlet_t tmp=0;
  
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (!GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
    ERR("the feature "<<feature_hi_table[feat]<<" is not available");
    return false;
  }
  if (!GetParam(BRIGHTNESS_INQ,ReadOut_Inq,tmp)){
    ERR("the value of feature "<<feature_hi_table[feat]<<" is invalid");
    return false;
  }
  
  ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  *value=tmp&((1<<24)-1);////GetParam(BRIGHTNESS,Value,tmp);
  return true;
}

/** 
 * Disables feature. 
 * 
 * @note  This functions is synonym for SetFeatureState(feat, OFF).
 *
 * @param feat 
 * 
 * @return True on success.
 */
bool
C1394CameraNode::DisableFeature(C1394CAMERA_FEATURE feat)
{  
  return SetFeatureState(feat, OFF);
}

/** 
 * Enable feature.
 *
 * @note After this function succeed, the state of feature will be set to 
 * AUTO, MANUAL, or OnePush. It's depend on the previous feature's
 * state. 
 * 
 * @param feat 
 *
 * @return True on success.
 *
 * @sa SetFeatureState().
 */
bool
C1394CameraNode::EnableFeature(C1394CAMERA_FEATURE feat)
{
    quadlet_t tmp=0;
    quadlet_t inq=0;

    ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&inq);
    if (0==GetParam(BRIGHTNESS_INQ,Presence_Inq,inq)){
	ERR("the feature "<<feature_hi_table[feat]<<" is not available");
	return false;
    }
    if (0==GetParam(BRIGHTNESS_INQ,On_Off_Inq,inq)){
	ERR("the feature "<<feature_hi_table[feat]<<" has no ON/OFF cap.");
	return false;
    }
    
    ReadReg(Addr(BRIGHTNESS)+4*feat,&tmp);
    tmp |= SetParam(BRIGHTNESS,ON_OFF,1); 
    WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);

    return true;
}

/** 
 * Sets the feature to the onepush state.
 *
 * @note This functions is synonym for SetFeatureState(feat, OnePush)
 * 
 * @param feat 
 * 
 * @return True on success.
 */
bool
C1394CameraNode::OnePush(C1394CAMERA_FEATURE feat)
{
  return SetFeatureState(feat, ONE_PUSH);
}

/** 
 * Sets auto-mode the feature.
 * 
 * @note this function is obsolete. please use SetFeatureState(feat,
 * AUTO)
 *
 * @param feat 
 *
 * @return True on success. 
 */
bool
C1394CameraNode::AutoModeOn(C1394CAMERA_FEATURE feat)
{
  return SetFeatureState(feat, AUTO);
}

/** 
 * Sets the camera feature to Manual control state.
 *
 * @note this function is obsolete. please use SetFeatureState(feat,
 * MANUAL)
 *
 * @param feat 
 *
 * @return True on success.
 */
bool
C1394CameraNode::AutoModeOff(C1394CAMERA_FEATURE feat)
{
  return SetFeatureState(feat, MANUAL);
}


/** 
 * Sets the camera paramter to factory defaults.
 * 
 * 
 * @return True on success.
 */
bool
C1394CameraNode::PreSet_All()
{
  WriteReg(Addr(Cur_Mem_Ch), 0 );
  return true;
}

/**
 * Sets all feature to auto state.
 *
 * @note  This function is obsolete.
 *
 * @since Sun Dec 12 20:10:33 1999 by hiromasa yoshimoto 
 * 
 * @return True on success.
 */
bool 
C1394CameraNode::AutoModeOn_All()
{
  C1394CAMERA_FEATURE feat;
  for (feat=BRIGHTNESS;feat<END_OF_FEATURE;
       feat=(C1394CAMERA_FEATURE)((int)feat+1)){
    SetFeatureState(feat, AUTO);
  }
  return true;
}

/** 
 * Sets all feature to manual state.
 * 
 * @note This function is obsolete.
 * 
 * @return True on success.
 */
bool 
C1394CameraNode::AutoModeOff_All()
{
  C1394CAMERA_FEATURE feat;
  for (feat=BRIGHTNESS;feat<END_OF_FEATURE;
       feat=(C1394CAMERA_FEATURE)((int)feat+1)){
    SetFeatureState(feat, MANUAL);
  }
  return true;
}

/** 
 * Sets all feature to onepush state.
 *
 * @note  This function is obsolete.
 *
 * @return True on success.
 */
bool 
C1394CameraNode::OnePush_All()
{
  C1394CAMERA_FEATURE feat;
  for (feat=BRIGHTNESS;feat<END_OF_FEATURE;
       feat=(C1394CAMERA_FEATURE)((int)feat+1)){
    SetFeatureState(feat, ONE_PUSH);
  }
  
    return true;
}

//--------------------------------------------------------------------------

/** 
 * Sets the format, mode, frame rate.
 * 
 * @param fmt          a FORMAT
 * @param mode         a VMODE
 * @param frame_rate   a FRAMERATE
 * 
 * @return True on success.
 *
 * @note  This library supports FORMAT_0, FORMAT_1, FORMAT_2 only.
 */
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
    }
    if (mode!=Mode_X){
	tmp=SetParam(Cur_V_Mode,,mode); 
	WriteReg(Addr(Cur_V_Mode),&tmp);	
    }
    if (frame_rate!=FrameRate_X){
	tmp=SetParam(Cur_V_Frm_Rate,,frame_rate);
	WriteReg(Addr(Cur_V_Frm_Rate),&tmp);
	
    }

    FORMAT f; VMODE m; FRAMERATE r;
    SPD cur_speed, req_speed;
    QueryIsoSpeed(&cur_speed);
    QueryFormat(&f,&m,&r);
    req_speed=GetRequiredSpeed(f,m,r);
    if (req_speed > cur_speed)
	SetIsoSpeed(req_speed);	    

    return true;  
}

/** 
 * Retrieves the format, mode, frame_rate of camera.
 * 
 * @param fmt         pointer to store format, or NULL        
 * @param mode        pointer to store mode, or NULL        
 * @param frame_rate  pointer to store frame rate, or NULL        
 * 
 * @return True on success.
 */
bool
C1394CameraNode::QueryFormat(FORMAT*    fmt,
			     VMODE*      mode,
			     FRAMERATE* frame_rate)
{
    quadlet_t tmp;

    if (fmt){
	ReadReg(Addr(Cur_V_Format),&tmp);
	*fmt=(FORMAT)GetParam(Cur_V_Format,,tmp);
    }
    if (mode){
	ReadReg(Addr(Cur_V_Mode),&tmp);
	*mode=(VMODE)GetParam(Cur_V_Mode,,tmp); 
    }
    if (frame_rate){
	ReadReg(Addr(Cur_V_Frm_Rate),&tmp);
	*frame_rate=(FRAMERATE)GetParam(Cur_V_Frm_Rate,,tmp);
    }
    return true;  
}

/** 
 * Sets isochronus channel to use
 * 
 * @param channel  an integer from 0 to 15.
 * 
 * @return True on success.
 */
bool
C1394CameraNode::SetIsoChannel(int channel)
{
    EXCEPT_FOR_FORMAT_6_ONLY;
    quadlet_t tmp;
    ReadReg(Addr(ISO_Channel), &tmp);
    tmp &= ~SetParam(ISO_Channel,,0xfffff); // clear field
    tmp |=  SetParam(ISO_Channel,,channel); // set field
    WriteReg(Addr(ISO_Channel),&tmp);
    m_channel = channel;
    return true;  
}

/**
 * Retrieves the camera setting and store in this class.
 * 
 * 
 * @return True
 */
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

/** 
 * Gets current isochronus channel
 * 
 * @param channel  pointer to store the current channel. 
 * 
 * @return True on success.
 */
bool
C1394CameraNode::QueryIsoChannel(int* channel)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  CHK_PARAM(channel!=NULL);
  quadlet_t tmp;
   
  ReadReg(Addr(ISO_Speed), &tmp);    
  WAIT;
  m_channel= GetParam(ISO_Channel,,tmp);
  *channel = m_channel;
  return true;
}

/** 
 * Gets current isochronus speed
 * 
 * @param spd  pointer ot store the current speed.
 * 
 * @return True on success
 */
bool  
C1394CameraNode::QueryIsoSpeed(SPD* spd)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  CHK_PARAM(spd!=NULL);
  quadlet_t tmp;
   
  ReadReg(Addr(ISO_Speed),&tmp);  
  *spd=(SPD)GetParam(ISO_Speed,,tmp);
  return true;
}

/** 
 * Sets  isochronus speed
 * 
 * @param iso_speed  a SPD
 * 
 * @return True on success
 */
bool
C1394CameraNode::SetIsoSpeed(SPD iso_speed)
{
  EXCEPT_FOR_FORMAT_6_ONLY;
  quadlet_t tmp;
  ReadReg(Addr(ISO_Speed), &tmp); 
  tmp &= ~SetParam(ISO_Speed,,0xfffff);    // clear field
  tmp |=  SetParam(ISO_Speed,,iso_speed);  // set field
  WriteReg(Addr(ISO_Speed),&tmp); 
  m_iso_speed = iso_speed;
  return true;
}

/** 
 * Starts camera to send an image.
 * 
 * 
 * @return True on success.
 */
bool 
C1394CameraNode::OneShot()
{
    quadlet_t tmp=SetParam(One_Shot,,1);
    WriteReg(Addr(One_Shot),&tmp) ;  
    return true;
}

/** 
 * Starts camera to send images continuously.
 * 
 * @param count_number      sending only count_number frames.
 * 
 * @return True on success.
 */
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
	
    }else{
	quadlet_t tmp;
	tmp =SetParam(Multi_Shot,,1);
	tmp|=SetParam(Count_Number,,count_number);
	WriteReg(
	    Addr(Multi_Shot),
	    &tmp);
	
    }
    return true;
}

/** 
 * Sets  external trigger mode.
 * 
 * @todo Currentry this function supports only trigger mode0. (But You
 * can change trigger mode with SetParameter().)
 * 
 * @return True.
 */
bool
C1394CameraNode:: SetTriggerOn()
{
    quadlet_t tmp;
    tmp=0x82000000;
    WriteReg(Addr(TRIGGER_MODE),&tmp);
    return true;
}

/** 
 * Sets internal trigger mode.
 * 
 * 
 * @return True.
 */
bool
C1394CameraNode:: SetTriggerOff()
{
    LOG("SetTriggerOff");
    quadlet_t tmp;
    tmp=0x80000000;
    WriteReg(Addr(TRIGGER_MODE),&tmp);     
    return true;
}


/** 
 * Stops to send images.
 * 
 * 
 * @return True.
 */
bool
C1394CameraNode:: StopIsoTx()
{
    EXCEPT_FOR_FORMAT_6_ONLY;
    quadlet_t tmp;
    tmp=SetParam(ISO_EN,,0);
    WriteReg(Addr(ISO_EN),&tmp);
    
    return true;
}


/*
 * pixel information.  bit weight
 *
 */
struct VideoPixelInfo{
    char* name;			/**< pixel format name */

    int weight0;		/**< 1st component weight */
    int weight1;		/**< 2nd component weight */
    int weight2;		/**< 3rd component weight */
};

/*
 *
 *
 */
struct VideoImageInfo{
    PIXEL_FORMAT pixel_format;	/**< pixel format */ 
    int  w;			/**< width  */
    int  h;			/**< height */
};

/*
 * packet format table.
 * 
 * see IIDC 1394-based Digital Camera Sepec (2.1.2 Video mode comparison chart)
 *
 */
struct VideoPacketInfo{
    int packet_sz;		/**< size of packet per a packet.    */
    int num_packets;		/**< number of packets per an image. */
    SPD required_speed;		/**< required bus speed.             */
};

static struct VideoPixelInfo video_pixel_info[]=
{
    {"YUV(4:4:4)",  4, 4, 4   }, // VFMT_YUV444
    {"YUV(4:2:2)",  4, 2, 2   }, // VFMT_VUV422
    {"YUV(4:1:1)",  4, 1, 1   }, // VFMT_VUV411
    {"RGB(8:8:8)",  8, 8, 8   }, // VFMT_RGB888
    {" Y (Mono) ",  8, 0, 0   }, // VFMT_Y8
    {" Y(Mono16)", 16, 0, 0   }, // VFMT_Y16
    {" unknown  ", -1,-1,-1   }
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
	{    VFMT_Y16   ,       640,  480}, // format_0/mode_6
	RESERVED,                           // format_0/mode_7
    },
    // format_1
    {
	{   VFMT_YUV422,        800, 600}, // format_1/mode_0
	{   VFMT_RGB888,        800, 600}, // format_1/mode_1
	{   VFMT_Y8    ,        800, 600}, // format_1/mode_2
	{   VFMT_YUV422,       1024, 768}, // format_1/mode_3
	{   VFMT_RGB888,       1024, 768}, // format_1/mode_4
	{   VFMT_Y8    ,       1024, 768}, // format_1/mode_5	
	{   VFMT_Y16   ,        800, 600}, // format_1/mode_6
	{   VFMT_Y16   ,       1024, 768}, // format_1/mode_7
    },
    // format_2
    {
	RESERVED,
	RESERVED,
	RESERVED,
	RESERVED,
	RESERVED,
	RESERVED,
	RESERVED,
	RESERVED,
    },
};
#undef RESERVED


#define RESERVED  { -1, -1, SPD_100M}
static struct VideoPacketInfo video_packet_info[][8][6]= // format / mode / fps
{
    // format_0
    {
	{ // format_1 mode_0 
	    RESERVED,                     // 1.875fps
	    RESERVED,                     // 3.75 fps
	    {  15*4, 120*8, SPD_100M  },  // 7.5  fps
	    {  30*4, 120*4, SPD_100M  },  // 15   fps
	    {  60*4, 120*2, SPD_100M  },  // 30   fps
	    RESERVED,                     // 60   fps
	},
	{ // format_1 mode_1 
	    RESERVED,
	    {  20*4, 240*8 ,SPD_100M},
	    {  40*4, 240*4 ,SPD_100M},
	    {  80*4, 240*2 ,SPD_100M},
	    { 160*4, 240   ,SPD_100M},
	    RESERVED,
	},
	{ // format_1 mode_2
	    RESERVED,
	    {  60*4, 480*4 ,SPD_100M},
	    { 120*4, 480*2 ,SPD_100M},
	    { 240*4, 480/1 ,SPD_100M},
	    { 480*4, 480/2 ,SPD_200M},
	    RESERVED,
	},
	{  // format_1 mode_3
	    RESERVED,
	    {  80*4, 480*4 ,SPD_100M},
	    { 160*4, 480*2 ,SPD_100M},
	    { 320*4, 480/1 ,SPD_200M},
	    { 640*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_1 mode_4
	    RESERVED,
	    { 120*4, 480*4 ,SPD_100M},
	    { 240*4, 480*2 ,SPD_100M},
	    { 480*4, 480*1 ,SPD_200M},
	    { 960*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_1 mode_5
	    RESERVED,
	    {  40*4, 480*4 ,SPD_100M},
	    {  80*4, 480*2 ,SPD_100M},
	    { 160*4, 480/1 ,SPD_100M},
	    { 320*4, 480/2 ,SPD_200M},
	    { 320*4, 480/4 ,SPD_400M},
	},  
	{  // format_1 mode_6
	    RESERVED,
	    {  80*4, 480*4 ,SPD_100M},
	    { 160*4, 480*2 ,SPD_100M},
	    { 320*4, 480/1 ,SPD_200M},
	    { 640*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_1 mode_7
	    RESERVED,
	    RESERVED,
	    RESERVED,
	    RESERVED,
	    RESERVED,
	    RESERVED,
	},  
    },

    // format_1
    {
	{ // format_1 mode_0
	    RESERVED,                      // 1.875fps
	    { 125*4, 600*16/5, SPD_100M }, // 3.75 fps
	    { 250*4, 600*8/5,  SPD_100M }, // 7.5  fps
	    { 500*4, 600*4/5,  SPD_200M }, // 15   fps
	    {1000*4, 600*2/5,  SPD_400M }, // 30   fps
	    RESERVED,                      // 60   fps
	},
	{ // format_1 mode_1
	    RESERVED,                      // 1.875fps
	    RESERVED,                      // 3.75 fps
	    { 375*4, 600*8/5,  SPD_200M }, // 7.5  fps
	    { 750*4, 600*4/5,  SPD_400M }, // 15   fps
	    RESERVED,                      // 30   fps
	    RESERVED,                      // 60   fps
	},
	{ // format_1 mode_2
	    RESERVED,
	    RESERVED,
	    { 125*4, 600* 8/5, SPD_100M }, // 7.5  fps
	    { 250*4, 600* 4/5, SPD_100M }, // 15   fps
	    { 500*4, 600* 2/5, SPD_200M }, // 30   fps	    
	    {1000*4, 600* 1/5, SPD_400M }, // 60   fps	    
	},
	{ // format_1 mode_3
	    {  96*4, 768*16/3, SPD_100M }, // 1.875fps
	    { 192*4, 768* 8/3, SPD_100M }, // 3.75 fps
	    { 384*4, 768* 4/3, SPD_200M }, // 7.5  fps
	    { 768*4, 768* 2/3, SPD_400M }, // 15   fps
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_1 mode_4
	    { 144*4, 768*16/3, SPD_100M }, // 1.875fps
	    { 288*4, 768* 8/3, SPD_200M }, // 3.75 fps
	    { 576*4, 768* 4/3, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_1 mode_5
	    {  48*4, 768*16/3, SPD_100M }, // 1.875fps
	    {  96*4, 768* 8/3, SPD_100M }, // 3.75 fps
	    { 192*4, 768* 4/3, SPD_100M }, // 7.5  fps
	    { 384*4, 768* 2/3, SPD_200M }, // 15   fps
	    { 768*4, 768* 1/3, SPD_400M }, // 30   fps
	    RESERVED,                      
	},
	{ // format_1 mode_6
	    RESERVED,                      // 1.875fps
	    { 125*4, 600*16/5, SPD_100M }, // 3.75 fps
	    { 250*4, 600*8/5,  SPD_100M }, // 7.5  fps
	    { 500*4, 600*4/5,  SPD_200M }, // 15   fps
	    {1000*4, 600*2/5,  SPD_400M }, // 30   fps
	    RESERVED,                      // 60   fps
	},
	{ // format_1 mode_7
	    {  96*4, 768*16/3, SPD_100M }, // 1.875fps
	    { 192*4, 768* 8/3, SPD_100M }, // 3.75 fps
	    { 384*4, 768* 4/3, SPD_200M }, // 7.5  fps
	    { 768*4, 768* 2/3, SPD_400M }, // 15   fps
	    RESERVED,                      
	    RESERVED,                      
	},
    },

    // format_2
    {
	{ // format_2 mode_0
	    { 160*4, 960* 4, SPD_100M }, // 1.875fps
	    { 320*4, 960* 2, SPD_200M }, // 3.75 fps
	    { 640*4, 960* 1, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_1
	    { 240*4, 960* 4, SPD_100M }, // 1.875fps
	    { 480*4, 960* 2, SPD_200M }, // 3.75 fps
	    { 960*4, 960* 1, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_2
	    {  80*4, 960* 4, SPD_100M }, // 1.875fps
	    { 160*4, 960* 2, SPD_100M }, // 3.75 fps
	    { 320*4, 960* 1, SPD_200M }, // 7.5  fps
	    { 640*4,960*1/2, SPD_400M }, // 15   fps
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_3
	    { 250*4,1200*16/5, SPD_100M }, // 1.875fps
	    { 500*4,1200* 8/5, SPD_200M }, // 3.75 fps
	    {1000*4,1200* 4/5, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_4
	    { 375*4,1200*16/5, SPD_200M }, // 1.875fps
	    { 750*4,1200* 8/5, SPD_400M }, // 3.75 fps
	    RESERVED,
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_5
	    { 125*4,1200*16/5, SPD_100M }, // 1.875fps
	    { 250*4,1200* 8/5, SPD_100M }, // 3.75 fps
	    { 500*4,1200* 4/5, SPD_200M }, // 7.5  fps
	    {1000*4,1200* 2/5, SPD_400M }, // 15   fps
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_6
	    { 160*4, 960* 4, SPD_100M }, // 1.875fps
	    { 320*4, 960* 2, SPD_200M }, // 3.75 fps
	    { 640*4, 960* 1, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                      
	},
	{ // format_2 mode_7
	    { 250*4,1200*16/5, SPD_100M }, // 1.875fps
	    { 500*4,1200* 8/5, SPD_200M }, // 3.75 fps
	    {1000*4,1200* 4/5, SPD_400M }, // 7.5  fps
	    RESERVED,
	    RESERVED,                      
	    RESERVED,                   
	},
    },
};
#undef RESERVED

/** 
 * Gets a string of the given video format.
 * 
 * @param fmt 
 * @param mode 
 * 
 * @return pointer to the string.
 */
const char* GetVideoFormatString(FORMAT fmt,VMODE mode)
{
    if (!(0<=fmt&&fmt<=2))
	return "unknown (or not supported) format";
  
    PIXEL_FORMAT pixel=video_image_info[fmt][mode].pixel_format;
    return video_pixel_info[ pixel ].name;
}

/** 
 * Getss a string of the given bus speed.
 * 
 * @param spd 
 * 
 * @return pointer to the string
 */
const char* GetSpeedString(SPD spd)
{
    static char* speed_string[]={
	"100Mbps",
	"200Mbps",
	"400Mbps",
    };
    if (0 <= spd && spd < (SPD)(sizeof(speed_string)/sizeof(char*)) )
	return speed_string[spd];
    else
	return "???Mbps";
}

/** 
 * Returns packet size for the given video format.
 * 
 * @param fmt 
 * @param mode 
 * @param frame_rate 
 * 
 * @return packet size (in bytes)
 */
int GetPacketSize(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
    return video_packet_info[fmt][mode][frame_rate].packet_sz;
}

/** 
 * Returns the number of packets per a image for the given video format.
 * 
 * @param fmt 
 * @param mode 
 * @param frame_rate 
 * 
 * @return number of packets (in bytes)
 */
int GetNumPackets(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
    return video_packet_info[fmt][mode][frame_rate].num_packets;
}

/** 
 * Returns width of image for the given video format.
 * 
 * @param fmt 
 * @param mode 
 * 
 * @return width of image
 */
int GetImageWidth(FORMAT fmt,VMODE mode)
{
    return video_image_info[fmt][mode].w;
}

/** 
 * Returns height of image for the given video format.
 * 
 * @param fmt 
 * @param mode 
 * 
 * @return height of image
 */
int GetImageHeight(FORMAT fmt,VMODE mode)
{
    return video_image_info[fmt][mode].h;
}

/** 
 * Returns bus speed for the given video format.
 * 
 * @param fmt 
 * @param mode 
 * @param frame_rate 
 * 
 * @return 
 */
SPD GetRequiredSpeed(FORMAT fmt,VMODE mode,FRAMERATE frame_rate)
{
    return video_packet_info[fmt][mode][frame_rate].required_speed;
}


/** 
 *
 * @note  This function is for backward compatibility.DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * 
 * @return 
 */
int EnableCyclemaster(raw1394handle_t handle)
{
/*  if (0!=ioctl(handle->fd,OHCI_ENABLE_CYCLEMASTER,NULL)){
    return false;
    }*/
    return true;
}

/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * 
 * @return 
 */
int DisableCyclemaster(raw1394handle_t handle)
{
/*  if (0!=ioctl(handle->fd,OHCI_DISABLE_CYCLEMASTER,NULL)){
    return false;
    }  */
    return true;
}

/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * @param channel
 * 
 * @return 
 */
int EnableIsoChannel(raw1394handle_t handle,int channel)
{
    return true;
}

/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * @param channel
 * 
 * @return 
 */
int DisableIsoChannel(raw1394handle_t handle,int channel)
{
    return true;
}


/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * @param channel 
 * @param spd 
 * 
 * @return 
 */
int AllocateIsoChannel(raw1394handle_t handle,
		   int channel,SPD spd)
{
    return true;
}

/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * @param channel 
 * 
 * @return 
 */
int ReleaseIsoChannel(raw1394handle_t handle,
		 int channel)
{
    return true;
}

/** 
 * @note This function is for backward compatibility. DO NOT USE THIS FUNCTION.
 * 
 * @param handle 
 * 
 * @return 
 */
int ReleaseIsoChannelAll(raw1394handle_t handle)
{
    return true;
}


/** 
 * Sets the counter of captured images.
 * 
 * @param fd        
 * @param counter 
 * 
 * @return  Zero on success.
 */
int SetFrameCounter(int fd, int counter)
{
    return  (0 != ioctl(fd, IOCTL_SET_COUNT, counter));
}

/** 
 * Gets count of captured images.
 * 
 * @param fd 
 * @param counter 
 * 
 * @return Zero on success. 
 */
int GetFrameCounter(int fd, int* counter)
{
    if (NULL==counter)
	return false;
    return  (0 != ioctl(fd, IOCTL_GET_COUNT, counter));
}


/** 
 * Allocate frame buffer memory for the given video format
 * 
 * @param channel 
 * @param fmt        a FORMAT
 * @param mode       a VMODE
 * @param rate       a FRAMERATE
 * 
 * @return Zero on success.
 */
int C1394CameraNode::AllocateFrameBuffer(int channel,
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

    m_packet_sz  = ::GetPacketSize(fmt,mode,rate);
    m_packet_sz += 8;
    m_num_packet = ::GetNumPackets(fmt,mode,rate);
    
    if ( m_num_packet < 0 ){
	cerr << "packet size is too big"<<endl;
	return -2;
    }

    char devname[1024];
    snprintf(devname, sizeof(devname), "/dev/isofb%d", this->m_port_no);
    fd=open(devname,O_RDWR);
    if (-1==fd){
	cerr << "can't open " << devname << " " << strerror(errno) << endl;
	return -1;
    }
  
    ISO1394_RxParam rxparam;
    TRY( ioctl(fd, IOCTL_INIT_RXPARAM, &rxparam) );
  
    rxparam.sz_packet      = m_packet_sz ;
    rxparam.num_packet     = m_num_packet ;
    rxparam.num_frames     = 4;
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
    m_lpFrameBuffer=pMaped;

    TRY( ioctl(fd, IOCTL_START_RX) );
    
    m_Image_W=::GetImageWidth(fmt,mode);
    m_Image_H=::GetImageHeight(fmt,mode);
    
    m_BufferSize=m_packet_sz*m_num_packet;
    m_pixel_format=video_image_info[fmt][mode].pixel_format;
 
    return 0;
}


/** 
 * Returns number of caputered frames.
 * 
 * @param count pointer to store the count.
 * 
 * @return Zero on success.
 */
int C1394CameraNode::GetFrameCount(int* count)
{
    return ::GetFrameCounter(fd,count);
}

/** 
 * Sets the number of caputered frame counter.
 * 
 * @param tmp 
 * 
 * @return Zero on success.
 */
int C1394CameraNode::SetFrameCount(int tmp)
{
    return ::SetFrameCounter(fd,tmp);
}

/** 
 * Retrieves  the pointer of caputered frame.
 *
 * This function waits until the frame buffer is updated, and returns
 * the pointer of the last captured image.
 * 
 * @param opt   a C1394CameraNode::BUFFER_OPTION
 * @param info  pointer to BufferInfo, or NULL.
 * 
 * @return ponter of latest caputered frame.
 */
void* C1394CameraNode::UpDateFrameBuffer(BUFFER_OPTION opt,BufferInfo* info)
{
    ISO1394_Chunk chunk;
    TRY( ioctl(fd, IOCTL_GET_RXBUF, &chunk) );
    return m_lpFrameBuffer=(pMaped+chunk.offset);
}

/** 
 * Gets the size fo frame buffer
 * 
 * 
 * @return size of frame buffer.
 */
int C1394CameraNode::GetFrameBufferSize()
{
    return m_BufferSize;
}

/** 
 * 
 * 
 * @return the width of image in bytes. 
 */
int C1394CameraNode::GetImageWidth()
{
    return m_Image_W;
}

/** 
 * 
 * 
 * @return the height of image in bytes.
 */
int C1394CameraNode::GetImageHeight()
{
    return m_Image_H;
}

/** 
 * Copies a caputured frame to IplImage buffer.
 *
 * This function converts the color-space if needed.
 *
 * @param dest  pointer to store the frame.
 * 
 * @return Zero on success, or -1 if you don't have IPL or OpenCV.
 *
 * @note This function uses LUT created by CreateYUVtoRGBAMap().
 * @note This function will be enabled if you have Intel's IPL or OpenCV.
 * 
 */
int C1394CameraNode::CopyIplImage(IplImage *dest)
{
#if !defined HAVE_CV_H && !defined HAVE_OPENCV_CV_H
    LOG("This system don't have ipl or OpenCV library.");
    return -1;
#else
    switch  ( m_pixel_format ){
    case VFMT_YUV422:
	::copy_YUV422toIplImage(dest,m_lpFrameBuffer,
				m_packet_sz,m_num_packet,
				REMOVE_HEADER);
    break;
    case VFMT_YUV411:
	::copy_YUV411toIplImage(dest,m_lpFrameBuffer,
				m_packet_sz,m_num_packet,
				REMOVE_HEADER);
    break;
    case VFMT_YUV444:
	::copy_YUV444toIplImage(dest,m_lpFrameBuffer,
				m_packet_sz,m_num_packet,
				REMOVE_HEADER);
    break;
    default:
	LOG("not support this pixel format yet.");
	break;
    }
  
    return 0;
#endif //#if !defined HAVE_CV_H && !defined HAVE_OPENCV_CV_H
}

/** 
 * Copies a caputured image to IplImage buffer.
 *
 * This function converts the image to gray-scaled one. When camera
 * uses YUV format, this function stores the Y component only.
 *
 * @param dest  pointer to store the frame.
 * 
 * @return Zero on success, or -1 if you don't have IPL or OpenCV.
 *
 * @note This function uses LUT created by CreateYUVtoRGBAMap().
 * @note This function will be enabled if you have Intel's IPL or OpenCV.
 * 
 */
int C1394CameraNode::CopyIplImageGray(IplImage *dest)
{
#if !defined HAVE_CV_H && !defined HAVE_OPENCV_CV_H
    LOG("This system don't have ipl or OpenCV library.");
    return -1;
#else
    switch  ( m_pixel_format ){
    case VFMT_YUV422:
	::copy_YUV422toIplImageGray(dest,m_lpFrameBuffer,
				    m_packet_sz,m_num_packet,
				    REMOVE_HEADER);
    break;
    case VFMT_YUV411:
	::copy_YUV411toIplImageGray(dest,m_lpFrameBuffer,
				    m_packet_sz,m_num_packet,
				    REMOVE_HEADER);
    break;
    case VFMT_YUV444:
	::copy_YUV444toIplImageGray(dest,m_lpFrameBuffer,
				    m_packet_sz,m_num_packet,
				    REMOVE_HEADER);
    break;
    default:
	LOG("not support this pixel format yet.");
	break;
    }
  
    return 0;
#endif //#if !defined HAVE_CV_H && !defined HAVE_OPENCV_CV_H
}


/** 
 * copies a caputured image  to RGBA buffer.
 * 
 * @param dest 
 *
 * @param dest  pointer to store the frame.
 * 
 * @return Zero on success.
 *
 * @note This function uses LUT created by CreateYUVtoRGBAMap().
 */
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

/** 
 * Save the current image to a file.
 * 
 * @param filename   the name of filename to store the image.
 * @param type       a FILE_TYPE
 * 
 * @return 0 on success.
 *
 * @note  Only FILE_TYPE_PPM is supported.
 */
int  
C1394CameraNode::SaveToFile(char* filename,FILE_TYPE type)
{
//    switch (type){ 
//    {
    int w=GetImageWidth();
    int h=GetImageHeight();
    RGBA buf[w*h];
    CopyRGBAImage(buf);
    return SaveRGBAtoFile(filename, buf, w, h);
//    }
}
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
