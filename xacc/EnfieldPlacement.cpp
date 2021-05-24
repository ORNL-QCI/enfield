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

void optimize_gate_sequence(
    std::shared_ptr<xacc::CompositeInstruction> program) {
  auto graphView = program->toGraph();
  std::vector<int> consecutive_cnot_nodes;
  for (int i = 1; i < graphView->order() - 2; i++) {
    auto node = graphView->getVertexProperties(i);
    if (node.getString("name") == "CNOT" &&
        program->getInstruction(node.get<std::size_t>("id") - 1)->isEnabled()) {
      consecutive_cnot_nodes.emplace_back(i);

      const auto is_next_node_cnot = [&graphView, &program](size_t node_id) {
        auto nAsVec = graphView->getNeighborList(node_id);
        if (nAsVec[0] == nAsVec[1] && nAsVec[0] != graphView->order() - 1 &&
            program->getInstruction(nAsVec[0] - 1)->name() == "CNOT") {
          return true;
        }
        return false;
      };

      auto node_to_check = node.get<std::size_t>("id");
      for (int j = 0; j < 3; ++i) {
        if (is_next_node_cnot(node_to_check)) {
          assert(graphView->getNeighborList(node_to_check).size() == 2);
          assert(graphView->getNeighborList(node_to_check)[0] ==
                 graphView->getNeighborList(node_to_check)[1]);
          node_to_check = graphView->getNeighborList(node_to_check)[0];
          consecutive_cnot_nodes.emplace_back(node_to_check);
        } else {
          consecutive_cnot_nodes.clear();
          break;
        }
      }

      if (consecutive_cnot_nodes.size() == 4) {
        std::cout << "Found CNOT sequence:\n";
        for (const auto &id : consecutive_cnot_nodes) {
          std::cout << program->getInstruction(id - 1)->toString() << "\n";
        }
      }
      consecutive_cnot_nodes.clear();
    }
  }
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
  // switch the order of SWAP decompose to reduce
  // gate depth:
  // e.g. CX 1,2; CX 2,1; CX 1,2 <==> CX 2,1; CX 1,2; CX 2,1
  // this can help cancel a CX 2,1 if exists.
  optimize_gate_sequence(program);
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