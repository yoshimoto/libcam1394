bin_PROGRAMS = cam1394 cam1394_save

cam1394_SOURCES  = cam1394.cc mmx_bgs.s common.h xview.h
cam1394_LDADD = ../src/libcam1394.la ../src/libxview.a
cam1394_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11 

cam1394_save_SOURCES  = cam1394_save.cc common.h xview.h
cam1394_save_LDADD = ../src/libcam1394.la ../src/libxview.a
cam1394_save_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11 

CXXFLAGS = @LIBRAW1394_CPPFLAGS@
INCLUDES = -I../src
