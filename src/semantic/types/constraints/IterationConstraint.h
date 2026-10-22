#pragma once
#include "TopType.h"
class ASTForStmt;

// binder = T for Copy T, borrow&T for Own T. Copy this relation with its
// scheme.
struct IterationConstraint {
  std::shared_ptr<TopType> element;
  std::shared_ptr<TopType> binder;
  ASTForStmt *loop;
};
