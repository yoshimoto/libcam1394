# $Id: Makefile.am,v 1.5 2004-08-29 08:54:34 yosimoto Exp $

lib_LTLIBRARIES = libcam1394.la
libcam1394_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@
libcam1394_la_CXXFLAGS = @LIBRAW1394_CFLAGS@
libcam1394_la_LIBADD   = @LIBRAW1394_LIBS@
libcam1394_la_SOURCES = \
	1394cam.cc \
	yuv2rgb.cc \
	1394cam.h \
	1394cam_registers.h \
	yuv.h \
	common.h \
	video1394.h \
	ieee1394-ioctl.h

LIBS =
pkginclude_HEADERS = 1394cam.h 1394cam_registers.h yuv.h
