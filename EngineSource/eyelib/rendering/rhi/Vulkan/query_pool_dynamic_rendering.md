# Query pools, host reset, and dynamic rendering (Vulkan RHI)

**Scope:** `CommandListVK` (`ResetQueryPool`, `BeginQuery`, `EndQuery`), `DeviceVK::ResetQueryPoolOnHost` / `CreateQueryPool`, `QueryPoolVK`, `GpuProfiler`. Applies to `vkCmdBeginRendering` / `vkCmdEndRendering` (no classic `VkRenderPass` here).

## Vulkan rule (why host reset matters)

`vkCmdResetQueryPool` must not be recorded while a **rendering pass instance** is active. Dynamic rendering counts: between `vkCmdBeginRendering` and `vkCmdEndRendering`, `CommandListVK` sets `m_isRenderingActive`. Outside that span, command-buffer reset is used.

## `CommandListVK` behavior

| API | Inside dynamic rendering (`m_isRenderingActive`) | Outside |
| --- | --- | --- |
| `ResetQueryPool` | If `RenderBackendPolicy::SupportsHostQueryReset`: `IDevice::ResetQueryPoolOnHost` (`vkResetQueryPool` on host). Else: **error** (no cmd reset in pass). | `vkCmdResetQueryPool` |
| `BeginQuery` (Timestamp) | Implicit reset of slots `index` and `index+1`: host reset when `SupportsHostQueryReset`; else **error** if still inside pass; else cmd reset. Then `vkCmdWriteTimestamp` (TOP_OF_PIPE at `index`). `EndQuery` writes BOTTOM_OF_PIPE at `index+1`. | Same reset branch logic when not in pass. |
| `BeginQuery` / `EndQuery` (Occlusion, Pipeline statistics) | `vkCmdBeginQuery` / `vkCmdEndQuery` only (no implicit reset). Caller must `ResetQueryPool` when queries are **available** (Vulkan “unavailable” state) before reuse. Prefer reset **before** entering the pass, or use host reset for `ResetQueryPool` inside the pass. | Cmd reset allowed via `ResetQueryPool` before begin. |

Active slots are tracked per `ICommandList`: **begin** on an active index, **end** without begin, or **reset** overlapping an active range → **error** logs.

## `RenderBackendPolicy` vs enforcement

| Policy helper | Matches |
| --- | --- |
| `SupportsGpuTimestampQueries` | `CreateQueryPoolVK` / `DeviceVK::CreateQueryPool` reject `QueryType::Timestamp` when `RHICapabilities::supportsGpuTimestampQueries` is false. |
| `SupportsOcclusionQueries` | Pool creation for `QueryType::Occlusion`. |
| `SupportsPipelineStatisticsQueries` | Pool creation + `GpuProfiler` only allocates stats pools when this passes. |
| `SupportsHostQueryReset` | `ResetQueryPool` / timestamp pre-reset path when `m_isRenderingActive`. `DeviceVK::ResetQueryPoolOnHost` also checks the same cap. |

Callers should use `RenderBackendPolicy` (or `RHICapabilities`) for gating; `CommandListVK` uses the policy helpers for host-reset routing.

## Timestamp calibration (Vulkan)

- `VkPhysicalDeviceLimits::timestampPeriod`: nanoseconds per timestamp counter increment (see Vulkan spec).
- `QueryPoolVK` stores `m_timestampPeriodNs` at pool create and `GetResult` for `QueryType::Timestamp` returns `uint64_t` nanoseconds: `round(raw * timestampPeriod)`.
- **Elapsed time:** `GetResult(i+1) - GetResult(i)` is not automatic; `BeginQuery`/`EndQuery` for timestamps reserve `i` and `i+1` for the two `vkCmdWriteTimestamp` calls — read both indices after GPU idle / wait, subtract in app code if needed.
- `GpuProfiler` uses **pipeline statistics** only (no timestamp pools). Any future timestamp profiler should reuse `QueryPoolVK` calibration above.

## See also

- `documentation/vulkan/steps_todo.md` — Phase 7.
- `documentation/vulkan/vulkan_check.md` — query / host-reset matrix rows.
