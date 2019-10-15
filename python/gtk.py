#!/usr/bin/env python3
import gi
#import OpenGL
#OpenGL.FULL_LOGGING = True
#import logging
#logging.basicConfig()

gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, GLib
builder = Gtk.Builder()
builder.add_from_file("test.glade")

# objects in the .glade file
obtemplate = ["scopewin", "brightadj", "brightscale", "gl"]
ob = {i: builder.get_object(i) for i in obtemplate }

#def bright_cb(self):
#    global scope, gl
#    br = self.get_value()
#    print ("br=%d" % br)
#    #scope.set_bright(br/5)
#    glcommon.bright = br/5
#    gl.queue_render()

print(ob)
gl = ob['gl']
#gl.set_required_version(2,1)
ob['scopewin'].show_all()
ob['brightadj'].set_value(19)
#ob['brightadj'].connect("value-changed", bright_cb)
#win = Gtk.Window()
#win.connect("destroy", Gtk.main_quit)
#win.show_all()
#init_python(gl.get_context())
#gl_init_python("graph.v.glsl","antialias.f.glsl")
def redraw(self, clock):
    gl.queue_render()
    return True
            
import render
gl.add_tick_callback(redraw)
gl.connect('render',render.gl_render)
#gl.connect('realize', area_realize)
Gtk.main()
exit()
import cProfile
GLib.timeout_add(30000, Gtk.main_quit, None)
pr = cProfile.Profile()
pr.enable()
Gtk.main()
pr.disable()
pr.print_stats(sort="cumtime")
