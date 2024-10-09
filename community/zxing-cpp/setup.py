import os
import platform
import subprocess
import sys
import shutil

from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext


# Adapted from here: https://github.com/pybind/cmake_example/blob/master/setup.py
class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        os.mkdir(extdir)
        with os.scandir(path="./build/wrappers/python") as it:
            for entry in it:
                if entry.name.endswith('.so'):
                    shutil.copy(entry.path, extdir)


with open("wrappers/python/README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='zxing-cpp',
    # setuptools_scm cannot be used because of the structure of the project until the following issues are solved:
    # https://github.com/pypa/setuptools_scm/issues/357
    # https://github.com/pypa/pip/issues/7549
    # Because pip works on a copy of current directory in a temporary directory, the temporary directory does not hold
    # the .git directory of the repo, so that setuptools_scm cannot guess the current version.
    # use_scm_version={
    #     "root": "../..",
    #     "version_scheme": "guess-next-dev",
    #     "local_scheme": "no-local-version",
    #     "tag_regex": "v?([0-9]+.[0-9]+.[0-9]+)",
    # },
    version='2.2.0',
    description='Python bindings for the zxing-cpp barcode library',
    long_description=long_description,
    long_description_content_type="text/markdown",
    author='ZXing-C++ Community',
    author_email='zxingcpp@gmail.com',
    url='https://github.com/zxing-cpp/zxing-cpp',
    license='Apache License 2.0',
    keywords=['barcode'],
    classifiers=[
        "Development Status :: 4 - Beta",
        "Programming Language :: Python :: 3",
        "License :: OSI Approved :: Apache Software License",
        "Operating System :: OS Independent",
        "Topic :: Multimedia :: Graphics",
    ],
    python_requires=">=3.6",
    ext_modules=[CMakeExtension('zxingcpp')],
    cmdclass=dict(build_ext=CMakeBuild),
    zip_safe=False,
)
