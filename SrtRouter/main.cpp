#include "srtRouter.h"
#include "SrtSocket.h"
#include "logger.h"
#include "configure.h"
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <spdlog/fmt/fmt.h>
#include "util.h"

static std::atomic<int> s_Quit{ 0 };

#ifdef _WIN32
#include <windows.h>
#include <csignal>

BOOL WINAPI console_handler(DWORD fdwCtrlType) {
	switch(fdwCtrlType) { 
	case CTRL_C_EVENT: 
	case CTRL_CLOSE_EVENT: 
	case CTRL_BREAK_EVENT: 
	case CTRL_LOGOFF_EVENT: 
	case CTRL_SHUTDOWN_EVENT: 
		s_Quit = 1;
		return TRUE;
	default: 
		return FALSE; 
	} 
}

void abortHandler(int signal) {
	RaiseException(0, 0, 0, NULL);
}

#else
#include <signal.h>
void console_handler(int sig) {
	switch (sig) {
	case SIGINT:
	case SIGQUIT:
	case SIGTERM:
	case SIGPWR:
		s_Quit = 1;
		break;
	}
	if (SIGSEGV == sig || SIGABRT == sig) {
		//signal(sig, SIG_DFL);
		_exit(128 + sig);
	}
}
#endif 

int main(int argc, char *argv[]) {
	SrtRouterConfigure initConfig;
	const std::string configFilePath = "srtrouterconfig.json";
	if (!initConfig.loadFromFile(configFilePath)) {
		Logger::init("logs/srtRouter.log", 10485760, 30, "info");
		Logger::logPrint(LogWarn, "Failed to load {}, using default settings", configFilePath);
	}
	SrtRouterConfigurePtr config = std::make_shared<SharedConfig>(initConfig);
	const auto& cfg = config->get();

	Logger::init(
		cfg.logging.logFilePath,
		cfg.logging.maxLogSize,
		cfg.logging.logKeepDays,
		cfg.logging.logLevel
	);

	Logger::logPrint(LogInfo, "SrtRouter starting...");
	Logger::logPrint(LogInfo, "Max streams limit: {}", cfg.maxStreamsLimit);

	// Create and start SrtRouter
	int apiPort = cfg.api.listenPort;
	int srtPort = cfg.srt.listenPort;

	SrtRouter router(config, configFilePath);

	if (cfg.api.enable) {
		int ret = router.startApiServer("0.0.0.0", apiPort, 128);

		if (ret == 0) {
			Logger::logPrint(LogInfo, "API server started successfully on port {}", apiPort);
		} else {
			Logger::logPrint(LogError, "Failed to start API server, ret={}", ret);
		}
	} else {
		Logger::logPrint(LogInfo, "API server is disabled in config");
	}

	if (cfg.srt.enable) {
		int ret = router.startSrtServer("0.0.0.0", srtPort);

		if (ret == 0) {
			Logger::logPrint(LogInfo, "SRT router started successfully on port {}", srtPort);
		} else {
			Logger::logPrint(LogError, "Failed to start SRT router, ret={}", ret);
		}
	} else {
		Logger::logPrint(LogInfo, "SRT router is disabled in config");
	}

#ifdef _WIN32
	SetConsoleCtrlHandler(console_handler, TRUE);
	signal(SIGABRT, abortHandler);
#else
	struct sigaction sa;
	sa.sa_handler = console_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;

	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGPWR, &sa, NULL);
	sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);

	signal(SIGHUP, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGPIPE, SIG_IGN);
#endif

	while (!s_Quit) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	Logger::logPrint(LogInfo, "SRT router stopping...");
	//router.stop();
	Logger::shutdown();
	return 0;
}