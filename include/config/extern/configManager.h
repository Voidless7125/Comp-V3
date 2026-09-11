#include <map>
#include <string>
#include <cstdint>

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
    int getMotorBackupPort(const std::string &motorName) const; // -1 if no backup port configured
    vex::triport::port *getTriPort(const std::string &portName);

    void updateOdometer(const int &deltaPosition);
    void checkServiceInterval();

    // Per-motor accumulated runtime (in encoder degrees), tracked separately
    // from the chassis odometer so you can tell which specific motor is
    // closest to end-of-life rather than just "the robot" in general.
    void updateMotorRuntime(MotorRole role, double deltaDegrees);
    long getMotorRuntimeDeg(MotorRole role) const;

    // updateOdometer()/updateMotorRuntime() above only update in-memory
    // values now (they used to each write to the SD card individually,
    // which meant one monitoring tick could trigger 5 separate writes).
    // Call this once after a batch of updates to persist everything in a
    // single write.
    void persistMaintenanceData();

    // Tamper protection: true if maintenance.txt's stored checksum didn't
    // match its contents on load (i.e. the file was likely hand-edited).
    bool isOdometerTamperDetected() const { return odometerTamperFlag; }

    ConfigType stringToConfigType(const std::string &str);
    Log::Level stringToLogLevel(const std::string &str);

    int getLeftDeadzone() const { return leftDeadzone; }
    void setLeftDeadzone(int value) { leftDeadzone = value; }

    int getRightDeadzone() const { return rightDeadzone; }
    void setRightDeadzone(int value) { rightDeadzone = value; }

private:
    std::map<std::string, int> motorPorts;
    std::map<std::string, int> motorBackupPorts; // optional; absent = no backup configured
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

    bool odometerTamperFlag = false;
    long motorRuntimeDeg[4] = {0, 0, 0, 0}; // indexed via roleIndexOf(MotorRole)
    static std::uint32_t computeMaintenanceChecksum(int odo, int lastSvc, int svcInterval, const long (&motorDeg)[4]);

    void readMaintenanceData();
    void writeMaintenanceData();
};

/// @brief Manages configuration settings.
extern configManager ConfigManager;