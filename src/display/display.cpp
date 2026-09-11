#include "vex.h"
#include <numeric>

// define some more colors
const vex::color grey((uint32_t)0x202020);
const vex::color dgrey((uint32_t)0x2F4F4F);
const vex::color lblue((uint32_t)0x303060);
const vex::color lred((uint32_t)0x603030);

/*-----------------------------------------------------------------------------*/
/** @brief      Display data for one motor                                     */
/*-----------------------------------------------------------------------------*/
void displayMotorData(vex::motor &m)
{
    int ypos = 0;
    int xpos = m.index() * 48;

    // There's no C++ API to get motor command value, use C API, this returns rpm
    int v1 = 0;
    if (m.installed())
    {
        v1 = vexMotorVelocityGet(m.index());
    }

    // The actual velocity of the motor in rpm
    int v2 = m.velocity(vex::velocityUnits::rpm);

    // The position of the motor internal encoder in revolutions
    double pos = m.position(vex::rotationUnits::rev);

    // Motor current in Amps
    double c1 = m.current();

    // Motor temperature
    double t1 = m.temperature(vex::temperatureUnits::celsius);

    Brain.Screen.setFont(vex::mono15);

    // background color based on
    // device and whether it's left, right or other motor
    if (!m.installed())
    {
        // no motor
        Brain.Screen.setFillColor(grey);
    }
    else if (m.index() == frontLeftMotor->index() || m.index() == rearLeftMotor->index())
    {
        Brain.Screen.setFillColor(lblue);
    }
    else if (m.index() == frontRightMotor->index() || m.index() == rearRightMotor->index())
    {
        Brain.Screen.setFillColor(lred);
    }
    else
    {
        Brain.Screen.setFillColor(dgrey);
    }
    int width = (m.index() < 9) ? 49 : 48;
    Brain.Screen.drawRectangle(xpos, ypos, width, 79);
    Brain.Screen.setPenColor(vex::white);
    int w = (m.index() < 9) ? 49 : 48;
    Brain.Screen.drawRectangle(xpos, ypos, w, 79);

    // no motor, then return early
    if (!m.installed())
    {
        Brain.Screen.printAt(xpos + 15, ypos + 30, true, "NC");
        return;
    }

    // Show commanded speed
    Brain.Screen.setPenColor(v1 != 0 ? vex::green : vex::white);
    Brain.Screen.printAt(xpos + 13, ypos + 13, true, "{:4}", v1);

    // Show actual speed
    Brain.Screen.setPenColor(vex::yellow);
    Brain.Screen.printAt(xpos + 13, ypos + 30, true, "{:4}", v2);

    // Show position
    Brain.Screen.printAt(xpos + 5, ypos + 45, true, "{:5.1f}", pos);

    // Show current
    Brain.Screen.printAt(xpos + 5, ypos + 60, true, "{:4.1f}A", c1);

    // Show temperature
    Brain.Screen.printAt(xpos + 5, ypos + 75, true, "{:4.0f}C", t1);

    Brain.Screen.setPenColor(vex::white);
    Brain.Screen.drawLine(xpos, ypos + 14, xpos + 48, ypos + 14);
}

/*----------------------------------------------------------------------------*/
/** @brief  Display task - show some useful motor data                        */
/*----------------------------------------------------------------------------*/
void displayTask()
{
    Brain.Screen.setFont(vex::prop40);
    Brain.Screen.setPenColor(vex::red);
    Brain.Screen.printAt(90, 160, "DIAGNOSTIC MODE");

    vex::motor *motors[] = {frontLeftMotor.get(),
                            frontRightMotor.get(),
                            rearLeftMotor.get(),
                            rearRightMotor.get()};

    while (true)
    {
        for (auto &motor : motors)
        {
            displayMotorData(*motor);
        }

        // Display battery status
        Brain.Screen.setCursor(10, 1);
        Brain.Screen.print("Battery: {}%", Brain.Battery.capacity());

        // Display using back buffer, stops flickering
        Brain.Screen.render();

        // Check if the exit button is pressed
        if (Brain.Screen.pressing() && Brain.Screen.xPosition() >= 0 && Brain.Screen.xPosition() < 50 && Brain.Screen.yPosition() >= 0 && Brain.Screen.yPosition() < 50)
        {
            logHandler("displayTask", "Exit button pressed, terminating program.", Log::Level::Fatal);
            vex::task::stopAll();
            return;
        }

        vex::this_thread::sleep_for(100);
    }
}

