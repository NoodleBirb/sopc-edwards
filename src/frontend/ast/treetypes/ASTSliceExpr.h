#pragma once
#include "ASTExpr.h"

class ASTSliceExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> Sequence;
  std::shared_ptr<ASTExpr> LowerBound;
  std::shared_ptr<ASTExpr> UpperBound;

public:
  ASTSliceExpr(std::shared_ptr<ASTExpr> sequence,
               std::shared_ptr<ASTExpr> lowerbound,
               std::shared_ptr<ASTExpr> upperbound)
      : Sequence(std::move(sequence)), LowerBound(std::move(lowerbound)),
        UpperBound(std::move(upperbound)) {
    assert(Sequence);
  }
  ASTExpr *getSequence() const { return Sequence.get(); }
  ASTExpr *getLowerBound() const { return LowerBound.get(); }
  ASTExpr *getUpperBound() const { return UpperBound.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(Sequence, oldChild, newChild) ||
           astReplaceSlot(LowerBound, oldChild, newChild) ||
           astReplaceSlot(UpperBound, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
