/*!
  @file  xview.h
  @brief 
  @author  YOSHIMOTO,Hiromasa <yosimoto@limu.is.kyushu-u.ac.jp>
  @version $Id: xview.h,v 1.3 2002-11-25 12:51:35 yosimoto Exp $
  @date    $Date: 2002-11-25 12:51:35 $
 */
// xview.h - 
//
// Copyright (C) 2000 by Hiromasa Yoshimoto <yosimoto@limu.is.kyushu-u.ac.jp>
// 

#if !defined(_XVIEW_H_INCLUDED_)
#define _XVIEW_H_INCLUDED_

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
  
    bool CreateWindow(int width,int height,char* strCaption);
    bool UpDate(RGBA*);
};

#endif //#if !defined(_XVIEW_H_INCLUDED_)
/*
 * Local Variables:
 * mode:c++
 * c-basic-offset: 4
 * End:
 */
