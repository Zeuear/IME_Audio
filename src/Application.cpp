#include "Application.h"
#include <QDir>

#ifdef Q_OS_WIN32
#include "Windows.h"
#endif

#include "utils/AppPaths.h"
#include "utils/Logger.h"
#include "utils/Diagnostics.h"
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
	Diagnostics::install();
	Diagnostics::logEnvironment();


	LOG_DEBUG(QString("Path | data dir : %1").arg(AppPaths::dataDir()));
	LOG_DEBUG(QString("Path | resources: %1").arg(AppPaths::resourceDir()));
	LOG_DEBUG(QString("Path | config   : %1").arg(AppPaths::configFile()));
	LOG_DEBUG(QString("Path | terms    : %1").arg(AppPaths::termsFile()));
	LOG_DEBUG(QString("Path | prompts  : %1").arg(AppPaths::promptsDir()));
	LOG_DEBUG(QString("Path | sherpa   : %1").arg(AppPaths::sherpaRoot()));
	LOG_DEBUG(QString("Path | vad model: %1").arg(AppPaths::vadModelFile()));

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