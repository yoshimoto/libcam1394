# $Id: Makefile.am,v 1.4 2004-06-20 02:45:08 yosimoto Exp $

lib_LTLIBRARIES = libcam1394.la
libcam1394_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@
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
CPPFLAGS = @LIBRAW1394_CPPFLAGS@
pkginclude_HEADERS = 1394cam.h 1394cam_registers.h yuv.h
