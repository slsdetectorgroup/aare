#define FRIEND_TEST(test_name) friend void test_name##_impl();

#define TEST_CASE_PRIVATE_FWD(test_name)                                       \
    void test_name##_impl(); // foward declaration
