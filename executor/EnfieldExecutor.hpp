#include <string>
#include <vector>

namespace xacc {
void initializeEnfield();
bool hasAllocator(const std::string &in_allocatorName);
bool hasArchitecture(const std::string &in_archName);
std::string runEnfield(const std::string &inFilepath,
                       const std::string &archName,
                       const std::string &allocatorName,
                       std::vector<uint32_t> &out_mapping,
                       bool in_jsonArch = false);
} // namespace xacc
