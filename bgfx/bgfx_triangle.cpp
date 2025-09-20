#include "bgfx_triangle.hpp"

#include "bgfx_util.hpp"

#include "../transform.hpp"

using namespace okami;

Error BGFXTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error BGFXTriangleModule::StartupImpl(ModuleInterface&) {
    auto vs = LoadBgfxShader("triangle_vs.sc");
    auto fs = LoadBgfxShader("triangle_fs.sc");

    OKAMI_ERROR_RETURN(vs);
    OKAMI_ERROR_RETURN(fs);

    auto program = bgfx::createProgram(*vs, *fs, true);
    if (!bgfx::isValid(program)) {
        return Error("Failed to create shader program");
    }

    m_program = AutoHandle(program);

    return {};
}

void BGFXTriangleModule::ShutdownImpl(ModuleInterface&) {
    m_program.Reset();
}

Error BGFXTriangleModule::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error BGFXTriangleModule::MergeImpl() {
    return {};
}
    
std::string_view BGFXTriangleModule::GetName() const {
    return "BGFX Triangle Module";
}

Error BGFXTriangleModule::Pass(Time const& time, ModuleInterface& mi, RenderPassInfo info) {
    auto transforms = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!transforms) {
        return Error("No IComponentView<Transform> available in BGFXTriangleModule");
    }

    if (!m_storage->IsEmpty() && bgfx::isValid(m_program)) {
        m_storage->ForEach([this, transforms](entity_t e, DummyTriangleComponent const&) {
            auto transform = glm::transpose(transforms->GetOr(e, Transform::Identity()).AsMatrix());
            bgfx::setTransform(&transform);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexCount(3);
            bgfx::submit(0, m_program);
        });
    }

    return Error{};
}

BGFXTriangleModule::BGFXTriangleModule() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}