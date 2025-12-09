#include "physics.hpp"
#include "storage.hpp"
#include "transform.hpp"

using namespace okami;

class PhysicsModule final : 
    public EngineModule {
private:
    StorageModule<Transform>* m_transformStorage = nullptr;

protected:
    Error RegisterImpl(InterfaceCollection& interfaces) override {
        return {};
    }

    Error StartupImpl(InitContext const& context) override {
        return {};
    }

    void ShutdownImpl(InitContext const& context) override {
    }

    Error ProcessFrameImpl(Time const& t, ExecutionContext const& context) override {
        return {};
    }

    Error MergeImpl(MergeContext const& context) override {
        return {};
    }

public:
    std::string GetName() const override {
        return "Physics Module";
    }

    PhysicsModule() {
        m_transformStorage = CreateChild<StorageModule<Transform>>();
    }
};

std::unique_ptr<EngineModule> PhysicsModuleFactory::operator()() {
    // Placeholder for actual PhysicsModule implementation
    return std::make_unique<PhysicsModule>();
}