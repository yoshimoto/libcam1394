/*!
  @file  yuv2rgb.cc
  @brief convert YUV to RGBA
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: yuv2rgb.cc,v 1.10 2004-08-29 08:54:34 yosimoto Exp $
 */

#include "config.h"
#include <stdio.h>

#if defined HAVE_CV_H
#include <cv.h>
#define IPL_IMG_SUPPORTED
#elif defined HAVE_OPENCV_CV_H
#include <opencv/cv.h>
#define IPL_IMG_SUPPORTED
#endif

#include "yuv.h"

using namespace std;

/**
 * @class RGBA yuv.h
 * @brief RGBA structure
 */


/*  LUT for yuv to RGBA conversion */


typedef signed int FIX ;	//!< fixex point integer for color conversion.
#define FIX_BASE  10  
#define FLOAT2FIX(f)  (FIX)(f*(float)(1<<FIX_BASE))
#define FIX2INT(F)    ((F)>>FIX_BASE)

static  FIX table_y [256];  //!< LUT for  Y component
static  FIX table_r [256];  //!< LUT for  u component
static  FIX table_g0[256];  //!< LUT for  u component
static  FIX table_g1[256];  //!< LUT for  v component
static  FIX table_b [256];  //!< LUT for  v component

/** 
 * Create look-up tabe for YUV to RGBA conversion.
 *
 * @return  Zero on success.
 */
int
CreateYUVtoRGBAMap()
{
    int i;
    for (i=0;i<256;i++){
	UCHAR y,u,v;
	y=u=v=i;
	table_y [y]=FLOAT2FIX(1.164f*((y>16)?(y-16):0));
	table_r [v]=FLOAT2FIX( 1.596f*(v-128));
	table_g0[v]=FLOAT2FIX(-0.813f*(v-128));
	table_g1[u]=FLOAT2FIX(-0.391f*(u-128));
	table_b [u]=FLOAT2FIX( 2.018f*(u-128));
    }
    return 0;
}

/*
 * convert YUV to RGB
 * 
 * Fri Jun  2 10:31:22 2000 By hiromasa yoshimoto 
 * 
 * @param r  
 * @param g 
 * @param b 
 * @param Y 
 * @param u 
 * @param v 
 */
static inline void
conv_YUVtoRGB(UCHAR* r, UCHAR* g, UCHAR* b,
	      UCHAR Y, UCHAR u, UCHAR v)
{
    signed int r_=FIX2INT(table_y[Y]+table_r[v]);
    signed int g_=FIX2INT(table_y[Y]+table_g0[v]+table_g1[u]);
    signed int b_=FIX2INT(table_y[Y]+table_b[u]);
    if (255<r_)
	r_=255;
    else if (r_<0)
	r_=0;
    if (255<g_)
	g_=255;
    else if (g_<0)
	g_=0;
    if (255<b_)
	b_=255;
    else if (b_<0)
	b_=0;
    
    *r=r_; 
    *g=g_; 
    *b=b_;
}

