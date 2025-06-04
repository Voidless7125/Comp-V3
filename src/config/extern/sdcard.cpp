#include "vex.h"
#include <sstream>

/**
 * @brief Resets or initializes the configuration file based on user input.
 *
 * This function prompts the user with a custom message to decide whether to reset the configuration.
 * If the user selects "Yes", it outputs a notification on the screen, attempts to open the configuration
 * file for writing, and writes a default configuration to it. The default configuration includes various
 * system and motor settings, such as motor ports, gear ratios, reversal statuses, and other system parameters.
 *
 * @param message A std::string_view containing the message to display when asking the user
 *                if they want to reset the configuration.
 *
 * @details
 * - Resets the maximum size option (maxOptionSize) to 4, as a safeguard before reconfiguring.
 * - Uses the getUserOption function to obtain the user decision with "Yes" or "No" options.
 * - If "Yes" is selected:
 *   - Notifies the user via the primary screen that the configuration is being reset.
 *   - Attempts to create/open the configuration file. If unsuccessful, logs a warning and exits.
 *   - Writes the default configuration settings into the file, ending with the current version.
 *   - Logs a debug message indicating a successful reset.
 * - If the user selects "No", no changes are made.
 */
void configManager::resetOrInitializeConfig(std::string_view message)
{
    maxOptionSize = 4; // Reset opt may not be ready yet so set to default
    std::string resetcfg = getUserOption(std::string(message), {"Yes", "No"});
    if (resetcfg == "Yes")
    {
        primaryController.Screen.print("Resetting config file...");
        
        // Create default configuration content
        std::string configContent = R"(
    # Config File:
    MOTOR_CONFIG {
        FRONT_LEFT_MOTOR {
        PORT=1
        GEAR_RATIO=6_1
        REVERSED=false
        }
        FRONT_RIGHT_MOTOR {
        PORT=10
        GEAR_RATIO=6_1
        REVERSED=true
        }
        REAR_LEFT_MOTOR {
        PORT=11
        GEAR_RATIO=6_1
        REVERSED=false
        }
        REAR_RIGHT_MOTOR {
        PORT=20
        GEAR_RATIO=6_1
        REVERSED=true
        }
        INERTIAL {
        PORT=3
        }
        Rear_Bumper {
        PORT=A
        }
    }
    PRINTLOGO=true
    LOGTOFILE=true
    MAXOPTIONSSIZE=4
    POLLINGRATE=5
    CTRLR1POLLINGRATE=25
    CONFIGTYPE=Controller
    TEAMNUMBER=12
    LOADINGGIFPATH=loading.gif
    AUTOGIFPATH=auto.gif
    DRIVEGIFPATH=drive.gif
    vsyncGif=true
    DRIVEMODE=Split
    LEFTDEADZONE=10
    RIGHTDEADZONE=10
    VERSION=)" + Version + "\n";

        if (!writeStringToFile(configFileName, configContent))
        {
            logHandler("resetOrInitializeConfig", "Could not create config.", Log::Level::Warn, 3);
            return;
        }

        logHandler("resetConfig", "Successfully reset config file.", Log::Level::Debug);
    }
}

/**
 * @brief Writes maintenance data to a file.
 *
 * This function opens the file specified by the maintenanceFileName, and if the file is successfully opened,
 * it writes the current maintenance values as key/value pairs. The keys include:
 * - ODOMETER
 * - LAST_SERVICE
 * - SERVICE_INTERVAL
 *
 * Each key is followed by its corresponding value and a newline character. Finally, the file is closed.
 */
void configManager::writeMaintenanceData()
{
    std::string content = "ODOMETER=" + std::to_string(odometer) + "\n";
    content += "LAST_SERVICE=" + std::to_string(lastService) + "\n";
    content += "SERVICE_INTERVAL=" + std::to_string(serviceInterval) + "\n";
    
    writeStringToFile(maintenanceFileName, content);
}

/**
 * @brief Updates the drive mode setting for the configManager.
 *
 * This function sets the in-memory drive mode and persists the change by appending
 * the corresponding mode value to the configuration file. The drive mode is written
 * as a string representing the corresponding DriveMode enumeration value.
 *
 * @param mode The drive mode to be set. Valid values are:
 *             - DriveMode::LeftArcade
 *             - DriveMode::RightArcade
 *             - DriveMode::SplitArcade
 *             - DriveMode::Tank
 */
