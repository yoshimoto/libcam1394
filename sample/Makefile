CPPFLAGS=-I/usr/local/include
LDFLAGS+=-L/usr/local/lib -lraw1394 -lcam1394 -L/usr/X11R6/lib -lX11

CPPFLAGS+=-g

TARGET= main-single main-dual main-all

all: $(TARGET)

main-single: xview.o 

main-dual: xview.o

main-all: xview.o 

clean: 
	rm -f $(TARGET) *.o
