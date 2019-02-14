
#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
from ctypes import *

from ..ocf import OcfLib
from .shared import SharedOcfObject


class QueueOps(Structure):
    INIT = CFUNCTYPE(c_int, c_void_p)
    KICK = CFUNCTYPE(None, c_void_p)
    KICK_SYNC = CFUNCTYPE(None, c_void_p)
    STOP = CFUNCTYPE(None, c_void_p)

    _fields_ = [
        ("init", INIT),
        ("kick", KICK),
        ("kick_sync", KICK_SYNC),
        ("stop", STOP),
    ]


class Queue(SharedOcfObject):
    _instances_ = {}
    _fields_ = [("queue", c_void_p)]

    def __init__(self, queue):
        self.queue = queue
        self._as_parameter_ = self.queue
        super().__init__()

    @classmethod
    def get_ops(cls):
        return QueueOps(init=cls._init, kick_sync=cls._kick_sync, stop=cls._stop)

    @staticmethod
    @QueueOps.INIT
    def _init(ref):
        q = Queue(ref)
        return 0

    @staticmethod
    @QueueOps.KICK_SYNC
    def _kick_sync(ref):
        Queue.get_instance(ref).kick_sync()

    @staticmethod
    @QueueOps.STOP
    def _stop(ref):
        Queue.get_instance(ref).stop()

    def kick_sync(self):
        OcfLib.getInstance().ocf_queue_run(self.queue)

    def stop(self):
        pass
