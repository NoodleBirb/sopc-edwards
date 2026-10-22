#pragma once

#include "IterationConstraint.h"
#include "TermUnifier.h"
#include "TopTermAdapter.h"
#include "TopType.h"
#include "TopTypeClosure.h"
#include "TopVar.h"
#include "TypeConstraint.h"
#include <map>
class ASTDeclNode;
#include <memory>
#include <vector>

/*!
 * \class Unifier
 *
 * \brief Facade over TermUnifier that provides the type-inference public API.
 *
 * Unifier delegates to TermUnifier (backed by TermUnionFind) internally.
 * Static helper predicates (isCons, isMu, isVar, isAlpha, isProperType)
 * classify TopType values for callers that need to inspect inference results.
 */
class Unifier {
public:
  Unifier();
  explicit Unifier(std::vector<TypeConstraint> constraints);
  ~Unifier() = default;

  /*!
   * \brief Attempt to unify two types immediately (on-the-fly mode).
   * \throws UnificationError when constraints cannot be unified.
   *
   * Wraps the constraint in a TermUnifier call and solves it inline,
   * mirroring Unifier::unify().
   */
  void unify(std::shared_ptr<TopType> t1, std::shared_ptr<TopType> t2);

  /*! \brief Add constraints to this bridge (mirrors Unifier::add()). */
  void add(std::vector<TypeConstraint> constraints);
  void addIterations(const std::vector<IterationConstraint> &constraints);
  void rememberIterations(ASTDeclNode *function,
                          const std::vector<IterationConstraint> &constraints);
  std::vector<IterationConstraint> iterationsFor(ASTDeclNode *function);
  std::vector<IterationConstraint> copyIterations(ASTDeclNode *function,
                                                  ASTNode *context);

  /*!
   * \brief Solve all pending constraints (batch mode).
   * \pre addConstraint calls have been made or constructor seeded with
   *      constraints.
   */
  void solve();

  /*!
   * \brief Return the inferred type for a given type variable.
   *
   * Closes the type by replacing all bound variables with their inferred
   * proper types, producing TopMu for recursive types.
   */
  std::shared_ptr<TopType> inferred(std::shared_ptr<TopType> t);

  static bool isCons(std::shared_ptr<TopType> type);
  static bool isMu(std::shared_ptr<TopType> type);
  static bool isVar(std::shared_ptr<TopType> type);
  static bool isAlpha(std::shared_ptr<TopType> type);
  static bool isProperType(std::shared_ptr<TopType> type);

private:
  TermUnifier termUnifier;
  std::vector<IterationConstraint> iterations;
  std::map<ASTDeclNode *, std::vector<IterationConstraint>> iterationSchemes;
};
