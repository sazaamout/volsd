
CC=g++
CPPFLAGS=-Wall

OBJDIR=obj
SRCDIR=src
BINDIR=bin
LIBDIR=lib

all: dispatcher 


dispatcher: Disks.o ServerSocket.o Socket.o Utils.o Logger.o 
	g++ $(SRCDIR)/dispatcher.cpp $(OBJDIR)/Disks.o $(OBJDIR)/ServerSocket.o $(OBJDIR)/Socket.o $(OBJDIR)/Utils.o $(OBJDIR)/Logger.o -std=c++0x -pthread -o $(BINDIR)/dispatcher



Disks.o : $(LIBDIR)/Disks.cpp $(LIBDIR)/Disks.h 
	g++ -c $(LIBDIR)/Disks.cpp -std=c++0x -o $(OBJDIR)/Disks.o

Utils.o : $(LIBDIR)/Utils.cpp $(LIBDIR)/Utils.h
	g++ -c $(LIBDIR)/Utils.cpp -o $(OBJDIR)/Utils.o 

Logger.o : $(LIBDIR)/Logger.cpp $(LIBDIR)/Logger.h
	g++ -c $(LIBDIR)/Logger.cpp -o $(OBJDIR)/Logger.o

ServerSocket.o : $(LIBDIR)/ServerSocket.cpp $(LIBDIR)/ServerSocket.h
	g++ -c $(LIBDIR)/ServerSocket.cpp -o $(OBJDIR)/ServerSocket.o

ClientSocket.o : $(LIBDIR)/ClientSocket.cpp $(LIBDIR)/ClientSocket.h
	g++ -c $(LIBDIR)/ClientSocket.cpp -o $(OBJDIR)/ClientSocket.o

Socket.o : $(LIBDIR)/Socket.cpp $(LIBDIR)/Socket.h
	g++ -c $(LIBDIR)/Socket.cpp -o $(OBJDIR)/Socket.o

clean:
	rm -f $(OBJDIR)/*.o  $(BINDIR)/*


