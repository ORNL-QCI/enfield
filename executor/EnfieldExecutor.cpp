#include "EnfieldExecutor.hpp"
#include <iostream>
// #include "enfield/Transform/QModule.h"
// #include "enfield/Transform/Driver.h"

namespace xacc {
void runEnfield(const std::string &inFilepath, const std::string &outFilepath,
                const std::string &archName, const std::string &allocatorName) {
  std::cout << "In file = " << inFilepath << "\n";
  std::cout << "Out file = " << outFilepath << "\n";
  std::cout << "Arch Name = " << archName << "\n";
  std::cout << "Allocator Name = " << allocatorName << "\n";
//   efd::QModule::uRef qmod = efd::ParseFile(inFilepath);

}
} // namespace xacc