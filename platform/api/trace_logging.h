// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_TRACE_LOGGING_H_
#define PLATFORM_API_TRACE_LOGGING_H_

#include <atomic>
#include <vector>

#include "absl/types/optional.h"
#include "osp_base/error.h"
#include "platform/api/time.h"

namespace openscreen {
namespace platform {

// Helper macros. These are used to simplify the macros below.
#define CONCAT(a, b) a##b
#define CONCAT_DUP(a, b) CONCAT(a, b)
#define MAKE_UNIQUE(a) CONCAT_DUP(a, __LINE__)
#define TRACE_FACTORY(catagory, name) \
  TraceLogger<catagory, name, __LINE__, __FILE__>

// All compiler-time macros for tracing.
// NOTE: the ternary operator is used here to ensure that the TraceLogger object
// is only constructed if tracing is enabled, but at the same time is created in
// the caller's scope. The C++ standards guide guarantees that the constructor
// should only be called when IsLoggingEnabled(...) evaluates to true.
#define TRACE_SET_HIERARCHY(ids)                                       \
  TraceBase::IsLoggingEnabled(TraceCatagory::Any) ? TraceIdSetter(ids) \
                                                  : TraceBase()
#define TRACE_SET_RESULT(result)                   \
  TraceBase::IsLoggingEnabled(TraceCatagory::Any)  \
      ? TraceIdHierarchyHandler::SetResult(result) \
      : false
#define TRACE_HIERARCHY                            \
  (TraceBase::IsLoggingEnabled(TraceCatagory::Any) \
       ? TraceIdHierarchyHandler::GetHierarchy()   \
       : TraceIdHierarchy::Empty())
#define TRACE_CURRENT_ID                           \
  (TraceBase::IsLoggingEnabled(TraceCatagory::Any) \
       ? TraceIdHierarchyHandler::GetCurrentId()   \
       : EmptyTraceId)
#define TRACE_ROOT_ID                              \
  (TraceBase::IsLoggingEnabled(TraceCatagory::Any) \
       ? TraceIdHierarchyHandler::GetRootId()      \
       : EmptyTraceId)

// Trace Macros. These macros must use #pragma suppress statements to allow for
// unreferenced named variables. Otherwise, the created Trace object will be
// deconstructed at the line where it is created, rather than at the end of the
// scoped area as expected.
#define TRACE_ASYNC_END(catagory, id, result)                   \
  TraceBase::IsLoggingEnabled(catagory)                         \
      ? TraceBase::TraceAsyncEnd(_LINE__, __FILE__, id, result) \
      : false

#if defined(__clang__)
#define TRACE(catagory, name, ...)                           \
  _Pragma("clang diagnostic push");                          \
  _Pragma("clang diagnostic ignored \"-Wunused-variable\""); \
  const auto& MAKE_UNIQUE(trace_ref_) =                      \
      TraceBase::IsLoggingEnabled(catagory)                  \
          ? TRACE_FACTORY(catagory, name){false, ##__VA_ARGS__}     \
          : TraceBase();                                     \
  _Pragma("clang diagnostic pop");
#define TRACE_ASYNC_START(catagory, name, ...)               \
  _Pragma("clang diagnostic push");                          \
  _Pragma("clang diagnostic ignored \"-Wunused-variable\""); \
  const auto& MAKE_UNIQUE(trace_ref_) =                      \
      TraceBase::IsLoggingEnabled(catagory)                  \
          ? TRACE_FACTORY(catagory, name){true, ##__VA_ARGS__}  \
          : TraceBase();                                     \
  _Pragma("clang diagnostic pop");

#elif defined(__GNUC__)
#define TRACE(catagory, name, ...)                         \
  _Pragma("GCC diagnostic push");                          \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\""); \
  const auto& MAKE_UNIQUE(trace_ref_) =                    \
      TraceBase::IsLoggingEnabled(catagory)                \
          ? TRACE_FACTORY(catagory, name){false, ##__VA_ARGS__}   \
          : TraceBase();                                   \
  _Pragma("GCC diagnostic pop");
#define TRACE_ASYNC_START(catagory, name, ...)              \
  _Pragma("GCC diagnostic push");                           \
  _Pragma("GCC diagnostic ignored \"-Wunused-variable\"");  \
  const auto& MAKE_UNIQUE(trace_ref_) =                     \
      TraceBase::IsLoggingEnabled(catagory)                 \
          ? TRACE_FACTORY(catagory, name){true, ##__VA_ARGS__} \
          : TraceBase();                                    \
  _Pragma("GCC diagnostic pop");

#else
#define TRACE(catagory, name, ...)                       \
  const auto& MAKE_UNIQUE(trace_ref_) =                  \
      TraceBase::IsLoggingEnabled(catagory)              \
          ? TRACE_FACTORY(catagory, name){false, ##__VA_ARGS__} \
          : TraceBase()

#define TRACE_ASYNC_START(catagory, name, ...)              \
  const auto& MAKE_UNIQUE(trace_ref_) =                     \
      TraceBase::IsLoggingEnabled(catagory)                 \
          ? TRACE_FACTORY(catagory, name){true, ##__VA_ARGS__} \
          : TraceBase()
#endif

// Constants used in tracing.
using TraceId = uint64_t;
constexpr TraceId EmptyTraceId = 0x0;

// BitFlags to represent the supported tracing catagories.
enum class TraceCatagory : uint64_t {
  Any = static_cast<uint64_t>(int64_t{-1}),
  CastPlatformLayer = 0x01,
  CastStreaming = 0x01 << 1,
  CastFlinging = 0x01 << 2
};
inline uint64_t operator|(TraceCatagory cat, uint64_t other) {
  return static_cast<uint64_t>(cat) | other;
}
inline uint64_t operator|(uint64_t cat, TraceCatagory other) {
  return other | cat;
}
inline uint64_t operator|(TraceCatagory cat, TraceCatagory other) {
  return cat | static_cast<uint64_t>(other);
}
inline uint64_t operator&(TraceCatagory cat, uint64_t other) {
  return static_cast<uint64_t>(cat) & other;
}
inline uint64_t operator&(uint64_t cat, TraceCatagory other) {
  return other & cat;
}
inline uint64_t operator&(TraceCatagory cat, TraceCatagory other) {
  return cat & static_cast<uint64_t>(other);
}

// Platform API base class. The platform will be an extension of this
// method, with an instantiation of that class being provided to the below
// TracePlatformWrapper class.
class TraceLoggingPlatform {
 public:
  virtual ~TraceLoggingPlatform() = default;

  virtual bool IsLoggingEnabled(TraceCatagory catagory) = 0;

  virtual void LogTrace(const char* name,
                        const uint32_t line,
                        const char* file,
                        Clock::time_point start_time,
                        Clock::time_point end_time,
                        TraceId trace_id,
                        TraceId parent_id,
                        TraceId root_id,
                        Error error) = 0;

  virtual void LogAsyncStart(const char* name,
                             const uint32_t line,
                             const char* file,
                             Clock::time_point timestamp,
                             TraceId trace_id,
                             TraceId parent_id,
                             TraceId root_id) = 0;

  virtual void LogAsyncEnd(const uint32_t line,
                           const char* file,
                           Clock::time_point timestamp,
                           TraceId trace_id,
                           Error error) = 0;
};

// A class to represent the current TraceId Hirearchy and for the user to
// pass around as needed.
struct TraceIdHierarchy {
  absl::optional<TraceId> current;
  absl::optional<TraceId> parent;
  absl::optional<TraceId> root;

  static TraceIdHierarchy Empty();
};

// Forward declaration so that it can be used in below classes.
class TraceIdSetter;

// Base class needed for Macro calls. It has been intentionally left with
// no instance variables so that construction is simple and fast.
class TraceBase {
 public:
  TraceBase() = default;
  virtual ~TraceBase() = default;

  // Passes the IsLoggingEnabled call to the platform layer.
  inline static bool IsLoggingEnabled(TraceCatagory catagory) {
    return trace_platform->IsLoggingEnabled(catagory);
  }

  // Traces an asynchronous call's end.
  // NOTE: This cannot be a void due to Macro behavior above, as the
  // ternary operator requires a return value.
  inline bool TraceAsyncEnd(const uint32_t line,
                            const char* file,
                            TraceId id,
                            Error e) {
    auto end_time = Clock::now();
    trace_platform->LogAsyncEnd(line, file, end_time, id, e);
    return true;
  }

 protected:
  // For now, only support one platform_layer. In future, we could add
  // more (ie: one to log to Chromium/etc, one to go to SQL, one for
  // Metrics, etc).
  static TraceLoggingPlatform* trace_platform;
};

// A base class for TraceLogging to allow for:
// 1) The sharing of all static and thread_local variables across template
// specializations.
// 2) Putting all template specializations into the same traces vector.
class TraceIdHierarchyHandler : public TraceBase {
 public:
  // Define the destructor to remove this item from the stack when it's
  // destroyed.
  virtual ~TraceIdHierarchyHandler() { traces.pop_back(); }

  // Getters the current Trace Hierarchy.
  static TraceId GetCurrentId();
  static TraceId GetRootId();
  static TraceIdHierarchy GetHierarchy();

  // Static method to set the result of the most recent trace.
  static bool SetResult(Error error);

  // Prevent copying this class because that would mess with the Trace
  // Hierarchy stack.
  TraceIdHierarchyHandler(const TraceIdHierarchyHandler& other) = delete;
  TraceIdHierarchyHandler& operator=(TraceIdHierarchyHandler& other) = delete;

  // Sets the result of this trace log.
  // NOTE: this must be define in this class rather than TraceLogger so that it
  // can be called on traces.back() without a potentially unsafe cast or type
  // checking at runtime.
  virtual bool SetTraceResult(Error error) = 0;

 protected:
  // Constructor to set all trace id information.
  TraceIdHierarchyHandler(absl::optional<TraceId> current_id,
                          absl::optional<TraceId> parent_id,
                          absl::optional<TraceId> root_id);

  // Current TraceId information.
  TraceId trace_id_;
  TraceId root_id_;
  TraceId parent_id_;

 private:
  // Counter to pick IDs when it is not provided.
  static std::atomic_uint64_t TraceIdCounter;

  // The FILO stack of TraceLoggers currently being watched by this
  // thread.
  static thread_local std::vector<TraceIdHierarchyHandler*> traces;
  static thread_local TraceIdSetter root_node;
  static std::vector<TraceIdHierarchyHandler*> InitializeTraces();
};

// The class which does actual trace logging.
template <TraceCatagory TCatagory,
          const char* TName,
          uint32_t TLine,
          const char* TFile>
class TraceLogger : public TraceIdHierarchyHandler {
 public:
  // Constructors to allow for various levels of provided trace id info.
  explicit TraceLogger(bool is_async)
      : TraceIdHierarchyHandler(absl::nullopt, absl::nullopt, absl::nullopt), is_async_(is_async) {
    this->Initialize();
  }
  TraceLogger(bool is_async, TraceId id)
      : TraceIdHierarchyHandler(id, absl::nullopt, absl::nullopt), is_async_(is_async) {
    this->Initialize();
  }
  TraceLogger(bool is_async, TraceId trace_id, TraceId parent_id, TraceId root_id)
      : TraceIdHierarchyHandler(trace_id, parent_id, root_id), is_async_(is_async) {
    this->Initialize();
  }
  TraceLogger(bool is_async, TraceIdHierarchy ids)
      : TraceIdHierarchyHandler(ids.current, ids.parent, ids.root), is_async_(is_async) {
    this->Initialize();
  }

  // Destructor that calls the platform layer's log method.
  virtual ~TraceLogger() {
    auto* current_platform =
        platform != nullptr ? platform : TraceBase::trace_platform;
    if (!is_async_) {
      auto end_time = Clock::now();
      current_platform->LogTrace(TName, TLine, TFile, start_time_, end_time,
                                 trace_id_, parent_id_, root_id_, result);
    } else {
      current_platform->LogAsyncStart(TName, TLine, TFile, start_time_,
                                      trace_id_, parent_id_, root_id_);
    }
  }

  // Set the result.
  bool SetTraceResult(Error error) {
    result = error;
    return true;
  }

  // Prevent copying this class, because that would mess with the Trace
  // Hierarchy stack.
  TraceLogger(const TraceLogger& other) = delete;
  TraceLogger& operator=(const TraceLogger& other) = delete;

 protected:
  // Timestamp for when the object was created.
  Clock::time_point start_time_;

  // Whether or not this trace will have an asynchronous end call later.
  bool is_async_;

  // Result of this operation.
  Error result;

  // Trace logging layer to use for this call. It is overridden for
  // testing purposes, but otherwise is set to nullptr.
  TraceLoggingPlatform* platform;

 private:
  // Constructor called by others to set local variables.
  void Initialize() {
    this->start_time_ = Clock::now();
    result = Error::None();
    platform = nullptr;
  }
};

// Inserts a fake element into the TraceIdHierarchyHandler stack to set
// the current TraceId Hierarchy manually.
class TraceIdSetter : public TraceIdHierarchyHandler {
 public:
  explicit TraceIdSetter(TraceIdHierarchy ids)
      : TraceIdHierarchyHandler(ids.current, ids.parent, ids.root) {}

  // Implement abstract method for use in Macros.
  bool SetTraceResult(Error error) { return false; }

  // Prevent copying this class because that would mess with the Trace
  // Hierarchy stack.
  TraceIdSetter(const TraceIdSetter& other) = delete;
  TraceIdSetter& operator=(const TraceIdSetter& other) = delete;
};

}  // namespace platform
}  // namespace openscreen

#endif
