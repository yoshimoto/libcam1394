# $Id: Makefile.am,v 1.15 2005-10-11 05:29:13 yosimoto Exp $

bin_PROGRAMS = cam1394  
noinst_PROGRAMS = dump_csr

cam1394_SOURCES  = cam1394.cc common.h xview.cc xview.h
cam1394_CXXFLAGS = @LIBRAW1394_CFLAGS@ @OPENCV_CFLAGS@
cam1394_LDADD = ../libcam1394/libcam1394.la @X_PRE_LIBS@ @X_LIBS@ -lX11 @OPENCV_LIBS@ @LIBRAW1394_LIBS@
cam1394_LDFLAGS  = 

dump_csr_SOURCES = dump_csr.cpp
dump_csr_CXXFLAGS = @LIBRAW1394_CFLAGS@
dump_csr_LDADD = ../libcam1394/libcam1394.la  @LIBRAW1394_LIBS@ 
dump_csr_LDFLAGS  = 

INCLUDES = -I../
