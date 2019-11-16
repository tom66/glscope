#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if 0
#include <drm.h>
#include <drm_mode.h>
#include <drm_fourcc.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#endif
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>
#include <xcb/xcb.h>
#include <xcb/dri3.h>

#include <interface/vcsm/user-vcsm.h>
#include "bcm_host.h"
#include "interface/mmal/mmal.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/mmal/mmal_queue.h"
#include "interface/vcos/vcos.h"
#include <stdio.h>

#undef NDEBUG
static EGLContext s_ctx;
static EGLDisplay s_dpy;
static EGLSurface s_surf;

void
get_egl_ctx(Display *dpy, Window win,
	     EGLContext *ctxRet, EGLSurface *surfRet) {
  EGLDisplay egl_dpy = eglGetDisplay(dpy);
  unsigned long mask;
   EGLContext ctx;
   EGLint egl_major, egl_minor;

   if (!eglInitialize(egl_dpy, &egl_major, &egl_minor)) {
     assert(0);
   }
    
   printf("egl v%d.%d\n", egl_major, egl_minor);
     
   static const EGLint attribs[] = {
     EGL_RED_SIZE, 1,
     EGL_GREEN_SIZE, 1,
     EGL_BLUE_SIZE, 1,
     EGL_ALPHA_SIZE, 1,
     EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
     //     EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
     EGL_NONE
   };
   EGLConfig config;
   EGLint num_configs;
   eglGetConfigs(egl_dpy, NULL, 0, &num_configs);
   printf("num_configs=%d\n", num_configs);
   if (!eglChooseConfig(egl_dpy, attribs, &config, 1, &num_configs)) {
      printf("Error: couldn't get an EGL visual config\n");
      exit(1);
   }

   EGLint vid;
   if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
      printf("Error: eglGetConfigAttrib() failed\n");
      exit(1);
   }

   if (!eglBindAPI(EGL_OPENGL_API)) {
     abort();
   }

   static const EGLint ctx_attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };
   ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
   if (!ctx) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }
   //win=DefaultRootWindow(dpy);
   EGLSurface surf = eglCreateWindowSurface(egl_dpy, config,
                                            (void *)(uintptr_t)win, NULL);
   if (!surf) {
      printf("Error: eglCreateWindowSurface failed\n");
      exit(1);
   }

   if (!eglMakeCurrent(egl_dpy, surf, surf, ctx)) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }

   //*ctxRet = ctx;
   //*surfRet = surf;
   s_ctx = ctx;
   s_surf = surf;
   s_dpy = egl_dpy;
}

void egl_swap(void) {
  eglSwapBuffers(s_dpy, s_surf);
}
