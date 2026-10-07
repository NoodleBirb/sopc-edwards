#include "ASTHelper.h"
#include "SemanticAnalysis.h"
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void expectAccepted(std::stringstream &program) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));
}

static void expectError(std::stringstream &program, const std::string &msgPart) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_THROWS_WITH(SemanticAnalysis::analyze(ast.get()),
                      Catch::Matchers::ContainsSubstring(msgPart));
}

// ---------------------------------------------------------------------------
// An operand of == or != must be Copy.
//
// Every constructor expression allocates a box and == lowers to a comparison
// of the two words, so equality on an Own type is address equality: Nil == Nil
// is false, and an owned temporary compared and dropped is never freed. An
// owned value is moved, borrowed, or matched, never compared.
//
// At a solved type the rule is a read-off (compare-owned). At a type variable
// in a generic body it is an instantiation fact: the formal the operand comes
// from carries a Self Copy requirement with the reason Compared, judged at
// each call. A compared value of variable type with no formal to carry the
// requirement is rejected at the definition (compare-undecidable). A value
// whose type is still a variable in a function that was never generalized has
// no owned source and is accepted.
// ---------------------------------------------------------------------------

TEST_CASE("Comparison: two constructor values compared by == are rejected",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    type List = Nil | Cons(h, t);
    main() { var r; r = 0; if (Nil == Nil) r = 1; return r; }
  )";
  expectError(program, "'==' on owned values compares their addresses");
}

TEST_CASE("Comparison: two owning references compared by == are rejected",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    main() { var p1, p2, r; p1 = alloc 7; p2 = alloc 6; r = p1 == p2; return r; }
  )";
  expectError(program, "compares their addresses");
}

TEST_CASE("Comparison: an owned temporary compared by != is rejected",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    type List = Nil | Cons(h, t);
    mk() { return Cons(1, Nil); }
    main() { var r; r = mk() != Nil; return r; }
  )";
  expectError(program, "'!=' on owned values compares their addresses");
}

TEST_CASE("Comparison: a solved owned operand is rejected inside a generic body too",
          "[Comparison]") {
  // p and q unify with own&int program-wide through the alloc actuals, so the
  // read-off sees a solved Own type at the definition.
  std::stringstream program;
  program << R"(
    eq(p, q) { var r; r = p == q; return *p + *q; }
    main() { return eq(alloc 1, alloc 2); }
  )";
  expectError(program, "compares their addresses");
}

TEST_CASE("Comparison: a compared generic formal is judged at each call",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    same(p) { var r; r = p == p; output r; return p; }
    main() { var a, b; a = alloc 1; b = same(a); return *b; }
  )";
  expectError(program,
              "passes an owned value to generic formal 'p' of 'same', which "
              "compares it with '=='");
}

TEST_CASE("Comparison: a compared generic formal is fine at Copy instances",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    same(a, b) { return a == b; }
    main() { var x, y, r; x = 3; y = 4; r = same(x, y) + same(&x, &y); return r; }
  )";
  expectAccepted(program);
}

TEST_CASE("Comparison: a compared formal reached through a local copy is judged at the call",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    same(p) { var r, c; c = p; r = c == c; output r; return p; }
    main() { var a, b; a = alloc 1; b = same(a); return *b; }
  )";
  expectError(program, "compares it with '=='");
}

TEST_CASE("Comparison: a compared formal that is never disposed of reports the drop",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    eq(p, q) { return p == q; }
    main() { var r; r = eq(alloc 7, alloc 7); return r; }
  )";
  expectError(program, "neither returns it nor passes it on");
}

TEST_CASE("Comparison: a compared formal that is also reused after passing on reports the reuse",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    look(a) { return 0; }
    sink(p) { return *p; }
    f(g, p) { var r; r = g(p); r = r + (p == p); return r; }
    main() { var a; a = alloc 1; return f(sink, a); }
  )";
  expectError(program, "which uses it again after passing it on");
}

TEST_CASE("Comparison: a deref-only generic function is judged per call",
          "[Comparison]") {
  // p and q are ref&α: the mode is unsolved, so the decision is the call's.
  std::stringstream accepted;
  accepted << R"(
    eq(p, q) { var r; r = p == q; return *p; }
    main() { var a, b; a = 3; b = 4; return eq(&a, &b); }
  )";
  expectAccepted(accepted);
}

TEST_CASE("Comparison: a compared value of variable type with no formal to carry it is rejected",
          "[Comparison]") {
  std::stringstream program;
  program << R"(
    inc(n) { return n + 1; }
    g(p, f) { var x, r; x = f(p); r = x == x; return x; }
    main() { return g(1, inc); }
  )";
  expectError(program, "cannot tell whether");
}

TEST_CASE("Comparison: Copy operands stay legal", "[Comparison]") {
  std::stringstream program;
  program << R"(
    inc(n) { return n + 1; }
    main() { var a, b, p1, p2, r;
      a = 3; b = 3; p1 = alloc 7; p2 = alloc 7;
      r = (a == b) + (*p1 == *p2) + (inc == inc) + (a != b);
      return r; }
  )";
  expectAccepted(program);
}

TEST_CASE("Comparison: operands whose type is unresolved in a non-generic function are accepted",
          "[Comparison]") {
  // Nothing ever gave these values a type, so nothing owned flowed into them.
  std::stringstream neverAssigned;
  neverAssigned << R"(
    main() { var a, b, r; r = a == b; return r; }
  )";
  expectAccepted(neverAssigned);

  std::stringstream unconstrainedResult;
  unconstrainedResult << R"(
    unset() { var q; return q; }
    main() { var u, v, r; u = unset(); v = unset(); r = u == v; return r; }
  )";
  expectAccepted(unconstrainedResult);
}

TEST_CASE("Comparison: comparing what a generic formal points at requires a Copy referent",
          "[Comparison]") {
  // look's formals are ref&α: comparing *p is judged at each call against
  // what the actual points at, like the move-out rule.
  std::stringstream accepted;
  accepted << R"(
    look(p, q) { return *p == *q; }
    main() { var a, b; a = 3; b = 3; return look(&a, &b); }
  )";
  expectAccepted(accepted);

  std::stringstream rejected;
  rejected << R"(
    look(p, q) { return *p == *q; }
    main() { var p1, p2; p1 = alloc 1; p2 = alloc 2; return look(&p1, &p2); }
  )";
  expectError(rejected, "compares what it points at with '=='");
}
