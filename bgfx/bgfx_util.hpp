#pragma once

#include "../common.hpp"

#include <filesystem>

#include <bx/bx.h>
#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace okami {
    template <typename T>
    class AutoHandle {
    private:
        T m_handle;
    
    public:
        AutoHandle() : m_handle(BGFX_INVALID_HANDLE) {}
        explicit AutoHandle(T handle) : m_handle(handle) {}
        ~AutoHandle() {
            if (bgfx::isValid(m_handle)) {
                bgfx::destroy(m_handle);
            }
        }
        OKAMI_NO_COPY(AutoHandle);
    };

    std::optional<AutoHandle<bgfx::ShaderHandle>> LoadBgfxShader(std::filesystem::path path);
}