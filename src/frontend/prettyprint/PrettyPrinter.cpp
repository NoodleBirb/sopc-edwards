#include "PrettyPrinter.h"

#include "ASTCtorPattern.h"
#include "ASTVarPattern.h"
#include "ASTWildcardPattern.h"

#include <iostream>
#include <sstream>

// ---------------------------------------------------------------------------
// Non-visitor helper: convert a pattern subtree to its source-text string.
// Used by endVisit(ASTCaseArm*) to reconstruct arm payloads without relying
// on the visitResults stack (which only has the flat DeclNode strings).
// ---------------------------------------------------------------------------
static std::string patternToString(ASTPattern *p) {
  if (dynamic_cast<ASTWildcardPattern *>(p)) {
    return "_";
  }
  if (auto *vp = dynamic_cast<ASTVarPattern *>(p)) {
    return vp->getName();
  }
  if (auto *cp = dynamic_cast<ASTCtorPattern *>(p)) {
    std::string s = cp->getTag();
    auto subs = cp->getSubPatterns();
    if (!subs.empty()) {
      s += "(";
      bool first = true;
      for (auto *sub : subs) {
        if (!first)
          s += ", ";
        s += patternToString(sub);
        first = false;
      }
      s += ")";
    }
    return s;
  }
  return "?"; // unreachable
}

namespace {
std::string grouped(ASTExpr *expr, std::string text, bool postfix) {
  const bool lambda = dynamic_cast<ASTLambdaExpr *>(expr);
  const bool prefix = dynamic_cast<ASTBorrowExpr *>(expr) ||
                      dynamic_cast<ASTDeRefExpr *>(expr) ||
                      dynamic_cast<ASTAllocExpr *>(expr) ||
                      dynamic_cast<ASTLengthExpr *>(expr);
  return lambda || (postfix && prefix) ? "(" + text + ")" : text;
}
std::vector<std::string> take(std::vector<std::string> &stack, size_t n) {
  std::vector<std::string> result(stack.end() - n, stack.end());
  stack.resize(stack.size() - n);
  return result;
}
} // namespace
std::string PrettyPrinter::expression(ASTNode *node) {
  PrettyPrinter printer;
  node->accept(&printer);
  return printer.visitResults.back();
}
void PrettyPrinter::print(ASTProgram *p, std::ostream &os, char c, int n) {
  PrettyPrinter visitor(os, c, n);
  p->accept(&visitor);
}

/**
 * join last sz items from the visitedResults with delimiter delim. Then, remove
 * these items from the stack
 *
 * the delimiter before the last skip items are not appended (e.g. to not add
 * trailing commas)
 *
 * returns the joined string
 */
std::string joinWithDelim(std::vector<std::string> &visitResults,
                          std::string delim, int sz, int skip) {
  std::string out;
  int i;
  for (i = visitResults.size() - sz; i < visitResults.size() - skip; i++) {
    out += visitResults[i];
    out += delim;
  }
  // skip the delimiter for the last several tokens
  for (; i < visitResults.size(); i++)
    out += visitResults[i];
  visitResults.erase(visitResults.begin() + visitResults.size() - sz,
                     visitResults.end());
  return out;
}

void PrettyPrinter::endVisit(ASTProgram *element) {
  int total = static_cast<int>(element->getTypedecls().size()) +
              static_cast<int>(element->getFunctions().size());
  os << joinWithDelim(visitResults, "\n", total, 1);
  os.flush();
}

/*
 * General approach taken by visit methods.
 * - visit() is used to increase indentation (decrease should happen in
 * endVisit).
 * - endVisit() should expect a string for all of its AST nodes in reverse order
 * in visitResults. Communicate the single string for the visited node by
 * pushing to the back of visitedResults.
 */

/*
 * Before visiting function, record string for signature and setup indentation
 * for body. This visit method pushes a string result, that the endVisit method
 * should extend.
 */
bool PrettyPrinter::visit(ASTFunction *element) {
  indentLevel++;
  return true;
}

/*
 * After visiting function, collect the string representations for the:
 *   statements, declarations, formals, and then function name
 * they are on the visit stack in that order.
 */
void PrettyPrinter::endVisit(ASTFunction *element) {
  auto bodyString =
      joinWithDelim(visitResults, "\n", element->getStmts().size(), 0);
  auto declString =
      joinWithDelim(visitResults, "\n", element->getDeclarations().size(), 0);
  auto formalsString =
      joinWithDelim(visitResults, ", ", element->getFormals().size(), 1);
  auto polyString = element->isPoly() ? " poly" : "";

  // function name is last element on stack, we modify it in place
  visitResults.back() += "(" + formalsString + ")" + polyString + "\n{\n" +
                         declString + bodyString + "}\n";
  indentLevel--;
}

void PrettyPrinter::endVisit(ASTNumberExpr *element) {
  visitResults.push_back(std::to_string(element->getValue()));
}

