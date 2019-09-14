LDLIBS=-lm -lglfw -lepoxy -lGLU -lmmal -lbcm_host -lvcos -lmmal_core -lmmal_util -lmmal_vc_client
LDFLAGS=-L/opt/vc/lib
CFLAGS=-I/opt/vc/include
CPPFLAGS=-g -O1
all: fgraph
clean:
	rm -f *.o graph
egraph: egraph.o shader_utils.o raspiraw.o RaspiCLI.o
fgraph: fgraph.o shader_utils.o raspiraw.o RaspiCLI.o
ggraph: fgraph.o shader_utils.o raspiraw.o RaspiCLI.o
graph: graph.o shader_utils.o

.PHONY: all clean
