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

void remove_cx_pairs(std::shared_ptr<xacc::CompositeInstruction> program) {
  // std::cout << "Before: \n" << program->toString() << "\n";
  auto graphView = program->toGraph();
  for (int i = 1; i < graphView->order() - 2; i++) {
    auto node = graphView->getVertexProperties(i);
    if (node.getString("name") == "CNOT" &&
        program->getInstruction(node.get<std::size_t>("id") - 1)->isEnabled()) {
      auto nAsVec = graphView->getNeighborList(node.get<std::size_t>("id"));
      // std::vector<int> nAsVec(neighbors.begin(), neighbors.end());
      // Note: There is an edge-case if the CNOT is last gate on a pair of qubit
      // wires, i.e. both of its neighbors will be the final state node. In that
      // case, we need to skip.
      if (nAsVec[0] == nAsVec[1] && nAsVec[0] != graphView->order() - 1) {
        // Check that the neighbor gate is indeed a CNOT gate, i.e. not a
        // different 2-qubit gate.
        if (program->getInstruction(nAsVec[0] - 1)->name() == "CNOT" &&
            program->getInstruction(nAsVec[0] - 1)->bits() ==
                program->getInstruction(node.get<std::size_t>("id") - 1)
                    ->bits()) {
          // std::cout << "Cancel:\n";
          // std::cout << program->getInstruction(node.get<std::size_t>("id") - 1)->toString() << "\n";
          // std::cout << program->getInstruction(nAsVec[0] - 1)->toString() << "\n";
          program->getInstruction(node.get<std::size_t>("id") - 1)->disable();
          program->getInstruction(nAsVec[0] - 1)->disable();
        }
      }
    }
  }
  program->removeDisabled();
}

