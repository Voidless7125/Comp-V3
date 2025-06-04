#include "vex.h"

/**
 * @brief Converts a given Log::Level enumeration value to its corresponding string representation.
 *
 * This function takes a Log::Level value and returns the appropriate string label that represents
 * the logging level. For example, Log::Level::Trace is converted to "Trace", Log::Level::Debug to "Debug", 
 * and so on. In case the log level is unrecognized, the function returns "Error".
 *
 * @param str A constant reference to a Log::Level value indicating the logging level.
 * @return A constant character pointer to the string representation of the logging level.
 */
const char *LogToString(const Log::Level &str)
{
    switch (str)
    {
    case Log::Level::Trace:
        return "Trace";
    case Log::Level::Debug:
        return "Debug";
    case Log::Level::Info:
        return "Info";
    case Log::Level::Warn:
        return "Warn";
    case Log::Level::Error:
        return "Unknown";
    case Log::Level::Fatal:
        return "Fatal";
    default:
        return "Error";
    }
}

/**
 * @brief Array of ANSI escape sequences corresponding to log level prefixes.
 *
 * This array holds color-coded string literals for various logging levels, intended
 * for use with console output. The color codes use ANSI escape sequences, which change
 * the text color in terminals that support these sequences.
 *
 * The array elements represent:
 * - Trace: Detailed tracing messages.
 * - Debug: Debugging messages for development.
 * - Info: Informational messages.
 * - Warn: Warning messages.
 * - Error: Error messages.
 * - Fatal: Critical errors that may require immediate attention.
 * - Unknown: A fallback for unspecified or unrecognized log levels.
 *
 * Each string starts with an ANSI escape sequence that sets the terminal text color,
 * followed by the corresponding log level tag.
 */
const char *consoleColors[] = {
    "\033[92m[Trace]",
    "\033[93m[Debug]",
    "\033[94m[Info]",
    "\033[38;5;216m[Warn]",
    "\033[31m[Error]",
    "\033[32m[Fatal]",
    "\033[37m[Unknown]"
};

/**
 * @brief Array mapping logging severity levels to their corresponding RTF color codes.
 *
 * This array holds the color codes for different logging levels in the following order:
 * - Trace: color code 3
 * - Debug: color code 4
 * - Info:  color code 5
 * - Warn:  color code 6
 * - Error: color code 2
 * - Fatal: color code 1
 */
const int rtfColors[] = {
    3, // Trace
    4, // Debug
    5, // Info
    6, // Warn
    2, // Error
    1  // Fatal
};

/**
 * @brief Maps a logging level to a corresponding console color.
 *
 * This function converts the provided logging level into an integer index, which is then used to
 * retrieve the corresponding color string from the consoleColors array.
 *
 * @param str The logging level for which the corresponding console color is required.
 * @return A pointer to a constant character array representing the console color associated with the given logging level.
 *
 * @note If the logging level is out of bounds (i.e., the index is negative or exceeds the array size),
 *       the code attempts to handle it by referencing an inline declaration of a scrollText function.
 *       This appears to be a placeholder or error-handling mechanism and should be refined for clarity.
 */
const char *LogToColor(const Log::Level &str)
{
    int index = static_cast<int>(str);
    size_t colorsSize = sizeof(consoleColors) / sizeof(consoleColors[0]);
    if (index < 0 || static_cast<size_t>(index) >= colorsSize)
    {
        void scrollText(const std::string &text, vex::controller &controller, const float &timeOfDisplay, const int &sleepDuration = 30);
    }
    return consoleColors[index];
}

/**
 * @brief Scrolls text on the controller screen for a specified duration.
 *
 * This function displays a scrolling substring of the provided text on the specified controller's screen.
 * The text is shown within a window of fixed length (20 characters). The scrolling effect is achieved
 * by updating the substring's starting position, scrolling forward until the end of the text is reached
 * and then backward to the beginning. Additionally, both primary and partner controller screens are
 * cleared and set before each text update.
 *
 * @param text The text string to be scrolled.
 * @param controller The controller on whose screen the text will be displayed.
 * @param timeOfDisplay The total time (in seconds) during which the text will be scrolled.
 * @param sleepDuration The duration (in milliseconds) to wait between screen updates. Defaults to 30.
 */
void scrollText(const std::string &text, vex::controller &controller, const float &timeOfDisplay, const int &sleepDuration = 30)
{
    const int displayLength = 20;
    int startIndex = 0;
    bool forward = true;
    const auto length = static_cast<int>(text.length());

    for (float elapsed = 0; elapsed < timeOfDisplay; elapsed += 0.3)
    {
        primaryController.Screen.clearScreen();
        primaryController.Screen.setCursor(1, 1);
        partnerController.Screen.clearScreen();
        partnerController.Screen.setCursor(1, 1);
        controller.Screen.print(text.substr(startIndex, displayLength).c_str());

        // Update startIndex for scrolling
        if (forward)
        {
            if (++startIndex + displayLength >= length)
            {
                forward = false;
            }
        }
        else
        {
            if (--startIndex <= 0)
                vex::this_thread::sleep_for(sleepDuration);
            forward = true;
        }
    }

    vex::this_thread::sleep_for(30);
}

