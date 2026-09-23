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
`AARE_WARNINGS_AS_ERRORS`, `AARE_VERBOSE` (raises the compile-time log
level), and `AARE_TUNE_LOCAL` (`-march=native`, not portable). Reconfigure an
existing build directory instead of creating alternate in-tree build layouts
unless isolation is needed; `build/CMakeCache.txt` records the options and
interpreter it was configured with.

`-Werror=return-type` is always enabled. New code should also compile cleanly
under the project's `-Wall -Wextra -pedantic -Wshadow -Wold-style-cast
-Wdouble-promotion` flags, which `AARE_WARNINGS_AS_ERRORS` turns into errors.

The build copies `python/aare/*.py` and the compiled `_aare` extension into
`build/aare/`, so `PYTHONPATH=build` makes the package importable. The
extension only imports into the interpreter CMake was configured with, which
is normally the Conda or venv Python active at configure time. Activate that
environment, or resolve the interpreter from the cache:

```bash
PY=$(grep -E '^_?Python_EXECUTABLE' build/CMakeCache.txt | head -1 | cut -d= -f2)
PYTHONPATH="$PWD/build" "$PY" -m pytest python/tests
```

Do not fall back to a different system Python.

With `AARE_BENCHMARKS=ON`, Google Benchmark programs build as
`build/run_benchmarks` and `build/fit_benchmark`.

## Architecture

### Core containers

- `NDArray<T, Ndim>` owns contiguous C-order memory; `NDView<T, Ndim>` is a
  non-owning view with the same shape and stride API. Both derive from
  `ArrayExpr` (expression templates for element-wise math). Shapes are
  `std::array<ssize_t, Ndim>`. `NDView<T>` converts to `NDView<const T>`.
- `Frame` is a byte buffer plus rows, cols, and `Dtype`, the lowest common
  denominator returned by file readers. `Dtype` is a runtime type tag built
  from a bitdepth, a NumPy descriptor, or `typeid`.
- `defs.hpp` holds detector enums (`DetectorType`, `TimingMode`,
  `FrameDiscardPolicy`, `ReadoutMode`), the on-disk `DetectorHeader`, chip
  constants (`Moench04`, `Matterhorn02`, ...), the `LOCATION` macro used in
  error messages, and `AARE_ASSERT` (active only with `AARE_CUSTOM_ASSERT`).
- `logger.hpp` provides `LOG(logLEVEL) << ...`; levels above `AARE_LOG_LEVEL`
  (set by CMake) are compiled out.

### File reading

`File` is the generic RAII facade. It holds a `unique_ptr<FileInterface>` and
dispatches on file extension to one of the concrete readers:

- `RawFile` reads slsDetector `.raw` data with a `_master_N.json` (or legacy
  `.raw`) master file. It owns a `RawMasterFile` (master JSON, older raw-text
  masters, scan parameters, ROIs, disabled UDP ports) and a two-dimensional
  table of `RawSubFile` indexed `[roi][module]`. Each `RawSubFile` handles one
  module's sequence of `_dN_fM_` data files and per-frame `DetectorHeader`s.
  Module placement into the assembled image goes through `DetectorGeometry`,
  `ModuleGeometry`, and `ROIGeometry` (module gaps, ROIs, UDP-port halves).
  Frame numbers from the detector are distinct from frame indices in the
  file; the API uses both and names them accordingly.
- `NumpyFile` (`.npy`), `JungfrauDataFile` (legacy `.dat`), and `CtbRawFile`
  (chip test board; raw ADC samples decoded through `decode.hpp` and the
  `PixelMap` generators, exposed in Python as `aare.transform` callables).
- `experimental::MultiThreadedFileReader` opens one `File` per worker and
  fills a caller-owned buffer in file order.

Some chip-specific decoding (Moench03/05 pixel maps, ADC SAR transforms)
lives in `python/aare/transform.py` on top of C++ generators.

### Pedestal and cluster finding

- `Pedestal<T>` and `FastPedestal<T>` maintain per-pixel running mean and
  standard deviation, always accumulating in `double`. `FastPedestal` must
  see `min_pedestal_samples` frames before `ready()`.
- `ClusterFinder<ClusterType, FRAME_TYPE, PEDESTAL_TYPE>` is single-threaded:
  push pedestal frames, then `find_clusters(view, frame_number)` appends to an
  internal `ClusterVector` that callers take with `steal_clusters()`.
  Thresholds are `n_sigma * std` and are recomputed by `update_threshold()`.
- `ClusterFinderMT` wraps N `ClusterFinder`s, one per thread, each with a
  `CircularFifo<FrameRef>` input queue backed by a preallocated `FramePool`
  and a `ProducerConsumerQueue` output. A collector thread funnels all
  outputs into a single sink queue. Consumers of that sink run their own
  thread: `ClusterCollector` (accumulate in memory, `steal_clusters()`) or
  `ClusterFileSink` (write the binary cluster file). The `stop()` and
  `sync()` ordering matters; read the existing tests before changing
  lifecycle code.
- `Cluster<T, SizeX, SizeY, CoordType>` is a POD (x, y, data[]) and
  `ClusterVector<Cluster<...>>` is a frame-tagged `std::vector` of them.
  `ClusterFile` reads and writes the binary format (`int32 frame_number`,
  `uint32 n_clusters`, raw cluster bytes). The format carries no shape or
  dtype metadata, so the caller must supply matching template arguments.
  `CalculateEta.hpp` and `Interpolator` operate on `ClusterVector`s;
  `algorithm.hpp` holds small 1-D search helpers.
- `hist/PixelHistogram` and `PedestalTrackingPixelHistogram` build per-pixel
  histograms across a frame series, multithreaded by row bands.

### Fitting

`Fit.hpp`, `FitModel<Model>`, and `Models.hpp` (`Gaussian`, `Pol1`, `Pol2`,
`RisingScurve`, `FallingScurve`, `GaussianErfcPlateau`,
`GaussianChargeSharing`, ...) wrap Minuit2. Template bodies and explicit
instantiations live in `src/Fit.cpp`; `FitModelImpl` is a pimpl so Minuit2
headers never leak into the public API. Minuit2 is a private,
`BUILD_INTERFACE`-only dependency of `aare_core`.

### Python layer

- `python/src/module.cpp` registers everything. Templated classes are
  instantiated per value type and cluster size through the
  `DEFINE_CLUSTER_BINDINGS` and `DEFINE_BINDINGS_CLUSTERFINDER` macros,
  producing names such as `ClusterFinder_Cluster3x3i`, `Pedestal_d`, and
  `FastPedestal_i16`. Supported cluster sizes are 2x2, 3x3, 5x5, 7x7, and
  9x9; value types are `i`, `f`, `d`, and `i16` (3x3 only).
  `module_config.hpp` fixes the module-wide pedestal type (`pd_type`).
- `python/aare/factory.py` maps NumPy dtypes to those suffixes. The facades
  in `python/aare/*.py` are thin factory functions or subclasses that select
  the right binding and add Pythonic conveniences (iteration, context
  managers, `ScanParameters`).
- `np_helper.hpp` provides `return_image_data` (moves an `NDArray` into a
  NumPy array with a capsule deleter) and `make_view_1d/2d/3d` (views into a
  NumPy buffer).
- `aare.experimental` and `_aare.experimental` hold unstable APIs.

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

For focused runs, use a Catch2 tag or test name, or an individual pytest
file or test:

```bash
build/run_tests "[tag]"
build/run_tests "Exact test case name"
PYTHONPATH="$PWD/build" python -m pytest python/tests/test_example.py
PYTHONPATH="$PWD/build" python -m pytest python/tests/test_example.py -k name
```

Large detector test files live outside this repository in
`https://gitea.psi.ch/detectors/aare-test-data.git` (Git LFS). To include
data-backed tests, set `AARE_TEST_DATA` and opt in explicitly:

```bash
export AARE_TEST_DATA=/path/to/aare-test-data
build/run_tests "[.with-data]"
PYTHONPATH="$PWD/build" python -m pytest python/tests --with-data
```

- Start bug fixes with a failing regression test when practical.
- Put C++ tests beside the relevant implementation as `src/Thing.test.cpp`
  and add them to `TestSources` in the root `CMakeLists.txt`.
- Mark C++ tests requiring external files with `[.with-data]`.
- Mark Python tests requiring external files with `@pytest.mark.withdata` and
  use the `test_data_path` fixture from `python/tests/conftest.py`.
- Prefer synthesizing small input files over depending on external data.
  `tests/raw_file_helpers.hpp` builds throwaway raw files in C++, and many
  Python tests write small raw files into `tmp_path`.
- `tests/test_config.hpp.in` provides `test_data_path()`;
  `tests/test_macros.hpp` and `tests/friend_test.hpp` (`TEST_CASE_PRIVATE`
  with `FRIEND_TEST`) let a test reach private members.
- Run both suites for changes that cross the C++/Python boundary.
- Do not silently skip required data-backed coverage. Report when the external
  test data is unavailable.

## Formatting and static analysis

The project uses pre-commit. Install the git hook once per clone with
`pre-commit install` so it runs on every commit. The hooks in
`.pre-commit-config.yaml` are clang-format on C++, cmake-format on CMake
files, `typos` on `docs/` (project words are whitelisted in `typos.toml`), and
basic YAML, TOML, JSON, and large-file checks. CI runs the same hooks on pull
requests.

Before handing off a broad change, run the relevant checks:

```bash
pre-commit install                            # once per clone
pre-commit run --all-files                    # what CI runs
cmake --build build --target check-format     # diff-based clang-format check
cmake --build build --target format-files     # apply clang-format to all .cpp/.hpp
cmake --build build --target clang-tidy       # uses build/compile_commands.json
```

C++ formatting follows `.clang-format` (four-space indentation and an 80-column
limit). CMake files are checked by `cmake-format`. Avoid formatting unrelated
code as part of a focused change.

## Documentation and release notes

- Update the relevant `.rst` pages for public behavior or API changes. C++
  pages live at `docs/src/<Class>.rst`; Python pages live under
  `docs/src/python/<area>/`.
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