/* convert YUV444 to RGBA
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpYUV444    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV444toRGBA(RGBA* lpRGBA,const void *lpYUV444,
		  int packet_sz,int num_packet,int flag)
{
    UCHAR *p=(UCHAR*)lpYUV444;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/3;i++){      
	    UCHAR Y,u,v; 
	    u=*p++;
	    Y=*p++;
	    v=*p++; // read v(K+0)      
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b, Y, u, v);// Y(K+0)
	    lpRGBA++;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}

/*
 * convert and copy YUV422 to RGBA 
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpYUV422    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV422toRGBA(RGBA* lpRGBA, const void *lpYUV422, int packet_sz,
		  int num_packet, int flag)
{
    UCHAR *p=(UCHAR*)lpYUV422;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz/4;i++){
	    UCHAR Y,u,v;
	    u=*p++;
	    Y=*p++;
	    v=*p++;
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b, Y, u, v);
	    lpRGBA++;
	    Y=*p++;
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b, Y, u, v);
	    lpRGBA++;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}



/* convert YUV411 to RGBA 
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpYUV411    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV411toRGBA(RGBA* lpRGBA,const void *lpYUV411,
		  int packet_sz,int num_packet,int flag)
{
    UCHAR *p=(UCHAR*)lpYUV411;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/(2*3);i++){      
	    UCHAR Y,u,v; 
	    u=*p++;
	    Y=*p++;
	    v=p[1]; // read v(K+0)      
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b,
			  Y, u, v);// Y(K+0)
	    lpRGBA++;
	    Y=*p++;
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b, 
			  Y, u, v);// Y(K+1)
	    lpRGBA++;
	    p++;  // skip v(K+0)
	    Y=*p++;
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b,
			  Y, u, v);// Y(K+2)
	    lpRGBA++;
	    Y=*p++;
	    conv_YUVtoRGB(&lpRGBA->r, &lpRGBA->g, &lpRGBA->b,
			  Y, u, v);// Y(K+3)
	    lpRGBA++;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}
/* convert RGB888 to RGBA
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpRGB888    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_RGB888toRGBA(RGBA* lpRGBA,const void *lpRGB888,
		  int packet_sz,int num_packet,int flag)
{
    UCHAR *p=(UCHAR*)lpRGB888;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz;i++){      
	    lpRGBA->r=*p++;
	    lpRGBA->g=*p++;
	    lpRGBA->b=*p++;
	    lpRGBA++;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}


/* convert Y8 to RGBA 
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpY8        pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y8toRGBA(RGBA* lpRGBA,const void *lpY8,
		  int packet_sz,int num_packet,int flag)
{
    UCHAR *p=(UCHAR*)lpY8;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz;i++){      
	    lpRGBA->r = *p;
	    lpRGBA->g = *p;
	    lpRGBA->b = *p;
	    lpRGBA++;
	    p++;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}


/* convert Y16 to RGBA 
 * 
 * @param lpRGBA      pointer to destnation image data.
 * @param lpY16        pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y16toRGBA(RGBA* lpRGBA,const void *lpY16,
		  int packet_sz,int num_packet,int flag)
{
    UCHAR *p=(UCHAR*)lpY16;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/2;i++){      
	    /*
	      int  Y = *p << 8;
	      p++;
	      Y |= *p ;
	    */
	    lpRGBA->r = *p;
	    lpRGBA->g = *p;
	    lpRGBA->b = *p;
	    lpRGBA++;
	    p++;
	    // ignore lower 8 bit data.
	    p++;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}


#ifdef IPL_IMG_SUPPORTED

/*
 * convert YUV444 to IplImage
 * (This function will work, only when there is IPL.) 
 *
 * @param img         pointer to IplImage object.
 * @param lpYUV444    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 *
 * @return
 */
bool
copy_YUV444toIplImage(IplImage* img, const void *lpYUV444, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)img->imageData;
    UCHAR *p=(UCHAR*)lpYUV444;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/3;i++){      
	    UCHAR r,g,b;
	    UCHAR Y,u,v; 
	    u=*p++;
	    Y=*p++;
	    v=*p++; // read v(K+0)      
	    conv_YUVtoRGB(&r, &g, &b, Y, u, v); // Y(K+3)
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}

/*
 * convert YUV444 to IplImage
 * (This function will work, only when there is IPL.) 
 *
 * @param img         pointer to IplImage object.
 * @param lpYUV444    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 *
 * @return
 */
bool
copy_YUV444toIplImageGray(IplImage* img, const void *lpYUV444, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)img->imageData;
    UCHAR *p=(UCHAR*)lpYUV444;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/3;i++){      
	    UCHAR Y;
	    p++;
	    Y=*p++;
	    p++; // read v(K+0)      
	    //conv_YUVtoRGB(&r, &g, &b, Y, u, v); // Y(K+3)
	    *dst++=Y;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    }
    return true;
}

