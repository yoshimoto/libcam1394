CPPFLAGS+=-I/usr/local/include
LDFLAGS+=-lstdc++ -L/usr/X11R6/lib -lX11  -lpopt 
LDFLAGS+=-L/usr/local/lib -lraw1394 -lcam1394

CPPFLAGS+=-g

all: main-single main-dual 

main-single: main-single.o xview.o

main-dual: main-dual.o xview.o


clean: 
	rm -f main-single main-dual *.o
