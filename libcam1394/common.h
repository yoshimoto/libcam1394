/*!
  \file   common.h
  \author $Id: common.h,v 1.6 2003-12-19 12:40:25 yosimoto Exp $
  \date   Thu Oct 31 18:45:40 2002
  \brief  

*/

#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_

#include <iostream>

#if defined(DEBUG)
#define LOG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#define ERR(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#define MSG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#else // #if defined(DEBUG)
#define LOG(msg) (void)(0)
#define ERR(msg) (void)(0)
#define MSG(msg) (void)(0)
#endif // #if defined(DEBUG)

#if !defined(TRACE_OFF) 
#define TRACE(msg) LOG(msg)
#else 
#define TRACE(msg) (void)(0)
#endif

#endif //#if !defined(_COMMON_H_INCLUDED_)