void move_qaoa_evol_to_swap(
    std::shared_ptr<xacc::CompositeInstruction> program) {
  std::vector<int> consecutive_cnot_nodes;
  for (int i = 1; i < program->toGraph()->order() - 2; i++) {
    auto graphView = program->toGraph();
    auto node = graphView->getVertexProperties(i);
    if (node.getString("name") == "CNOT" &&
        program->getInstruction(node.get<std::size_t>("id") - 1)->isEnabled()) {
      consecutive_cnot_nodes.emplace_back(i);

      const auto is_next_node_cnot = [&graphView, &program](size_t node_id) {
        auto nAsVec = graphView->getNeighborList(node_id);
        // std::cout << "Neighbor: " << program->getInstruction(nAsVec[0] - 1)->toString() << "\n";
        if (nAsVec[0] == nAsVec[1] && nAsVec[0] != graphView->order() - 1 &&
            program->getInstruction(nAsVec[0] - 1)->name() == "CNOT") {
          // std::cout << "Is CNOT...\n";
          return true;
        }
        return false;
      };
      // std::cout << "CNOT: "
      //           << program->getInstruction(node.get<std::size_t>("id") - 1)
      //                  ->toString()
      //           << "\n";
      auto node_to_check = node.get<std::size_t>("id");
      for (int j = 0; j < 3; ++j) {
        if (is_next_node_cnot(node_to_check)) {
          assert(graphView->getNeighborList(node_to_check).size() == 2);
          assert(graphView->getNeighborList(node_to_check)[0] ==
                 graphView->getNeighborList(node_to_check)[1]);
          node_to_check = graphView->getNeighborList(node_to_check)[0];
          consecutive_cnot_nodes.emplace_back(node_to_check);
        } else {
          break;
        }
      }

      if (consecutive_cnot_nodes.size() == 3) {
        // std::cout << "Found CNOT sequence:\n";
        auto cx = program->getInstruction(consecutive_cnot_nodes[0] - 1);
        auto last_swap_cx = program->getInstruction(consecutive_cnot_nodes[2] - 1);
        const std::pair<size_t, size_t> swap_qubits =
            std::make_pair(cx->bits()[0], cx->bits()[1]);
        // for (const auto &id : consecutive_cnot_nodes) {
        //   std::cout << program->getInstruction(id - 1)->toString() << "\n";
        // }
        
        // Collect the sequence of gates from a previos CNOT to this swap:
        std::vector<size_t> cx_pair_inst_idxs;
        for (int inst_id = 0; inst_id < consecutive_cnot_nodes[0]; ++inst_id) {
          auto inst_ptr = program->getInstruction(inst_id);
          if (inst_ptr->name() == "CNOT" &&
              inst_ptr->bits()[0] == swap_qubits.second &&
              inst_ptr->bits()[1] == swap_qubits.first) {
            // std::cout << "Found instruction: " << inst_ptr->toString() << "\n";
            cx_pair_inst_idxs.emplace_back(inst_id);
          }
        }

        if (cx_pair_inst_idxs.size() == 2) {
          // std::vector<std::shared_ptr<xacc::Instruction>> to_swap_inst;
          for (int test_node = cx_pair_inst_idxs[1] + 1;
               test_node < consecutive_cnot_nodes[0]; ++test_node) {
            auto test_inst = program->getInstruction(test_node - 1);
            if (test_inst->bits().size() == 1 &&
                test_inst->bits()[0] == swap_qubits.second) {
              // std::cout << "TO SWAP: " << test_inst->toString() << "\n";
              // to_swap_inst.emplace_back(test_inst->clone());
              // test_inst->disable();
              auto new_inst = test_inst->clone();
              new_inst->setBits({swap_qubits.first});
              program->insertInstruction(consecutive_cnot_nodes[2], new_inst);
              test_inst->disable();
            }
          }

          // last_swap_cx->disable();
          auto first_cx_node = cx_pair_inst_idxs[0] + 1;
          auto neighbor_gates = program->toGraph()->getNeighborList(first_cx_node);
          assert(neighbor_gates.size() == 2);
          assert(neighbor_gates[0] == cx_pair_inst_idxs[1] + 1 ||
                 neighbor_gates[1] == cx_pair_inst_idxs[1] + 1);
          if (neighbor_gates[0] == cx_pair_inst_idxs[1] + 1) {
            auto rot_gate_id = neighbor_gates[1];
            // std::cout << "Internal gate: " << program->getInstruction(rot_gate_id - 1)->toString() << "\n";
            auto first_cx = program->getInstruction(cx_pair_inst_idxs[0])->clone();
            auto second_cx = program->getInstruction(cx_pair_inst_idxs[1])->clone();
            auto rot_gate = program->getInstruction(rot_gate_id - 1)->clone();
            assert(first_cx->name() == "CNOT");
            assert(second_cx->name() == "CNOT");
            assert(first_cx->bits() == second_cx->bits());
            program->getInstruction(cx_pair_inst_idxs[0])->disable();
            program->getInstruction(cx_pair_inst_idxs[1])->disable();
            program->getInstruction(rot_gate_id - 1)->disable();
            // Move to next to the swap:
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, first_cx); 
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, rot_gate); 
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, second_cx); 
          }
          else {
            auto rot_gate_id = neighbor_gates[0];
            // std::cout << "Internal gate: " << program->getInstruction(rot_gate_id - 1)->toString() << "\n";
            auto first_cx = program->getInstruction(cx_pair_inst_idxs[0])->clone();
            auto second_cx = program->getInstruction(cx_pair_inst_idxs[1])->clone();
            auto rot_gate = program->getInstruction(rot_gate_id - 1)->clone();
            assert(first_cx->name() == "CNOT");
            assert(second_cx->name() == "CNOT");
            assert(first_cx->bits() == second_cx->bits());
            program->getInstruction(cx_pair_inst_idxs[0])->disable();
            program->getInstruction(cx_pair_inst_idxs[1])->disable();
            program->getInstruction(rot_gate_id - 1)->disable();
            // Move to next to the swap:
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, first_cx); 
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, rot_gate); 
            program->insertInstruction(consecutive_cnot_nodes[0] - 1, second_cx); 
          }
        }
      }
      consecutive_cnot_nodes.clear();
    }
  }
  program->removeDisabled();
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
  // std::cout << outputCirq << "\n";
  // Use Staq to re-compile the output file.
  auto ir = staq->compile(outputCirq);
  // reset the program and add optimized instructions
  program->clear();
  program->addInstructions(ir->getComposites()[0]->getInstructions());
  // switch the order of SWAP decompose to reduce
  // gate depth:
  // e.g. CX 1,2; CX 2,1; CX 1,2 <==> CX 2,1; CX 1,2; CX 2,1
  // this can help cancel a CX 2,1 if exists.
  if (options.keyExists<bool>("qaoa-swap")) {
    move_qaoa_evol_to_swap(program);
  }
  if (options.keyExists<bool>("cx-opt")) {
    remove_cx_pairs(program);
  }

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