// Display messages on controller screens
// #NoRedundantCode
/**
 * @brief Displays a message on both controllers with scrolling text effects and logs additional details.
 *
 * This function performs the following steps:
 * - Clears the screens on both the primary and partner controllers.
 * - Sets the cursor to the initial position on the primary controller.
 * - Scrolls the provided message on both controllers for the specified display duration.
 * - Prints a formatted log message on the partner controller's screen which includes the original message
 *   and the module or function name.
 * - If the message length exceeds 20 characters, it scrolls the message again on both controllers.
 * - Pauses for the duration of timeOfDisplay before clearing the screens and resetting the cursors.
 *
 * @param functionName The name of the function or module generating the message, used for logging.
 * @param message The message content to be displayed on the controllers' screens.
 * @param timeOfDisplay The duration, in seconds, for which the message and scrolling effects are maintained.
 */
void displayControllerMessage(const std::string &functionName, const std::string &message, const float &timeOfDisplay)
{
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    partnerController.Screen.clearScreen();
    scrollText(message, primaryController, timeOfDisplay, 30);
    scrollText(message, partnerController, timeOfDisplay, 30);
    partnerController.Screen.print(std::format("{}\nCheck logs.\nModule: {}\n", message, functionName).c_str());
    if (message.length() > 20)
    {
        scrollText(message, primaryController, timeOfDisplay);
        scrollText(message, partnerController, timeOfDisplay);
    }
    vex::this_thread::sleep_for(timeOfDisplay * 1000);
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    partnerController.Screen.clearScreen();
    partnerController.Screen.setCursor(1, 1);
}

// Log handler function
/**
 * @brief Logs a message with a specific log level and optionally displays it on the controller.
 *
 * This function handles logging by performing the following actions:
 * - Records the log to an SD card via SD_Card_Logging.
 * - Prints a formatted message to the console including a timestamp, log level color,
 *   the module/function name, and the message.
 * - For warning, error, or fatal log levels, it displays the message on the controller.
 * - If the log level is fatal, it interrupts all threads and requests system exit.
 *
 * @param functionName Name of the function generating the log message.
 * @param message The log message detailing the current event or error.
 * @param level The severity level of the log (e.g., informational, warning, error, fatal).
 * @param timeOfDisplay Duration for which the message should be displayed on the controller.
 */
void logHandler(const std::string &functionName, const std::string &message, const Log::Level level, const float &timeOfDisplay)
{
    SD_Card_Logging(level, functionName, message);
    printf("%s > Time: %.3f > Module: %s > %s \033[0m\n", LogToColor(level), Brain.Timer.time(vex::timeUnits::sec), functionName.c_str(), message.c_str());

    if (level == Log::Level::Warn || level == Log::Level::Error || level == Log::Level::Fatal)
    {
        displayControllerMessage(functionName, message, timeOfDisplay);
        if (level == Log::Level::Fatal)
        {
            vex::thread::interruptAll(); // Scary! 👾
            vexSystemExitRequest();      // Exit program
        }
    }
}

/**
 * @brief Logs a message to the SD card in RTF format if file logging is enabled.
 *
 * This function writes a formatted log entry to a file ("log.rtf") using the RTF color formatting. 
 * It first checks if file logging is enabled via the ConfigManager. In case the log file cannot be opened,
 * it flags the failure, logs a warning message using logHandler, disables file logging, and aborts further logging.
 *
 * Each log entry includes:
 * - The logging level, which influences the text color according to a predefined color table.
 * - The current timestamp retrieved from Brain.Timer.time in seconds.
 * - The module or function name that generated the log.
 * - The log message itself.
 *
 * After a failure to create the log file, further attempts to log via this function will be short-circuited.
 *
 * @param level         The severity level of the log message.
 * @param functionName  The name of the function or module that generated the log.
 * @param message       The actual log message to be recorded.
 */
void SD_Card_Logging(const Log::Level &level, const std::string &functionName, const std::string &message)
{
    static bool logFileCreationFailed = false;
    if (logFileCreationFailed)
    {
        return;
    }

    if (ConfigManager.getLogToFile())
    {
        if (!Brain.SDcard.isInserted())
        {
            logFileCreationFailed = true;
            logHandler("logHandler", "Could not create logfile - SD card not inserted.", Log::Level::Warn, 3);
            ConfigManager.setLogToFile(false);
            return;
        }

        // Prepare log content
        std::string logContent = "{\\rtf1\\ansi\\deff0 {\\colortbl;\\red0\\green0\\blue0;\\red255\\green0\\blue0;\\red0\\green255\\blue0;\\red0\\green0\\blue255;\\red255\\green255\\blue0;\\red255\\green0\\blue255;\\red0\\green255\\blue255;}\n";
        logContent += "\\cf" + std::to_string(rtfColors[static_cast<int>(level)]) + " ";
        logContent += "[" + LogToString(level) + "] > Time: " + std::to_string(Brain.Timer.time(vex::timeUnits::sec)) + " > Module: " + functionName + " > " + message + "\\line\n";
        logContent += "}\n";
        
        // Write to SD card using VEX API (append mode)
        int bytesWritten = Brain.SDcard.appendfile("log.rtf", const_cast<char*>(logContent.c_str()), logContent.length());
        if (bytesWritten != static_cast<int>(logContent.length()))
        {
            logFileCreationFailed = true;
            logHandler("logHandler", "Could not write to logfile.", Log::Level::Warn, 3);
            ConfigManager.setLogToFile(false);
            return;
        }
    }
}