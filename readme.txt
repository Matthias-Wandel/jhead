
Some notes:

    When I wrote Jhead, there wasn't much software around for looking inside
    exif headers, so I wrote jhead for that task.  Since then, a lot of much
    more sophisticated programs for looking at Exif headers have been
    written, many with GUIs, and features that Jhead lacks.

    Seeing that Jhead does everything I need it to do, My goal is not to
    have every feature imaginable.  Rather, I want Jhead to be a small,
    simple, easy to understand program.  My goal is that if you need to 
    understand Exif internsals, or add Exif capability to your program,
    Jhead is the place to cut and paste code from.

    As a result, Jhead may not have your pet feature.  Feel free to add
    your pet feature to Jhead - its meant to be hacked.  If you send
    me your change, I might integrate it, but only if its simple.  If you
    find that it dies on a certain jpeg file, send it to me, and I will
    look at it.


Compiling:

    Windows:

    Make sure visual C is on your path (I use version 6, but it shouldn't 
    matter much).
    Run the batch file make.bat

    Linux & Unices:

    type 'make'.

Portability:

    Although I have never done so myself, people tell me it compiles
    under platforms as diverse as such as Mac OS-X, or NetBSD on Mac68k.
    Jhead doesn't care about the endian-ness of your CPU, and should not
    have problems with processors that do not handle unaligned data,
    such as ARM or Alpha.  The main portability problem is the use 
    of C++ style '//' comments.  This is intentional, and won't change.

    Jhead has also made its way into various linux disributions and ports
    trees, so you might already have it on your system without knowing.
    Note that I am a windows weenie myself, although for 2.0 I am building
    the RPMs under RedHat 7.2 myself.
   

Liscence:

    Jhead is public domain software - that is, you can do whatever you want
    with it, and include it software that is licensesed under the GNU or the 
    BSD license, or whatever other licence you chose, including proprietary
    closed source licenses.  Although not part of the liscense, I do expect
    common courtesy, please.

    If you do integrate the code into some software of yours, I'd appreciate
    knowing about it though. 

Matthias Wandel

