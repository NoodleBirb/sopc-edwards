#pragma once
#include "ASTDeclNode.h"
#include "ASTExpr.h"
#include "ASTFunction.h"
#include "ASTReturnStmt.h"

class ASTLambdaExpr : public ASTExpr {
  std::vector<std::shared_ptr<ASTDeclNode>> Parameters;
  std::shared_ptr<ASTExpr> Body;
  std::shared_ptr<ASTFunction> Function;

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
  ASTFunction *getFunction() const { return Function.get(); }
  std::shared_ptr<ASTFunction> materialize(const std::string &name) {
    if (!Function) {
      auto decl = std::make_shared<ASTDeclNode>(name);
      decl->setLocation(getLine(), getColumn());
      auto ret = std::make_shared<ASTReturnStmt>(Body);
      ret->setLocation(Body->getLine(), Body->getColumn());
      Function = std::make_shared<ASTFunction>(
          decl, Parameters, std::vector<std::shared_ptr<ASTDeclStmt>>{},
          std::vector<std::shared_ptr<ASTStmt>>{ret}, false);
      Function->setLocation(getLine(), getColumn());
      Function->setLambdaFunction();
    }
    return Function;
  }
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
