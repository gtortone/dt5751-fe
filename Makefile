####################################################################
#
#  Name:         Makefile
#  Created by:   Thomas Lindner
#
#  Contents:     Makefile for the v1725 frontend
#
#  Copied from DEAP frontend
#
#####################################################################
#

# Path to gcc 4.8.1 binaries (needed to use new C++ stuff)
#PATH := /home/deap/packages/newgcc/bin:$(PATH)

# Hardware setup
NBLINKSPERA3818=4 # Number of optical links used per A3818
NBLINKSPERFE=4 # Number of optical links controlled by each frontend
NB1725PERLINK=1 # Number of daisy-chained v1725s per optical link
NBV1725TOTAL=4 # Number of v1725 boards in total
NBCORES=12
USE_SYSTEM_BUFFER=1

HWFLAGS = -DUSE_SYSTEM_BUFFER=$(USE_SYSTEM_BUFFER) \
-DNv1725=$(Nv1725) -DNBLINKSPERA3818=$(NBLINKSPERA3818) -DNBLINKSPERFE=$(NBLINKSPERFE) \
-DNB1725PERLINK=$(NB1725PERLINK) -DNBV1725TOTAL=$(NBV1725TOTAL) -DNBCORES=$(NBCORES)


#--------------------------------------------------------------------
# The MIDASSYS should be defined prior the use of this Makefile
ifndef MIDASSYS
missmidas::
	@echo "...";
	@echo "Missing definition of environment variable 'MIDASSYS' !";
	@echo "...";
endif

#--------------------------------------------------------------------
# The following lines contain specific switches for different UNIX
# systems. Find the one which matches your OS and outcomment the 
# lines below.
#
# get OS type from shell
OSTYPE = $(shell uname)

#-----------------------------------------
# This is for Linux
ifeq ($(OSTYPE),Linux)
OSTYPE = linux
endif

ifeq ($(OSTYPE),linux)
#OS_DIR = linux-m64
OS_DIR = linux
OSFLAGS = -DOS_LINUX -DLINUX
CFLAGS = -g -Wall -pthread $(HWFLAGS)
#For backtrace
#CFLAGS = -g -Wall -pthread $(HWFLAGS) -rdynamic -fno-omit-frame-pointer -fno-inline -fno-inline-functions
LDFLAGS = -g -lm -lz -lutil -lnsl -lpthread -lrt -lc 
endif

#-----------------------------------------
# optimize?

CFLAGS += -O2

#-----------------------------------------
# ROOT flags and libs
#
ifdef ROOTSYS
ROOTCFLAGS := $(shell  $(ROOTSYS)/bin/root-config --cflags)
ROOTCFLAGS += -DHAVE_ROOT -DUSE_ROOT -I/Users/lindner/packages/CAENComm-1.2/include/
ROOTLIBS   := $(shell  $(ROOTSYS)/bin/root-config --libs) -Wl,-rpath,$(ROOTSYS)/lib
ROOTLIBS   += -lThread
else
missroot:
	@echo "...";
	@echo "Missing definition of environment variable 'ROOTSYS' !";
	@echo "...";
endif
#-------------------------------------------------------------------
#-------------------------------------------------------------------
# The following lines define directories. Adjust if necessary
#
# Expect the CAENCOMM and CAENVME to be installed system-wide
# using the libCAENComm.so, libCAENVME.so
#
# CONET2_DIR   = $(HOME)/packages/CONET2
# CAENCOMM_DIR = $(CONET2_DIR)/CAENComm-1.02
# CAENCOMM_LIB = $(CAENCOMM_DIR)/lib/x64
# CAENVME_DIR  = $(CONET2_DIR)/CAENVMELib-2.30.2
# CAENVME_DIR  = $(CONET2_DIR)/CAENVMELib-2.41
# CAENVME_LIB  = $(CAENVME_DIR)/lib/x64
MIDAS_INC    = $(MIDASSYS)/include
MIDAS_LIB    = $(MIDASSYS)/$(OS_DIR)/lib
MIDAS_SRC    = $(MIDASSYS)/src
MIDAS_DRV    = $(MIDASSYS)/drivers/vme
ROOTANA      = $(HOME)/packages/rootana

####################################################################
# Lines below here should not be edited
####################################################################
#
# compiler
CC   = gcc # -std=c99
#CXX  = g++ -std=c++11 -v
CXX  = g++ -std=c++11
#
# MIDAS library
LIBMIDAS=-L$(MIDAS_LIB) -lmidas
#
# CAENComm
#LIBCAENCOMM=-L$(CAENCOMM_LIB) -lCAENComm
LIBCAENCOMM=-lCAENComm
#
# CAENVME
#LIBCAENVME=-L$(CAENVME_LIB) -lCAENVME
LIBCAENVME=-lCAENVME

# ZMQ
LIBZMQ=-lzmq

# ALLLIBS
LIBALL= $(LIBMIDAS) $(LIBCAENCOMM) $(LIBCAENVME) $(LIBZMQ)

#
# All includes
# INCS = -I. -I./include -I$(MIDAS_INC) -I$(MIDAS_DRV) -I$(CAENVME_DIR)/include -I$(CAENCOMM_DIR)/include
INCS = -I. -I./include -I$(MIDAS_INC) -I$(MIDAS_DRV) -I/Users/lindner/packages/CAENComm-1.2/include/ -I/Users/lindner/packages/CAENVMELib-2.50/include/

####################################################################
# General commands
####################################################################

all: fe
	@echo "***** Finished"
	@echo "***** Use 'make doc' to build documentation"

fe : feoV1725mt.exe

doc ::
	doxygen
	@echo "***** Use firefox --no-remote doc/html/index.html to view if outside gateway"

####################################################################
# Libraries/shared stuff
####################################################################

ov1725.o : ov1725.c
	$(CC) -c $(CFLAGS) $(INCS) $< -o $@ 

####################################################################
# Single-thread frontend
####################################################################

feoV1725mt.exe: $(MIDAS_LIB)/mfe.o  feoV1725.o ov1725.o v1725CONET2.o
	$(CXX) $(OSFLAGS) feoV1725.o v1725CONET2.o ov1725.o $(MIDAS_LIB)/mfe.o $(LIBALL) -o $@ $(LDFLAGS)

feoV1725.o : feoV1725.cxx v1725CONET2.o
	$(CXX) $(CFLAGS) $(OSFLAGS) $(INCS) -I. -Ife -c $< -o $@

v1725CONET2.o : v1725CONET2.cxx
	$(CXX) $(CFLAGS) $(OSFLAGS) $(INCS) -Ife -c $< -o $@

$(MIDAS_LIB)/mfe.o:
	@cd $(MIDASSYS) && make
####################################################################
# Clean
####################################################################

clean:
	rm -f *.o *.exe
	rm -f *~
	rm -rf html
	rm -rf stress

####################################################################
# Stress test program
####################################################################
stress: stress_test.c
	$(CC) $(CFLAGS) $(INCS) -o $@ $(LDFLAGS) $< $(LIBCAENCOMM) $(LIBCAENVME)

setcards: setcards.cxx
	$(CXX) $(CFLAGS) $(INCS) -o $@ $(LDFLAGS) $< $(LIBCAENCOMM) $(LIBCAENVME)

#end file

