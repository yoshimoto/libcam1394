CXXFLAGS=-I/usr/local/include
LDFLAGS=-L/usr/local/lib -lraw1394 -lcam1394 -L/usr/X11R6/lib -lX11 

all: main-single main-dual

main-single: xview.cc

main-dual: xview.cc

clean: 
	rm -f main-single main-dual *.o *~


