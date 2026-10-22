#include "Unifier.h"

#include "FreshAlphaCopier.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopCons.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "TopSequence.h"
#include "TopSumType.h"
#include "UnificationError.h"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

bool Unifier::isCons(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopCons>(type) != nullptr;
}

bool Unifier::isMu(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopMu>(type) != nullptr;
}

bool Unifier::isVar(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopVar>(type) != nullptr;
}

bool Unifier::isAlpha(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopAlpha>(type) != nullptr;
}

bool Unifier::isProperType(std::shared_ptr<TopType> type) {
  return isCons(type) || isMu(type) || isAlpha(type);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Unifier::Unifier() = default;

Unifier::Unifier(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TopTermAdapter::wrap(c.lhs),
                              TopTermAdapter::wrap(c.rhs));
  }
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

void Unifier::add(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TopTermAdapter::wrap(c.lhs),
                              TopTermAdapter::wrap(c.rhs));
  }
}

void Unifier::solve() {
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
  bool changed;
  do {
    changed = false;
    for (const auto &constraint : iterations) {
      auto element = inferred(constraint.element);
      auto binder = inferred(constraint.binder);
      const auto before = element->toString() + "|" + binder->toString();
      bool known = !isVar(element);
      bool own = dynamic_cast<TopSequence *>(element.get()) ||
                 dynamic_cast<TopSumType *>(element.get()) || isMu(element);
      if (auto *ref = dynamic_cast<ReferenceType *>(element.get())) {
        auto mode = std::dynamic_pointer_cast<ReferenceMode>(ref->getMode());
        known = mode != nullptr;
        own = mode && mode->getMode() == ReferenceMode::Mode::Own;
      }
      if (known) {
        auto expected =
            own ? std::shared_ptr<TopType>(
                      std::make_shared<TopBorrowRef>(constraint.element))
                : constraint.element;
        unify(constraint.binder, expected);
      } else if (isVar(element) && !isVar(binder)) {
        if (auto *ref = dynamic_cast<ReferenceType *>(binder.get())) {
          auto raw = TopTermAdapter::unwrap(
              termUnifier.find(TopTermAdapter::wrap(constraint.binder)));
          auto payload =
              dynamic_cast<ReferenceType *>(raw.get())->getReferencedType();
          unify(constraint.element, payload);
          unify(constraint.binder, std::make_shared<TopBorrowRef>(payload));
        } else if (dynamic_cast<TopSequence *>(binder.get()) ||
                   dynamic_cast<TopSumType *>(binder.get()) || isMu(binder)) {
          throw UnificationError("Cannot unify an owning iteration binder with "
                                 "Copy borrowed access");
        } else
          unify(constraint.element, constraint.binder);
      }
      const auto after = inferred(constraint.element)->toString() + "|" +
                         inferred(constraint.binder)->toString();
      changed = changed || before != after;
    }
  } while (changed);
}

void Unifier::addIterations(
    const std::vector<IterationConstraint> &constraints) {
  iterations.insert(iterations.end(), constraints.begin(), constraints.end());
}
void Unifier::rememberIterations(
    ASTDeclNode *function,
    const std::vector<IterationConstraint> &constraints) {
  iterationSchemes[function] = constraints;
}
std::vector<IterationConstraint> Unifier::iterationsFor(ASTDeclNode *function) {
  std::vector<IterationConstraint> result;
  for (const auto &constraint : iterationSchemes[function])
    result.push_back({inferred(constraint.element), inferred(constraint.binder),
                      constraint.loop});
  return result;
}
std::vector<IterationConstraint> Unifier::copyIterations(ASTDeclNode *function,
                                                         ASTNode *context) {
  auto result = iterationsFor(function);
  for (auto &constraint : result) {
    constraint.element =
        FreshAlphaCopier::copy(constraint.element.get(), context);
    constraint.binder =
        FreshAlphaCopier::copy(constraint.binder.get(), context);
  }
  return result;
}

/**
 * Unify two types immediately (on-the-fly).
 *
 * Mirrors Unifier::unify(): adds one constraint and solves it right away so
 * that subsequent calls to inferred() see the updated union-find state.
 *
 * Translates TermUnificationError → UnificationError so callers that catch
 * UnificationError continue to work without changes.
 */
void Unifier::unify(std::shared_ptr<TopType> t1, std::shared_ptr<TopType> t2) {
  termUnifier.addConstraint(TopTermAdapter::wrap(t1), TopTermAdapter::wrap(t2));
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
}

// ---------------------------------------------------------------------------
// Inference
// ---------------------------------------------------------------------------

std::shared_ptr<TopType> Unifier::inferred(std::shared_ptr<TopType> t) {
  return TopTypeClosure(termUnifier).close(t);
}
