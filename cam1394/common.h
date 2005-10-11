/*!
  \file   common.h
  \author $Id: common.h,v 1.4 2005-10-11 05:29:13 yosimoto Exp $
  \date   Thu Oct 31 18:45:40 2002
  \brief  

*/

#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_


extern int opt_debug_level;

#if defined(DEBUG)
#define CAM1394_OUTPUT(msg) \
 std::cerr<<__FILE__<<"("<<__LINE__<<"): "<<msg<<std::endl
#else // #if defined(DEBUG)
#define CAM1394_OUTPUT(msg) \
 std::cerr<<msg<<std::endl
#endif // #if defined(DEBUG)

#define LOG(msg) do { if (opt_debug_level>2){ CAM1394_OUTPUT(msg);} } while(0)
#define WRN(msg) do { if (opt_debug_level>1){ CAM1394_OUTPUT(msg);} } while(0)
#define ERR(msg) do { if (opt_debug_level>0){ CAM1394_OUTPUT(msg);} } while(0)
#define MSG(msg) do { if (opt_debug_level>=0){ CAM1394_OUTPUT(msg);} } while(0)

#define TRY(f) f

#if !defined(TRACE_OFF) 
#define TRACE(msg) LOG(msg)
#else 
#define TRACE(msg) (void*)(0)
#endif

#endif //#if !defined(_COMMON_H_INCLUDED_)
