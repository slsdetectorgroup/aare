add_library(common "defs.cpp")

target_link_libraries(
    common
 PUBLIC
    fmt::fmt
   PRIVATE 
   aare_compiler_flags
     )


if(AARE_BUILD_TESTS)
    set(TestSources
            ${CMAKE_CURRENT_SOURCE_DIR}/defs.test.cpp
        )
    target_sources(tests PRIVATE ${TestSources} )

    #Work around to remove, this is not the way to do it =) 
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/include/common)
    target_link_libraries(tests PRIVATE common)

endif()