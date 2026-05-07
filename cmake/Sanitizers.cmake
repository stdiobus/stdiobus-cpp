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

function(stdiobus_enable_sanitizers target)
    if(NOT STDIOBUS_SANITIZER)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(SANITIZER_FLAGS "-fsanitize=${STDIOBUS_SANITIZER}")
        
        target_compile_options(${target} PRIVATE ${SANITIZER_FLAGS} -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE ${SANITIZER_FLAGS})

        # ASAN-specific options
        if(STDIOBUS_SANITIZER MATCHES "address")
            target_compile_options(${target} PRIVATE -fno-optimize-sibling-calls)
        endif()
    else()
        message(WARNING "Sanitizers are not supported for compiler: ${CMAKE_CXX_COMPILER_ID}")
    endif()
endfunction()
