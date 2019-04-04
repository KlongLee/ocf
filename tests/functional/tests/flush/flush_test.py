#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
import math
import logging
import random
from ctypes import c_int

from ...pyocf.types.core import Core
from ...pyocf.types.volume import Volume
from ...pyocf.types.data import Data
from ...pyocf.types.io import IoDir
from ...pyocf.types.shared import OcfCompletion
from ...pyocf.types.cache import CacheMode, CacheLineSize, Cache
from ...pyocf.utils import Size as Size


LOGGER = logging.getLogger(__name__)


def run_io(device, queue, data, direction):
    write_data = Data.from_bytes(data)
    io = device.new_io()
    io.set_data(write_data)
    io.configure(0, write_data.size, direction, 0, 0)
    io.set_queue(queue)

    cmpl = OcfCompletion([("err", c_int)])
    io.callback = cmpl.callback
    io.submit()
    cmpl.wait()
    assert cmpl.results["err"] == 0


def check_configuration(cache, expected_conf):
    stats = cache.get_stats()
    assert stats["conf"]["cache_line_size"] == expected_conf["cache_line_size"]
    assert stats["conf"]["cache_mode"] == expected_conf["cache_mode"]


def check_statistics(stats, expected_occupancy, expected_dirty, expected_clean):
    assert stats["usage"]["occupancy"]["value"] == expected_occupancy
    assert stats["usage"]["dirty"]["value"] == expected_dirty
    assert stats["usage"]["clean"]["value"] == expected_clean


@pytest.mark.xfail
@pytest.mark.parametrize("cls", CacheLineSize)
def test_wb_flush_cache(pyocf_ctx, start_cache_with_core, cls):
    cache_device, core_device, cache, core = start_cache_with_core(cache_mode=CacheMode.WB,
                                                                   cache_line_size=cls)

    check_configuration(cache, {"cache_line_size": cls, "cache_mode": CacheMode.WB})

    stats_before_io = cache.get_stats()

    io_size = Size.from_MiB(50)
    run_io(core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)

    stats_before_flush = cache.get_stats()
    check_statistics(stats_before_flush,
                     stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                     math.ceil(io_size.B / 4096),
                     stats_before_io["usage"]["clean"]["value"])
    assert core.exp_obj_md5() != core_device.md5()

    cache.flush()

    stats_after_flush = cache.get_stats()
    check_statistics(stats_after_flush,
                     stats_before_flush["usage"]["occupancy"]["value"],
                     0,
                     stats_before_flush["usage"]["clean"]["value"] + math.ceil(io_size.B / 4096))

    assert core.exp_obj_md5() == core_device.md5()

    cache.remove_core(core)
    cache.stop()


@pytest.mark.xfail
@pytest.mark.parametrize("cls", CacheLineSize)
def test_wb_stop_without_flushing(pyocf_ctx, start_cache_with_core, cls):
    cache_device, core_device, cache, core = start_cache_with_core(cache_mode=CacheMode.WB,
                                                                   cache_line_size=cls)

    check_configuration(cache, {"cache_line_size": cls, "cache_mode": CacheMode.WB})

    stats_before_io = cache.get_stats()

    io_size = Size.from_MiB(50)
    run_io(core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)

    stats_before_stop = cache.get_stats()

    check_statistics(stats_before_stop,
                     stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                     math.ceil(io_size.B / 4096),
                     stats_before_io["usage"]["clean"]["value"])
    assert core.exp_obj_md5() != core_device.md5()

    cache.stop(False)
    cache.load_cache(cache_device)

    stats_after_stop_without_flush = cache.get_stats()

    assert stats_after_stop_without_flush == stats_before_stop

    cache.stop()
    cache.load_cache(cache_device)

    stats_after_stop_with_flush = cache.get_stats()

    check_statistics(stats_after_stop_with_flush,
                     stats_before_stop["usage"]["occupancy"]["value"],
                     0,
                     stats_before_io["usage"]["clean"]["value"] + math.ceil(io_size.B / 4096))
    assert core.exp_obj_md5() == core_device.md5()

    cache.remove_core(core)
    cache.stop()


@pytest.mark.xfail
@pytest.mark.parametrize("cls", CacheLineSize)
@pytest.mark.parametrize("cm", [CacheMode.PT, CacheMode.WA, CacheMode.WT, CacheMode.WI])
def test_wb_change_cache_mode(pyocf_ctx, start_cache_with_core, cls, cm):
    cache_device, core_device, cache, core = start_cache_with_core(cache_mode=CacheMode.WB,
                                                                   cache_line_size=cls)

    check_configuration(cache, {"cache_line_size": cls, "cache_mode": CacheMode.WB})

    stats_before_io = cache.get_stats()

    io_size = Size.from_MiB(50)
    run_io(core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)

    stats_before_change_cache_mode = cache.get_stats()

    check_statistics(stats_before_change_cache_mode,
                     stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                     math.ceil(io_size.B / 4096),
                     stats_before_io["usage"]["clean"]["value"])
    assert core.exp_obj_md5() != core_device.md5()

    cache.change_cache_mode(cm)

    stats_after = cache.get_stats()

    check_statistics(stats_after,
                     stats_before_change_cache_mode["usage"]["occupancy"]["value"],
                     0,
                     stats_before_change_cache_mode["usage"]["clean"]["value"] + math.ceil(io_size.B / 4096))
    assert core.exp_obj_md5() == core_device.md5()

    cache.remove_core(core)
    cache.stop()


