CXXFLAGS=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lraw1394 -L/usr/X11R6/lib -lX11  -lpopt 

CXXFLAGS+=-g

all: main-single main-dual 

main-single: xview.o ../libcam1394/1394cam.o ../libcam1394/yuv2rgb.o

main-dual: xview.o ../libcam1394/1394cam.o ../libcam1394/yuv2rgb.o


clean: 
	rm -f main-single main-dual *.o
