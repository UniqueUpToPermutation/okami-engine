#include "physics.hpp"
#include "storage.hpp"
#include "transform.hpp"
#include "aabb_tree.hpp"

using namespace okami;

struct TransformSolveMessage {
    entity_t m_entity;
    Transform m_result;
};

class PhysicsModule final : 
    public EngineModule {
private:
    StorageModule<Transform>* m_transformStorage = nullptr;
    AABBTree<entity_t, AABB2> m_aabbTree2D;

protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        context.m_messages.EnsurePort<TransformSolveMessage>();

        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error ProcessFrameImpl(Time const& t, ExecutionContext const& ec) override {
        ec.m_graph->AddMessageNode([this](
            JobContext& jobContext,
            In<Time> time,
            In<AddVelocity2DMessage> inAddVelocity,
            Out<TransformSolveMessage> outStagedTransforms) -> Error {

            inAddVelocity.Handle([&](AddVelocity2DMessage const& msg) {
                if (auto* transformPtr = m_transformStorage->TryGet(msg.m_entity)) {
                    auto dt = static_cast<float>(time->m_deltaTime);
                    auto transform = *transformPtr;
                    transform.m_position += glm::vec3(msg.m_velocity * dt, 0.0);
                    transform.m_rotation = glm::rotate(transform.m_rotation, msg.m_angular * dt, glm::vec3(0.0, 0.0, 1.0));
                    outStagedTransforms.Send(TransformSolveMessage{.m_entity = msg.m_entity, .m_result = transform});
                }
            });

            return {};
        });

        ec.m_graph->AddMessageNode([this](
            JobContext& jobContext,
            In<Time> time,
            In<AddVelocityMessage> inAddVelocity,
            Out<TransformSolveMessage> outStagedTransforms) -> Error {
            inAddVelocity.Handle([&](AddVelocityMessage const& msg) {
                if (auto* transformPtr = m_transformStorage->TryGet(msg.m_entity)) {
                    auto dt = static_cast<float>(time->m_deltaTime);
                    auto transform = *transformPtr;
                    transform.m_position += msg.m_velocity * dt;
                    transform.m_rotation = glm::rotate(transform.m_rotation, dt, msg.m_angular);
                    outStagedTransforms.Send(TransformSolveMessage{.m_entity = msg.m_entity, .m_result = transform});
                }
            });

            return {};
        });

        return {};
    }

    Error MergeImpl(MergeContext const& mc) override {
        auto solveMessages = mc.m_messages.GetPort<TransformSolveMessage>();

        solveMessages->Handle([&](TransformSolveMessage const& msg) {
            m_transformStorage->Set(msg.m_entity, msg.m_result);
        });

        return {};
    }

public:
    std::string GetName() const override {
        return "Physics Module";
    }

    PhysicsModule() {
        m_transformStorage = CreateChild<StorageModule<Transform>>();
        m_transformStorage->m_publishOnAddEvent = true;
        m_transformStorage->m_publishOnUpdateEvent = true;
        m_transformStorage->m_publishOnRemoveEvent = true;
    }
};

std::unique_ptr<EngineModule> PhysicsModuleFactory::operator()() {
    // Placeholder for actual PhysicsModule implementation
    return std::make_unique<PhysicsModule>();
}