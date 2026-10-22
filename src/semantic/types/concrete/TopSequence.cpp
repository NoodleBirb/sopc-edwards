#include "TopSequence.h"
#include "TopTypeVisitor.h"
void TopSequence::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this))
    getElementType()->accept(visitor);
  visitor->endVisit(this);
}
