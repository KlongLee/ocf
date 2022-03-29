import pytest
from datetime import timedelta
import logging
from random import randint
from contextlib import nullcontext as does_not_raise

from pyocf.types.volume_replicated import ReplicatedVolume
from pyocf.types.volume_cache import CacheVolume
from pyocf.types.volume_core import CoreVolume
from pyocf.types.volume import RamVolume
from pyocf.types.cache import Cache, CacheMetadataSegment, CacheMode
from pyocf.types.cache import Core
from pyocf.utils import Size
from pyocf.types.shared import CacheLineSize, OcfError, OcfErrorCode
from pyocf.types.ctx import OcfCtx
from pyocf.rio import Rio, ReadWrite
from pyocf.helpers import get_metadata_segment_size, get_metadata_segment_page_location

logger = logging.getLogger(__name__)


@pytest.mark.security
@pytest.mark.parametrize("cache_line_size", CacheLineSize)
@pytest.mark.parametrize(
    "bs",
    [
        Size.from_B(512),
        Size.from_KiB(1),
        Size.from_KiB(18),
        Size.from_KiB(128),
    ],
)
@pytest.mark.parametrize(
    "io_size",
    [
        Size.from_B(512),
        Size.from_KiB(10),
        Size.from_MiB(1),
        Size.from_MiB(10),
        Size.from_GiB(1),
    ],
)
@pytest.mark.parametrize("section", CacheMetadataSegment)
def test_garbage_on_cache_exported_object(pyocf_ctx, cache_line_size, bs, io_size, section):
    num_jobs = 1
    qd = 64

    vol_size = Size.from_MiB(100)
    cache_vol = RamVolume(vol_size)
    secondary_cache_volume = RamVolume(vol_size)

    cache = Cache(owner=OcfCtx.get_default(), cache_line_size=cache_line_size)

    cache.start_cache(init_default_io_queue=False)

    for i in range(num_jobs):
        cache.add_io_queue(f"io-queue-{i}")

    cache.standby_attach(cache_vol)
    start = get_metadata_segment_page_location(cache, section)
    count = get_metadata_segment_size(cache, section)
    io_offset = Size.from_page(start)

    r = (
        Rio()
        .target(cache)
        .njobs(num_jobs)
        .readwrite(ReadWrite.RANDWRITE)
        .io_size(io_size)
        .bs(bs)
        .qd(qd)
        .norandommap()
        .run(cache.io_queues)
    )

    cache.standby_detach()
    try:
        cache.standby_activate(secondary_cache_volume, open_cores=False)
    except OcfError as e:
        logger.info("Cache activation failed")
        return

    logger.warn("Cache activation succeeded - trying to use cache")

    core = Core(RamVolume(Size.from_MiB(100)))
    cache.add_core(core)

    r = (
        Rio()
        .target(core)
        .readwrite(ReadWrite.RANDWRITE)
        .io_size(Size.from_MiB(1))
        .run(cache.io_queues)
    )
