#include "eyelib/render/framegraph/FrameGraphRecipe.h"

namespace eyelib {
namespace render {

void AddPassesFromRecipe(FrameGraph& graph,
                        const FrameGraphRecipe& recipe,
                        const FGNameToHandle& nameToHandle,
                        std::function<FrameGraph::ExecuteFunc(const std::string&)> getExecutor) {
    for (const FrameGraphPassDef& pass : recipe) {
        std::vector<FGTextureHandle> readHandles;
        readHandles.reserve(pass.reads.size());
        for (const std::string& r : pass.reads) {
            auto it = nameToHandle.find(r);
            if (it != nameToHandle.end())
                readHandles.push_back(it->second);
        }
        std::vector<FGTextureHandle> writeHandles;
        writeHandles.reserve(pass.writes.size());
        for (const std::string& w : pass.writes) {
            auto it = nameToHandle.find(w);
            if (it != nameToHandle.end())
                writeHandles.push_back(it->second);
        }
        graph.AddPass(pass.name, getExecutor(pass.name), readHandles, writeHandles, pass.sideEffect);
    }
}

} // namespace render
} // namespace eyelib
