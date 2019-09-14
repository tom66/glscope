#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h> /* usleep */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/time.h>

//#include "EGL/eglext.h"
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <epoxy/egl.h>
//#include <GL/glew.h>

//#include <GL/glut.h>
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_EGL

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <GL/glu.h> //gluGetError
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_brcm.h>

#include "shader_utils.h"
#include "scope.h"
GLuint program;
GLint attribute_coord1d;
GLint uniform_offset_x;
GLint uniform_scale_x;
GLint uniform_mytexture;
GLint uniform_wavenum;
GLint uniform_NWAVES;
GLint uniform_nxtiles;
GLint uniform_xtile;
GLint uniform_color;

bool interpolate = false;
bool clamp = false;
bool showpoints = false;
double bright = 1;

#define NPOINTS 2048
#define TEXSIZE 2048
#define NTEXTURES (NPOINTS/TEXSIZE)
#define NWAVES 256

GLuint vbo;
GLbyte graph[NTEXTURES][TEXSIZE * NWAVES];
//GLbyte graph[NPOINTS * NWAVES];
GLuint texture_id[NTEXTURES];

float offset_x = 0.0;
float scale_x = 1.0;
GLuint cam_ytex, cam_utex, cam_vtex;
EGLImageKHR yimg = EGL_NO_IMAGE_KHR;
EGLImageKHR uimg = EGL_NO_IMAGE_KHR;
EGLImageKHR vimg = EGL_NO_IMAGE_KHR;

EGLDisplay GDisplay;

GLFWwindow* win;

int init_resources() {
  //program = create_program("graph.v.glsl", "graph.f.glsl");
	program = create_program("graph.v.glsl", "antialias.f.glsl");
	if (program == 0)
		return 0;
	//printf("texture_id=%p\n",texture_id);

	attribute_coord1d = get_attrib(program, "coord1d");
	uniform_offset_x = get_uniform(program, "offset_x");
	uniform_scale_x = get_uniform(program, "scale_x");
	uniform_mytexture = get_uniform(program, "mytexture");
	uniform_wavenum = get_uniform(program, "wavenum");
	//uniform_NWAVES = get_uniform(program, "NWAVES");
	uniform_nxtiles = get_uniform(program, "nxtiles");
	uniform_xtile = get_uniform(program, "xtile");
	uniform_color = get_uniform(program, "color");

	if (attribute_coord1d == -1 || uniform_offset_x == -1 || uniform_scale_x == -1 || uniform_mytexture == -1)
		return 0;

	// Create our datapoints, store it as bytes

	bool positive = false;
	int x=0;
	for (int j = 0; j < NWAVES; j++) {
		for (int i = 0; ; i++, x++) {
			float y = sin(x/100.0) * (sin(x/10003.0)*.4+.5);
			//float y = sin(x * 10.0) / (1.0 + x * x);
			if (i < NPOINTS) {
				graph[i/TEXSIZE][(i%TEXSIZE) + j*TEXSIZE] = roundf(y * 128 + 128);
			} else {
				if (positive && y<=0)
					break; // retrig on falling edge
			}
			positive = (y > 0);
		}
	}

	/* Upload the texture with our datapoints */
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, &cam_ytex);
	glGenTextures(NTEXTURES, texture_id);
	for (int i=0; i<NTEXTURES; i++) {
		printf("texture %d has id %d\n", i, texture_id[i]);
		glBindTexture(GL_TEXTURE_2D, texture_id[i]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEXSIZE, NWAVES, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, graph[i]);
		/* Set texture wrapping mode */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		
		/* Set texture interpolation mode */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);

	}

	// Create the vertex buffer object
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE,GL_ONE);
	// Create an array with only x values.
	GLfloat line[NPOINTS];

	// Fill it in just like an array
	for (int i = 0; i < NPOINTS; i++) {
		line[i] = (i - ((NPOINTS-1)/2.0)) / ((NPOINTS-1)/2.0);
	}

	// Tell OpenGL to copy our array to the buffer object
	glBufferData(GL_ARRAY_BUFFER, sizeof line, line, GL_STATIC_DRAW);

	// Enable point size control in vertex shader
