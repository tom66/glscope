#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h> /* usleep */
#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <epoxy/egl.h>
#include <GL/glut.h>

#include "shader_utils.h"

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

#define NPOINTS 8192
#define TEXSIZE 2048
#define NTEXTURES (NPOINTS/TEXSIZE)
#define NWAVES 400

GLuint vbo;
GLbyte graph[NTEXTURES][TEXSIZE * NWAVES];
//GLbyte graph[NPOINTS * NWAVES];
GLuint texture_id[NTEXTURES];

float offset_x = 0.0;
float scale_x = 1.0;


int init_resources() {
	program = create_program("graph.v.glsl", "graph.f.glsl");
	//program = create_program("graph.v.glsl", "antialias.f.glsl");
	if (program == 0)
		return 0;
	printf("texture_id=%p\n",texture_id);

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
static unsigned int fps_start = 0;
static unsigned int fps_frames = 0;

void display() {
	  /* FPS count */
  {
    fps_frames++;
    int delta_t = glutGet(GLUT_ELAPSED_TIME) - fps_start;
    if (delta_t > 1000) {
		printf("%f\n",1000.0 * fps_frames / delta_t);
      fps_frames = 0;
      fps_start = glutGet(GLUT_ELAPSED_TIME);
    }
  }

	glUseProgram(program);
	glUniform1i(uniform_mytexture, 0);

	glUniform1f(uniform_offset_x, offset_x);
	glUniform1f(uniform_scale_x, scale_x);
	//	glUniform1f(uniform_NWAVES, NWAVES);

	glClearColor(0.0, 0.0, 0.0, 0.0);
	glClear(GL_COLOR_BUFFER_BIT);


	/* Draw using the vertices in our vertex buffer object */
	glBindBuffer(GL_ARRAY_BUFFER, vbo);

	glEnableVertexAttribArray(attribute_coord1d);
	glVertexAttribPointer(attribute_coord1d, 1, GL_FLOAT, GL_FALSE, 0, 0);

	/* Draw the line */
	//glDrawArrays(GL_LINE_STRIP, 0, NPOINTS);

	/* Draw points as well, if requested */
	//if (showpoints)

	float scale = 1./NWAVES/2;
	for (int j=0; j<NTEXTURES; j++) {
		//int j=1;
		glBindTexture(GL_TEXTURE_2D, texture_id[j]);
		for (int i=0; i<NWAVES; i++) {
			glUniform1f(uniform_wavenum, (2*i+1.0)/(2*NWAVES));
			//glUniform1f(uniform_offset_x, offset_x);
			//glUniform1f(uniform_scale_x, scale_x);
			glUniform1f(uniform_nxtiles, NTEXTURES);
			glUniform1f(uniform_xtile, ((float)j*2-NTEXTURES+1)/NTEXTURES);
			//			glUniform4f(uniform_color, (j&1)*scale,((j&2)>>1)*scale,((j&4)>>2)*scale,1);
			glUniform4f(uniform_color, 1*scale,4*scale,1*scale,1);
			//glUniform4f(uniform_color, (i&1)*scale,((i&2)>>1)*scale,((i&4)>>2)*scale,1);

			//glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, NPOINTS, NWAVES, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, graph+NPOINTS*i);
			glDrawArrays(GL_POINTS, 0, NPOINTS);
		}
		//		glutSwapBuffers();	usleep(50000);
	}
	glutSwapBuffers();
	glutPostRedisplay();
}

void special(int key, int x, int y) {
	switch (key) {
	case GLUT_KEY_F1:
		interpolate = !interpolate;
		printf("Interpolation is now %s\n", interpolate ? "on" : "off");
		break;
	case GLUT_KEY_F2:
		clamp = !clamp;
		printf("Clamping is now %s\n", clamp ? "on" : "off");
		break;
	case GLUT_KEY_F3:
		showpoints = !showpoints;
		printf("Showing points is now %s\n", showpoints ? "on" : "off");
		break;
	case GLUT_KEY_LEFT:
		offset_x -= 0.1;
		break;
	case GLUT_KEY_RIGHT:
		offset_x += 0.1;
		break;
	case GLUT_KEY_UP:
		scale_x *= 1.5;
		break;
	case GLUT_KEY_DOWN:
		scale_x /= 1.5;
		break;
	case GLUT_KEY_HOME:
		offset_x = 0.0;
		scale_x = 1.0;
		break;
	}

	glutPostRedisplay();
}

void free_resources() {
	glDeleteProgram(program);
}

int main(int argc, char *argv[]) {
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGB);
	glutInitWindowSize(1920, 1080);
	glutCreateWindow("testy");

	GLenum glew_status = glewInit();

	if (GLEW_OK != glew_status) {
		fprintf(stderr, "Error: %s\n", glewGetErrorString(glew_status));
		return 1;
	}

	if (!GLEW_VERSION_2_0) {
		fprintf(stderr, "No support for OpenGL 2.0 found\n");
		return 1;
	}

	GLint max_units;

	glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &max_units);
	if (max_units < 1) {
		fprintf(stderr, "Your GPU does not have any vertex texture image units\n");
		return 1;
	}

	GLfloat range[2];

	glGetFloatv(GL_ALIASED_POINT_SIZE_RANGE, range);
	if (range[1] < 5.0)
		fprintf(stderr, "WARNING: point sprite range (%f, %f) too small\n", range[0], range[1]);

	printf("Use left/right to move horizontally.\n");
	printf("Use up/down to change the horizontal scale.\n");
	printf("Press home to reset the position and scale.\n");
	printf("Press F1 to toggle interpolation.\n");
	printf("Press F2 to toggle clamping.\n");
	printf("Press F3 to toggle drawing points.\n");

	if (init_resources()) {
		glutDisplayFunc(display);
		glutSpecialFunc(special);
		glutMainLoop();
	}

	free_resources();
	return 0;
}
