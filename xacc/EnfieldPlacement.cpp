#include "EnfieldPlacement.hpp"
#include "EnfieldExecutor.hpp"
#include "InstructionIterator.hpp"
#include "xacc.hpp"
#include <cstdio>
#include <filesystem>

namespace xacc {
namespace quantum {
void EnfieldPlacement::apply(std::shared_ptr<CompositeInstruction> program,
                             const std::shared_ptr<Accelerator> accelerator,
                             const HeterogeneousMap &options) {
  // (1) Use Staq to translate the program to OpenQASM
  auto staq = xacc::getCompiler("staq");
  const auto src = staq->translate(program);

  // (2) Write OpenQASM to temp file
  char inTemplate[] = "/tmp/inQasmFileXXXXXX";

  if (mkstemp(inTemplate) == -1) {
    xacc::error("Failed to create temporary files.");
  }

  const std::string in_fName(inTemplate);
  std::ofstream inFile(in_fName);
  inFile << src;
  inFile.close();
  const auto outputCirq = runEnfield(in_fName, "A_ibmqx2", "Q_wpm");
  // Use Staq to re-compile the output file.
  auto ir = staq->compile(outputCirq);
  // reset the program and add optimized instructions
  program->clear();
  program->addInstructions(ir->getComposites()[0]->getInstructions());

  // Clean-up the temporary files.
  remove(in_fName.c_str());
}
} // namespace quantum
} // namespace xacc