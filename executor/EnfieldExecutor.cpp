#include "EnfieldExecutor.hpp"
#include "enfield/Transform/Driver.h"
#include "enfield/Transform/QModule.h"
#include <iostream>

namespace xacc {
void initializeEnfield() {
  static bool initialized = false;
  if (!initialized) {
    efd::InitializeAllQbitAllocators();
    efd::InitializeAllArchitectures();
    initialized = true;
  }
}

bool hasAllocator(const std::string &in_allocatorName) {
  initializeEnfield();
  return efd::HasAllocator(in_allocatorName);
}

bool hasArchitecture(const std::string &in_archName) {
  initializeEnfield();
  return efd::HasArchitecture(in_archName);
}

std::string runEnfield(const std::string &inFilepath,
                       const std::string &archName,
                       const std::string &allocatorName,
                       std::vector<uint32_t> &out_mapping, bool in_jsonArch) {
  initializeEnfield();
  // std::cout << "In file = " << inFilepath << "\n";
  // std::cout << "Arch Name = " << archName << "\n";
  // std::cout << "Allocator Name = " << allocatorName << "\n";
  efd::QModule::uRef inputQModule = efd::ParseFile(inFilepath);
  if (inputQModule) {
    // Debug: print input circuit
    // inputQModule->print();
    if (in_jsonArch || efd::HasArchitecture(archName)) {
      if (in_jsonArch) {
        std::cout << "Json:\n" << archName << "\n";
      }
      efd::ArchGraph::sRef archGraph =
          in_jsonArch ? efd::JsonParser<efd::ArchGraph>::ParseString(archName)
                      : efd::CreateArchitecture(archName);
      efd::Mapping cacheResultMap;
      efd::CompilationSettings settings{
          archGraph, allocatorName, {{"U", 1}, {"CX", 10}}, false,
          true,      false,         &cacheResultMap};
      inputQModule.reset(
          efd::Compile(std::move(inputQModule), settings).release());
      if (inputQModule) {
        std::stringstream outputCircuit;
        inputQModule->print(outputCircuit);
        out_mapping = cacheResultMap;
        std::ofstream statOutfile;
        statOutfile.open("stat_file.dat");
        efd::PrintStats(statOutfile);
        return outputCircuit.str();
      }
    } else {
      std::cout << "Invalid Architecture '" << archName << "' encountered.\n";
    }
  }
  return "";
}
} // namespace xacc