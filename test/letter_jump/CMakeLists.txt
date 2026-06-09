add_executable(LetterJumpTest
  LetterJumpTest.cpp
)

target_include_directories(LetterJumpTest PRIVATE
  ${REPO_ROOT}/src/activities/home
)

target_link_libraries(LetterJumpTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(LetterJumpTest)
