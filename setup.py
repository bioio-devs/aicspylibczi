from setuptools import Extension, setup
from setuptools.command.build_ext import build_ext

import os
from pathlib import Path
import platform
import re
import subprocess
import sys
from distutils.version import LooseVersion


with open("README.md") as readme_file:
    readme = readme_file.read()

requirements = [
    "numpy>=1.14.1",
]

test_requirements = [
    "flake8",
    "pytest",
    "pytest-cov",
    "pytest-raises",
]

dev_requirements = [
    "bump2version>=1.0.1",
    "coverage>=5.0a4",
    "flake8>=3.7.7",
    "ipython>=7.5.0",
    "myst-parser>=4.0.0",
    "pytest>=4.3.0",
    "pytest-cov==2.6.1",
    "pytest-raises>=0.10",
    "pytest-runner>=4.4",
    "Sphinx",
    "sphinx_rtd_theme>=0.1.2",
    "breathe",
    "tox>=3.5.2",
    "twine>=1.13.0",
    "wheel>=0.33.1",
    "cmake",
]

setup_requirements = [
    "pytest-runner",
]

interactive_requirements = [
    "altair",
    "jupyterlab",
    "matplotlib",
    "pillow",
]

extra_requirements = {
    "test": test_requirements,
    "dev": dev_requirements,
    "setup": setup_requirements,
    "interactive": interactive_requirements,
    "all": [
        *requirements,
        *test_requirements,
        *setup_requirements,
        *dev_requirements,
        *interactive_requirements,
    ],
}


class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=""):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)


class CMakeBuild(build_ext):
    def build_extension(self, ext):
        path_var = os.environ.get("PATH")
        path_var = str(Path(sys.executable).parent) + ":" + path_var
        env = dict(os.environ.copy(), PATH=path_var)
        extdir = os.path.abspath(os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args = [
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + extdir,
            "-DPYTHON_EXECUTABLE=" + sys.executable,
        ]

        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        if platform.system() == "Windows":
            cmake_args += [
                "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), extdir)
            ]
            cmake_args += ["-DCMAKE_GENERATOR_PLATFORM=x64"]
            build_args += ["--", "/m"]
        else:
            cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg]
            build_args += ["--", "-j2"]

        env["CXXFLAGS"] = '{} -DVERSION_INFO=\\"{}\\"'.format(
            env.get("CXXFLAGS", ""), self.distribution.get_version()
        )
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(
            ["cmake", ext.sourcedir] + cmake_args, cwd=self.build_temp, env=env
        )
        subprocess.check_call(
            ["cmake", "--build", ".", "--target", "_aicspylibczi"] + build_args,
            cwd=self.build_temp,
            env=env,
        )


setup(
    name="aicspylibczi",
    # Do not edit this string manually, always use bumpversion
    # Details in CONTRIBUTING.md
    version="4.0.0",
    author="Jamie Sherman, Paul Watkins",
    author_email="jamies@alleninstitute.org, pwatkins@gmail.com",
    description="A python module and a python extension for Zeiss (CZI/ZISRAW) microscopy files.",
    long_description=readme,
    long_description_content_type="text/markdown",
    include_package_data=True,
    keywords="aicspylibczi, allen cell, imaging, computational biology",
    license="GPL-3.0-or-later",
    classifiers=[],
    ext_modules=[CMakeExtension("_aicspylibczi")],
    packages=["aicspylibczi"],
    cmdclass=dict(build_ext=CMakeBuild),
    install_requires=requirements,
    setup_requires=setup_requirements,
    test_suite="aicspylibczi/tests",
    tests_require=test_requirements,
    extras_require=extra_requirements,
    url="https://github.com/bioio-devs/aicspylibczi",
    zip_safe=False,
)
