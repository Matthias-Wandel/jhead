#--------------------------------
# jhead makefile for Unix
#--------------------------------
PREFIX=$(DESTDIR)/usr/local
BINDIR=$(PREFIX)/bin
DOCDIR=$(PREFIX)/share/doc/jhead
MANDIR=$(PREFIX)/share/man/man1
OBJ=obj
SRC=.

DPKG_BUILDFLAGS := $(shell command -v dpkg-buildflags 2> /dev/null)
ifdef DPKG_BUILDFLAGS
CFLAGS:=$(shell dpkg-buildflags --get CFLAGS)
LDFLAGS:=$(shell dpkg-buildflags --get LDFLAGS)
endif

# To enable electric fence, set ELECTRIC_FENCE=1
ifeq ($(ELECTRIC_FENCE),1)
CFLAGS += -fsanitize=address
LDFLAGS += -fsanitize=address
endif

all: objdir jhead

docs = $(SRC)/usage.html

objdir:
	@mkdir -p obj

objs = $(OBJ)/jhead.o $(OBJ)/jpgfile.o $(OBJ)/jpgqguess.o $(OBJ)/paths.o \
	$(OBJ)/exif.o $(OBJ)/iptc.o $(OBJ)/gpsinfo.o $(OBJ)/makernote.o

$(OBJ)/%.o:$(SRC)/%.c objdir
	${CC} $(CFLAGS) $(CPPFLAGS) -c $< -o $@

jhead: $(objs) jhead.h
	${CC} $(LDFLAGS) -o jhead $(objs) -lm

clean:
	rm -f $(objs) jhead

install: all
	install -d $(BINDIR) $(DOCDIR) $(MANDIR)
	install -m 0755 jhead $(BINDIR)
	install -m 0644 $(docs) $(DOCDIR)
	install -m 0644 jhead.1 $(MANDIR)
