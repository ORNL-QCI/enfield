#include <string>

namespace xacc {
void initializeEnfield();
bool hasAllocator(const std::string& in_allocatorName);
bool hasArchitecture(const std::string& in_archName);
std::string runEnfield(const std::string &inFilepath, const std::string &archName,
                const std::string &allocatorName);
}
