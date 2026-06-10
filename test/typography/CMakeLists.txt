add_executable(TypographyTest
  TypographyTest.cpp
)

target_include_directories(TypographyTest PRIVATE
  ${REPO_ROOT}/lib/Epub
)

target_link_libraries(TypographyTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(TypographyTest)
