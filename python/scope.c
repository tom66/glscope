#include <Python.h>
#include "scope.h"

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
