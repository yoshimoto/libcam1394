#if !defined(_COMMON_H_INCLUDED_)
#define _COMMON_H_INCLUDED_

// flags
// DEBUG
// TRACE_OFF     

#if defined(DEBUG)
#define LOG(msg) cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<endl
#define ERR(msg) cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<endl
#define MSG(msg) cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<endl
//#define TRY(func)  { int result= func ; if (result<0){ \
//cerr <<__FILE__<<"("<<__LINE__<<")"<<__STRING(func)<<":"<<result<<endl; exit(-1); }}
#else // #if defined(DEBUG)
#define LOG(msg) (void*)(0)
#define ERR(msg) cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<endl
#define MSG(msg) cerr<<__FILE__<<"("<<__LINE__<<")"<< msg <<endl
//#define TRY(func)  { int result = func ; if (result<0){ \
//cerr <<__FILE__<<"("<<__LINE__<<")"<<__STRING(func)<<":"<<result<<endl; exit(-1); }}
#endif // #if defined(DEBUG)

#define TRY(f) f

#if !defined(TRACE_OFF) 
#define TRACE(msg) LOG(msg)
#else 
#define TRACE(msg) (void*)(0)
#endif

#endif //#if !defined(_COMMON_H_INCLUDED_)
