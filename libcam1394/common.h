/*!
  \file   common.h
  \author $Id: common.h,v 1.4 2002-11-10 23:12:18 yosimoto Exp $
  \date   Thu Oct 31 18:45:40 2002
  \brief  

*/

#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_


#if defined(DEBUG)
#define LOG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<std::endl
#define ERR(msg) std::cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<std::endl
#define MSG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<std::endl
#else // #if defined(DEBUG)
#define LOG(msg) (void)(0)
#define ERR(msg) (void)(0)
#define MSG(msg) (void)(0)
#endif // #if defined(DEBUG)

#define TRY(f) f

#if !defined(TRACE_OFF) 
#define TRACE(msg) LOG(msg)
#else 
#define TRACE(msg) (void)(0)
#endif

#endif //#if !defined(_COMMON_H_INCLUDED_)
