// Copyright (c) 2018-2019 LG Electronics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

#include "parser.h"

namespace gmp { namespace parser {

Parser::Parser(const char * message) {
  pbnjson::JDomParser parser;
  if (!parser.parse(message)) {
    throw parser_error("JSON parsing failure");
  }
  _dom = parser.getDom();
}

namespace {

// Finds a "playTime" object anywhere below v, so that it does not matter
// how the caller wraps the options.
bool FindPlayTimeStart(const pbnjson::JValue &v, gint64 &start, int depth) {
  if (!v.isObject() || depth > 8)
    return false;
  for (const auto &child : v.children()) {
    const pbnjson::JValue &value = child.second;
    if (child.first.isString() && child.first.asString() == "playTime" &&
        value.isObject() && value["start"].isNumber() &&
        value["start"].asNumber<int64_t>(start) == CONV_OK)
      return true;
    if (FindPlayTimeStart(value, start, depth + 1))
      return true;
  }
  return false;
}

}  // namespace

// This used to serialise every member, split it into words and take the
// word after any "start" as a number: a URI with "/start" at its end read
// past the last match, a long number threw out_of_range out of the load,
// and "start" in any path or name could become a seek position.
gint64 Parser::get_start_time(void) const {
  gint64 start = 0;
  if (!FindPlayTimeStart(_dom, start, 0) || start < 0)
    return 0;
  return start;
}

}  // namespace parser
}  // namespace gmp
