/**
 * @file    1394cam.cc
 * @brief   1394-based Digital Camera control class
 * @date    Sat Dec 11 07:01:01 1999
 * @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
 * @version $Id: 1394cam.cc,v 1.51 2005-11-09 10:41:29 yosimoto Exp $
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
#if defined HAVE_ISOFB
#include <linux/ohci1394_iso.h>
#else
#include "video1394.h"
#endif

#if defined HAVE_CV_H || defined HAVE_OPENCV
#include <cv.h>
#define IPL_IMG_SUPPORTED
#elif defined HAVE_OPENCV_CV_H
#include <opencv/cv.h>
#define IPL_IMG_SUPPORTED
#else
#undef  IPL_IMG_SUPPORTED
#endif

#include "common.h"
#include "1394cam_registers.h"
#include "1394cam.h"
#include "yuv.h"


using namespace std;

#define CHK_PARAM(exp) {if (!(exp)) LOG( "illegal param passed. " << __STRING(exp)); }
#define EXCEPT_FOR_FORMAT_6_ONLY  {if (is_format6) return false;}

#define Addr(name) (m_command_regs_base+OFFSET_##name)
#define EQU(val,mask,pattern)  (0==(((val)&(mask))^(pattern)))

#define ADDR_CONFIGURATION_ROM        (CSR_REGISTER_BASE + CSR_CONFIG_ROM)

#define WAIT usleep(500)

// string-table for feature codes
static const char *feature_table[]={
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
    "trigger_delay",
    "white_shading",
    "frame_rate",

    "reserved","reserved",
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


static const char* abs_control_unit_table[64] = {
    "%", "EV", "", "K", 
    "deg", "%", "", "s",
    "dB", "F", "m", "",       // gain 
    "times", "s",  "", "fps", // trigger
    
    "", "", "", "",   // reserved
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    
    "power", "deg", "deg", "",   // zoom
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",	

    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
    "", "", "", "",
};


/**
 * read  CSR space 
 *
 * @param  handle
 * @param  nodeid
 * @param  addr
 * @param  len
 * @param  buf
 *
 * @return 0 on success, otherwize -1.
 *
 * @sa IEEE1212
 */
static int 
try_raw1394_read(raw1394handle_t handle, nodeid_t nodeid,
			    nodeaddr_t addr, int len, quadlet_t *buf)
{
    int retry = 3;
    while (retry-->0){
	int retval = raw1394_read(handle, nodeid, addr, len, buf);
	if (retval >= 0){
	    return 0;
	}
	usleep(500);
    }
    WRN("try_raw1394_read() failed.");
    return -1;
}

/** 
 * 
 * 
 * @param handle 
 * @param nodeid 
 * @param addr 
 * @param root 
 * 
 * @return 
 */
static int 
get_root_directory_address(nodeaddr_t *root,
			   raw1394handle_t handle, nodeid_t nodeid)
{
    if (root)
	*root = ADDR_CONFIGURATION_ROM + 0x14;
    return 0;
}

/** 
 * get directory length.
 * 
 * @param handle 
 * @param nodeid 
 * @param addr 
 * @param length 
 * 
 * @return 
 */
static int 
get_directory_length(int *length,
		     raw1394handle_t handle, nodeid_t nodeid, nodeaddr_t addr) 
{
    quadlet_t tmp;    
    if (try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp))
	return -1;
    tmp = htonl( tmp );
    *length = tmp >> 16;
    return 0;
}

/** 
 * 
 * 
 * @param key      
 * @param value    result 
 * @param handle 
 * @param node_id  
 * @param addr      begining of the directory.
 * @param length    length  of the directory.
 * 
 * @return 
 */
static int 
get_value(quadlet_t  *value,
	  unsigned char key,
	  raw1394handle_t handle, 
	  nodeid_t node_id, nodeaddr_t addr, int  length)
{
    quadlet_t tmp;
    while (length-->0){
	addr+=4;
	try_raw1394_read(handle, node_id, addr, sizeof(tmp), &tmp);	
	tmp = htonl( tmp );	 
	if (key == (tmp >> 24)){
	    if (value)
		*value = tmp & 0xffffff;
	    return 0;
	}
    }
    return -1;
}


/** 
 * 
 * 
 * @param key 
 * @param directory 
 * @param handle 
 * @param node_id 
 * @param addr 
 * @param length 
 * 
 * @return 
 */
static int 
get_sub_directory_address(nodeaddr_t *directory, 
			  unsigned char key,
			  raw1394handle_t handle,
			  nodeid_t node_id, nodeaddr_t addr, int  length)
{
    quadlet_t tmp;
    while (length-->0){
	addr+=4;
	try_raw1394_read(handle, node_id, addr, sizeof(tmp), &tmp);	
	tmp = htonl( tmp );	 
	if (key == (tmp >> 24)){
	    if (directory)
		*directory = addr + 4*(tmp & 0xffffff);
	    return 0;
	}
    }
    return -1;
}
				

