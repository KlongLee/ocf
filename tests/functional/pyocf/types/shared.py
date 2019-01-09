#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from ctypes import *
from enum import IntEnum, auto

from ..utils import Size as S


class OcfErrorCode(IntEnum):
    OCF_ERR_INVAL = 1000000
    OCF_ERR_INVAL_VOLUME_TYPE = auto()
    OCF_ERR_INTR = auto()
    OCF_ERR_UNKNOWN = auto()
    OCF_ERR_TOO_MANY_CACHES = auto()
    OCF_ERR_NO_MEM = auto()
    OCF_ERR_NO_FREE_RAM = auto()
    OCF_ERR_START_CACHE_FAIL = auto()
    OCF_ERR_CACHE_IN_USE = auto()
    OCF_ERR_CACHE_NOT_EXIST = auto()
    OCF_ERR_CACHE_EXIST = auto()
    OCF_ERR_TOO_MANY_CORES = auto()
    OCF_ERR_CORE_NOT_AVAIL = auto()
    OCF_ERR_NOT_OPEN_EXC = auto()
    OCF_ERR_CACHE_NOT_AVAIL = auto()
    OCF_ERR_IO_CLASS_NOT_EXIST = auto()
    OCF_ERR_WRITE_CACHE = auto()
    OCF_ERR_WRITE_CORE = auto()
    OCF_ERR_DIRTY_SHUTDOWN = auto()
    OCF_ERR_DIRTY_EXISTS = auto()
    OCF_ERR_FLUSHING_INTERRUPTED = auto()
    OCF_ERR_CANNOT_ADD_CORE_TO_POOL = auto()
    OCF_ERR_CACHE_IN_INCOMPLETE_STATE = auto()
    OCF_ERR_CORE_IN_INACTIVE_STATE = auto()
    OCF_ERR_INVALID_CACHE_MODE = auto()
    OCF_ERR_INVALID_CACHE_LINE_SIZE = auto()


class OcfError(BaseException):
    def __init__(self, msg, error_code):
        super().__init__(self, msg)
        self.error_code = OcfErrorCode(abs(error_code))
        self.msg = msg

    def __str__(self):
        return "{} ({})".format(self.msg, repr(self.error_code))


class SharedOcfObject(Structure):
    _instances_ = {}

    def __init__(self):
        super().__init__()
        type(self)._instances_[self._as_parameter_] = self

    @classmethod
    def get_instance(cls, ref: int):
        try:
            return cls._instances_[ref]
        except:
            print(
                "OcfSharedObject corruption. wanted: {} instances: {}".format(
                    ref, cls._instances_
                )
            )
            return None

    @classmethod
    def del_object(cls, ref: int):
        del cls._instances_[ref]


class Uuid(Structure):
    _fields_ = [("_size", c_size_t), ("_data", c_char_p)]


class CacheLineSize(IntEnum):
    LINE_4KiB = S.from_KiB(4)
    LINE_8KiB = S.from_KiB(8)
    LINE_16KiB = S.from_KiB(16)
    LINE_32KiB = S.from_KiB(32)
    LINE_64KiB = S.from_KiB(64)
    DEFAULT = LINE_4KiB


class CacheLines(S):
    def __init__(self, count: int, line_size: CacheLineSize):
        self.bytes = count * line_size
        self.line_size = line_size

    def __int__(self):
        return int(self.bytes / self.line_size)

    def __str__(self):
        return "{} ({})".format(int(self), super().__str__())

    __repr__ = __str__
