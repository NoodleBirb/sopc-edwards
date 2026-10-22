#include "SOPD3Cases.h"

#include "ASTBorrowExpr.h"
#include "ASTDestroyStmt.h"
#include "ASTForStmt.h"
#include "ASTFunAppExpr.h"
#include "ASTLambdaExpr.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "BorrowChecker.h"
#include "FrontEnd.h"
#include "SemanticAnalysis.h"
#include "SemanticError.h"
#include "TopFunction.h"
#include "cfg/CFGRenderer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>
#include <sstream>

namespace {
std::shared_ptr<ASTProgram> parse(const std::string &source) {
  std::istringstream input(source);
  return FrontEnd::parse(input);
}

template <class T> std::vector<T *> nodes(ASTNode *root) {
  std::vector<T *> result;
  std::function<void(ASTNode *)> visit = [&](ASTNode *node) {
    if (auto *match = dynamic_cast<T *>(node))
      result.push_back(match);
    for (const auto &child : node->getChildren())
      visit(child.get());
  };
  visit(root);
  return result;
}

std::string compact(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(),
                            [](unsigned char c) { return std::isspace(c); }),
             text.end());
  return text;
}

std::string typeText(const std::shared_ptr<TopType> &type) {
  REQUIRE(type != nullptr);
  std::ostringstream text;
  text << *type;
  return compact(text.str());
}

std::multiset<std::string> destroyTargets(ASTFunction *function) {
  std::multiset<std::string> targets;
  for (auto *destroy : nodes<ASTDestroyStmt>(function)) {
    REQUIRE(destroy->getVar() != nullptr);
    targets.insert(destroy->getVar()->getName());
  }
  return targets;
}

bool reachable(const ControlFlowGraph &cfg, BlockId from, BlockId to) {
  std::vector<BlockId> pending{from};
  std::set<BlockId> seen;
  while (!pending.empty()) {
    const auto next = pending.back();
    pending.pop_back();
    if (next == to)
      return true;
    if (!seen.insert(next).second)
      continue;
    const auto *block = cfg.findBlock(next);
    REQUIRE(block != nullptr);
    for (const auto &edge : block->getSuccessors())
      pending.push_back(edge.target);
  }
  return false;
}

void checkLoopCFG(ASTProgram *ast, SemanticAnalysis *analysis) {
  std::size_t checked = 0;
  for (auto *function : ast->getFunctions()) {
    const auto &cfg = analysis->getIntraproceduralCFGs()->get(function);
    REQUIRE_NOTHROW(cfg.validate());
    for (auto *loop : nodes<ASTForStmt>(function)) {
      const BasicBlock *header = nullptr;
      for (const auto &block : cfg.getBlocks()) {
        if (block.getTerminatorStatement() == loop)
          header = &block;
        // Source CFGs must remain free of synthetic destruction statements.
        for (auto *stmt : block.getStatements())
          CHECK(dynamic_cast<const ASTDestroyStmt *>(stmt) == nullptr);
      }
      REQUIRE(header != nullptr);
      REQUIRE(header->getSuccessors().size() == 2);
      const CFGEdge *body = nullptr;
      const CFGEdge *exit = nullptr;
      for (const auto &edge : header->getSuccessors()) {
        if (edge.kind == CFGEdgeKind::TrueBranch)
          body = &edge;
        if (edge.kind == CFGEdgeKind::FalseBranch)
          exit = &edge;
      }
      REQUIRE(body != nullptr);
      REQUIRE(exit != nullptr);
      CHECK(body->target != exit->target);
      CHECK(reachable(cfg, body->target, header->getId()));
      CHECK(reachable(cfg, exit->target, cfg.getExit().getId()));
      CHECK_FALSE(reachable(cfg, exit->target, header->getId()));
      std::ostringstream rendered;
      CFGRenderer::renderAscii(cfg, rendered);
      CHECK_THAT(rendered.str(), Catch::Matchers::ContainsSubstring("for"));
      CHECK_THAT(rendered.str(), Catch::Matchers::ContainsSubstring("true"));
      CHECK_THAT(rendered.str(), Catch::Matchers::ContainsSubstring("false"));
      ++checked;
    }
  }
  REQUIRE(checked > 0);
}

