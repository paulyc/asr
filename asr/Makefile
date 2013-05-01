CC=g++
CFLAGS=-c -Wall -I../fftw-3.3.3 -I../libmad-0.15.1b -I../flac-1.2.1/include -I../ASIOSDK2/common -I/System/Library/Frameworks/OpenGL.framework/Headers -I/System/Library/Frameworks/GLUT.framework/Headers -fasm-blocks

all: asr

asr: io.o

ui_gl.o: ui_gl.cpp
	$(CC) $(CFLAGS) ui_gl.cpp

io.o: io.cpp
	$(CC) $(CFLAGS) io.cpp
