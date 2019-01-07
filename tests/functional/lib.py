#
# Copyright(c) 2012-2018 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
from ctypes import *

lib = None

#TODO Consider changing lib to singleton
def LoadOcfLib():
    global lib
    lib = cdll.LoadLibrary('./libocf.so')