void PrettyPrinter::endVisit(ASTVariableExpr *element) {
  visitResults.push_back(element->getName());
}

void PrettyPrinter::endVisit(ASTBinaryExpr *element) {
  std::string rightString = visitResults.back();
  visitResults.pop_back();
  std::string leftString = visitResults.back();
  visitResults.pop_back();

  visitResults.push_back("(" + grouped(element->getLeft(), leftString, false) +
                         " " + element->getOp() + " " +
                         grouped(element->getRight(), rightString, false) +
                         ")");
}

void PrettyPrinter::endVisit(ASTInputExpr *element) {
  visitResults.push_back("input");
}

void PrettyPrinter::endVisit(ASTFunAppExpr *element) {
  auto actualsString =
      joinWithDelim(visitResults, ", ", element->getActuals().size(), 1);
  visitResults.back() =
      grouped(element->getFunction(), visitResults.back(), true) + "(" +
      actualsString + ")";
}

void PrettyPrinter::endVisit(ASTAllocExpr *element) {
  std::string init = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(
      "alloc " +
      grouped(dynamic_cast<ASTExpr *>(element->getChildren()[0].get()), init,
              false));
}

void PrettyPrinter::endVisit(ASTBorrowExpr *element) {
  std::string var = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back("&" + grouped(element->getVar(), var, false));
}

void PrettyPrinter::endVisit(ASTDeRefExpr *element) {
  std::string base = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(
      "*" + grouped(dynamic_cast<ASTExpr *>(element->getChildren()[0].get()),
                    base, false));
}

void PrettyPrinter::endVisit(ASTDeclNode *element) {
  visitResults.push_back(element->getName());
}

void PrettyPrinter::endVisit(ASTDeclStmt *element) {
  visitResults.push_back(
      indent() + "var " +
      joinWithDelim(visitResults, ", ", element->getVars().size(), 1) + ";");
}

void PrettyPrinter::endVisit(ASTAssignStmt *element) {
  std::string rhsString = visitResults.back();
  visitResults.pop_back();
  std::string lhsString = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(indent() + lhsString + " = " + rhsString + ";");
}

bool PrettyPrinter::visit(ASTBlockStmt *element) {
  indentLevel++;
  return true;
}

void PrettyPrinter::endVisit(ASTBlockStmt *element) {
  indentLevel--;
  visitResults.push_back(
      indent() + "{\n" +
      joinWithDelim(visitResults, "\n", element->getStmts().size(), 0) +
      indent() + "}");
}

/*
 * For a while the body should be indented, but not the condition.
 * Since conditions are expressions and their visit methods never indent
 * incrementing here works.
 */
bool PrettyPrinter::visit(ASTWhileStmt *element) {
  indentLevel++;
  return true;
}

void PrettyPrinter::endVisit(ASTWhileStmt *element) {
  std::string bodyString = visitResults.back();
  visitResults.pop_back();
  std::string condString = visitResults.back();
  visitResults.pop_back();

  indentLevel--;

  std::string whileString =
      indent() + "while (" + condString + ")\n" + bodyString;
  visitResults.push_back(whileString);
}

bool PrettyPrinter::visit(ASTIfStmt *element) {
  indentLevel++;
  return true;
}

void PrettyPrinter::endVisit(ASTIfStmt *element) {
  std::string elseString;
  if (element->getElse() != nullptr) {
    elseString = visitResults.back();
    visitResults.pop_back();
  }

  std::string thenString = visitResults.back();
  visitResults.pop_back();

  std::string condString = visitResults.back();
  visitResults.pop_back();

  indentLevel--;

  std::string ifString = indent() + "if (" + condString + ")\n" + thenString;

  if (element->getElse() != nullptr) {
    ifString += "\n" + indent() + "else\n" + elseString;
  }

  visitResults.push_back(ifString);
}

void PrettyPrinter::endVisit(ASTOutputStmt *element) {
  std::string argString = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(indent() + "output " + argString + ";");
}

void PrettyPrinter::endVisit(ASTErrorStmt *element) {
  std::string argString = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(indent() + "error " + argString + ";");
}

void PrettyPrinter::endVisit(ASTReturnStmt *element) {
  std::string argString = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(indent() + "return " + argString + ";");
}

std::string PrettyPrinter::indent() const {
  return std::string(indentLevel * indentSize, indentChar);
}

// TOP/SOP pretty-printer implementations

void PrettyPrinter::endVisit(ASTSumCtorExpr *element) {
  if (element->getArgs().empty()) {
    visitResults.push_back(element->getTag());
  } else {
    std::string argsStr =
        joinWithDelim(visitResults, ", ", element->getArgs().size(), 1);
    visitResults.push_back(element->getTag() + "(" + argsStr + ")");
  }
}

