#pragma once

namespace openeye {
namespace editor {

struct EditorContext;

namespace panels {

/** Profiler overlay: FPS, frame time, CPU breakdown, GPU pass times, optional frame-time graph. */
void ShowProfilerPanel(EditorContext* ctx);

} // namespace panels
} // namespace editor
} // namespace openeye
