// Copyright (c) Maia

#include "maia/application/file_dialogs.h"

#include <nfd.h>
#include <string>
#include <vector>

#include "maia/logging.h"

namespace maia::application {

namespace {

struct NfdManager {
  NfdManager() {
    if (NFD_Init() != NFD_OKAY) {
      LogError("Failed to initialize NFD: {}", NFD_GetError());
    }
  }

  ~NfdManager() {
    NFD_Quit();
  }

  NfdManager(const NfdManager&) = delete;
  NfdManager& operator=(const NfdManager&) = delete;
};

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

struct NfdFilterGuard {
  std::vector<std::string> string_storage;
  std::vector<nfdu8filteritem_t> items;

  explicit NfdFilterGuard(std::span<const FileFilter> filters) {
    if (filters.empty()) {
      return;
    }

    string_storage.reserve(filters.size() * 2);
    for (const auto& f : filters) {
      string_storage.emplace_back(f.name);
      string_storage.emplace_back(f.spec);
    }

    items.reserve(filters.size());
    for (size_t i = 0; i < filters.size(); ++i) {
      items.push_back(
          nfdu8filteritem_t{.name = reinterpret_cast<const nfdu8char_t*>(
                                string_storage[i * 2].c_str()),
                            .spec = reinterpret_cast<const nfdu8char_t*>(
                                string_storage[i * 2 + 1].c_str())});
    }
  }

  [[nodiscard]] const nfdu8filteritem_t* data() const {
    return items.empty() ? nullptr : items.data();
  }

  [[nodiscard]] nfdfiltersize_t size() const {
    return static_cast<nfdfiltersize_t>(items.size());
  }
};

void EnsureNfdInitialized() {
  // Use thread_local if dialogs are truly spawned from multiple threads.
  // Be aware that native UI dialogs often require the main thread.
  [[maybe_unused]] thread_local NfdManager manager;
}

std::filesystem::path ToPath(const nfdu8char_t* utf8_ptr) {
  if (!utf8_ptr) {
    return {};
  }
  return std::filesystem::path(
      std::u8string_view(reinterpret_cast<const char8_t*>(utf8_ptr)));
}

std::string ToUtf8String(const std::filesystem::path& path) {
  const auto u8str = path.u8string();
  return std::string(u8str.begin(), u8str.end());
}

}  // namespace

std::optional<std::filesystem::path> FileDialogs::ShowOpenDialog(
    std::span<const FileFilter> filters,
    const std::optional<std::filesystem::path>& default_path) {
  EnsureNfdInitialized();
  NfdFilterGuard nfd_filters(filters);

  std::string default_path_u8;
  const nfdu8char_t* default_path_ptr = nullptr;

  if (default_path) {
    default_path_u8 = ToUtf8String(*default_path);
    default_path_ptr =
        reinterpret_cast<const nfdu8char_t*>(default_path_u8.c_str());
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
  const nfdu8char_t* default_path_ptr = nullptr;

  if (default_path) {
    default_path_u8 = ToUtf8String(*default_path);
    default_path_ptr =
        reinterpret_cast<const nfdu8char_t*>(default_path_u8.c_str());
  }

  const nfdu8char_t* default_name_ptr =
      default_name ? reinterpret_cast<const nfdu8char_t*>(default_name->c_str())
                   : nullptr;

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
