# $Id: Makefile.am,v 1.11 2004-06-20 02:45:07 yosimoto Exp $

bin_PROGRAMS = cam1394  
noinst_PROGRAMS = cam1394_save view1394 dump_csr

cam1394_SOURCES  = cam1394.cc common.h xview.cc xview.h
cam1394_LDADD = ../libcam1394/libcam1394.la @X_LIBS@ @X_PRE_LIBS@ -lX11
cam1394_LDFLAGS  = 

cam1394_save_SOURCES  = cam1394_save.cc common.h xview.cc xview.h
cam1394_save_LDADD = ../libcam1394/libcam1394.la @X_LIBS@ @X_PRE_LIBS@ -lX11
cam1394_save_LDFLAGS  = 

view1394_SOURCES = view1394.cc xview.cc xview.h
view1394_LDADD = ../libcam1394/libcam1394.la @X_LIBS@ @X_PRE_LIBS@ -lX11
view1394_LDFLAGS  = 

dump_csr_SOURCES = dump_csr.cpp
dump_csr_LDADD = ../libcam1394/libcam1394.la 
dump_csr_LDFLAGS  = 

CPPFLAGS = @LIBRAW1394_CPPFLAGS@
INCLUDES = -I../
