#include "DestructionPlan.h"
#include "ASTAllocExpr.h"
#include "ASTAssignStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTDestroyStmt.h"
#include "ASTForStmt.h"
#include "ASTFunAppExpr.h"
#include "ASTLambdaExpr.h"
#include "ASTLengthExpr.h"
#include "ASTReturnStmt.h"
#include "ASTSequenceExpr.h"
#include "ASTSliceExpr.h"
#include "ASTSumCtorExpr.h"
#include "ASTVariableExpr.h"
#include "TopMu.h"
#include "TopSequence.h"
#include "TopVar.h"
#include "TypeInference.h"
#include <algorithm>
#include <functional>
#include <map>
#include <sstream>

namespace {
std::string expression(ASTNode *node) {
  std::ostringstream out;
  out << *node;
  return out.str();
}
std::shared_ptr<CleanupRecipe>
recipe(const std::shared_ptr<TopType> &type,
       std::map<std::string, std::string> bound = {}) {
  auto result = std::make_shared<CleanupRecipe>();
  result->type = type;
  if (auto *mu = dynamic_cast<TopMu *>(type.get())) {
    result->kind = CleanupRecipe::Kind::RecursiveBody;
    result->recursiveName = "recursive-" + std::to_string(bound.size());
    bound[mu->getV()->toString()] = result->recursiveName;
    result->children.push_back(recipe(mu->getT(), bound));
  } else if (dynamic_cast<TopVar *>(type.get())) {
    const auto found = bound.find(type->toString());
    result->kind = found == bound.end()
                       ? CleanupRecipe::Kind::Generic
                       : CleanupRecipe::Kind::RecursiveReference;
    if (found != bound.end())
      result->recursiveName = found->second;
  } else if (auto *sequence = dynamic_cast<TopSequence *>(type.get())) {
    result->kind = CleanupRecipe::Kind::Sequence;
    result->children.push_back(recipe(sequence->getElementType(), bound));
  } else if (OwnershipClassifier::classifyType(type.get()) ==
             OwnershipClass::Copy) {
    result->kind = CleanupRecipe::Kind::Copy;
  } else if (type->getFunctor() == "ref") {
    result->kind = CleanupRecipe::Kind::Reference;
  } else {
    result->kind = CleanupRecipe::Kind::Sum;
    for (const auto &child : type->getChildTypes())
      result->children.push_back(recipe(child, bound));
  }
  return result;
}
void printRecipe(const CleanupRecipe &recipe, std::ostream &out,
                 std::string indent) {
  using Kind = CleanupRecipe::Kind;
  switch (recipe.kind) {
  case Kind::Copy:
    out << indent << "Copy: no cleanup\n";
    break;
  case Kind::Generic:
    out << indent << "Copy requirement: no element cleanup for " << *recipe.type
        << "\n";
    break;
  case Kind::Reference:
    out << indent << "release owning reference " << *recipe.type << "\n";
    break;
  case Kind::Sequence:
    out << indent << "sequence elements ("
        << (recipe.children[0]->kind == Kind::Copy ||
                    recipe.children[0]->kind == Kind::Generic
                ? "Copy"
                : "Own")
        << "):\n";
    printRecipe(*recipe.children[0], out, indent + "  ");
    out << indent << "then release sequence storage\n";
    break;
  case Kind::Sum:
    out << indent << "owned constructor payloads:\n";
    for (const auto &child : recipe.children)
      if (child->kind != Kind::Copy)
        printRecipe(*child, out, indent + "  ");
    out << indent << "then release sum storage\n";
    break;
  case Kind::RecursiveBody:
    out << indent << "strategy " << recipe.recursiveName << ":\n";
    printRecipe(*recipe.children[0], out, indent + "  ");
    break;
  case Kind::RecursiveReference:
    out << indent << "apply strategy " << recipe.recursiveName << "\n";
    break;
  }
}
} // namespace

