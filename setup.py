import os
import sys
import pathlib

from pybind11 import get_cmake_dir
from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as build_ext_orig


class CMakeExtension(Extension):

    def __init__(self, name):
        # don't invoke the original build_ext for this special extension
        super().__init__(name, sources=[])


class build_ext(build_ext_orig):

    def run(self):
        old_inplace, self.inplace = self.inplace, 0
        # custom build
        for ext in self.extensions:
            self.build_cmake(ext)
        # copy
        self.inplace = old_inplace
        if old_inplace:
            self.copy_extensions_to_source()

    def build_cmake(self, ext):
        cwd = pathlib.Path().absolute()

        dst_path = ext.name.replace('.', '/') 
        os.makedirs(os.path.join(cwd, os.path.dirname(dst_path)), exist_ok=True)

        src_dir = os.path.join(cwd, "src")

        # these dirs will be created in build_py, so if you don't have
        # any python sources to bundle, the dirs will be missing
        build_temp = pathlib.Path(os.path.join(self.build_temp, "ffutils"))
        build_temp.mkdir(parents=True, exist_ok=True)
        extdir = pathlib.Path(os.path.dirname(self.get_ext_fullpath(ext.name)))
        extdir.mkdir(parents=True, exist_ok=True)
        print("> Build cpp module:", ext.name)
        print(">   source code dir:", src_dir)
        print(">   build dir:", build_temp)
        print(">   lib dir:", extdir)

        # example of cmake args
        config = 'Debug' if self.debug else 'Release'
        cmake_args = [
            '-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + str(extdir.absolute()),
            '-DCMAKE_BUILD_TYPE=' + config,
            '-DPYTHON_EXECUTABLE=' + sys.executable,
            '-Dpybind11_DIR=' + get_cmake_dir(),
        ]

        # example of build args
        build_args = [
            '--config', config,
            '--', '-j4'
        ]

        os.chdir(str(build_temp))
        self.spawn(['cmake', str(src_dir)] + cmake_args)
        if not self.dry_run:
            self.spawn(['cmake', '--build', '.'] + build_args)
        # Troubleshooting: if fail on line above then delete all possible 
        # temporary CMake files including "CMakeCache.txt" in top level dir.
        os.chdir(str(cwd))


setup(
    name='ffutils',
    version='0.0.1',
    packages=['ffutils'],
    ext_modules=[
        CMakeExtension('ffutils.bind.ffutils'),
    ],
    cmdclass={
        'build_ext': build_ext,
    },
    install_requires=[
        'numpy'
    ]
)
