#include "module.hpp"

#include <glog/logging.h>

using namespace okami;

namespace okami {

// InterfaceCollection method definitions
InterfaceCollection::~InterfaceCollection() = default;

std::optional<std::any> InterfaceCollection::QueryType(std::type_info const& type) const {
    auto it = m_interfaces.find(type);
    if (it != m_interfaces.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto InterfaceCollection::begin() {
    return m_interfaces.begin();
}

auto InterfaceCollection::end() {
    return m_interfaces.end();
}

auto InterfaceCollection::cbegin() const {
    return m_interfaces.cbegin();
}

auto InterfaceCollection::cend() const {
    return m_interfaces.cend();
}

// EngineModule destructor definition
EngineModule::~EngineModule() = default;

// EngineModule method definitions
auto EngineModule::begin() { return m_submodules.begin(); }
auto EngineModule::end() { return m_submodules.end(); }
auto EngineModule::begin() const { return m_submodules.begin(); }
auto EngineModule::end() const { return m_submodules.end(); }

Error EngineModule::Register(InterfaceCollection& a) {
    Error e = RegisterImpl(a);
    OKAMI_ERROR_RETURN(e); 

    for (auto& mod : m_submodules) {
        e += mod->Register(a);
        OKAMI_ERROR_RETURN(e);
    }
    return {};
}

Error EngineModule::Startup(InitContext const& a) {
    LOG(INFO) << "Starting " << GetName() << "...\n";

    Error e = StartupImpl(a);
    OKAMI_ERROR_RETURN(e);

    if (b_children_process_startup) {
        for (auto& mod : m_submodules) {
            e += mod->Startup(a);
            OKAMI_ERROR_RETURN(e);
        }
    }

    b_started = true;

    return {};
}

Error EngineModule::BuildGraph(JobGraph& a, BuildGraphParams const& b) {
    OKAMI_ASSERT(b_started, "Module must be started before processing frames");

    Error e = BuildGraphImpl(a, b);
    OKAMI_ERROR_RETURN(e);

    if (!b_children_build_update_graph) {
        return {};
    }
    
    for (auto& mod : m_submodules) {
        e += mod->BuildGraph(a, b);
        OKAMI_ERROR_RETURN(e);
    }
    return {};
}

Error EngineModule::SendMessages(MessageBus& a) {
    OKAMI_ASSERT(b_started, "Module must be started before processing frames");

    Error e = SendMessagesImpl(a);
    OKAMI_ERROR_RETURN(e);

    for (auto& mod : m_submodules) {
        e += mod->SendMessages(a);
        OKAMI_ERROR_RETURN(e);
    }
    return {};
}

Error EngineModule::ReceiveMessages(MessageBus& a) {
    OKAMI_ASSERT(b_started, "Module must be started before processing frames");

    Error e = ReceiveMessagesImpl(a);
    OKAMI_ERROR_RETURN(e);

    for (auto& mod : m_submodules) {
        e += mod->ReceiveMessages(a);
        OKAMI_ERROR_RETURN(e);
    }
    return {};
}

void EngineModule::Shutdown(InitContext const& a) {
    if (b_shutdown) {
        return;
    }
    b_shutdown = true;
    LOG(INFO) << "Shutting down " << GetName() << "...\n";
    for (auto it = m_submodules.rbegin(); it != m_submodules.rend(); ++it) {
        auto& mod = *it;
        mod->Shutdown(a);
    }
    m_submodules.clear();
    ShutdownImpl(a);
}

std::string EngineModule::GetName() const {
    return "Unnamed Module";
}

} // namespace okami