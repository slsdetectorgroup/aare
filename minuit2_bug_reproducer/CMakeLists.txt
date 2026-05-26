find_package(ROOT REQUIRED COMPONENTS Core Hist MathCore Graf Gpad)

add_executable(minuit2_root_interface_reproducer
    reproducer.cpp
)

target_link_libraries(minuit2_root_interface_reproducer
    PRIVATE
        aare_core
        ROOT::Core
        ROOT::Hist
        ROOT::MathCore
        ROOT::Graf
        ROOT::Gpad
)