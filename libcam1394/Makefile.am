# $Id: Makefile.am,v 1.6 2005-11-09 10:41:29 yosimoto Exp $

EXTRA_DIST = Doxyfile.in


lib_LTLIBRARIES = libcam1394.la
libcam1394_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@
libcam1394_la_CXXFLAGS = @LIBRAW1394_CFLAGS@ @OPENCV_CFLAGS@
if HAVE_JUJU
libcam1394_la_CXXFLAGS += -DHAVE_JUJU
endif
if HAVE_VIDEO1394
libcam1394_la_CXXFLAGS += -DHAVE_VIDEO1394
endif
if HAVE_ISOFB
libcam1394_la_CXXFLAGS += -DHAVE_ISOFB
endif
libcam1394_la_LIBADD   = @LIBRAW1394_LIBS@
libcam1394_la_SOURCES = \
	1394cam.cc \
	yuv2rgb.cc \
	1394cam.h \
	1394cam_registers.h \
	yuv.h \
	common.h \
	video1394.h \
	ieee1394-ioctl.h \
	drv_video1394.cc \
	drv_isofb.cc \
	drv_juju.cc \
	1394cam_drv.cc \
	1394cam_drv.h

LIBS =
pkginclude_HEADERS = 1394cam.h 1394cam_registers.h yuv.h
