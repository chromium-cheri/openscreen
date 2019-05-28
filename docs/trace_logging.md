## Trace Logging Library Usage

The idea behind Trace Logging is that we can choose to trace a function and then,
with minimal additional code, determine how long function execution ran for,
whether the function was successful, and connect all of this information to any
existing informational logs. The below provides information about how this can be
achieved with OSP's TraceLogging Infrastructure.

## Imports

To use TraceLogging, import platform/api/trace_logging.h
This file should import all other required trace logging files.

## File Division
The code for Trace Logging is divided up as follows:
  - platform/base/trace_logging.h = the macros external callers are expected to use
  - platform/base/trace_logging_types.h = the types/enums used by all trace logging
      classes that external callers are expected to use. Cannot be combined with
      trace_logging.h due to circular dependency issues.
  - platform/base/trace_logging_internal.h/.cc = the implementation of the tracing
      library
  - platform/base/trace_logging_platform.h = the platform layer we expect implemented
      to pass the actual call along
  - platform/api/trace_logging_platform.cc = implementation of trace_logging_platform.h

## Trace IDs

When a Trace Log is created, a new unsigned integer is associated with this specific
trace, which will going forward be referred to as a trace ID. Associating a Trace ID has the following benefits:

- The same trace ID can be associated with all Informational Logs generated during this
  traced method’s execution. This will allow for all informational logs to easily be
  associated with a method execution, so that if an unexpected failure occurs or edge
  case pops up we will have additional information useful in debugging this issue and
  can more easily determine the state of the system leading to this issue

- If another Trace Log is created during the execution of this first traced method, we
  will have a hierarchy of Trace IDs. This will allow us to easily create an execution
  tree (through trivial scripting, preexisting tool, or database operations) to get a
  better view of how execution is proceeding. Together, the IDs associated with these
  calls form a TraceID Hierarchy. The Hierarchy has the following format:

  - Current TraceID = the ID of the function currently being traced

  - Parent TraceID = the ID of the function which was being traced when this
    Trace was initiated

  - Root TraceID = the ID of the first Traced function under which this the current
    Trace was called

As an example:
public void DoThings() {
  TRACE_SCOPED(category, "outside");
  {
    TRACE_SCOPED(category, "middle");
    {
      TRACE_SCOPED(category, "inside");
    }
  }
}

This could result in the following TraceID Information:
  NAME          ROOT ID     PARENT ID   TRACE ID    RESULT
  outside       100         0           100         success
  middle        100         100         101         success
  inside        100         101         102         success

Note that, prior to any trace calls, the TraceID is considered to be 0x0 by convention.

## Trace Results

The "result" of a trace is meant to indicate whether the traced function completed
successfully or not. Results are handled differently for Synchronous and Asynchronous
traces.

For scoped traces, the trace is by default assumed to be successful. If a non- success
state is desired, this should be set using TRACE_SET_RESULT(result) where result is
some Error::Code enum value.

For asynchronous calls, the result must be set as part of the TRACE_ASYNC_END call.
As with scoped traces, the result must be some Error::Code enum value.

## Traceing Functions
SCOPED TRACING
  TRACE_SCOPED(category, name)
    If logging is enabled for the provided category, this function will trace the
    current function until the current scope ends with name as provided.

  TRACE_SCOPED(category, name, traceID)
    If logging is enabled for the provided category, this function will trace the
    current function until the current scope ends with name as provided. The TraceID
    used for tracing this function will be set to the one provided.

  TRACE_SCOPED(category, name, traceId, parentId, rootId)
    If logging is enabled for the provided category, this function will trace the
    current function until the current scope ends with name as provided. The TraceID
    used for tracing this function will be set to the one provided, as will the
    parent and root ids.

  TRACE_SCOPED(category, name, traceIdHierarchy)
    This call is intended for use in conjunction with the TRACE_HIERARCHY macro (as
    described below). If logging is enabled for the provided category, this function
    will trace the current function until the current scope ends with name as provided. The TraceID Hierarchy will be set as provided in the provided traceIdHierarchy
    parameter.

ASYNCHRONOUS TRACING
  TRACE_ASYNC_START(category, name)
    If logging is enabled for the provided category, this function will initiate a
    new asynchronous function trace with name as provided. It will not end until
    TRACE_ASYNC_END is called with the same TraceID generated for this async trace.

  TRACE_ASYNC_START(category, name, traceID)
    If logging is enabled for the provided category, this function will initiate a
    new asynchronous function trace with name and TraceID as provided. It will not
    end until TRACE_ASYNC_END is called with the same TraceID provided for this
    async trace.

  TRACE_ASYNC_START(category, name, traceId, parentId, rootId)
    If logging is enabled for the provided category, this function will initiate a
    new asynchronous function trace with name and full TraceID Hierarchy as
    provided. It will not end until TRACE_ASYNC_END is called with the same TraceID
    provided for this async trace.

  TRACE_ASYNC_START(category, name, traceIdHierarchy)
    This call is intended for use in conjunction with the TRACE_HIERARCHY macro (as
    described below). this function will initiate a
    new asynchronous function trace with name and full TraceID Hierarchy as
    provided. It will not end until TRACE_ASYNC_END is called with the same TraceID
    provided for this async trace.

  TRACE_ASYNC_END(category, id, result)
    This call will end a trace initiated by TRACE_ASYNC_START (as described above)
    if logging is enabled for the associated category. The id is expected to match
    that used by an ASYNC_TRACE_START call, and result is the result of this call.

OTHER TRACING MACROS
  TRACE_CURRENT_ID
    This macro returns the current TraceID at this point in time.

  TRACE_ROOT_ID
    This macro returns the root TraceID at this point in time.

  TRACE_HIERARCHY
    This macro returns an instance of struct TraceIdHierarchy containing the current
    TraceID Hierarchy. This is intended to be used with TRACE_SET_HIERARCHY (described
    below) so that TraceID Hierarchy can be maintained when crossing thread or machine
    boundaries.

  TRACE_SET_HIERARCHY(ids)
    This macro sets the current TraceId Hierarchy without initiating a scoped trace.
    The set ids will cease to apply when the current scope ends. This is intended to
    be used with TRACE_HIERARCHY (described above) so that TraceID Hierarchy can be
    maintained when crossing thread or machine boundaries.

  TRACE_SET_RESULT(result)
    Sets the current scoped trace's result as described above.

## Embedder-Specific Tracing Implementations

For an embedder to create a custom TraceLogging implementation, there are 3 steps:

1) Create a TraceLoggingPlatform
  In platform/api/trace_logging_platform.h, the interface TraceLoggingPlatform is
  defined. An embedder must define a class implementing this interface.

2) Define method IsLoggingEnabled(...)
  In platform/api/trace_logging_platform.h, the following function is define:
    bool IsLoggingEnabled(TraceCategory category);
  A TraceLogging implementation must define this method. Note that the implementation
  of this method should be as performance-optimal as possible, as this will be called
  frequently even when logging is disabled, so any performance

3) Call TRACE_SET_DEFAULT_PLATFORM
  TRACE_SET_DEFAULT_PLATFORM takes in a pointer to an instance of a
  TraceLoggingPlatform created via a call to 'new'. As part of the shutdown sequence
  for the service, 'delete' will be called on this pointer, so if it was created in
  any other way this will result in a Segmentation Fault during shutdown.

An example implementation can be seen in platform/api/trace_logging_platform.cc
