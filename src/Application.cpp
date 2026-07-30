#include "Application.h"
#include <QDir>

#ifdef Q_OS_WIN32
#include "Windows.h"
#endif

#include "utils/Logger.h"
#include "ConfigManager.h"
#include "widgets/inforbar/inforbarmanager.h"
#include "widgets/inforbar/inforposmanager.h"

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


	InforBarManager::registerManager<TopInforBarManager>(InforBarPosition::I_TOP);
	InforBarManager::registerManager<TopRightInfoBarManager>(InforBarPosition::I_TOP_RIGHT);
	InforBarManager::registerManager<BottomRightInfoBarManager>(InforBarPosition::I_BOTTOM_RIGHT);
	InforBarManager::registerManager<TopLeftInfoBarManager>(InforBarPosition::I_TOP_LEFT);
	InforBarManager::registerManager<BottomLeftInfoBarManager>(InforBarPosition::I_BOTTOM_LEFT);
	InforBarManager::registerManager<BottomInfoBarManager>(InforBarPosition::I_BOTTOM);
}


void Application::quit() {

}