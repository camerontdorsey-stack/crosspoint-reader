add_executable(BookPagesTest
  BookPagesTest.cpp
)

target_link_libraries(BookPagesTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(BookPagesTest)
