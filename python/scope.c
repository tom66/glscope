#include <Python.h>
#include "scope.h"
#include "egl.h"
#undef NDEBUG
static PyObject *callback = NULL;

static PyObject *set_callback(PyObject *dummy, PyObject *args)
{
  PyObject *result = NULL;
  PyObject *temp;

  if (PyArg_ParseTuple(args, "O:set_callback", &temp)) {
    if (!PyCallable_Check(temp)) {
      PyErr_SetString(PyExc_TypeError, "parameter must be callable");
      return NULL;
    }
    Py_XINCREF(temp);         /* Add a reference to new callback */
    Py_XDECREF(callback);  /* Dispose of previous callback */
    callback = temp;       /* Remember new callback */
    /* Boilerplate to return "None" */
    Py_INCREF(Py_None);
    result = Py_None;
  }
  return result;
}

static PyObject* init_graph(PyObject* self, PyObject* args) {
  initgraph();
  return Py_None;
}

static PyObject* display(PyObject* self, PyObject* args) {
  graph_display();
  return Py_None;
}

void draw() {
}

void call_callback () {
  int arg = 2;
  PyObject *arglist;
  PyObject *result;
  arglist = Py_BuildValue("(i)", arg);
  result = PyObject_CallObject(callback, arglist);
  Py_DECREF(arglist);
}

// Function 1: A simple 'hello world' function
static PyObject* start(PyObject* self, PyObject* args) {
  if (graph_start())
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

static PyObject* get_egl_ctx_wrap(PyObject* self, PyObject* args) {
  void *dpy;
  PyObject *python_handle;
  unsigned long win;
  if (!PyArg_ParseTuple(args, "Ok", &python_handle, &win))
    return NULL;

  dpy = ((void **)python_handle)[2];

  printf("%p dpy=%p win=%u\n", python_handle, dpy, win);
  //abort();
  void *a, *b;
  get_egl_ctx(dpy, win, &a, &b);
  printf("a=%p b=%p\n", a, b);
  return Py_None;
}

static PyObject* egl_swap_wrap(PyObject* self, PyObject* args) {
  egl_swap();
  return Py_None;
}

static PyObject* stop(PyObject* self, PyObject* args) {
  graph_stop();
  return Py_None;
}

static PyObject* poll(PyObject* self, PyObject* args) {
  if (graph_poll())
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

static PyObject* bright(PyObject* self, PyObject* args) {
  graph_set_bright(PyFloat_AS_DOUBLE(args));
  return Py_None;
}

// Our Module's Function Definition struct
// We require this `NULL` to signal the end of our method
// definition 
static PyMethodDef myMethods[] = {
				  { "start", start, METH_NOARGS, "Starts" },
				  { "stop", stop, METH_NOARGS, "Stops" },
				  { "poll", poll, METH_NOARGS, "Polls" },
				  { "set_bright", bright, METH_O, "Sets brightness" },
				  { "set_callback", set_callback, METH_VARARGS, "Sets callback" },
				  { "get_egl_ctx", get_egl_ctx_wrap, METH_VARARGS, "gets EGL context" },
				  { "egl_swap", egl_swap_wrap, METH_VARARGS, "swaps EGL buffers" },
				  { "init", init_graph, METH_NOARGS, "Init" },
				  { "display", display, METH_NOARGS, "Draws" },
				  { NULL, NULL, 0, NULL }
};

// Our Module Definition struct
static struct PyModuleDef scope_module = {
					  PyModuleDef_HEAD_INIT,
					  "scope",
					  "Scope Interface",
					  -1,
					  myMethods
};

// Initializes our module using our above struct
PyMODINIT_FUNC PyInit_scope(void)
{
  return PyModule_Create(&scope_module);
}
