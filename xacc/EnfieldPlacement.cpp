#include "EnfieldPlacement.hpp"
#include "EnfieldExecutor.hpp"
#include "InstructionIterator.hpp"
#include "json.hpp"
#include "xacc.hpp"
#include <cstdio>

namespace {
size_t
getNumberQubits(const std::vector<std::pair<int, int>> &in_connectivity) {
  size_t result = 1;
  for (const auto &[q1, q2] : in_connectivity) {
    if (q1 + 1 > result) {
      result = q1 + 1;
    }
    if (q2 + 1 > result) {
      result = q2 + 1;
    }
  }

  return result;
}

std::string connectivityToArchJson(
    const std::vector<std::pair<int, int>> &in_connectivity) {
  const auto nbQubits = getNumberQubits(in_connectivity);
  nlohmann::json arch;
  arch["qubits"] = nbQubits;
  nlohmann::json qReg;
  qReg["name"] = "q";
  qReg["qubits"] = nbQubits;
  arch["registers"] = std::vector<nlohmann::json>{qReg};
  std::vector<std::vector<nlohmann::json>> adjList(nbQubits);
  for (size_t qId = 0; qId < nbQubits; ++qId) {
    auto &vertexList = adjList[qId];
    for (const auto &[q1, q2] : in_connectivity) {
      if (q1 == qId) {
        nlohmann::json vertex;
        const std::string fullName = "q[" + std::to_string(q2) + "]";
        vertex["v"] = fullName;
        vertexList.emplace_back(vertex);
      }
    }
  }
  arch["adj"] = adjList;
  return arch.dump();
}
} // namespace
namespace xacc {
namespace quantum {
void EnfieldPlacement::apply(std::shared_ptr<CompositeInstruction> program,
                             const std::shared_ptr<Accelerator> accelerator,
                             const HeterogeneousMap &options) {
  // Default allocator
  std::string allocatorName = "Q_wpm";
  if (options.stringExists("allocator")) {
    allocatorName = options.getString("allocator");
    if (!hasAllocator(allocatorName)) {
      xacc::error("Allocator named '" + allocatorName + "' does not exist.");
    }
  }

  // Archname or Json
  const bool useArchJson = (accelerator && !accelerator->getConnectivity().empty());
  const std::string archName = [&]() -> std::string {
    if (useArchJson) {
      return connectivityToArchJson(accelerator->getConnectivity());
    }
    if (options.stringExists("architecture")) {
      const auto name = options.getString("architecture");
      if (!hasArchitecture(name)) {
        xacc::error("Architecture named '" + name + "' does not exist.");
      }
      return name;
    }
    return "";
  }();

  // No architecture to do mapping.
  if (archName.empty()) {
    return;
  }
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
  std::vector<uint32_t> resultMapping;
  const auto outputCirq = runEnfield(in_fName, archName, allocatorName, resultMapping, useArchJson);
  // Use Staq to re-compile the output file.
  auto ir = staq->compile(outputCirq);
  // reset the program and add optimized instructions
  program->clear();
  program->addInstructions(ir->getComposites()[0]->getInstructions());

  // Clean-up the temporary files.
  remove(in_fName.c_str());

  const auto mappingToString = [](const std::vector<uint32_t> &in_mapping) {
    std::string s = "[";
    for (uint32_t i = 0, e = in_mapping.size(); i < e; ++i) {
      s = s + std::to_string(i) + " => ";
      s = s + std::to_string(in_mapping[i]);
      s = s + ";";
      if (i != e - 1) {
        s = s + " ";
      }
    }
    s = s + "]";
    return s;
  };
  // Write mapping to file:
  if (options.stringExists("map-file")) {
    std::ofstream mappingFile(options.getString("map-file"));
    mappingFile << mappingToString(resultMapping);
    mappingFile.close();
  }
}
} // namespace quantum
} // namespace xacc