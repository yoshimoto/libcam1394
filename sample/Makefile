CXXFLAGS=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lraw1394 -L/usr/X11R6/lib -lX11  -lpopt 

CXXFLAGS+=-g

all: main-single main-dual main-caminfo

main-single: xview.o /usr/local/lib/libcam1394.a

main-dual: xview.o /usr/local/lib/libcam1394.a

main-caminfo: xview.o ../src/.libs/libcam1394.a

clean: 
	rm -f main-single main-dual *.o
