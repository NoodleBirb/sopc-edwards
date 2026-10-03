#pragma once
#include "ASTBlockStmt.h"
#include "ASTDeclNode.h"
#include "ASTExpr.h"
#include "ASTNode.h"
#include "ASTStmt.h"
#include "ASTVariableExpr.h"

class ASTForStmt : public ASTStmt {
  std::shared_ptr<ASTVariableExpr> Binder;
  std::shared_ptr<ASTNode> Input;
  std::shared_ptr<ASTExpr> Step;
  std::shared_ptr<ASTBlockStmt> Body;

public:
  ASTForStmt(std::shared_ptr<ASTVariableExpr> binder,
             std::shared_ptr<ASTNode> input, std::shared_ptr<ASTExpr> step,
             std::shared_ptr<ASTBlockStmt> body)
      : Binder(std::move(binder)), Input(std::move(input)),
        Step(std::move(step)), Body(std::move(body)) {
    assert(Binder);
    assert(Input);
    assert(Body);
  }
  ASTVariableExpr *getBinder() const { return Binder.get(); }
  ASTNode *getInput() const { return Input.get(); }
  ASTExpr *getStep() const { return Step.get(); }
  ASTBlockStmt *getBody() const { return Body.get(); }
  ASTDeclNode *getDeclaration() const { return Declaration; }
  void setDeclaration(ASTDeclNode *decl) { Declaration = decl; }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  void accept(ASTVisitor *visitor) override;
  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(Binder, oldChild, newChild) ||
           astReplaceSlot(Input, oldChild, newChild) ||
           astReplaceSlot(Step, oldChild, newChild) ||
           astReplaceSlot(Body, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  ASTDeclNode *Declaration =
      nullptr; // Non-owning link resolved by symbol processing.
};
