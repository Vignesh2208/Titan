#include <Python.h>
#include "VT_functions.h"
#include "utility_functions.h"


static PyObject *py_gettime_pid(PyObject *self, PyObject *args) {
  s64 ret;
  int pid;

  if (!PyArg_ParseTuple(args, "i", &pid)) return NULL;

  ret = GetCurrentTimePid(pid);
  return Py_BuildValue("L", ret);
}

static PyObject *py_get_la(PyObject *self, PyObject *args) {
  s64 ret;
  int tracer_id;

  if (!PyArg_ParseTuple(args, "i", &tracer_id)) return NULL;

  ret = GetTracerLookahead(tracer_id);
  return Py_BuildValue("L", ret);
}

static PyObject *py_StopExp(PyObject *self, PyObject *args) {
  int ret;
  ret = StopExp();
  return Py_BuildValue("i", ret);
}

static PyObject *py_InitializeExp(PyObject *self, PyObject *args) {
  int ret;
  int n_tracers;
  if (!PyArg_ParseTuple(args, "i", &n_tracers)) return NULL;

  ret = InitializeExp(n_tracers);
  return Py_BuildValue("i", ret);
}

static PyObject *py_InitializeVtExp(PyObject *self, PyObject *args) {
  int ret;
  int n_tracers;
  int exp_type;
  int n_timelines;
  if (!PyArg_ParseTuple(args, "iii", &exp_type, &n_timelines, &n_tracers)) 
    return NULL;

  ret = InitializeVtExp(exp_type, n_timelines, n_tracers);
  return Py_BuildValue("i", ret);
}

static PyObject *py_ProgressBy(PyObject *self, PyObject *args) {
  s64 duration;
  int num_rounds;
  int ret;

  if (!PyArg_ParseTuple(args, "Li", &duration, &num_rounds)) return NULL;
  ret = ProgressBy(duration, num_rounds);
  return Py_BuildValue("i", ret);
}

static PyObject *py_ProgressTimelineBy(PyObject *self, PyObject *args) {
  s64 duration;
  int timeline_id;
  int ret;

  if (!PyArg_ParseTuple(args, "iL", &timeline_id, &duration)) return NULL;
  ret = ProgressTimelineBy(timeline_id, duration);
  return Py_BuildValue("i", ret);
}

static PyObject *py_set_eat(PyObject *self, PyObject *args) {
  s64 eat;
  int tracer_id;
  int ret;

  if (!PyArg_ParseTuple(args, "iL", &tracer_id, &eat)) return NULL;
  ret = SetEarliestArrivalTime(tracer_id, eat);
  return Py_BuildValue("i", ret);
}

static PyObject *py_SynchronizeAndFreeze(PyObject *self, PyObject *args) {
  int ret;
  ret = SynchronizeAndFreeze();
  return Py_BuildValue("i", ret);
}

static PyMethodDef vt_functions_methods[] = {
    {"SynchronizeAndFreeze", py_SynchronizeAndFreeze, METH_VARARGS, NULL},
    {"GetCurrentTimePid", py_gettime_pid, METH_VARARGS, NULL},
    {"StopExp", py_StopExp, METH_VARARGS, NULL},
    {"InitializeExp", py_InitializeExp, METH_VARARGS, NULL},
    {"InitializeVtExp", py_InitializeVtExp, METH_VARARGS, NULL},
    {"ProgressBy", py_ProgressBy, METH_VARARGS, NULL},
    {"ProgressTimelineBy", py_ProgressTimelineBy, METH_VARARGS, NULL},
    {"SetEarliestArrivalTime", py_set_eat, METH_VARARGS, NULL},
    {"GetTracerLookahead", py_get_la, METH_VARARGS, NULL},
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
