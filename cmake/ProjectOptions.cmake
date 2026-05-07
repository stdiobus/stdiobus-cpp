#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# =============================================================================
# Project-wide Options
# =============================================================================

option(STDIOBUS_CPP_EXCEPTIONS "Enable exception throwing mode" OFF)
option(STDIOBUS_BUILD_TESTS "Build test targets" ON)
option(STDIOBUS_BUILD_EXAMPLES "Build example targets" ON)
option(STDIOBUS_BUILD_BENCHMARKS "Build benchmark targets" OFF)
option(STDIOBUS_BUILD_FUZZ "Build fuzz testing targets (requires Clang)" OFF)
option(STDIOBUS_WARNINGS_AS_ERRORS "Treat warnings as errors (CI mode)" OFF)
option(STDIOBUS_INSTALL "Generate install targets" ON)

set(STDIOBUS_SANITIZER "" CACHE STRING
    "Enable sanitizer (address, undefined, thread, address,undefined)")