void configManager::setDriveMode(const configManager::DriveMode &mode)
{
    driveMode = mode; // Update in-memory

    // Also write to config file
    std::string content = "DRIVEMODE=";
    switch (mode)
    {
    case DriveMode::LeftArcade:
        content += "LeftArcade";
        break;
    case DriveMode::RightArcade:
        content += "RightArcade";
        break;
    case DriveMode::SplitArcade:
        content += "SplitArcade";
        break;
    case DriveMode::Tank:
        content += "Tank";
        break;
    }
    content += "\n";
    
    writeStringToFile(configFileName, content, true); // append = true
}

/**
 * @brief Reads maintenance data from the specified file.
 *
 * This function opens the maintenance file using the filename stored in 'maintenanceFileName'.
 * It reads the file line by line, expecting each line to be in the format "KEY=VALUE".
 * For each line, it parses the key and its associated value, and based on the key,
 * converts the value to the appropriate numeric type using the 'stringToNumber' template:
 *
 *   - "ODOMETER": Converts the value to an int and assigns it to 'odometer'.
 *   - "LAST_SERVICE": Converts the value to a long and assigns it to 'lastService'.
 *   - "SERVICE_INTERVAL": Converts the value to a long and assigns it to 'serviceInterval'.
 *
 * The file is closed after processing is complete.
 */
void configManager::readMaintenanceData()
{
    std::string fileContent = readFileToString(maintenanceFileName);
    if (fileContent.empty())
    {
        return; // File doesn't exist or is empty
    }

    std::istringstream stream(fileContent);
    std::string line;
    while (std::getline(stream, line))
    {
        std::istringstream iss(line);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value))
        {
            if (key == "ODOMETER")
            {
                odometer = stringToNumber<int>(value);
            }
            else if (key == "LAST_SERVICE")
            {
                lastService = stringToNumber<long>(value);
            }
            else if (key == "SERVICE_INTERVAL")
            {
                serviceInterval = stringToNumber<long>(value);
            }
        }
    }
}

// Method to convert a string to boolean
/**
 * @brief Converts a string view to a boolean value.
 *
 * This function interprets the input string view as a boolean. If the string is empty,
 * an error is logged and the function returns false. Valid true values are "true" and "1",
 * while valid false values are "false" and "0". For any string that does not match these values,
 * an error is logged and the function returns false.
 *
 * @param str The string view representing a boolean value.
 * @return bool Returns true if the string is "true" or "1", false otherwise.
 */
bool configManager::stringToBool(std::string_view str)
{
    if (str.empty())
    {
        logHandler("stringToBool", "Empty string cannot be converted to boolean", Log::Level::Error);
        return false;
    }

    if (str == "true" || str == "1")
    {
        return true;
    }
    else if (str == "false" || str == "0")
    {
        return false;
    }
    else
    {
        logHandler("stringToBool", std::format("Invalid boolean string: {}", str), Log::Level::Error);
        return false;
    }
}

// Method to convert a string to a number
/**
 * @brief Converts a string representation to a numeric value of type T.
 *
 * This template function attempts to convert a given string view into the numeric type T.
 * The conversion supports the following types:
 *   - int: Uses std::stoi.
 *   - long: Uses std::stol.
 *   - double: Uses std::stod.
 *   - std::size_t: Uses std::stoul (with a static_cast to std::size_t).
 *
 * If the input string is empty, the function logs an error and returns a default-constructed value of T.
 * In case of conversion errors (invalid_argument or out_of_range), the function catches the exception,
 * logs the error, and returns a default-constructed value of T.
 *
 * @tparam T The numeric type for the conversion.
 * @param str The string view containing the number to be converted.
 * @return A value of type T converted from the string, or a default-constructed T on failure.
 */
template <typename T>
T configManager::stringToNumber(std::string_view str)
{
    if (str.empty())
    {
        logHandler("stringToNumber", "Empty string cannot be converted to number", Log::Level::Error);
        return T{};
    }

    try
    {
        if constexpr (std::is_same_v<T, int>)
        {
            return std::stoi(std::string(str));
        }
        else if constexpr (std::is_same_v<T, long>)
        {
            return std::stol(std::string(str));
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            return std::stod(std::string(str));
        }
        else if constexpr (std::is_same_v<T, std::size_t>)
        {
            return static_cast<std::size_t>(std::stoul(std::string(str)));
        }
    }
    catch (const std::invalid_argument &e)
    {
        logHandler("stringToNumber", std::format("Invalid argument: {}", str), Log::Level::Error);
    }
    catch (const std::out_of_range &e)
    {
        logHandler("stringToNumber", std::format("Out of range: {}", str), Log::Level::Error);
    }

    return T{};
}

