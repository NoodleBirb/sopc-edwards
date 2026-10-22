#include "BorrowChecker.h"
#include "../SemanticLogging.h"
#include "RuleToggles.h"

#include "ASTAssignStmt.h"
#include "ASTBinaryExpr.h"
#include "ASTBlockStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTDeRefExpr.h"
#include "ASTErrorStmt.h"
#include "ASTForStmt.h"
#include "ASTFunAppExpr.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTIndexExpr.h"
#include "ASTLambdaExpr.h"
#include "ASTLengthExpr.h"
#include "ASTOutputStmt.h"
#include "ASTProgram.h"
#include "ASTRange.h"
#include "ASTReturnStmt.h"
#include "ASTSequenceExpr.h"
#include "ASTSliceExpr.h"
#include "ASTSumCtorExpr.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"
#include "OwnershipClassifier.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SymbolTable.h"
#include "TopBorrowRef.h"
#include "TopVar.h"
#include "TypeInference.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <utility>

std::vector<BorrowChecker::BorrowTraceEvent> BorrowChecker::lastTrace;
std::vector<BorrowChecker::BorrowProvenanceEvent> BorrowChecker::lastProvenance;

namespace {

std::string repr(const ASTNode *node) {
  std::ostringstream oss;
  oss << *node;
  return oss.str();
}

std::string calleeName(ASTFunAppExpr *call) {
  if (auto *var = dynamic_cast<ASTVariableExpr *>(call->getFunction())) {
    return var->getName();
  }
  return repr(call->getFunction());
}

bool isBorrow(ASTExpr *e) {
  return dynamic_cast<ASTBorrowExpr *>(e) != nullptr;
}

void rejectPosition(int line, const char *where) {
  std::ostringstream oss;
  oss << "Borrow error on line " << line << ": borrow expression " << where
      << "\n";
  RuleToggles::reject("borrow-position", oss.str());
}

void rejectNotArgument(ASTBorrowExpr *element) {
  std::ostringstream oss;
  oss << "Borrow error on line " << element->getLine()
      << ": borrow expression must be an immediate function argument"
         " — storing a borrow in a variable or using it in any other"
         " position is not permitted";
  RuleToggles::reject("borrow-position", oss.str());
}

} // namespace

// ===========================================================================
// check(): before types, `&x` only as a call argument
// ===========================================================================

// Pre-order: approve every direct borrow actual before its endVisit fires.
bool BorrowChecker::visit(ASTFunAppExpr *element) {
  auto actuals = element->getActuals();
  for (std::size_t index = 0; index < actuals.size(); ++index) {
    if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(actuals[index])) {
      approvedBorrows.insert(borrow);
      addProvenance(provenance,
                    {BorrowProvenanceEvent::Kind::Direct, borrow->getLine(),
                     borrow->getColumn(), repr(borrow), 0, element->getLine(),
                     element->getColumn(), repr(borrow), calleeName(element),
                     index});
    }
  }
  return true;
}

bool BorrowChecker::visit(ASTForStmt *loop) {
  if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(loop->getInput())) {
    approvedBorrows.insert(borrow);
    addProvenance(provenance,
                  {BorrowProvenanceEvent::Kind::Loop, borrow->getLine(),
                   borrow->getColumn(), repr(borrow), 0, loop->getLine(),
                   loop->getColumn(), repr(borrow), "", 0});
  }
  return true;
}

// Positions that are never a call argument, named in the diagnostic. Checked
// on entry, before the borrow operand's own endVisit reports the general rule.
bool BorrowChecker::visit(ASTBinaryExpr *element) {
  if (isBorrow(element->getLeft()) || isBorrow(element->getRight())) {
    rejectPosition(element->getLine(),
                   "cannot be used in arithmetic or relational expression");
  }
  return true;
}

bool BorrowChecker::visit(ASTOutputStmt *element) {
  if (isBorrow(element->getArg())) {
    rejectPosition(element->getLine(), "cannot be the argument of 'output'");
  }
  return true;
}

bool BorrowChecker::visit(ASTErrorStmt *element) {
  if (isBorrow(element->getArg())) {
    rejectPosition(element->getLine(), "cannot be the argument of 'error'");
  }
  return true;
}

bool BorrowChecker::visit(ASTReturnStmt *element) {
  if (isBorrow(element->getArg())) {
    rejectPosition(element->getLine(), "cannot appear in a 'return' statement");
  }
  return true;
}

