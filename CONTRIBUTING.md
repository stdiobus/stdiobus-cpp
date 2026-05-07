# Contributing to stdiobus C++ SDK

Thank you for your interest in contributing. This document explains how to get started.

## Prerequisites

- C++17 compiler (GCC 11+, Clang 14+, AppleClang 15+)
- CMake 3.14+
- Git

## Getting Started

```bash
git clone https://github.com/stdiobus/stdiobus-cpp.git
cd stdiobus-cpp
./scripts/build.sh
./scripts/test.sh --all
```

## Development Workflow

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Run the full verification: `./scripts/verify.sh`
5. Commit with a clear message (see below)
6. Push and open a Pull Request

## Building

```bash
# Debug build with tests and examples
./scripts/build.sh

# Release build
./scripts/build.sh --release

# Build with sanitizers
cmake -S . -B build -DSTDIOBUS_SANITIZER=address,undefined
cmake --build build
```

## Running Tests

```bash
./scripts/test.sh --all          # All tests
./scripts/test.sh --unit         # Unit tests only
./scripts/test.sh --e2e          # End-to-end tests
./scripts/test.sh --conformance  # Conformance tests
```

## Code Style

- Follow existing code conventions (see `.clang-format`)
- `snake_case` for functions, variables, files
- `PascalCase` for classes, enums
- `UPPER_CASE` for macros (prefixed with `STDIOBUS_`)
- Every source file must have the Apache-2.0 SPDX header
- Use `@file` / `@brief` Doxygen comments in headers

## Formatting

```bash
./scripts/format.sh              # Format all source files
./scripts/format.sh --check      # Check without modifying
```

## Commit Messages

Use conventional commit format:

```
type(scope): short description

Longer explanation if needed.
```

Types: `feat`, `fix`, `docs`, `refactor`, `test`, `build`, `ci`, `chore`

Examples:
- `feat(bus): add graceful shutdown timeout option`
- `fix(async): resolve race condition in timeout check`
- `docs(readme): update installation instructions`

## Pull Request Guidelines

- One logical change per PR
- Include tests for new features and bug fixes
- Ensure CI passes (build + tests + sanitizers)
- Update CHANGELOG.md under `[Unreleased]`
- Keep PR description concise: what changed, why, how tested

## Adding Tests

- One test file per header: `test_bus.cpp` tests `bus.hpp`
- Use GoogleTest `TEST()` macros
- Test both success and error paths
- Test boundary conditions

## Reporting Issues

- Use GitHub Issues
- Include: OS, compiler version, steps to reproduce
- For crashes: include sanitizer output if possible

## License

By contributing, you agree that your contributions will be licensed under Apache-2.0.
