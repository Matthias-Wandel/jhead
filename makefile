#--------------------------------
# jhead makefile for Unix
#--------------------------------

OBJ=.
SRC=.

all: jhead

objs = $(OBJ)/jhead.o $(OBJ)/jpgfile.o $(OBJ)/exif.o

$(OBJ)/%.o:$(SRC)/%.c
	cc -O3 -Wall -c $< -o $@

jhead: $(objs) jhead.h
	cc -o jhead $(objs) -lm

