vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO stdiobus/stdiobus-cpp
    REF "v${VERSION}"
    SHA512 0  # Updated during port submission
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSTDIOBUS_BUILD_TESTS=OFF
        -DSTDIOBUS_BUILD_EXAMPLES=OFF
        -DSTDIOBUS_BUILD_BENCHMARKS=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME stdiobus CONFIG_PATH lib/cmake/stdiobus)
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
