#pragma once
#include "ASTLambdaExpr.h"
#include "ASTProgram.h"
#include <functional>
#include <set>

// Keep the source lambda, and share its parameters/body with a function body.
// The source lambda becomes a leaf for analysis visitors after preparation.
inline void prepareLambdas(ASTProgram *program) {
  std::set<ASTNode *> seen;
  std::vector<std::shared_ptr<ASTFunction>> functions;
  std::function<void(ASTNode *)> walk = [&](ASTNode *node) {
    if (!seen.insert(node).second)
      return;
    for (const auto &child : node->getChildren())
      walk(child.get());
    if (auto *lambda = dynamic_cast<ASTLambdaExpr *>(node)) {
      if (!lambda->getFunction())
        functions.push_back(
            lambda->materialize("$lambda@" + std::to_string(lambda->getLine()) +
                                ":" + std::to_string(lambda->getColumn())));
    }
  };
  walk(program);
  for (auto &function : functions)
    program->addSemanticFunction(function);
}
