/*! 
  @file    yuv.h
  @brief   convert YUV to RGBA
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: yuv.h,v 1.2 2003-01-08 18:38:01 yosimoto Exp $
  @date    $Date: 2003-01-08 18:38:01 $
 */

#if !defined(_YUV_H_INCLUDED_)
#define _YUV_H_INCLUDED_

#if !defined UCHAR
typedef unsigned char UCHAR; 
#endif 

//! RGBA structure
class RGBA {
public:
    UCHAR r;  //!< red component
    UCHAR g;  //!< green component
    UCHAR b;  //!< blue component
    UCHAR a;  //!< alpha component, but you may use this field as a work area.

    RGBA(){}
    RGBA(UCHAR r_, UCHAR g_, UCHAR b_):r(r_),g(g_),b(b_){}
    RGBA(UCHAR r_, UCHAR g_, UCHAR b_,UCHAR a_):r(r_),g(g_),b(b_),a(a_){}
};

//! video format type
enum {
    REMOVE_HEADER    = 0x100,
    FMT_YUV411       = 0x001,
    FMT_YUV422       = 0x002,
    FMT_YUV444       = 0x003,
};

bool copy_YUV411toRGBA(RGBA* lpRGBA, const void* lpYUV411,
		       int sz_packet, int num_packet, int flag);
bool copy_YUV422toRGBA(RGBA* lpRGBA, const void* lpYUV422,
		       int sz_packet, int num_packet, int flag);
bool copy_YUV444toRGBA(RGBA* lpRGBA, const void* lpYUV444,
		       int sz_packet, int num_packet, int flag);

int SaveRGBAtoFile(char *pFile, const RGBA* img, int w, int h, int fmt=0);
int CreateYUVtoRGBAMap();

#if defined OPENCVAPI
bool copy_YUV411toIplImage(IplImage* dst, const void* lpYUV411,
			   int sz_packet, int num_packet, int flag);
bool copy_YUV422toIplImage(IplImage* dst, const void* lpYUV422,
			   int sz_packet, int num_packet, int flag);
bool copy_YUV444toIplImage(IplImage* dst, const void* lpYUV444,
			   int sz_packet, int num_packet, int flag);
#endif // #if !defined IPL_IMAGE_MAGIC_VAL



#endif //#if !defined(_YUV_H_INCLUDED_) 
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
