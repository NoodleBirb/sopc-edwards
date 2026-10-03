#pragma once
#include "ASTExpr.h"

class ASTSequenceExpr : public ASTExpr {
  std::vector<std::shared_ptr<ASTExpr>> Elements;

public:
  ASTSequenceExpr(std::vector<std::shared_ptr<ASTExpr>> elements)
      : Elements(std::move(elements)) {}
  std::vector<ASTExpr *> getElements() const {
    std::vector<ASTExpr *> result;
    for (auto &item : Elements)
      result.push_back(item.get());
    return result;
  }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceVec(Elements, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
