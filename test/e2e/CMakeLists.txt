# This is a STANDALONE project that simulates a real user of the stdio Bus C++ SDK.
# It uses find_package(stdiobus) — exactly as documented in README.
# It does NOT live inside the SDK build tree.

cmake_minimum_required(VERSION 3.14)
project(stdiobus_user_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find the installed SDK — same as a real user would
find_package(stdiobus REQUIRED)

# Build a test app that exercises the SDK
add_executable(user_app user_app.cpp)
target_link_libraries(user_app PRIVATE stdiobus::stdiobus)
