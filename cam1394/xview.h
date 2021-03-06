/*!
  \file   $Id: xview.h,v 1.7 2005-11-09 10:41:29 yosimoto Exp $
  \author YOSHIMOTO Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  \date   Sat Aug 31 03:58:46 2002
  
  \brief    
*/

#if !defined(_XVIEW_H_INCLUDED_)
#define _XVIEW_H_INCLUDED_

#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef struct _IplImage IplImage;

class CXview{
     typedef unsigned int UINT;
protected:

     UINT r_wait,g_wait,b_wait;    
     UINT r_shift,g_shift,b_shift;

     Display *display;
     Window  view_window;
  
     int width,height;   // video width , height
     GC  gc;
     XImage *image;

     int bytes_per_pixel;

protected:
     void calc_pixel_param(UINT *wait,UINT *shift,UINT mask);
  
public:
     CXview();
     virtual ~CXview();
  
     bool CreateWindow(int width,int height,const char* strCaption);
     bool UpDate(RGBA*);
     bool UpDate(IplImage*);

     Display *GetDisplay() const;
     Window  GetWindow() const;
};

#endif //#if !defined(_XVIEW_H_INCLUDED_)
// end of [xview.h]
