#include "vex.h"
#include <memory>

std::array<ControllerButtonInfo, 12> createControllerButtonArray(const vex::controller &controller)
{
    return {
        ControllerButtonInfo{&controller.ButtonA, "A"},
        ControllerButtonInfo{&controller.ButtonB, "B"},
        ControllerButtonInfo{&controller.ButtonX, "X"},
        ControllerButtonInfo{&controller.ButtonY, "Y"},
        ControllerButtonInfo{&controller.ButtonUp, "Up"},
        ControllerButtonInfo{&controller.ButtonDown, "Down"},
        ControllerButtonInfo{&controller.ButtonLeft, "Left"},
        ControllerButtonInfo{&controller.ButtonRight, "Right"},
        ControllerButtonInfo{&controller.ButtonL1, "L1"},
        ControllerButtonInfo{&controller.ButtonL2, "L2"},
        ControllerButtonInfo{&controller.ButtonR1, "R1"},
        ControllerButtonInfo{&controller.ButtonR2, "R2"}};
}

std::array<ControllerButtonInfo, 12> getControllerButtonArray(const vex::controller &controller)
{
    return createControllerButtonArray(controller);
}

configManager ConfigManager("config.cfg", "maintenance.txt");

// Constructor
configManager::configManager(const std::string &configFileName, const std::string &maintenanceFileName)
    : driveMode(DriveMode::SplitArcade),
      configFileName(configFileName),
      maintenanceFileName(maintenanceFileName),
      maxOptionSize(4),
      logToFile(true),
      POLLINGRATE(5),
      PRINTLOGO(true),
      CTRLR1POLLINGRATE(25),
      logLevel(Log::Level::Info),
      vsyncGif(true),
      odometer(0),
      lastService(0),
      serviceInterval(1000)
{
    readMaintenanceData();
    serviceWarningLogged = false;

    // Initialize triPorts with pointers to Brain.ThreeWirePort
    triPorts["A"] = &Brain.ThreeWirePort.A;
    triPorts["B"] = &Brain.ThreeWirePort.B;
    triPorts["C"] = &Brain.ThreeWirePort.C;
    triPorts["D"] = &Brain.ThreeWirePort.D;
    triPorts["E"] = &Brain.ThreeWirePort.E;
    triPorts["F"] = &Brain.ThreeWirePort.F;
    triPorts["G"] = &Brain.ThreeWirePort.G;
    triPorts["H"] = &Brain.ThreeWirePort.H;
}

vex::triport::port *configManager::getTriPort(const std::string &portName)
{
    auto it = triPorts.find(portName);
    if (it != triPorts.end())
    {
        return it->second;
    }
    else
    {
        resetOrInitializeConfig("Triport not found: " + portName);
        return it->second;
    }
}

// New validation functions for strings
bool configManager::validateStringNotEmpty(const std::string &value)
{
    if (!value.empty())
    {
        return true;
    }
    else
    {
        logHandler("validateStringNotEmpty", "String value cannot be empty", Log::Level::Error, 5);
        return false;
    }
}

// Setters
void configManager::setMaxOptionSize(const std::size_t &value)
{
    if (value < 4)
    {
        maxOptionSize = 4;
        resetOrInitializeConfig("maxOptionSize cannot be lower than 4. Resetting...");
    }

    maxOptionSize = value;
}

void configManager::SetVsyncGif(const bool &value)
{
    vsyncGif = value;
}

void configManager::setLogToFile(const bool &value)
{
    logToFile = value;
}

void configManager::setPollingRate(const std::size_t &value)
{
    POLLINGRATE = value;
}

void configManager::setPrintLogo(const bool &value)
{
    PRINTLOGO = value;
}

void configManager::setCtrlr1PollingRate(const std::size_t &value)
{
    CTRLR1POLLINGRATE = value;
}

void configManager::setLogLevel(const Log::Level &value)
{
    logLevel = value;
}

void configManager::setTeamNumber(const std::string &value)
{
    if (!validateStringNotEmpty(value))
    {
        return;
    }
    if (value.length() > 2)
    {
        resetOrInitializeConfig("Team number cannot be more than 2 digits");
        return;
    }
    teamNumber = value;
}

void configManager::setLoadingGifPath(const std::string &value)
{
    validateStringNotEmpty(value);
    if (value.length() > 20)
    {
        resetOrInitializeConfig("GIF path cannot be more than 20 characters");
        return;
    }

    loadingGifPath = value;
}

void configManager::setAutoGifPath(const std::string &value)
{
    if (!validateStringNotEmpty(value))
    {
        return;
    }
    if (value.length() > 20)
    {
        resetOrInitializeConfig("GIF path cannot be more than 20 characters");
        return;
    }

    autoGifPath = value;
}

void configManager::setDriverGifPath(const std::string &value)
{
    if (!validateStringNotEmpty(value))
    {
        return;
    }
    if (value.length() > 20)
    {
        resetOrInitializeConfig("GIF path cannot be more than 20 characters");
        return;
    }
    driverGifPath = value;
}

int configManager::getMotorPort(const std::string &motorName)
{
    auto it = motorPorts.find(motorName);
    if (it != motorPorts.end())
    {
        return it->second;
    }
    else
    {
        resetOrInitializeConfig("Motor port not found: " + motorName);
        return it->second;
    }
}

std::string configManager::getGearRatio(const std::string &motorName) const
{
    auto it = motorGearRatios.find(motorName);
    if (it != motorGearRatios.end())
    {
        return it->second;
    }
    else
    {
        logHandler("configManager::getGearRatio", "Motor gear ratio not found for: " + motorName + ". Using default ratio 18_1.", Log::Level::Warn, 3);
        return "18_1"; // Default ratio
    }
}

