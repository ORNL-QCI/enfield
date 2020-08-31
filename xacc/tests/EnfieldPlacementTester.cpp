#include <memory>
#include <gtest/gtest.h>
#include "xacc.hpp"
#include "xacc_service.hpp"

namespace {
// List of all available allocators (placement mapping)
const std::vector<std::string> ALLOCATORS = {"Q_dynprog",  "Q_grdy",
                                             "Q_bmt",      "Q_simplified_bmt",
                                             "Q_ibmt",     "Q_simplified_ibmt",
                                             "Q_opt_bmt",  "Q_layered_bmt",
                                             "Q_ibm",      "Q_wpm",
                                             "Q_random",   "Q_qubiter",
                                             "Q_wqubiter", "Q_jku",
                                             "Q_sabre",    "Q_chw"};
} // namespace

TEST(TriQPlacementTester, checkSimple)
{
    // Allocate some qubits
    auto buffer = xacc::qalloc(3);
    auto xasmCompiler = xacc::getCompiler("xasm");
    auto ir = xasmCompiler->compile(R"(__qpu__ void bell(qbit q) {
      H(q[0]);
      CX(q[0], q[1]);
      CX(q[0], q[2]);
      Measure(q[0]);
      Measure(q[1]);
      Measure(q[2]);
})",
                                    nullptr);
    auto program = ir->getComposites()[0];
    auto irt = xacc::getIRTransformation("enfield");
    irt->apply(program, nullptr);
    std::cout << "HOWDY:\n" << program->toString() << "\n";
}

TEST(TriQPlacementTester, testAll)
{
  const std::vector<std::string> TEST_FILES = {"011_3_qubit_grover_50_.qasm",
                                               "inverseqft2.qasm",
                                               "prog_1.qasm",
                                               "prog_4.qasm",
                                               "qft.qasm",
                                               "teleport.qasm",
                                               "7x1mod15.qasm",
                                               "adder_4.qasm",
                                               "ipea_3_pi_8.qasm",
                                               "prog_2.qasm",
                                               "qec.qasm",
                                               "qpt.qasm",
                                               "W-state.qasm",
                                               "inverseqft1.qasm",
                                               "pea_3_pi_8.qasm",
                                               "prog_3.qasm",
                                               "rb.qasm"};
  // Run all the test cases
  for (const auto &testFile : TEST_FILES) {
    // Test all Enfield allocators:
    for (const auto &mapper : ALLOCATORS) {
      auto buffer = xacc::qalloc(5);
      auto compiler = xacc::getCompiler("staq");
      const std::string QASM_SRC_FILE =
          std::string(RESOURCE_DIR) + "/" + testFile;
      std::cout << "Test: " << testFile << " using " << mapper << "\n";
      // Read source file:
      std::ifstream inFile;
      inFile.open(QASM_SRC_FILE);
      std::stringstream strStream;
      strStream << inFile.rdbuf();
      const std::string qasmSrcStr = strStream.str();

      // Compile:
      auto IR = compiler->compile(qasmSrcStr);
      // BEFORE
      auto program = IR->getComposites()[0];
      std::cout << "BEFORE:\n" << program->nInstructions() << "\n";

      auto irt = xacc::getIRTransformation("enfield");
      irt->apply(program, nullptr,
                 {{"architecture", "A_ibmqx2"}, {"allocator", mapper}});
      std::cout << "AFTER:\n" << program->nInstructions() << "\n";
      EXPECT_TRUE(program->nInstructions() > 0);
    }
  }
}

int main(int argc, char **argv)
{
    xacc::Initialize(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    auto ret = RUN_ALL_TESTS();
    xacc::Finalize();
    return ret;
}
