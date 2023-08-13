#pragma once

#include <span>
#include <string>
#include <vector>

#include "maiascan/scanner/types.h"

namespace maia {

std::vector<ProcessData> GetProcs();

void ListProcs();

}  // namespace maia
