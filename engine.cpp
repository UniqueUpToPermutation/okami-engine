#include "engine.hpp"

#include "entity_manager.hpp"
#include "config.hpp"

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
}

entity_t Engine::CreateEntity(entity_t parent) {
    if (m_entityManager) {
        return m_entityManager->CreateEntity(parent);
    } else {
        throw std::runtime_error("No IEntityManager available in Engine. Call Startup first!");
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
    if (!m_entityManager) {
        return Error("No IEntityManager found after registering modules");
    }

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

std::filesystem::path Engine::GetRenderOutputPath(size_t frameIndex) {
	return std::filesystem::path("renders/") /
		(std::string{m_params.m_headlessOutputFileStem} + "_" + std::to_string(frameIndex) + ".png");
}

void Engine::Run(std::optional<size_t> runFrameCount) {
	m_shouldExit.store(false);

	auto beginTick = std::chrono::high_resolution_clock::now();
	auto lastTick = beginTick;

	std::optional<size_t> maxFrames = runFrameCount;
	bool headlessMode = m_params.m_headlessMode;

	/*if (!renderer) {
		LOG(WARNING) << "No renderer module found, running headless!";
		headlessMode = true;
	}*/

	if (headlessMode && !maxFrames) {
		maxFrames = 1; // Default to 1 frames in headless mode
		LOG(INFO) << "Running in headless mode, defaulting to " << *maxFrames << " frames.";
	}

	size_t frameCount = 0;

	while (!m_shouldExit.load()) {
		auto now = std::chrono::high_resolution_clock::now();
		std::chrono::duration<double> deltaTime = now - lastTick;
		std::chrono::duration<double> totalTime = now - beginTick;

		Time time{
			.m_deltaTime = deltaTime.count(),
			.m_totalTime = totalTime.count(),
			.m_frame = frameCount
		};

		// Update frame and render
        m_ioModules.ProcessFrame(time, m_moduleInterface);
		m_updateModules.ProcessFrame(time, m_moduleInterface);
        m_renderModules.ProcessFrame(time, m_moduleInterface);

		/*if (m_params.m_headlessMode && renderer) {
			if (!std::filesystem::exists("renders")) {
				std::filesystem::create_directory("renders");
			}

			auto outputFile = GetRenderOutputPath(frameCount);
			LOG(INFO) << "Saving headless frame to: " << outputFile;

			renderer->SaveToFile(outputFile.string());
		}*/

		frameCount++;

		if (maxFrames && frameCount >= *maxFrames) {
			m_shouldExit.store(true);
		}

		while (m_exitHandler && m_exitHandler->GetMessage()) {
			m_shouldExit.store(true);
		}

		lastTick = now;
	}
}