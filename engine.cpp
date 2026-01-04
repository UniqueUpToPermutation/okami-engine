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
    m_params(params) {
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

    CreateModule(EntityManagerFactory{});
    CreateModule(ConfigModuleFactory{});
	CreateModule(PhysicsModuleFactory{});

	CreateModule(TextureIOModuleFactory{});
	CreateModule(GeometryIOModuleFactory{});
}

entity_t Engine::CreateEntity(entity_t parent) {
    if (m_entityManager) {
		auto port = m_messages.GetPortOut<EntityCreateSignal>();
        return m_entityManager->CreateEntity(port, parent);
    } else {
        throw std::runtime_error("No IEntityManager available in Engine. Call Startup first!");
    }
}

void Engine::RemoveEntity(entity_t entity) {
	m_messages.Send(EntityRemoveMessage{entity});
}

void Engine::SetActiveCamera(entity_t e) {
	m_interfaces.ForEachInterface<IRenderModule>([e](IRenderModule* renderer) {
		renderer->SetActiveCamera(e);
	});
}

Engine::~Engine() {
	Shutdown();
	google::ShutdownGoogleLogging();
}

InitContext Engine::GetInitContext() {
	return InitContext{
		.m_messages = m_messages,
		.m_interfaces = m_interfaces
	};
}

Error Engine::Startup() {
	LOG(INFO) << "Starting Okami Engine";

	m_interfaces.RegisterSignalHandler<SignalExit>(&m_exitHandler);

	auto initContext = GetInitContext();

    Error e;
    e += m_modules.Register(m_interfaces);
    OKAMI_ERROR_RETURN(e);

    m_entityManager = m_interfaces.Query<IEntityManager>();
	OKAMI_ERROR_RETURN_IF(!m_entityManager, "No IEntityManager registered after registering modules");

    e += m_modules.Startup(initContext);
    OKAMI_ERROR_RETURN(e);

	return {};
}

void Engine::Shutdown() {
	LOG(INFO) << "Shutting down Okami Engine";

	auto initContext = GetInitContext();

    m_modules.Shutdown(initContext);
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
	bool shouldExit = false;

	auto beginTick = std::chrono::high_resolution_clock::now();
	auto lastTick = beginTick;

	std::optional<size_t> maxFrames = runFrameCount;

	auto processIO = [&]() {
		Error err;
		m_interfaces.ForEachInterface<IIOModule>([&](IIOModule* ioModule) {
			err += ioModule->IOProcess(m_interfaces);
		});
		return err;
	};

	auto processGUI = [&]() {
		Error err;
		m_interfaces.ForEachInterface<IGUIModule>([&](IGUIModule* guiModule) {
			err += guiModule->MessagePump(m_interfaces);
		});
		return err;
	};

	auto render = [&]() {
		Error err;
		m_interfaces.ForEachInterface<IRenderModule>([&](IRenderModule* renderModule) {
			err += renderModule->Render();
		});
		return err;
	};

	// IO modules process first to stage any initial data
	processIO();

	// Modules receive any initial messages
	m_modules.ReceiveMessages(m_messages);

	FrameTimeEstimator timeEstimator;
	DefaultJobGraphExecutor executor;

	while (!shouldExit) {
		auto time = timeEstimator.GetTime();

		// Clear message bus for this frame
		m_messages.Clear();

       	processIO();
		render();

		// Execute the update job graph
		JobGraph updateJobGraph;
		m_modules.BuildGraph(updateJobGraph);

		// Send messages for this frame
		m_messages.Send(time);
		m_modules.SendMessages(m_messages);

		// Run GUI message pump if any
		processGUI();

		// Run the message processing graph for this frame
		executor.Execute(updateJobGraph, m_messages);

		// Receive messages after update, commit staged object changes
		m_modules.ReceiveMessages(m_messages);

		// Frame timing
		timeEstimator = timeEstimator.Step();

		// Check for exit conditions
		if (maxFrames && timeEstimator.m_nextFrame >= *maxFrames) {
			shouldExit = true;
		}

		if (m_exitHandler.FetchAndReset() > 0) {
			shouldExit = true;
		}

		m_messages.Handle<MessageExit>([&shouldExit](MessageExit const&) { 
			shouldExit = true; 
		});
	}
}

void Engine::AddScriptBundle(
	std::function<void(JobGraph&, BuildGraphParams const&)> script, 
	std::string_view name) {
	CreateModule([script = std::move(script), name = std::string{name}]() {
		class ScriptModule : public EngineModule {
		private:
			std::function<void(JobGraph&, BuildGraphParams const&)> m_script;
			std::string m_name;
		protected:
			Error RegisterImpl(InterfaceCollection&) override { return {}; }
			Error StartupImpl(InitContext const&) override { return {}; }
			void ShutdownImpl(InitContext const&) override { }

			Error BuildGraphImpl(JobGraph& graph, BuildGraphParams const& params) override {
				m_script(graph, params);
				return {};
			}

		public:
			ScriptModule(
				std::function<void(JobGraph&, BuildGraphParams const&)> script,
				std::string name)
				: m_script(std::move(script)), m_name(std::move(name)) {}

			std::string GetName() const override {
				return m_name;
			}
		};

		return std::make_unique<ScriptModule>(script, name);
	})->Startup(GetInitContext());
}