void BorrowChecker::endVisit(ASTBorrowExpr *element) {
  bool approved = approvedBorrows.count(element) > 0;
  trace.push_back(
      {element->getLine(), element->getColumn(), repr(element), approved});
  if (!approved) {
    lastTrace = trace;
    rejectNotArgument(element);
  }
}

void BorrowChecker::check(ASTProgram *p) {
  SEMANTIC_LOG(1, "borrow-direct") << "start";
  BorrowChecker checker;
  p->accept(&checker);
  publish(std::move(checker.trace), std::move(checker.provenance));
  SEMANTIC_LOG(1, "borrow-direct") << "complete events=" << lastTrace.size();
}

// ===========================================================================
// checkPositions(): after summaries, borrow-derived values and aliases
// ===========================================================================

class BorrowChecker::PositionWalk {
public:
  PositionWalk(SymbolTable *sym, TypeInference *types,
               FunctionEffectSummaries *effects)
      : sym(sym), types(types), effects(effects) {}

  void checkFunction(ASTFunction *f) {
    current = f;
    aliasNames.clear();
    requirements[f->getDecl()].assign(
        f->getFormals().size(), FunctionEffectSummaries::FormalRequirement{});
    for (auto *stmt : f->getStmts()) {
      checkStmt(stmt);
    }
  }

  Requirements requirements;
  std::vector<BorrowTraceEvent> trace;
  std::vector<BorrowProvenanceEvent> provenance;

private:
  // The position an expression is evaluated in. Taken: its value is used
  // (assigned, returned, passed, placed, computed with). Argument: a direct
  // call actual, where a borrow-derived value may flow. Reborrow, Deref, and
  // Scrutinee: under `&`, under `*`, and as a `case *` scrutinee, where an
  // alias is only looked at.
  enum class Ctx {
    Taken,
    Argument,
    Reborrow,
    Deref,
    Scrutinee,
    Read,
    Iterable
  };

  struct BorrowOrigin {
    bool derived = false;
    bool hasConcreteOrigin = false;
    int line = 0;
    int column = 0;
    std::string expr;
    int hop = 0;
  };

  SymbolTable *sym;
  TypeInference *types;
  FunctionEffectSummaries *effects;
  ASTFunction *current = nullptr;
  /// Names of alias binders in scope (the arms being checked).
  std::set<std::string> aliasNames;

