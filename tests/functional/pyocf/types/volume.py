#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from ctypes import *
from hashlib import md5
from collections import defaultdict

from .io import Io, IoOps, IoDir
from .shared import OcfError
from ..ocf import OcfLib
from ..utils import print_buffer, Size as S
from .data import Data


class VolumeCaps(Structure):
    _fields_ = [("_atomic_writes", c_uint32, 1)]


class VolumeOps(Structure):
    SUBMIT_IO = CFUNCTYPE(None, POINTER(Io))
    SUBMIT_FLUSH = CFUNCTYPE(None, c_void_p)
    SUBMIT_METADATA = CFUNCTYPE(None, c_void_p)
    SUBMIT_DISCARD = CFUNCTYPE(None, c_void_p)
    SUBMIT_WRITE_ZEROES = CFUNCTYPE(None, c_void_p)
    OPEN = CFUNCTYPE(c_int, c_void_p)
    CLOSE = CFUNCTYPE(None, c_void_p)
    GET_MAX_IO_SIZE = CFUNCTYPE(c_uint, c_void_p)
    GET_LENGTH = CFUNCTYPE(c_uint64, c_void_p)

    _fields_ = [
        ("_submit_io", SUBMIT_IO),
        ("_submit_flush", SUBMIT_FLUSH),
        ("_submit_metadata", SUBMIT_METADATA),
        ("_submit_discard", SUBMIT_DISCARD),
        ("_submit_write_zeroes", SUBMIT_WRITE_ZEROES),
        ("_open", OPEN),
        ("_close", CLOSE),
        ("_get_max_io_size", GET_MAX_IO_SIZE),
        ("_get_length", GET_LENGTH),
    ]


class VolumeProperties(Structure):
    _fields_ = [
        ("_name", c_char_p),
        ("_io_priv_size", c_uint32),
        ("_volume_priv_size", c_uint32),
        ("_caps", VolumeCaps),
        ("_ops", VolumeOps),
        ("_io_ops", IoOps),
    ]


class VolumeIoPriv(Structure):
    _fields_ = [("_data", c_void_p)]


