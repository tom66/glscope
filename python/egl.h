#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xlib-xcb.h>
#include <epoxy/gl.h>
#include <epoxy/egl.h>

void get_egl_ctx(Display *dpy, Window win,
	    void *ctxRet, void *surfRet);
void egl_swap(void);
