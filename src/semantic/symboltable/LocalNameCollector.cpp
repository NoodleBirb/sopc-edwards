#include "LocalNameCollector.h"
#include "../SemanticLogging.h"
#include "SemanticError.h"

std::map<ASTDeclNode *, std::map<std::string, ASTDeclNode *>>
LocalNameCollector::build(
    ASTProgram *p, std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap) {
  LocalNameCollector visitor(fMap);
  p->accept(&visitor);
  return visitor.lMap;
}

bool LocalNameCollector::visit(ASTFunction *element) {
  curMap.clear();
  variableDeclarations.clear();
  caseArmBindingNames.clear();
  funName = element->getName();
  first = true;
  return true;
}

void LocalNameCollector::endVisit(ASTFunction *element) {
  auto decl = element->getDecl();
  SEMANTIC_LOG(2, "symbol-table")
      << "add function=" << decl->getName() << " line=" << decl->getLine()
      << " column=" << decl->getColumn();
  lMap.insert(std::pair<ASTDeclNode *, std::map<std::string, ASTDeclNode *>>(
      decl, curMap));
}

void LocalNameCollector::endVisit(ASTDeclNode *element) {
  if (first) {
    // first declaration found in visiting a function is the function name which
    // is in the function map so we skip it
    first = false;
  } else {
    if (fMap.count(element->getName()) == 0) {
      if (curMap.count(element->getName()) == 0 ||
          caseArmBindingNames.count(element->getName()) != 0) {
        // Either first occurrence, or a case arm binding being re-bound by
        // a subsequent case arm — silently overwrite.
        SEMANTIC_LOG(2, "symbol-table") << "add local=" << element->getName()
                                        << " line=" << element->getLine()
                                        << " column=" << element->getColumn();
        curMap[element->getName()] = element;
      } else {
        throw SemanticError(
            "Symbol error line " + std::to_string(element->getLine()) +
            " in column " + std::to_string(element->getColumn()) + ": " +
            element->getName() + " redeclared in function " + funName + "\n");
      }
    } else {
      throw SemanticError(
          "Symbol error line " + std::to_string(element->getLine()) +
          " in column " + std::to_string(element->getColumn()) + ": " +
          element->getName() + " already declared as function\n");
    }
  }
}

void LocalNameCollector::endVisit(ASTVariableExpr *element) {
  auto local = curMap.find(element->getName());
  if (local != curMap.end())
    resolutions[element] = local->second;
  else if (fMap.count(element->getName()))
    resolutions[element] = fMap.at(element->getName()).first;
  if (fMap.count(element->getName()) == 0) {
    if (curMap.count(element->getName()) == 0) {
      throw SemanticError(
          "Symbol error line " + std::to_string(element->getLine()) +
          " in column " + std::to_string(element->getColumn()) + ": " +
          element->getName() + " undeclared in function " + funName + "\n");
    }
  }
}

// ---- Case arm binding scoping ----

bool LocalNameCollector::visit(ASTCaseArm *element) {
  // Register binding names so they may be overwritten by a subsequent arm.
  for (auto b : element->getBindings())
    caseArmBindingNames.insert(b->getName());
  return true;
}

void LocalNameCollector::endVisit(ASTCaseArm *element) {
  // Arm bindings stay in curMap (and thus in localNames) for type inference.
  // The caseArmBindingNames set allows subsequent arms with the same binding
  // name to overwrite without a "redeclared" error.
}

bool LocalNameCollector::visit(ASTDeclStmt *element) {
  for (auto &child : element->getChildren())
    variableDeclarations.insert(dynamic_cast<ASTDeclNode *>(child.get()));
  return true;
}

bool LocalNameCollector::visit(ASTLambdaExpr *element) {
  lambdas.push_back(element);
  auto enclosing = curMap;
  std::set<std::string> parameters;
  for (auto *param : element->getParameters()) {
    if (!parameters.insert(param->getName()).second)
      throw SemanticError(
          "Symbol error line " + std::to_string(param->getLine()) +
          " in column " + std::to_string(param->getColumn()) + ": " +
          param->getName() + " duplicated in lambda parameter list");
  }
  for (auto *param : element->getParameters()) {
    if (curMap.count(param->getName()) || fMap.count(param->getName()))
      throw SemanticError(
          "Symbol error line " + std::to_string(param->getLine()) +
          " in column " + std::to_string(param->getColumn()) + ": " +
          param->getName() + " shadows an existing declaration");
    curMap[param->getName()] = param;
  }
  element->getBody()->accept(this);
  curMap = std::move(enclosing);
  return false; // Parameters were registered in their own scope above.
}

bool LocalNameCollector::visit(ASTForStmt *element) {
  auto *binder = element->getBinder();
  auto found = curMap.find(binder->getName());
  if (found == curMap.end() || !variableDeclarations.count(found->second))
    throw SemanticError(
        "Symbol error line " + std::to_string(binder->getLine()) +
        " in column " + std::to_string(binder->getColumn()) + ": " +
        binder->getName() + " undeclared in function " + funName);
  element->setDeclaration(found->second);
  return true;
}
