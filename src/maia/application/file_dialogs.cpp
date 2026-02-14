// Copyright (c) Maia

#include "maia/application/file_dialogs.h"

#include <nfd.h>
#include <string>
#include <vector>

#include "maia/logging.h"

namespace maia::application {

namespace {

// -----------------------------------------------------------------------------
// Internal RAII Helpers
// -----------------------------------------------------------------------------

// Manages NFD initialization/shutdown for the current thread.
struct NfdManager {
  NfdManager() {
    // NFD_Init must be called for every thread that uses NFD
    if (NFD_Init() != NFD_OKAY) {
      LogError("Failed to initialize NFD on thread: {}", NFD_GetError());
    }
  }

  ~NfdManager() {
    NFD_Quit();
  }

  // Non-copyable/movable
  NfdManager(const NfdManager&) = delete;
  NfdManager& operator=(const NfdManager&) = delete;
};

// Ensures the path pointer returned by NFD is freed.
struct NfdPathGuard {
  nfdu8char_t* ptr;

  explicit NfdPathGuard(nfdu8char_t* p)
      : ptr(p) {}

  ~NfdPathGuard() {
    if (ptr) {
      NFD_FreePathU8(ptr);
    }
  }

  NfdPathGuard(const NfdPathGuard&) = delete;
  NfdPathGuard& operator=(const NfdPathGuard&) = delete;
};

// Handles lifetime of filter strings and the C-struct array.
struct NfdFilterGuard {
  std::vector<std::string> string_storage;
  std::vector<nfdu8filteritem_t> items;

  explicit NfdFilterGuard(std::span<const FileFilter> filters) {
    if (filters.empty()) {
      return;
    }

    // 1. Store all strings first to ensure stable memory addresses
    string_storage.reserve(filters.size() * 2);
    for (const auto& f : filters) {
      string_storage.emplace_back(f.name);
      string_storage.emplace_back(f.spec);
    }

    // 2. Point C-structs to the stable strings
    items.reserve(filters.size());
    for (size_t i = 0; i < filters.size(); ++i) {
      items.push_back(
          nfdu8filteritem_t{.name = string_storage[i * 2].c_str(),
                            .spec = string_storage[i * 2 + 1].c_str()});
    }
  }

  [[nodiscard]] const nfdu8filteritem_t* data() const {
    return items.empty() ? nullptr : items.data();
  }

  [[nodiscard]] nfdfiltersize_t size() const {
    return static_cast<nfdfiltersize_t>(items.size());
  }
};

// -----------------------------------------------------------------------------
// Utility Functions
// -----------------------------------------------------------------------------

void EnsureNfdInitialized() {
  // CRITICAL FIX: thread_local ensures Init/Quit is called
  // once per thread, not once per process.
  thread_local NfdManager manager;
  (void)manager;  // Suppress unused variable warning
}

std::filesystem::path ToPath(const nfdu8char_t* utf8_ptr) {
  // Safely construct a path from a UTF-8 string view (C++20)
  if (!utf8_ptr) {
    return {};
  }
  return std::filesystem::path(
      std::u8string_view(reinterpret_cast<const char8_t*>(utf8_ptr)));
}

std::string ToUtf8String(const std::filesystem::path& path) {
  // Portable conversion from path to UTF-8 std::string
  const auto u8str = path.u8string();
  return std::string(u8str.begin(), u8str.end());
}

}  // namespace

// -----------------------------------------------------------------------------
// Public Implementation
// -----------------------------------------------------------------------------

std::optional<std::filesystem::path> FileDialogs::ShowOpenDialog(
    std::span<const FileFilter> filters,
    const std::optional<std::filesystem::path>& default_path) {
  EnsureNfdInitialized();
  NfdFilterGuard nfd_filters(filters);

  std::string default_path_u8;
  const char* default_path_ptr = nullptr;

  if (default_path) {
    default_path_u8 = ToUtf8String(*default_path);
    default_path_ptr = default_path_u8.c_str();
  }

  nfdu8char_t* out_path = nullptr;
  const nfdresult_t result = NFD_OpenDialogU8(
      &out_path, nfd_filters.data(), nfd_filters.size(), default_path_ptr);

  if (result == NFD_OKAY) {
    NfdPathGuard guard(out_path);
    return ToPath(out_path);
  }

  if (result == NFD_ERROR) {
    LogError("NFD Open Error: {}", NFD_GetError());
  }

  return std::nullopt;
}

std::optional<std::filesystem::path> FileDialogs::ShowSaveDialog(
    std::span<const FileFilter> filters,
    const std::optional<std::filesystem::path>& default_path,
    const std::optional<std::string>& default_name) {
  EnsureNfdInitialized();
  NfdFilterGuard nfd_filters(filters);

  std::string default_path_u8;
  const char* default_path_ptr = nullptr;

  if (default_path) {
    default_path_u8 = ToUtf8String(*default_path);
    default_path_ptr = default_path_u8.c_str();
  }

  const char* default_name_ptr = default_name ? default_name->c_str() : nullptr;

  nfdu8char_t* out_path = nullptr;
  const nfdresult_t result = NFD_SaveDialogU8(&out_path,
                                              nfd_filters.data(),
                                              nfd_filters.size(),
                                              default_path_ptr,
                                              default_name_ptr);

  if (result == NFD_OKAY) {
    NfdPathGuard guard(out_path);
    return ToPath(out_path);
  }

  if (result == NFD_ERROR) {
    LogError("NFD Save Error: {}", NFD_GetError());
  }

  return std::nullopt;
}

}  // namespace maia::application
