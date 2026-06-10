add_executable(BigDigitsTest
  BigDigitsTest.cpp
)

target_link_libraries(BigDigitsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BigDigitsTest)
