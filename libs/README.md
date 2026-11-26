# Bundled dependency sources

Unmodified upstream source archives of the dependencies that aare builds from
source. CMake unpacks them at configure time (`aare_declare_dependency` in
`cmake/helpers.cmake`) and checks the SHA256 recorded at each call, so a
default build needs no network access. Each archive keeps its own license; see
`THIRD-PARTY-NOTICES.txt`.

| Archive | License | Downloaded from |
|---|---|---|
| `Minuit2-6-40-02.tar.gz` | LGPL-2.1-or-later | https://github.com/GooFit/Minuit2/archive/refs/tags/v6-40-02.tar.gz |
| `fmt-12.1.0.tar.gz` | MIT | https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz |
| `json-3.11.3.tar.xz` | MIT | https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz |
| `pybind11-3.0.3.tar.gz` | BSD-3-Clause | https://github.com/pybind/pybind11/archive/refs/tags/v3.0.3.tar.gz |
| `Catch2-3.5.3.tar.gz` | BSL-1.0 | https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.3.tar.gz |
| `benchmark-1.8.3.tar.gz` | Apache-2.0 | https://github.com/google/benchmark/archive/refs/tags/v1.8.3.tar.gz |

Catch2 and Google Benchmark are only used by the tests and benchmarks and are
not part of the installed library or the Python packages.

To update one, see "Implementation conventions" in `AGENTS.md`.
