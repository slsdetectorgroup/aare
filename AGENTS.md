# Repository guidelines

## Scope and project goals

These instructions apply to the entire repository unless a more specific
`AGENTS.md` exists in a subdirectory.

Aare is a data-analysis library for PSI hybrid detectors. The C++17 core is
the canonical implementation, while Python is the main user-facing interface
through pybind11. Changes should preserve the project's priorities: fast and
efficient processing, simple interfaces, API stability, and a small dependency
footprint.

## Repository layout

- `include/aare/` contains the public C++ API and header-defined templates.
- `src/` contains C++ implementations and colocated Catch2 tests named
  `*.test.cpp`.
- `tests/` contains the C++ test executable, configuration, and shared helpers.
- `python/src/` contains pybind11 bindings and module registration.
- `python/aare/` contains Python facades, convenience APIs, and public exports.
- `python/tests/` contains the pytest suite.
- `docs/src/` contains Sphinx/reStructuredText documentation. Doxygen and
  Breathe integrate the C++ API into the Sphinx site.
- `benchmarks/` contains Google Benchmark programs.
- `cmake/`, `conda-recipe/`, and `pyproject.toml` support builds and packaging.
- `RELEASE.md` contains pending and published release notes.

## Build environment

The project requires CMake 3.15 or newer, C++17 with compiler extensions
disabled, and Python 3.11 or newer. `etc/dev-env.yml` defines the Conda
development environment.

CMake fetches several dependencies by default. Use
`-DAARE_SYSTEM_LIBRARIES=ON` only when all required system or Conda packages
are available.

Use this configuration for normal development:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAARE_TESTS=ON \
  -DAARE_PYTHON_BINDINGS=ON
cmake --build build -j4
```

Useful optional settings include `AARE_DOCS`, `AARE_BENCHMARKS`, `AARE_ASAN`,
and `AARE_WARNINGS_AS_ERRORS`. Reconfigure an existing build directory instead
of creating alternate in-tree build layouts unless isolation is needed.

## Implementation conventions

- Put public declarations in `include/aare/` and private implementation details
  in `src/`.
- Add new compiled headers, sources, and C++ tests to the explicit lists in the
  root `CMakeLists.txt`.
- Keep template implementations in headers unless the supported types are
  explicitly instantiated.
- Follow the naming in adjacent code. Broadly, use the `aare` namespace,
  CamelCase types, and snake_case functions.
- Treat ownership, lifetime, const-correctness, array shapes, and buffer
  contiguity as part of the API when working with `NDArray`, `NDView`, or NumPy
  bindings.
- Avoid unnecessary allocations and copies in detector-data and per-pixel
  processing paths. Add or update a benchmark when performance is central to a
  change.
- Place APIs that may change without notice under the existing experimental
  namespace/module.
- Start new source files with `SPDX-License-Identifier: MPL-2.0`, using the
  appropriate comment syntax.
- Prefer descriptive names to comments that only restate the code.
- Do not add a dependency unless the benefit justifies the packaging and
  deployment cost.

## Python-facing changes

A Python-facing feature can require coordinated changes in several layers:

1. Update the C++ public API and implementation.
2. Add or update its binding in `python/src/`.
3. Register new bindings in `python/src/module.cpp`.
4. Update the facade or public exports in `python/aare/`.
5. If adding a Python module, add it to `PYTHON_FILES` in
   `python/CMakeLists.txt` so it is copied and installed.
6. Add Python tests and update user documentation.

Bindings must validate NumPy dimensions and data types before constructing
views. Do not return a view whose backing C++ or Python storage can expire while
the view remains reachable.

## Tests

Run tests that do not require external detector data with:

```bash
ctest --test-dir build --output-on-failure -j4
PYTHONPATH="$PWD/build" python -m pytest python/tests
```

For focused runs, use a Catch2 tag or an individual pytest file/test:

```bash
build/run_tests "[tag]"
PYTHONPATH="$PWD/build" python -m pytest python/tests/test_example.py
```

Large detector test files live outside this repository. To include data-backed
tests, set `AARE_TEST_DATA` and opt in explicitly:

```bash
export AARE_TEST_DATA=/path/to/aare-test-data
build/run_tests "[.with-data]"
PYTHONPATH="$PWD/build" python -m pytest python/tests --with-data
```

- Start bug fixes with a failing regression test when practical.
- Put C++ tests beside the relevant implementation as `src/Thing.test.cpp`.
- Mark C++ tests requiring external files with `[.with-data]`.
- Mark Python tests requiring external files with `@pytest.mark.withdata`.
- Run both suites for changes that cross the C++/Python boundary.
- Do not silently skip required data-backed coverage. Report when the external
  test data is unavailable.

## Formatting and static analysis

Before handing off a broad change, run the relevant checks:

```bash
pre-commit run --all-files
cmake --build build --target check-format
cmake --build build --target clang-tidy
```

C++ formatting follows `.clang-format` (four-space indentation and an 80-column
limit). CMake files are checked by `cmake-format`. Avoid formatting unrelated
code as part of a focused change.

## Documentation and release notes

- Update the relevant `.rst` pages for public behavior or API changes.
- Add new pages to `docs/src/index.rst` or the relevant nested toctree.
- Update `RELEASE.md` under `## Next` for user-visible features, bug fixes, and
  API changes.
- Do not change `VERSION` unless performing an explicitly requested release.
- Preserve compatibility with existing detector formats and older recorded
  files where practical. Call out intentional API or format incompatibilities.

Build the documentation with:

```bash
cmake -S . -B build \
  -DAARE_DOCS=ON \
  -DAARE_PYTHON_BINDINGS=ON
cmake --build build --target docs
```

## Working practices and handoff

- Inspect the adjacent implementation, tests, and documentation before editing.
- Keep changes focused and preserve unrelated modifications in the worktree.
- Do not edit generated files or fetched dependency sources under `build/`.
- For large features, prefer independently testable increments.
- At handoff, summarize the behavior and important files changed, checks run,
  checks not run and why, and any compatibility or performance considerations.
