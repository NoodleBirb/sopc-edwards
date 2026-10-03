#pragma once
#include "ASTExpr.h"
#include "ASTNode.h"

class ASTRange : public ASTNode {
  std::shared_ptr<ASTExpr> Start;
  std::shared_ptr<ASTExpr> End;

public:
  ASTRange(std::shared_ptr<ASTExpr> start, std::shared_ptr<ASTExpr> end)
      : Start(std::move(start)), End(std::move(end)) {
    assert(Start);
    assert(End);
  }
  ASTExpr *getStart() const { return Start.get(); }
  ASTExpr *getEnd() const { return End.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(Start, oldChild, newChild) ||
           astReplaceSlot(End, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
