from OpenGL.GL import *
from OpenGL.arrays import *
from OpenGL.GL import shaders
import pyshaders
import pyglbuffers
import time
from math import sin

import cProfile

NPOINTS = 1024 #2048
MAX_DRAWARRAYS = 65535 #GL_MAX_ELEMENTS_VERTICES
TEXSIZE = 1024 #2048
NTEXTURES = int(NPOINTS/TEXSIZE)
NWAVES = 120 # 256
WAVE_STRIDE = 60

GTK = False
USE_C = False #True
SIXTEEN_BIT = False #True

first_render = True

class glcommon:
    vbo = None
    vao = None
    shader = None
    textures = None
    bright = 4
    graticule = None
    frames = 0
    total = 0.0
    start_tick = None
    
#def bright_cb(self):
#    global scope, gl
#    br = self.get_value()
#    print ("br=%d" % br)
#    #scope.set_bright(br/5)
#    glcommon.bright = br/5
#    gl.queue_render()

def draw_graticule():
    glcommon.trivial_shader.use()
    glcommon.trivial_shader.uniforms.color = (0.25, 0.25, 0.25, 1.0)
    glcommon.graticule.bind(target=GL_ARRAY_BUFFER)

    c2d = glcommon.trivial_shader.attributes.coord2d
    c2d.enable()
    c2d.point_to(0, GL_FLOAT, 2, False, 0)
    glDrawArrays(GL_LINES, 0, 2*len(glcommon.graticule))
    glcommon.trivial_shader.clear()

def gl_render(area, context):
    if (glcommon.frames % 100)==0:
        now = time.perf_counter()
        if glcommon.frames>0:
            print("frame % 5d %.2f fps" % (glcommon.frames, 100/(now-glcommon.start_tick)))
        glcommon.start_tick = now
    glcommon.frames+=1

    global first_render
    if first_render:
        gl_init_python("graph.v.glsl","graph.f.glsl")
    start = time.perf_counter()
    if first_render:
        glcommon.start = start
        first_render = False
    glClearColor (0,0,0, 1.0)
    glClear (GL_COLOR_BUFFER_BIT)

    draw_graticule()
    python_display()
    return True

def gen_line(vbo):
    wstr = 4
    pstr = 4
    line = [(w+ww) + ((p+pp) / NPOINTS)
              for pp in range(pstr) for ww in range(wstr)
              for p in range(0,NPOINTS,pstr) for w in range(0,WAVE_STRIDE,wstr)]
    
    glBindBuffer(GL_ARRAY_BUFFER, glcommon.vbo)
    array_type = (GLfloat * len(line))
    glBufferData(GL_ARRAY_BUFFER, len(line)*4, array_type(*line), GL_STATIC_DRAW)
    
def gl_init_python(ver, frag):
    #gl_version = float(glGetString(GL_VERSION).split(b" ")[0])
    GL3 = False #gl_version >= 3
    glcommon.shader = pyshaders.from_files_names(ver, frag)
    shader = glcommon.shader
    shader.use()
    shader.uniforms.nwaves = NWAVES
    shader.uniforms.mytexture = 0
    if GL3:
        glcommon.vao = glGenVertexArrays(1)
        glBindVertexArray(glcommon.vao)
    glPointSize(2)
    glEnable(GL_BLEND)
    glBlendFunc(GL_SRC_ALPHA,GL_ONE)
    c1d = shader.attributes.coord1d
    c1d.enable()
    c1d.point_to(0, GL_FLOAT, 1, False, 0)
    glcommon.vbo = glGenBuffers(1)
    gen_line(glcommon.vbo)
    shader.clear()

    # glGenTextures returns either an array (if n>1) or an int (if n==1), which is unhelpful
    glcommon.texid = [ glGenTextures(1) for i in range(NTEXTURES) ]

    for i in range(NTEXTURES):
        glBindTexture(GL_TEXTURE_2D, glcommon.texid[i])
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED if GL3 else GL_LUMINANCE, TEXSIZE, NWAVES, 0,
                     GL_LUMINANCE16 if SIXTEEN_BIT else GL_LUMINANCE,
                     GL_UNSIGNED_SHORT if SIXTEEN_BIT else GL_UNSIGNED_BYTE,
                     glcommon.textures[i])
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST)


    glcommon.trivial_shader = pyshaders.from_files_names("graticule.v.glsl", "graticule.f.glsl")
    glcommon.graticule = make_graticule(8,6,5,5)

