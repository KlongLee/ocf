#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import pytest

from pyocf.types.cache import Cache, CacheMode
from pyocf.types.volume import Volume
from pyocf.utils import Size as S
from pyocf.types.shared import CacheLineSize


@pytest.mark.parametrize("cache_mode", CacheMode)
@pytest.mark.parametrize("cls", CacheLineSize)
def test_change_cache_mode(pyocf_ctx, cache_mode, cls):
    # Start cache device
    cache_device = Volume(S.from_MiB(100))
    cache = Cache.start_on_device(cache_device, cache_mode=cache_mode, cache_line_size=cls)

    # Check if started with correct cache mode
    stats = cache.get_stats()
    assert stats["conf"]["cache_mode"] == cache_mode

    # For every cache mode check if after switch stats are as expected
    for mode in CacheMode:
        cache.change_cache_mode(mode)
        stats1 = cache.get_stats()
        assert stats1["conf"]["cache_mode"] == mode
