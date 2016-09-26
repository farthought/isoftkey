CXX		= g++
CFLAGS	= -g -O2 -Wall -fPIC -fpermissive -std=c++11 
CPPPATH	=
LIBPATH	=
LIBS	= -lX11 
all: isoftkey

isoftkey: main.cpp
	$(CXX) -o xactivewin.o -c $(CFLAGS) $(CPPPATH) main.cpp
	$(CXX) -o isoftkey xactivewin.o $(LIBPATH) $(LIBS)

clean: 
	rm -rf *.o isoftkey