void checkBorrows(ASTProgram *ast, TypeInference *types) {
  const auto &events = BorrowChecker::getLastTrace();
  for (auto *borrow : nodes<ASTBorrowExpr>(ast)) {
    auto event = std::find_if(events.begin(), events.end(), [&](const auto &e) {
      return e.line == borrow->getLine() && e.column == borrow->getColumn();
    });
    REQUIRE(event != events.end());
    CHECK(event->approved);
    std::ostringstream expression;
    expression << *borrow;
    CHECK(compact(event->expr) == compact(expression.str()));
    auto borrowType = types->getInferredType(static_cast<ASTExpr *>(borrow));
    REQUIRE(borrowType != nullptr);
    CHECK_THAT(typeText(borrowType), Catch::Matchers::StartsWith("borrow&"));
    CHECK(OwnershipClassifier::classifyType(borrowType.get()) ==
          OwnershipClass::Copy);
  }
}

void checkAccepted(const sopd3::Case &example,
                   const std::shared_ptr<ASTProgram> &ast) {
  std::shared_ptr<SemanticAnalysis> analysis;
  REQUIRE_NOTHROW(analysis = SemanticAnalysis::analyze(ast.get()));
  REQUIRE(analysis != nullptr);
  auto *symbols = analysis->getSymbolTable();
  auto *types = analysis->getTypeResults();
  auto *classes = analysis->getOwnershipClassifier();
  REQUIRE(symbols != nullptr);
  REQUIRE(types != nullptr);
  REQUIRE(classes != nullptr);

  for (const auto &expected : example.locals) {
    INFO(expected.function << "." << expected.name);
    auto *function = ast->findFunctionByName(expected.function);
    REQUIRE(function != nullptr);
    auto *local = symbols->getLocal(expected.name, function->getDecl());
    REQUIRE(local != nullptr);
    if (!expected.type.empty())
      CHECK(typeText(types->getInferredType(local)) == compact(expected.type));
    CHECK(classes->classify(local) ==
          (expected.own ? OwnershipClass::Own : OwnershipClass::Copy));
  }
  for (auto *function : symbols->getFunctions())
    CHECK(classes->classify(function) == OwnershipClass::Copy);

  for (const auto &expected : example.destructions) {
    INFO("destroy targets in " << expected.function);
    auto *function = ast->findFunctionByName(expected.function);
    REQUIRE(function != nullptr);
    const std::multiset<std::string> targets(expected.targets.begin(),
                                             expected.targets.end());
    CHECK(destroyTargets(function) == targets);
    const auto statements = function->getStmts();
    REQUIRE_FALSE(statements.empty());
    CHECK(dynamic_cast<ASTReturnStmt *>(statements.back()) != nullptr);
  }

  // Expected transitions are an ordered subsequence; unrelated events are OK.
  const auto &events = MoveAnalysis::getLastTrace();
  auto cursor = events.begin();
  for (const auto &expected : example.moves) {
    INFO(expected.kind << " " << expected.variable);
    cursor = std::find_if(cursor, events.end(), [&](const auto &event) {
      return event.kind == expected.kind && event.variable == expected.variable;
    });
    REQUIRE(cursor != events.end());
    CHECK(cursor->line > 0);
    ++cursor;
  }
  for (const auto &name : example.neverMoved) {
    INFO("must not move " << name);
    CHECK(std::none_of(events.begin(), events.end(), [&](const auto &event) {
      return event.kind == "move" && event.variable == name;
    }));
  }

  for (const auto &expected : example.callTargets) {
    bool found = false;
    for (auto *call : nodes<ASTFunAppExpr>(ast.get())) {
      auto *callee = dynamic_cast<ASTVariableExpr *>(call->getFunction());
      if (!callee || callee->getName() != expected.calleeExpression)
        continue;
      found = true;
      const auto targets = analysis->getCallGraph()->getCalledFuns(call);
      REQUIRE(targets.size() == expected.count);
      for (auto *target : targets) {
        REQUIRE(target != nullptr);
        if (expected.destroysPerTarget >= 0)
          CHECK(destroyTargets(target).size() ==
                static_cast<std::size_t>(expected.destroysPerTarget));
      }
    }
    REQUIRE(found);
  }

  for (auto *lambda : nodes<ASTLambdaExpr>(ast.get())) {
    const auto type = types->getInferredType(static_cast<ASTExpr *>(lambda));
    const auto *functionType = dynamic_cast<TopFunction *>(type.get());
    REQUIRE(functionType != nullptr);
    CHECK(OwnershipClassifier::classifyType(type.get()) ==
          OwnershipClass::Copy);
    const auto parameters = lambda->getParameters();
    const auto parameterTypes = functionType->getParamTypes();
    REQUIRE(parameters.size() == parameterTypes.size());
    for (std::size_t i = 0; i < parameters.size(); ++i)
      CHECK(typeText(types->getInferredType(parameters[i])) ==
            typeText(parameterTypes[i]));
  }

  if (auto *pass = ast->findFunctionByName("pass")) {
    const auto *summary =
        analysis->getFunctionEffectSummaries()->get(pass->getDecl());
    REQUIRE(summary != nullptr);
    CHECK(summary->returnOrigin ==
          FunctionEffectSummaries::ReturnOrigin::FromFormal);
    CHECK(summary->returnFormalIndex == 0);
  }
  checkBorrows(ast.get(), types);
  if (example.loopCFG)
    checkLoopCFG(ast.get(), analysis.get());
}

