# Third-Party Dependencies

## Runtime Dependencies

### libstdio_bus (C kernel)

- **License:** Apache-2.0
- **Source:** https://github.com/stdiobus/stdiobus
- **Usage:** Prebuilt static library (`libstdio_bus.a`) linked into the SDK
- **Note:** Bundled in `prebuilds/` per platform triple

## Development/Test Dependencies

### GoogleTest v1.14.0

- **License:** BSD-3-Clause
- **Source:** https://github.com/google/googletest
- **Usage:** Unit and integration testing (fetched via CMake FetchContent at build time)
- **Note:** Not included in distributed packages; downloaded during test builds only

## No Other Runtime Dependencies

The stdio Bus C++ SDK has zero external runtime dependencies beyond the bundled C kernel library and the C++ standard library. This is by design to minimize supply chain risk and simplify deployment.
