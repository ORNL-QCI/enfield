#include "EnfieldPlacement.hpp"
#include "xacc.hpp"
#include "InstructionIterator.hpp"
#include <cstdio>
#include <filesystem>
#include "EnfieldExecutor.hpp"

namespace xacc
{
  namespace quantum
  {
    void EnfieldPlacement::apply(std::shared_ptr<CompositeInstruction> program,
                                 const std::shared_ptr<Accelerator> accelerator,
                                 const HeterogeneousMap &options)
    {
      // (1) Use Staq to translate the program to OpenQASM
      auto staq = xacc::getCompiler("staq");
      const auto src = staq->translate(program);

      // (2) Write OpenQASM to temp file
      char inTemplate[] = "/tmp/inQasmFileXXXXXX";
      char outTemplate[] = "/tmp/outQasmFileXXXXXX";

      if (mkstemp(inTemplate) == -1 || mkstemp(outTemplate) == -1)
      {
        xacc::error("Failed to create temporary files.");
      }

      const std::string in_fName(inTemplate);
      const std::string out_fName(outTemplate);
      std::cout << "Input filename: " << in_fName << "\n";
      std::ofstream inFile(in_fName);
      inFile << src;
      inFile.close();
      runEnfield(in_fName, out_fName, "A_ibmqx2", "Q_wpm");
    }
  } // namespace quantum
} // namespace xacc