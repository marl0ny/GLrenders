SHELL = /bin/bash

# Clang
# COMPILE = /usr/bin/clang++
# FLAGS = -O2 -Wall -g -std=c++11
# LIBS = -lm -lGL -lGLEW -lglfw

# GCC
COMPILE = /usr/bin/g++ 
FLAGS = -Og -Wall -g
OPENMP = -fopenmp
INCLUDE = ${OPENMP}
LIBS = -lm -lGL -lGLEW -lglfw -fPIC ${OPENMP}

TARGETDIR = .
TARGET = ${TARGETDIR}/program
SOURCES = program.cpp gl_wrappers.cpp fft_gl.cpp fft.hpp
OBJECTS = program.o gl_wrappers.o fft_gl.o

all: ${TARGET}

${TARGET}: ${OBJECTS}
	${COMPILE} ${FLAGS} -o $@ $^ ${LIBS}

${OBJECTS}: ${SOURCES}
	${COMPILE} ${FLAGS} -c $^ ${INCLUDE}

clean:
	rm -f *.o *~ *.gch ${TARGET}

