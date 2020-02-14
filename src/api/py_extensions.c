#include <Python.h>
#include "VT_functions.h"
#include "utility_functions.h"


static PyObject *py_gettime_pid(PyObject *self, PyObject *args) {
  s64 ret;
  int pid;

  if (!PyArg_ParseTuple(args, "i", &pid)) return NULL;

  ret = get_current_time_pid(pid);
  return Py_BuildValue("L", ret);
}

static PyObject *py_stopExp(PyObject *self, PyObject *args) {
  int ret;
  ret = stopExp();
  return Py_BuildValue("i", ret);
}

static PyObject *py_initializeExp(PyObject *self, PyObject *args) {
  int ret;
  int n_tracers;
  if (!PyArg_ParseTuple(args, "i", &n_tracers)) return NULL;

  ret = initializeExp(n_tracers);
  return Py_BuildValue("i", ret);
}

static PyObject *py_progress_by(PyObject *self, PyObject *args) {
  s64 duration;
  int num_rounds;
  int ret;

  if (!PyArg_ParseTuple(args, "Li", &duration, &num_rounds)) return NULL;
  ret = progressBy(duration, num_rounds);
  return Py_BuildValue("i", ret);
}

static PyObject *py_synchronizeAndFreeze(PyObject *self, PyObject *args) {
  int ret;
  ret = synchronizeAndFreeze();
  return Py_BuildValue("i", ret);
}

static PyMethodDef vt_functions_methods[] = {
    {"synchronizeAndFreeze", py_synchronizeAndFreeze, METH_VARARGS, NULL},
    {"getTimePID", py_gettime_pid, METH_VARARGS, NULL},
    {"stopExp", py_stopExp, METH_VARARGS, NULL},
    {"initializeExp", py_initializeExp, METH_VARARGS, NULL},
    {"progressBy", py_progress_by, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}};

#if PY_MAJOR_VERSION <= 2

void initvt_functions(void) {
  Py_InitModule3("vt_functions", vt_functions_methods,
                 "virtual time functions");
}

#elif PY_MAJOR_VERSION >= 3

static struct PyModuleDef vt_api_definition = {
    PyModuleDef_HEAD_INIT, "vt functions",
    "A Python module that exposes vt module's API", -1, vt_functions_methods};
PyMODINIT_FUNC PyInit_vt_functions(void) {
  Py_Initialize();
  return PyModule_Create(&vt_api_definition);
}

#endif
