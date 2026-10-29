// SPDX-License-Identifier: Apache-2.0

// Parser::get_start_time(): the resume position of a reload.

#include <gtest/gtest.h>

#include "parser/parser.h"

namespace {

gint64 StartTime(const char *message) {
  return gmp::parser::Parser(message).get_start_time();
}

// What umediaserver's Pipeline::updatePayloadForResume() makes of a load.
TEST(ParserTest, ResumePosition) {
  EXPECT_EQ(StartTime(R"({"id":"m1","uri":"https://h/a.mp4","options":{"option":
      {"appId":"a","transmission":{"playTime":{"realTime":false,"start":1234}}}}})"),
            1234);
}

TEST(ParserTest, NoResumePosition) {
  EXPECT_EQ(StartTime(R"({"id":"m1","uri":"https://h/a.mp4","options":{"option":{}}})"), 0);
}

// The old word scan read past its last match here.
TEST(ParserTest, UriEndingInStart) {
  EXPECT_EQ(StartTime(R"({"id":"m1","uri":"https://h/start"})"), 0);
}

// ... threw std::out_of_range here,
TEST(ParserTest, HugeNumberAfterStart) {
  EXPECT_EQ(StartTime(R"({"id":"m1","uri":"https://h/start/99999999999999999999"})"), 0);
}

// ... and took this as a seek to 77 ms.
TEST(ParserTest, StartInPath) {
  EXPECT_EQ(StartTime(R"({"id":"m1","uri":"https://h/start/77/v.mp4"})"), 0);
}

TEST(ParserTest, NegativeOrNonNumericStart) {
  EXPECT_EQ(StartTime(R"({"option":{"transmission":{"playTime":{"start":-5}}}})"), 0);
  EXPECT_EQ(StartTime(R"({"option":{"transmission":{"playTime":{"start":"10"}}}})"), 0);
}

TEST(ParserTest, BadJsonThrowsParserError) {
  EXPECT_THROW(gmp::parser::Parser("{"), gmp::parser::parser_error);
}

}  // namespace
