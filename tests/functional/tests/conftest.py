#
# Copyright(c) 2019-2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

import os
import sys
import pytest
import gc

sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir))
from pyocf.types.logger import LogLevel, DefaultLogger, BufferLogger
from pyocf.types.volume import RamVolume, ErrorDevice
from pyocf.types.volume_cache import CacheVolume
from pyocf.types.volume_core import CoreVolume
from pyocf.types.ctx import OcfCtx


def pytest_configure(config):
    sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir))


@pytest.fixture()
def pyocf_ctx():
    c = OcfCtx.with_defaults(DefaultLogger(LogLevel.WARN))
    c.register_volume_type(RamVolume)
    c.register_volume_type(ErrorDevice)
    c.register_volume_type(CacheVolume)
    c.register_volume_type(CoreVolume)
    yield c
    c.exit()
    gc.collect()


@pytest.fixture()
def pyocf_ctx_log_buffer():
    logger = BufferLogger(LogLevel.DEBUG)
    c = OcfCtx.with_defaults(logger)
    c.register_volume_type(RamVolume)
    c.register_volume_type(ErrorDevice)
    c.register_volume_type(CacheVolume)
    c.register_volume_type(CoreVolume)
    yield logger
    c.exit()
    gc.collect()
