#include "bgfx_triangle.hpp"

#include "bgfx_util.hpp"

#include "../transform.hpp"

#include <bx/math.h>

using namespace okami;

Error BgfxTriangleModule::RegisterImpl(ModuleInterface&) {
    return {};
}

Error BgfxTriangleModule::StartupImpl(ModuleInterface&) {
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

void BgfxTriangleModule::ShutdownImpl(ModuleInterface&) {
    m_program.Reset();
}

Error BgfxTriangleModule::ProcessFrameImpl(Time const&, ModuleInterface& mi) {
    return {};
}

Error BgfxTriangleModule::MergeImpl() {
    return {};
}
    
std::string_view BgfxTriangleModule::GetName() const {
    return "BGFX Triangle Module";
}

Error BgfxTriangleModule::Pass(Time const& time, ModuleInterface& mi, RenderPassInfo info) {
    auto transforms = mi.m_interfaces.Query<IComponentView<Transform>>();
    if (!transforms) {
        return Error("No IComponentView<Transform> available in BGFXTriangleModule");
    }

    if (!m_storage->IsEmpty() && bgfx::isValid(m_program)) {
        m_storage->ForEach([this, time, transforms](entity_t e, DummyTriangleComponent const&) {
            auto transform = transforms->GetOr(e, Transform::Identity()).AsMatrix();

            bgfx::setTransform(&transform);
            bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
            bgfx::setVertexCount(3);
            bgfx::submit(0, m_program);
        });
    }

    return Error{};
}

BgfxTriangleModule::BgfxTriangleModule() {
    m_storage = CreateChild<StorageModule<DummyTriangleComponent>>();
}