std::shared_ptr<DestructionPlan>
DestructionPlan::build(ASTProgram *program, SymbolTable *,
                       FunctionEffectSummaries *effects,
                       const MoveAnalysis &moves) {
  auto plan = std::make_shared<DestructionPlan>();
  static_cast<MoveAnalysis::DestructionPlan &>(*plan) = moves.destructionPlan();
  for (auto *function : program->getFunctions()) {
    auto *scope = function->getDecl();
    auto found = moves.getExitStates().find(scope);
    if (found != moves.getExitStates().end()) {
      std::vector<ASTDeclNode *> retained;
      for (const auto &[decl, state] : found->second)
        if (state == MoveAnalysis::OwnershipState::Owned)
          retained.push_back(decl);
      std::sort(retained.begin(), retained.end(),
                [](auto *a, auto *b) { return a->getName() < b->getName(); });
      for (auto *owner : retained) {
        auto action = std::make_shared<CleanupAction>();
        action->kind = CleanupAction::Kind::Destroy;
        action->function = function;
        action->owner = owner;
        action->point = function->getStmts().back();
        action->cleanupPoint = CleanupAction::Point::AfterReturnValue;
        action->type = effects->getTypes()->getInferredType(owner);
        action->recipe = recipe(action->type);
        plan->actions.push_back(action);
        effects->addCleanupRequirement(scope, action->type, owner->getLine());
      }
    }

    std::vector<ASTNode *> ancestors;
    std::function<void(ASTNode *, ASTNode *)> walk = [&](ASTNode *node,
                                                         ASTNode *parent) {
      if (dynamic_cast<ASTLambdaExpr *>(node) ||
          dynamic_cast<ASTDestroyStmt *>(node))
        return;
      auto *expr = dynamic_cast<ASTExpr *>(node);
      const bool producesValue = dynamic_cast<ASTAllocExpr *>(node) ||
                                 dynamic_cast<ASTSequenceExpr *>(node) ||
                                 dynamic_cast<ASTSliceExpr *>(node) ||
                                 dynamic_cast<ASTFunAppExpr *>(node) ||
                                 dynamic_cast<ASTSumCtorExpr *>(node);
      if (expr && producesValue) {
        auto type = effects->typeOf(expr, scope);
        if (OwnershipClassifier::classifyType(type.get()) ==
            OwnershipClass::Own) {
          auto action = std::make_shared<CleanupAction>();
          action->kind = CleanupAction::Kind::Destroy;
          action->function = function;
          action->temporary = expr;
          action->point = parent;
          action->type = type;
          action->recipe = recipe(type);
          if (auto *assign = dynamic_cast<ASTAssignStmt *>(parent)) {
            if (assign->getRHS() == expr)
              action->recipient = "binding " + expression(assign->getLHS());
          } else if (dynamic_cast<ASTReturnStmt *>(parent))
            action->recipient = "return result";
          else if (dynamic_cast<ASTSequenceExpr *>(parent))
            action->recipient = "enclosing sequence element";
          else if (dynamic_cast<ASTSumCtorExpr *>(parent))
            action->recipient = "enclosing constructor payload";
          else if (auto *call = dynamic_cast<ASTFunAppExpr *>(parent)) {
            const auto actuals = call->getActuals();
            if (std::find(actuals.begin(), actuals.end(), expr) !=
                actuals.end())
              action->recipient =
                  "argument of " + expression(call->getFunction());
          }
          if (!action->recipient.empty())
            action->kind = CleanupAction::Kind::Transfer;
          else {
            for (auto ancestor = ancestors.rbegin();
                 ancestor != ancestors.rend(); ++ancestor)
              if (dynamic_cast<ASTBorrowExpr *>(*ancestor)) {
                for (auto lifetime = ancestor + 1; lifetime != ancestors.rend();
                     ++lifetime)
                  if (dynamic_cast<ASTFunAppExpr *>(*lifetime) ||
                      dynamic_cast<ASTForStmt *>(*lifetime)) {
                    action->point = *lifetime;
                    action->cleanupPoint =
                        CleanupAction::Point::AfterBorrowedOperation;
                    break;
                  }
                break;
              }
            effects->addCleanupRequirement(scope, type, expr->getLine());
          }
          plan->actions.push_back(std::move(action));
        }
      }
      ancestors.push_back(node);
      for (const auto &child : node->getChildren())
        walk(child.get(), node);
      ancestors.pop_back();
    };
    walk(function, nullptr);
  }
  effects->validateRequirements();
  return plan;
}

void DestructionPlan::print(std::ostream &out) const {
  out << "\n[destruction-plan]\n";
  for (const auto &action : actions) {
    out << "  "
        << (action->kind == CleanupAction::Kind::Transfer ? "transfer "
                                                          : "cleanup ");
    if (action->owner)
      out << action->function->getName() << "." << action->owner->getName();
    else
      out << "temporary " << *action->temporary << " @"
          << action->temporary->getLine() << ":"
          << action->temporary->getColumn();
    out << " : " << *action->type;
    if (action->kind == CleanupAction::Kind::Transfer)
      out << " -> " << action->recipient << "\n";
    else {
      if (action->cleanupPoint == CleanupAction::Point::AfterReturnValue)
        out << " after return value evaluation\n";
      else if (action->cleanupPoint ==
               CleanupAction::Point::AfterBorrowedOperation)
        out << " after borrowed operation " << *action->point << "\n";
      else
        out << (dynamic_cast<ASTLengthExpr *>(action->point)
                    ? " after length "
                    : " after expression ")
            << *action->point << "\n";
      printRecipe(*action->recipe, out, "    ");
    }
  }
}