std::string getUserOption(const std::string &settingName, const std::vector<std::string> &options)
{
    constexpr std::size_t maxWrongAttempts = 3;
    const std::string wrongMessages[maxWrongAttempts] = {
        "Invalid selection!", "Do you need a map?", "Rocks can do this!"};

    if (Competition.isEnabled())
    {
        logHandler("getUserOption", "Robot is IN competition mode!", Log::Level::Error, 2);
        return "DEFAULT";
    }

    if (options.size() > ConfigManager.getMaxOptionSize() || options.size() < 2)
    {
        logHandler("getUserOption", "`options` size error!", Log::Level::Error, 2);
        return "DEFAULT";
    }

    std::ostringstream oss;
    oss << "Options: " << options[0];
    for (auto it = std::next(options.begin()); it != options.end(); ++it)
    {
        oss << ", " << *it;
    }
    std::string optmessage = oss.str();

    logHandler("getUserOption", optmessage, Log::Level::Debug);

    std::size_t wrongAttemptCount = 0;
    std::size_t Index = options.size(); // Invalid selection by default
    int offset = 0;
    std::string buttonString;

    const auto buttonsInfo = getControllerButtonArray(primaryController);
    std::vector<std::string> buttons;
    std::vector<std::string> scrollButtons;

    for (const auto &buttonInfo : buttonsInfo)
    {
        if (buttonInfo.name == "A" || buttonInfo.name == "X" || buttonInfo.name == "Y" || buttonInfo.name == "B")
        {
            buttons.push_back(buttonInfo.name);
        }
        else if (buttonInfo.name == "DOWN" || buttonInfo.name == "UP")
        {
            scrollButtons.push_back(buttonInfo.name);
        }
    }

    while (!primaryController.installed())
    {
        vex::this_thread::sleep_for(25);
    }

    while (!Competition.isEnabled() && primaryController.installed())
    {
        buttonString.clear(); // fix bug of buttons not displaying
        primaryController.Screen.clearScreen();
        primaryController.Screen.setCursor(1, 1);
        partnerController.Screen.clearScreen();
        partnerController.Screen.setCursor(1, 1);
        primaryController.Screen.print(settingName.c_str());
        partnerController.Screen.print("Waiting for #1...");

        int displayedOptions = std::min(static_cast<int>(options.size() - offset), static_cast<int>(buttons.size()));
        std::string displayBuffer;
        displayBuffer.reserve(128); // Reserve some space to avoid multiple allocations (Fixes bug of random characters appearing on the screen)

        for (int i = 0; i < displayedOptions; ++i) // Display available options
        {
            displayBuffer += std::format("{}: {}\n", buttons[i], options[i + offset]);
            buttonString += buttons[i];
            if (i != displayedOptions - 1) // Add comma if not the last button
            {
                buttonString += ", ";
            }
        }

        if (options.size() > buttons.size())
        {
            if (offset + displayedOptions < static_cast<int>(options.size()))
            {
                displayBuffer += ">\n";
            }
            if (offset > 0)
            {
                displayBuffer += "^\n";
            }
        }

        primaryController.Screen.print(displayBuffer.c_str());

        optmessage = std::format("Available buttons for current visible options: {}", buttonString); // Append button string to message.
        logHandler("getUserOption", optmessage, Log::Level::Debug);

        auto buttonPressed = controllerButtonsPressed(primaryController);
        std::string buttonPressedStr;
        if (!buttonPressed.empty())
        {
            buttonPressedStr = buttonPressed.begin()->first;
        }

        auto buttonIt = std::ranges::find(buttons, buttonPressedStr);
        if (buttonIt != buttons.end())
        {
            int buttonIndex = std::distance(buttons.begin(), buttonIt);
            if (buttonIndex < displayedOptions)
            {
                Index = buttonIndex + offset;
                logHandler("getUserOption", std::format("[Valid Selection] Index = {} | Offset = {} | Button Pressed = {}", Index, offset, buttonPressedStr), Log::Level::Debug);
                break;
            }
        }
        else if (std::ranges::find(scrollButtons, buttonPressedStr) != scrollButtons.end())
        {
            if (buttonPressedStr == "DOWN" && (offset + displayedOptions < static_cast<int>(options.size())))
            {
                ++offset;
            }
            else if (buttonPressedStr == "UP" && offset > 0)
            {
                --offset;
            }
            logHandler("getUserOption", std::format("[Scroll {}] Offset = {}", buttonPressedStr, offset), Log::Level::Debug);
        }
        else
        {
            logHandler("getUserOption", std::format("[Invalid Selection] Index = {} | Offset = {} | Button Pressed = {}", Index, offset, buttonPressedStr), Log::Level::Debug);
            // Display message
            if (wrongAttemptCount < maxWrongAttempts)
            {
                primaryController.Screen.clearScreen();
                primaryController.Screen.setCursor(1, 1);
                partnerController.Screen.clearScreen();
                partnerController.Screen.setCursor(1, 1);
                primaryController.Screen.print(wrongMessages[wrongAttemptCount].c_str());
                ++wrongAttemptCount; // Increment wrong attempt count
                logHandler("getUserOption", std::format("wrongAttemptCount: {}", wrongAttemptCount), Log::Level::Debug);
                vex::this_thread::sleep_for(2000);
            }
            else
            {
                logHandler("getUserOption", "Too many invalid attempts, returning default option.", Log::Level::Fatal);
                return "DEFAULT";
            }
        }
    }
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    partnerController.Screen.clearScreen();
    if (Index < options.size())
    {
        return options[Index];
    }
    else
    {
        logHandler("getUserOption", "Index out of bounds, returning default option.", Log::Level::Error);
        return "DEFAULT";
    }
    return options[Index];
}