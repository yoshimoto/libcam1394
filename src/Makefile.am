#

lib_LTLIBRARIES = libcam1394.la
libcam1394_la_LDFLAGS = -version-info @lt_major@:@lt_revision@:@lt_age@
libcam1394_la_SOURCES = 1394cam.cc yuv2rgb.cc 

pkginclude_HEADERS = 1394cam.h yuv.h

noinst_LIBRARIES = libxview.a
libxview_a_SOURCES = xview.cc xview.h

noinst_PROGRAMS = main_single main_dual
main_single_SOURCES = main-single.cc xview.h yuv.h
main_single_LDADD = libcam1394.la libxview.a 
main_single_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11 -lpopt

main_dual_SOURCES = main-dual.cc xview.h yuv.h
main_dual_LDADD = libcam1394.la libxview.a
main_dual_LDFLAGS  = @LIBRAW1394_LIBS@ @X_LIBS@ -lX11 -lpopt
