#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest
from ctypes import c_int

from pyocf.types.cache import Cache, CacheMode
from pyocf.types.core import Core
from pyocf.types.volume import Volume
from pyocf.utils import Size as S
from pyocf.types.data import Data, DataOps
from pyocf.types.ctx import OcfCtx
from pyocf.types.logger import DefaultLogger, LogLevel
from pyocf.ocf import OcfLib
from pyocf.types.metadata_updater import MetadataUpdater
from pyocf.types.cleaner import Cleaner
from pyocf.types.io import IoDir
from pyocf.types.shared import OcfCompletion


class DataCopyTracer(Data):
    """
        This class enables tracking whether each copied over Data instance is
        then securely erased.
    """
    copied_instances = []

    @staticmethod
    @DataOps.ALLOC
    def _alloc(pages):
        data = DataCopyTracer.pages(pages)
        Data._ocf_instances_.append(data)

        return data.handle.value

    def copy(self, src, end, start, size):
        DataCopyTracer.copied_instances.append(self)
        return super().copy(src, end, start, size)

    def secure_erase(self):
        DataCopyTracer.copied_instances.remove(self)
        return super().secure_erase()


@pytest.mark.security
@pytest.mark.parametrize(
    "cache_mode", [CacheMode.WT, CacheMode.WB, CacheMode.WA, CacheMode.WI]
)
def test_secure_erase_simple_io(cache_mode):
    """
        Perform simple IO which will trigger read misses, which in turn should
        trigger backfill. Track all the data copied over for backfill and make
        sure OCF calls secure erase on them.
    """
    ctx = OcfCtx(
        OcfLib.getInstance(),
        b"Security tests ctx",
        DefaultLogger(LogLevel.WARN),
        DataCopyTracer,
        MetadataUpdater,
        Cleaner,
    )

    ctx.register_volume_type(Volume)

    cache_device = Volume(S.from_MiB(30))
    cache = Cache.start_on_device(cache_device, cache_mode=cache_mode)

    core_device = Volume(S.from_MiB(50))
    core = Core.using_device(core_device)
    cache.add_core(core)

    write_data = Data.from_string("This is test data")
    io = core.new_io()
    io.set_data(write_data)
    io.configure(20, write_data.size, IoDir.WRITE, 0, 0)
    io.set_queue(cache.get_default_queue())

    cmpl = OcfCompletion([("err", c_int)])
    io.callback = cmpl.callback
    io.submit()
    cmpl.wait()

    cmpls = []
    for i in range(100):
        read_data = Data(500)
        io = core.new_io()
        io.set_data(read_data)
        io.configure(
            (i * 1259) % int(core_device.size), read_data.size, IoDir.READ, 0, 0
        )
        io.set_queue(cache.get_default_queue())

        cmpl = OcfCompletion([("err", c_int)])
        io.callback = cmpl.callback
        cmpls.append(cmpl)
        io.submit()

    for c in cmpls:
        c.wait()

    write_data = Data.from_string("TEST DATA" * 100)
    io = core.new_io()
    io.set_data(write_data)
    io.configure(500, write_data.size, IoDir.WRITE, 0, 0)
    io.set_queue(cache.get_default_queue())

    cmpl = OcfCompletion([("err", c_int)])
    io.callback = cmpl.callback
    io.submit()
    cmpl.wait()

    stats = cache.get_stats()

    for cache in ctx.caches[:]:
        cache.stop()
    ctx.exit()

    assert (
        len(DataCopyTracer.copied_instances) == 0
    ), "Not all copied Data instances were secure erased!"
    assert (
        stats["req"]["rd_partial_misses"]["value"]
        + stats["req"]["rd_full_misses"]["value"]
    ) > 0