@pytest.mark.xfail
@pytest.mark.parametrize("cls", CacheLineSize)
def test_wb_flush_core(pyocf_ctx, start_cache_with_core, cls):
    cache_device, core_device, cache, core = start_cache_with_core(cache_mode=CacheMode.WB,
                                                                   cache_line_size=cls)
    second_core_device = Volume(Size.from_MiB(200))
    second_core = Core.using_device(second_core_device)
    cache.add_core(second_core)
    core_device.reset_stats()

    cache_stats_before_io = cache.get_stats()
    core_stats_before_io = core.get_stats()
    second_core_stats_before_io = second_core.get_stats()

    io_size = Size.from_MiB(20)
    run_io(core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)
    run_io(second_core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)

    cache_stats_after_io = cache.get_stats()
    core_stats_after_io = core.get_stats()
    second_core_stats_after_io = second_core.get_stats()

    check_statistics(cache_stats_after_io,
                     cache_stats_before_io["usage"]["occupancy"]["value"] + 2 * math.ceil(io_size.B / 4096),
                     2 * math.ceil(io_size.B / 4096),
                     cache_stats_before_io["usage"]["clean"]["value"])
    check_statistics(core_stats_after_io,
                     core_stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                     math.ceil(io_size.B / 4096),
                     core_stats_before_io["usage"]["clean"]["value"])
    check_statistics(second_core_stats_after_io,
                     second_core_stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                     math.ceil(io_size.B / 4096),
                     second_core_stats_before_io["usage"]["clean"]["value"])

    core.flush()

    cache_stats_after_core_flush = cache.get_stats()
    core_stats_after_core_flush = core.get_stats()
    second_core_stats_after_core_flush = second_core.get_stats()

    check_statistics(cache_stats_after_core_flush,
                     cache_stats_after_io["usage"]["occupancy"]["value"],
                     math.ceil(io_size.B / 4096),
                     cache_stats_after_io["usage"]["clean"]["value"] - math.ceil(io_size.B / 4096))
    check_statistics(core_stats_after_core_flush,
                     core_stats_after_io["usage"]["occupancy"]["value"],
                     0,
                     core_stats_after_io["usage"]["clean"]["value"] + math.ceil(io_size.B / 4096))
    check_statistics(second_core_stats_after_core_flush,
                     second_core_stats_after_io["usage"]["occupancy"]["value"],
                     second_core_stats_after_io["usage"]["dirty"]["value"],
                     second_core_stats_after_io["usage"]["clean"]["value"])

    assert core.exp_obj_md5() == core_device.md5()
    assert second_core.exp_obj_md5() != second_core_device.md5()

    cache.remove_core(core)
    cache.remove_core(second_core)

    check_statistics(cache.get_stats(), 0, 0, 0)
    cache.stop()


@pytest.mark.xfail
@pytest.mark.parametrize("cls", CacheLineSize)
def test_wb_100_flush(pyocf_ctx, start_cache_with_core, cls):
    cache_device, core_device, cache, core = start_cache_with_core(cache_mode=CacheMode.WB, cache_line_size=cls)
    check_configuration(cache, {"cache_line_size": cls, "cache_mode": CacheMode.WB})

    stats_before_io = cache.get_stats()
    for i in range(0, 100):
        io_size = Size.from_MiB(random.randint(1, 100))
        run_io(core, cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)

        cache.flush()
        stats_after_flush = cache.get_stats()
        check_statistics(stats_after_flush,
                         stats_before_io["usage"]["occupancy"]["value"] + math.ceil(io_size.B / 4096),
                         0,
                         stats_before_io["usage"]["clean"]["value"] + math.ceil(io_size.B / 4096))
        assert core.exp_obj_md5() == core_device.md5()
        stats_before_io = stats_after_flush

    cache.remove_core(core)
    cache.stop()


@pytest.mark.parametrize("cls", CacheLineSize)
def test_wb_flush_50core(pyocf_ctx, cls):
    cache_device = Volume(Size.from_MiB(100))
    cache = Cache.start_on_device(cache_device, cache_mode=CacheMode.WB, cache_line_size=cls)
    core_devices = []
    core_exp_objects = []
    core_count = 50
    io_size = Size.from_MiB(10)

    for i in range(0, core_count):
        core_device = Volume(Size.from_MiB(20))
        core = Core.using_device(core_device)
        core_exp_objects.append(core)
        core_devices.append(core_device)
        cache.add_core(core)

    for i in range(0, core_count):
        run_io(core_exp_objects[i], cache.get_default_queue(), bytes(io_size.B), IoDir.WRITE)
        core_exp_objects[i].flush()
        assert core_exp_objects[i].exp_obj_md5() == core_devices[i].md5()

    cache.stop()
