#include "ASTVisitor.h"
#include <sstream>

void ASTSequenceExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTSequenceExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  for (auto &child : Elements)
    result.push_back(child);
  return result;
}
void ASTSliceExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTSliceExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  result.push_back(Sequence);
  if (LowerBound)
    result.push_back(LowerBound);
  if (UpperBound)
    result.push_back(UpperBound);
  return result;
}
void ASTIndexExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTIndexExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  result.push_back(Sequence);
  result.push_back(Index);
  return result;
}
void ASTLengthExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTLengthExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  result.push_back(Sequence);
  return result;
}
void ASTLambdaExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this) &&
      (!getFunction() || visitor->visitMaterializedLambdaBody()))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTLambdaExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  for (auto &child : Parameters)
    result.push_back(child);
  result.push_back(Body);
  return result;
}
void ASTRange::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTRange::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  result.push_back(Start);
  result.push_back(End);
  return result;
}
void ASTForStmt::accept(ASTVisitor *visitor) {
  if (visitor->visit(this))
    for (auto &child : getChildren())
      child->accept(visitor);
  visitor->endVisit(this);
}
std::vector<std::shared_ptr<ASTNode>> ASTForStmt::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> result;
  result.push_back(Binder);
  result.push_back(Input);
  if (Step)
    result.push_back(Step);
  result.push_back(Body);
  return result;
}
std::ostream &ASTSequenceExpr::print(std::ostream &out) const {
  out << "[";
  const char *sep = "";
  for (auto *e : getElements()) {
    out << sep << *e;
    sep = ", ";
  }
  return out << "]";
}
std::ostream &ASTSliceExpr::print(std::ostream &out) const {
  out << *getSequence() << "[";
  if (getLowerBound())
    out << *getLowerBound();
  out << ":";
  if (getUpperBound())
    out << *getUpperBound();
  return out << "]";
}
std::ostream &ASTIndexExpr::print(std::ostream &out) const {
  return out << *getSequence() << "[" << *getIndex() << "]";
}
std::ostream &ASTLengthExpr::print(std::ostream &out) const {
  return out << "#" << *getSequence();
}
std::ostream &ASTLambdaExpr::print(std::ostream &out) const {
  out << "(";
  const char *sep = "";
  for (auto *p : getParameters()) {
    out << sep << *p;
    sep = ", ";
  }
  return out << ") => " << *getBody();
}
std::ostream &ASTRange::print(std::ostream &out) const {
  return out << *getStart() << " .. " << *getEnd();
}
std::ostream &ASTForStmt::print(std::ostream &out) const {
  out << "for (" << *getBinder() << " : " << *getInput();
  if (getStep())
    out << " by " << *getStep();
  return out << ") " << *getBody();
}
