/*!
  @file  1394cam_registers.h
  @brief 1394-based Digital Camera command registers
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: 1394cam_registers.h,v 1.2 2002-03-15 21:08:31 yosimoto Exp $
 */

#if !defined(_1394cam_registers_h_included_)
#define _1394cam_registers_h_included_

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
RegInfo(0x0180,V_MODE_INQ_0)
RegInfo(0x0184,V_MODE_INQ_1)
RegInfo(0x0188,V_MODE_INQ_2)
//RegInfo(0x018c,V_MODE_INQ_3)  // not defined at spec 1.30
//RegInfo(0x0190,V_MODE_INQ_4)
//RegInfo(0x0194,V_MODE_INQ_5)
RegInfo(0x0198,V_MODE_INQ_6)
RegInfo(0x019c,V_MODE_INQ_7)
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

#define SetParam(name,field,value)  \
( CAT(MASK_##name,_##field ) & ((value)<< CAT(WAIT_##name,_##field)))
#define GetParam(name,field,value)  \
(( CAT(MASK_##name,_##field) & (value))>> CAT(WAIT_##name,_##field) )


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


#endif // #if !defined(_1394cam_registers_h_included_)
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
