# $Id: Makefile.am,v 1.8 2005-02-09 09:28:48 yosimoto Exp $
AUTOMAKE_OPTIONS = foreign
SUBDIRS = libcam1394 cam1394
EXTRA_DIST = \
	sample/README sample/*.cpp sample/*.h \
	sample/Makefile.in sample/Makefile.opencv.in \
	libcam1394/Doxyfile.in \
	libcam1394.pc.in \
	libcam1394.spec \
	libcam1394.spec.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcam1394.pc
$(pkgconfig_DATA): $(top_builddir)/config.status
