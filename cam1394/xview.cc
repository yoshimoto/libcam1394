/*!
  @file  xview.cc
  @brief 
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: xview.cc,v 1.3 2002-03-15 21:08:31 yosimoto Exp $
  @date
*/
//
// xview.cc - 
//
// Copyright (C) 2000 by Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>
//

#include <iostream>
using namespace std;
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <libcam1394/yuv.h>
#include "xview.h"

#define ERR(msg) cerr<< msg <<endl
#define LOG(msg) cerr<< msg <<endl

void
CXview::calc_pixel_param(UINT *wait,UINT *shift,UINT mask)
{
  *wait=0;
  while (!(mask&1)){
    mask>>=1;
    (*wait)++;
  }

  *shift=8;
  while (mask&1){
    mask>>=1;
    (*shift)--;
  }
}


bool
CXview::CreateWindow(int w,int h,char* strCaption)
{
  width=w;
  height=h;

  display = XOpenDisplay (NULL);
  if (display == NULL) {
    ERR("can't open X11 display.");
    return -1;
  }

  view_window = XCreateSimpleWindow (display, 
				     DefaultRootWindow(display), 
				     0, 0, 
				     width, height, 
				     0,1,
				     0);
  
  
  long event_mask = ExposureMask | KeyPressMask | ButtonPressMask;
  XSelectInput (display, view_window, event_mask);
  XMapWindow (display, view_window); 
  XStoreName (display, view_window, strCaption); 

  // create GC.
  XGCValues values;
  values.function = GXcopy;  
  values.graphics_exposures = False; // exposures events are not generated.
  gc  = XCreateGC(display,view_window,
		  GCFunction|GCGraphicsExposures,
		  &values);
  
  // set a window's WM_HINTS property 
  XWMHints wmhints;
  wmhints.input = True; 
  wmhints.flags = InputHint;
  XSetWMHints(display,view_window,&wmhints);

  // 
  int depth= DefaultDepth(display,DefaultScreen(display));

  image=XCreateImage(display,
		     DefaultVisual(display,DefaultScreen(display)),
		     depth,
		     ZPixmap,
		     0, // x offset
		     NULL,
		     width,
		     height,
		     8,    
		     0);    // bytes_per_line auto calculate

  if (NULL==image){
    ERR("can't allocate image.");
    return false;
  }
  bytes_per_pixel = image->bits_per_pixel / 8;

//  LOG( depth << " depth /"<< image->bits_per_pixel <<" bpp mode "
//       << image->bytes_per_line << " bytes/line");
//  LOG( hex
//       << image->red_mask <<" "
//       << image->green_mask <<" "
//       << image->blue_mask <<dec);
  
  calc_pixel_param(&r_wait,&r_shift,image->red_mask);
  calc_pixel_param(&g_wait,&g_shift,image->green_mask);
  calc_pixel_param(&b_wait,&b_shift,image->blue_mask);
//  LOG(    r_shift << " "<<hex<<r_wait<<" / "<<
//	  g_shift << " "<<hex<<g_wait<<" / "<<
//	  b_shift << " "<<hex<<b_wait<<dec);
  
  image->byte_order=LSBFirst;

  image->data=new char[image->bytes_per_line*height];
  if (!image->data){
    LOG("failure @ allocate image->data ");
  }
  image->byte_order=LSBFirst;
  

  XSync(display,False);

}

bool
CXview::UpDate(RGBA* src)
{

  unsigned char *dst=(unsigned char*)image->data;
  int i;
  for (i=0;i<width*height;i++){
    *((unsigned int*)dst)=
      (( src->r>>r_shift )<<r_wait) | 
      (( src->g>>g_shift )<<g_wait) |
      (( src->b>>b_shift )<<b_wait) ;	
    dst += bytes_per_pixel ; // div 8
    src++;
  }      

  XPutImage(display,view_window,gc,image,0,0,
	    0,0,width,height);
  XSync(display,False);

  return false;
}


CXview::CXview():
  image(NULL),display(NULL),gc(NULL),
  width(640),height(480)
{
  
}

CXview::~CXview()
{
  XDestroyImage(image); 
  XFreeGC(display,gc);
  XCloseDisplay(display);
}
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
