#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_connection.h"

int initgraph(void);
void graph_set_buffer(MMAL_BUFFER_HEADER_T *buf);
void graph_display();
int graph_start(void);
void graph_stop(void);
int graph_poll(void);
void graph_set_bright(double f);
