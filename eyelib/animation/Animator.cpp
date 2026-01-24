#include "Animator.h"

#include "eyelib/math/TransformUtils.h"

namespace eyelib {
namespace animation {

void Animator::SetSkeleton(std::shared_ptr<Skeleton> skeleton) {
    m_skeleton = std::move(skeleton);
    const size_t boneCount = m_skeleton ? m_skeleton->BoneCount() : 0;
    m_pose.Resize(boneCount);
    m_boneMatrices.assign(boneCount, math::Mat4(1.0f));
    if (m_stateMachine) m_stateMachine->SetBoneCount(boneCount);
}

void Animator::SetStateMachine(std::shared_ptr<AnimationStateMachine> sm) {
    m_stateMachine = std::move(sm);
    if (m_stateMachine && m_skeleton) {
        m_stateMachine->SetBoneCount(m_skeleton->BoneCount());
    }
}

void Animator::SetBlendTree(std::shared_ptr<BlendTree> tree) {
    m_blendTree = std::move(tree);
}

void Animator::SetClip(std::shared_ptr<AnimationClip> clip) {
    m_clip = std::move(clip);
    m_time = 0.0f;
}

void Animator::SetParameter(const std::string& name, float value) {
    m_floatParams[name] = value;
}

void Animator::SetParameter(const std::string& name, bool value) {
    m_boolParams[name] = value;
}

float Animator::GetParameter(const std::string& name) const {
    auto it = m_floatParams.find(name);
    return it != m_floatParams.end() ? it->second : 0.0f;
}

bool Animator::GetBoolParameter(const std::string& name) const {
    auto it = m_boolParams.find(name);
    return it != m_boolParams.end() ? it->second : false;
}

void Animator::SetTrigger(const std::string& name) {
    m_triggers[name] = true;
}

void Animator::Update(float dt) {
    m_time += dt;
    if (!m_skeleton) return;

    if (m_stateMachine) {
        for (const auto& [k, v] : m_boolParams) m_stateMachine->SetCondition(k, v);
        for (const auto& [k, v] : m_floatParams) m_stateMachine->SetCondition(k, v);
        for (const auto& [k, v] : m_triggers) {
            if (v) m_stateMachine->SetTrigger(k);
        }
        m_triggers.clear();
        if (m_stateMachine->GetCurrentState() < 0 && m_skeleton->BoneCount() > 0) {
            m_stateMachine->SetBoneCount(m_skeleton->BoneCount());
        }
        m_stateMachine->Update(dt);
        m_pose = m_stateMachine->GetCurrentPose();
    } else if (m_blendTree) {
        std::unordered_map<std::string, float> params;
        params.insert(m_floatParams.begin(), m_floatParams.end());
        m_pose = m_blendTree->Evaluate(params, m_time, m_skeleton->BoneCount());
    } else if (m_clip) {
        m_pose = BlendSystem::SampleClip(*m_clip, m_time, m_skeleton->BoneCount());
    }

    BuildBoneMatrices();
}

void Animator::RebuildPose() {
    if (!m_skeleton) return;
    m_pose.Resize(m_skeleton->BoneCount());
}

void Animator::BuildBoneMatrices() {
    if (!m_skeleton) return;
    const size_t count = m_skeleton->BoneCount();
    if (m_pose.GetBoneCount() < count) m_pose.Resize(count);
    if (m_boneMatrices.size() != count) m_boneMatrices.assign(count, math::Mat4(1.0f));

    std::vector<math::Mat4> local(count, math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        const auto& t = m_pose.GetBoneTransform(i);
        local[i] = math::Compose(t.position, t.rotation, t.scale);
    }

    std::vector<math::Mat4> global(count, math::Mat4(1.0f));
    for (size_t i = 0; i < count; ++i) {
        const Bone& b = m_skeleton->GetBone(i);
        if (b.parentIndex >= 0) {
            global[i] = global[static_cast<size_t>(b.parentIndex)] * local[i];
        } else {
            global[i] = local[i];
        }
        m_boneMatrices[i] = global[i] * b.inverseBindMatrix;
    }
}

} // namespace animation
} // namespace eyelib
