#pragma once
#include "ASTDeclNode.h"
#include "ASTExpr.h"

class ASTLambdaExpr : public ASTExpr {
  std::vector<std::shared_ptr<ASTDeclNode>> Parameters;
  std::shared_ptr<ASTExpr> Body;

public:
  ASTLambdaExpr(std::vector<std::shared_ptr<ASTDeclNode>> parameters,
                std::shared_ptr<ASTExpr> body)
      : Parameters(std::move(parameters)), Body(std::move(body)) {
    assert(Body);
    assert(!Parameters.empty());
  }
  std::vector<ASTDeclNode *> getParameters() const {
    std::vector<ASTDeclNode *> result;
    for (auto &item : Parameters)
      result.push_back(item.get());
    return result;
  }
  ASTExpr *getBody() const { return Body.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceVec(Parameters, oldChild, newChild) ||
           astReplaceSlot(Body, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};
