#include "engine.hpp"

#include "entity_manager.hpp"
#include "config.hpp"
#include "physics.hpp"
#include "renderer.hpp"
#include "io.hpp"

#include <chrono>
#include <filesystem>

#include <glog/logging.h>

using namespace okami;

Engine::Engine(EngineParams params) : 
    m_params(params),
    m_ioModules("I/O Modules"),
    m_updateModules("Update Modules"),
    m_renderModules("Render Modules") {
	if (!google::IsGoogleLoggingInitialized()) {
		std::string name;
		if (m_params.m_argc == 0) {
			name = "okami";
		}
		else {
			name = m_params.m_argv[0];
		}
		google::InitGoogleLogging(name.c_str());

#ifndef NDEBUG
		google::SetStderrLogging(google::LogSeverity::GLOG_INFO); // Enable INFO and WARNING printouts in debug mode
		google::LogToStderr(); // Ensure logs are printed to stderr
#else
		google::SetStderrLogging(google::LogSeverity::GLOG_ERROR);
		if (params.m_forceLogToConsole) {
			google::LogToStderr();
		}
#endif
	}

    CreateUpdateModule(EntityManagerFactory{});
    CreateUpdateModule(ConfigModuleFactory{});
	CreateUpdateModule(PhysicsModuleFactory{});

	CreateIOModule(TextureIOModuleFactory{});
	CreateIOModule(GeometryIOModuleFactory{});
}

entity_t Engine::CreateEntity(entity_t parent) {
    if (m_entityManager) {
        return m_entityManager->CreateEntity(parent);
    } else {
        throw std::runtime_error("No IEntityManager available in Engine. Call Startup first!");
    }
}

void Engine::RemoveEntity(entity_t entity) {
	m_moduleInterface.m_messages.Send(EntityRemoveSignal{entity});
}

void Engine::SetActiveCamera(entity_t e) {
	auto* renderer = m_moduleInterface.m_interfaces.Query<IRenderer>();
	if (renderer) {
		renderer->SetActiveCamera(e);
	}
}

Engine::~Engine() {
	Shutdown();
	google::ShutdownGoogleLogging();
}

Error Engine::Startup() {
	LOG(INFO) << "Starting Okami Engine";

	m_exitHandler = m_moduleInterface.m_messages.CreateQueue<SignalExit>();

    Error e;
    e += m_ioModules.Register(m_moduleInterface);
    e += m_updateModules.Register(m_moduleInterface);
    e += m_renderModules.Register(m_moduleInterface);
    OKAMI_ERROR_RETURN(e);

    m_entityManager = m_moduleInterface.m_interfaces.Query<IEntityManager>();
	OKAMI_ERROR_RETURN_IF(!m_entityManager, "No IEntityManager registered after registering modules");

    e += m_ioModules.Startup(m_moduleInterface);
    e += m_updateModules.Startup(m_moduleInterface);
    e += m_renderModules.Startup(m_moduleInterface);
    OKAMI_ERROR_RETURN(e);

	return {};
}

void Engine::Shutdown() {
	LOG(INFO) << "Shutting down Okami Engine";

    m_renderModules.Shutdown(m_moduleInterface);
    m_updateModules.Shutdown(m_moduleInterface);
    m_ioModules.Shutdown(m_moduleInterface);
}

struct FrameTimeEstimator {
	size_t m_nextFrame = 0;
	std::chrono::high_resolution_clock::time_point m_startTime;
	std::chrono::high_resolution_clock::time_point m_lastFrameTime;
	double m_nextDelta = 1.0 / 60.0; // Default to 60 FPS
	double m_frameTimeEstimate = 1.0 / 60.0; // Smoothed estimate of frame time
	double m_smoothingFactor = 0.1; // Smoothing factor for frame time estimate

	FrameTimeEstimator() {
		m_startTime = std::chrono::high_resolution_clock::now();
		m_lastFrameTime = m_startTime;
	}

	FrameTimeEstimator Step() const {
		FrameTimeEstimator next = *this;

		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> deltaTime = now - m_lastFrameTime;

		auto lastFrameEstimate = m_frameTimeEstimate;
		auto lastError = deltaTime.count() - lastFrameEstimate;

		next.m_frameTimeEstimate = m_frameTimeEstimate * (1.0 - m_smoothingFactor) + deltaTime.count() * m_smoothingFactor;

		// Adjust next frame time based on the error we had of our last estimate
		next.m_nextDelta = m_frameTimeEstimate + lastError;

		next.m_nextFrame++;
		next.m_lastFrameTime = now;
		return next;
	} 

	Time GetTime() const {
		auto lastFrameTime = std::chrono::duration<double>(m_lastFrameTime - m_startTime).count();
		return Time{
			.m_deltaTime = m_nextDelta,
			.m_nextFrameTime = lastFrameTime + m_nextDelta,
			.m_lastFrameTime = lastFrameTime,
			.m_nextFrame = m_nextFrame
		};
	}
};

void Engine::Run(std::optional<size_t> runFrameCount) {
	m_shouldExit.store(false);

	auto beginTick = std::chrono::high_resolution_clock::now();
	auto lastTick = beginTick;

	std::optional<size_t> maxFrames = runFrameCount;

	FrameTimeEstimator timeEstimator;

	while (!m_shouldExit.load()) {
		auto time = timeEstimator.GetTime();

		// Merge staged changes for this frame
		m_ioModules.Merge();
		m_updateModules.Merge();
		m_renderModules.Merge();

		// Update frame and render
		// Stage changes for next frame
        m_ioModules.ProcessFrame(time, m_moduleInterface);
		m_updateModules.ProcessFrame(time, m_moduleInterface);
        m_renderModules.ProcessFrame(time, m_moduleInterface);

		timeEstimator = timeEstimator.Step();

		if (maxFrames && timeEstimator.m_nextFrame >= *maxFrames) {
			m_shouldExit.store(true);
		}

		while (m_exitHandler && m_exitHandler->GetMessage()) {
			m_shouldExit.store(true);
		}
	}
}

void Engine::AddScript(
	std::function<void(Time const&, ModuleInterface&)> script, 
	std::string_view name) {
	CreateUpdateModule([script = std::move(script), name = std::string{name}]() {
		class ScriptModule : public EngineModule {
		private:
			std::function<void(Time const&, ModuleInterface&)> m_script;
			std::string m_name;
		protected:
			Error RegisterImpl(ModuleInterface&) override { return {}; }
			Error StartupImpl(ModuleInterface&) override { return {}; }
			void ShutdownImpl(ModuleInterface&) override { }

			Error ProcessFrameImpl(Time const& t, ModuleInterface& mi) override {
				m_script(t, mi);
				return {};
			}
			Error MergeImpl() override { return {}; }
		public:
			ScriptModule(
				std::function<void(Time const&, ModuleInterface&)> script,
				std::string name)
				: m_script(std::move(script)), m_name(std::move(name)) {}

			std::string_view GetName() const override {
				return m_name;
			}
		};

		return std::make_unique<ScriptModule>(script, name);
	})->Startup(m_moduleInterface);
}