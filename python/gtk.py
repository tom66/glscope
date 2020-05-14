#!/usr/bin/env python3
import os
os.environ['PYOPENGL_PLATFORM'] = 'egl'
import gi
#import OpenGL
#OpenGL.FULL_LOGGING = True
#import logging
#logging.basicConfig()

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib, Gdk, GdkX11
builder = Gtk.Builder()
builder.add_from_file("test.glade")

# objects in the .glade file
obtemplate = ["scopewin", "brightadj", "brightscale", "da"]
ob = {i: builder.get_object(i) for i in obtemplate }

#def bright_cb(self):
#    global scope, gl
#    br = self.get_value()
#    print ("br=%d" % br)
#    #scope.set_bright(br/5)
#    glcommon.bright = br/5
#    gl.queue_render()

print(ob)
#gl = ob['gl']
da = ob['da']
#gl.set_required_version(2,1)
#object_methods = [method_name for method_name in dir(da)
#                  if callable(getattr(da, method_name))]

#print(object_methods)
ob['scopewin'].show_all()
ob['brightadj'].set_value(19)
#ob['brightadj'].connect("value-changed", bright_cb)
#win = Gtk.Window()
#win.connect("destroy", Gtk.main_quit)
#win.show_all()
#init_python(gl.get_context())
#gl_init_python("graph.v.glsl","antialias.f.glsl")
from OpenGL.GL import *
def redraw(self, clock):

    #glClearColor (0,1,0, 1.0)
    #glClear (GL_COLOR_BUFFER_BIT)
    #glFlush()
    render.gl_render(None,None)
    scope.egl_swap()
    #gl.queue_render()
    return True

def new_gdk_window():
    a = Gdk.WindowAttr()
    a.window_type = Gdk.WindowType.TOPLEVEL
    a.wclass = Gdk.WindowWindowClass.INPUT_OUTPUT
    a.width = 400
    a.height = 400
    w = Gdk.Window(parent=None,attributes=a,attributes_mask=0)
    print ("window",w)
    w.show()
    
def camera_init():
    rc = rawcam.init() # initializes camera interface, returns config object

    rawcam.set_timing(0, 0, 0, 0, 0, 0, 0)
    rawcam.set_data_lanes(2)
    rawcam.set_image_id(0x2a)
    rawcam.set_camera_num(1)
    rawcam.set_buffer_size(2048 * 128)
    rawcam.set_buffer_num(4)
    rawcam.set_buffer_dimensions(2048, 128)
    rawcam.set_pack_mode(0)
    rawcam.set_unpack_mode(0)
    rawcam.set_unpack_mode(0)
    rawcam.set_encoding_fourcc(ord('G'), ord('R'), ord('G'), ord('B'))
    #rawcam.set_encode_block_length(32)
    #rawcam.set_embedded_data_lines(32)
    rawcam.format_commit()
    rawcam.set_zero_copy(1)
    rawcam.debug()

    rawcam.start()
    return rawcam.get_eventfd()

def got_camera():
    n = rawcam.buffer_count()
    assert (n)
    buf = rawcam.buffer_get()
    if n>1:
        print("discarding %d buffers" % (n-1))
        for i in range(n-1):
            rawcam.buffer_free(rawcam.buffer_get())
    render.set_buffer(buf)
    da.redraw()

#da.add_tick_callback(redraw)
#new_gdk_window()
#gl.connect('render',render.gl_render)
#gl.connect('realize', area_realize)
daxid=da.get_window().get_xid()
r=da.get_allocated_size().allocation
print("%d %d %d %d\n" % (r.x, r.y, r.width, r.height))
dpy=Gdk.Display.get_default().get_xdisplay()
print("python dpy=",dpy," win=",daxid)
#print ([a for a in dir(dpy)])
import scope
scope.get_egl_ctx(dpy,daxid)
import render
import camera
cam_src = GLib.Source()
cam_src.add_unix_fd(camera_init())
cam_src.set_callback(got_camera)
#root=Gdk.get_default_root_window()
#scr=Gdk.Display.get_default_screen()
print("foo")
Gtk.main()
exit()
import cProfile
GLib.timeout_add(30000, Gtk.main_quit, None)
pr = cProfile.Profile()
pr.enable()
Gtk.main()
pr.disable()
pr.print_stats(sort="cumtime")
