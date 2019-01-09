#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
from ctypes import *


def print_buffer(buf, length, offset=0, width=16, stop_after_zeros=0):
    end = offset + length
    zero_lines = 0
    buf = string_at(buf, length)
    whole_buffer_empty = True
    stop_after_zeros = int(stop_after_zeros / width)

    for addr in range(offset, end, width):
        cur_line = buf[addr : min(end, addr + width)]
        all_zeros = True
        byteline = ""
        asciiline = ""
        if not any(cur_line):
            if stop_after_zeros and zero_lines > stop_after_zeros:
                print(
                    "<{} bytes of empty space encountered, stopping>".format(
                        stop_after_zeros * width
                    )
                )
                return
            zero_lines += 1
            continue

        if zero_lines:
            print("<{} zero bytes omitted>".format(zero_lines * width))
            zero_lines = 0

        for x in cur_line:
            x = int(x)
            byteline += "{:02X} ".format(x)
            if 31 < x < 126:
                char = chr(x)
            else:
                char = "."
            asciiline += char

        print("{:#08X}\t{}\t{}".format(addr, byteline, asciiline))
        whole_buffer_empty = False

    if whole_buffer_empty:
        print("<whole buffer empty>")
    elif zero_lines:
        print("<zero until end>".format(zero_lines * width))


class Time:
    _ms = 1000
    _s = _ms * 1000
    _min = _s * 60
    _h = _min * 60
    _d = _h * 24

    def __init__(self, ns: int):
        self.ns = ns

    def __int__(self):
        return self.ns

    @classmethod
    def from_ns(cls, value: int):
        return cls(value)

    @classmethod
    def from_ms(cls, value: int):
        return cls(value * cls._ms)

    @classmethod
    def from_s(cls, value: int):
        return cls(value * cls._s)

    @classmethod
    def from_min(cls, value: int):
        return cls(value * cls._min)

    @classmethod
    def from_h(cls, value: int):
        return cls(value * cls._h)

    @classmethod
    def from_d(cls, value: int):
        return cls(value * cls._d)

    @property
    def ms(self):
        return self.ns / self._ms

    @property
    def s(self):
        return self.ns / self._s

    @property
    def min(self):
        return self.ns / self._min

    @property
    def min(self):
        return self.ns / self._min

    @property
    def h(self):
        return self.ns / self._h

    @property
    def d(self):
        return self.ns / self._d

    def __isub__(self, other):
        self.ns -= int(other)

    def __iadd__(self, other):
        self.ns += int(other)

    def __str__(self):
        ret = ""
        rest = Time(int(self))

        if int(rest.d):
            ret += "{}:".format(int(rest.d))
            rest -= Time.from_d(int(rest.d))
        if int(rest.h) or ret:
            ret += "{:02}:".format(int(rest.h))
            rest -= Time.from_h(int(rest.h))
        if int(rest.min) or ret:
            ret += "{:02}:".format(int(rest.min))
            rest -= Time.from_min(int(rest.min))

        ret += "{:02}".format(int(rest.s))
        rest.ns -= int(Time.from_s(int(rest.s)))

        if int(rest):
            ret += ".{}".format(int(rest))
        else:
            ret += "s"

        return ret

    __repr__ = __str__


class Size:
    _KiB = 1024
    _MiB = _KiB * 1024
    _GiB = _MiB * 1024
    _TiB = _GiB * 1024

    def __init__(self, b: int):
        self.bytes = b

    def __int__(self):
        return self.bytes

    @classmethod
    def from_B(cls, value):
        return cls(value)

    @classmethod
    def from_KiB(cls, value):
        return cls(value * cls._KiB)

    @classmethod
    def from_MiB(cls, value):
        return cls(value * cls._MiB)

    @classmethod
    def from_GiB(cls, value):
        return cls(value * cls._GiB)

    @classmethod
    def from_TiB(cls, value):
        return cls(value * cls._TiB)

    @property
    def B(self):
        return self.bytes

    @property
    def KiB(self):
        return self.bytes / self._KiB

    @property
    def MiB(self):
        return self.bytes / self._MiB

    @property
    def GiB(self):
        return self.bytes / self._GiB

    @property
    def TiB(self):
        return self.bytes / self._TiB

    def __str__(self):
        if self.bytes < self._KiB:
            return "{} B".format(self.B)
        elif self.bytes < self._MiB:
            return "{} KiB".format(self.KiB)
        elif self.bytes < self._GiB:
            return "{} MiB".format(self.MiB)
        elif self.bytes < self._TiB:
            return "{} GiB".format(self.GiB)
        else:
            return "{} TiB".format(self.TiB)

    __repr__ = __str__


def print_structure(struct, indent=0):
    print(struct)
    for field, field_type in struct._fields_:
        value = getattr(struct, field)
        if hasattr(value, "_fields_"):
            print("{}{: <20} :".format("   " * indent, field))
            print_structure(value, indent=indent + 1)
            continue

        print("{}{: <20} : {}".format("   " * indent, field, value))


def struct_to_dict(struct):
    d = {}
    for field, field_type in struct._fields_:
        value = getattr(struct, field)
        if hasattr(value, "_fields_"):
            d[field] = struct_to_dict(value)
            continue
        d[field] = value

    return d
