#pragma once
#include "MoveAnalysis.h"
#include "TopType.h"
#include <memory>
#include <ostream>
#include <vector>

struct CleanupRecipe {
  // Sequence/Sum recipes visit owned children before releasing their storage.
  enum class Kind {
    Copy,
    Reference,
    Sequence,
    Sum,
    RecursiveBody,
    RecursiveReference,
    Generic
  };
  Kind kind;
  std::shared_ptr<TopType> type;
  std::string recursiveName;
  std::vector<std::shared_ptr<CleanupRecipe>> children;
};

struct CleanupAction {
  enum class Kind { Destroy, Transfer };
  enum class Point {
    AfterExpression,
    AfterReturnValue,
    AfterBorrowedOperation
  };
  Kind kind;
  ASTFunction *function;
  ASTDeclNode *owner = nullptr;
  ASTExpr *temporary = nullptr;
  ASTNode *point = nullptr;
  Point cleanupPoint = Point::AfterExpression;
  std::string recipient;
  std::shared_ptr<TopType> type;
  std::shared_ptr<CleanupRecipe> recipe;
};

class DestructionPlan : public MoveAnalysis::DestructionPlan {
  std::vector<std::shared_ptr<CleanupAction>> actions;

public:
  static std::shared_ptr<DestructionPlan>
  build(ASTProgram *program, SymbolTable *symbols,
        FunctionEffectSummaries *effects, const MoveAnalysis &moves);
  const std::vector<std::shared_ptr<CleanupAction>> &getActions() const {
    return actions;
  }
  void print(std::ostream &out) const;
};
