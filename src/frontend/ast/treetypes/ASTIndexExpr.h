#pragma once
#include "ASTExpr.h"

class ASTIndexExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> Sequence;
  std::shared_ptr<ASTExpr> Index;

public:
  ASTIndexExpr(std::shared_ptr<ASTExpr> sequence,
               std::shared_ptr<ASTExpr> index)
      : Sequence(std::move(sequence)), Index(std::move(index)) {
    assert(Sequence);
    assert(Index);
  }
  ASTExpr *getSequence() const { return Sequence.get(); }
  ASTExpr *getIndex() const { return Index.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(Sequence, oldChild, newChild) ||
           astReplaceSlot(Index, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
