// Implements the user's T1–T45 designs in REQUIREMENTS2.md.
// T44's assignment-to-borrow expectation is deliberately not implemented.
#include "ASTHelper.h"
#include "ASTSOP.h"
#include "CheckAssignable.h"
#include "FrontEnd.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <set>
#include <sstream>
#include <typeinfo>

namespace {
std::shared_ptr<ASTProgram> parse(const std::string &source) {
  std::stringstream input(source);
  return FrontEnd::parse(input);
}
std::shared_ptr<ASTProgram> fragment(const std::string &text) {
  return parse("main() { " + text + " return 0; }");
}
std::string printed(ASTProgram *ast) {
  std::ostringstream out;
  FrontEnd::prettyprint(ast, out);
  return out.str();
}
std::string shape(ASTNode *node) {
  std::string result = typeid(*node).name();
  if (auto *n = dynamic_cast<ASTNumberExpr *>(node))
    result += std::to_string(n->getValue());
  if (auto *n = dynamic_cast<ASTVariableExpr *>(node))
    result += n->getName();
  if (auto *n = dynamic_cast<ASTDeclNode *>(node))
    result += n->getName();
  if (auto *n = dynamic_cast<ASTBinaryExpr *>(node))
    result += n->getOp();
  if (auto *n = dynamic_cast<ASTSliceExpr *>(node))
    result += std::string(n->getLowerBound() ? "L" : "-") +
              (n->getUpperBound() ? "U" : "-");
  if (auto *n = dynamic_cast<ASTForStmt *>(node))
    result += n->getStep() ? "step" : "no-step";
  result += "{";
  for (auto &child : node->getChildren())
    result += shape(child.get()) + ";";
  return result + "}";
}
struct Recorder : ASTVisitor {
  std::vector<ASTNode *> nodes;
#define RECORD(T)                                                              \
  void endVisit(T *node) override { nodes.push_back(node); }
  RECORD(ASTSequenceExpr)
  RECORD(ASTSliceExpr)
  RECORD(ASTIndexExpr) RECORD(ASTLengthExpr) RECORD(ASTLambdaExpr)
      RECORD(ASTRange) RECORD(ASTForStmt) RECORD(ASTNumberExpr)
          RECORD(ASTVariableExpr) RECORD(ASTDeclNode) RECORD(ASTBinaryExpr)
              RECORD(ASTBlockStmt) RECORD(ASTAssignStmt)
#undef RECORD
};
std::vector<ASTNode *> walk(ASTNode *node) {
  Recorder recorder;
  node->accept(&recorder);
  return recorder.nodes;
}
std::shared_ptr<ASTExpr> number(int n) {
  return std::make_shared<ASTNumberExpr>(n);
}
std::shared_ptr<ASTExpr> variable(const std::string &n) {
  return std::make_shared<ASTVariableExpr>(n);
}
std::vector<std::shared_ptr<ASTNode>> examples() {
  auto body =
      std::make_shared<ASTBlockStmt>(std::vector<std::shared_ptr<ASTStmt>>{});
  auto range = std::make_shared<ASTRange>(number(1), number(5));
  return {std::make_shared<ASTSequenceExpr>(
              std::vector<std::shared_ptr<ASTExpr>>{number(1), number(2)}),
          std::make_shared<ASTSliceExpr>(variable("s"), number(1), number(5)),
          std::make_shared<ASTIndexExpr>(variable("s"), number(2)),
          std::make_shared<ASTLengthExpr>(variable("s")),
          std::make_shared<ASTLambdaExpr>(
              std::vector<std::shared_ptr<ASTDeclNode>>{
                  std::make_shared<ASTDeclNode>("a")},
              variable("a")),
          range,
          std::make_shared<ASTForStmt>(std::make_shared<ASTVariableExpr>("i"),
                                       range, number(2), body)};
}
void expectPrint(const std::string &expr, const std::string &expected) {
  auto ast = fragment("x = " + expr + ";");
  REQUIRE(printed(ast.get()) ==
          "main()\n{\n  x = " + expected + ";\n  return 0;\n}\n");
}
} // namespace

