/*!
  \file   common.h
  \author $Id: common.h,v 1.3 2003-10-07 13:16:27 yosimoto Exp $
  \date   Thu Oct 31 18:45:40 2002
  \brief  

*/

#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_


#if defined(DEBUG)
#define LOG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#define ERR(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#define MSG(msg) std::cerr<<__FILE__<<"("<<__LINE__<<"): "<< msg <<std::endl
#else // #if defined(DEBUG)
#define LOG(msg) 
#define ERR(msg) std::cerr << msg << std::endl
#define MSG(msg) std::cerr << msg << std::endl
#endif // #if defined(DEBUG)

#define TRY(f) f

#if !defined(TRACE_OFF) 
#define TRACE(msg) LOG(msg)
#else 
#define TRACE(msg) (void*)(0)
#endif

#endif //#if !defined(_COMMON_H_INCLUDED_)
