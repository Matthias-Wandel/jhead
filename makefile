#--------------------------------
# jhead makefile for Unix
#--------------------------------
OBJ=.
SRC=.

all: jhead

objs = $(OBJ)/jhead.o $(OBJ)/jpgfile.o $(OBJ)/exif.o $(OBJ)/iptc.o $(OBJ)/gpsinfo.o $(OBJ)/makernote.o 

$(OBJ)/%.o:$(SRC)/%.c
	${CC} -O3 -Wall -c $< -o $@

jhead: $(objs) jhead.h
	${CC} -o jhead $(objs) -lm
