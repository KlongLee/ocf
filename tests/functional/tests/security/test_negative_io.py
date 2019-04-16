#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

from ctypes import c_int
from random import randrange

from pyocf.types.cache import Cache, Core
from pyocf.types.data import Data
from pyocf.types.io import IoDir
from pyocf.types.shared import OcfCompletion
from pyocf.types.volume import Volume
from pyocf.utils import Size


def test_neg_write_too_long_data(pyocf_ctx, c_uint16_randomize):
    """
        Check if writing data larger than exported object size is properly blocked
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(1))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    write_data = Data(int(Size.from_KiB(c_uint16_randomize)))
    io = core.new_io()
    io.set_data(write_data)
    io.configure(0, write_data.size, IoDir.WRITE, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if c_uint16_randomize > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


def test_neg_read_too_long_data(pyocf_ctx, c_uint16_randomize):
    """
        Check if reading data larger than exported object size is properly blocked
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(1))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    read_data = Data(int(Size.from_KiB(c_uint16_randomize)))
    io = core.new_io()
    io.set_data(read_data)
    io.configure(0, read_data.size, IoDir.READ, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if c_uint16_randomize > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


def test_neg_write_too_far(pyocf_ctx, c_uint16_randomize):
    """
        Check if writing data which would normally fit on exported object is
        blocked when offset is set so that data goes over exported device end
    """

    limited_size = c_uint16_randomize % (int(Size.from_KiB(4)) + 1)
    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(4))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    write_data = Data(int(Size.from_KiB(limited_size)))
    io = core.new_io()
    io.set_data(write_data)
    io.configure(int(Size.from_MiB(3)), write_data.size, IoDir.WRITE, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if limited_size > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


def test_neg_read_too_far(pyocf_ctx, c_uint16_randomize):
    """
        Check if reading data which would normally fit on exported object is
        blocked when offset is set so that data is read beyond exported device end
    """

    limited_size = c_uint16_randomize % (int(Size.from_KiB(4)) + 1)
    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(4))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    read_data = Data(int(Size.from_KiB(limited_size)))
    io = core.new_io()
    io.set_data(read_data)
    io.configure(int(Size.from_MiB(3)), read_data.size, IoDir.READ, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if limited_size > 1024:
        assert completion.results["err"] != 0
    else:
        assert completion.results["err"] == 0


def test_neg_write_offset_outside_of_device(pyocf_ctx, c_int_randomize):
    """
        Check that write operations are blocked when
        IO offset is located outside of device range
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(2))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    write_data = Data(int(Size.from_KiB(1)))
    io = core.new_io()
    io.set_data(write_data)
    io.configure(c_int_randomize, write_data.size, IoDir.WRITE, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if 0 <= c_int_randomize <= int(Size.from_MiB(2)) - int(Size.from_KiB(1)):
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


def test_neg_read_offset_outside_of_device(pyocf_ctx, c_int_randomize):
    """
        Check that read operations are blocked when
        IO offset is located outside of device range
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(2))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    read_data = Data(int(Size.from_KiB(1)))
    io = core.new_io()
    io.set_data(read_data)
    io.configure(c_int_randomize, read_data.size, IoDir.READ, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if 0 <= c_int_randomize <= int(Size.from_MiB(2)) - int(Size.from_KiB(1)):
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


def test_neg_io_class(pyocf_ctx, c_int_randomize):
    """
        Check that IO operations are blocked when IO class
        number is not in allowed values {0, ..., 32}
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(2))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    write_data = Data(int(Size.from_MiB(1)))
    io = core.new_io()
    io.set_data(write_data)
    io.configure(0, write_data.size, randrange(0, 2), c_int_randomize, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if 0 <= c_int_randomize <= 32:
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0


def test_neg_io_direction(pyocf_ctx, c_int_randomize):
    """
        Check that IO operations are not executed for unknown IO direction,
        that is when IO direction value is not in allowed values {0, 1}
    """

    cache_device = Volume(Size.from_MiB(20))
    core_device = Volume(Size.from_MiB(2))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    write_data = Data(int(Size.from_MiB(1)))
    io = core.new_io()
    io.set_data(write_data)
    io.configure(0, write_data.size, c_int_randomize, 0, 0)
    io.set_queue(cache.get_default_queue())

    completion = OcfCompletion([("err", c_int)])
    io.callback = completion.callback
    io.submit()
    completion.wait()

    if c_int_randomize in [0, 1]:
        assert completion.results["err"] == 0
    else:
        assert completion.results["err"] != 0
