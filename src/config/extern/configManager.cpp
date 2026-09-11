#include "vex.h"
#include <fstream>

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
    // Previous behaviour dereferenced `it` here even though it == end()
    // (undefined behaviour). Log and return an explicit, safe default
    // instead of crashing/reading garbage.
    logHandler("configManager::getMotorPort", "Motor port not found for: " + motorName + ". Defaulting to port 1 - check config.cfg!", Log::Level::Error, 5);
    return 1;
}

int configManager::getMotorBackupPort(const std::string &motorName) const
{
    auto it = motorBackupPorts.find(motorName);
    return (it != motorBackupPorts.end()) ? it->second : -1;
}

namespace
{
    int roleIndexOf(MotorRole role)
    {
        switch (role)
        {
        case MotorRole::FrontLeft:
            return 0;
        case MotorRole::FrontRight:
            return 1;
        case MotorRole::RearLeft:
            return 2;
        case MotorRole::RearRight:
            return 3;
        }
        return 0;
    }
} // namespace

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

void configManager::updateOdometer(const int &deltaPosition)
{
    odometer += deltaPosition;
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

    // NOTE: this used to call writeMaintenanceData() directly, which meant
    // every updateOdometer()/updateMotorRuntime() call in a monitoring tick
    // triggered its own SD write. Callers now batch updates and call
    // persistMaintenanceData() once - see motorMonitor() in functions.cpp.
}

void configManager::updateMotorRuntime(MotorRole role, double deltaDegrees)
{
    motorRuntimeDeg[roleIndexOf(role)] += static_cast<long>(deltaDegrees);
}

long configManager::getMotorRuntimeDeg(MotorRole role) const
{
    return motorRuntimeDeg[roleIndexOf(role)];
}

void configManager::persistMaintenanceData()
{
    writeMaintenanceData();
}

void configManager::checkServiceInterval()
{
    if (odometer - lastService >= serviceInterval)
    {
        logHandler("Service", "Service needed! Distance: " + std::to_string(odometer), Log::Level::Warn, 5);
        lastService = odometer;
        writeMaintenanceData();
    }
}

/**
 * @brief Computes a lightweight tamper-detection checksum over the
 * maintenance values (odometer, service tracking, and per-motor runtime).
 *
 * @note This is deliberately simple (FNV-1a over the values plus a fixed
 * salt) rather than a cryptographic MAC - there's no crypto library in this
 * toolchain, and the realistic threat model here is a student opening
 * maintenance.txt in a text editor and zeroing the odometer, not a
 * determined adversary with a disassembler. It stops the former; it will
 * not stop the latter. Treat it as tamper-evidence, not tamper-proofing.
 */
std::uint32_t configManager::computeMaintenanceChecksum(int odo, int lastSvc, int svcInterval, const long (&motorDeg)[4])
{
    constexpr std::uint32_t salt = 0xA53C91F7u;
    std::string data = std::to_string(odo) + ":" + std::to_string(lastSvc) + ":" + std::to_string(svcInterval) +
                        ":" + std::to_string(motorDeg[0]) + ":" + std::to_string(motorDeg[1]) +
                        ":" + std::to_string(motorDeg[2]) + ":" + std::to_string(motorDeg[3]);

    std::uint32_t hash = salt;
    for (unsigned char c : data)
    {
        hash ^= c;
        hash *= 16777619u; // FNV-1a prime
    }
    return hash;
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