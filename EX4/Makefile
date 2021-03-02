CC=g++
CXX=g++
LD=g++

EXESRC1=whatsappClient.cpp whatsappio.cpp 
EXESRC2=whatsappServer.cpp whatsappio.cpp 
EXEOBJ1=$(EXESRC1:.cpp=.o)
EXEOBJ2=$(EXESRC2:.cpp=.o)
CPPFILES=whatsappServer.cpp whatsappClient.cpp whatsappio.cpp 
HFILES=$(CPPFILES:.cpp=.h)
INCS=-I.
CFLAGS = -Wall -std=c++11 -g $(INCS)
CXXFLAGS = -Wall -std=c++11 -g $(INCS)


EXE1 = whatsappClient
EXE2 = whatsappServer


TAR=tar
TARFLAGS=-cvf
TARNAME=ex4.tar
TARSRCS=$(CPPFILES) $(HFILES) Makefile README

all: $(EXE1) $(EXE2)

$(EXE1): $(EXEOBJ1)
	$(LD) $(CXXFLAGS) $(EXEOBJ1) -o $(EXE1)


$(EXE2): $(EXEOBJ2)
	$(LD) $(CXXFLAGS) $(EXEOBJ2) -o $(EXE2)

clean:
	$(RM) $(EXE1) $(EXE2) $(EXEOBJ1) $(EXEOBJ2) *~ *core

depend:
	makedepend -- $(CFLAGS) -- $(SRC) $(LIBSRC)

tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