def make_graticule(xticks, yticks, xt, yt):
    left = bot = -1.0
    right = top = 0.995
    width = right-left
    height = top-bot
    # vertical lines
    points = [ ((x/xticks*width+left, bot),
                (x/xticks*width+left, top)) for x in range(xticks+1) ]
    # horizontal lines
    points += [ ((left, y/yticks*height+bot),
                   ( right, y/yticks*height+bot)) for y in range(yticks+1) ]
    ticksize = 1.0/xticks/4
    # bottom ticks
    points += [ ((x/(xt*xticks)*width+left,bot),
                 (x/(xt*xticks)*width+left, bot+ticksize)) for x in range(xt*xticks+1) ]
    # top ticks
    points += [ ((x/(xt*xticks)*width+left, top),
                 (x/(xt*xticks)*width+left,  top-ticksize)) for x in range(xt*xticks+1) ]
    # left ticks
    points += [ ((left, y/(yt*yticks)*height+bot),
                (left+ticksize, y/(yt*yticks)*height+bot)) for y in range(yt*yticks+1) ]
    # right ticks
    points += [ ((right, y/(yt*yticks)*height+bot),
                 (right-ticksize, y/(yt*yticks)*height+bot)) for y in range(yt*yticks+1) ]

    buf = pyglbuffers.Buffer.array('(2f)[start](2f)[end]')
    buf.init(points)
    return buf

def make_test_waveform():
    positive = True
    x=0
    textures = [ (GLshortArray if SIXTEEN_BIT else GLbyteArray).zeros((TEXSIZE*NWAVES,))
                 for i in range(NTEXTURES) ]
    sc = 32768 if SIXTEEN_BIT else 128
    print("making test waveform...")
    for j in range(NWAVES):
        for i in range(TEXSIZE*NWAVES):
            x+=1
            y = sin(x/100.1) * (sin(x/1003.0)*.4+.5)
            if i < NPOINTS:
                textures[int(i/TEXSIZE)][(i%TEXSIZE) + j*TEXSIZE] = int(y * sc + sc)
            else:
                if not positive and y>0:
                    break # retrig on rising edge
                
            positive = (y > 0)
    print("done")
    return textures

def python_display():
    glBindBuffer(GL_ARRAY_BUFFER, glcommon.vbo)
    c1d = glcommon.shader.attributes.coord1d
    c1d.enable()
    c1d.point_to(0, GL_FLOAT, 1, False, 0)

    scale = 4*glcommon.bright/NWAVES/2
    glcommon.shader.use()
    u = glcommon.shader.uniforms
    u.nxtiles = NTEXTURES
    u.color =  [ 4*scale,2*scale,0.5*scale ]

    u.offset_x2 = glcommon.frames*0.001
    WIDTH = 1000
    HEIGHT = 800
    for j in range(NTEXTURES):
        glBindTexture(GL_TEXTURE_2D, glcommon.texid[j])
        u.xtile = (j*2-NTEXTURES+1.0)/NTEXTURES
        for i in range(0,NWAVES,WAVE_STRIDE):
            u.wavenum = (2*i+1.0)/(2*NWAVES)
            glDrawArrays(GL_POINTS, 0, NPOINTS*(WAVE_STRIDE if NWAVES-i>WAVE_STRIDE else NWAVES-i))
    glcommon.shader.clear()
    glFinish()

glcommon.textures = make_test_waveform()
