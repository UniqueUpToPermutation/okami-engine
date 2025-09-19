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
        operator T() const { return m_handle; }

        void Reset(T handle = BGFX_INVALID_HANDLE) {
            if (bgfx::isValid(m_handle)) {
                bgfx::destroy(m_handle);
            }
            m_handle = handle;
        }

        AutoHandle() : m_handle(BGFX_INVALID_HANDLE) {}
        explicit AutoHandle(T handle) : m_handle(handle) {}
        ~AutoHandle() {
            Reset();
        }
        OKAMI_NO_COPY(AutoHandle);
        
        inline AutoHandle(AutoHandle&& other) noexcept : m_handle(other.m_handle) {
            other.m_handle = BGFX_INVALID_HANDLE;
        }
        inline AutoHandle& operator=(AutoHandle&& other) noexcept {
            if (this != &other) {
                Reset(other.m_handle);
                other.m_handle = BGFX_INVALID_HANDLE;
            }
            return *this;
        }
    };

   Expected<AutoHandle<bgfx::ShaderHandle>> LoadBgfxShader(std::filesystem::path path);
}