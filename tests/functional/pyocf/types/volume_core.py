#
# Copyright(c) 2022 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause
#

from .core import Core
from .volume_exp_obj import ExpObjVolume
from pyocf.types.io import IoDir


class CoreVolume(ExpObjVolume):
    def __init__(self, core, uuid=None):
        super().__init__(core, uuid)
        self.core = core

    def md5(self):
        return self.core.exp_obj_md5()