/** 
 * 
 * 
 * @param buf 
 * @param size 
 * @param ket 
 * @param handle 
 * @param node_id 
 * @param addr 
 * @param length 
 * 
 * @return 
 */
static int get_name_leaf(char *buf, int size, 
			 unsigned char key,
			 raw1394handle_t handle, 
			 nodeid_t node_id, nodeaddr_t addr, int length)
{
    nodeaddr_t leaf_address;
    if (get_sub_directory_address(&leaf_address,
				  key, handle, node_id, addr, length))
	return -1;


    if (get_directory_length(&length, handle, node_id, leaf_address))
	return -1;
    length *= 4;
    length -= 8;
    leaf_address += 0x0c;
    
    char *p=buf;
    while (length > 0 && size>0){
	quadlet_t tmp;
	try_raw1394_read(handle, node_id, leaf_address, sizeof(tmp), &tmp);
	tmp = htonl( tmp );	  
	
	int c=32;
	while (c>=0 && size-->0){
	    c-=8;
	    *p++ = tmp >> c;
	}

	length -= 4;
	leaf_address += 4;
    }

    // fill with zero
    while (size-->0){
	*p++ = 0;
    }

    return 0;
}

/** 
 * get  vendor and chip id.
 * 
 * @param pNode 
 * @param handle 
 * @param node_id 
 * @param addr_root 
 * @param len_root 
 * 
 * @return  0 on success, otherwize -1.
 */
static int
get_vendor_and_chip_id(C1394CameraNode *pNode,
		       raw1394handle_t handle, 
		       nodeid_t node_id, nodeaddr_t addr_root, int len)
{
    nodeaddr_t addr_node_uniq_id_leaf;
    if (get_sub_directory_address(&addr_node_uniq_id_leaf, 0x8d, 
				  handle, node_id, addr_root, len))
	return -1;
    
    int len_node_uniq_id_leaf;    
    if (get_directory_length(&len_node_uniq_id_leaf,
			     handle, node_id, addr_node_uniq_id_leaf))
	return -1;
    
    quadlet_t tmp;
    if (0!=try_raw1394_read(handle,node_id, 
			    addr_node_uniq_id_leaf + 4, sizeof(tmp), &tmp))
	return -1;
    tmp = ntohl( tmp );
    pNode->m_VenderID = tmp>>8;
    quadlet_t chip_id_hi = tmp&0xff;
    if (0!=try_raw1394_read(handle,node_id, addr_node_uniq_id_leaf + 8, 
			    sizeof(tmp), &tmp))
	return -1;
    tmp = ntohl( tmp );
    pNode->m_ChipID = (((uint64_t)chip_id_hi)<<32)+(uint64_t)tmp;
    return 0;
}

/**
 * returns euid (Extended Unique identifier) of the node.
 *
 * @param pNode
 * @param  handle
 * @param  node_id
 *
 * @return  0 on success,  otherwize -1.
 */
