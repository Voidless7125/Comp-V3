#include "vex.h"

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

// Control system constants
const double SLIP_THRESHOLD = 50.0;     // RPM difference threshold for slip detection
const double TRACTION_REDUCTION = 0.8;   // Reduce power to 80% when slip detected
const double HEADING_KP = 0.02;          // Proportional gain for heading correction
const double MAX_HEADING_CORRECTION = 2.0; // Maximum voltage correction for heading
const double ABS_VELOCITY_THRESHOLD = 300.0; // RPM drop threshold for ABS activation
const double ABS_REDUCTION = 0.5;        // Reduce braking power to 50% during ABS
const double ANTI_TIP_THRESHOLD = 15.0;  // Degrees of tilt before anti-tip activation

// Static variables for control systems
static double targetHeading = 0.0;
static bool headingInitialized = false;
static double previousVelocities[4] = {0.0, 0.0, 0.0, 0.0}; // FL, RL, FR, RR
static bool debugOutput = false; // Enable debug printing

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
    Brain.Screen.print("Traction Control: %s\nStability Control: %s\nABS: %s\nDebug Output: %s",
                       tractionControlEnabled ? "ON" : "OFF",
                       stabilityControlEnabled ? "ON" : "OFF",
                       absEnabled ? "ON" : "OFF",
                       debugOutput ? "ON" : "OFF");
}

// Function to toggle debug output
void toggleDebugOutput()
{
    debugOutput = !debugOutput;
    if (debugOutput) {
        printf("Drive Control Debug Output: ENABLED\n");
    } else {
        printf("Drive Control Debug Output: DISABLED\n");
    }
}

// Function to apply traction control
void applyTractionControl(double &forwardVolts, double &turnVolts)
{
    // Get current motor velocities
    double frontLeftRPM = frontLeftMotor.velocity(vex::velocityUnits::rpm);
    double rearLeftRPM = rearLeftMotor.velocity(vex::velocityUnits::rpm);
    double frontRightRPM = frontRightMotor.velocity(vex::velocityUnits::rpm);
    double rearRightRPM = rearRightMotor.velocity(vex::velocityUnits::rpm);
    
    // Calculate average velocities for left and right sides
    double leftSideRPM = (frontLeftRPM + rearLeftRPM) / 2.0;
    double rightSideRPM = (frontRightRPM + rearRightRPM) / 2.0;
    
    // Calculate expected velocities based on commanded voltages
    // Assuming linear relationship between voltage and RPM for simplicity
    double expectedLeftRPM = (forwardVolts + turnVolts) * 10.0; // Scale factor
    double expectedRightRPM = (forwardVolts - turnVolts) * 10.0;
    
    // Detect slip by comparing expected vs actual velocities
    double leftSlip = std::abs(expectedLeftRPM - leftSideRPM);
    double rightSlip = std::abs(expectedRightRPM - rightSideRPM);
    
    bool leftSlipping = leftSlip > SLIP_THRESHOLD;
    bool rightSlipping = rightSlip > SLIP_THRESHOLD;
    
    // Apply traction control corrections
    if (leftSlipping || rightSlipping) {
        if (leftSlipping) {
            // Reduce power to left side
            forwardVolts *= TRACTION_REDUCTION;
            turnVolts *= TRACTION_REDUCTION;
        }
        if (rightSlipping) {
            // Reduce power to right side
            forwardVolts *= TRACTION_REDUCTION;
            turnVolts *= TRACTION_REDUCTION;
        }
        
        if (debugOutput) {
            printf("Traction Control: Left slip=%.1f, Right slip=%.1f, Power reduced\n", 
                   leftSlip, rightSlip);
        }
    }
    
    // Optional: Use inertial sensor for additional heading correction during slip
    if ((leftSlipping || rightSlipping) && InertialGyro.installed()) {
        double currentHeading = InertialGyro.heading(vex::rotationUnits::deg);
        if (headingInitialized) {
            double headingError = targetHeading - currentHeading;
            
            // Normalize heading error to [-180, 180]
            while (headingError > 180) headingError -= 360;
            while (headingError < -180) headingError += 360;
            
            // Apply small heading correction during traction control
            double headingCorrection = headingError * HEADING_KP * 0.5; // Reduced gain during slip
            headingCorrection = std::max(-MAX_HEADING_CORRECTION/2, 
                                       std::min(MAX_HEADING_CORRECTION/2, headingCorrection));
            
            turnVolts += headingCorrection;
        }
    }
}

