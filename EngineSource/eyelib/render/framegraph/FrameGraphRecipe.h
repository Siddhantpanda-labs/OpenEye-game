#pragma once

#include "eyelib/render/framegraph/FrameGraph.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace eyelib {
namespace render {

/**
 * Declarative pass definition for frame graph (§5).
 * Pass declares reads/writes by resource name; engine resolves to handles and adds the pass.
 */
struct FrameGraphPassDef {
    std::string name;
    std::vector<std::string> reads;
    std::vector<std::string> writes;
    bool sideEffect = true;
};

using FrameGraphRecipe = std::vector<FrameGraphPassDef>;

/** Name -> FGTextureHandle map built from ImportTextureRHI(name, ...) / CreateTexture. */
using FGNameToHandle = std::unordered_map<std::string, FGTextureHandle>;

/**
 * Add passes to the graph from a recipe. Resolves read/write names to handles via nameToHandle.
 * getExecutor(passName) must return the ExecuteFunc for that pass (captures view, context, etc.).
 */
void AddPassesFromRecipe(FrameGraph& graph,
                        const FrameGraphRecipe& recipe,
                        const FGNameToHandle& nameToHandle,
                        std::function<FrameGraph::ExecuteFunc(const std::string&)> getExecutor);

} // namespace render
} // namespace eyelib
