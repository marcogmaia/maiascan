# Memory Scanner Architecture Refactoring Plan

## Overview

This document outlines the plan to refactor the memory scanner architecture to achieve better separation of concerns. The new design separates low-level memory operations from high-level scan logic interpretation.

## Current State Analysis

### Existing Components

- `IMemoryScanner`: High-level interface for scanning operations
- `MemoryScanner`: Implementation that handles both memory access and scan logic
- `IMemoryAccessor`: Low-level interface for raw memory operations
- `Scanner`: Low-level scanning implementation

### Issues with Current Design

- MemoryScanner inherits from IMemoryScanner, creating tight coupling
- Mixes low-level memory operations with high-level scan logic
- Difficult to test scan logic independently from memory access
- Hard to reuse memory access functionality for other purposes

## Proposed Architecture

### Component Hierarchy

```
┌─────────────────────────────────────────────────────────────┐
│                    MemoryScanner (Concrete)                 │
│  - Implements scan type logic (kExactValue, kGreaterThan,   │
│    kChanged, kIncreased, etc.)                              │
│  - Interprets different value types                         │
│  - Uses IMemoryAccessor for raw operations                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ uses
                              ▼
┌─────────────────────────────────────────────────────────────┐
│              IMemoryAccessor (Interface)                    │
│  - ReadMemory(address, buffer)                              │
│  - WriteMemory(address, data)                               │
│  - GetMemoryRegions()                                       │
└─────────────────────────────────────────────────────────────┘
                              │
                              │ implemented by
                              ▼
┌─────────────────────────────────────────────────────────────┐
│            MemoryAccessor (Implementation)                  │
│  - Concrete implementation using process API                │
│  - Handles actual memory reading/writing                    │
└─────────────────────────────────────────────────────────────┘
```

## Detailed Component Specifications

### 1. IMemoryAccessor Interface

**File**: `src/maia/core/i_memory_accessor.h`

```cpp
class IMemoryAccessor {
 public:
  virtual ~IMemoryAccessor() = default;

  /// Reads a block of memory from a given virtual address
  virtual bool ReadMemory(MemoryPtr address, std::span<std::byte> out_buffer) = 0;

  /// Writes a block of memory to a given virtual address
  virtual bool WriteMemory(MemoryPtr address, std::span<const std::byte> data) = 0;

  /// Gets a list of all scannable memory regions
  virtual std::vector<MemoryRegion> GetMemoryRegions() = 0;
};
```

**Responsibilities**:

- Provide raw byte-level memory access
- No knowledge of scan types or value interpretation
- Focus solely on memory I/O operations

### 2. MemoryScanner Class (Refactored)

**File**: `src/maia/application/memory_scanner.h/cpp`

```cpp
class MemoryScanner {
 public:
  explicit MemoryScanner(IMemoryAccessor& memory_accessor);

  ScanResult NewScan(const ScanParams& params);
  ScanResult NextScan(const ScanResult& previous_result, const ScanParams& params);

 private:
  IMemoryAccessor& memory_accessor_;
  std::vector<MemoryRegion> memory_regions_;
  std::shared_ptr<const MemorySnapshot> snapshot_;

  // Helper methods for different scan types
  template<CScannableType T>
  std::vector<uintptr_t> FindValuesInRegion(MemoryRegion region, const ScanParamsType<T>& params);

  template<CScannableType T>
  bool CompareValue(const T& current, const T& target, ScanComparison comparison);

  // ... other helper methods
};
```

**Responsibilities**:

- Implement all scan comparison logic (kExactValue, kGreaterThan, kChanged, etc.)
- Handle different value types (int8, int16, float, double, etc.)
- Interpret scan parameters and apply appropriate logic
- Manage scan state and snapshots
- **Does NOT inherit from any interface**

### 3. Scan Type Logic Implementation

Based on `src/maia/core/scan_types.h`, MemoryScanner will implement:

#### Initial Scan Types

- `kUnknown`: Snapshot all memory regions
- `kExactValue`: Find exact byte matches
- `kNotEqual`: Find values not equal to target
- `kGreaterThan`: Find values greater than target
- `kLessThan`: Find values less than target
- `kBetween`: Find values within range [value1, value2]
- `kNotBetween`: Find values outside range

#### Subsequent Scan Types

