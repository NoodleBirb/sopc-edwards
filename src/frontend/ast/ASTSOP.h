#pragma once
#include "ASTVisitor.h"

// Detect source constructs whose later-phase semantics are not implemented yet.
inline bool containsSOP(ASTNode *root) {
  class Detector : public ASTVisitor {
  public:
    bool found = false;
    bool visit(ASTSequenceExpr *) override {
      found = true;
      return false;
    }
    bool visit(ASTSliceExpr *) override {
      found = true;
      return false;
    }
    bool visit(ASTIndexExpr *) override {
      found = true;
      return false;
    }
    bool visit(ASTLengthExpr *) override {
      found = true;
      return false;
    }
    bool visit(ASTLambdaExpr *) override {
      found = true;
      return false;
    }
    bool visit(ASTRange *) override {
      found = true;
      return false;
    }
    bool visit(ASTForStmt *) override {
      found = true;
      return false;
    }
  } detector;
  root->accept(&detector);
  return detector.found;
}