static int
get_extended_unique_identifier(C1394CameraNode *pNode,
			       raw1394handle_t handle, 
			       nodeid_t node_id)
{
    quadlet_t tmp;

    nodeaddr_t addr = ADDR_CONFIGURATION_ROM + 0x0c;
    if (0!=try_raw1394_read(handle, node_id, addr, sizeof(tmp), &tmp))
	return -1;    
    tmp = ntohl( tmp );
    pNode->m_VenderID = tmp >> 8;
    quadlet_t chip_id_hi = tmp&0xff;
    addr += sizeof(tmp);
    if (0!=try_raw1394_read(handle, node_id, addr, sizeof(tmp), &tmp))
	return -1;
    tmp = ntohl( tmp );
    pNode->m_ChipID = (((uint64_t)chip_id_hi)<<32)+(uint64_t)tmp;
    return 0;
}

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
callback_1394Camera(raw1394handle_t handle, nodeid_t node_id,
		    C1394CameraNode* pNode, void* arg)
{
    int port_no = (int)arg;

    // search  "root_directory"
    nodeaddr_t addr_root;
    int len_root;
    if (get_root_directory_address(&addr_root, handle, node_id))
	return false;
    if (get_directory_length(&len_root, handle, node_id, addr_root))
	return false;

    // search  "unit_directory"
    nodeaddr_t addr_unit_dir;
    int len_unit_dir;
    if (get_sub_directory_address(&addr_unit_dir, 0xd1, 
				  handle, node_id, addr_root, len_root))
	return false;
    if (get_directory_length(&len_unit_dir, handle, node_id, addr_unit_dir))
	return false;
    
    // check the  "unit_directory.unit_spec_ID"
    quadlet_t unit_spec_ID;
    if (get_value(&unit_spec_ID, 0x12, 
		  handle, node_id, addr_unit_dir, len_unit_dir))
	return false;
    if (unit_spec_ID != 0x00A02D)
	return false;
    
    // search "unit_dependent_directory"
    nodeaddr_t addr_unit_dep_dir;
    int len_unit_dep_dir;    
    if (get_sub_directory_address(&addr_unit_dep_dir, 0xd4, 
				  handle, 
				  node_id, addr_unit_dir, len_unit_dir))
	return false;
    if (get_directory_length(&len_unit_dep_dir,
			     handle, node_id, addr_unit_dep_dir))
	return false;
    
    
    // get "unit_dependent_directory.command_regs_base"
    quadlet_t tmp;
    if (get_value(&tmp, 0x40, handle, node_id, 
		  addr_unit_dep_dir, len_unit_dep_dir))
	return false;
    pNode->m_command_regs_base = CSR_REGISTER_BASE + tmp * 4;
    

    // get "unit_dependent_directory.vendor_name_leaf"
    // get "unit_dependent_directory.model_name_leaf"

    char buf[32];
    if (!get_name_leaf(buf, sizeof(buf), 0x81, handle, node_id,
		       addr_unit_dep_dir, len_unit_dep_dir)) {
	pNode->m_lpVecderName = new char[ strlen(buf)+1 ];
	strcpy(pNode->m_lpVecderName, buf);
    } else {
	pNode->m_lpVecderName = NULL;
    }
    if (!get_name_leaf(buf, sizeof(buf), 0x82, handle, node_id,
		       addr_unit_dep_dir, len_unit_dep_dir)) {
	pNode->m_lpModelName = new char[ strlen(buf)+1 ];
	strcpy(pNode->m_lpModelName, buf);
    } else {
	pNode->m_lpModelName = NULL;
    }

    // get   node_unique_id_leaf.node_vendor_id and so on.
    // search "unit_dependent_directory"
    pNode->m_VenderID = 0;
    pNode->m_ChipID = 0;
    if (0!=get_vendor_and_chip_id(pNode, handle, node_id, 
				  addr_root, len_root)){
	// Because some camera such as ptgey's dragonfly  has no
	// vendor_id leaf on its unit_dependent_directory,
	// we'll try to use euid-64 alternatively.
	if (0!=get_extended_unique_identifier(pNode, handle, node_id)){
	    // Accroding to the IEEE1212 spec,
	    // all of 1394 nodes shall contain euid-64 field,
	    // so the get_extended_unique_identifier() will not failed.
	}
    }
    
    // store the infomation of this camera-device.
    pNode->m_handle  = handle;
    pNode->m_node_id = node_id;
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
Enum1394Node(raw1394handle_t handle,
	     raw1394_portinfo* pPort,
	     list<T>* pList,
	     int 
	     (*func)(raw1394handle_t handle,nodeid_t node_id,
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
	ERR("couldn't get card info. " <<strerror(errno));
	return false;
    } else {
	LOG(numcards << " card(s) found");
    }

    if (!numcards) {
	return false;
    }

    pList->clear();

    int i;
    for (i = 0; i< numcards; i++) {
	raw1394handle_t new_handle = raw1394_new_handle();

	if (raw1394_set_port(new_handle, i) < 0) {
	    ERR("couldn't set port. "<< strerror(errno));
	    return false;
	}	
	size_t pre = pList->size();
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
    int retry=4;
    //*value=0x12345678;    
    while (retry-- > 0){
	int retval = raw1394_read(m_handle, m_node_id, addr, 4, value);
	if (retval >= 0){
	    *value = (quadlet_t)ntohl((unsigned long int)*value);    
	    return true;
	}
	WAIT;
    } 
    return false;
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
    int retry=4;
    quadlet_t tmp=htonl(*value);
    while (retry-- > 0){
	int retval = raw1394_write(m_handle, m_node_id, addr, 4, &tmp);
	if (retval >= 0){
	    return true;
	}
	WAIT;
    }
    return false;
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

  fd = -1;
}

C1394CameraNode::~C1394CameraNode()
{
#if defined(_WITH_ISO_FRAME_BUFFER_)
    m_BufferSize=0;
#endif //#if defined(_WITH_ISO_FRAME_BUFFER_)    
    if (fd>0)
	close(fd);
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
    return WriteReg(Addr(Camera_Power),&tmp);
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
    return WriteReg(Addr(Camera_Power),&tmp);
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
	default:
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
    default:
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
	return feature_table[feat];
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
    ERR("the feature "<<feature_table[feat]<<" is not available.");
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
//  tmp |= SetParam(BRIGHTNESS,Abs_Control, 0);
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
    ERR("the feature "<<feature_table[feat]<<" is not available");
    return false;
  }
  if (!GetParam(BRIGHTNESS_INQ,ReadOut_Inq,tmp)){
    ERR("the value of feature "<<feature_table[feat]<<" is invalid");
    return false;
  }
  
  // disable abs_control if abs_control mode
  ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  if ( GetParam(BRIGHTNESS, Abs_Control, tmp) ){
      tmp &= ~SetParam(BRIGHTNESS, Abs_Control, 1);
      WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  }
  
  ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  *value=tmp&((1<<24)-1);////GetParam(BRIGHTNESS,Value,tmp);
  return true;
}


/** 
 * Get minimum/maximum value for the feature control.
 * 
 * @param feat 
 * @param min   pointer to store the minimum value, or NULL.
 * @param max   pointer to store the maximum value, or NULL.
 *
 * @return 
 *
 * @since  libcam1394-0.2.0
 *
 */
bool
C1394CameraNode::GetParameterRange(C1394CAMERA_FEATURE feat,
				   unsigned int *min,
				   unsigned int *max)
{
  quadlet_t tmp;
    
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);

  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_table[feat]<<" is not available.");
    return false;
  }
  if (GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)==0){
    ERR("the feature "<<feature_table[feat]<<" is not available.");
    return false;
  }
  
  if (min){
      *min = GetParam(BRIGHTNESS_INQ,MIN_Value,tmp);
  }
  if (max){
      *max = GetParam(BRIGHTNESS_INQ,MAX_Value,tmp);
  }

  return true;
}

