#pragma once
#include "TopCons.h"
#include <stdexcept>

class TopSequence : public TopCons {
public:
  explicit TopSequence(std::shared_ptr<TopType> element)
      : TopCons({std::move(element)}) {}
  std::shared_ptr<TopType> getElementType() const { return arguments[0]; }
  std::string getFunctor() const override { return "Seq"; }
  bool operator==(const TopType &other) const override {
    auto *seq = dynamic_cast<const TopSequence *>(&other);
    return seq && *getElementType() == *seq->getElementType();
  }
  std::ostream &print(std::ostream &out) const override {
    return out << "Seq(" << *getElementType() << ")";
  }
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override {
    if (children.size() != 1)
      throw std::invalid_argument("Seq needs one element type");
    return std::make_shared<TopSequence>(children[0]);
  }
  void accept(TopTypeVisitor *visitor) override;
};