void PrettyPrinter::endVisit(ASTSumVariant *element) {
  if (element->getParams().empty()) {
    visitResults.push_back(element->getTag());
  } else {
    std::string paramsStr =
        joinWithDelim(visitResults, ", ", element->getParams().size(), 1);
    visitResults.push_back(element->getTag() + "(" + paramsStr + ")");
  }
}

void PrettyPrinter::endVisit(ASTSumTypeDecl *element) {
  std::string variantsStr =
      joinWithDelim(visitResults, " | ", element->getVariants().size(), 1);
  visitResults.push_back("type " + element->getName() + " = " + variantsStr +
                         ";");
}

bool PrettyPrinter::visit(ASTCaseStmt *element) {
  indentLevel++;
  return true;
}

void PrettyPrinter::endVisit(ASTCaseStmt *element) {
  indentLevel--;
  // Arms are on top of the stack; the case expression is below them.
  std::string armsStr =
      joinWithDelim(visitResults, "\n", element->getArms().size(), 1);
  std::string scrutStr = visitResults.back();
  visitResults.pop_back();
  visitResults.push_back(indent() + "case " + scrutStr + " of {\n" + armsStr +
                         "\n" + indent() + "}");
}

void PrettyPrinter::endVisit(ASTCaseArm *element) {
  std::string bodyStr = visitResults.back();
  visitResults.pop_back();
  // strip leading whitespace so the body is inline after "->"
  bodyStr.erase(0, bodyStr.find_first_not_of(" \t"));

  // Discard the flat DeclNode strings pushed by visiting BINDINGS
  // (they exist for LocalNameCollector backward compat, not for pretty-print).
  for (size_t i = 0; i < element->getBindings().size(); i++) {
    visitResults.pop_back();
  }

  // Reconstruct the payload string using the actual pattern tree.
  std::string bindingsStr;
  auto patterns = element->getPatterns();
  if (!patterns.empty()) {
    bindingsStr = "(";
    bool first = true;
    for (auto *p : patterns) {
      if (!first)
        bindingsStr += ", ";
      bindingsStr += patternToString(p);
      first = false;
    }
    bindingsStr += ")";
  }
  visitResults.push_back(indent() + element->getTag() + bindingsStr + " -> " +
                         bodyStr);
}

// ASTDestroyStmt is a synthetic leaf inserted by DestructionPass.
// It is skipped in pretty-printed output (not part of source syntax).
bool PrettyPrinter::visit(ASTDestroyStmt *element) {
  return false; // no children to visit
}

void PrettyPrinter::endVisit(ASTDestroyStmt *element) {
  // Push a placeholder so endVisit(ASTFunction) pops the right number of
  // results.
  visitResults.push_back(indent() + "/* destroy " +
                         element->getVar()->getName() + " */");
}

void PrettyPrinter::endVisit(ASTSequenceExpr *element) {
  auto children = take(visitResults, element->getElements().size());
  std::string result = "[", sep;
  for (auto &child : children) {
    result += sep + child;
    sep = ", ";
  }
  visitResults.push_back(result + "]");
}
void PrettyPrinter::endVisit(ASTSliceExpr *element) {
  auto c = take(visitResults, element->getChildren().size());
  size_t i = 1;
  std::string result = grouped(element->getSequence(), c[0], true) + "[";
  if (element->getLowerBound())
    result += c[i++];
  result += ":";
  if (element->getUpperBound())
    result += c[i++];
  visitResults.push_back(result + "]");
}
void PrettyPrinter::endVisit(ASTIndexExpr *element) {
  auto c = take(visitResults, 2);
  visitResults.push_back(grouped(element->getSequence(), c[0], true) + "[" +
                         c[1] + "]");
}
void PrettyPrinter::endVisit(ASTLengthExpr *element) {
  visitResults.back() =
      "#" + grouped(element->getSequence(), visitResults.back(), false);
}
void PrettyPrinter::endVisit(ASTLambdaExpr *element) {
  auto c = take(visitResults, element->getParameters().size() + 1);
  std::string result = "(", sep;
  for (size_t i = 0; i + 1 < c.size(); ++i) {
    result += sep + c[i];
    sep = ", ";
  }
  visitResults.push_back(result + ") => " + c.back());
}
void PrettyPrinter::endVisit(ASTRange *element) {
  auto c = take(visitResults, 2);
  visitResults.push_back(c[0] + " .. " + c[1]);
}
bool PrettyPrinter::visit(ASTForStmt *element) {
  ++indentLevel;
  return true;
}
void PrettyPrinter::endVisit(ASTForStmt *element) {
  auto c = take(visitResults, element->getChildren().size());
  --indentLevel;
  std::string result = indent() + "for (" + c[0] + " : " + c[1];
  if (element->getStep())
    result += " by " + c[2];
  visitResults.push_back(result + ")\n" + c.back());
}
