#include "AnimationLoader.h"

#include "eyelib/assets/AnimationAsset.h"
#include "eyelib/assets/AssetManager.h"
#include "eyelib/animation/AnimationClip.h"
#include "eyelib/animation/KeyframeEditor.h"
#include "eyelib/animation/Skeleton.h"
#include "eyelib/core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <thread>

namespace eyelib {
namespace assets {

namespace {
static math::Mat4 ToMat4(const aiMatrix4x4& m) {
    return math::Mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

static void Decompose(const aiMatrix4x4& m, math::Vec3& pos, math::Quat& rot, math::Vec3& scale) {
    aiVector3D s, p;
    aiQuaternion r;
    m.Decompose(s, r, p);
    pos = math::Vec3(p.x, p.y, p.z);
    rot = math::Quat(r.w, r.x, r.y, r.z);
    scale = math::Vec3(s.x, s.y, s.z);
}

static void ProcessNode(aiNode* node, int parentIndex, const aiMatrix4x4& parentTransform,
                        animation::Skeleton& skeleton) {
    aiMatrix4x4 local = node->mTransformation;
    aiMatrix4x4 global = parentTransform * local;

    animation::Bone bone;
    bone.name = node->mName.C_Str();
    bone.parentIndex = parentIndex;
    Decompose(local, bone.bindPosition, bone.bindRotation, bone.bindScale);
    bone.inverseBindMatrix = glm::inverse(ToMat4(global));
    skeleton.AddBone(bone);
    int currentIndex = static_cast<int>(skeleton.BoneCount() - 1);

    for (unsigned i = 0; i < node->mNumChildren; ++i) {
        ProcessNode(node->mChildren[i], currentIndex, global, skeleton);
    }
}

static std::shared_ptr<animation::Skeleton> BuildSkeleton(const aiScene* scene) {
    if (!scene || !scene->mRootNode) return nullptr;
    auto skeleton = std::make_shared<animation::Skeleton>();
    ProcessNode(scene->mRootNode, -1, aiMatrix4x4(), *skeleton);
    skeleton->BuildBoneMap();
    return skeleton;
}

static std::vector<std::shared_ptr<animation::AnimationClip>> BuildClips(const aiScene* scene, const animation::Skeleton& skeleton) {
    std::vector<std::shared_ptr<animation::AnimationClip>> clips;
    if (!scene) return clips;

    for (unsigned a = 0; a < scene->mNumAnimations; ++a) {
        const aiAnimation* anim = scene->mAnimations[a];
        const double ticks = anim->mTicksPerSecond != 0.0 ? anim->mTicksPerSecond : 25.0;
        const float duration = static_cast<float>(anim->mDuration / ticks);
        auto clip = std::make_shared<animation::AnimationClip>(anim->mName.C_Str(), duration);

        for (unsigned c = 0; c < anim->mNumChannels; ++c) {
            const aiNodeAnim* channel = anim->mChannels[c];
            const int boneIndex = skeleton.FindBoneIndex(channel->mNodeName.C_Str());
            if (boneIndex < 0) continue;

            for (unsigned i = 0; i < channel->mNumPositionKeys; ++i) {
                const auto& key = channel->mPositionKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    animation::KeyframeEditor::Track::Position);
            }
            for (unsigned i = 0; i < channel->mNumRotationKeys; ++i) {
                const auto& key = channel->mRotationKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z));
            }
            for (unsigned i = 0; i < channel->mNumScalingKeys; ++i) {
                const auto& key = channel->mScalingKeys[i];
                const float t = static_cast<float>(key.mTime / ticks);
                animation::KeyframeEditor::AddKeyframe(*clip, boneIndex, t,
                    math::Vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                    animation::KeyframeEditor::Track::Scale);
            }
        }
        clips.push_back(clip);
    }
    return clips;
}
} // namespace

AssetId AnimationLoader::LoadAnimation(const std::string& path) {
    auto& manager = AssetManager::Get();
    const AssetId id = manager.Registry().RegisterAsset(path, "Animation");

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenSmoothNormals);
    if (!scene) {
        core::Log::WarnF("AnimationLoader: failed to load %s", path.c_str());
        return id;
    }

    auto asset = std::make_shared<AnimationAsset>();
    asset->skeleton = BuildSkeleton(scene);
    if (asset->skeleton) {
        asset->clips = BuildClips(scene, *asset->skeleton);
    }

    manager.Cache().Store(id, asset, "Animation");
    return id;
}

void AnimationLoader::LoadAnimationAsync(const std::string& path, LoadCallback callback) {
    std::thread([path, callback]() {
        const AssetId id = LoadAnimation(path);
        if (callback) callback(id);
    }).detach();
}

} // namespace assets
} // namespace eyelib
