LDLIBS=-lm -lglut -lGLEW -lGL
CPPFLAGS=-g -O1
all: graph
clean:
	rm -f *.o graph
graph: graph.o shader_utils.o
.PHONY: all clean
