
#include "maiascan/scanner/engine.h"
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
      if (auto mem = process_.ReadPage(page); mem) {
        // TODO(marco): This transformation `ToBytesView` is dangerous, fix this.
        if (auto matches = process_.Find(ToBytesView(needle)); matches) {
          SetMatches(*matches, sizeof needle);
        }
      }
    }
    return scan_;
  }

 private:
  void SetMatches(const Matches& matches, int buffer_size) {
    ForEachMatchesAddress(matches, [this, buffer_size](MemoryAddress address) {
      Bytes buffer(buffer_size, std::byte{});
      if (process_.ReadIntoBuffer(address, buffer)) {
        scan_.emplace_back(ScanMatch{buffer, address});
      }
    });
  }

  Process& process_;
  std::vector<ScanMatch> scan_;
};

}  // namespace maia::scanner