TEST_CASE("SOP T1 T2 T15: deterministic post-order traversal", "[SOP][D2]") {
  auto ast = fragment("x = 3 + 5;");
  auto *binary = ASTHelper::find_node<ASTBinaryExpr>(ast);
  REQUIRE(walk(binary) == std::vector<ASTNode *>{binary->getLeft(),
                                                 binary->getRight(), binary});
  for (auto &node : examples()) {
    auto first = walk(node.get());
    REQUIRE(first == walk(node.get()));
    REQUIRE(std::set<ASTNode *>(first.begin(), first.end()).size() ==
            first.size());
    REQUIRE(first.back() == node.get());
    // The independently enumerated child subtrees must precede their parent.
    std::vector<ASTNode *> expected;
    for (auto &child : node->getChildren()) {
      auto subtree = walk(child.get());
      expected.insert(expected.end(), subtree.begin(), subtree.end());
    }
    expected.push_back(node.get());
    REQUIRE(first == expected);
  }
}

TEST_CASE("SOP T3: visualize new constructs", "[SOP][D2]") {
  auto ast = fragment(
      "for (i : 0 .. 3 by 1) { s = [(a) => a, [2]]; x = #s[0:2]; y = s[0]; }");
  std::ostringstream ascii, dot;
  FrontEnd::astVisualize(ast, ascii, "ascii");
  FrontEnd::astVisualize(ast, dot, "dot");
  for (auto label : {"ForStmt", "Range", "SequenceExpr", "LambdaExpr",
                     "SliceExpr", "IndexExpr", "LengthExpr"})
    REQUIRE_THAT(ascii.str(), Catch::Matchers::ContainsSubstring(label));
  REQUIRE_THAT(ascii.str(), Catch::Matchers::ContainsSubstring(
                                "parameter[0]: Declaration: a"));
  REQUIRE_THAT(ascii.str(), Catch::Matchers::ContainsSubstring(
                                "lower-bound: IntegerLiteral: 0"));
  REQUIRE_THAT(dot.str(), Catch::Matchers::ContainsSubstring(" -> "));
}

TEST_CASE("SOP T4: source declaration location", "[SOP][D2]") {
  auto ast = parse("main() {\n  var item;\n  return 0;\n}\n");
  auto *decl = ast->getFunctions()[0]->getDeclarations()[0]->getVars()[0];
  REQUIRE(decl->getLine() == 2);
  REQUIRE(decl->getColumn() == 6);
}

TEST_CASE("SOP T5 T6: child lifetime and access", "[SOP][D2]") {
  for (auto &parent : examples()) {
    std::vector<std::weak_ptr<ASTNode>> observers;
    for (auto &child : parent->getChildren())
      observers.push_back(child);
    auto retained = std::move(parent);
    REQUIRE(parent == nullptr);
    auto children = retained->getChildren();
    REQUIRE(children.size() == observers.size());
    for (size_t i = 0; i < children.size(); ++i) {
      REQUIRE_FALSE(observers[i].expired());
      REQUIRE(observers[i].lock().get() == children[i].get());
    }
  }
}

TEST_CASE("SOP T7 T8: sequence AST construction", "[SOP][D2]") {
  for (int count : {0, 1, 3}) {
    std::string input = "[";
    std::vector<std::shared_ptr<ASTExpr>> elements;
    for (int i = 0; i < count; ++i) {
      if (i)
        input += ", ";
      input += std::to_string(i + 1);
      elements.push_back(number(i + 1));
    }
    auto ast = fragment("s = " + input + "];");
    auto *actual = ASTHelper::find_node<ASTSequenceExpr>(ast);
    auto expected = std::make_shared<ASTSequenceExpr>(elements);
    REQUIRE(shape(actual) == shape(expected.get()));
    REQUIRE(actual->getElements().size() == size_t(count));
  }
}

TEST_CASE("SOP T9 T16: optional slice bounds", "[SOP][D2]") {
  for (bool lower : {false, true})
    for (bool upper : {false, true}) {
      auto ast = fragment(std::string("x = s[") + (lower ? "1" : "") + ":" +
                          (upper ? "5" : "") + "];");
      auto *slice = ASTHelper::find_node<ASTSliceExpr>(ast);
      auto expected = std::make_shared<ASTSliceExpr>(
          variable("s"), lower ? number(1) : nullptr,
          upper ? number(5) : nullptr);
      REQUIRE(shape(slice) == shape(expected.get()));
      REQUIRE(bool(slice->getLowerBound()) == lower);
      REQUIRE(bool(slice->getUpperBound()) == upper);
      std::vector<ASTNode *> order{slice->getSequence()};
      if (lower)
        order.push_back(slice->getLowerBound());
      if (upper)
        order.push_back(slice->getUpperBound());
      order.push_back(slice);
      REQUIRE(walk(slice) == order);
    }
}

