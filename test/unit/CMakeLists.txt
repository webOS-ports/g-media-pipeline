# SPDX-License-Identifier: Apache-2.0

# Unit tests that need neither a bus nor a display.

find_package(GTest REQUIRED)
pkg_check_modules(UNIT_DEPS REQUIRED gstreamer-pbutils-1.0 pbnjson_cpp PmLogLib)

add_executable(gmp-unit-tests
    DSIGeneratorAACTest.cpp
    ParserTest.cpp
    UriPlayerFactoryTest.cpp
    ../../src/dsi/DSIGeneratorAAC.cpp
    ../../src/parser/parser.cpp
    ../../src/playerfactory/UriPlayerFactory.cpp
    ../../src/log/log.cpp
)
target_include_directories(gmp-unit-tests PRIVATE
    ../../src ../../src/player ../../src/base ../../src/log
    ../../src/lsm-connector/include ${UNIT_DEPS_INCLUDE_DIRS})
target_link_libraries(gmp-unit-tests GTest::gtest_main ${UNIT_DEPS_LIBRARIES})

enable_testing()
add_test(NAME gmp-unit-tests COMMAND gmp-unit-tests)
