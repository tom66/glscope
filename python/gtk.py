#!/usr/bin/env python3
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from OpenGL.GL import *
from OpenGL.arrays import *
from OpenGL.GL import shaders
import pyshaders
import scope
from math import sin
NPOINTS = 2048
TEXSIZE = 2048
NTEXTURES = int(NPOINTS/TEXSIZE)
NWAVES = 256

builder = Gtk.Builder()
builder.add_from_file("test.glade")

# objects in the .glade file
obtemplate = ["scopewin", "brightadj", "brightscale", "gl"]
ob = {i: builder.get_object(i) for i in obtemplate }

first_render = True

class glcommon:
    vbo = None
    shader = None
    textures = None
    
def bright_cb(self):
    global scope, gl
    br = self.get_value()
    print ("br=%d" % br)
    scope.set_bright(br/5)
    gl.queue_render()

def area_realize (self, gl_area):
    error = gl_area.get_error()
    if error != None:
        print (error)
    #else:
    #    print (gl_area, "realize... fine so far")
    

def area_render(area, context):
    global first_render
    if first_render:
        #scope.init()
        gl_init_python("graph.v.glsl","graph.f.glsl")
        first_render = False
    glClearColor (0,0,0, 1.0)
    glClear (GL_COLOR_BUFFER_BIT)
    glBegin(GL_LINES)
    glColor3f(.25,.25,.25)
    for x in range(-4,4):
        glVertex2f(x/4,-1)
        glVertex2f(x/4,1)
    for y in range(-4,4):
        glVertex2f(-1,y/4)
        glVertex2f(1,y/4)
    glEnd()

    python_display()
    #scope.display()

    #glFlush()
    print ("rendering... done")
    return True

def gl_init_python(ver, frag):
    glcommon.shader = pyshaders.from_files_names(ver, frag)
    shader = glcommon.shader
    shader.use()
    shader.uniforms.offset_x = 0
    shader.uniforms.scale_x = 1
    shader.uniforms.mytexture = 0
    glcommon.vbo = glGenBuffers(1)
    glBindBuffer(GL_ARRAY_BUFFER, glcommon.vbo)
    c1d = shader.attributes.coord1d
    c1d.enable()
    c1d.point_to(0, GL_FLOAT, 1, False, 0)
    #glVertexAttribPointer(attribute_coord1d, 1, GL_FLOAT, GL_FALSE, 0, 0);
    
    line = [(i - ((NPOINTS-1)/2.0)) / ((NPOINTS-1)/2.0)
            for i in range(NPOINTS)]

    array_type = (GLfloat * len(line))
    glBufferData(GL_ARRAY_BUFFER, NPOINTS*4, array_type(*line), GL_STATIC_DRAW)
    shader.clear()

    glcommon.texid = glGenTextures(NTEXTURES)
    print ("texid=%d" % glcommon.texid)
    for i in range(NTEXTURES):
        glBindTexture(GL_TEXTURE_2D, glcommon.texid+i)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, TEXSIZE, NWAVES, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE,
                     glcommon.textures[i])
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST)

def make_test_waveform():
    positive = False
    x=0
    textures = [ GLbyteArray.zeros((TEXSIZE*NWAVES,)) for i in range(NTEXTURES) ]
    print("making test waveform...")
    for j in range(NWAVES):
        for i in range(TEXSIZE*NWAVES):
            x+=1
            y = sin(x/100.0) * (sin(x/10003.0)*.4+.5)
            if i < NPOINTS:
                textures[int(i/TEXSIZE)][(i%TEXSIZE) + j*TEXSIZE] = int(y * 128 + 128)
            else:
                if positive and y<=0:
                    break # retrig on falling edge
                
                positive = (y > 0)
    print("done")
    return textures

def python_display():
    #glUseProgram(program)
    #glUniform1i(uniform_mytexture, 0)

    #glClearColor(0.0, 0.0, 0.0, 0.0)
    #glClear(GL_COLOR_BUFFER_BIT)
    glBindBuffer(GL_ARRAY_BUFFER, glcommon.vbo)
    
    #glEnableVertexAttribArray(attribute_coord1d)
    #glVertexAttribPointer(attribute_coord1d, 1, GL_FLOAT, GL_FALSE, 0, 0)

    scale = .1 #bright/NWAVES/2
    glcommon.shader.use()
    u = glcommon.shader.uniforms
    u.nxtiles = NTEXTURES
    u.color =  [4*scale,2*scale,0.5*scale,1.0 ]
    for j in range(NTEXTURES):
        #glBindTexture(GL_TEXTURE_2D, texture_id[j])
        u.xtile = (j*2-NTEXTURES+1)/NTEXTURES
        for i in range(NWAVES):
            u.wavenum = (2*i+1.0)/(2*NWAVES)
            glDrawArrays(GL_POINTS, 0, NPOINTS)
    glcommon.shader.clear()

glcommon.textures = make_test_waveform()
print(ob)
gl = ob['gl']
#gl.set_required_version(2,1)
ob['scopewin'].show_all()
ob['brightadj'].set_value(19)
ob['brightadj'].connect("value-changed", bright_cb)
#win = Gtk.Window()
#win.connect("destroy", Gtk.main_quit)
#win.show_all()
#init_python(gl.get_context())
#gl_init_python("graph.v.glsl","antialias.f.glsl")
gl.connect('render',area_render)
gl.connect('realize', area_realize)

Gtk.main()
