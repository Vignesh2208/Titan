#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION

#include <Python.h>
#include <stdbool.h>
#include <numpy/arrayobject.h>
#include "VT_functions.h"
#include "utility_functions.h"

bool writeLookaheadToFile(struct lookahead_map * lmap, char * filePath) {
  FILE *outfile; 
      
  // open file for writing 
  outfile = fopen (filePath, "w"); 
  if (outfile == NULL) { 
      fprintf(stderr, "\nError opening lookahead dump file\n"); 
      return false;
  } 
    
  // write struct to file 
  fwrite (lmap, sizeof(struct lookahead_map), 1, outfile); 
  fwrite (lmap->lookahead_values,
    sizeof(long), lmap->number_of_values, outfile); 

  // close file 
  fclose (outfile); 
  return true;
}

static PyObject * py_dump_lookahead(PyObject * module, PyObject * args) {
    PyObject * argy=NULL;        
    PyObject * ldumpFilePathObj = NULL;
    PyArrayObject * nparr=NULL;
    bool ret = false;
    
    int start_offset, number_of_values;
    struct lookahead_map * lmap;

    // "O" format -> read argument as a PyObject type into argy (Python/C API)
    if (!PyArg_ParseTuple(args, "OOii", &argy, &ldumpFilePathObj,
        &start_offset,&number_of_values)) {
        PyErr_SetString(PyExc_ValueError, "Error parsing arguments.");
        return NULL;
    }

    if (start_offset < 0 || number_of_values <= 0) {
      PyErr_SetString(PyExc_ValueError,
        "Start offset and number of values must be positive ...");
      return NULL;
    }

    // Determine the array's data type
    int DTYPE = PyArray_ObjectType(argy, NPY_LONG);    

    // parse python object into numpy array (Numpy/C API)
    nparr = (PyArrayObject *)PyArray_FROM_OTF(argy, DTYPE, NPY_ARRAY_IN_ARRAY);
    if (nparr==NULL) {
      PyErr_SetString(PyExc_RuntimeError, "Failed to allot memory for copy op ...");
      return NULL;
    }

    //just assume this for 1 dimensional array
    if (PyArray_NDIM(nparr) != 1) {
        Py_CLEAR(nparr);
        PyErr_SetString(PyExc_ValueError, "Expected 1 dimensional input array");
        return NULL;
    }
    
    npy_intp * dims = PyArray_DIMS(nparr);
    npy_intp i;
    int j = 0;

    lmap = (struct lookahead_map *)malloc(sizeof(struct lookahead_map));
    if (!lmap) {
      Py_CLEAR(nparr);
      PyErr_SetString(PyExc_RuntimeError, "Failed to allot memory for copy op ...");
      return NULL;
    }
    lmap->number_of_values = number_of_values;
    lmap->start_offset = start_offset;
    lmap->finish_offset = lmap->start_offset + number_of_values - 1;
    lmap->lookahead_values = (long *)malloc(sizeof(long) * (number_of_values));
    if (!lmap->lookahead_values) {
      Py_CLEAR(nparr);
      free(lmap);
      PyErr_SetString(PyExc_RuntimeError, "Failed to allot memory for copy op ...");
      return NULL;
    }

    
    for (i = 0; i < dims[0];  i++) {
      lmap->lookahead_values[j] = *((long*)PyArray_GETPTR1(nparr, i));
      j++;
    }

    printf ("Number of lookahead values: %ld\n", lmap->number_of_values);

    /* Write to lookahead dump file */ 
    PyObject *encodedString = PyUnicode_AsASCIIString(ldumpFilePathObj);
    if (encodedString) { //returns NULL if an exception was raised
        char *ldumpFilePath = PyBytes_AsString(encodedString);
        if(ldumpFilePath) {
            printf("Dumping lookahead to file: %s\n", ldumpFilePath);
            ret = writeLookaheadToFile(lmap, ldumpFilePath);
        }
        
    }

    free(lmap->lookahead_values);
    free(lmap);

    Py_XDECREF(encodedString);
    Py_CLEAR(nparr);

    return Py_BuildValue("O", ret ? Py_True: Py_False);

};

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
    {"DumpLookahead", py_dump_lookahead, METH_VARARGS, NULL},
    {NULL, NULL, 0, NULL}};

#if PY_MAJOR_VERSION <= 2

void initvt_functions(void) {
  Py_InitModule3("vt_functions", vt_functions_methods,
                 "virtual time functions");
}

#elif PY_MAJOR_VERSION >= 3

static struct PyModuleDef vt_api_definition = {
    PyModuleDef_HEAD_INIT, "vt_functions",
    "A Python module that exposes vt module's API", -1, vt_functions_methods};
PyMODINIT_FUNC PyInit_vt_functions(void) {
  Py_Initialize();
  import_array();
  return PyModule_Create(&vt_api_definition);
}

#endif
