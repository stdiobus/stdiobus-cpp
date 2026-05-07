# syntax=docker/dockerfile:1
#
# Development container for stdiobus C++ SDK
# Includes all build tools, sanitizers, and static analysis
#
# Usage:
#   docker build -t stdiobus-dev .
#   docker run -it --rm -v $(pwd):/workspace stdiobus-dev
#

FROM ubuntu:24.04 AS dev

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    gcc-12 \
    g++-12 \
    clang-15 \
    clang-tidy-15 \
    clang-format-15 \
    llvm-15 \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Set default compilers
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && update-alternatives --install /usr/bin/clang clang /usr/bin/clang-15 100 \
    && update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-15 100 \
    && update-alternatives --install /usr/bin/clang-tidy clang-tidy /usr/bin/clang-tidy-15 100 \
    && update-alternatives --install /usr/bin/clang-format clang-format /usr/bin/clang-format-15 100

WORKDIR /workspace

CMD ["/bin/bash"]
