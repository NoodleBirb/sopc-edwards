#pragma once
#include "ASTExpr.h"

class ASTLengthExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> Sequence;

public:
  ASTLengthExpr(std::shared_ptr<ASTExpr> sequence)
      : Sequence(std::move(sequence)) {
    assert(Sequence);
  }
  ASTExpr *getSequence() const { return Sequence.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(Sequence, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