- `kChanged`: Values that changed since last scan
- `kUnchanged`: Values that remained the same
- `kIncreased`: Values that increased
- `kDecreased`: Values that decreased
- `kIncreasedBy`: Values increased by specific amount
- `kDecreasedBy`: Values decreased by specific amount

### 4. Value Type Support

MemoryScanner will handle all types defined in scan_types.h:

- Integer types: int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t
- Floating point: float, double
- Variable length: std::string, std::wstring, std::vector<std::byte>

## Implementation Steps

### Phase 1: Foundation

1. **Keep IMemoryAccessor interface** - Already exists and is suitable
2. **Create MemoryAccessor implementation** - Concrete class that implements IMemoryAccessor
3. **Update existing code** to use MemoryAccessor instead of direct process access

### Phase 2: MemoryScanner Refactoring

1. **Remove inheritance** from IMemoryScanner
2. **Add IMemoryAccessor dependency** via constructor
3. **Migrate scan logic** from old implementation
4. **Implement template methods** for different value types
5. **Handle all ScanComparison types**

### Phase 3: Integration

1. **Update composition root** to wire up dependencies
2. **Modify CMakeLists.txt** if needed
3. **Update tests** to work with new design
4. **Verify backward compatibility** where needed

## Key Design Decisions

### 1. Why MemoryScanner Doesn't Inherit from Interface

- **Flexibility**: Can evolve independently without breaking contracts
- **Simplicity**: No virtual function overhead
- **Composition**: Better suited for composition over inheritance
- **Testing**: Easier to mock and test in isolation

### 2. Why IMemoryAccessor is Separate

- **Reusability**: Can be used for other memory operations beyond scanning
- **Testability**: Easy to mock for unit testing scan logic
- **Single Responsibility**: Focuses only on memory I/O
- **Platform Abstraction**: Hides platform-specific details

### 3. Template-Based Value Handling

- **Performance**: Compile-time type resolution
- **Type Safety**: Compile-time checking of value types
- **Flexibility**: Supports any scannable type
- **No Runtime Overhead**: No virtual dispatch for value operations

## Testing Strategy

### Unit Tests

1. **Mock IMemoryAccessor** to test scan logic in isolation
2. **Test each ScanComparison type** independently
3. **Test value type handling** for all supported types
4. **Test edge cases** (empty results, boundary conditions, etc.)

### Integration Tests

1. **Test with real MemoryAccessor** implementation
2. **End-to-end scanning scenarios**
3. **Performance benchmarks** for large memory regions

## Migration Path

### Backward Compatibility

- Existing code using MemoryScanner will need minor updates
- Constructor changes from `MemoryScanner(IProcess&)` to `MemoryScanner(IMemoryAccessor&)`
- Return types and method signatures remain the same

### Gradual Migration

1. Create new components alongside existing ones
2. Migrate one scan type at a time
3. Run tests after each migration step
4. Remove old code once migration is complete

## Benefits of New Architecture

1. **Clear Separation**: Memory access vs. scan logic
2. **Better Testability**: Each component can be tested independently
3. **Improved Reusability**: IMemoryAccessor can be used elsewhere
4. **Easier Maintenance**: Changes to scan logic don't affect memory access
5. **Enhanced Extensibility**: New scan types easier to add
6. **Performance**: Template-based implementation avoids runtime overhead

## Files to Create/Modify

### New Files

- `src/maia/scanner/memory_accessor.h/cpp` - Implementation of IMemoryAccessor

### Modified Files

- `src/maia/application/memory_scanner.h/cpp` - Refactored to use composition
- `src/maia/scanner/CMakeLists.txt` - Add new files
- Test files - Update to work with new design

### Unchanged Files

- `src/maia/core/i_memory_accessor.h` - Already suitable
- `src/maia/core/scan_types.h` - Definition remains the same
- `src/maia/core/scan_result.h` - No changes needed

## Timeline Estimate

- **Phase 1**: 2-3 hours
- **Phase 2**: 4-5 hours
- **Phase 3**: 2-3 hours
- **Testing & Refinement**: 2-3 hours
- **Total**: 10-14 hours

## Risk Mitigation

- Keep old implementation until new one is fully tested
- Run parallel tests during migration
- Maintain backward compatibility where possible
- Document all breaking changes