/** 
 *
 * Set the value for absolute feature control.
 * 
 * @param feat 
 * @param value 
 * 
 * @return 
 *
 * @since  libcam1394-0.2.0
 */
bool
C1394CameraNode::SetAbsParameter(C1394CAMERA_FEATURE feat, float value)
{
  quadlet_t tmp=0;
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (!GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
      ERR("the feature ( "<<feature_table[feat]<<") is not available");
      return false;
  }
  if (!GetParam(BRIGHTNESS_INQ,Abs_Control_Inq,tmp)){
      ERR("the capability of control with absolute value ( "
	  <<feature_table[feat]<<") is not available");
      return false;
  }

  // enable absolute control
  //ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  tmp = 0;
  tmp |= SetParam(BRIGHTNESS, Presence_Inq, 1);
  tmp |= SetParam(BRIGHTNESS, Abs_Control, 1);
  tmp |= SetParam(BRIGHTNESS, ON_OFF, 1);
  WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);
 
  // retrive the offset of absolute value CSR.
  quadlet_t off = 0;
  ReadReg(Addr(ABS_CSR_HI_INQ_0)+4*feat, &off);  
  // write abs value
  WriteReg(CSR_REGISTER_BASE + off*4 + 0x0008, (quadlet_t*)&value);

  return true;
}

/** 
 * Get the value for absolute feature control.
 * 
 * @param feat 
 * @param value 
 * 
 * @return 
 *
 * @since  libcam1394-0.2.0
 *
 */
bool
C1394CameraNode::GetAbsParameter(C1394CAMERA_FEATURE feat, float *value)
{
  quadlet_t tmp=0;
  
  ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
  if (!GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
      ERR("the feature ( "<<feature_table[feat]<<") is not available");
      return false;
  }
  if (!GetParam(BRIGHTNESS_INQ,Abs_Control_Inq,tmp)){
      ERR("the capability of control with absolute value ( "
	  <<feature_table[feat]<<") is not available");
    return false;
  }

  // enable absolute control
  ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
  tmp |= SetParam(BRIGHTNESS, Abs_Control, 1);
  WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);
 
  // retrive the offset of absolute value CSR.
  quadlet_t off = 0;
  ReadReg(Addr(ABS_CSR_HI_INQ_0)+4*feat, &off);
  LOG("off is "<<hex<<off<<dec);
  
  // read Value
  ReadReg(CSR_REGISTER_BASE + off*4 + 0x0008, (quadlet_t*)value);

  return true;
}


/** 
 * Get the "Unit" string for the feature.
 * 
 * @param feat 
 * 
 * @return pointer to the string for the feat.
 *
 * @since  libcam1394-0.2.0
 */
const char* 
C1394CameraNode::GetAbsParameterUnit(C1394CAMERA_FEATURE feat)
{
    //!!FIXME!!
    return abs_control_unit_table[feat];
}

/** 
 *
 * Get minimum/maximum value for the feature absolute control.
 * 
 * @param feat 
 * @param min   pointer to store the minimum value, or NULL.
 * @param max   pointer to store the maximum value, or NULL.
 * 
 * @return 
 *
 * @since  libcam1394-0.2.0
 */
bool 
C1394CameraNode::GetAbsParameterRange(C1394CAMERA_FEATURE feat, 
				      float* min,
				      float* max)
{
    quadlet_t tmp=0;
  
    ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
    if (!GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
	ERR("the feature ( "<<feature_table[feat]<<") is not available");
	return false;
    }
    if (!GetParam(BRIGHTNESS_INQ,Abs_Control_Inq,tmp)){
	ERR("the capability of control with absolute value ( "
	    <<feature_table[feat]<<") is not available");
	return false;
    }

    // enable absolute control
    ReadReg(Addr(BRIGHTNESS)+4*feat, &tmp);
    tmp |= SetParam(BRIGHTNESS, Abs_Control, 1);
    WriteReg(Addr(BRIGHTNESS)+4*feat, &tmp);
 
    // retrive the offset of absolute value CSR.
    quadlet_t off = 0;
    ReadReg(Addr(ABS_CSR_HI_INQ_0)+4*feat, &off);
    LOG("off is "<<hex<<off<<dec);
    
    // read Value
    if (min){
	ReadReg(CSR_REGISTER_BASE + off*4 + 0x0000, (quadlet_t*)min);
    }
    if (max){
	ReadReg(CSR_REGISTER_BASE + off*4 + 0x0004, (quadlet_t*)max);
    }

    return false;
}