/*
 * convert YUV422 to IplImage. 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpYUV422    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV422toIplImage(IplImage* img, const void *lpYUV422, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpYUV422;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz/4;i++){
	    UCHAR r,g,b;
	    UCHAR Y,u,v;
	    u=*p++;
	    Y=*p++;
	    v=*p++;
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v);
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	    Y=*p++;
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v);
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert YUV422 to IplImage(gray). 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpYUV422    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV422toIplImageGray(IplImage* img, const void *lpYUV422, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpYUV422;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz/4;i++){
	    UCHAR Y;
	    p++;
	    Y=*p++;
	    p++;
	    *dst++=Y;
	    Y=*p++;
	    *dst++=Y;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert YUV411 to IplImage
 * (This function will work, only when there is IPL.) 
 * 
 * @param img         pointer to IplImage object.
 * @param lpYUV411    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV411toIplImage(IplImage* img, const void *lpYUV411, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)img->imageData;
    UCHAR *p=(UCHAR*)lpYUV411;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/(2*3);i++){      
	    UCHAR r,g,b;
	    UCHAR Y,u,v; 
	    u=*p++;
	    Y=*p++;
	    v=p[1]; // read v(K+0)      
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+0)
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	    Y=*p++;
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+1)
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	    p++;  // skip v(K+0)
	    Y=*p++;
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+2)
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	    Y=*p++;
	    conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+3)
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } 
    return true;
}

/*
 * convert YUV411 to IplImage(Gray)
 * (This function will work, only when there is IPL.) 
 * 
 * @param img         pointer to IplImage object.
 * @param lpYUV411    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_YUV411toIplImageGray(IplImage* img, const void *lpYUV411, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)img->imageData;
    UCHAR *p=(UCHAR*)lpYUV411;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (int i=0;i<packet_sz/(2*3);i++){      
	    UCHAR Y; 
	    p++;
	    Y=*p++;
	    //v=p[1]; // read v(K+0)      
	    //conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+0)
	    *dst++=Y;
	    Y=*p++;
	    //conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+1)
	    *dst++=Y;
	    p++;  // skip v(K+0)
	    Y=*p++;
	    //conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+2)
	    *dst++=Y;
	    Y=*p++;
	    //conv_YUVtoRGB(&r,&g,&b,Y,u,v); // Y(K+3)
	    *dst++=Y;
	}
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } 
    return true;
}

/*
 * convert RGB888 to IplImage. 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpRGB888    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_RGB888toIplImage(IplImage* img, const void *lpRGB888, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpRGB888;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){
	    unsigned char r=*p++;
	    unsigned char g=*p++;
	    unsigned char b=*p++;
	    *dst++=b;
	    *dst++=g;
	    *dst++=r;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert RGB888 to IplImage. 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpRGB888    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_RGB888toIplImageGray(IplImage* img, const void *lpRGB888, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpRGB888;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){
	    unsigned char r=*p++;
	    unsigned char g=*p++;
	    unsigned char b=*p++;
	    int gray = (r+g+b)/3;
	    *dst++ = gray;
	    *dst++ = gray;
	    *dst++ = gray;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert Y8 to IplImage. 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpY8    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y8toIplImage(IplImage* img, const void *lpY8, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpY8;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){	    
	    *dst++=*p;
	    *dst++=*p;
	    *dst++=*p;
	    p++;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert Y8 to IplImage (Gray)
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpY8   pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y8toIplImageGray(IplImage* img, const void *lpY8, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpY8;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){
	    *dst++ = *p++;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}


/*
 * convert Y16 to IplImage. 
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpY16    pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y16toIplImage(IplImage* img, const void *lpY16, 
		      int packet_sz,
		      int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpY16;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){	    
	    *dst++ = *p;
	    *dst++ = *p;
	    *dst++ = *p;
	    // ignore lower byte:
	    p++;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

/*
 * convert Y16 to IplImage (Gray)
 * (This function will work, only when there is IPL.)
 *
 * @param img         pointer to IplImage object.
 * @param lpY16   pointer to source image data.
 * @param packet_sz   the size of each packet.
 * @param num_packet  the number of packets per one image.
 * @param flag        REMOVE_HEADER : remove packet's  header/trailer.
 */
bool
copy_Y16toIplImageGray(IplImage* img, const void *lpY16, 
			  int packet_sz,
			  int num_packet, int flag)
{
    uchar *dst = (uchar*)(img->imageData);
    
    UCHAR *p=(UCHAR*)lpY16;
    int i;
    if (flag&REMOVE_HEADER){
	packet_sz-=8;
	p+=4;
    }
    while (num_packet-->0){
	for (i=0;i<packet_sz;i++){
	    *dst++ = *p++;
	} //     for (i=0;i<packet_sz/4;i++){
	if (flag&REMOVE_HEADER)
	    p+=4*2;
    } //   while (num_packet-->0) {
    return true;
}

#endif // #ifdef IPL_IMG_SUPPORTED


/*
 * export RGBA image to the file.
 * 
 * @param pFile   filename
 * @param img     pointer to the image.
 * @param w 
 * @param h 
 * @param fmt     file format.  currently only supports PPM.
 * 
 * @return        true or false
 */
int 
SaveRGBAtoFile(char *pFile,const RGBA* img,int w,int h,int fmt)
{
    bool result=false;
    FILE* fp=fopen(pFile,"w");
    if (fp){
	int i;
	fprintf(fp,"P6\n %d %d\n 255\n",w ,h);
	for (i=0;i<w*h;i++){
	    fwrite(&img->r,1,1,fp);
	    fwrite(&img->g,1,1,fp);
	    fwrite(&img->b,1,1,fp);
	    img++;
	}
	fclose(fp);
    }else{
	fprintf(stderr,"can't create file %s \n",pFile);
    }
    return result;
}

/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
