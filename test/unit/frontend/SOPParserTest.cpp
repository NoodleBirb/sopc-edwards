#include "FrontEnd.h"
#include "ParseError.h"
#include "SemanticError.h"
#include "ParserHelper.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <sstream>
#include <string>
#include <utility>

namespace {
std::string program(const std::string &fragment) {
  // Catalog fragments are statements, whereas TOP's start rule is a program.
  // Unresolved names are intentional: these tests exercise syntax only.
  return "main() { " + fragment + " return 0; }";
}

void accepts(const std::string &fragment) {
  INFO(fragment);
  std::stringstream input(program(fragment));
  REQUIRE(ParserHelper::is_parsable(input));
}

void rejects(const std::string &fragment) {
  INFO(fragment);
  std::stringstream input(program(fragment));
  REQUIRE_FALSE(ParserHelper::is_parsable(input));
}
} // namespace

TEST_CASE("SOP catalog: sequence forms accept", "[SOP Parser][catalog]") {
  for (const auto *fragment : {
           "s = [1, 2*2, 3];", "s = [];", "s = [a, f(b), x + 1];",
           "t = [[1, 42], [3]];", "x = s[0];", "x = s[i + 1];",
           "x = #s;", "x = #(s[1:3]);", "x = s[lo:hi];",
           "x = s[i+1:];", "x = s[:#s];", "x = s[:];", "x = s[0][1];"}) {
    DYNAMIC_SECTION(fragment) { accepts(fragment); }
  }
}

TEST_CASE("SOP catalog: sequence boundaries reject", "[SOP Parser][catalog]") {
  for (const auto *fragment : {"s = [1, 2, ];", "x = s[1:2:3];", "x = s#;"}) {
    DYNAMIC_SECTION(fragment) { rejects(fragment); }
  }
}

TEST_CASE("SOP catalog: iteration forms accept", "[SOP Parser][catalog]") {
  for (const auto *fragment : {
           "for (i : min() .. hi) { x = i; }",
           "for (i : lo .. hi by 2*f()) { x = i; }",
           "for (e : s) { x = e; }", "for (e : &s) { x = e; }"}) {
    DYNAMIC_SECTION(fragment) { accepts(fragment); }
  }
}

TEST_CASE("SOP catalog: range boundaries reject", "[SOP Parser][catalog]") {
  for (const auto *fragment : {
           "x = 1 .. 5;", "for (i : 1 .. 5 by 2 by 3) { x = i; }"}) {
    DYNAMIC_SECTION(fragment) { rejects(fragment); }
  }
}

TEST_CASE("SOP catalog: lambda forms accept", "[SOP Parser][catalog]") {
  for (const auto *fragment : {"f = (x) => x + 1;",
                               "g = (x, y) => f(x) + y;",
                               "h = (x) => (y) => x + y;"}) {
    DYNAMIC_SECTION(fragment) { accepts(fragment); }
  }
}

TEST_CASE("SOP catalog: lambda boundaries reject", "[SOP Parser][catalog]") {
  for (const auto *fragment : {"f = () => 5;", "f = (x) => { return x; };"}) {
    DYNAMIC_SECTION(fragment) { rejects(fragment); }
  }
}

TEST_CASE("SOP parser: generalized valid syntax and deferred checks", "[SOP Parser]") {
  for (const auto *fragment : {
           "s = [1];", "s = [[], [(x) => x], f(1)[0]];",
           "x = f()[i][:][j];", "x = #s[1:3];", "x = ##s;",
           "x = 42[0];", "x = #42;", "x = s[missing():f()];",
           "x = s[(a) => a];", "x = ((x) => x + 1)(2);",
           "f = (x) => (y) => (z) => x + y * z;",
           "f = (x, x) => missing;", // Duplicate names are not a syntax error.
           "for (e : 42) {}", "for (e : &missing) {}",
           "for (i : f(1) + 2 .. g()[0] by 0) {}",
           "for (i : 5 .. 1 by -1) {}",
           "for (i : 1 .. 5 by 2) { for (j : 1 .. 3 by 1) {} }",
           "for (i : 1 .. 5) {} for (j : s) {}"}) {
    DYNAMIC_SECTION(fragment) { accepts(fragment); }
  }
}

