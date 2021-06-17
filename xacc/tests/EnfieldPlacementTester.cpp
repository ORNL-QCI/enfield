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

TEST(EnfieldPlacementTester, checkSimple)
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

TEST(EnfieldPlacementTester, testAll)
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

// Check automatic connectivity data
TEST(EnfieldPlacementTester, checkRemote) {
  // Use a specific IBMQ backend
  auto accelerator = xacc::getAccelerator("aer:ibmq_16_melbourne");
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
                                  accelerator);
  auto program = ir->getComposites()[0];
  auto irt = xacc::getIRTransformation("enfield");
  irt->apply(program, accelerator);
  std::cout << "HOWDY:\n" << program->toString() << "\n";
}

TEST(EnfieldPlacementTester, checkHangIssue) {
  const int n_tests = 1000;
  for (int i = 0; i < n_tests; ++i) {
    auto compiler = xacc::getCompiler("staq");
    auto ir = compiler->compile(R"(
include "qelib1.inc";
 qreg q[7];
 creg q_c[7];
 h q[0];
 h q[1];
 h q[2];
 h q[3];
 h q[4];
 h q[5];
 h q[6];
 rz((-1.0000000000000000)) q[6];
 CX q[2], q[6];
 rz((-1.0000000000000000)) q[5];
 CX q[1], q[5];
 rz((-1.0000000000000000)) q[4];
 CX q[0], q[4];
 CX q[2], q[5];
 rz((-1.0000000000000000)) q[5];
 CX q[2], q[5];
 CX q[4], q[6];
 rz((-1.0000000000000000)) q[6];
 CX q[4], q[6];
 CX q[1], q[6];
 rz((-1.0000000000000000)) q[6];
 CX q[1], q[6];
 CX q[3], q[5];
 rz((-1.0000000000000000)) q[5];
 CX q[3], q[5];
 CX q[0], q[6];
 rz((-1.0000000000000000)) q[6];
 CX q[0], q[6];
 h q[0];
 rz(2.0000000000000000) q[0];
 h q[0];
 h q[1];
 rz(2.0000000000000000) q[1];
 h q[1];
 h q[2];
 rz(2.0000000000000000) q[2];
 h q[2];
 h q[3];
 rz(2.0000000000000000) q[3];
 h q[3];
 h q[4];
 rz(2.0000000000000000) q[4];
 h q[4];
 h q[5];
 rz(2.0000000000000000) q[5];
 h q[5];
 h q[6];
 rz(2.0000000000000000) q[6];
 h q[6];
)");
    const std::string arch_json =
        R"({"adj":[[{"v":"q[6]"},{"v":"q[1]"}],[{"v":"q[0]"},{"v":"q[2]"}],[{"v":"q[7]"},{"v":"q[1]"}],[{"v":"q[8]"},{"v":"q[4]"}],[{"v":"q[3]"},{"v":"q[5]"}],[{"v":"q[9]"},{"v":"q[4]"}],[{"v":"q[0]"},{"v":"q[10]"}],[{"v":"q[2]"},{"v":"q[11]"}],[{"v":"q[3]"},{"v":"q[13]"}],[{"v":"q[5]"},{"v":"q[14]"}],[{"v":"q[6]"},{"v":"q[15]"}],[{"v":"q[7]"},{"v":"q[16]"},{"v":"q[12]"}],[{"v":"q[11]"},{"v":"q[13]"}],[{"v":"q[8]"},{"v":"q[17]"},{"v":"q[12]"}],[{"v":"q[9]"},{"v":"q[18]"}],[{"v":"q[10]"},{"v":"q[19]"}],[{"v":"q[11]"},{"v":"q[21]"}],[{"v":"q[13]"},{"v":"q[22]"}],[{"v":"q[14]"},{"v":"q[24]"}],[{"v":"q[15]"},{"v":"q[25]"},{"v":"q[20]"}],[{"v":"q[19]"},{"v":"q[21]"}],[{"v":"q[16]"},{"v":"q[26]"},{"v":"q[20]"}],[{"v":"q[17]"},{"v":"q[27]"},{"v":"q[23]"}],[{"v":"q[22]"},{"v":"q[24]"}],[{"v":"q[18]"},{"v":"q[28]"},{"v":"q[23]"}],[{"v":"q[19]"},{"v":"q[29]"}],[{"v":"q[21]"},{"v":"q[30]"}],[{"v":"q[22]"},{"v":"q[32]"}],[{"v":"q[24]"},{"v":"q[33]"}],[{"v":"q[25]"}],[{"v":"q[26]"},{"v":"q[31]"}],[{"v":"q[30]"},{"v":"q[32]"}],[{"v":"q[27]"},{"v":"q[31]"}],[{"v":"q[28]"}]],"qubits":34,"registers":[{"name":"q","qubits":34}]})";
    auto program = ir->getComposites()[0];
    std::cout << "HOWDY:\n" << program->toString() << "\n";
    auto irt = xacc::getIRTransformation("enfield");
    irt->apply(program, nullptr,
               {{"allocator", "Q_sabre"}, {"arch-json", arch_json}});
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
