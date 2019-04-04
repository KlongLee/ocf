#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#


from pyocf.types.logger import LogLevel, DefaultLogger, BufferLogger
from pyocf.types.volume import Volume, ErrorDevice
from pyocf.types.ctx import get_default_ctx
from pyocf.utils import Size as Size
from pyocf.types.cache import Cache
from pyocf.types.core import Core
import pytest


@pytest.fixture()
def pyocf_ctx():
    c = get_default_ctx(DefaultLogger(LogLevel.WARN))
    c.register_volume_type(Volume)
    c.register_volume_type(ErrorDevice)

    yield c
    for cache in c.caches:
        cache.stop(flush=False)
    c.exit()


@pytest.fixture()
def pyocf_ctx_log_buffer():
    logger = BufferLogger(LogLevel.DEBUG)
    c = get_default_ctx(logger)
    c.register_volume_type(Volume)
    c.register_volume_type(ErrorDevice)
    yield logger
    for cache in c.caches:
        cache.stop(flush=False)


@pytest.fixture()
def start_cache_with_core():
    def start_with_config(cache_size: Size, core_size: Size, **config):
        cache_device = Volume(cache_size)
        core_device = Volume(core_size)
        cache = Cache.start_on_device(cache_device, **config)
        core = Core.using_device(core_device)
        cache.add_core(core)
        cache_device.reset_stats()
        core_device.reset_stats()
        return cache_device, core_device, cache, core

    return start_with_config
