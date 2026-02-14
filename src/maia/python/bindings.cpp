// Copyright (c) Maia

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl_bind.h>

#include <format>
#include <ranges>

#include "maia/core/pointer_map.h"
#include "maia/core/pointer_scanner.h"
#include "maia/core/process.h"
#include "maia/core/scan_session.h"
#include "maia/core/scan_types.h"
#include "maia/core/scanner.h"

PYBIND11_MAKE_OPAQUE(std::vector<uintptr_t>);

namespace maia::core {

namespace {

namespace py = pybind11;

void BindMaiascan(py::module_& m) {
  m.doc() = "MaiaScan Python Bindings";

  // Enums
  py::enum_<ScanValueType>(m, "ScanValueType")
      .value("kInt8", ScanValueType::kInt8)
      .value("kUInt8", ScanValueType::kUInt8)
      .value("kInt16", ScanValueType::kInt16)
      .value("kUInt16", ScanValueType::kUInt16)
      .value("kInt32", ScanValueType::kInt32)
      .value("kUInt32", ScanValueType::kUInt32)
      .value("kInt64", ScanValueType::kInt64)
      .value("kUInt64", ScanValueType::kUInt64)
      .value("kFloat", ScanValueType::kFloat)
      .value("kDouble", ScanValueType::kDouble)
      .value("kString", ScanValueType::kString)
      .value("kWString", ScanValueType::kWString)
      .value("kArrayOfBytes", ScanValueType::kArrayOfBytes)
      .export_values();

  py::enum_<ScanComparison>(m, "ScanComparison")
      .value("kUnknown", ScanComparison::kUnknown)
      .value("kExactValue", ScanComparison::kExactValue)
      .value("kNotEqual", ScanComparison::kNotEqual)
      .value("kGreaterThan", ScanComparison::kGreaterThan)
      .value("kLessThan", ScanComparison::kLessThan)
      .value("kBetween", ScanComparison::kBetween)
      .value("kNotBetween", ScanComparison::kNotBetween)
      .value("kChanged", ScanComparison::kChanged)
      .value("kUnchanged", ScanComparison::kUnchanged)
      .value("kIncreased", ScanComparison::kIncreased)
      .value("kDecreased", ScanComparison::kDecreased)
      .value("kIncreasedBy", ScanComparison::kIncreasedBy)
      .value("kDecreasedBy", ScanComparison::kDecreasedBy)
      .export_values();

  py::enum_<mmem::Protection>(m, "Protection")
      .value("kNone", mmem::Protection::kNone)
      .value("kRead", mmem::Protection::kRead)
      .value("kWrite", mmem::Protection::kWrite)
      .value("kExecute", mmem::Protection::kExecute)
      .value("kExecuteRead", mmem::Protection::kExecuteRead)
      .value("kExecuteWrite", mmem::Protection::kExecuteWrite)
      .value("kReadWrite", mmem::Protection::kReadWrite)
      .value("kExecuteReadWrite", mmem::Protection::kExecuteReadWrite)
      .export_values();

  // Opaque Vectors
  py::bind_vector<std::vector<uintptr_t>>(m, "AddressVector");

  // mmem Types
  py::class_<mmem::ModuleDescriptor>(m, "ModuleDescriptor")
      .def_readwrite("base", &mmem::ModuleDescriptor::base)
      .def_readwrite("end", &mmem::ModuleDescriptor::end)
      .def_readwrite("size", &mmem::ModuleDescriptor::size)
      .def_readwrite("path", &mmem::ModuleDescriptor::path)
      .def_readwrite("name", &mmem::ModuleDescriptor::name);

  py::class_<mmem::SegmentDescriptor>(m, "MemoryRegion")
      .def_readwrite("base", &mmem::SegmentDescriptor::base)
      .def_readwrite("end", &mmem::SegmentDescriptor::end)
      .def_readwrite("size", &mmem::SegmentDescriptor::size)
      .def_readwrite("protection", &mmem::SegmentDescriptor::protection);

  // Structs
  py::class_<ScanConfig>(m, "ScanConfig")
      .def(py::init<>())
      .def_readwrite("value_type", &ScanConfig::value_type)
      .def_readwrite("comparison", &ScanConfig::comparison)
      .def_property(
          "value",
          [](const ScanConfig& self) {
            return py::bytes(reinterpret_cast<const char*>(self.value.data()),
                             self.value.size());
          },
          [](ScanConfig& self, py::bytes b) {
            std::string s = b;
            self.value.resize(s.size());
            std::memcpy(self.value.data(), s.data(), s.size());
          })
      .def_property(
          "value_end",
          [](const ScanConfig& self) {
            return py::bytes(
                reinterpret_cast<const char*>(self.value_end.data()),
                self.value_end.size());
          },
          [](ScanConfig& self, py::bytes b) {
            std::string s = b;
            self.value_end.resize(s.size());
            std::memcpy(self.value_end.data(), s.data(), s.size());
          })
      .def_property(
          "mask",
          [](const ScanConfig& self) {
            return py::bytes(reinterpret_cast<const char*>(self.mask.data()),
                             self.mask.size());
          },
          [](ScanConfig& self, py::bytes b) {
            std::string s = b;
            self.mask.resize(s.size());
            std::memcpy(self.mask.data(), s.data(), s.size());
          })
      .def_readwrite("alignment", &ScanConfig::alignment)
      .def_readwrite("use_previous_results", &ScanConfig::use_previous_results)
      .def_readwrite("pause_while_scanning", &ScanConfig::pause_while_scanning)
      .def("validate", &ScanConfig::Validate)
      .def("__repr__", [](const ScanConfig& self) {
        return "<maiascan.ScanConfig type=" +
               py::str(py::cast(self.value_type)).cast<std::string>() +
               " comp=" +
               py::str(py::cast(self.comparison)).cast<std::string>() + ">";
      });

  py::class_<ScanStorage>(m, "ScanStorage")
      .def(py::init<>())
      .def_readwrite("addresses", &ScanStorage::addresses)
      .def_property_readonly("curr_raw",
                             [](const ScanStorage& self) {
                               return py::bytes(reinterpret_cast<const char*>(
                                                    self.curr_raw.data()),
                                                self.curr_raw.size());
                             })
      .def_property_readonly("prev_raw",
                             [](const ScanStorage& self) {
                               return py::bytes(reinterpret_cast<const char*>(
                                                    self.prev_raw.data()),
                                                self.prev_raw.size());
                             })
      .def_readwrite("stride", &ScanStorage::stride)
      .def_readwrite("value_type", &ScanStorage::value_type)
      .def("__repr__", [](const ScanStorage& self) {
        return "<maiascan.ScanStorage count=" +
               std::to_string(self.addresses.size()) + " type=" +
               py::str(py::cast(self.value_type)).cast<std::string>() + ">";
      });

  py::class_<ScanResult>(m, "ScanResult")
      .def(py::init<>())
      .def_readwrite("storage", &ScanResult::storage)
      .def_readwrite("success", &ScanResult::success)
      .def_readwrite("error_message", &ScanResult::error_message)
      .def("__repr__", [](const ScanResult& self) {
        if (self.success) {
          return "<maiascan.ScanResult success=True count=" +
                 std::to_string(self.storage.addresses.size()) + ">";
        }
        return "<maiascan.ScanResult success=False error='" +
               self.error_message + "'>";
      });

  // Pointer Scanning Types
  py::class_<PointerPath>(m, "PointerPath")
      .def_readwrite("base_address", &PointerPath::base_address)
      .def_readwrite("module_name", &PointerPath::module_name)
      .def_readwrite("module_offset", &PointerPath::module_offset)
      .def_readwrite("offsets", &PointerPath::offsets)
      .def("__repr__", [](const PointerPath& self) {
        std::string s = "PointerPath(";
        if (!self.module_name.empty()) {
          s += std::format("{}+0x{:x}", self.module_name, self.module_offset);
        } else {
          s += std::format("0x{:x}", self.base_address);
        }
        for (auto offset : self.offsets) {
          s += std::format(" -> 0x{:x}", offset);
        }
        return s + ")";
      });

  py::class_<PointerScanConfig>(m, "PointerScanConfig")
      .def(py::init<>())
      .def_readwrite("target_address", &PointerScanConfig::target_address)
      .def_readwrite("max_level", &PointerScanConfig::max_level)
      .def_readwrite("max_offset", &PointerScanConfig::max_offset)
      .def_readwrite("allow_negative_offsets",
                     &PointerScanConfig::allow_negative_offsets)
      .def_readwrite("max_results", &PointerScanConfig::max_results)
      .def_property(
          "last_offsets",
          [](const PointerScanConfig& c) {
            // Convert vector<optional<int64_t>> to Python list (with None for
            // nullopt). Reverse so Python sees natural order [16, 0] instead
            // of C++ internal order [0, 16].
            py::list result;
            for (auto last_offset : std::views::reverse(c.last_offsets)) {
              if (last_offset.has_value()) {
                result.append(last_offset.value());
              } else {
                result.append(py::none());
              }
            }
            return result;
          },
          [](PointerScanConfig& c, py::list lst) {
            // Convert Python list (with None for wildcard) to
            // vector<optional<int64_t>>. Reverse so Python [16, 0] becomes
            // C++ [0, 16] (index 0 = last offset closest to target).
            c.last_offsets.clear();
            c.last_offsets.reserve(py::len(lst));
            // Iterate in reverse to store in C++ order
            for (auto it = lst.end(); it != lst.begin();) {
              --it;
              if (it->is_none()) {
                c.last_offsets.emplace_back(std::nullopt);
              } else {
                c.last_offsets.emplace_back(it->cast<int64_t>());
              }
            }
          });

  py::class_<PointerScanResult>(m, "PointerScanResult")
      .def_readwrite("paths", &PointerScanResult::paths)
      .def_readwrite("success", &PointerScanResult::success)
      .def_readwrite("error_message", &PointerScanResult::error_message);

  // Process must be bound BEFORE PointerMap/PointerScanner (they reference it)
  py::class_<Process, std::unique_ptr<Process>>(m, "Process")
      .def_static("Create", py::overload_cast<uint32_t>(&Process::Create))
      .def_static("Create",
                  py::overload_cast<std::string_view>(&Process::Create))
      .def("GetProcessId", &Process::GetProcessId)
      .def("GetProcessName", &Process::GetProcessName)
      .def("IsValid", &Process::IsProcessValid)
      .def("GetBaseAddress", &Process::GetBaseAddress)
      .def("GetModules", &Process::GetModules)
      .def("GetMemoryRegions", &Process::GetMemoryRegions)
      .def("Suspend", &Process::Suspend)
      .def("Resume", &Process::Resume)
      .def("GetPointerSize", &Process::GetPointerSize)
      .def("ReadMemory",
           [](Process& self, uintptr_t address, size_t size) {
             std::vector<std::byte> buffer(size);
             MemoryAddress addr = address;
             bool ok = self.ReadMemory({&addr, 1}, size, buffer, nullptr);
             if (!ok) {
               throw std::runtime_error("Failed to read memory");
             }
             return py::bytes(reinterpret_cast<const char*>(buffer.data()),
                              buffer.size());
           })
      .def("__repr__", [](const Process& self) {
        return "<maiascan.Process name='" + std::string(self.GetProcessName()) +
               "' pid=" + std::to_string(self.GetProcessId()) + ">";
      });

  py::class_<PointerMap>(m, "PointerMap")
      .def_static(
          "Generate",
          [](Process& process) { return PointerMap::Generate(process); },
          py::arg("process"))
      .def("GetEntryCount", &PointerMap::GetEntryCount);

  py::class_<PointerScanner>(m, "PointerScanner")
      .def(py::init<>())
      .def(
          "FindPaths",
          [](const PointerScanner& self,
             const PointerMap& map,
             const PointerScanConfig& config,
             const std::vector<mmem::ModuleDescriptor>& modules) {
            return self.FindPaths(map, config, modules);
          },
          py::arg("map"),
          py::arg("config"),
          py::arg("modules"),
          py::call_guard<py::gil_scoped_release>())
      .def(
          "ResolvePath",
          [](const PointerScanner& self,
             Process& process,
             const PointerPath& path) {
            return self.ResolvePath(process, path);
          },
          py::arg("process"),
          py::arg("path"))
      .def(
          "ResolvePath",
          [](const PointerScanner& self,
             Process& process,
             const PointerPath& path,
             const std::vector<mmem::ModuleDescriptor>& modules) {
            return self.ResolvePath(process, path, modules);
          },
          py::arg("process"),
          py::arg("path"),
          py::arg("modules"))
      .def(
          "FilterPaths",
          [](const PointerScanner& self,
             Process& process,
             const std::vector<PointerPath>& paths,
             uint64_t expected_target) {
            return self.FilterPaths(process, paths, expected_target);
          },
          py::arg("process"),
          py::arg("paths"),
          py::arg("expected_target"),
          "Filter paths: keep only those that resolve to expected_target");

  py::class_<Scanner>(m, "Scanner")
      .def(py::init<>())
      .def(
          "FirstScan",
          [](Scanner& self, Process& process, const ScanConfig& config) {
            return self.FirstScan(process, config);
          },
          py::call_guard<py::gil_scoped_release>())
      .def(
          "NextScan",
          [](Scanner& self,
             Process& process,
             const ScanConfig& config,
             const ScanStorage& prev) {
            return self.NextScan(process, config, prev);
          },
          py::call_guard<py::gil_scoped_release>());

  py::class_<ScanSession>(m, "ScanSession")
      .def(py::init<>())
      .def("GetStorageSnapshot", &ScanSession::GetStorageSnapshot)
      .def("GetConfig", &ScanSession::GetConfig)
      .def("CommitResults", &ScanSession::CommitResults)
      .def("Clear", &ScanSession::Clear)
      .def("GetResultCount", &ScanSession::GetResultCount)
      .def("HasResults", &ScanSession::HasResults);
}

}  // namespace

}  // namespace maia::core

PYBIND11_MODULE(maiascan, m) {
  maia::core::BindMaiascan(m);
}