#ifndef GL_ES_VERSION_2_0
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

	return 1;
}

void graph_set_bright(double f) {
  bright = f;
}

static unsigned int fps_frames = 0;
static struct timeval fps_start = {0,0};

void graph_display() {
	assert(!glGetError());
	  /* FPS count */
  {
    fps_frames++;
    struct timeval now, delta;
    gettimeofday (&now, NULL);
    timersub (&now, &fps_start, &delta);
    if (delta.tv_sec) {
	printf("fps %f\n",1000000.0 * fps_frames / (delta.tv_sec*100000 + delta.tv_usec));
	fflush(stdout);
	fps_frames = 0;
        fps_start = now;
    }
  }

	glUseProgram(program);
	glUniform1i(uniform_mytexture, 0);

	glUniform1f(uniform_offset_x, offset_x);
	glUniform1f(uniform_scale_x, scale_x);
	//	glUniform1f(uniform_NWAVES, NWAVES);

	//glClearColor(0.0, 0.0, 0.0, 0.0);
	//glClear(GL_COLOR_BUFFER_BIT);


	/* Draw using the vertices in our vertex buffer object */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(attribute_coord1d);
	glVertexAttribPointer(attribute_coord1d, 1, GL_FLOAT, GL_FALSE, 0, 0);

	float scale = bright/NWAVES/2;
	for (int j=0; j<NTEXTURES; j++) {
		glBindTexture(GL_TEXTURE_2D, texture_id[j]);
		for (int i=0; i<NWAVES; i++) {
			glUniform1f(uniform_wavenum, (2*i+1.0)/(2*NWAVES));
			glUniform1f(uniform_nxtiles, NTEXTURES);
			glUniform1f(uniform_xtile, ((float)j*2-NTEXTURES+1)/NTEXTURES);
			glUniform4f(uniform_color, 4*scale,2*scale,0.5*scale,1);
			glDrawArrays(GL_POINTS, 0, NPOINTS);
		}
	}
	assert(!glGetError());
	glFlush();
}

static void check_real(const char *file, int line) {
	int q;
	do {
		q=glGetError();
		if (q) {
			printf("%s:%d: glError: %s (%x)\n", file, line, gluErrorString(q), q);
		}
	} while (q);
}

#define check() check_real(__FILE__, __LINE__)
#define C(...) __VA_ARGS__;check();

void free_resources() {
	glDeleteProgram(program);
}


void graph_set_buffer(MMAL_BUFFER_HEADER_T *buf) {
  //printf("cb: %08x %d tid=%d\n", buf->data, buf->length, syscall(SYS_gettid));
	assert(!glGetError());
	int i=0;

	for (int i=0; i<NTEXTURES; i++) {
	  void *ptr = (uint8_t *)buf->data; // + i * TEXSIZE * NWAVES;
	  glBindTexture(GL_TEXTURE_2D, texture_id[i]);
	  glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEXSIZE, NWAVES, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, ptr);
		/* Set texture wrapping mode */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		
		/* Set texture interpolation mode */
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interpolate ? GL_LINEAR : GL_NEAREST);
	}
}

int initgraph(void) {
	GLint max_units;

	C(glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_units));
	if (max_units < 1) {
		fprintf(stderr, "Your GPU does not have any vertex texture image units\n");
		return 1;
	}
#if 0
	if (!glIsEnabled (GL_TEXTURE_EXTERNAL_OES)) {
		fprintf(stderr, "no TEXTURE_EXTERNAL_OES\n");
		return 1;
	}
#endif
	C(glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_units));
	if (max_units < 1) {
		fprintf(stderr, "Your GPU does not have any vertex texture image units\n");
		return 1;
	}

	GLfloat range[2];

	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
	if (range[1] < 5.0)
		fprintf(stderr, "WARNING: point sprite range (%f, %f) too small\n", range[0], range[1]);

	if (!init_resources())
		return 1;

	assert(!glGetError());
	return 0;
}
