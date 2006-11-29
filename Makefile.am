# $Id: Makefile.am,v 1.10 2006-11-29 10:06:10 yosimoto Exp $
AUTOMAKE_OPTIONS = gnu
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
