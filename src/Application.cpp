#include "Application.h"
#include <QDir>

#ifdef Q_OS_WIN32
#include "Windows.h"
#endif

#include "utils/AppPaths.h"
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

	Logger::instance().setLogPath(AppPaths::logFile());

	// 各平台的数据目录语义不同（macOS 走 Application Support），排查线上问题时
	// 必须能从日志里看到实际生效的路径
	LOG_INFO(QString("Data dir: %1").arg(AppPaths::dataDir()));
	LOG_INFO(QString("Resource dir: %1").arg(AppPaths::resourceDir()));

	ConfigManager::setConfigFilePath(AppPaths::configFile());

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