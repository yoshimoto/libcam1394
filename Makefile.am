# $Id: Makefile.am,v 1.9 2005-11-09 10:41:29 yosimoto Exp $
AUTOMAKE_OPTIONS = foreign
SUBDIRS = libcam1394 cam1394 debian
EXTRA_DIST = \
	sample/README sample/*.cpp sample/*.h \
	sample/Makefile.in sample/Makefile.opencv.in \
	libcam1394.pc.in \
	libcam1394.spec \
	libcam1394.spec.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcam1394.pc
$(pkgconfig_DATA): $(top_builddir)/config.status
