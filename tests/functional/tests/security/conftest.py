#
# Copyright(c) 2019 Intel Corporation
# SPDX-License-Identifier: BSD-3-Clause-Clear
#

import os
import sys
import random
import string
from ctypes import (
    c_uint64,
    c_uint32,
    c_uint16,
)

import pytest

sys.path.append(os.path.join(os.path.dirname(__file__), os.path.pardir))


def generate_random_numbers(c_type):
    type_dict = {
        c_uint16: [0, c_uint16(-1).value],
        c_uint32: [0, c_uint32(-1).value],
        c_uint64: [0, c_uint64(-1).value]
    }

    values = []
    for i in range(0, 1000):
        values.append(random.randint(type_dict[c_type][0], type_dict[c_type][1]))
    return values


def generate_random_strings():
    values = []
    for t in [string.digits,
              string.ascii_letters + string.digits,
              string.ascii_lowercase,
              string.ascii_uppercase,
              string.printable,
              string.punctuation,
              string.hexdigits]:
        for i in range(0, 50):
            values.append(''.join(random.choice(t) for _ in range(random.randint(0, 20))))
    return values


def get_random_ints(c_type):
    for value in generate_random_numbers(c_type):
        yield value


def get_random_strings():
    for value in generate_random_strings():
        yield value


def get_fuzzed_values(c_type):
    values = generate_random_numbers(c_type)
    values += generate_random_strings()

    for value in values:
        yield value


@pytest.fixture(params=get_random_ints(c_uint16))
def c_uint16_randomize(request):
    return request.param


@pytest.fixture(params=get_random_ints(c_uint32))
def c_uint32_randomize(request):
    return request.param


@pytest.fixture(params=get_random_ints(c_uint64))
def c_uint64_randomize(request):
    return request.param


@pytest.fixture(params=get_random_strings())
def string_randomize(request):
    return request.param

