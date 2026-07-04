"""
Setup script for pysgn - Python binding for HPDC ABI.

Build:
    python setup.py build_ext --inplace

Or use pip:
    pip install .

Requirements:
    - pybind11 (pip install pybind11)
    - C++11 compatible compiler
"""

from pybind11.setup_helpers import Pybind11Extension, build_ext
from pybind11 import get_cmake_dir
import pybind11
from setuptools import setup, Extension

import os

pybind11_include = pybind11.get_include()
base = os.path.join(os.path.dirname(__file__), "..", "..")

sources = [
    os.path.join(base, "bindings", "python", "pysgn.cpp"),
    os.path.join(base, "src", "hc.c"),
    os.path.join(base, "src", "hc8.c"),
    os.path.join(base, "src", "hc16.c"),
    os.path.join(base, "src", "hc32.c"),
    os.path.join(base, "src", "hc64.c"),
    os.path.join(base, "src", "dc.c"),
    os.path.join(base, "src", "hpdc_sandbox.cpp"),
    os.path.join(base, "src", "hpdc_trie.cpp"),
    os.path.join(base, "src", "hpdc_engine.cpp"),
    os.path.join(base, "src", "hpdc_storage.cpp"),
    os.path.join(base, "src", "hpdc_network.cpp"),
    os.path.join(base, "src", "hpdc_plugin.cpp"),
    os.path.join(base, "src", "hc_simd.c"),
]

ext_modules = [
    Pybind11Extension(
        "pysgn",
        sources=sources,
        include_dirs=[pybind11_include, os.path.join(base, "include")],
        cxx_std=11,
        define_macros=[("SGN_PC_EXTENSION", "1")],
    ),
]

setup(
    name="pysgn",
    version="1.0.0",
    description="Python binding for HPDC (Hierarchical Positional Decimal Counter) ABI",
    long_description=open("README_pysgn.md").read() if os.path.exists("README_pysgn.md") else "",
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    zip_safe=False,
    python_requires=">=3.7",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: C++",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
    ],
)