#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class StdioBusCppConan(ConanFile):
    name = "stdiobus"
    version = "1.0.0"
    license = "Apache-2.0"
    author = "Raman Marozau <raman@stdiobus.com>, stdiobus contributors"
    url = "https://github.com/stdiobus/stdiobus-cpp"
    homepage = "https://stdiobus.com"
    description = "C++ SDK for stdio Bus - AI agent transport layer for MCP/ACP protocols"
    topics = ("stdiobus", "mcp", "acp", "agent", "transport", "json-rpc", "ipc")
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "fPIC": [True, False],
        "exceptions": [True, False],
    }
    default_options = {
        "fPIC": True,
        "exceptions": False,
    }
    exports_sources = (
        "CMakeLists.txt",
        "src/*",
        "include/*",
        "prebuilds/*",
        "LICENSE",
        "README.md",
        "stdiobus.pc.in",
    )
    # No external dependencies — libstdio_bus.a is bundled as prebuilt

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def layout(self):
        cmake_layout(self)

    def _get_prebuilt_triple(self):
        """Map Conan settings to prebuilt library triple (matches dist/lib/ layout)."""
        os_name = str(self.settings.os).lower()
        arch = str(self.settings.arch)

        if os_name == "macos":
            if arch == "armv8" or arch == "aarch64":
                return "aarch64-apple-darwin"
            else:
                return "x86_64-apple-darwin"
        elif os_name == "linux":
            if arch == "armv8" or arch == "aarch64":
                return "aarch64-unknown-linux-gnu"
            else:
                return "x86_64-unknown-linux-gnu"
        return None

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["STDIOBUS_CPP_EXCEPTIONS"] = self.options.exceptions
        tc.variables["STDIOBUS_BUILD_TESTS"] = False
        tc.variables["STDIOBUS_BUILD_EXAMPLES"] = False

        # Point to prebuilt libstdio_bus.a
        triple = self._get_prebuilt_triple()
        if triple:
            prebuilt_dir = os.path.join(self.source_folder, "prebuilds", triple)
            tc.variables["STDIO_BUS_LIB_DIR"] = prebuilt_dir

        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder,
             dst=os.path.join(self.package_folder, "licenses"))
        # C++ SDK headers
        copy(self, "*.hpp", src=os.path.join(self.source_folder, "include"),
             dst=os.path.join(self.package_folder, "include"))
        # C kernel header (required by ffi.hpp)
        triple = self._get_prebuilt_triple()
        if triple:
            prebuilt_dir = os.path.join(self.source_folder, "prebuilds", triple)
            include_dir = os.path.join(prebuilt_dir, "..", "..", "..", "include")
            copy(self, "stdio_bus_embed.h", src=include_dir,
                 dst=os.path.join(self.package_folder, "include"))
        # Built SDK library
        copy(self, "*.a", src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*.lib", src=self.build_folder,
             dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        # Prebuilt kernel library
        if triple:
            prebuilt_dir = os.path.join(self.source_folder, "prebuilds", triple)
            copy(self, "libstdio_bus.a", src=prebuilt_dir,
                 dst=os.path.join(self.package_folder, "lib"))

    def package_info(self):
        self.cpp_info.libs = ["stdiobus", "stdio_bus"]
        self.cpp_info.set_property("cmake_file_name", "stdiobus")
        self.cpp_info.set_property("cmake_target_name", "stdiobus::stdiobus")

        if self.options.exceptions:
            self.cpp_info.defines = ["STDIOBUS_CPP_EXCEPTIONS=1"]

        if self.settings.os in ["Linux", "FreeBSD"]:
            self.cpp_info.system_libs = ["pthread"]
