#include "eyelib/render/submission/DrawBatch.h"
#include <unordered_map>
#include <cstdint>

namespace eyelib {
namespace render {

Drawable DrawBatch::ToDrawable(size_t i) const {
    Drawable d;
    d.mesh = mesh;
    d.material = material;
    d.materialInstance = materialInstance;
    d.submeshIndex = submeshIndex;
    d.bones = nullptr;
    d.boneCount = 0;
    if (i < transforms.size())
        d.transform = transforms[i];
    return d;
}

namespace {

struct BatchKey {
    Mesh* mesh = nullptr;
    Material* material = nullptr;
    int32_t submeshIndex = -1;
    bool operator==(const BatchKey& o) const {
        return mesh == o.mesh && material == o.material && submeshIndex == o.submeshIndex;
    }
};

struct BatchKeyHash {
    size_t operator()(const BatchKey& k) const {
        size_t h = std::hash<Mesh*>()(k.mesh);
        h ^= std::hash<Material*>()(k.material) << 1;
        h ^= std::hash<int32_t>()(k.submeshIndex) << 2;
        return h;
    }
};

} // namespace

std::vector<DrawBatch> BuildBatches(const std::vector<Drawable>& drawables) {
    std::unordered_map<BatchKey, DrawBatch, BatchKeyHash> map;
    std::vector<DrawBatch> skinnedBatches;

    for (const Drawable& d : drawables) {
        if (!d.mesh || !d.material) continue;

        const bool skinned = (d.bones != nullptr && d.boneCount > 0);
        if (skinned) {
            DrawBatch single;
            single.mesh = d.mesh;
            single.material = d.material;
            single.materialInstance = d.materialInstance;
            single.submeshIndex = d.submeshIndex;
            single.transforms.push_back(d.transform);
            skinnedBatches.push_back(std::move(single));
            continue;
        }

        BatchKey key{ d.mesh.get(), d.material.get(), d.submeshIndex };
        auto it = map.find(key);
        if (it == map.end()) {
            DrawBatch b;
            b.mesh = d.mesh;
            b.material = d.material;
            b.materialInstance = d.materialInstance;
            b.submeshIndex = d.submeshIndex;
            b.transforms.push_back(d.transform);
            map[key] = std::move(b);
        } else {
            it->second.transforms.push_back(d.transform);
        }
    }

    std::vector<DrawBatch> out;
    out.reserve(map.size() + skinnedBatches.size());
    for (auto& [_, b] : map)
        out.push_back(std::move(b));
    for (auto& b : skinnedBatches)
        out.push_back(std::move(b));
    return out;
}

} // namespace render
} // namespace eyelib
