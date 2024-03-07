find_package (Python 3.11 COMPONENTS Interpreter Development)
find_package(pybind11 2.11 REQUIRED)
pybind11_add_module(_aare src/bindings.cpp)

target_link_libraries(_aare PRIVATE aare nlohmann_json::nlohmann_json fmt::fmt aare_compiler_flags)

