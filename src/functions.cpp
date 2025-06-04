#include "vex.h"

/**
 * @brief Calibrates the InertialGyro sensor.
 *
 * This function initiates the calibration process for the InertialGyro sensor and waits
 * until the calibration is complete. It periodically sleeps the thread for a short duration
 * to allow the sensor to finish calibrating. Once the calibration completes, a log entry is
 * made to indicate success.
 *
 * @note Ensure that the InertialGyro sensor is properly initialized before calling this function.
 */
void calibrateGyro()
{
    InertialGyro.calibrate();
    while (InertialGyro.isCalibrating())
    {
        vex::this_thread::sleep_for(20);
    }
    logHandler("calibrateGyro", "Finished calibrating Inertial Gyro.", Log::Level::Trace);
    return;
}

/**
 * @brief Monitors and records the durations of button presses on a controller.
 *
 * This function checks the press state of each button on the provided controller and logs
 * the duration of each press. When a button is pressed, it records the start time; when released,
 * it logs the duration based on the elapsed time measured by an internal timer.
 *
 * During a competition (i.e., when Competition.isEnabled() returns true), it logs an error message.
 * Otherwise, it continues to poll the controller's buttons until either competition mode is enabled 
 * or a 30-second timeout is reached.
 *
 * Each button's press durations are stored in a map with the button name as the key and a vector of 
 * integers as the value. A placeholder value of -1 is used to mark the beginning of a press event.
 *
 * @param controller A constant reference to a vex::controller object, representing the controller to monitor.
 *
 * @return std::map<std::string, std::vector<int>> A map from button names to vectors of press durations in milliseconds.
 *
 * @note The polling rate for button state checks is determined by the value returned from 
 *       ConfigManager.getCtrlr1PollingRate(), and the function uses local timers to measure durations.
 */
std::map<std::string, std::vector<int>> controllerButtonsPressed(const vex::controller &controller)
{
    if (Competition.isEnabled())
    {
        logHandler("controllerButtonsPressed", "Robot is IN competition mode!", Log::Level::Error, 3);
    }

    std::map<std::string, std::vector<int>> buttonPressTimes;
    vex::timer pressTimer;

    static std::array<ControllerButtonInfo, 12> AllControllerButtons = createControllerButtonArray(controller);

    auto checkButtonPress = [&](const ControllerButtonInfo &info)
    {
        if (info.button->pressing())
        {
            if (buttonPressTimes[info.name].empty() || buttonPressTimes[info.name].back() != -1)
            {
                pressTimer.clear();
                buttonPressTimes[info.name].push_back(-1); // Mark the start of a press
            }
        }
        else
        {
            if (!buttonPressTimes[info.name].empty() && buttonPressTimes[info.name].back() == -1)
            {
                buttonPressTimes[info.name].back() = pressTimer.time(); // Record the press duration
            }
        }
    };

    vex::timer timeoutTimer;
    while (!Competition.isEnabled() && timeoutTimer.time() < 30000) // 30 seconds timeout
    {
        for (const auto &buttonInfo : AllControllerButtons)
        {
            checkButtonPress(buttonInfo);
        }
        vex::this_thread::sleep_for(ConfigManager.getCtrlr1PollingRate());
    }

    for (const auto &[buttonName, pressDurations] : buttonPressTimes)
    {
        for (auto pressDuration : pressDurations)
        {
            std::string logMessage = std::format("Button: {}, Duration: {} ms", buttonName, pressDuration);
            logHandler("controllerButtonsPressed", logMessage, Log::Level::Debug);
        }
    }

    return buttonPressTimes;
}

std::string formatMotorTemps(const std::array<int, 4> &motorTemps, double batteryVoltage)
{
    return std::format("\n | LeftTemp: {}°\n | RightTemp: {}°\n | RearLeftTemp: {}°\n | RearRightTemp: {}°\n | Battery Voltage: {}V\n",
                       motorTemps[0], motorTemps[1], motorTemps[2], motorTemps[3], batteryVoltage);
}

/**
 * @brief Monitors motor temperatures and battery voltage during a competition run.
 *
 * This function continuously monitors the temperatures of the front left, front right,
 * rear left, and rear right motors. It logs a warning if any motor's temperature exceeds
 * 55°C by calling the logHandler() with an appropriate message. Every 10 seconds, it checks the
 * battery voltage and, if the voltage is below 12V, logs a critical warning and formats the
 * current motor temperatures along with the battery voltage.
 *
 * Additionally, it updates the odometer reading based on the average position of the left and
 * right drive motors, logs inertial measurements from a gyro sensor (pitch, roll, and yaw), and
 * updates the display on both the primary and partner controllers with the latest motor and battery data.
 *
 * @note This function runs inside a loop that continues while Competition.isEnabled() returns true.
 *
 * @see logHandler()
 * @see formatMotorTemps()
 * @see ConfigManager::updateOdometer()
 */
