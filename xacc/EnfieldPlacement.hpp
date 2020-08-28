#pragma once
#include "IRTransformation.hpp"
#include "OptionsProvider.hpp"

namespace xacc {
namespace quantum {
class EnfieldPlacement : public IRTransformation, public OptionsProvider {

public:
  EnfieldPlacement() {}
  void apply(std::shared_ptr<CompositeInstruction> program,
                     const std::shared_ptr<Accelerator> accelerator,
                     const HeterogeneousMap& options = {}) override {};
  const IRTransformationType type() const override {return IRTransformationType::Placement;}

  const std::string name() const override { return "enfield"; }
  const std::string description() const override { return ""; }
};
} // namespace quantum
} // namespace xacc