TEST_CASE("SOP T10 T11 T12 T13: node fields", "[SOP][D2]") {
  auto ast = fragment("x=s[2]; y=#s; f=(a,b)=>a+b; for(i:1..5){}");
  auto *index = ASTHelper::find_node<ASTIndexExpr>(ast);
  REQUIRE(
      shape(index) ==
      shape(std::make_shared<ASTIndexExpr>(variable("s"), number(2)).get()));
  REQUIRE(dynamic_cast<ASTNumberExpr *>(index->getIndex())->getValue() == 2);
  auto *length = ASTHelper::find_node<ASTLengthExpr>(ast);
  REQUIRE(dynamic_cast<ASTVariableExpr *>(length->getSequence())->getName() ==
          "s");
  auto *lambda = ASTHelper::find_node<ASTLambdaExpr>(ast);
  REQUIRE(lambda->getParameters().size() == 2);
  REQUIRE(lambda->getParameters()[0]->getName() == "a");
  REQUIRE(lambda->getParameters()[1]->getName() == "b");
  auto *body = dynamic_cast<ASTBinaryExpr *>(lambda->getBody());
  REQUIRE(body != nullptr);
  REQUIRE(body->getOp() == "+");
  REQUIRE(dynamic_cast<ASTVariableExpr *>(body->getLeft())->getName() == "a");
  REQUIRE(dynamic_cast<ASTVariableExpr *>(body->getRight())->getName() == "b");
  auto *range = ASTHelper::find_node<ASTRange>(ast);
  REQUIRE(dynamic_cast<ASTNumberExpr *>(range->getStart())->getValue() == 1);
  REQUIRE(dynamic_cast<ASTNumberExpr *>(range->getEnd())->getValue() == 5);
}

TEST_CASE("SOP T14 T17: loop construction and optional step", "[SOP][D2]") {
  for (bool range : {false, true})
    for (bool step : {false, true}) {
      auto ast = fragment(std::string("for(i:") + (range ? "1..5" : "s") +
                          (step ? " by 2" : "") + "){ x=i; }");
      auto *loop = ASTHelper::find_node<ASTForStmt>(ast);
      REQUIRE(loop->getBinder()->getName() == "i");
      REQUIRE(bool(dynamic_cast<ASTRange *>(loop->getInput())) == range);
      REQUIRE(bool(loop->getStep()) == step);
      REQUIRE(loop->getBody()->getStmts().size() == 1);
      auto order = walk(loop);
      REQUIRE(order.front() == loop->getBinder());
      REQUIRE(order.back() == loop);
      REQUIRE(loop->getChildren().size() == size_t(step ? 4 : 3));
      if (step)
        REQUIRE(dynamic_cast<ASTNumberExpr *>(loop->getStep())->getValue() ==
                2);
    }
}

TEST_CASE("SOP T18: nested expression traversal", "[SOP][D2]") {
  auto ast = fragment("s=[(a)=>a,[2],3+5];");
  auto *seq = ASTHelper::find_node<ASTSequenceExpr>(ast);
  auto e = seq->getElements();
  auto *lambda = dynamic_cast<ASTLambdaExpr *>(e[0]);
  auto *nested = dynamic_cast<ASTSequenceExpr *>(e[1]);
  auto *sum = dynamic_cast<ASTBinaryExpr *>(e[2]);
  REQUIRE(walk(seq) ==
          std::vector<ASTNode *>{lambda->getParameters()[0], lambda->getBody(),
                                 lambda, nested->getElements()[0], nested,
                                 sum->getLeft(), sum->getRight(), sum, seq});
}

TEST_CASE("SOP T19: source round trip", "[SOP][D2]") {
  auto ast =
      fragment("s=[1,2*2,[3]]; f=(a)=>(b)=>a+b; for(i:0..3 by 2){ x=#s[1:]; }");
  auto text = printed(ast.get());
  auto again = parse(text);
  REQUIRE(printed(again.get()) == text);
  REQUIRE(shape(again.get()) == shape(ast.get()));
}

TEST_CASE("SOP T20: existing binary printing", "[SOP][D2]") {
  for (auto op : {"+", "-", "*", "/", ">", "==", "!="})
    expectPrint(std::string("3") + op + "5", std::string("(3 ") + op + " 5)");
}

