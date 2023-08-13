#pragma once

#include <string>
#include <vector>

#include "maiascan/scanner/types.h"

namespace maia {

struct Process {
  std::string name;
  Pid pid;
};

std::vector<Process> GetProcs();
void ListProcs();

}  // namespace maia
