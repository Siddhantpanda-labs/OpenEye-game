#pragma once

#include "api/asset/AssetId.h"

namespace openeye::editor {

struct EditorContext;

/** Lit sphere preview of the material; uses ctx->materialPreviewPipeline. Safe when pipeline is null. */
void DrawMaterialEditorPreview(EditorContext* ctx, const ::eyelib::api::AssetId& matId);

} // namespace openeye::editor
