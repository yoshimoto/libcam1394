# $Id: Makefile,v 1.11 2003-03-13 11:27:10 yosimoto Exp $
# $Author: yosimoto $

CXXFLAGS+=-I/usr/local/include
LDFLAGS+=-lstdc++ -L/usr/X11R6/lib -lX11  -lpopt 
LDFLAGS+=-L/usr/local/lib -lraw1394 -lcam1394

CXXFLAGS+=-g

all: main-single main-dual 

main-single: main-single.o xview.o

main-dual: main-dual.o xview.o

clean: 
	rm -f main-single main-dual *.o
