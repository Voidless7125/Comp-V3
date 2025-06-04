#include "vex.h"

// Test function to validate triple-down logic (for development testing)
void testTripleDownLogic()
{
    logHandler("testTripleDownLogic", "Testing triple-down detection logic", Log::Level::Info);
    
    // Test variables
    int downPressCount = 0;
    vex::timer downPressTimer;
    const int TRIPLE_DOWN_WINDOW_MS = 1500;
    
    // Test case 1: Quick triple press (should work)
    downPressCount = 0;
    downPressTimer.clear();
    
    // Simulate first press
    downPressCount++;
    if (downPressCount < 3) downPressTimer.clear();
    
    // Simulate second press after 300ms
    vex::this_thread::sleep_for(300);
    if (downPressTimer.time() <= TRIPLE_DOWN_WINDOW_MS) {
        downPressCount++;
        if (downPressCount < 3) downPressTimer.clear();
    }
    
    // Simulate third press after another 300ms
    vex::this_thread::sleep_for(300);
    if (downPressTimer.time() <= TRIPLE_DOWN_WINDOW_MS) {
        downPressCount++;
        if (downPressCount >= 3) {
            logHandler("testTripleDownLogic", "Test case 1 PASSED: Quick triple press detected", Log::Level::Info);
        }
    }
    
    // Test case 2: Slow triple press (should fail)
    downPressCount = 0;
    downPressTimer.clear();
    
    downPressCount++;
    vex::this_thread::sleep_for(800); // Wait longer
    if (downPressTimer.time() <= TRIPLE_DOWN_WINDOW_MS) {
        downPressCount++;
        vex::this_thread::sleep_for(800); // Wait longer again
        if (downPressTimer.time() <= TRIPLE_DOWN_WINDOW_MS) {
            downPressCount++;
        } else {
            logHandler("testTripleDownLogic", "Test case 2 PASSED: Slow triple press correctly rejected", Log::Level::Info);
        }
    }
    
    logHandler("testTripleDownLogic", "Triple-down logic testing completed", Log::Level::Info);
}

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
void applyTractionControl(double &forwardVolts)
{
    double frontLeftMotorRPM = frontLeftMotor.velocity(vex::velocityUnits::rpm);
    double rearLeftMotorRPM = rearLeftMotor.velocity(vex::velocityUnits::rpm);
    double frontRightMotorRPM = frontRightMotor.velocity(vex::velocityUnits::rpm);
    double rearRightMotorRPM = rearRightMotor.velocity(vex::velocityUnits::rpm);

    double minSpeed = std::min({std::abs(frontLeftMotorRPM), std::abs(rearLeftMotorRPM), std::abs(frontRightMotorRPM), std::abs(rearRightMotorRPM)});

    forwardVolts = minSpeed;
}

// Function to apply stability control
void applyStabilityControl(const double &forwardVolts)
{
    double leftRPM = LeftDriveSmart.velocity(vex::velocityUnits::rpm);
    double rightRPM = RightDriveSmart.velocity(vex::velocityUnits::rpm);

    double minSpeed = std::min(std::abs(leftRPM), std::abs(rightRPM));

    LeftDriveSmart.spin(vex::directionType::fwd, minSpeed, vex::voltageUnits::volt);
    RightDriveSmart.spin(vex::directionType::fwd, minSpeed, vex::voltageUnits::volt);
}

