/*!
  @file  yuv2rgb.cc
  @brief convert YUV to RGBA
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: yuv2rgb.cc,v 1.2 2002-02-12 14:41:02 yosimoto Exp $
  @date    $Date: 2002-02-12 14:41:02 $
 */
//
// yuv2rgb.cc - convert YUV to RGBA
//
// by hiromasa yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp> 
//
// Sun Jan 30 03:13:43 2000 By hiromasa yoshimoto  
// Fri Jun  2 10:09:42 2000 By hiromasa yoshimoto  エンディアンの変更

#include <stdio.h>

#include "yuv.h"

/* yuv -> RGBA 変換を整数演算で行ない高速化をはかっています */
typedef signed int FIX ; /*  32bit整数を固定小数点として利用しています */
#define FIX_BASE  10  
#define FLOAT2FIX(f)  (FIX)(f*(float)(1<<FIX_BASE))
#define FIX2INT(F)    ((F)>>FIX_BASE)

static  FIX table_y [256];  // Y
static  FIX table_r [256];  // u
static  FIX table_g0[256];  // u
static  FIX table_g1[256];  // v
static  FIX table_b [256];  // v

/* YUV->RGBA の積和計算のためのテーブルの計算
 * 最初に一度だけ呼び出す必要があります。
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

/* yuv->rgbaへの計算を実際に行なう
   この部分はMMX化すると良いかも知れません  */
// Fri Jun  2 10:31:22 2000 By hiromasa yoshimoto 
static inline void
conv_YUVtoRGBA(RGBA* p,UCHAR Y,UCHAR u,UCHAR v)
{
  signed int r=FIX2INT(table_y[Y]+table_r[v]);
  signed int g=FIX2INT(table_y[Y]+table_g0[v]+table_g1[u]);
  signed int b=FIX2INT(table_y[Y]+table_b[u]);
  if (255<r)
    r=255;
  else if (r<0)
    r=0;
  if (255<g)
    g=255;
  else if (g<0)
    g=0;
  if (255<b)
    b=255;
  else if (b<0)
    b=0;
  p->r=r;p->g=g;p->b=b;
}

/* YUV422 -> RGBA への変換
 *
 * flag
 *  REMOVE_HEADER : remove packet's  header/trailer.
 */
void
copy_YUV422toRGBA(RGBA* lpRGBA,const void *lpYUV422,int packet_sz,
		  int num_packet,int flag)
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
      conv_YUVtoRGBA(lpRGBA,Y,u,v);
      lpRGBA++;
      Y=*p++;
      conv_YUVtoRGBA(lpRGBA,Y,u,v);
      lpRGBA++;
    } //     for (i=0;i<packet_sz/4;i++){
    if (flag&REMOVE_HEADER)
      p+=4*2;
  } //   while (num_packet-->0) {
}

/* YUV(4:4:4) -> RGBA への変換
 * 
 *
 * flag
 *  REMOVE_HEADER : remove packet's  header/trailer.
 */
void
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
      conv_YUVtoRGBA(lpRGBA,Y,u,v); // Y(K+0)
      lpRGBA++;
    }
    if (flag&REMOVE_HEADER)
      p+=4*2;
  }
}


/* YUV(4:1:1) -> RGBA への変換
 * 
 *
 * flag
 *  REMOVE_HEADER : remove packet's  header/trailer.
 */
void
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
      conv_YUVtoRGBA(lpRGBA,Y,u,v); // Y(K+0)
      lpRGBA++;
      Y=*p++;
      conv_YUVtoRGBA(lpRGBA,Y,u,v); // Y(K+1)
      lpRGBA++;
      p++;  // skip v(K+0)
      Y=*p++;
      conv_YUVtoRGBA(lpRGBA,Y,u,v); // Y(K+2)
      lpRGBA++;
      Y=*p++;
      conv_YUVtoRGBA(lpRGBA,Y,u,v); // Y(K+3)
      lpRGBA++;
    }
    if (flag&REMOVE_HEADER)
      p+=4*2;
  }
}

/* ファイルへの吐きだし。現時点では ppm format のみサポートです
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
