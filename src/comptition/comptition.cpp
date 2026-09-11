#include "vex.h"
#include <algorithm>

void autonomous()
{
    logHandler("autonomous", "Test message.", Log::Level::Warn, 2);
    return;
}

void collision(const vex::axisType axis, const double x, const double y, const double z)
{
    printf("collision %d %6.2f %6.2f %6.2f\n", (int)axis, x, y, z);
}

// Drive system flags
bool tractionControlEnabled = true;
bool stabilityControlEnabled = true;
bool absEnabled = true;

/**
 * @author @DVT7125
 * @date 4/10/24
 * @brief User control task.
 * @return 0
 */

// Function to display system states on the Brain Screen
void displaySystemStates()
{
    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("Traction Control: %s\nStability Control: %s\nABS: %s",
                       tractionControlEnabled ? "ON" : "OFF",
                       stabilityControlEnabled ? "ON" : "OFF",
                       absEnabled ? "ON" : "OFF");
}

// Function to apply traction control
//
// PREVIOUS BUG: this overwrote forwardVolts (a -12..12 voltage) with a raw
// wheel RPM value (0..~200) every call - a unit mismatch that either barely
// moved the robot or slammed it to full voltage depending on speed, and
// threw away the driver's actual input. It's rewritten below to *scale*
// the driver's command down only when it detects wheel slip (one wheel
// spinning much faster than the others), which is what "traction control"
// is meant to do. The threshold/gain are reasonable starting points -
// tune them on the actual robot.
void applyTractionControl(double &forwardVolts)
{
    double speeds[] = {
        std::abs(frontLeftMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(rearLeftMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(frontRightMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(rearRightMotor->velocity(vex::velocityUnits::rpm))};

    double maxSpeed = *std::max_element(std::begin(speeds), std::end(speeds));
    double minSpeed = *std::min_element(std::begin(speeds), std::end(speeds));

    constexpr double slipThresholdRpm = 40.0; // TUNE ME on hardware
    if (maxSpeed > 1.0 && (maxSpeed - minSpeed) > slipThresholdRpm)
    {
        double scale = std::clamp(minSpeed / maxSpeed, 0.4, 1.0);
        forwardVolts *= scale;
    }
}

// Function to apply stability control
//
// PREVIOUS BUG: took forwardVolts by const-ref (so it could never actually
// affect the drive command) and instead directly called .spin() on both
// drive groups using RPM values as if they were volts, unconditionally
// overriding whatever the driver had just commanded a few lines later.
// Stability control's actual job - correcting left/right drift - belongs
// on turnVolts, so that's what this now adjusts, without touching the
// motors directly.
void applyStabilityControl(double &turnVolts)
{
    double leftRPM = LeftDriveSmart->velocity(vex::velocityUnits::rpm);
    double rightRPM = RightDriveSmart->velocity(vex::velocityUnits::rpm);

    constexpr double correctionGain = 0.05; // TUNE ME on hardware
    turnVolts -= correctionGain * (leftRPM - rightRPM);
    turnVolts = std::clamp(turnVolts, -12.0, 12.0);
}

// Function to apply ABS
//
// PREVIOUS BUG: parameter was named brakeVolts but the only call site
// passed forwardVolts by reference into it, so this silently clobbered the
// forward command with a raw RPM value again. Rewritten to ease off the
// forward command (rather than replace it outright) when the driver is
// trying to stop but the wheels are still spinning fast (a skid).
void applyABS(double &forwardVolts)
{
    double speeds[] = {
        std::abs(frontLeftMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(rearLeftMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(frontRightMotor->velocity(vex::velocityUnits::rpm)),
        std::abs(rearRightMotor->velocity(vex::velocityUnits::rpm))};

    double minSpeed = *std::min_element(std::begin(speeds), std::end(speeds));

    constexpr double stopCommandThreshold = 1.0; // TUNE ME on hardware
    constexpr double skidRpmThreshold = 80.0;    // TUNE ME on hardware
    if (std::abs(forwardVolts) < stopCommandThreshold && minSpeed > skidRpmThreshold)
    {
        forwardVolts *= 0.5;
    }
}

// Scales voltage commands up as the battery sags, so the robot doesn't feel
// weaker late in a match than it did at the start. Reference voltage is a
// freshly-charged pack; compensation is capped so it never asks for more
// than a genuinely fresh battery could give, and the caller still clamps
// the final command to +-12V regardless.
double batteryCompensationScale()
{
    constexpr double referenceVoltage = 12.8;    // TUNE ME on hardware - your pack's "full" voltage
    constexpr double minVoltageForScaling = 10.5; // below this, stop compensating further - charge the battery, don't paper over it
    constexpr double maxScale = 1.25;             // TUNE ME on hardware

    double batteryV = Brain.Battery.voltage();
    if (batteryV <= 0)
    {
        return 1.0; // sensor fault - don't amplify anything
    }
    double scale = referenceVoltage / std::max(batteryV, minVoltageForScaling);
    return std::clamp(scale, 1.0, maxScale);
}

// Function to display drive mode menu
void displayDriveModeMenu()
{
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    getUserOption("Drive Mode", {"Left Arcade", "Right Arcade", "Split Arcade", "Tank"});

    auto buttonPressDurations = controllerButtonsPressed(primaryController);
    std::string buttonPressed;

    if (buttonPressed == "A")
    {
        ConfigManager.setDriveMode(configManager::DriveMode::LeftArcade);
    }
    else if (buttonPressed == "B")
    {
        ConfigManager.setDriveMode(configManager::DriveMode::RightArcade);
    }
    else if (buttonPressed == "X")
    {
        ConfigManager.setDriveMode(configManager::DriveMode::SplitArcade);
    }
    else if (buttonPressed == "Y")
    {
        ConfigManager.setDriveMode(configManager::DriveMode::Tank);
    }

    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    primaryController.Screen.print("Drive Mode Selected");
}

// Lets the driver pick which motor role to relearn a port for, then hands
// off to learnNewPortForRole() to watch for the newly plugged-in motor.
void displayHotSwapMenu()
{
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    auto roleName = getUserOption("Relearn which motor?", {"FrontLeft", "FrontRight", "RearLeft", "RearRight"});

    MotorRole role;
    if (roleName == "FrontLeft")
    {
        role = MotorRole::FrontLeft;
    }
    else if (roleName == "FrontRight")
    {
        role = MotorRole::FrontRight;
    }
    else if (roleName == "RearLeft")
    {
        role = MotorRole::RearLeft;
    }
    else if (roleName == "RearRight")
    {
        role = MotorRole::RearRight;
    }
    else
    {
        return; // "DEFAULT" from getUserOption (e.g. timed out) - abort
    }

    learnNewPortForRole(role, primaryController);
}

// User control task
void userControl()
{
    if (!Competition.isEnabled())
    {
        logHandler("drivetrain_main", "Ctrl1 is NOT in command mode!", Log::Level::Fatal);
    }

    vex::thread motortemp(motorMonitor);
    InertialGyro->collision(collision);

    double turnVolts, forwardVolts;

    bool configMenuActive = false; // Tracks if options menu is active

    // Load drive mode from config
    configManager::DriveMode currentDriveMode = ConfigManager.getDriveMode();

    int leftDeadzone = ConfigManager.getLeftDeadzone();
    int rightDeadzone = ConfigManager.getRightDeadzone();

    // Hot-swap: automatic failover check cadence and the manual port-relearn
    // button combo latch (rewrite of user_control for the hot-swap feature).
    vex::timer hotSwapMonitorTimer;
    bool hotSwapComboLatched = false;

    while (Competition.isEnabled())
    {
        // Open configuration menu
        if (primaryController.ButtonUp.pressing())
        {
            configMenuActive = !configMenuActive;
            if (configMenuActive)
            {
                displayDriveModeMenu();
                currentDriveMode = ConfigManager.getDriveMode(); // Update currentDriveMode after selection
            }
        }

        // Hold L1+R1+Down to manually relearn which port a motor moved to
        // after plugging its cable into a different open port mid-match.
        bool hotSwapComboPressed = primaryController.ButtonL1.pressing() &&
                                    primaryController.ButtonR1.pressing() &&
                                    primaryController.ButtonDown.pressing();
        if (hotSwapComboPressed && !hotSwapComboLatched)
        {
            displayHotSwapMenu();
        }
        hotSwapComboLatched = hotSwapComboPressed;

        // Automatically fail over to a configured BACKUP_PORT if a drive
        // motor stops responding. Throttled to every 200ms - installed()
        // checks are cheap, but no need to run them every 5-25ms tick.
        if (hotSwapMonitorTimer.time() > 200)
        {
            checkAndHotSwapMotor(MotorRole::FrontLeft);
            checkAndHotSwapMotor(MotorRole::FrontRight);
            checkAndHotSwapMotor(MotorRole::RearLeft);
            checkAndHotSwapMotor(MotorRole::RearRight);
            hotSwapMonitorTimer.clear();
        }

        switch (currentDriveMode)
        {
        case configManager::DriveMode::LeftArcade:
            turnVolts = primaryController.Axis4.position() * 0.12; // -12 to 12
            forwardVolts = primaryController.Axis3.position() * 0.12;
            // Apply deadzones
            if (std::abs(primaryController.Axis3.position()) < leftDeadzone)
            {
                forwardVolts = 0;
            }
            if (std::abs(primaryController.Axis4.position()) < rightDeadzone)
            {
                turnVolts = 0;
            }
            break;

        case configManager::DriveMode::RightArcade:
            turnVolts = primaryController.Axis1.position() * 0.12; // -12 to 12
            forwardVolts = primaryController.Axis2.position() * 0.12;
            // Apply deadzones
            if (std::abs(primaryController.Axis2.position()) < leftDeadzone)
            {
                forwardVolts = 0;
            }
            if (std::abs(primaryController.Axis1.position()) < rightDeadzone)
            {
                turnVolts = 0;
            }
            break;

        case configManager::DriveMode::SplitArcade:
            turnVolts = primaryController.Axis1.position() * 0.12; // -12 to 12
            forwardVolts = primaryController.Axis3.position() * 0.12;
            // Apply deadzones
            if (std::abs(primaryController.Axis3.position()) < leftDeadzone)
            {
                forwardVolts = 0;
            }
            if (std::abs(primaryController.Axis1.position()) < rightDeadzone)
            {
                turnVolts = 0;
            }
            break;

        case configManager::DriveMode::Tank:
            double leftVolts = primaryController.Axis3.position() * 0.12;  // -12 to 12
            double rightVolts = primaryController.Axis2.position() * 0.12; // -12 to 12

            // Apply deadzones
            if (std::abs(primaryController.Axis3.position()) < leftDeadzone)
            {
                leftVolts = 0;
            }
            if (std::abs(primaryController.Axis2.position()) < rightDeadzone)
            {
                rightVolts = 0;
            }

            {
                // Battery-compensated drive scaling: keeps the robot feeling
                // the same at 10.5V as it does at 12.8V instead of getting
                // sluggish late in a match. Clamp afterward since
                // compensation can push a command slightly past +-12V.
                double compensation = batteryCompensationScale();
                leftVolts = std::clamp(leftVolts * compensation, -12.0, 12.0);
                rightVolts = std::clamp(rightVolts * compensation, -12.0, 12.0);
            }

            LeftDriveSmart->spin(vex::directionType::fwd, leftVolts, vex::voltageUnits::volt);
            RightDriveSmart->spin(vex::directionType::fwd, rightVolts, vex::voltageUnits::volt);
            break;
        }
        if (currentDriveMode != configManager::DriveMode::Tank)
        {

            // Apply traction control if enabled
            if (tractionControlEnabled)
            {
                applyTractionControl(forwardVolts);
            }

            // Apply stability control if enabled
            if (stabilityControlEnabled)
            {
                applyStabilityControl(turnVolts);
            }

            // Apply ABS if enabled
            if (absEnabled)
            {
                applyABS(forwardVolts);
            }

            // Battery-compensated drive scaling (see Tank case above for
            // why) - applied to the combined left/right command so the
            // final +-12V clamp accounts for both forward and turn at once.
            double compensation = batteryCompensationScale();
            double leftCmd = std::clamp((forwardVolts + turnVolts) * compensation, -12.0, 12.0);
            double rightCmd = std::clamp((forwardVolts - turnVolts) * compensation, -12.0, 12.0);

            // Apply the calculated voltages to the motors
            LeftDriveSmart->spin(vex::directionType::fwd, leftCmd, vex::voltageUnits::volt);
            RightDriveSmart->spin(vex::directionType::fwd, rightCmd, vex::voltageUnits::volt);
        }
        vex::this_thread::sleep_for(ConfigManager.getCtrlr1PollingRate());
    }
}