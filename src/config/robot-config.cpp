#include "vex.h"
#include <array>
#include <algorithm>
#include <cstdint>

vex::brain Brain;

std::unique_ptr<vex::motor> frontLeftMotor;
std::unique_ptr<vex::motor> rearLeftMotor;
std::unique_ptr<vex::motor_group> LeftDriveSmart;

std::unique_ptr<vex::motor> frontRightMotor;
std::unique_ptr<vex::motor> rearRightMotor;
std::unique_ptr<vex::motor_group> RightDriveSmart;

std::unique_ptr<vex::smartdrive> Drivetrain;

vex::controller primaryController = vex::controller(vex::controllerType::primary);
vex::controller partnerController = vex::controller(vex::controllerType::partner);

std::unique_ptr<vex::inertial> InertialGyro;
std::unique_ptr<vex::bumper> RearBumper;

vex::competition Competition;

namespace
{
    // Tracks which physical port each drive-motor role is currently bound
    // to, and which (optional) backup port it can fail over to.
    struct RoleBinding
    {
        std::string configKey;
        int currentPort = -1;
        int backupPort = -1; // -1 = no backup configured
        bool onBackup = false;
    };

    RoleBinding flBinding{"FRONT_LEFT_MOTOR"};
    RoleBinding frBinding{"FRONT_RIGHT_MOTOR"};
    RoleBinding rlBinding{"REAR_LEFT_MOTOR"};
    RoleBinding rrBinding{"REAR_RIGHT_MOTOR"};

    RoleBinding &bindingFor(MotorRole role)
    {
        switch (role)
        {
        case MotorRole::FrontLeft:
            return flBinding;
        case MotorRole::FrontRight:
            return frBinding;
        case MotorRole::RearLeft:
            return rlBinding;
        case MotorRole::RearRight:
            return rrBinding;
        }
        return flBinding;
    }

    std::unique_ptr<vex::motor> &motorFor(MotorRole role)
    {
        switch (role)
        {
        case MotorRole::FrontLeft:
            return frontLeftMotor;
        case MotorRole::FrontRight:
            return frontRightMotor;
        case MotorRole::RearLeft:
            return rearLeftMotor;
        case MotorRole::RearRight:
            return rearRightMotor;
        }
        return frontLeftMotor;
    }

    std::unique_ptr<vex::motor> makeConfiguredMotor(RoleBinding &binding)
    {
        binding.currentPort = ConfigManager.getMotorPort(binding.configKey);
        binding.backupPort = ConfigManager.getMotorBackupPort(binding.configKey);
        binding.onBackup = false;

        auto gear = ConfigManager.getGearSetting(ConfigManager.getGearRatio(binding.configKey));
        bool reversed = ConfigManager.getMotorReversed(binding.configKey);
        return std::make_unique<vex::motor>(binding.currentPort, gear, reversed);
    }
} // namespace

void rebuildDriveGroups()
{
    LeftDriveSmart = std::make_unique<vex::motor_group>(*frontLeftMotor, *rearLeftMotor);
    RightDriveSmart = std::make_unique<vex::motor_group>(*frontRightMotor, *rearRightMotor);
    Drivetrain = std::make_unique<vex::smartdrive>(*LeftDriveSmart, *RightDriveSmart, *InertialGyro,
                                                    319.19, 320, 165, vex::distanceUnits::mm, 1);
    Drivetrain->setStopping(vex::brakeType::coast);
    // Safety net for the autonomous primitives (driveStraightMm/turnToHeadingDeg/
    // turnByDeg): without a timeout, a stalled/jammed drive would hang
    // driveFor()/turnFor() forever waiting to reach a position it can't.
    Drivetrain->setTimeout(3, vex::timeUnits::sec); // TUNE ME on hardware
}

void constructRobotHardware()
{
    frontLeftMotor = makeConfiguredMotor(flBinding);
    frontRightMotor = makeConfiguredMotor(frBinding);
    rearLeftMotor = makeConfiguredMotor(rlBinding);
    rearRightMotor = makeConfiguredMotor(rrBinding);

    InertialGyro = std::make_unique<vex::inertial>(vex::PORT3);

    auto *bumperPort = ConfigManager.getTriPort("REAR_BUMPER");
    RearBumper = std::make_unique<vex::bumper>(*bumperPort);

    rebuildDriveGroups();

    // These used to run at the tail of ConfigManager::parseConfig(), but
    // they need InertialGyro/Drivetrain to exist first, so they moved here.
    calibrateGyro();
    gifplayer(ConfigManager.getVsyncGif());

    // ConfigManager's constructor (static-init time, before Brain/
    // primaryController necessarily exist) already checked the maintenance
    // file's checksum and just set a flag if it didn't match - it couldn't
    // safely call logHandler() from there. This is the first point after
    // main() starts (i.e. after every global is guaranteed constructed)
    // where it's safe to actually surface that warning.
    if (ConfigManager.isOdometerTamperDetected())
    {
        logHandler("startup",
                   "maintenance.txt checksum mismatch - odometer/service/runtime values appear to have been edited by hand. Treating service as already due.",
                   Log::Level::Warn, 6);
    }
}

