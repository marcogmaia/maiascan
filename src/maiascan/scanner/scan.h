
#include "maiascan/scanner/process.h"

namespace maia::scanner {

class Scan {
 public:
  struct ScanMatch {
    Bytes bytes;
    MemoryAddress address;
  };

  explicit Scan(Process* process) : process_(*process) {}

  template <typename T>
  const std::vector<ScanMatch>& Find(T needle) {
    auto pages = process_.QueryPages();
    for (auto& page : pages) {
      if (auto mem = process_.ReadPage(page)) {
        // Search(, BytesView bytes)
      }
    }
  }

 private:
  Process& process_;
  std::vector<ScanMatch> scan_;
};

}  // namespace maia::scanner
