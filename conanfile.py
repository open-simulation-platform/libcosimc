import os

from conans import ConanFile, CMake, tools
from os import path


class LibCosimCConan(ConanFile):
    name = "libcosimc"
    author = "osp"
    exports = "version.txt"
    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto"
    }
    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake", "virtualrunenv"
    requires = (
        "libcosim/0.10.2@osp/stable"
        )

    def set_version(self):
        self.version = tools.load(path.join(self.recipe_folder, "version.txt")).strip()

    def imports(self):
        binDir = os.path.join("output", str(self.settings.build_type).lower(), "bin")
        self.copy("*.dll", dst=binDir, keep_path=False)
        self.copy("*.pdb", dst=binDir, keep_path=False)

    def configure_cmake(self):
        cmake = CMake(self)
        cmake.definitions["LIBCOSIMC_USING_CONAN"] = "ON"
        cmake.configure()
        return cmake

    def build(self):
        cmake = self.configure_cmake()
        cmake.build()
        cmake.build(target="doc")

    def package(self):
        cmake = self.configure_cmake()
        cmake.install()
        cmake.build(target="install-doc")

    def package_info(self):
        self.cpp_info.libs = [ "cosimc" ]
