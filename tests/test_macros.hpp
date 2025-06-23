#include <catch2/catch_test_macros.hpp>

#include <catch2/interfaces/catch_interfaces_capture.hpp>
#include <catch2/internal/catch_test_registry.hpp>
#include <catch2/internal/catch_unique_ptr.hpp>

#define TEST_CASE_PRIVATE(namespace_name, test_name, test_name_str,            \
                          test_tags_str)                                       \
    namespace namespace_name {                                                 \
    void test_name##_impl();                                                   \
                                                                               \
    struct test_name##_Invoker : Catch::ITestInvoker {                         \
        void invoke() const override { test_name##_impl(); }                   \
    };                                                                         \
    Catch::AutoReg                                                             \
        autoReg_##test_name(Catch::Detail::make_unique<test_name##_Invoker>(), \
                            Catch::SourceLineInfo(__FILE__, __LINE__), "",     \
                            Catch::NameAndTags{test_name_str, test_tags_str}); \
                                                                               \
    void test_name##_impl()
