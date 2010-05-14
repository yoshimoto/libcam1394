/*!
  \file   common.h
  \author $Id: common.h,v 1.7 2004-08-30 08:04:21 yosimoto Exp $
  \date   Thu Oct 31 18:45:40 2002
  \brief  

*/

#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_

#include <iostream>

extern int libcam1394_debug_level;

#if defined(DEBUG)
#define LIBCAM1394_OUTPUT(msg) \
 std::cerr<<__FILE__<<"("<<__LINE__<<"): "<<msg<<std::endl
#else // #if defined(DEBUG)
#define LIBCAM1394_OUTPUT(msg) \
 std::cerr<<"libcam1394: "<<msg<<std::endl
#endif // #if defined(DEBUG)

#define DBG(msg) do { if (libcam1394_debug_level>3){ LIBCAM1394_OUTPUT(msg);} } while(0)
#define LOG(msg) do { if (libcam1394_debug_level>2){ LIBCAM1394_OUTPUT(msg);} } while(0)
#define WRN(msg) do { if (libcam1394_debug_level>1){ LIBCAM1394_OUTPUT(msg);} } while(0)
#define ERR(msg) do { if (libcam1394_debug_level>0){ LIBCAM1394_OUTPUT(msg);} } while(0)

#endif //#if !defined(_COMMON_H_INCLUDED_)