void runGroup(const std::string &group) {
  for (const auto &example : sopd3::cases()) {
    if (example.group != group)
      continue;
    DYNAMIC_SECTION(example.name << " [" << example.requirements << "]") {
      INFO(example.source);
      auto ast = parse(example.source);
      REQUIRE(ast != nullptr);
      if (example.diagnostic.empty()) {
        checkAccepted(example, ast);
      } else {
        try {
          (void)SemanticAnalysis::analyze(ast.get());
          FAIL("expected SemanticError");
        } catch (const SemanticError &error) {
          const std::string message = error.what();
          INFO(message);
          CHECK_THAT(message,
                     !Catch::Matchers::ContainsSubstring("not implemented"));
          for (const auto &fragment : example.diagnostic)
            CHECK_THAT(message, Catch::Matchers::ContainsSubstring(fragment));
        }
      }
    }
  }
}
} // namespace

TEST_CASE("SOP D3 fixtures are syntactically valid", "[SOP][D3][fixtures]") {
  for (const auto &example : sopd3::cases()) {
    DYNAMIC_SECTION(example.group << ": " << example.name) {
      INFO(example.source);
      REQUIRE_NOTHROW(parse(example.source));
    }
  }
}

TEST_CASE("SOP D3 semantic assertions work on existing TOP",
          "[SOP][D3][harness]") {
  sopd3::Case example{
      "control",
      "TOP owner transfer",
      "",
      "main() { var p, q, x; p = alloc 7; q = p; x = 3; return *q + x; }",
      {},
      {{"main", "p", "own&int", true},
       {"main", "q", "own&int", true},
       {"main", "x", "int", false}},
      {{"main", {"q"}}},
      {{"own", "p"}, {"move", "p"}, {"own", "q"}},
      {"x"}};
  checkAccepted(example, parse(example.source));
}

// The group labels correspond to the worksheet in REQUIREMENTS3.md.
#define D3_GROUP(number, title)                                                \
  TEST_CASE("SOP D3 G" #number ": " title, "[SOP][D3][G" #number "]") {        \
    runGroup("G" #number);                                                     \
  }
D3_GROUP(1, "literal types and inference")
D3_GROUP(2, "construction, moves, overwrites, and joins")
D3_GROUP(3, "sequence ownership and cleanup")
D3_GROUP(4, "index types and extraction")
D3_GROUP(5, "indexed borrows, mutation, and lifetime")
D3_GROUP(6, "length and borrowed reads")
D3_GROUP(7, "copying slices")
D3_GROUP(8, "range loops and CFG")
D3_GROUP(9, "borrowed iteration types")
D3_GROUP(10, "borrowed iteration lifetimes and uses")
D3_GROUP(11, "lambda typing and scope")
D3_GROUP(12, "function boundaries and lambda effects")
#undef D3_GROUP
