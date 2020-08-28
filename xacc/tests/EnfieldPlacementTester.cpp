#include <memory>
#include <gtest/gtest.h>
#include "xacc.hpp"
#include "xacc_service.hpp"

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

int main(int argc, char **argv)
{
    xacc::Initialize(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    auto ret = RUN_ALL_TESTS();
    xacc::Finalize();
    return ret;
}
