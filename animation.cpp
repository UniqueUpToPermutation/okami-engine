#include "animation.hpp"

#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>

#include <glog/logging.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace okami;

// ---------------------------------------------------------------------------
// AnimationSystemModule
//
// Each frame, for every entity with SkeletonComponent + SkeletonStateComponent:
//   1. Advance playback time.
//   2. Run SamplingJob (using the per-entity context cache stored in this module).
//   3. Run LocalToModelJob to produce model-space matrices.
//   4. Send UpdateComponentSignal<SkeletonStateComponent> so the registry is
//      updated during the frame-merge RecieveMessages phase.
//
// The SamplingJob::Context requires write access during sampling and therefore
// cannot live in a component (registry is read-only during job graph execution).
// It is stored in m_contexts, keyed by entity id.
// ---------------------------------------------------------------------------

class AnimationSystemModule final : public EngineModule {
    // Per-entity sampling context cache. Allocated lazily on first use.
    std::unordered_map<uint32_t,
        std::unique_ptr<ozz::animation::SamplingJob::Context>> m_contexts;

public:
    Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
        graph.AddMessageNode(
            [this, &registry = params.m_registry](
                JobContext&,
                In<Time> inTime,
                Out<UpdateComponentSignal<SkeletonStateComponent>> outState) -> Error
            {
                const float dt = inTime ? inTime->GetDeltaTimeF() : 0.0f;

                registry.view<SkeletonComponent const, SkeletonStateComponent const>().each(
                    [&](entt::entity entity,
                        SkeletonComponent    const& skel,
                        SkeletonStateComponent const& prevState)
                    {
                        if (!skel.m_skeleton || skel.m_animations.empty()) return;

                        const int idx = std::clamp(
                            skel.m_currentAnimation, 0,
                            static_cast<int>(skel.m_animations.size()) - 1);

                        auto const& skeleton = *skel.m_skeleton;
                        auto const& anim     = *skel.m_animations[static_cast<size_t>(idx)];

                        // Lazily create / resize the context for this entity.
                        auto& ctxPtr = m_contexts[static_cast<uint32_t>(entity)];
                        if (!ctxPtr) {
                            ctxPtr = std::make_unique<ozz::animation::SamplingJob::Context>();
                            ctxPtr->Resize(skeleton.num_joints());
                        }

                        // Advance playback time.
                        const float duration = anim.duration();
                        float newTime = prevState.m_time + dt * skel.m_speed;
                        if (skel.m_loop && duration > 0.0f) {
                            newTime = newTime - std::floor(newTime / duration) * duration;
                        } else {
                            newTime = std::clamp(newTime, 0.0f, duration);
                        }

                        // Build new state with pre-sized output buffers.
                        SkeletonStateComponent newState;
                        newState.m_time = newTime;
                        newState.m_localTransforms.resize(
                            static_cast<size_t>(skeleton.num_soa_joints()));
                        newState.m_modelMatrices.resize(
                            static_cast<size_t>(skeleton.num_joints()));

                        // Sample animation at the new ratio.
                        ozz::animation::SamplingJob sampleJob;
                        sampleJob.animation = &anim;
                        sampleJob.context   = ctxPtr.get();
                        sampleJob.ratio     = duration > 0.0f ? newTime / duration : 0.0f;
                        sampleJob.output    = ozz::make_span(newState.m_localTransforms);
                        if (!sampleJob.Run()) {
                            LOG(WARNING) << "AnimationSystemModule: SamplingJob failed for entity "
                                         << static_cast<uint32_t>(entity);
                            return;
                        }

                        // Convert to model-space matrices.
                        ozz::animation::LocalToModelJob ltmJob;
                        ltmJob.skeleton = &skeleton;
                        ltmJob.input    = ozz::make_span(newState.m_localTransforms);
                        ltmJob.output   = ozz::make_span(newState.m_modelMatrices);
                        if (!ltmJob.Run()) {
                            LOG(WARNING) << "AnimationSystemModule: LocalToModelJob failed";
                            return;
                        }

                        outState.Send(UpdateComponentSignal<SkeletonStateComponent>{
                            entity, std::move(newState)});
                    });

                return {};
            });
        return {};
    }

    std::string GetName() const override { return "Animation System"; }
};

std::unique_ptr<EngineModule> AnimationSystemModuleFactory::operator()() const {
    return std::make_unique<AnimationSystemModule>();
}
