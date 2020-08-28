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

bool hasAllocator(const std::string& in_allocatorName) {
  initializeEnfield();
  return efd::HasAllocator(in_allocatorName);
}

bool hasArchitecture(const std::string& in_archName) {
  initializeEnfield();
  return efd::HasArchitecture(in_archName);
}

std::string runEnfield(const std::string &inFilepath,
                       const std::string &archName,
                       const std::string &allocatorName) {
  initializeEnfield();
  std::cout << "In file = " << inFilepath << "\n";
  std::cout << "Arch Name = " << archName << "\n";
  std::cout << "Allocator Name = " << allocatorName << "\n";
  efd::QModule::uRef inputQModule = efd::ParseFile(inFilepath);
  if (inputQModule) {
    // Debug: print input circuit
    inputQModule->print();
    if (efd::HasArchitecture(archName)) {
      efd::ArchGraph::sRef archGraph = efd::CreateArchitecture(archName);
      efd::CompilationSettings settings{
          archGraph, allocatorName, {{"U", 1}, {"CX", 10}}, false, true, false};
      inputQModule.reset(
          efd::Compile(std::move(inputQModule), settings).release());
      if (inputQModule) {
        std::stringstream outputCircuit;
        inputQModule->print(outputCircuit);
        return outputCircuit.str();
      }
    } else {
      std::cout << "Invalid Architecture '" << archName << "' encountered.\n";
    }
  }
  return "";
}
} // namespace xacc