/** 
 * 
 * 
 * @param feat 
 * 
 * @return returns true if the absolute control is available.
 *
 * @since  libcam1394-0.2.0
 */
bool
C1394CameraNode::HasAbsControl(C1394CAMERA_FEATURE feat)
{
    quadlet_t tmp=0;
    ReadReg(Addr(BRIGHTNESS_INQ)+4*feat,&tmp);
    if (!GetParam(BRIGHTNESS_INQ,Presence_Inq,tmp)){
	return false;
    }
    if (!GetParam(BRIGHTNESS_INQ,Abs_Control_Inq,tmp)){
	return false;
    }
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
	ERR("the feature "<<feature_table[feat]<<" is not available");
	return false;
    }
    if (0==GetParam(BRIGHTNESS_INQ,On_Off_Inq,inq)){
	ERR("the feature "<<feature_table[feat]<<" has no ON/OFF cap.");
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
    tmp=0x82010000;
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
    {"YUV(4:4:4)",  4, 4, 4   }, //! VFMT_YUV444
    {"YUV(4:2:2)",  4, 2, 2   }, //! VFMT_VUV422
    {"YUV(4:1:1)",  4, 1, 1   }, //! VFMT_VUV411
    {"RGB(8:8:8)",  8, 8, 8   }, //! VFMT_RGB888
    {" Y (Mono) ",  8, 0, 0   }, //! VFMT_Y8
    {" Y(Mono16)", 16, 0, 0   }, //! VFMT_Y16
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
	{   VFMT_YUV422,       1280, 960}, // format_2/mode_0
	{   VFMT_RGB888,       1280, 960}, // format_2/mode_1
	{   VFMT_Y8    ,       1280, 960}, // format_2/mode_2
	{   VFMT_YUV422,       1600,1200}, // format_2/mode_3
	{   VFMT_RGB888,       1600,1200}, // format_2/mode_4
	{   VFMT_Y8    ,       1600,1200}, // format_2/mode_5	
	{   VFMT_Y16   ,       1280, 960}, // format_2/mode_6
	{   VFMT_Y16   ,       1600,1200}, // format_2/mode_7	
    },
};
#undef RESERVED