  void checkStmt(ASTStmt *stmt) {
    if (auto *loop = dynamic_cast<ASTForStmt *>(stmt)) {
      if (auto *input = dynamic_cast<ASTExpr *>(loop->getInput())) {
        checkExpr(input, Ctx::Iterable);
        if (dynamic_cast<TopBorrowRef *>(
                types->getInferredType(input, current->getDecl()).get())) {
          if (!dynamic_cast<ASTBorrowExpr *>(input))
            trace.push_back(
                {input->getLine(), input->getColumn(), repr(input), true});
          addProvenance(provenance,
                        {BorrowProvenanceEvent::Kind::Loop, input->getLine(),
                         input->getColumn(), repr(input), 0, loop->getLine(),
                         loop->getColumn(), repr(input), "", 0});
        }
      } else if (auto *range = dynamic_cast<ASTRange *>(loop->getInput())) {
        checkExpr(range->getStart(), Ctx::Taken);
        checkExpr(range->getEnd(), Ctx::Taken);
      }
      if (loop->getStep())
        checkExpr(loop->getStep(), Ctx::Taken);
      checkStmt(loop->getBody());
      return;
    }
    if (auto *assign = dynamic_cast<ASTAssignStmt *>(stmt)) {
      if (auto *target = dynamic_cast<ASTDeRefExpr *>(assign->getLHS())) {
        // Write-through: the *slot* is taken over, so it must be Copy.
        checkDerefTaken(target, "overwrite");
        checkExpr(target->getPtr(), Ctx::Deref);
      } else {
        checkExpr(assign->getLHS(), Ctx::Taken);
      }
      checkExpr(assign->getRHS(), Ctx::Taken, "assignment");
      return;
    }
    if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
      // `case *e` borrows; `case v` on an alias binder would consume it.
      auto *scrutinee = caseStmt->getCaseExpr();
      bool borrowed = dynamic_cast<ASTDeRefExpr *>(scrutinee) != nullptr;
      checkExpr(scrutinee, borrowed ? Ctx::Scrutinee : Ctx::Taken);
      for (auto *arm : caseStmt->getArms()) {
        // Every binding of the arm shadows an enclosing binding of the same
        // name for the arm's body. Under a borrowed scrutinee an Own-typed
        // binding names a payload the caller still owns: an alias. Binders are
        // tracked by name and arm because uses resolve by name.
        auto saved = aliasNames;
        for (auto *binding : arm->getBindings()) {
          auto type = types->getInferredType(binding);
          if (borrowed && OwnershipClassifier::classifyType(type.get()) ==
                              OwnershipClass::Own) {
            aliasNames.insert(binding->getName());
            SEMANTIC_LOG(2, "alias-check")
                << "function=" << current->getName()
                << " alias-binder=" << binding->getName()
                << " line=" << caseStmt->getLine();
          } else {
            aliasNames.erase(binding->getName());
          }
        }
        checkStmt(arm->getBody());
        aliasNames = saved;
      }
      return;
    }
    if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
      for (auto *s : block->getStmts()) {
        checkStmt(s);
      }
      return;
    }
    if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
      checkExpr(ifStmt->getCondition(), Ctx::Taken);
      checkStmt(ifStmt->getThen());
      if (ifStmt->getElse() != nullptr) {
        checkStmt(ifStmt->getElse());
      }
      return;
    }
    if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
      checkExpr(whileStmt->getCondition(), Ctx::Taken);
      checkStmt(whileStmt->getBody());
      return;
    }
    if (auto *ret = dynamic_cast<ASTReturnStmt *>(stmt)) {
      checkExpr(ret->getArg(), Ctx::Taken, "return");
      return;
    }
    // output, error: the value is taken. (A borrow-derived int operand is a
    // type error before this pass, so these are not escape sinks.)
    for (auto &child : stmt->getChildren()) {
      if (auto *e = dynamic_cast<ASTExpr *>(child.get())) {
        checkExpr(e, Ctx::Taken);
      }
    }
  }

  /* \p escapeSink names the position if a borrow-derived value may not flow
   * into it (checked after the expression's own operands). */
  void checkExpr(ASTExpr *expr, Ctx ctx, const char *escapeSink = nullptr) {
    if (dynamic_cast<ASTLambdaExpr *>(expr))
      return;
    if (auto *var = dynamic_cast<ASTVariableExpr *>(expr)) {
      if ((ctx == Ctx::Taken || ctx == Ctx::Argument) &&
          aliasNames.count(var->getName()) > 0) {
        std::ostringstream oss;
        oss << "Ownership error on line " << var->getLine() << ": '"
            << var->getName()
            << "' is bound by matching a borrowed value and can only be "
               "reborrowed (&"
            << var->getName() << ").";
        RuleToggles::reject("alias-binder", oss.str());
      }
    } else if (auto *deref = dynamic_cast<ASTDeRefExpr *>(expr)) {
      if (ctx == Ctx::Taken || ctx == Ctx::Argument) {
        checkDerefTaken(deref, "move");
      }
      checkExpr(deref->getPtr(), Ctx::Deref);
    } else if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(expr)) {
      checkExpr(borrow->getVar(), Ctx::Reborrow);
      bool approved = ctx == Ctx::Argument || ctx == Ctx::Iterable;
      trace.push_back(
          {borrow->getLine(), borrow->getColumn(), repr(borrow), approved});
      if (!approved) {
        rejectNotArgument(borrow); // LCOV_EXCL_LINE -- check() rejects it first
      }
    } else if (auto *call = dynamic_cast<ASTFunAppExpr *>(expr)) {
      checkExpr(call->getFunction(), Ctx::Taken);
      recordProvenance(call);
      for (auto *actual : call->getActuals()) {
        checkExpr(actual, Ctx::Argument);
      }
    } else if (auto *index = dynamic_cast<ASTIndexExpr *>(expr)) {
      checkExpr(index->getSequence(), Ctx::Read);
      checkExpr(index->getIndex(), Ctx::Taken);
    } else if (auto *length = dynamic_cast<ASTLengthExpr *>(expr)) {
      checkExpr(length->getSequence(), Ctx::Read);
    } else if (auto *slice = dynamic_cast<ASTSliceExpr *>(expr)) {
      checkExpr(slice->getSequence(), Ctx::Read);
      if (slice->getLowerBound())
        checkExpr(slice->getLowerBound(), Ctx::Taken);
      if (slice->getUpperBound())
        checkExpr(slice->getUpperBound(), Ctx::Taken);
    } else if (auto *sequence = dynamic_cast<ASTSequenceExpr *>(expr)) {
      for (auto *element : sequence->getElements())
        checkExpr(element, Ctx::Taken, "sequence storage");
    } else if (auto *ctor = dynamic_cast<ASTSumCtorExpr *>(expr)) {
      // A payload lives in a heap box that outlives the call.
      for (auto *payload : ctor->getArgs()) {
        checkExpr(payload, Ctx::Taken, "constructor payload");
      }
    } else {
      // Arithmetic, alloc, input: every operand is taken.
      for (auto &child : expr->getChildren()) {
        if (auto *e = dynamic_cast<ASTExpr *>(child.get())) {
          checkExpr(e, Ctx::Taken);
        }
      }
    }

    if (escapeSink != nullptr && borrowOrigin(expr).derived) {
      std::ostringstream oss;
      oss << "Borrow error on line " << expr->getLine()
          << ": borrow-derived value " << repr(expr) << " escapes into "
          << escapeSink
          << "; a borrowed alias may only flow through immediate call "
             "arguments. Return or store a copy instead.";
      RuleToggles::reject("borrow-escape", oss.str());
    }
  }

  void recordProvenance(ASTFunAppExpr *call) {
    auto actuals = call->getActuals();
    for (std::size_t index = 0; index < actuals.size(); ++index) {
      auto *actual = actuals[index];
      if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(actual)) {
        addProvenance(provenance,
                      {BorrowProvenanceEvent::Kind::Direct, borrow->getLine(),
                       borrow->getColumn(), repr(borrow), 0, call->getLine(),
                       call->getColumn(), repr(borrow), calleeName(call),
                       index});
        continue;
      }
      const auto origin = borrowOrigin(actual);
      if (origin.derived && origin.hasConcreteOrigin) {
        addProvenance(provenance,
                      {BorrowProvenanceEvent::Kind::Flow, origin.line,
                       origin.column, origin.expr, origin.hop, call->getLine(),
                       call->getColumn(), repr(actual), calleeName(call),
                       index});
      }
    }
  }

  /* Whether \p expr evaluates to a borrow: `&x`, or a call that returns (a
   * value derived from) one of its borrowed actuals. */
  BorrowOrigin borrowOrigin(ASTExpr *expr) const {
    if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(expr)) {
      return {true,         true, borrow->getLine(), borrow->getColumn(),
              repr(borrow), 0};
    }
    auto *call = dynamic_cast<ASTFunAppExpr *>(expr);
    if (!call)
      return {};
    BorrowOrigin combined;
    for (auto *target : effects->targets(call)) {
      const auto *summary = effects->get(target->getDecl());
      if (!summary ||
          (summary->returnOrigin !=
               FunctionEffectSummaries::ReturnOrigin::FromFormal &&
           summary->returnOrigin !=
               FunctionEffectSummaries::ReturnOrigin::BorrowFromFormal))
        continue;
      auto actuals = call->getActuals();
      int index = summary->returnFormalIndex;
      if (index < 0 || static_cast<std::size_t>(index) >= actuals.size())
        continue;
      auto origin = borrowOrigin(actuals[index]);
      if (origin.derived) {
        ++origin.hop;
        combined = origin;
      }
    }
    return combined;
  }

  // `*e` in a position that takes the value (or, for `what == "overwrite"`,
  // the slot `*e` being replaced). Concretely Own: reject here. Still a type
  // variable: the caller decides, so record the requirement on the formal.
  void checkDerefTaken(ASTDeRefExpr *deref, const char *what) {
    auto type = types->getInferredType(deref);
    if (dynamic_cast<const TopVar *>(type.get()) != nullptr) {
      // Through an *owning* reference the referent can never be Own (alloc
      // payloads are Copy), so `**p` reads a Copy whatever a turns out to be.
      auto ptrType = std::dynamic_pointer_cast<ReferenceType>(
          types->getInferredType(deref->getPtr(), current->getDecl()));
      auto mode =
          ptrType != nullptr
              ? std::dynamic_pointer_cast<ReferenceMode>(ptrType->getMode())
              : nullptr;
      if (mode != nullptr && mode->getMode() == ReferenceMode::Mode::Own) {
        return;
      }
      require(deref->getPtr(), deref, what);
      return;
    }
    if (OwnershipClassifier::classifyType(type.get()) != OwnershipClass::Own) {
      return;
    }
    std::ostringstream oss;
    oss << "Ownership error on line " << deref->getLine() << ": ";
    bool overwrite = std::string(what) == "overwrite";
    if (overwrite) {
      oss << "cannot overwrite the owned value '" << repr(deref)
          << "' through a borrow.";
    } else {
      oss << "'" << repr(deref)
          << "' is an owned value reached through a borrow and cannot be moved "
             "out; reborrow it with & or build a copy.";
    }
    RuleToggles::reject(overwrite ? "alias-overwrite" : "alias-move-out",
                        oss.str());
  }

  void require(ASTExpr *operand, ASTDeRefExpr *deref, const char *what) {
    auto *var = dynamic_cast<ASTVariableExpr *>(operand);
    auto *decl = var != nullptr
                     ? sym->getLocal(var->getName(), current->getDecl())
                     : nullptr;
    auto formals = current->getFormals();
    for (std::size_t i = 0; i < formals.size(); ++i) {
      if (formals[i] == decl) {
        auto &requirement = requirements[current->getDecl()][i];
        requirement.kind = FunctionEffectSummaries::CopyRequirement::Referent;
        requirement.reasons |=
            std::string(what) == "overwrite"
                ? FunctionEffectSummaries::OverwritesThroughBorrow
                : FunctionEffectSummaries::MovesOutOfBorrow;
        SEMANTIC_LOG(2, "alias-check")
            << "function=" << current->getName() << " formal=" << i
            << " requires=referent-copy reason=" << what
            << " line=" << deref->getLine();
        return;
      }
    }
    // Not a formal: nobody can decide this later, so decide now.
    std::ostringstream oss;
    oss << "Ownership error on line " << deref->getLine()
        << ": cannot tell whether '" << repr(deref)
        << "' is an owned value; dereference a formal parameter directly so "
           "the decision can be made where the function is called.";
    RuleToggles::reject("alias-undecidable", oss.str());
  }
};

