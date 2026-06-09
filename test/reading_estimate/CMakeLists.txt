add_executable(ReadingEstimateTest
  ReadingEstimateTest.cpp
)

target_include_directories(ReadingEstimateTest PRIVATE
  ${REPO_ROOT}/src/activities/reader
)

target_link_libraries(ReadingEstimateTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingEstimateTest)