TEST_CASE("SOP T21 through T32: expression printing", "[SOP][D2]") {
  const std::vector<std::pair<std::string, std::string>> cases = {
      {"[3+5,#(s[:])]", "[(3 + 5), #s[:]]"},
      {"[]", "[]"},
      {"[1]", "[1]"},
      {"[1,2]", "[1, 2]"},
      {"[1,2,3]", "[1, 2, 3]"},
      {"s[1:]", "s[1:]"},
      {"s[:5]", "s[:5]"},
      {"s[1:5]", "s[1:5]"},
      {"s[:]", "s[:]"},
      {"s[x+1:y]", "s[(x + 1):y]"},
      {"s[1:5][2:3]", "s[1:5][2:3]"},
      {"#(s[:])", "#s[:]"},
      {"#s", "#s"},
      {"s[2]", "s[2]"},
      {"(a)=>a", "(a) => a"},
      {"(a,b)=>a+b", "(a, b) => (a + b)"}};
  for (auto &entry : cases) {
    DYNAMIC_SECTION(entry.first) { expectPrint(entry.first, entry.second); }
  }
}

TEST_CASE("SOP T33 through T38: loop printing", "[SOP][D2]") {
  for (bool step : {false, true}) {
    auto ast = fragment(std::string("for(i:1..5") + (step ? " by 2" : "") +
                        "){ x=i; }");
    REQUIRE(printed(ast.get()) ==
            std::string("main()\n{\n  for (i : 1 .. 5") +
                (step ? " by 2" : "") +
                ")\n    {\n      x = i;\n    }\n  return 0;\n}\n");
  }
}

TEST_CASE("SOP T39 T40 T41: symbol errors", "[SOP][D2]") {
  auto duplicate = fragment("var f; f=(a,a)=>a;");
  REQUIRE_THROWS_WITH(SymbolTable::build(duplicate.get()),
                      Catch::Matchers::ContainsSubstring(
                          "duplicated in lambda parameter list"));
  auto escaped = fragment("var f,x; f=(a)=>a; x=a;");
  REQUIRE_THROWS_WITH(SymbolTable::build(escaped.get()),
                      Catch::Matchers::ContainsSubstring("a undeclared"));
  auto binder = fragment("for(i:0..2){}");
  REQUIRE_THROWS_WITH(SymbolTable::build(binder.get()),
                      Catch::Matchers::ContainsSubstring("i undeclared"));
}

TEST_CASE("SOP T42: lambda symbol report", "[SOP][D2]") {
  auto ast = parse("main() {\n  var f, g;\n  f = (a) => a;\n  g = (b) => b;\n  "
                   "return 0;\n}\n");
  auto symbols = SymbolTable::build(ast.get());
  std::ostringstream report;
  symbols->print(report);
  REQUIRE(report.str() == "Functions : {main}\n"
                          "Lambdas : {(a) => a @3:6, (b) => b @4:6}\n"
                          "Locals for function main : {f, g}\n"
                          "Params for lambda (a) => a @3:6 : {a}\n"
                          "Params for lambda (b) => b @4:6 : {b}\n");
}

TEST_CASE("SOP T43: assignment target rejection", "[SOP][D2]") {
  for (auto target : {"#s", "s[:]", "s[0]", "((a)=>a)", "[1,2]"}) {
    auto ast = fragment(std::string(target) + "=3;");
    REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                        Catch::Matchers::ContainsSubstring("Assignment error"));
  }
}

TEST_CASE("SOP T45: AST construction does not type-check", "[SOP][D2]") {
  auto ast = fragment("x=(3+1)[:];");
  auto *slice = ASTHelper::find_node<ASTSliceExpr>(ast);
  REQUIRE(slice != nullptr);
  REQUIRE(dynamic_cast<ASTBinaryExpr *>(slice->getSequence()) != nullptr);
}

// User-authored earlier in the tutoring session: f(&s[1]) is the passing
// borrow-target example. Assign the call result to satisfy TOP statement
// syntax. This does not implement T44's contradictory assignment-to-borrow
// expectation.
TEST_CASE("SOP user example: borrow a sequence element", "[SOP][D2]") {
  auto ast = fragment("r = f(&s[1]);");
  REQUIRE_NOTHROW(CheckAssignable::check(ast.get()));
}
