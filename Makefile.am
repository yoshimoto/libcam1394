# $Id: Makefile.am,v 1.7 2004-08-30 02:00:37 yosimoto Exp $
AUTOMAKE_OPTIONS = foreign
SUBDIRS = libcam1394 cam1394
EXTRA_DIST = \
	sample/README sample/*.cpp sample/*.h \
	sample/Makefile.in sample/Makefile.opencv.in \
	libcam1394/Doxyfile.in \
	libcam1394.pc.in \
	libcam1394.spec.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcam1394.pc
$(pkgconfig_DATA): $(top_builddir)/config.status