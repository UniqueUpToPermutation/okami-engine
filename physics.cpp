#include "physics.hpp"
#include "storage.hpp"
#include "transform.hpp"

using namespace okami;

class PhysicsModule final : 
    public EngineModule {
private:
    StorageModule<Transform>* m_transformStorage = nullptr;

protected:
    Error RegisterImpl(ModuleInterface& mi) override {
        return {};
    }

    Error StartupImpl(ModuleInterface&) override {
        return {};
    }

    void ShutdownImpl(ModuleInterface&) override {
    }

    Error ProcessFrameImpl(Time const& t, ModuleInterface&) override {
        return {};
    }

    Error MergeImpl(ModuleInterface&) override {
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