bool configManager::getMotorReversed(const std::string &motorName) const
{
    auto it = motorReversed.find(motorName);
    if (it != motorReversed.end())
    {
        return it->second;
    }
    else
    {
        logHandler("configManager::getMotorReversed", "Motor reversed state not found for: " + motorName + ". Using default state false.", Log::Level::Warn, 3);
        return false; // Default reversed state
    }
}

vex::gearSetting configManager::getGearSetting(const std::string &ratio) const
{
    if (ratio == "6_1")
    {
        return vex::gearSetting::ratio6_1;
    }
    else if (ratio == "18_1")
    {
        return vex::gearSetting::ratio18_1;
    }
    else if (ratio == "36_1")
    {
        return vex::gearSetting::ratio36_1;
    }
    else
    {
        logHandler("configManager::getGearSetting", "Invalid gear ratio: " + ratio + ". Using default ratio 18_1.", Log::Level::Warn, 3);
        return vex::gearSetting::ratio18_1; // Default
    }
}

void configManager::updateOdometer(const int &averagePosition)
{
    odometer += averagePosition;
    // Removed unused writeThreshold and accumulatedDistance.

    if (odometer - lastService >= serviceInterval && !serviceWarningLogged)
    {
        logHandler("Service", "Service needed! Distance: " + std::to_string(odometer), Log::Level::Warn, 5);
        serviceWarningLogged = true;
    }
    else if (odometer - lastService < serviceInterval)
    {
        serviceWarningLogged = false;
    }
}

void configManager::checkServiceInterval()
{
    if (odometer - lastService >= serviceInterval)
    {
        logHandler("Service", "Service needed! Distance: " + std::to_string(odometer), Log::Level::Warn, 5);
        lastService = odometer;
    }
}

configManager::ConfigType configManager::stringToConfigType(const std::string &str)
{
    switch (str[0])
    {
    case 'B':
        if (str == "Brain")
            return ConfigType::Brain;
        break;
    case 'C':
        if (str == "Controller")
            return ConfigType::Controller;
        break;
    default:
        logHandler("configManager::stringToConfigType", "Invalid config type", Log::Level::Error, 5);
        return ConfigType::Brain; // Default return to avoid compilation error
    }
    logHandler("configManager::stringToConfigType", "Invalid config type", Log::Level::Error, 5);
    return ConfigType::Brain; // Default return to avoid compilation error
}

Log::Level configManager::stringToLogLevel(const std::string &str)
{
    switch (str[0])
    {
    case 'T':
        return Log::Level::Trace;
    case 'D':
        return Log::Level::Debug;
    case 'I':
        return Log::Level::Info;
    case 'W':
        return Log::Level::Warn;
    case 'E':
        return Log::Level::Error;
    case 'F':
        return Log::Level::Fatal;
    default:
        logHandler("configManager::stringToLogLevel", "Invalid log level", Log::Level::Error, 5);
        return Log::Level::Info; // Default return to avoid compilation error
    }
}

// VEX API helper functions
std::string configManager::readFileToString(const std::string &filename)
{
    if (!Brain.SDcard.isInserted())
    {
        logHandler("readFileToString", "SD card not inserted", Log::Level::Error);
        return "";
    }
    
    if (!Brain.SDcard.exists(filename.c_str()))
    {
        logHandler("readFileToString", "File does not exist: " + filename, Log::Level::Warn);
        return "";
    }
    
    int size = Brain.SDcard.size(filename.c_str());
    if (size <= 0)
    {
        logHandler("readFileToString", "File is empty or invalid size: " + filename, Log::Level::Warn);
        return "";
    }
    
    // Allocate buffer for file content
    std::unique_ptr<char[]> buffer(new char[size + 1]);
    if (!buffer)
    {
        logHandler("readFileToString", "Failed to allocate buffer for file: " + filename, Log::Level::Error);
        return "";
    }
    
    // Load file into buffer
    int bytesRead = Brain.SDcard.loadfile(filename.c_str(), buffer.get(), size);
    if (bytesRead <= 0)
    {
        logHandler("readFileToString", "Failed to read file: " + filename, Log::Level::Error);
        return "";
    }
    
    // Null terminate the buffer
    buffer[bytesRead] = '\0';
    
    return std::string(buffer.get());
}

bool configManager::writeStringToFile(const std::string &filename, const std::string &content, bool append)
{
    if (!Brain.SDcard.isInserted())
    {
        logHandler("writeStringToFile", "SD card not inserted", Log::Level::Error);
        return false;
    }
    
    int bytesWritten = 0;
    if (append)
    {
        bytesWritten = Brain.SDcard.appendfile(filename.c_str(), const_cast<char*>(content.c_str()), content.length());
    }
    else
    {
        bytesWritten = Brain.SDcard.savefile(filename.c_str(), const_cast<char*>(content.c_str()), content.length());
    }
    
    if (bytesWritten != static_cast<int>(content.length()))
    {
        logHandler("writeStringToFile", "Failed to write complete content to file: " + filename, Log::Level::Error);
        return false;
    }
    
    return true;
}

bool configManager::fileExists(const std::string &filename)
{
    if (!Brain.SDcard.isInserted())
    {
        return false;
    }
    
    return Brain.SDcard.exists(filename.c_str());
}

int configManager::getFileSize(const std::string &filename)
{
    if (!Brain.SDcard.isInserted())
    {
        return -1;
    }
    
    if (!Brain.SDcard.exists(filename.c_str()))
    {
        return -1;
    }
    
    return Brain.SDcard.size(filename.c_str());
}