void motorMonitor()
{
    logHandler("motorMonitor", "motorMonitor is starting up...", Log::Level::Trace);

    auto logOverheat = [&](const std::string &motorName, int temperature)
    {
        if (temperature >= 55)
        {
            logHandler("motorMonitor", std::format("{} overheat: {}°", motorName, temperature), Log::Level::Warn, 3);
        }
    };
    vex::timer monitorTimer;
    vex::timer voltageCheckTimer;
    while (Competition.isEnabled())
    {
        std::array motorTemps = {
            static_cast<int>(frontLeftMotor.temperature(vex::temperatureUnits::celsius)),
            static_cast<int>(frontRightMotor.temperature(vex::temperatureUnits::celsius)),
            static_cast<int>(rearLeftMotor.temperature(vex::temperatureUnits::celsius)),
            static_cast<int>(rearRightMotor.temperature(vex::temperatureUnits::celsius))};

        constexpr std::array motorNames = {"FLM", "FRM", "RLM", "RRM"};

        for (size_t i = 0; i < motorTemps.size(); ++i)
        {
            logOverheat(motorNames[i], motorTemps[i]);
        }

        if (voltageCheckTimer.time() > 10000) // Check voltage every 10 seconds
        {
            std::string motorTempsStr;

            if (Brain.Battery.voltage() < 12)
            {
                logHandler("motorMonitor", "Brain voltage at a critical level!", Log::Level::Warn, 3);
                motorTempsStr = formatMotorTemps(motorTemps, Brain.Battery.voltage());
            }

            int leftMotorPosition = LeftDriveSmart.position(vex::rotationUnits::deg);
            int rightMotorPosition = RightDriveSmart.position(vex::rotationUnits::deg);
            int averagePosition = (leftMotorPosition + rightMotorPosition) / 2;
            ConfigManager.updateOdometer(averagePosition);

            // Update motorTempsStr with fresh data
            motorTempsStr = std::format(
                "\n | LeftTemp: {}°\n | RightTemp: {}°\n | RearLeftTemp: {}°\n | RearRightTemp: {}°\n | Battery Voltage: {}V\n",
                motorTemps[0], motorTemps[1], motorTemps[2], motorTemps[3], Brain.Battery.voltage());
            logHandler("motorMonitor", motorTempsStr, Log::Level::Info);

            // Log detailed motor data including RPM, current, and position
            std::array motors = {&frontLeftMotor, &frontRightMotor, &rearLeftMotor, &rearRightMotor};
            for (size_t i = 0; i < motors.size(); ++i)
            {
                if (motors[i]->installed())
                {
                    int commandedRpm = vexMotorVelocityGet(motors[i]->index());
                    int actualRpm = motors[i]->velocity(vex::velocityUnits::rpm);
                    double position = motors[i]->position(vex::rotationUnits::rev);
                    double current = motors[i]->current();
                    
                    std::string motorDetailStr = std::format(
                        "\n | {}: CmdRPM: {} | ActRPM: {} | Pos: {:.2f}rev | Current: {:.2f}A",
                        motorNames[i], commandedRpm, actualRpm, position, current);
                    logHandler("motorMonitor", motorDetailStr, Log::Level::Info);
                }
            }

            std::string dataBuffer = std::format("\nX Axis: {}\nY Axis: {}\nZ Axis: {}",
                                                 InertialGyro.pitch(vex::rotationUnits::deg),
                                                 InertialGyro.roll(vex::rotationUnits::deg),
                                                 InertialGyro.yaw(vex::rotationUnits::deg));
            logHandler("motorMonitor", dataBuffer, Log::Level::Info);

            primaryController.Screen.clearScreen();
            primaryController.Screen.setCursor(1, 1);
            partnerController.Screen.clearScreen();
            partnerController.Screen.setCursor(1, 1);

            std::string display = std::format("FLM: {}° | FRM: {}°\nRLM: {}° | RRM: {}°\nBattery: {:.1f}V",
                                              motorTemps[0], motorTemps[1], motorTemps[2], motorTemps[3], Brain.Battery.voltage());

            primaryController.Screen.print(display.c_str());
            partnerController.Screen.print(display.c_str());
            vex::this_thread::sleep_for(5000);
        }
    }
}

void gifplayer(bool enableVsync)
{
    if (!ConfigManager.getPrintLogo())
    {
        return;
    }
    else if (Competition.isAutonomous())
    {
        vex::Gif gif("assets/auto.gif", 0, 0);
        while (Competition.isAutonomous())
        {
            Brain.Screen.print("");
            vex::this_thread::sleep_for(20); // Add a small delay to prevent blocking
        }
    }
    else if (Competition.isDriverControl())
    {
        vex::Gif gif("assets/driver.gif", 0, 0);
        while (Competition.isDriverControl())
        {
            Brain.Screen.print("");
            vex::this_thread::sleep_for(20); // Add a small delay to prevent blocking
        }
    }
    else
    {
        vex::Gif gif("assets/auto.gif", 0, 0);
        vex::timer timeoutTimer;
        while (Competition.isAutonomous() && timeoutTimer.time() < 30000) // 30 seconds timeout
        {
            Brain.Screen.print("");
        }
    }
}