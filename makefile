#--------------------------------
# jhead makefile for Unix
#--------------------------------
OBJ=obj
SRC=.

CFLAGS:=$(shell dpkg-buildflags --get CFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)

# To enable electric fence:
#CFLAGS:=$(shell dpkg-buildflags --get CFLAGS) -fsanitize=address
#LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS) -fsanitize=address


all: objdir jhead

objdir:
	@mkdir -p obj

objs = $(OBJ)/jhead.o $(OBJ)/jpgfile.o $(OBJ)/jpgqguess.o $(OBJ)/paths.o \
	$(OBJ)/exif.o $(OBJ)/iptc.o $(OBJ)/gpsinfo.o $(OBJ)/makernote.o

$(OBJ)/%.o:$(SRC)/%.c 
	${CC} $(CFLAGS) $(CPPFLAGS) -c $< -o $@

jhead: $(objs) jhead.h
	${CC} $(LDFLAGS) -o jhead $(objs) -lm

clean:
	rm -f $(objs) jhead

install:
	mkdir -p ${DESTDIR}/usr/bin/
	cp jhead ${DESTDIR}/usr/bin/
