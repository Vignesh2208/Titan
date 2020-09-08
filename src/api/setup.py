#!/usr/bin/env python
from distutils.core import setup, Extension
import numpy
import sys
print(sys.executable)

print (numpy.__version__)
#setup(name='vt_functions', version='1.0',  \
#      ext_modules=[Extension('vt_functions', include_dirs=['/usr/local/include','/usr/local/include/python3.2'], library_dirs = ['/usr/local/lib','/usr/lib/i386-linux-gnu'], sources = ['py_extensions.c', 'VT_functions.c', 'utility_functions.c'])])

setup(name='vt_functions', version='1.0',  \
      ext_modules=[Extension('vt_functions', include_dirs=[numpy.get_include()],
      sources = ['py_extensions.c', 'VT_functions.c', 'utility_functions.c'])])