class Volume(Structure):
    _fields_ = [("_storage", c_void_p)]
    _instances_ = {}
    _uuid_ = {}

    def __init__(self, size: S, uuid=None):
        self.size = size
        if uuid:
            if uuid in type(self)._uuid_:
                raise Exception("Volume with uuid {} already created".format(uuid))
            self.uuid = uuid
        else:
            self.uuid = str(id(self))

        type(self)._uuid_[self.uuid] = self

        self.data = create_string_buffer(int(self.size))
        self._storage = cast(self.data, c_void_p)
        self.reset_stats()

    @classmethod
    def get_props(cls):
        return VolumeProperties(
            _name=str(cls.__name__).encode("ascii"),
            _io_priv_size=sizeof(VolumeIoPriv),
            _volume_priv_size=0,
            _caps=VolumeCaps(_atomic_writes=0),
            _ops=VolumeOps(
                _submit_io=cls._submit_io,
                _submit_flush=cls._submit_flush,
                _submit_metadata=cls._submit_metadata,
                _submit_discard=cls._submit_discard,
                _submit_write_zeroes=cls._submit_write_zeroes,
                _open=cls._open,
                _close=cls._close,
                _get_max_io_size=cls._get_max_io_size,
                _get_length=cls._get_length,
            ),
            _io_ops=IoOps(_set_data=cls._io_set_data, _get_data=cls._io_get_data),
        )

    @classmethod
    def get_instance(cls, ref):
        return cls._instances_[ref]

    @classmethod
    def get_by_uuid(cls, uuid):
        return cls._uuid_[uuid]

    @staticmethod
    @VolumeOps.SUBMIT_IO
    def _submit_io(io):
        io_structure = cast(io, POINTER(Io))
        volume = Volume.get_instance(io_structure.contents._volume)

        volume.submit_io(io_structure)

    @staticmethod
    @VolumeOps.SUBMIT_FLUSH
    def _submit_flush(flush):
        io_structure = cast(io, POINTER(Io))
        volume = Volume.get_instance(io_structure.contents._volume)

        volume.submit_flush(io_structure)

    @staticmethod
    @VolumeOps.SUBMIT_METADATA
    def _submit_metadata(meta):
        pass

    @staticmethod
    @VolumeOps.SUBMIT_DISCARD
    def _submit_discard(discard):
        io_structure = cast(io, POINTER(Io))
        volume = Volume.get_instance(io_structure.contents._volume)

        volume.submit_discard(io_structure)

    @staticmethod
    @VolumeOps.SUBMIT_WRITE_ZEROES
    def _submit_write_zeroes(write_zeroes):
        pass

    @staticmethod
    @CFUNCTYPE(c_int, c_void_p)
    def _open(ref):
        uuid_ptr = cast(OcfLib.getInstance().ocf_volume_get_uuid(ref), c_void_p)
        uuid_str = cast(
            OcfLib.getInstance().ocf_uuid_to_str_wrapper(uuid_ptr), c_char_p
        )
        uuid = str(uuid_str.value, encoding="ascii")
        try:
            volume = Volume.get_by_uuid(uuid)
        except:
            print("Tried to access unallocated volume {}".format(uuid))
            print("{}".format(Volume._uuid_))
            return -1

        type(volume)._instances_[ref] = volume

        return volume.open()

    @staticmethod
    @VolumeOps.CLOSE
    def _close(ref):
        Volume.get_instance(ref).close()
        del Volume._instances_[ref]

    @staticmethod
    @VolumeOps.GET_MAX_IO_SIZE
    def _get_max_io_size(ref):
        return S.from_KiB(128)

    @staticmethod
    @VolumeOps.GET_LENGTH
    def _get_length(ref):
        return Volume.get_instance(ref).get_length()

    @staticmethod
    @IoOps.SET_DATA
    def _io_set_data(io, data, offset):
        io_priv = cast(OcfLib.getInstance().ocf_io_get_priv(io), POINTER(VolumeIoPriv))
        data = Data.get_instance(data)
        data.position = offset
        io_priv.contents._data = cast(data, c_void_p)
        return 0

    @staticmethod
    @IoOps.GET_DATA
    def _io_get_data(io):
        io_priv = cast(OcfLib.getInstance().ocf_io_get_priv(io), POINTER(VolumeIoPriv))
        return io_priv.contents._data

    def open(self):
        return 0

    def close(self):
        pass

    def get_length(self):
        return self.size

    def submit_flush(self, flush):
        flush.contents._end(io, 0)

    def submit_discard(self, discard):
        discard.contents._end(io, 0)

    def get_stats(self):
        return self.stats

    def reset_stats(self):
        self.stats = {IoDir.WRITE: 0, IoDir.READ: 0}

    def submit_io(self, io):
        try:
            self.stats[io.contents._dir] += 1
            if io.contents._dir == IoDir.WRITE:
                src_ptr = cast(io.contents._ops.contents._get_data(io), c_void_p)
                src = Data.get_instance(src_ptr.value)
                dst = self._storage + io.contents._addr
            elif io.contents._dir == IoDir.READ:
                dst_ptr = cast(io.contents._ops.contents._get_data(io), c_void_p)
                dst = Data.get_instance(dst_ptr.value)
                src = self._storage + io.contents._addr

            memmove(dst, src, io.contents._bytes)

            io.contents._end(io, 0)
        except:
            io.contents._end(io, -5)

    def dump_contents(self, stop_after_zeros=0, offset=0, size=0):
        if size == 0:
            size = self.size
        print_buffer(self._storage + offset, size, stop_after_zeros=stop_after_zeros)

    def md5(self):
        m = md5()
        m.update(string_at(self._storage, self.size))
        return m.hexdigest()


class ErrorDevice(Volume):
    def __init__(self, size, error_sectors: set = None, uuid=None):
        super().__init__(size, uuid)
        self.error_sectors = error_sectors or set()

    def set_mapping(self, error_sectors: set):
        self.error_sectors = error_sectors

    def submit_io(self, io):
        if io.contents._addr in self.error_sectors:
            io.contents._end(io, -5)
            self.stats["errors"][io.contents._dir] += 1
        else:
            super().submit_io(io)

    def reset_stats(self):
        super().reset_stats()
        self.stats["errors"] = {IoDir.WRITE: 0, IoDir.READ: 0}