bool checkAndHotSwapMotor(MotorRole role)
{
    RoleBinding &binding = bindingFor(role);
    auto &motorPtr = motorFor(role);

    if (motorPtr && motorPtr->installed())
    {
        return false; // primary is healthy
    }

    if (binding.backupPort <= 0 || binding.onBackup)
    {
        return false; // no backup configured, or already failed over once
    }

    // Probe the backup port without disturbing anything else.
    vex::motor probe(binding.backupPort);
    if (!probe.installed())
    {
        return false; // backup not plugged in (yet)
    }

    auto gear = ConfigManager.getGearSetting(ConfigManager.getGearRatio(binding.configKey));
    bool reversed = ConfigManager.getMotorReversed(binding.configKey);
    motorPtr = std::make_unique<vex::motor>(binding.backupPort, gear, reversed);
    binding.onBackup = true;
    rebuildDriveGroups();

    logHandler("hotSwap",
               std::format("{} stopped responding on port {} - switched to backup port {}.",
                            binding.configKey, binding.currentPort, binding.backupPort),
               Log::Level::Warn, 5);
    blackBoxLogEvent(std::format("hotswap-auto: {} -> backup port {}", binding.configKey, binding.backupPort));
    return true;
}

bool learnNewPortForRole(MotorRole role, vex::controller &controllerRef, int timeoutMs)
{
    RoleBinding &binding = bindingFor(role);
    auto &motorPtr = motorFor(role);

    controllerRef.Screen.clearScreen();
    controllerRef.Screen.setCursor(1, 1);
    controllerRef.Screen.print("%s:", binding.configKey.c_str());
    controllerRef.Screen.setCursor(2, 1);
    controllerRef.Screen.print("Plug into a free port...");

    // Snapshot which ports currently have a motor, so we know which one is new.
    std::array<bool, 21> wasInstalled{};
    for (int p = 1; p <= 21; ++p)
    {
        wasInstalled[p - 1] = vex::motor(p).installed();
    }

    // vex::timer::time() returns uint32_t; compare like-for-like to avoid a
    // signed/unsigned warning (timeoutMs is otherwise a plain "milliseconds"
    // int for a nicer public API).
    const std::uint32_t timeoutMsU = static_cast<std::uint32_t>(std::max(timeoutMs, 0));
    vex::timer t;
    while (t.time() < timeoutMsU)
    {
        for (int p = 1; p <= 21; ++p)
        {
            if (wasInstalled[p - 1])
            {
                continue;
            }
            if (vex::motor(p).installed())
            {
                auto gear = ConfigManager.getGearSetting(ConfigManager.getGearRatio(binding.configKey));
                bool reversed = ConfigManager.getMotorReversed(binding.configKey);
                motorPtr = std::make_unique<vex::motor>(p, gear, reversed);
                binding.currentPort = p;
                binding.onBackup = false;
                rebuildDriveGroups();

                controllerRef.Screen.clearScreen();
                controllerRef.Screen.setCursor(1, 1);
                controllerRef.Screen.print("%s -> Port %d", binding.configKey.c_str(), p);
                logHandler("hotSwap", std::format("{} manually remapped to port {}.", binding.configKey, p), Log::Level::Info, 3);
                blackBoxLogEvent(std::format("hotswap-manual: {} -> port {}", binding.configKey, p));
                vex::this_thread::sleep_for(1500);
                return true;
            }
        }
        vex::this_thread::sleep_for(50);
    }

    controllerRef.Screen.clearScreen();
    controllerRef.Screen.setCursor(1, 1);
    controllerRef.Screen.print("No new motor found.");
    vex::this_thread::sleep_for(1500);
    return false;
}

/**
 * Check if the Y button is held at startup to enter diagnostic mode.
 */
bool isDiagnosticMode()
{
    auto buttonPressTimes = controllerButtonsPressed(primaryController);
    if (buttonPressTimes.find("Y") != buttonPressTimes.end())
    {
        for (auto duration : buttonPressTimes["Y"])
        {
            if (duration >= 1000)
            {
                return true;
            }
        }
    }
    return false;
}

