#
# Copyright(c) 2019-2021 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from ctypes import c_int

from pyocf.types.cache import Cache
from pyocf.types.core import Core
from pyocf.types.volume import Volume, ErrorDevice
from pyocf.types.data import Data
from pyocf.types.io import IoDir
from pyocf.utils import Size as S
from pyocf.types.shared import OcfError, OcfCompletion
from pyocf.rio import Rio, ReadWrite


def test_ctx_fixture(pyocf_ctx):
    pass


def test_simple_wt_write(pyocf_ctx):
    cache_device = Volume(S.from_MiB(30))
    core_device = Volume(S.from_MiB(30))

    cache = Cache.start_on_device(cache_device)
    core = Core.using_device(core_device)

    cache.add_core(core)

    cache_device.reset_stats()
    core_device.reset_stats()

    r = Rio().target(core).readwrite(ReadWrite.WRITE).size(S.from_sector(1)).run()

    assert r.error_count == 0
    assert cache_device.get_stats()[IoDir.WRITE] == 1
    stats = cache.get_stats()
    assert stats["req"]["wr_full_misses"]["value"] == 1
    assert stats["usage"]["occupancy"]["value"] == 1

    assert core.exp_obj_md5() == core_device.md5()
    cache.stop()


def test_start_corrupted_metadata_lba(pyocf_ctx):
    cache_device = ErrorDevice(S.from_MiB(30), error_sectors=set([0]))

    with pytest.raises(OcfError, match="OCF_ERR_WRITE_CACHE"):
        cache = Cache.start_on_device(cache_device)


def test_load_cache_no_preexisting_data(pyocf_ctx):
    cache_device = Volume(S.from_MiB(30))

    with pytest.raises(OcfError, match="OCF_ERR_NO_METADATA"):
        cache = Cache.load_from_device(cache_device)


def test_load_cache(pyocf_ctx):
    cache_device = Volume(S.from_MiB(30))

    cache = Cache.start_on_device(cache_device)
    cache.stop()

    cache = Cache.load_from_device(cache_device)


def test_load_cache_recovery(pyocf_ctx):
    cache_device = Volume(S.from_MiB(30))

    cache = Cache.start_on_device(cache_device)

    device_copy = cache_device.get_copy()

    cache.stop()

    cache = Cache.load_from_device(device_copy)
