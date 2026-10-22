#include "SOPD3Cases.h"

#include <utility>

namespace sopd3 {
namespace {
std::string mainProgram(const std::string &declarations,
                        const std::string &statements,
                        const std::string &result = "0",
                        const std::string &prefix = "") {
  return prefix + "\nmain() {\n" +
         (declarations.empty() ? "" : "var " + declarations + ";\n") +
         statements + "\nreturn " + result + ";\n}\n";
}

std::vector<Case> buildCases() {
  std::vector<Case> result;
  auto accept = [&](const char *group, const char *name,
                    const char *requirements, std::string source,
                    std::vector<Local> locals,
                    std::vector<Destruction> destructions) -> Case & {
    result.push_back({group,
                      name,
                      requirements,
                      std::move(source),
                      {},
                      std::move(locals),
                      std::move(destructions)});
    return result.back();
  };
  auto reject = [&](const char *group, const char *name,
                    const char *requirements, std::string source,
                    std::vector<std::string> diagnostic) {
    result.push_back(
        {group, name, requirements, std::move(source), std::move(diagnostic)});
  };

  // G1: homogeneous literals, expressions, and inference failures.
  for (const auto &example : std::vector<std::pair<std::string, std::string>>{
           {"[1, 2]", "Seq(int)"},
           {"[alloc 1, alloc 2]", "Seq(own&int)"},
           {"[[1], [2, 3]]", "Seq(Seq(int))"},
           {"[[alloc 1], [alloc 2, alloc 3]]", "Seq(Seq(own&int))"}}) {
    accept("G1", example.second.c_str(), "SL1 SL2 SL15",
           mainProgram("s", "s = " + example.first + ";"),
           {{"main", "s", example.second, true}}, {{"main", {"s"}}});
  }
  accept("G1", "expression elements", "SL1 SL5 SL15",
         mainProgram("seed, s", "seed = 7; s = [seed, seed + 1, inc(seed)];",
                     "seed + #s", "inc(x) { return x + 1; }"),
         {{"main", "seed", "int", false}, {"main", "s", "Seq(int)", true}},
         {{"main", {"s"}}, {"inc", {}}})
      .neverMoved = {"seed", "s"};
  accept("G1", "empty literal solved by sibling context", "SL1 SL2 SL15",
         mainProgram("s", "s = [[], [1]];", "#s"),
         {{"main", "s", "Seq(Seq(int))", true}}, {{"main", {"s"}}});
  reject("G1", "mixed integer and owner", "SL1",
         mainProgram("s", "s = [1, alloc 2];"), {"Cannot unify"});
  reject("G1", "mixed integer and sequence", "SL1",
         mainProgram("s", "s = [1, [2]];"), {"Cannot unify"});
  reject("G1", "handout integer and function", "SL1",
         mainProgram("s", "s = [1, inc];", "0", "inc(x) { return x + 1; }"),
         {"Cannot unify"});
  reject("G1", "undetermined empty literal", "SL2", mainProgram("s", "s = [];"),
         {"element type", "undetermined"});

  // G2: moves, overwrites, joins, and reinitialization.
  auto &construct = accept(
      "G2", "owned and Copy sources", "SL4 SL5 SL11 SL17",
      mainProgram("p, n, s, ints", "p = alloc 7; n = 3; s = [p]; ints = [n];",
                  "n + #s + #ints"),
      {{"main", "p", "own&int", true},
       {"main", "n", "int", false},
       {"main", "s", "Seq(own&int)", true},
       {"main", "ints", "Seq(int)", true}},
      {{"main", {"s", "ints"}}});
  construct.moves = {{"own", "p"}, {"move", "p"}, {"own", "s"}};
  construct.neverMoved = {"n", "s", "ints"};
  accept("G2", "reinitialize moved binding", "SL11 SL12 SL17",
         mainProgram("s, t", "s = [1]; t = s; s = [2];", "#s + #t"),
         {{"main", "s", "Seq(int)", true}, {"main", "t", "Seq(int)", true}},
         {{"main", {"s", "t"}}})
      .moves = {{"own", "s"}, {"move", "s"}, {"own", "t"}, {"own", "s"}};
  reject("G2", "read moved sequence", "SL11",
         mainProgram("s, t", "s = [1]; t = s;", "#s"), {"used after move"});
  reject("G2", "read moved literal source", "SL4 SL11",
         mainProgram("p, s", "p = alloc 7; s = [p];", "*p"),
         {"used after move"});
  reject("G2", "borrow moved sequence", "SL11",
         mainProgram("s, t", "s = [1]; t = s;", "read(&s)",
                     "read(p) { return #*p; }"),
         {"used after move"});
  reject("G2", "transfer moved sequence", "SL11",
         mainProgram("s, t, u", "s = [1]; t = s; u = s;"), {"moved"});
  reject("G2", "overwrite live sequence", "SL12",
         mainProgram("s", "s = [1]; s = [2];"), {"assigned while still owned"});
  accept("G2", "compatible branch moves", "SL13",
         mainProgram("s, t", "s = [1]; if (input) { t = s; } else { t = s; }",
                     "#t"),
         {{"main", "s", "Seq(int)", true}, {"main", "t", "Seq(int)", true}},
         {{"main", {"t"}}});
  reject("G2", "incompatible branch states", "SL13",
         mainProgram("s, n", "s = [1]; if (input) { n = size(s); }", "0",
                     "size(q) { return #q; }"),
         {"ownership state disagreement"});
  for (const auto &bounds : {"0 .. 3", "0 .. 0"})
    reject("G2", bounds, "RG3 SL11",
           mainProgram("s, i, n",
                       std::string("s = [1]; for (i : ") + bounds +
                           ") { n = size(s); }",
                       "0", "size(q) { return #q; }"),
           {"move", "loop"});

  reject(
      "G2", "different sequence recipients across branches", "SL13",
      mainProgram("s, t, u", "s = [1]; if (input) { t = s; } else { u = s; }"),
      {"ownership state disagreement"});

  // G3: recursive cleanup and transferred/temporary owners.
  accept("G3", "retained recursive owners", "SL3 SL6 SL7 SL14 SL16 SL18 SL19",
         mainProgram("ints, cells, nested",
                     "ints = [1, 2]; cells = [alloc 3]; "
                     "nested = [[alloc 4], [alloc 5, alloc 6]];",
                     "#ints + #cells + #nested"),
         {{"main", "ints", "Seq(int)", true},
          {"main", "cells", "Seq(own&int)", true},
          {"main", "nested", "Seq(Seq(own&int))", true}},
         {{"main", {"ints", "cells", "nested"}}});
  accept("G3", "named and unnamed cleanup", "SL7 SL8 SL14 SL18 SL19 LN5 SC5",
         mainProgram("s", "s = [1, 2, 3];", "#mk() + #(s[1:3])",
                     "mk() { return [4, 5, 6]; }"),
         {{"main", "s", "Seq(int)", true}}, {{"main", {"s"}}, {"mk", {}}});
  accept("G3", "temporary transferred to binder", "SL7 SL8 SL18 SL19",
         mainProgram("t", "t = mk();", "#t", "mk() { return [alloc 1]; }"),
         {{"main", "t", "Seq(own&int)", true}}, {{"main", {"t"}}, {"mk", {}}});
  accept("G3", "temporary transferred into sequence", "SL6 SL8 SL18 SL19",
         mainProgram("s", "s = [mk()];", "#s", "mk() { return [alloc 1]; }"),
         {{"main", "s", "Seq(Seq(own&int))", true}},
         {{"main", {"s"}}, {"mk", {}}});
  accept("G3", "temporary transferred into callee", "SL7 SL8 SL18 SL19",
         mainProgram("n", "n = size(mk());", "n",
                     "mk() { return [1, 2]; } size(q) { return #q; }"),
         {{"main", "n", "int", false}},
         {{"main", {}}, {"mk", {}}, {"size", {"q"}}});

  // G4: index operand generality and the Copy/Own boundary.
  accept("G4", "Copy element indexing", "IX1 IX2 IX7",
         mainProgram("s, x", "s = [1, 2]; x = s[0];", "x + #s"),
         {{"main", "s", "Seq(int)", true}, {"main", "x", "int", false}},
         {{"main", {"s"}}})
      .neverMoved = {"s", "x"};
  accept("G4", "expression base and index", "IX1 IX2 IX7 SL8",
         mainProgram("x", "x = mk()[inc(0) - 1];", "x",
                     "mk() { return [4, 5]; } inc(n) { return n + 1; }"),
         {{"main", "x", "int", false}},
         {{"main", {}}, {"mk", {}}, {"inc", {}}});
  reject("G4", "non-sequence index base", "IX1",
         mainProgram("n, x", "n = 1; x = n[0];"), {"Cannot unify"});
  reject("G4", "sequence-valued index", "IX1",
         mainProgram("s, x", "s = [1]; x = s[s];"), {"Cannot unify"});
  reject("G4", "extract owned element", "IX3",
         mainProgram("s, x", "s = [alloc 1]; x = s[0];"), {"owned element"});
  reject("G4", "extract owned element through borrow", "IX3",
         mainProgram("s, x", "s = [alloc 1]; x = first(&s);", "0",
                     "first(p) { return (*p)[0]; }"),
         {"owned element"});
  reject("G4", "extract nested sequence element", "IX3",
         mainProgram("s, x", "s = [[1]]; x = s[0];"), {"owned element"});

  // G5: borrow provenance, permitted mutations, and forbidden storage.
  accept("G5", "read borrowed owned element", "IX4 IX5 IX8 IX9",
         mainProgram("s, x", "s = [alloc 1, alloc 2]; x = read(&s[0]);",
                     "x + #s", "read(p) { return **p; }"),
         {
             {"main", "s", "Seq(own&int)", true},
             {"main", "x", "int", false},
         },
         {{"main", {"s"}}, {"read", {}}})
      .neverMoved = {"s", "p"};
  accept("G5", "reborrow through sequence parameter", "IX4 IX5 IX8 IX9",
         mainProgram("s", "s = [alloc 1];", "first(&s) + #s",
                     "read(p) { return **p; } "
                     "first(q) { return read(&(*q)[0]); }"),
         {
             {"main", "s", "Seq(own&int)", true},
         },
         {{"main", {"s"}}, {"first", {}}, {"read", {}}});
  for (bool owned : {false, true})
    accept("G5", owned ? "modify integer behind owner" : "modify Copy element",
           "IX4 IX5 IX6 IX8 IX9",
           mainProgram("s, n",
                       std::string("s = [") + (owned ? "alloc 1" : "1") +
                           "]; n = bump(&s[0]);",
                       "#s",
                       owned ? "bump(p) { **p = **p + 1; return 0; }"
                             : "bump(p) { *p = *p + 1; return 0; }"),
           {{"main", "s", owned ? "Seq(own&int)" : "Seq(int)", true}},
           {{"main", {"s"}}, {"bump", {}}})
        .neverMoved = {"s", "p"};
  reject("G5", "overwrite owned element through borrow", "IX6",
         mainProgram("s, n", "s = [alloc 1]; n = replace(&s[0]);", "n",
                     "replace(p) { *p = alloc 99; return 0; }"),
         {"overwrite", "borrow"});
  reject("G5", "store indexed borrow", "IX6",
         mainProgram("s, p", "s = [alloc 1]; p = &s[0];"),
         {"borrow", "immediate function argument"});
  reject("G5", "store indexed borrow in literal", "SL5 IX6",
         mainProgram("s, u", "s = [alloc 1]; u = [&s[0]];"),
         {"borrow", "immediate function argument"});
  reject("G5", "move before borrow in same call", "IX5",
         mainProgram("s, n", "s = [1]; n = take(s, &s[0]);", "n",
                     "take(q, p) { return #q + *p; }"),
         {"borrow", "s"});
  reject("G5", "borrow before move in same call", "IX5",
         mainProgram("s, n", "s = [1]; n = take(&s[0], s);", "n",
                     "take(p, q) { return *p + #q; }"),
         {"borrow", "s"});
  reject("G5", "forwarded borrow before move", "IX5",
         mainProgram("s, n", "s = [1]; n = take(ident(&s[0]), s);", "n",
                     "ident(p) { return p; } take(p, q) { return *p + #q; }"),
         {"borrow", "s"});
  reject("G5", "move before forwarded borrow", "IX5",
         mainProgram("s, n", "s = [1]; n = take(s, ident(&s[0]));", "n",
                     "ident(p) { return p; } take(q, p) { return #q + *p; }"),
         {"borrow", "s"});

  // G6: length reads preserve ownership and do not constrain element types.
  for (bool owned : {false, true})
    accept("G6", owned ? "owned-element length" : "integer-element length",
           "LN1 LN2 LN3 LN6 LN7",
           mainProgram("s, n",
                       std::string("s = [") + (owned ? "alloc 1" : "1") +
                           "]; n = #s;",
                       "n + #s"),
           {{"main", "s", owned ? "Seq(own&int)" : "Seq(int)", true},
            {"main", "n", "int", false}},
           {{"main", {"s"}}})
        .neverMoved = {"s"};
  accept("G6", "borrowed length", "LN1 LN2 LN3 LN4 LN6 LN7",
         mainProgram("s", "s = [alloc 1];", "length(&s) + #s",
                     "length(p) { return #*p; }"),
         {{"main", "s", "Seq(own&int)", true}},
         {{"main", {"s"}}, {"length", {}}})
      .neverMoved = {"s", "p"};
  reject("G6", "length of integer", "LN1", mainProgram("n", "n = 1;", "#n"),
         {"Cannot unify"});
  reject("G6", "length does not solve empty literal", "LN2 SL2",
         mainProgram("", "", "#[]"), {"element type", "undetermined"});

  // G7: copying slices create an independent owner.
  accept("G7", "independent slice owner", "SC1 SC2 SC3 SC5 SC6 SC7",
         mainProgram("s, u", "s = [1, 2, 3]; u = s[0:2];", "s[0] + u[0]"),
         {{"main", "s", "Seq(int)", true}, {"main", "u", "Seq(int)", true}},
         {{"main", {"s", "u"}}})
      .neverMoved = {"s", "u"};
  accept("G7", "function elements are Copy",
         "SC1 SC2 SC3 SC5 SC6 SC7 IX1 IX2 IX7",
         mainProgram("s, u, g", "s = [inc, inc]; u = s[0:1]; g = u[0];",
                     "g(41) + #s", "inc(x) { return x + 1; }"),
         {{"main", "s", "Seq((int)->int)", true},
          {"main", "u", "Seq((int)->int)", true},
          {"main", "g", "(int)->int", false}},
         {{"main", {"s", "u"}}, {"inc", {}}})
      .neverMoved = {"s", "u", "g"};
  const std::string cut = "cut(p) { return (*p)[0:1]; }";
  accept("G7", "generic slice Copy instantiation", "SC1 SC2 SC3 SC5 SC6 SC7",
         mainProgram("s, u", "s = [1, 2]; u = cut(&s);", "#s + #u", cut),
         {{"main", "u", "Seq(int)", true}}, {{"main", {"s", "u"}}, {"cut", {}}})
      .neverMoved = {"s"};
  for (const auto &expr : {"[alloc 1]", "[[1], [2]]"})
    reject("G7", expr, "SC3 SC4",
           mainProgram("s, u", std::string("s = ") + expr + "; u = s[0:1];"),
           {"slice", "owned"});
  reject("G7", "generic slice Own instantiation", "SC3 SC4",
         mainProgram("s, u", "s = [alloc 1]; u = cut(&s);", "0", cut),
         {"slice", "owned"});
  reject("G7", "non-sequence slice base", "SC1",
         mainProgram("n, u", "n = 1; u = n[0:1];"), {"Cannot unify"});
  for (const auto &bounds : {"s:1", "0:s"})
    reject("G7", bounds, "SC1",
           mainProgram("s, u", std::string("s = [1]; u = s[") + bounds + "];"),
           {"Cannot unify"});

  // G8: range typing and while-shaped source CFG.
  accept("G8", "range types and preserved owner", "RG1 RG2 RG3 RG4 RG5 RG6",
         mainProgram("s, i, acc",
                     "s = [1]; acc = 0; "
                     "for (i : 0 .. 3) { acc = acc + i + #s; }",
                     "acc + #s"),
         {{"main", "i", "int", false},
          {"main", "acc", "int", false},
          {"main", "s", "Seq(int)", true}},
         {{"main", {"s"}}})
      .loopCFG = true;
  for (const auto &bounds : {"s .. 3", "0 .. s"})
    reject("G8", bounds, "RG1",
           mainProgram("s, i", std::string("s = [1]; for (i : ") + bounds +
                                   ") { output i; }"),
           {"Cannot unify"});

  // G9: header borrows, borrowed parameters, and conditional binder types.
  accept("G9", "Copy-element iteration", "BI1 BI2 BI5 BI7 BI8",
         mainProgram("s, e, acc",
                     "s = [1, 2]; acc = 0; "
                     "for (e : &s) { acc = acc + e; }",
                     "acc + #s"),
         {{"main", "e", "int", false}, {"main", "s", "Seq(int)", true}},
         {{"main", {"s"}}})
      .loopCFG = true;
  accept(
      "G9", "already-borrowed iterable", "BI1 BI2 BI5 BI7 BI8",
      mainProgram("s", "s = [1, 2];", "total(&s) + #s",
                  "total(q) { var e, acc; acc = 0; "
                  "for (e : q) { acc = acc + e; } return acc; }"),
      {{"total", "q", "borrow&Seq(int)", false}, {"total", "e", "int", false}},
      {{"main", {"s"}}, {"total", {}}})
      .loopCFG = true;
  accept("G9", "Own-element borrowed binder", "BI1 BI2 BI5 BI7 BI8",
         mainProgram("s, e, acc",
                     "s = [alloc 1, alloc 2]; acc = 0; "
                     "for (e : &s) { acc = acc + read(e); }",
                     "acc + #s", "read(p) { return **p; }"),
         {{"main", "e", "borrow&own&int", false},
          {"main", "s", "Seq(own&int)", true}},
         {{"main", {"s"}}, {"read", {}}})
      .loopCFG = true;
  accept("G9", "nested sequence borrowed binder", "BI1 BI2 BI5 BI7 BI8",
         mainProgram("s, e, acc",
                     "s = [[1], [2, 3]]; acc = 0; "
                     "for (e : &s) { acc = acc + length(e); }",
                     "acc + #s", "length(p) { return #*p; }"),
         {{"main", "e", "borrow&Seq(int)", false},
          {"main", "s", "Seq(Seq(int))", true}},
         {{"main", {"s"}}, {"length", {}}})
      .loopCFG = true;
  reject("G9", "plain sequence iterable", "BI1",
         mainProgram("s, e", "s = [1]; for (e : s) { output e; }"),
         {"Cannot unify"});

  // G10: whole-loop protection and the restricted borrowed binder.
  for (const auto &element : {"1", "alloc 1"})
    reject("G10", element, "BI3",
           mainProgram("s, e, t", std::string("s = [") + element +
                                      "]; for (e : &s) { t = s; }"),
           {"borrow", "s"});
  for (const auto &body : {"keep = e;", "keep = [e];"})
    reject("G10", body, "BI4 SL5",
           mainProgram("s, e, keep", std::string("s = [alloc 1]; ") +
                                         "for (e : &s) { " + body + " }"),
           {"borrow", "stor"});
  reject("G10", "take owned value through binder", "BI4",
         mainProgram("s, e, keep", "s = [alloc 1]; "
                                   "for (e : &s) { keep = *e; }"),
         {"borrow", "own"});
  reject("G10", "return-derived borrow escapes", "BI4",
         mainProgram("s, e, keep",
                     "s = [alloc 1]; "
                     "for (e : &s) { keep = ident(e); }",
                     "0", "ident(p) { return p; }"),
         {"borrow", "escape"});
  reject("G10", "borrowed binder used after loop", "BI3 BI4",
         mainProgram("s, e, n",
                     "s = [alloc 1]; n = 0; "
                     "for (e : &s) { n = read(e); } n = read(e);",
                     "n", "read(p) { return **p; }"),
         {"borrow", "escapes"});
  accept("G10", "modify Copy payload through binder", "BI3 BI4 BI6 BI9 IX6",
         mainProgram("s, e, n",
                     "s = [alloc 1]; n = 0; "
                     "for (e : &s) { n = bump(e); }",
                     "n + #s", "bump(p) { **p = **p + 1; return 0; }"),
         {{"main", "e", "borrow&own&int", false}},
         {{"main", {"s"}}, {"bump", {}}})
      .loopCFG = true;
  reject("G10", "replace owner through binder", "BI4 IX6",
         mainProgram("s, e, n",
                     "s = [alloc 1]; "
                     "for (e : &s) { n = replace(e); }",
                     "0", "replace(p) { *p = alloc 99; return 0; }"),
         {"overwrite", "borrow"});

  // G11: ordinary function typing, scope, and possible lambda targets.
  accept("G11", "direct and higher-order lambda", "LA1 LA2 LA3 LA4 LA7",
         mainProgram("g, direct, higher",
                     "g = (x) => x + 1; "
                     "direct = g(41); higher = apply(g, 41);",
                     "direct + higher", "apply(f, v) { return f(v); }"),
         {{"main", "g", "(int)->int", false},
          {"main", "direct", "int", false},
          {"main", "higher", "int", false}},
         {{"main", {}}, {"apply", {}}})
      .callTargets = {{"g", 1, 0}, {"f", 1, 0}};
  reject("G11", "capture enclosing local", "LA2",
         mainProgram("delta, g", "delta = 1; g = (x) => x + delta;", "g(41)"),
         {"delta"});
  reject("G11", "wrong lambda arity", "LA4",
         mainProgram("g, n", "g = (x) => x + 1; n = g(1, 2);", "n"),
         {"Cannot unify"});
  reject("G11", "wrong lambda argument type", "LA4",
         mainProgram("g, n", "g = (x) => x + 1; n = g([1]);", "n"),
         {"Cannot unify"});
  accept("G11", "multiple possible lambda bodies", "LA1 LA3 LA4 LA5 LA7",
         mainProgram("f, n",
                     "if (input) { f = (x) => x + 1; } "
                     "else { f = (y) => y + 2; } n = f(10);",
                     "n"),
         {{"main", "f", "(int)->int", false}, {"main", "n", "int", false}},
         {{"main", {}}})
      .callTargets = {{"f", 2, 0}};

  accept("G10", "generic borrowed iteration with independent instantiations",
         "BI2 BI4 BI6",
         mainProgram("a, b", "a = [1]; b = [alloc 2];", "both(&a, &b)",
                     "ignore(x) { return 0; } "
                     "visit(q) { var e, n; n = 0; for (e : q) { n = ignore(e); "
                     "} return n; } "
                     "both(a, b) { return visit(a) + visit(b); }"),
         {{"main", "a", "Seq(int)", true}, {"main", "b", "Seq(own&int)", true}},
         {{"main", {"a", "b"}}, {"ignore", {}}, {"visit", {}}, {"both", {}}});
  accept("G10", "generic borrowed nested element read", "BI2 BI4 BI6",
         mainProgram("s", "s = [[1], [2]];", "visit(&s)",
                     "length(p) { return #*p; } "
                     "visit(q) { var e, n; n = 0; for (e : q) { n = n + "
                     "length(e); } return n; }"),
         {{"main", "s", "Seq(Seq(int))", true}},
         {{"main", {"s"}}, {"length", {}}, {"visit", {}}});
  reject(
      "G10", "generic integer iteration rejects owned elements", "BI2",
      mainProgram(
          "s", "s = [alloc 1];", "visit(&s)",
          "visit(q) { var e, n; n = 0; for (e : q) { n = n + e; } return n; }"),
      {"Cannot unify"});
  reject("G10", "generic borrowed binder returned", "BI4 BI6",
         mainProgram("s, x", "s = [alloc 1]; x = last(&s);", "0",
                     "last(q) { var e; for (e : q) {} return e; }"),
         {"borrow"});

  // G12: ordinary and higher-order ownership effects.
  accept("G12", "generic Copy-element disposal", "SL9 LA6",
         mainProgram("s, n", "s = [1, 2]; n = size(s);", "n",
                     "size(q) { return #q; }"),
         {{"main", "s", "Seq(int)", true}}, {{"main", {}}, {"size", {"q"}}})
      .moves = {{"move", "s"}};
  reject("G12", "generic unknown-element disposal", "SL9",
         mainProgram("s, n", "s = [alloc 1]; n = size(s);", "n",
                     "size(q) { return #q; }"),
         {"element type"});
  accept(
      "G12", "concrete owned-element disposal", "SL9 SL6 SL18",
      mainProgram("s, n", "s = [Full(1), Full(2)]; n = sizeKnown(s);", "n",
                  "type Box = Full(value); "
                  "readBox(p) { var n; case *p of { Full(x) -> n = x; } "
                  "return n; } "
                  "sizeKnown(q) { var n; n = readBox(&q[0]); return #q + n; }"),
      {}, {{"main", {}}, {"readBox", {}}, {"sizeKnown", {"q"}}})
      .moves = {{"move", "s"}};
  accept("G12", "borrowed owned-element sequence", "SL9 SL10",
         mainProgram("s", "s = [alloc 1];", "sizeBorrow(&s) + #s",
                     "sizeBorrow(p) { return #*p; }"),
         {{"main", "s", "Seq(own&int)", true}},
         {{"main", {"s"}}, {"sizeBorrow", {}}})
      .neverMoved = {"s", "p"};
  accept("G12", "return owned-element sequence", "SL9 SL10 LA6",
         mainProgram("s, t", "s = [alloc 1]; t = pass(s);", "#t",
                     "pass(q) { return q; }"),
         {{"main", "t", "Seq(own&int)", true}}, {{"main", {"t"}}, {"pass", {}}})
      .moves = {{"move", "s"}};
  accept("G12", "higher-order consuming lambda", "LA3 LA5 LA6 LA8 SL9",
         mainProgram("g, s, n", "g = (q) => #q; s = [1, 2]; n = apply(g, s);",
                     "n", "apply(f, v) { return f(v); }"),
         {{"main", "g", "", false}, {"main", "s", "Seq(int)", true}},
         {{"main", {}}, {"apply", {}}})
      .callTargets = {{"f", 1, 1}};
  accept("G12", "higher-order returning lambda", "LA3 LA5 LA6 LA8 SL9 SL10",
         mainProgram("g, s, t", "g = (q) => q; s = [alloc 1]; t = apply(g, s);",
                     "#t", "apply(f, v) { return f(v); }"),
         {{"main", "t", "Seq(own&int)", true}},
         {{"main", {"t"}}, {"apply", {}}})
      .callTargets = {{"f", 1, 0}};
  reject("G12", "use source after consuming lambda", "LA5 LA6 LA8 SL11",
         mainProgram("g, s, n", "g = (q) => #q; s = [1]; n = apply(g, s);",
                     "#s", "apply(f, v) { return f(v); }"),
         {"used after move"});
  reject("G12", "lambda unknown-element disposal", "SL9 LA5 LA6",
         mainProgram("g, s, n",
                     "g = (q) => #q; s = [alloc 1]; "
                     "n = apply(g, s);",
                     "n", "apply(f, v) { return f(v); }"),
         {"element type"});
  reject(
      "G12", "generic sequence moved on only one branch", "SL9 SL13",
      mainProgram("s, n", "s = [1]; n = dispose(s, input);", "n",
                  "pass(p) { return p; } "
                  "dispose(q, c) { var r; if (c) { r = pass(q); } return 0; }"),
      {"ownership state disagreement"});
  reject("G12", "generic retained sequence alias lacks cleanup shape", "SL9",
         mainProgram("s, n", "s = [1]; n = dispose(s);", "n",
                     "pass(p) { return p; } "
                     "dispose(q) { var r; r = pass(q); return 0; }"),
         {"element type"});
  accept("G12", "generic forwarding to Copy element disposal", "SL9 LA6",
         mainProgram("s", "s = [1];", "relay(s)",
                     "size(q) { return #q; } relay(q) { return size(q); }"),
         {{"main", "s", "Seq(int)", true}},
         {{"main", {}}, {"size", {"q"}}, {"relay", {}}});
  return result;
}
} // namespace

const std::vector<Case> &cases() {
  static const auto result = buildCases();
  return result;
}
} // namespace sopd3