#define RESERVED  { -1, -1, SPD_100M}
static struct VideoPacketInfo video_packet_info[][8][6]= // format / mode / fps
{
    // format_0
    {
	{ // format_0 mode_0 
	    RESERVED,                     // 1.875fps
	    RESERVED,                     // 3.75 fps
	    {  15*4, 120*8, SPD_100M  },  // 7.5  fps
	    {  30*4, 120*4, SPD_100M  },  // 15   fps
	    {  60*4, 120*2, SPD_100M  },  // 30   fps
	    RESERVED,                     // 60   fps
	},
	{ // format_0 mode_1 
	    RESERVED,
	    {  20*4, 240*8 ,SPD_100M},
	    {  40*4, 240*4 ,SPD_100M},
	    {  80*4, 240*2 ,SPD_100M},
	    { 160*4, 240   ,SPD_100M},
	    RESERVED,
	},
	{ // format_0 mode_2
	    RESERVED,
	    {  60*4, 480*4 ,SPD_100M},
	    { 120*4, 480*2 ,SPD_100M},
	    { 240*4, 480/1 ,SPD_100M},
	    { 480*4, 480/2 ,SPD_200M},
	    RESERVED,
	},
	{  // format_0 mode_3
	    RESERVED,
	    {  80*4, 480*4 ,SPD_100M},
	    { 160*4, 480*2 ,SPD_100M},
	    { 320*4, 480/1 ,SPD_200M},
	    { 640*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_0 mode_4
	    RESERVED,
	    { 120*4, 480*4 ,SPD_100M},
	    { 240*4, 480*2 ,SPD_100M},
	    { 480*4, 480*1 ,SPD_200M},
	    { 960*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_0 mode_5
	    RESERVED,
	    {  40*4, 480*4 ,SPD_100M},
	    {  80*4, 480*2 ,SPD_100M},
	    { 160*4, 480/1 ,SPD_100M},
	    { 320*4, 480/2 ,SPD_200M},
	    { 640*4, 480/4 ,SPD_400M},
	},  
	{  // format_0 mode_6
	    RESERVED,
	    {  80*4, 480*4 ,SPD_100M},
	    { 160*4, 480*2 ,SPD_100M},
	    { 320*4, 480/1 ,SPD_200M},
	    { 640*4, 480/2 ,SPD_400M},
	    RESERVED,
	},
	{ // format_0 mode_7
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
 * Returns pixel format.
 * 
 * @param fmt 
 * @param mode 
 * 
 * @return PIXEL_FORMAT
 */
PIXEL_FORMAT GetPixelFormat(FORMAT fmt, VMODE mode)
{
    if (!(0<=fmt&&fmt<=2))
	return VFMT_NOT_SUPPORTED;
    else
	return video_image_info[fmt][mode].pixel_format;
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
	" 100Mbps",
	" 200Mbps",
	" 400Mbps",
	" 800Mbps",
	"1600Mbps",
	"3200Mbps",
    };
    if (0 <= spd && spd < (SPD)(sizeof(speed_string)/sizeof(char*)) )
	return speed_string[spd];
    else
	return "????Mbps";
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
#if !defined(HAVE_ISOFB)
    return -1;
#else
    return (0 != ioctl(fd, IOCTL_SET_COUNT, counter));
#endif

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
#if !defined(HAVE_ISOFB)
    return -1;
#else
    return  (0 != ioctl(fd, IOCTL_GET_COUNT, counter));
#endif
}

#if !defined(HAVE_ISOFB)
static void* mmap_video1394(int port_no, int channel,
			    int m_packet_sz, int m_num_packet, 
			    int *m_num_frame,
			    int *fd,  int *m_BufferSize)
{
    unsigned int i;
    void *buffer=NULL;
    
    char devname[1024];
    snprintf(devname, sizeof(devname), "/dev/video1394/%d", port_no);
    LOG("open("<<devname<<")");
    *fd = open(devname, O_RDONLY);
    // quick hack for backward-compatibility
    if (*fd<0){
	snprintf(devname, sizeof(devname), "/dev/video1394%c", 
		 char('a'+port_no));
	*fd = open(devname, O_RDONLY);	
    }
    // check whether the driver is loaded or not.
    if (*fd<0){
	ERR("Failed to open video1394 device (" 
	    << devname << ") : " << strerror(errno));
	ERR("The video1394 module must be loaded, "
	    "and you must have read and write permission to "<<devname<<".");
	return NULL;
    }    
    
    struct video1394_mmap vmmap;
    struct video1394_wait vwait;

    vmmap.channel = channel;  
    vmmap.sync_tag = 1;
    vmmap.nb_buffers = *m_num_frame;
    vmmap.buf_size =   m_packet_sz*m_num_packet;
//    vmmap.packet_size = m_packet_sz;
    vmmap.fps = 0;
    vmmap.syt_offset = 0;
    vmmap.flags = VIDEO1394_SYNC_FRAMES | VIDEO1394_INCLUDE_ISO_HEADERS;
    
    /* tell the video1394 system that we want to listen to the given channel */
    if (ioctl(*fd, VIDEO1394_IOC_LISTEN_CHANNEL, &vmmap) < 0)
    {
        ERR("VIDEO1394_IOC_LISTEN_CHANNEL ioctl failed");
        return NULL;
    }

    channel = vmmap.channel;
    *m_BufferSize = vmmap.buf_size;
    *m_num_frame = vmmap.nb_buffers;

    LOG("channel: "<< vmmap.channel);

    /* QUEUE the buffers */
    for (i = 0; i < vmmap.nb_buffers; i++){
	vwait.channel = channel;
        vwait.buffer = i;
        if (ioctl(*fd,VIDEO1394_IOC_LISTEN_QUEUE_BUFFER,&vwait) < 0)
        {
	    LOG("unlisten channel " << channel);
	    ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER ioctl failed");
            ioctl(*fd,VIDEO1394_IOC_UNLISTEN_CHANNEL,&channel);
            return NULL;
        }
    }
    
    /* allocate ring buffer  */
    vwait.channel= vmmap.channel;
    buffer = mmap(0, vmmap.nb_buffers * vmmap.buf_size,
		  PROT_READ,MAP_SHARED, *fd, 0);
    if (buffer == MAP_FAILED) {
        ERR("video1394_mmap failed");
        ioctl(*fd, VIDEO1394_IOC_UNLISTEN_CHANNEL, &vmmap.channel);
        return NULL;
    }
    
    return buffer;
}
#else
static void* mmap_isofb(int port_no, int channel,
			int m_packet_sz, int m_num_packet,
			int *m_num_frame,
			int *fd, int *m_BufferSize)
{
    void *buffer=NULL;

    char devname[1024];
    snprintf(devname, sizeof(devname), "/dev/isofb%d", port_no);
    LOG("open("<<devname<<")");
    *fd=open(devname,O_RDONLY);
    if (*fd < 0){
	ERR("Failed to open isofb device (" 
	    << devname << ") : " << strerror(errno));
	ERR("The ohci1394_fb module must be loaded, "
	    "and you must have read and write permission to "<<devname<<".");
	goto err;
    }
  
    ISO1394_RxParam rxparam;
    if (0>ioctl(*fd, IOCTL_INIT_RXPARAM, &rxparam)){
	ERR("IOCTL_INIT_RXPARAM failed");
	goto err;
    }
  
    rxparam.sz_packet      = m_packet_sz ;
    rxparam.num_packet     = m_num_packet ;
    rxparam.num_frames     = *m_num_frame;
    rxparam.packet_per_buf = 0;
    rxparam.wait           = 1;
    rxparam.sync           = 1;
    rxparam.channelNum     = channel ;

    if (0 > ioctl(*fd, IOCTL_CREATE_RXBUF, &rxparam) ){
	ERR("IOCTL_CREATE_RXBUF failed");
	goto err;
    }
    
    buffer = mmap(NULL,
		  rxparam.sz_allocated,
		  PROT_READ,
		  MAP_SHARED, 
		  *fd,0);
    if (buffer == MAP_FAILED){
	ERR("isofb_mmap failed");
	goto err;
    }


    if (0 >  ioctl(*fd, IOCTL_START_RX) ){
	ERR("IOCTL_START_RX failed");
	goto err;
    }

    *m_BufferSize = m_packet_sz*m_num_packet;

    return buffer;

err:
    if (*fd>0) close(*fd);
    *fd=-1;
    return NULL;
}
#endif

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
    if (m_packet_sz < 0){
	LOG("this fmt/mode/rate is not supported.");
	return -1;
    }
    m_packet_sz += 8;
    m_num_packet = ::GetNumPackets(fmt,mode,rate);
    if ( m_num_packet < 0 ){
	ERR("packet size is too big");
	return -2;
    }
    
    m_num_frame = 64;

    LOG("packet size: " << m_packet_sz);
    LOG("packet num:  " << m_num_packet);
#if !defined(HAVE_ISOFB)
    pMaped = (char*)mmap_video1394(m_port_no, channel, 
				   m_packet_sz, m_num_packet,
				   &m_num_frame,
				   &fd, &m_BufferSize);
    if (!pMaped){
	ERR("mmap_video1394() failed");
	return -3;
    }
#else
    m_num_frame = 16;

    pMaped = (char*)mmap_isofb(m_port_no, channel,
			       m_packet_sz, m_num_packet,
			       &m_num_frame,
			       &fd, &m_BufferSize);
    if (!pMaped){
	ERR("mmap_isofb() failed");
	return -3;
    }
#endif


    m_Image_W=::GetImageWidth(fmt,mode);
    m_Image_H=::GetImageHeight(fmt,mode);

    m_last_read_frame = 0;
    m_pixel_format  = video_image_info[fmt][mode].pixel_format;
    m_lpFrameBuffer = pMaped;
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
    if (!count)
	return -EINVAL;
#if !defined(HAVE_ISOFB)
    *count = m_last_read_frame;
    return 0;
#else
    return ::GetFrameCounter(fd,count);
#endif
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
#if !defined(HAVE_ISOFB)
    ERR("video1394 doesn't supporte this function");
    return -1;
#else
    return ::SetFrameCounter(fd,tmp);
#endif
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
 *
 * @todo  AS_FIFO and WAIT_NEW_FRAME is not implemented yet.
 */
void* C1394CameraNode::UpDateFrameBuffer(BUFFER_OPTION opt,BufferInfo* info)
{
#if !defined(HAVE_ISOFB)
    int result;
    struct video1394_wait vwait;
    vwait.channel = m_channel;
    vwait.buffer = m_last_read_frame%m_num_frame;
    result = ioctl(fd, VIDEO1394_IOC_LISTEN_WAIT_BUFFER, &vwait);
    //result = ioctl(fd, VIDEO1394_IOC_LISTEN_POLL_BUFFER, &vwait);
    
    if (result!=0){
	if (errno == EINTR){
	    
	}
	return m_lpFrameBuffer;
    }

    int drop_frames = vwait.buffer;
    while (drop_frames-->0){
	vwait.channel = m_channel;
	vwait.buffer = m_last_read_frame++%m_num_frame;
	if (ioctl(fd,VIDEO1394_IOC_LISTEN_QUEUE_BUFFER,&vwait) < 0){
	    ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER failed.");
	}	    
    }

    vwait.channel = m_channel;
    vwait.buffer = m_last_read_frame++ % m_num_frame;
    if (ioctl(fd,VIDEO1394_IOC_LISTEN_QUEUE_BUFFER,&vwait) < 0){
	ERR("VIDEO1394_IOC_LISTEN_QUEUE_BUFFER failed.");
    }
    m_lpFrameBuffer =
	pMaped + m_BufferSize*((m_last_read_frame-1)%m_num_frame);
#else
    ISO1394_Chunk chunk;
    if (0 > ioctl(fd, IOCTL_GET_RXBUF, &chunk) ){
	ERR("ISOFB_IOCTL_GET_RXBUF failed:  " << strerror(errno) );
	return NULL;
    }
    m_lpFrameBuffer=(pMaped+chunk.offset);
#endif
    if (info){
	quadlet_t *ts = (quadlet_t*)(m_lpFrameBuffer + m_packet_sz - 4);
	info->timestamp = (*ts) & 0x0000ffff;
    }
    return  m_lpFrameBuffer;
}

/** 
 * Gets the size fo a frame.
 * 
 * @return size of frame buffer.
 *
 * @note Sometimes m_BufferSize is greater than m_packet_sz * m_num_packet.
 */
int C1394CameraNode::GetFrameBufferSize()
{
    return m_packet_sz*m_num_packet;
}

/** 
 * Returns the width of the image.
 * 
 * @return the width of image.
 */
int C1394CameraNode::GetImageWidth()
{
    return m_Image_W;
}

/** 
 * Returns the height of the image.
 * 
 * @return the height of image.
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
#ifndef IPL_IMG_SUPPORTED
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
    case VFMT_RGB888:
	::copy_RGB888toIplImage(dest,m_lpFrameBuffer,
				m_packet_sz,m_num_packet,
				REMOVE_HEADER);
    break;
    case VFMT_Y8:
	::copy_Y8toIplImage(dest,m_lpFrameBuffer,
				m_packet_sz,m_num_packet,
				REMOVE_HEADER);
    break;
    case VFMT_Y16:
	::copy_Y16toIplImage(dest,m_lpFrameBuffer,
			     m_packet_sz,m_num_packet,
			     REMOVE_HEADER);
    break;
    default:
	LOG("this pixel format is not supported yet.");
	break;
    }
  
    return 0;
#endif //#ifndef IPL_IMG_SUPPORTED
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
#ifndef IPL_IMG_SUPPORTED
    LOG("This system doesn't  have ipl or OpenCV library.");
    return -1;
#else
    switch  ( m_pixel_format ){
    case VFMT_YUV422:
	copy_YUV422toIplImageGray(dest,m_lpFrameBuffer,
				  m_packet_sz,m_num_packet,
				  REMOVE_HEADER);
	break;
    case VFMT_YUV411:
	copy_YUV411toIplImageGray(dest,m_lpFrameBuffer,
				  m_packet_sz,m_num_packet,
				  REMOVE_HEADER);
	break;
    case VFMT_YUV444:
	copy_YUV444toIplImageGray(dest,m_lpFrameBuffer,
				  m_packet_sz,m_num_packet,
				  REMOVE_HEADER);
	break;
    case VFMT_RGB888:
	copy_RGB888toIplImageGray(dest,m_lpFrameBuffer,
				  m_packet_sz,m_num_packet,
				  REMOVE_HEADER);
	break;
    case VFMT_Y8:
	copy_Y8toIplImageGray(dest,m_lpFrameBuffer,
			      m_packet_sz,m_num_packet,
			      REMOVE_HEADER);
	break;
    case VFMT_Y16:
	copy_Y16toIplImageGray(dest,m_lpFrameBuffer,
			       m_packet_sz,m_num_packet,
			       REMOVE_HEADER);
	break;
    default:
	LOG("this pixel format is not supported yet.");
	break;
    }
  
    return 0;
#endif //#ifndef IPL_IMG_SUPPORTED
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
	copy_YUV422toRGBA((RGBA*)dest,m_lpFrameBuffer,
			  m_packet_sz,m_num_packet,
			  REMOVE_HEADER);
	break;
    case VFMT_YUV411:
	copy_YUV411toRGBA((RGBA*)dest,m_lpFrameBuffer,
			  m_packet_sz,m_num_packet,
			  REMOVE_HEADER);
	break;
    case VFMT_YUV444:
	copy_YUV444toRGBA((RGBA*)dest,m_lpFrameBuffer,
			  m_packet_sz,m_num_packet,
			  REMOVE_HEADER);
	break;
    case VFMT_RGB888:
	copy_RGB888toRGBA((RGBA*)dest,m_lpFrameBuffer,
			  m_packet_sz,m_num_packet,
			  REMOVE_HEADER);
	break;
    case VFMT_Y8:
	copy_Y8toRGBA((RGBA*)dest,m_lpFrameBuffer,
		      m_packet_sz,m_num_packet,
		      REMOVE_HEADER);
	break;
    case VFMT_Y16:
	copy_Y16toRGBA((RGBA*)dest,m_lpFrameBuffer,
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
    int w=GetImageWidth();
    int h=GetImageHeight();
    RGBA buf[w*h];
    CopyRGBAImage(buf);
    return SaveRGBAtoFile(filename, buf, w, h);
}

//! debug level 
//  @sa ERR,LOG,WRN
#ifndef DEBUG
int libcam1394_debug_level = 0;
#else
int libcam1394_debug_level = INT_MAX;
#endif

/** 
 * Set debug level.
 * 
 * @param level
 */
void libcam1394_set_debug_level(int level)
{
    libcam1394_debug_level = level;
}

/** 
 * Returns version string.
 * 
 * 
 * @return pointer to the string.
 */
char *libcam1394_get_version(void)
{
    return PACKAGE_VERSION;
}

/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