// Function to apply stability control
void applyStabilityControl(double &forwardVolts, double &turnVolts)
{
    if (!InertialGyro.installed()) {
        return; // Cannot provide stability control without inertial sensor
    }
    
    double currentHeading = InertialGyro.heading(vex::rotationUnits::deg);
    double currentPitch = InertialGyro.pitch(vex::rotationUnits::deg);
    double currentRoll = InertialGyro.roll(vex::rotationUnits::deg);
    double yawRate = InertialGyro.gyroRate(vex::axisType::zaxis, vex::velocityUnits::dps);
    
    // Initialize target heading when first called or when not moving
    if (!headingInitialized || (std::abs(forwardVolts) < 0.5 && std::abs(turnVolts) < 0.5)) {
        targetHeading = currentHeading;
        headingInitialized = true;
    }
    
    // Anti-tip feature: reduce power if excessive tilt detected
    bool tippingDetected = (std::abs(currentPitch) > ANTI_TIP_THRESHOLD || 
                           std::abs(currentRoll) > ANTI_TIP_THRESHOLD);
    
    if (tippingDetected) {
        forwardVolts *= 0.3; // Dramatically reduce forward power
        turnVolts *= 0.3;    // Reduce turn power
        
        if (debugOutput) {
            printf("Anti-tip activated: Pitch=%.1f°, Roll=%.1f°\n", currentPitch, currentRoll);
        }
        return; // Skip normal stability control during anti-tip
    }
    
    // When driving mostly straight (small turn input), apply heading correction
    if (std::abs(turnVolts) < 2.0 && std::abs(forwardVolts) > 1.0) {
        double headingError = targetHeading - currentHeading;
        
        // Normalize heading error to [-180, 180]
        while (headingError > 180) headingError -= 360;
        while (headingError < -180) headingError += 360;
        
        // Apply proportional correction
        double headingCorrection = headingError * HEADING_KP;
        headingCorrection = std::max(-MAX_HEADING_CORRECTION, 
                                   std::min(MAX_HEADING_CORRECTION, headingCorrection));
        
        turnVolts += headingCorrection;
        
        if (debugOutput && std::abs(headingCorrection) > 0.1) {
            printf("Stability Control: Heading error=%.1f°, Correction=%.2fV\n", 
                   headingError, headingCorrection);
        }
    }
    
    // Limit turn rate to avoid instability (yaw rate limiting)
    const double MAX_YAW_RATE = 180.0; // degrees per second
    if (std::abs(yawRate) > MAX_YAW_RATE) {
        double yawRateReduction = MAX_YAW_RATE / std::abs(yawRate);
        turnVolts *= yawRateReduction;
        
        if (debugOutput) {
            printf("Yaw rate limiting: %.1f dps, reduction factor=%.2f\n", yawRate, yawRateReduction);
        }
    }
    
    // Update target heading when intentionally turning
    if (std::abs(turnVolts) > 2.0) {
        targetHeading = currentHeading; // Use current heading as new target when turning
    }
}

