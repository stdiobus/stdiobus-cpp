#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# =============================================================================
# Sanitizer Configuration
# =============================================================================
# Usage:
#   cmake -DSTDIOBUS_SANITIZER=address ..
#   cmake -DSTDIOBUS_SANITIZER=undefined ..
#   cmake -DSTDIOBUS_SANITIZER=thread ..
#   cmake -DSTDIOBUS_SANITIZER=address,undefined ..
#
# Sanitizer flags are applied globally (compile + link) because:
# - Static libraries don't propagate link flags to consumers
# - All translation units in the process must be compiled with the same sanitizer
# - The sanitizer runtime must be linked into the final executable

if(STDIOBUS_SANITIZER)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-fsanitize=${STDIOBUS_SANITIZER} -fno-omit-frame-pointer)
        add_link_options(-fsanitize=${STDIOBUS_SANITIZER})

        if(STDIOBUS_SANITIZER MATCHES "address")
            add_compile_options(-fno-optimize-sibling-calls)
        endif()

        message(STATUS "Sanitizer enabled: ${STDIOBUS_SANITIZER}")
    else()
        message(WARNING "Sanitizers are not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endif()