// Method to set values from the config file and parse complex config sections
/**
 * @brief Reads and applies configuration values from a config file.
 *
 * This function opens the configuration file specified by the internal variable (configFileName) and
 * iterates over each line to parse and apply configuration settings. It handles key-value pairs separated
 * by '=' and processes several recognized configuration keys such as:
 *   - ConfigType: Sets the configuration type by converting the string value.
 *   - TeamNumber: Sets the team number.
 *   - LoadingGifPath, AutoGifPath, DriverGifPath: Set file paths for various GIF resources.
 *   - VsyncGif, PRINTLOGO, LOGTOFILE: Convert string values to bool and store the settings.
 *   - MAXOPTIONSSIZE, POLLINGRATE, CTRLR1POLLINGRATE: Convert string values to numeric types.
 *   - LOGLEVEL: Converts the value to a log level type.
 *   - DRIVEMODE: Maps string values ("Arcade", "SplitArcade", "Tank", "Custom") to corresponding drive modes.
 *   - LEFTDEADZONE, RIGHTDEADZONE: Set deadzone values for controllers.
 *   - VERSION: Checks for a version mismatch between the configuration file and code.
 *
 * Additionally, it processes configuration sections (e.g., MOTOR_CONFIG, INERTIAL, TRIPORT_CONFIG) by:
 *   - Reading section-specific keys and values surrounded by braces.
 *   - Setting motor configurations (ports, gear ratios, reversed flags), inertial ports, and tri-port configurations.
 *
 * If the configuration file cannot be opened, or if an invalid or unrecognized key/line is encountered,
 * appropriate warnings are logged. The function may also trigger a config reset or initialization process
 * when encountering invalid lines.
 */
void configManager::setValuesFromConfig()
{
    std::string fileContent = readFileToString(configFileName);
    if (fileContent.empty())
    {
        logHandler("setValForConfig", "Could not open config file. Reason Unknown.", Log::Level::Warn, 4);
        return;
    }

    std::istringstream stream(fileContent);
    std::string configLine;
    while (std::getline(stream, configLine))
    {
        if (configLine.empty() || configLine[0] == ';' || configLine[0] == '#')
        {
            continue; // Skip empty lines and comments
        }

        std::istringstream iss(configLine);
        std::string key, value;
        if (std::getline(iss, key, '=') && std::getline(iss, value))
        {
            if (key == "ConfigType")
            {
                configType = stringToConfigType(value);
            }
            else if (key == "TeamNumber")
            {
                setTeamNumber(value);
            }
            else if (key == "LoadingGifPath")
            {
                setLoadingGifPath(value);
            }
            else if (key == "AutoGifPath")
            {
                setAutoGifPath(value);
            }
            else if (key == "DriverGifPath")
            {
                setDriverGifPath(value);
            }
            else if (key == "VsyncGif")
            {
                SetVsyncGif(stringToBool(value));
            }
            else if (key == "PRINTLOGO")
            {
                setPrintLogo(stringToBool(value));
            }
            else if (key == "LOGTOFILE")
            {
                setLogToFile(stringToBool(value));
            }
            else if (key == "MAXOPTIONSSIZE")
            {
                setMaxOptionSize(stringToNumber<std::size_t>(value));
            }
            else if (key == "POLLINGRATE")
            {
                setPollingRate(stringToNumber<std::size_t>(value));
            }
            else if (key == "CTRLR1POLLINGRATE")
            {
                setCtrlr1PollingRate(stringToNumber<std::size_t>(value));
            }
            else if (key == "LOGLEVEL")
            {
                setLogLevel(stringToLogLevel(value));
            }
            else if (key == "DRIVEMODE")
            {
                if (value == "Arcade")
                    setDriveMode(DriveMode::LeftArcade);
                else if (value == "SplitArcade")
                    setDriveMode(DriveMode::RightArcade);
                else if (value == "Tank")
                    setDriveMode(DriveMode::SplitArcade);
                else if (value == "Custom")
                    setDriveMode(DriveMode::Tank);
            }
            else if (key == "LEFTDEADZONE")
            {
                setLeftDeadzone(stringToNumber<int>(value));
            }
            else if (key == "RIGHTDEADZONE")
            {
                setRightDeadzone(stringToNumber<int>(value));
            }
            else if (key == "VERSION")
            {
                if (value != Version)
                {
                    logHandler("setValuesFromConfig", std::format("Version mismatch with Config file ({}) and code version ({}). Potential problems may occur.", value, Version), Log::Level::Warn, 4);
                }
            }
            else
            {
                logHandler("setValuesFromConfig", std::format("Unknown key in config file: {}. This value will be ignored.", value), Log::Level::Warn, 4);
            }
        }
        else if (key == "MOTOR_CONFIG" || key == "INERTIAL" || key == "TRIPORT_CONFIG")
        {
            std::string section = key;
            while (std::getline(stream, configLine) && configLine != "}")
            {
                if (configLine.empty() || configLine.starts_with(';') || configLine.starts_with('#'))
                {
                    continue; // Skip empty lines and comments
                }

                std::string name = configLine;
                std::getline(stream, configLine); // Skip the opening brace

                std::string port, gearRatio, reversedStr;
                while (std::getline(stream, configLine) && configLine != "}")
                {
                    std::istringstream iss(configLine);
                    std::string configKey, configValue;
                    if (std::getline(iss, configKey, '=') && std::getline(iss, configValue))
                    {
                        if (configKey == "PORT")
                        {
                            port = configValue;
                        }
                        else if (configKey == "GEAR_RATIO")
                        {
                            gearRatio = configValue;
                        }
                        else if (configKey == "REVERSED")
                        {
                            reversedStr = configValue;
                        }
                    }
                }

                if (section == "MOTOR_CONFIG")
                {
                    motorPorts[name] = stringToNumber<int>(port);
                    motorGearRatios[name] = gearRatio;
                    motorReversed[name] = stringToBool(reversedStr);
                }
                else if (section == "INERTIAL")
                {
                    inertialPorts[name] = stringToNumber<int>(port);
                }
                else if (section == "TRIPORT_CONFIG")
                {
                    triPorts[name] = getTriPort(port);
                }
            }
        }
        else
        {
            resetOrInitializeConfig(std::format("Invalid line in config file: {}. Do you want to reset the config?", configLine));
        }
    }
}

