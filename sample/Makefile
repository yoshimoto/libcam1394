CXXFLAGS=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lraw1394 -lcam1394 -L/usr/X11R6/lib -lX11  -lpopt 
LDFLAGS+=-lopencv -lcvvis

CXXFLAGS+=-g

all: main-single main-dual  main-with-opencv

main-single: xview.o

main-dual: xview.o

clean: 
	rm -f main-single main-dual *.o