// Function to apply ABS
void applyABS(double &forwardVolts, double &turnVolts)
{
    // Get current motor velocities
    double frontLeftRPM = frontLeftMotor.velocity(vex::velocityUnits::rpm);
    double rearLeftRPM = rearLeftMotor.velocity(vex::velocityUnits::rpm);
    double frontRightRPM = frontRightMotor.velocity(vex::velocityUnits::rpm);
    double rearRightRPM = rearRightMotor.velocity(vex::velocityUnits::rpm);
    
    double currentVelocities[4] = {frontLeftRPM, rearLeftRPM, frontRightRPM, rearRightRPM};
    
    // Detect braking condition: check if we're commanding low/negative power but still have momentum
    bool brakingDetected = false;
    double avgCurrentVelocity = (std::abs(frontLeftRPM) + std::abs(rearLeftRPM) + 
                                std::abs(frontRightRPM) + std::abs(rearRightRPM)) / 4.0;
    
    // Braking detected if commanding low power but robot still has significant velocity
    if (std::abs(forwardVolts) < 2.0 && avgCurrentVelocity > 30.0) {
        brakingDetected = true;
    }
    
    // Also detect if motors are actively being commanded to stop/reverse
    if (forwardVolts < -1.0) { // Reverse/braking command
        brakingDetected = true;
    }
    
    if (brakingDetected) {
        bool lockupDetected = false;
        
        // Check for sudden velocity drops indicating wheel lockup
        for (int i = 0; i < 4; i++) {
            double velocityDrop = std::abs(previousVelocities[i]) - std::abs(currentVelocities[i]);
            
            if (velocityDrop > ABS_VELOCITY_THRESHOLD) {
                lockupDetected = true;
                
                if (debugOutput) {
                    const char* motorNames[] = {"FL", "RL", "FR", "RR"};
                    printf("ABS: %s wheel lockup detected, velocity drop=%.1f RPM\n", 
                           motorNames[i], velocityDrop);
                }
                break;
            }
        }
        
        // Apply ABS correction if lockup detected
        if (lockupDetected) {
            // Reduce braking force to prevent lockup
            if (forwardVolts < 0) {
                forwardVolts *= ABS_REDUCTION; // Reduce reverse/braking power
            } else {
                forwardVolts = std::max(forwardVolts, 1.0); // Maintain minimum forward power
            }
            
            // Also reduce turn power during ABS activation
            turnVolts *= ABS_REDUCTION;
            
            if (debugOutput) {
                printf("ABS activated: Braking power reduced to %.2fV\n", forwardVolts);
            }
        }
    }
    
    // Store current velocities for next iteration
    for (int i = 0; i < 4; i++) {
        previousVelocities[i] = currentVelocities[i];
    }
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

// User control task
void userControl()
{
    if (!Competition.isEnabled())
    {
        logHandler("drivetrain_main", "Ctrl1 is NOT in command mode!", Log::Level::Fatal);
    }

    vex::thread motortemp(motorMonitor);
    InertialGyro.collision(collision);

    double turnVolts, forwardVolts;

    bool configMenuActive = false; // Tracks if options menu is active

    // Load drive mode from config
    configManager::DriveMode currentDriveMode = ConfigManager.getDriveMode();

    int leftDeadzone = ConfigManager.getLeftDeadzone();
    int rightDeadzone = ConfigManager.getRightDeadzone();

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

        // Toggle debug output with Down button
        static bool debugButtonPressed = false;
        if (primaryController.ButtonDown.pressing() && !debugButtonPressed)
        {
            toggleDebugOutput();
            debugButtonPressed = true;
        }
        else if (!primaryController.ButtonDown.pressing())
        {
            debugButtonPressed = false;
        }

        // Toggle control systems with shoulder buttons
        static bool l1ButtonPressed = false, l2ButtonPressed = false, r1ButtonPressed = false;
        
        // L1: Toggle Traction Control
        if (primaryController.ButtonL1.pressing() && !l1ButtonPressed)
        {
            tractionControlEnabled = !tractionControlEnabled;
            printf("Traction Control: %s\n", tractionControlEnabled ? "ENABLED" : "DISABLED");
            l1ButtonPressed = true;
        }
        else if (!primaryController.ButtonL1.pressing())
        {
            l1ButtonPressed = false;
        }
        
        // L2: Toggle Stability Control
        if (primaryController.ButtonL2.pressing() && !l2ButtonPressed)
        {
            stabilityControlEnabled = !stabilityControlEnabled;
            printf("Stability Control: %s\n", stabilityControlEnabled ? "ENABLED" : "DISABLED");
            l2ButtonPressed = true;
        }
        else if (!primaryController.ButtonL2.pressing())
        {
            l2ButtonPressed = false;
        }
        
        // R1: Toggle ABS
        if (primaryController.ButtonR1.pressing() && !r1ButtonPressed)
        {
            absEnabled = !absEnabled;
            printf("ABS: %s\n", absEnabled ? "ENABLED" : "DISABLED");
            r1ButtonPressed = true;
        }
        else if (!primaryController.ButtonR1.pressing())
        {
            r1ButtonPressed = false;
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

            LeftDriveSmart.spin(vex::directionType::fwd, leftVolts, vex::voltageUnits::volt);
            RightDriveSmart.spin(vex::directionType::fwd, rightVolts, vex::voltageUnits::volt);
            break;
        }
        if (currentDriveMode != configManager::DriveMode::Tank)
        {

            // Apply traction control if enabled
            if (tractionControlEnabled)
            {
                applyTractionControl(forwardVolts, turnVolts);
            }

            // Apply stability control if enabled
            if (stabilityControlEnabled)
            {
                applyStabilityControl(forwardVolts, turnVolts);
            }

            // Apply ABS if enabled
            if (absEnabled)
            {
                applyABS(forwardVolts, turnVolts);
            }

            // Apply the calculated voltages to the motors
            LeftDriveSmart.spin(vex::directionType::fwd, forwardVolts + turnVolts, vex::voltageUnits::volt);
            RightDriveSmart.spin(vex::directionType::fwd, forwardVolts - turnVolts, vex::voltageUnits::volt);
        }
        vex::this_thread::sleep_for(ConfigManager.getCtrlr1PollingRate());
    }
}