TEST_CASE("SOP parser: neighboring malformed forms reject", "[SOP Parser]") {
  for (const auto *fragment : {
           "s = [,1];", "s = [1,,2];", "s = [1 2];", "s = [1;",
           "x = s[];", "x = s[::];", "x = s[1,2];", "x = #;",
           "for (i + 1 : s) {}", "for (i : .. hi) {}",
           "for (i : lo ..) {}", "for (i : lo .. hi by) {}",
           "for (i : lo .. hi by 2 3) {}", "for (i : lo .. hi .. end) {}",
           "for (e : s by 2) {}", "for (e : &s by 2) {}", // D3.
           "for (e : s) x = e;", "for (e : s) { return e; }",
           "x = [1 .. 5];", "x = f(1 .. 5);",
           "f = (x,) => x;", "f = (,x) => x;", "f = (1) => 1;",
           "f = (x + y) => x;", "f = (x) =>;", "f = x => x;",
           "for = 1;", "by = 1;"}) {
    DYNAMIC_SECTION(fragment) { rejects(fragment); }
  }
}

TEST_CASE("SOP parser: sequence tree preserves elements and postfix grouping", "[SOP Parser][structure]") {
  std::stringstream input(program("s = [1, 2*2, []]; x = #s[0][1] + 2;"));
  const auto tree = FrontEnd::parseSyntax(input);
  REQUIRE_THAT(tree, Catch::Matchers::ContainsSubstring(
      "(expr [ (expr 1) , (expr (expr 2) * (expr 2)) , (expr [ ]) ])"));
  REQUIRE_THAT(tree, Catch::Matchers::ContainsSubstring(
      "(expr (expr # (expr (expr (expr s) [ (expr 0) ]) [ (expr 1) ])) + (expr 2))"));
}

TEST_CASE("SOP parser: slice tree distinguishes omitted bounds", "[SOP Parser][structure]") {
  for (const auto &example : {
           std::pair{"s[:];", "(expr (expr s) [ : ])"},
           std::pair{"s[lo:];", "(expr (expr s) [ (expr lo) : ])"},
           std::pair{"s[:hi];", "(expr (expr s) [ : (expr hi) ])"},
           std::pair{"s[lo:hi];", "(expr (expr s) [ (expr lo) : (expr hi) ])"}}) {
    DYNAMIC_SECTION(example.first) {
      std::stringstream input(program(std::string("x = ") + example.first));
      REQUIRE_THAT(FrontEnd::parseSyntax(input),
                   Catch::Matchers::ContainsSubstring(example.second));
    }
  }
}

TEST_CASE("SOP parser: lambda tree preserves parameter order and nested body", "[SOP Parser][structure]") {
  std::stringstream input(program("h = (x, y) => (z) => x + y * z;"));
  REQUIRE_THAT(FrontEnd::parseSyntax(input), Catch::Matchers::ContainsSubstring(
      "(lambda ( (nameDeclaration x) , (nameDeclaration y) ) => "
      "(expr (lambda ( (nameDeclaration z) ) => "
      "(expr (expr x) + (expr (expr y) * (expr z))))))"));
}

TEST_CASE("SOP parser: loop tree separates range step and body", "[SOP Parser][structure]") {
  std::stringstream input(program("for (i : lo .. hi by 2*f()) { x = i; }"));
  REQUIRE_THAT(FrontEnd::parseSyntax(input), Catch::Matchers::ContainsSubstring(
      "(forStmt for ( (nameDeclaration i) : "
      "(forInput (expr lo) .. (expr hi) by (expr (expr 2) * (expr (expr f) ( )))) ) "
      "(blockStmt { (statement (assignStmt (expr x) = (expr i) ;)) }))"));
}

TEST_CASE("SOP parser: syntax API rejects whole-input errors", "[SOP Parser][API]") {
  for (const auto *source : {"", "main() { return 0; } x = 1;",
                             "main() { return 0; } @",
                             "main() { x = s[1:2:3]; return 0; }"}) {
    DYNAMIC_SECTION(source) {
      std::stringstream input(source);
      REQUIRE_THROWS_AS(FrontEnd::parseSyntax(input), ParseError);
    }
  }
}

TEST_CASE("SOP parser: syntax API defers semantic checks", "[SOP Parser][API]") {
  std::stringstream input(program("for (e : &missing) { f = (x, x) => #42; }"));
  REQUIRE_NOTHROW(FrontEnd::parseSyntax(input));
}

TEST_CASE("SOP parser: normal AST path diagnoses unsupported constructs", "[SOP Parser][API]") {
  for (const auto *fragment : {"s = [];", "x = s[0];", "x = s[:];",
                               "x = #s;", "f = (x) => x;", "for (e : s) {}"}) {
    DYNAMIC_SECTION(fragment) {
      std::stringstream input(program(fragment));
      REQUIRE_THROWS_AS(FrontEnd::parse(input), SemanticError);
    }
  }
}
