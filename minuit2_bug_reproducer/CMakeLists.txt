add_executable(minuit2_root_interface_reproducer
    reproducer.cpp
)

target_link_libraries(minuit2_root_interface_reproducer
    PRIVATE
        aare_core
        ${ROOT_LIBRARIES}
)