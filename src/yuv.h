/*! 
  @file yuv.h
  @brief convert YUV to RGBA
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: yuv.h,v 1.3 2002-02-15 19:50:24 yosimoto Exp $
  @date    $Date: 2002-02-15 19:50:24 $
 */
// yuv.h - convert YUV to RGBA
//
// by hiromasa yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp> 
//
// Wed Jan 26 06:37:06 2000 By hiromasa yoshimoto 
// Fri Jun  2 10:09:42 2000 By hiromasa yoshimoto  エンディアンの変更

#if !defined(_YUV_H_INCLUDED_)
#define _YUV_H_INCLUDED_

typedef unsigned char UCHAR; 
class RGBA {
public:
  UCHAR r;
  UCHAR g;
  UCHAR b;
  UCHAR a;
  RGBA(){}
  RGBA(UCHAR r_,UCHAR g_,UCHAR b_):r(r_),g(g_),b(b_){}
  RGBA(UCHAR r_,UCHAR g_,UCHAR b_,UCHAR a_):r(r_),g(g_),b(b_),a(a_){}
};
typedef UCHAR YUV411;

enum {
  REMOVE_HEADER    =0x100,
  FMT_YUV411       =0x001,
  FMT_YUV422       =0x002,
};

void copy_YUV411toRGBA(RGBA* lpRGBA,const void* lpYUV411,
		       int sz_packet,int num_packet,int flag);
void copy_YUV422toRGBA(RGBA* lpRGBA,const void* lpYUV422,
		       int sz_packet,int num_packet,int flag);
void copy_YUV444toRGBA(RGBA* lpRGBA,const void* lpYUV444,
		       int sz_packet,int num_packet,int flag);

int SaveRGBAtoFile(char *pFile,const RGBA* img,int w,int h,int fmt=0);
int CreateYUVtoRGBAMap();

#if defined IPLAPI
void copy_YUV411toIplImage(IplImage* dst ,const void* lpYUV411,
		       int sz_packet,int num_packet,int flag);
void copy_YUV422toIplImage(IplImage* dst ,const void* lpYUV422,
		       int sz_packet,int num_packet,int flag);
void copy_YUV444toIplImage(IplImage* dst ,const void* lpYUV444,
		       int sz_packet,int num_packet,int flag);
#endif // #if defined IPLAPI

#endif //#if !defined(_YUV_H_INCLUDED_) 
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
