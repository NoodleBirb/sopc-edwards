#include "FreshAlphaCopier.h"
#include "ASTNode.h"
#include <sstream>

/*
 * The Copier inherits all of the methods above from Substituter, but
 * it overrides the behavior for TopVar and TopAlpha.
 */
std::shared_ptr<TopType> FreshAlphaCopier::copy(TopType *t, ASTNode *c) {
  FreshAlphaCopier visitor;
  visitor.context = c;
  t->accept(&visitor);
  return visitor.getResult();
}

void FreshAlphaCopier::endVisit(TopAlpha *element) {
  auto name = element->getName();
  // Separate inner instantiations must stay distinct when copying an enclosing
  // scheme. Source spans keep the resulting inspection names deterministic.
  if (auto *previous = element->getContext(); previous && previous != context) {
    name += "@" + std::to_string(previous->getLine()) + ":" +
            std::to_string(previous->getColumn());
    if (!previous->getLine()) {
      std::ostringstream expression;
      expression << *previous;
      name += ":" + expression.str();
    }
  }
  visitedTypes.push_back(
      std::make_shared<TopAlpha>(element->getNode(), context, name));
}
