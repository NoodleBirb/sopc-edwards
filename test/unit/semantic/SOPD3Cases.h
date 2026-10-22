#pragma once

#include <string>
#include <vector>

namespace sopd3 {

struct Local {
  std::string function;
  std::string name;
  // Empty leaves a polymorphic type unconstrained; classification is checked.
  std::string type;
  bool own;
};

struct Destruction {
  std::string function;
  std::vector<std::string> targets;
};

struct Move {
  std::string kind;
  std::string variable;
};

struct Targets {
  std::string calleeExpression;
  std::size_t count;
  int destroysPerTarget = -1;
};

struct Case {
  std::string group;
  std::string name;
  std::string requirements;
  std::string source;
  // A nonempty list denotes rejection; every fragment must match.
  std::vector<std::string> diagnostic;
  std::vector<Local> locals;
  std::vector<Destruction> destructions;
  std::vector<Move> moves;
  std::vector<std::string> neverMoved;
  bool loopCFG = false;
  std::vector<Targets> callTargets;
};

const std::vector<Case> &cases();

} // namespace sopd3
