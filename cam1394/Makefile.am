bin_PROGRAMS = cam1394  
noinst_PROGRAMS = cam1394_save view1394 dump_csr

cam1394_SOURCES  = cam1394.cc common.h xview.cc xview.h
cam1394_LDADD = ../libcam1394/libcam1394.la
cam1394_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11

cam1394_save_SOURCES  = cam1394_save.cc common.h xview.cc xview.h
cam1394_save_LDADD = ../libcam1394/libcam1394.la
cam1394_save_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11

view1394_SOURCES = view1394.cc xview.cc xview.h
view1394_LDADD = ../libcam1394/libcam1394.la
view1394_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11

dump_csr_SOURCES = dump_csr.cpp
dump_csr_LDADD = ../libcam1394/libcam1394.la
dump_csr_LDFLAGS  = @LIBRAW1394_LIBS@

CXXFLAGS = @LIBRAW1394_CPPFLAGS@
INCLUDES = -I../
