CXXFLAGS=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lraw1394 -L/usr/X11R6/lib -lX11  -lpopt 

all: main-single main-dual main-bgs

main-single: xview.cc /usr/local/lib/libcam1394.a

main-bgs: xview.cc /usr/local/lib/libcam1394.a

main-dual: xview.cc /usr/local/lib/libcam1394.a

clean: 
	rm -f main-single main-dual *.o
