#include "Application.h"
#include <QDir>

#ifdef Q_OS_WIN32
#include "Windows.h"
#endif

#include "utils/Logger.h"
#include "ConfigManager.h"

Application::Application(int& argc, char** argv):QApplication(argc, argv) {
	Initialize();
	connect(this, &QApplication::aboutToQuit, this, &Application::quit);

}

void Application::Initialize() {
#ifdef Q_OS_WIN32
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

	QString logPath = QApplication::applicationDirPath() + QDir::separator() + "voice_ime.log";
	//QString logPath =QDir::temp().dirName() + QDir::separator() + "app.log";
	Logger::instance().setLogPath(logPath);	

	QString configPath = QApplication::applicationDirPath() + QDir::separator() + "voice_ime.ini";
	ConfigManager::setConfigFilePath(configPath);

	auto& configManager = ConfigManager::instance();
	if (!QFile::exists(configManager.configFilePath())) {
		configManager.applyDefaults();
		configManager.save();
	}
}


void Application::quit() {

}