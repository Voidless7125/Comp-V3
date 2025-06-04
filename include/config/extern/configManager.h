#include <map>
#include <string>

/**
 * @class Log
 * @brief Handles logging with different severity levels.
 * @details Provides an enumeration for log levels.
 */
class Log
{
public:
    /**
     * @enum Level
     * @brief Defines the severity levels for logging.
     */
    enum class Level
    {
        Trace, ///< Detailed information.
        Debug, ///< Debug-level messages.
        Info,  ///< Informational messages that highlight the progress of the application.
        Warn,  ///< Potentially harmful situations.
        Error, ///< Error events that still allow the application to continue running.
        Fatal  ///< Very severe error events that will lead the application to abort.
    };
};

struct ControllerButtonInfo
{
    const vex::controller::button *button;
    std::string name;
};

extern std::array<ControllerButtonInfo, 12> AllControllerButtons;

std::array<ControllerButtonInfo, 12> createControllerButtonArray(const vex::controller &controller);
std::array<ControllerButtonInfo, 12> getControllerButtonArray(const vex::controller &controller);

class configManager
{
public:
    configManager(const std::string &configFileName, const std::string &maintenanceFileName);
    void resetOrInitializeConfig(std::string_view message);
    bool stringToBool(std::string_view str);
    template <typename T>
    T stringToNumber(std::string_view str);
    void setValuesFromConfig();
    bool validateStringNotEmpty(const std::string &value);
    void parseConfig();
    void parseComplexConfig(std::ifstream &configFile, const std::string &section);

    enum class DriveMode
    {
        LeftArcade,
        RightArcade,
        SplitArcade,
        Tank
    };

    enum class ConfigType
    {
        Brain,
        Controller
    };

    ConfigType configType;
    DriveMode driveMode;

    std::size_t getMaxOptionSize() const { return maxOptionSize; }
    bool getLogToFile() const { return logToFile; }
    std::size_t getPollingRate() const { return POLLINGRATE; }
    bool getPrintLogo() const { return PRINTLOGO; }
    std::size_t getCtrlr1PollingRate() const { return CTRLR1POLLINGRATE; }
    Log::Level getLogLevel() const { return logLevel; }
    std::string getTeamNumber() const { return teamNumber; };
    int getOdometer() const { return odometer; }
    int getLastService() const { return lastService; }
    int getServiceInterval() const { return serviceInterval; }
    DriveMode getDriveMode() const { return driveMode; };
    bool getVsyncGif() const { return vsyncGif; }

    void setMaxOptionSize(const std::size_t &value);
    void setLogToFile(const bool &value);
    void setPollingRate(const std::size_t &value);
    void setPrintLogo(const bool &value);
    void setCtrlr1PollingRate(const std::size_t &value);
    void setLogLevel(const Log::Level &value);

    void setTeamNumber(const std::string &value);
    void setLoadingGifPath(const std::string &value);
    void setAutoGifPath(const std::string &value);
    void setDriverGifPath(const std::string &value);
    void setDriveMode(const DriveMode &mode);
    void SetVsyncGif(const bool &value);

    std::string getGearRatio(const std::string &motorName) const;
    bool getMotorReversed(const std::string &motorName) const;
    vex::gearSetting getGearSetting(const std::string &ratio) const;
    int getMotorPort(const std::string &motorName);
    vex::triport::port *getTriPort(const std::string &portName);

    void updateOdometer(const int &averagePosition);
    void checkServiceInterval();

    ConfigType stringToConfigType(const std::string &str);
    Log::Level stringToLogLevel(const std::string &str);

    int getLeftDeadzone() const { return leftDeadzone; }
    void setLeftDeadzone(int value) { leftDeadzone = value; }

    int getRightDeadzone() const { return rightDeadzone; }
    void setRightDeadzone(int value) { rightDeadzone = value; }

private:
    std::map<std::string, int> motorPorts;
    std::map<std::string, std::string> motorGearRatios;
    std::map<std::string, bool> motorReversed;
    std::map<std::string, vex::triport::port *> triPorts;
    std::map<std::string, int> inertialPorts;

    std::string configFileName;
    std::string maintenanceFileName;
    std::size_t maxOptionSize;
    bool logToFile;
    std::size_t POLLINGRATE;
    bool PRINTLOGO;
    std::size_t CTRLR1POLLINGRATE;
    Log::Level logLevel;
    bool vsyncGif;

    std::string teamNumber;
    std::string loadingGifPath;
    std::string autoGifPath;
    std::string driverGifPath;
    std::string customMessage;

    int odometer;
    int lastService;
    int serviceInterval;
    bool serviceWarningLogged;
    int leftDeadzone;
    int rightDeadzone;

    void readMaintenanceData();
    void writeMaintenanceData();
    
    // VEX API helper functions
    std::string readFileToString(const std::string &filename);
    bool writeStringToFile(const std::string &filename, const std::string &content, bool append = false);
    bool fileExists(const std::string &filename);
    int getFileSize(const std::string &filename);
};

/// @brief Manages configuration settings.
extern configManager ConfigManager;