// Method to parse the config file
/**
 * @brief Parses the system configuration.
 *
 * This function initializes the system configuration by:
 * - Logging the current version and build date.
 * - Displaying a startup message on the screen.
 * - Checking for an inserted SD card.
 *   - If an SD card is present, verifies the existence of the configuration file.
 *     - If the file does not exist, prompts the user to reset or initialize the configuration.
 *     - Otherwise, loads configuration values from the file.
 *   - If no SD card is detected, sets default configuration values, such as polling rates, log settings,
 *     drive mode, deadzones, and service intervals.
 * - Calibrating the gyro sensor.
 * - Playing a GIF using the Vsync GIF.
 * - Setting the drivetrain's stopping mode to 'coast'.
 *
 * @note This function interacts with external components like Brain, primaryController, Drivetrain,
 *       and logging mechanisms to perform its tasks.
 *
 * @see calibrateGyro(), gifplayer(), getVsyncGif()
 */
void configManager::parseConfig()
{
    logHandler("main", std::format("Version: {} | Build date: {}", Version, BuildDate), Log::Level::Info);
    primaryController.Screen.print("Starting up...");

    if (Brain.SDcard.isInserted())
    {
        if (!fileExists(configFileName))
        {
            resetOrInitializeConfig("Missing config file. Create it?");
        }
        setValuesFromConfig();
    }
    else
    {
        // Default values
        POLLINGRATE = 5;
        PRINTLOGO = true;
        logToFile = false;
        maxOptionSize = 4;
        CTRLR1POLLINGRATE = 25;
        logLevel = Log::Level::Info;
        driveMode = DriveMode::SplitArcade;
        configType = ConfigType::Controller;
        odometer = 0;
        lastService = 0;
        serviceInterval = 1000;
        leftDeadzone = 10;  // Default left deadzone
        rightDeadzone = 10; // Default right deadzone
        logHandler("configParser", "No SD card installed. Using default values.", Log::Level::Info);
    }
    calibrateGyro();
    gifplayer(getVsyncGif());
    Drivetrain.setStopping(vex::brakeType::coast);
}