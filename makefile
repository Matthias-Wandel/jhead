#--------------------------------
# makehtml makefile
#--------------------------------

OBJ=.
SRC=.

objsmakehtml = $(OBJ)/imagegather.o $(OBJ)/crosslink.o $(OBJ)/main.o $(OBJ)/makehtml.o \
               $(OBJ)/updatecache.o $(OBJ)/utility.o $(OBJ)/collectdir.o

all: makehtml jhead

jhead: jhead.c exif.c
	cc -O3 -Wall -lm -o jhead jhead.c exif.c
	cp jhead $(HOME)/bin

$(objsmakehtml) : makehtml.h

$(OBJ)/%.o:$(SRC)/%.c
	cc -O3 -Wall -c $< -o $@

makehtml: $(objsmakehtml)
	cc -o makehtml $(objsmakehtml)
	cp makehtml $(HOME)/bin


clean :
	rm -f $(objsmakehtml)
