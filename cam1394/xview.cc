/**
   @file   $Id: xview.cc,v 1.7 2003-01-08 18:38:01 yosimoto Exp $
   @author YOSHIMOTO Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
   @date   Sat Aug 31 03:58:46 2002
   @brief    
*/
#include "config.h"

#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <libcam1394/yuv.h>

#if defined HAVE_CV_H
#include <cv.h>
#endif
#if defined HAVE_OPENCV_CV_H
#include <opencv/cv.h>
#endif

#include "xview.h"

using namespace std;

void CXview::calc_pixel_param(UINT *wait,UINT *shift,UINT mask)
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

//! 
/*! 
  
\param w 
\param h 
\param strCaption 

\return
*/
bool CXview::CreateWindow(int w,int h,const char* strCaption)
{
    width =w;
    height=h;

    display = XOpenDisplay (NULL);
    if (display == NULL) {
	//ERR("can't open X11 display.");
	return false;
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
	//ERR("can't allocate image.");
	return false;
    }
    this->bytes_per_pixel = image->bits_per_pixel / 8;

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
	//LOG("failure @ allocate image->data ");
    }
    image->byte_order=LSBFirst;
  

    XSync(display,False);
     
    return true;
}

//! combines an image with CXview 
/*! 
  
\param src 

\return 
*/
bool CXview::UpDate(RGBA* src)
{
     unsigned char *dst=(unsigned char*)image->data;
     int i;
     for (i=0;i<width*height;i++){
	  *((unsigned int*)dst)=
	       (( src->r>>r_shift )<<r_wait) | 
	       (( src->g>>g_shift )<<g_wait) |
	       (( src->b>>b_shift )<<b_wait) ;	
	  dst += this->bytes_per_pixel ; // div 8
	  src++;
     }      

     XPutImage(display,view_window,gc,image,0,0,
	       0,0,width,height);
     XSync(display,False);

     return true;
}


#if defined OPENCVAPI
//! combines an image with CXview
/*! 
  
\param src 

\return 
*/
bool CXview::UpDate(IplImage *src)
{
     const int bpp = ((src->depth & 255) >> 3 ) * src->nChannels;
     const int H = src->height;
     const int W = src->width;

     unsigned char *dst=(unsigned char*)image->data;
     unsigned char *data=(unsigned char*)src->imageData;
     int x,y;
     for (y=0; y<H; y++){
	  uchar *p = data;
	  for (x=0; x<W; x++){
	       *((unsigned int*)dst)=
		    (( p[2]>>r_shift )<<r_wait) | 
		    (( p[1]>>g_shift )<<g_wait) |
		    (( p[0]>>b_shift )<<b_wait) ;	
	       dst += this->bytes_per_pixel ; // div 8
	       p   += bpp;
	  }
	  data += src->widthStep;
     }      
     
     XPutImage(display,view_window,gc,image,0,0,
	       0,0,width,height);
     XSync(display,False);

     return true;
}
#endif // #if defined OPENCVAPI


CXview::CXview():
    image(NULL),display(NULL),gc(NULL),
    width(640),height(480)
{
  
}

CXview::~CXview()
{
    if (image)
	XDestroyImage(image); 
    if (gc)
	XFreeGC(display,gc);
    if (display)
	XCloseDisplay(display);
}

//! returns a Display structure.
/*! The GetDisplay function returns a Display structure that serves as
  the connection to the X server and that contains all the
  information about that X server.

\return display structure
*/
Display* CXview::GetDisplay() const
{
    return display;
}

//! returns a window ID of the CXview's window.
/*! 

\return 
*/
Window CXview::GetWindow() const
{
    return view_window;
}

/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
