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
    


da.add_tick_callback(redraw)
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