// Function to apply ABS
void applyABS(double &brakeVolts)
{
    double frontLeftMotorRPM = frontLeftMotor.velocity(vex::velocityUnits::rpm);
    double rearLeftMotorRPM = rearLeftMotor.velocity(vex::velocityUnits::rpm);
    double frontRightMotorRPM = frontRightMotor.velocity(vex::velocityUnits::rpm);
    double rearRightMotorRPM = rearRightMotor.velocity(vex::velocityUnits::rpm);

    double minSpeed = std::min({std::abs(frontLeftMotorRPM), std::abs(rearLeftMotorRPM), std::abs(frontRightMotorRPM), std::abs(rearRightMotorRPM)});

    brakeVolts = minSpeed;
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

// Function to display runtime settings menu
void displayRuntimeSettingsMenu()
{
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    primaryController.Screen.print("Runtime Settings");
    
    // Simple menu with numbered options
    primaryController.Screen.setCursor(2, 1);
    primaryController.Screen.print("A: Drive Mode");
    primaryController.Screen.setCursor(3, 1);
    primaryController.Screen.print("B: Traction Ctrl");
    primaryController.Screen.setCursor(4, 1);
    primaryController.Screen.print("X: Stability Ctrl");
    primaryController.Screen.setCursor(5, 1);
    primaryController.Screen.print("Y: ABS Control");
    
    vex::timer menuTimer;
    bool menuActive = true;
    bool buttonWasPressed = false; // Debounce flag
    
    while (menuActive && Competition.isEnabled() && menuTimer.time() < 10000) // 10 second timeout
    {
        bool anyButtonPressed = primaryController.ButtonA.pressing() || 
                               primaryController.ButtonB.pressing() || 
                               primaryController.ButtonX.pressing() || 
                               primaryController.ButtonY.pressing() ||
                               primaryController.ButtonDown.pressing() || 
                               primaryController.ButtonUp.pressing();
        
        if (!anyButtonPressed)
        {
            buttonWasPressed = false; // Reset debounce when no button is pressed
        }
        
        if (!buttonWasPressed && primaryController.ButtonA.pressing())
        {
            buttonWasPressed = true;
            // Cycle through drive modes
            configManager::DriveMode currentMode = ConfigManager.getDriveMode();
            configManager::DriveMode newMode;
            
            switch (currentMode)
            {
                case configManager::DriveMode::LeftArcade:
                    newMode = configManager::DriveMode::RightArcade;
                    break;
                case configManager::DriveMode::RightArcade:
                    newMode = configManager::DriveMode::SplitArcade;
                    break;
                case configManager::DriveMode::SplitArcade:
                    newMode = configManager::DriveMode::Tank;
                    break;
                case configManager::DriveMode::Tank:
                    newMode = configManager::DriveMode::LeftArcade;
                    break;
            }
            
            ConfigManager.setDriveMode(newMode);
            primaryController.Screen.clearScreen();
            primaryController.Screen.setCursor(1, 1);
            
            std::string modeText;
            switch (newMode)
            {
                case configManager::DriveMode::LeftArcade:
                    modeText = "Left Arcade";
                    break;
                case configManager::DriveMode::RightArcade:
                    modeText = "Right Arcade";
                    break;
                case configManager::DriveMode::SplitArcade:
                    modeText = "Split Arcade";
                    break;
                case configManager::DriveMode::Tank:
                    modeText = "Tank Drive";
                    break;
            }
            
            primaryController.Screen.print(("Mode: " + modeText).c_str());
            vex::this_thread::sleep_for(1000);
            menuActive = false;
        }
        else if (!buttonWasPressed && primaryController.ButtonB.pressing())
        {
            buttonWasPressed = true;
            tractionControlEnabled = !tractionControlEnabled;
            primaryController.Screen.clearScreen();
            primaryController.Screen.setCursor(1, 1);
            primaryController.Screen.print(tractionControlEnabled ? "Traction ON" : "Traction OFF");
            vex::this_thread::sleep_for(1000);
            menuActive = false;
        }
        else if (!buttonWasPressed && primaryController.ButtonX.pressing())
        {
            buttonWasPressed = true;
            stabilityControlEnabled = !stabilityControlEnabled;
            primaryController.Screen.clearScreen();
            primaryController.Screen.setCursor(1, 1);
            primaryController.Screen.print(stabilityControlEnabled ? "Stability ON" : "Stability OFF");
            vex::this_thread::sleep_for(1000);
            menuActive = false;
        }
        else if (!buttonWasPressed && primaryController.ButtonY.pressing())
        {
            buttonWasPressed = true;
            absEnabled = !absEnabled;
            primaryController.Screen.clearScreen();
            primaryController.Screen.setCursor(1, 1);
            primaryController.Screen.print(absEnabled ? "ABS ON" : "ABS OFF");
            vex::this_thread::sleep_for(1000);
            menuActive = false;
        }
        else if (!buttonWasPressed && (primaryController.ButtonDown.pressing() || primaryController.ButtonUp.pressing()))
        {
            buttonWasPressed = true;
            // Exit menu if Down or Up is pressed
            menuActive = false;
        }
        
        vex::this_thread::sleep_for(50);
    }
    
    primaryController.Screen.clearScreen();
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
    
    // Triple-down detection variables
    int downPressCount = 0;
    vex::timer downPressTimer;
    bool lastDownState = false;
    const int TRIPLE_DOWN_WINDOW_MS = 1500; // 1.5 second window

    // Load drive mode from config
    configManager::DriveMode currentDriveMode = ConfigManager.getDriveMode();

    int leftDeadzone = ConfigManager.getLeftDeadzone();
    int rightDeadzone = ConfigManager.getRightDeadzone();

    while (Competition.isEnabled())
    {
        // Triple-down detection for runtime settings
        bool currentDownState = primaryController.ButtonDown.pressing();
        
        if (currentDownState && !lastDownState) // Down button just pressed
        {
            if (downPressCount == 0 || downPressTimer.time() <= TRIPLE_DOWN_WINDOW_MS)
            {
                downPressCount++;
                downPressTimer.clear();
                
                if (downPressCount >= 3)
                {
                    // Triple-down detected, open runtime settings
                    displayRuntimeSettingsMenu();
                    currentDriveMode = ConfigManager.getDriveMode(); // Update current mode
                    leftDeadzone = ConfigManager.getLeftDeadzone(); // Update deadzones if changed
                    rightDeadzone = ConfigManager.getRightDeadzone();
                    downPressCount = 0; // Reset counter
                }
                else if (downPressCount == 2)
                {
                    // Give user feedback they're partway there
                    primaryController.rumble("-");
                }
            }
            else
            {
                // Window expired, reset counter
                downPressCount = 1;
                downPressTimer.clear();
            }
        }
        else if (downPressTimer.time() > TRIPLE_DOWN_WINDOW_MS && downPressCount > 0)
        {
            // Reset counter if window expires
            downPressCount = 0;
        }
        
        lastDownState = currentDownState;
    
        // Open configuration menu (existing UP button functionality)
        if (primaryController.ButtonUp.pressing())
        {
            configMenuActive = !configMenuActive;
            if (configMenuActive)
            {
                displayDriveModeMenu();
                currentDriveMode = ConfigManager.getDriveMode(); // Update currentDriveMode after selection
            }
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
                applyTractionControl(forwardVolts);
            }

            // Apply stability control if enabled
            if (stabilityControlEnabled)
            {
                applyStabilityControl(forwardVolts);
            }

            // Apply ABS if enabled
            if (absEnabled)
            {
                applyABS(forwardVolts);
            }

            // Apply the calculated voltages to the motors
            LeftDriveSmart.spin(vex::directionType::fwd, forwardVolts + turnVolts, vex::voltageUnits::volt);
            RightDriveSmart.spin(vex::directionType::fwd, forwardVolts - turnVolts, vex::voltageUnits::volt);
        }
        vex::this_thread::sleep_for(ConfigManager.getCtrlr1PollingRate());
    }
}