BorrowChecker::Requirements
BorrowChecker::checkPositions(ASTProgram *p, SymbolTable *sym,
                              TypeInference *types, OwnershipClassifier *,
                              FunctionEffectSummaries *effects) {
  SEMANTIC_LOG(1, "borrow-positions") << "start";
  PositionWalk walk(sym, types, effects);
  for (auto *f : p->getFunctions()) {
    walk.checkFunction(f);
  }
  publish(std::move(walk.trace), std::move(walk.provenance));
  SEMANTIC_LOG(1, "borrow-positions") << "complete events=" << lastTrace.size();
  return std::move(walk.requirements);
}

// ===========================================================================
// Results
// ===========================================================================

void BorrowChecker::addProvenance(std::vector<BorrowProvenanceEvent> &events,
                                  BorrowProvenanceEvent event) {
  for (const auto &existing : events) {
    if (existing.kind == event.kind &&
        existing.originLine == event.originLine &&
        existing.originColumn == event.originColumn &&
        existing.hop == event.hop && existing.useLine == event.useLine &&
        existing.useColumn == event.useColumn &&
        existing.argumentIndex == event.argumentIndex &&
        existing.expression == event.expression &&
        existing.callee == event.callee) {
      return; // LCOV_EXCL_LINE -- defensive: one pass produces no
              // exact-duplicate event
    }
  }
  events.push_back(std::move(event));
}

void BorrowChecker::publish(std::vector<BorrowTraceEvent> trace,
                            std::vector<BorrowProvenanceEvent> provenance) {
  std::sort(provenance.begin(), provenance.end(),
            [](const auto &left, const auto &right) {
              return std::tie(left.originLine, left.originColumn, left.hop,
                              left.useLine, left.useColumn, left.argumentIndex,
                              left.callee) <
                     std::tie(right.originLine, right.originColumn, right.hop,
                              right.useLine, right.useColumn,
                              right.argumentIndex, right.callee);
            });
  lastTrace = std::move(trace);
  lastProvenance = std::move(provenance);
}

const std::vector<BorrowChecker::BorrowTraceEvent> &
BorrowChecker::getLastTrace() {
  return lastTrace;
}

const std::vector<BorrowChecker::BorrowProvenanceEvent> &
BorrowChecker::getLastProvenance() {
  return lastProvenance;
}