/**
 * Initialize diagnostic mode.
 */
void initializeDiagnosticMode()
{
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("Entering Diagnostic Mode...");

    // Add any additional diagnostic initialization here

    // Run the display task for diagnostic mode
    displayTask();
}

/**
 * Used to initialize code/tasks/devices added using tools in VEXcode Pro.
 *
 * This should be called at the start of your int main function, AFTER
 * ConfigManager.parseConfig() and constructRobotHardware() have both run.
 */
void vexCodeInit()
{

    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    partnerController.Screen.clearScreen();
    partnerController.Screen.setCursor(1, 1);

    primaryController.Screen.print("Starting up...");
    partnerController.Screen.print("Starting up...");
    logHandler("startup", "Starting GUI startup...", Log::Level::Info);

    if (Competition.isEnabled())
    {
        logHandler("startup", "Robot is IN Competition mode!", Log::Level::Fatal);
    }

    ConfigManager.checkServiceInterval();

    vex::competition::bStopAllTasksBetweenModes = false;

    if (isDiagnosticMode())
    {
        initializeDiagnosticMode();
        return;
    }

    if (ConfigManager.configType == configManager::ConfigType::Brain)
    {
        // Display team number at the top right
        auto teamNumber = ConfigManager.getTeamNumber();
        Brain.Screen.setCursor(1, 20);
        Brain.Screen.print("Team #%s", teamNumber.c_str());

        // Start the GIF player
        gifplayer();

        // Add the autonomous prompt
        Brain.Screen.clearScreen();
        Brain.Screen.setCursor(3, 5);
        Brain.Screen.print("Run Autonomous?");
        Brain.Screen.setCursor(5, 5);
        Brain.Screen.print("Yes");

        Brain.Screen.setCursor(5, 15);
        Brain.Screen.print("No");

        bool optionSelected = false;
        bool runAutonomous = false;

        while (!optionSelected)
        {
            if (Brain.Screen.pressing())
            {
                auto x = Brain.Screen.xPosition();
                auto y = Brain.Screen.yPosition();

                // Check if 'Yes' button is clicked
                if (x > 40 && x < 100 && y > 100 && y < 140)
                { // Adjust positions based on screen layout
                    logHandler("startup", "Starting autonomous from Brain screen.", Log::Level::Trace);
                    Brain.Screen.clearScreen();
                    Brain.Screen.setCursor(3, 5);
                    Brain.Screen.print("Running Autonomous...");
                    runAutonomous = true;
                    optionSelected = true;
                }
                // Check if 'No' button is clicked
                else if (x > 140 && x < 200 && y > 100 && y < 140)
                { // Adjust positions
                    logHandler("startup", "Skipped autonomous from Brain screen.", Log::Level::Trace);
                    Brain.Screen.clearScreen();
                    Brain.Screen.setCursor(3, 5);
                    Brain.Screen.print("Skipped Autonomous.");
                    runAutonomous = false;
                    optionSelected = true;
                }
            }
            vex::this_thread::sleep_for(50); // Avoid high CPU usage
        }

        if (runAutonomous)
        {
            autonomous();
            logHandler("startup", "Finished autonomous from Brain screen.", Log::Level::Trace);
        }
        vex::this_thread::sleep_for(1000);
    }
    else if (ConfigManager.configType == configManager::ConfigType::Controller)
    {
        auto message = "Battery is at: " + std::to_string(Brain.Battery.capacity()) + "%"; // Fix bug of displaying 2 % signs on controller
        if (Brain.Battery.capacity() < 90)
        {
            logHandler("startup", message + "%", Log::Level::Warn, 3); // Need to add another % sign to display correctly
        }
        else
        {
            logHandler("startup", message + "%", Log::Level::Info, 3);
        }

        auto selfTestChoice = getUserOption("Run self-test?", {"Yes", "No"});
        if (selfTestChoice == "Yes")
        {
            runPreMatchSelfTest();
        }

        auto autoRun = getUserOption("Run Autonomous?", {"Yes", "No"});
        if (autoRun == "Yes")
        {
            logHandler("startup", "Starting autonomous from setup.", Log::Level::Trace);
            primaryController.Screen.print("Running autonomous.");

            logHandler("startup", "Finished autonomous.", Log::Level::Trace);
        }
        else if (autoRun == "No")
        {
            primaryController.Screen.print("Skipped autonomous.");
            logHandler("startup", "Skipped autonomous.", Log::Level::Trace);
            vex::this_thread::sleep_for(1000);
        }
    }

    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    partnerController.Screen.clearScreen();
    partnerController.Screen.setCursor(1, 1);
    return;
}
