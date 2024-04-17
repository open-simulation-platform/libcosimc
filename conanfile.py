import os

from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout
from conan.tools.env import VirtualRunEnv
from conan.tools.files import load


class LibCosimCConan(ConanFile):
    # Basic package info
    name = "libcosimc"

    def set_version(self):
        self.version = load(self, os.path.join(self.recipe_folder, "version.txt")).strip()

    # Metadata
    license = "MPL-2.0"
    author = "osp"
    description = "A C wrapper for libcosim, a co-simulation library for C++"

    # Binary configuration
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": True,
        "fPIC": True,
    }

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        self.options["*"].shared = self.options.shared

    # Dependencies/requirements
    tool_requires = (
        "cmake/[>=3.15]",
        "doxygen/[>=1.8]",
    )
    requires = (
        "libcosim/0.10.3@osp/stable",
    )

    # Exports
    exports = "version.txt"
    exports_sources = "*"

    # Build steps
    generators = "CMakeDeps", "CMakeToolchain"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        cmake.build(target="doc")
        if self._is_tests_enabled():
            env = VirtualRunEnv(self).environment()
            env.define("CTEST_OUTPUT_ON_FAILURE", "ON")
            with env.vars(self).apply():
                cmake.test()

    # Packaging
    def package(self):
        cmake = CMake(self)
        cmake.install()
        cmake.build(target="install-doc")

    def package_info(self):
        self.cpp_info.libs = [ "cosimc" ]
        # Ensure that consumers use our CMake package configuration files
        # rather than ones generated by Conan.
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs.append(".")

    # Helper functions
    def _is_tests_enabled(self):
        return os.getenv("LIBCOSIMC_RUN_TESTS_ON_CONAN_BUILD", "False").lower() in ("true", "1")
