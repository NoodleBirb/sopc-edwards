#include "ParserHelper.h"
#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

// Only the user-authored cases under REQUIREMENTS1.md's Test Cases heading.
// Wrap each statement in a complete program; names and types are not checked.

TEST_CASE("SOP requirements: sequences accept", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "s1 = [1, 2, 3];",
           "s2 = [];",
           "s3 = [5*3];",
           "s4 = [[1, 2], [1, 3]];",
           "x1 = #s1;",
           "x2 = s1[0];",
           "x3 = s1[1:];",
           "x4 = s1[1+1:3];",
           "x5 = s3[:];",
           "x6 = s4[0][:1];"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE(ParserHelper::is_parsable(input));
    }
  }
}

TEST_CASE("SOP requirements: sequences reject", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "s1 = [1, ];",
           "s2 = [,];",
           "x1 = s1#;",
           "x2 = s1[0:1:2];"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE_FALSE(ParserHelper::is_parsable(input));
    }
  }
}

TEST_CASE("SOP requirements: iteration accepts", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "for (i : 0 .. 2) { x = i; }",
           "for (i : s) { x = i; }",
           "for (i : s by y) { x = i; }",
           "for (i : &s) { x = i; }",
           "for (i : min() .. max() by 5 - 3) { x = i; }",
           "for (i : 2 + 3 .. 5 + 6 by -1) { x = i; }"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE(ParserHelper::is_parsable(input));
    }
  }
}

TEST_CASE("SOP requirements: iteration rejects", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "for (i) { x = i; }",
           "for (i : 0 .. 2 by 1 by 1) { x = i; }",
           "x = 1 .. 4;"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE_FALSE(ParserHelper::is_parsable(input));
    }
  }
}

TEST_CASE("SOP requirements: lambdas accept", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "f = (x) => x + 1;",
           "g = (x, y) => f(x) + y;",
           "h = (x) => (y) => x + y;"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE(ParserHelper::is_parsable(input));
    }
  }
}

TEST_CASE("SOP requirements: lambdas reject", "[SOP Parser][requirements]") {
  for (const auto *fragment : {
           "f = () => 5;",
           "f = (x) => { return x; }",
           "f = (x, ) => x + 1;"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input;
      input << "main() { " << fragment << " return 0; }";
      REQUIRE_FALSE(ParserHelper::is_parsable(input));
    }
  }
}
