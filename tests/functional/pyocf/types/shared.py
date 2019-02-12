#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from ctypes import *
from enum import IntEnum

from ..utils import Size as S


class OcfError(BaseException):
    def __init__(self, msg, error_code):
        super().__init__(self, msg)
        self.error_code = error_code
        self.msg = msg

    def __str__(self):
        return "{} ({})".format(self.msg, self.error_code)


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
    LINE_4KiB = (S.from_KiB(4),)
    LINE_8KiB = (S.from_KiB(8),)
    LINE_16KiB = (S.from_KiB(16),)
    LINE_32KiB = (S.from_KiB(32),)
    LINE_64KiB = (S.from_KiB(64),)
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
