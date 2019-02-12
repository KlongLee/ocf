
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
    pass


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
    def _init(queue):
        q = Queue(queue)
        return 0

    @staticmethod
    @QueueOps.KICK_SYNC
    def _kick_sync(queue):
        Queue.get_instance(queue).kick_sync()

    @staticmethod
    @QueueOps.STOP
    def _stop(queue):
        Queue.get_instance(queue).stop()

    def kick_sync(self):
        OcfLib.getInstance().ocf_queue_run(self.queue)

    def stop(self):
        pass
