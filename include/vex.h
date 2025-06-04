#include <map>

#include "v5_cpp.h"

#include "config/robot-config.h"
#include "config/extern/configManager.h"

#include "display/gifdec.h"

extern std::string Version;
extern std::string BuildDate;

void logHandler(const std::string &functionName, const std::string &message, const Log::Level level, const float &timeOfDisplay = 2);
void SD_Card_Logging(const Log::Level &level, const std::string &functionName, const std::string &message);
std::string getUserOption(const std::string &settingName, const std::vector<std::string> &options);
void calibrateGyro();
void autonomous();
void userControl();
void motorMonitor();
std::map<std::string, std::vector<int>> controllerButtonsPressed(const vex::controller &controller);
void gifplayer(bool enableVsync = false);