// ---- START VEXCODE CONFIGURED DEVICES ----
// Robot Configuration:
// [Name]               [Type]        [Port(s)]
// Drivetrain           drivetrain    1, 10, 11, 20, IG
// InertialGyro         inertial      2
// Radio                radio         6
// RearBumper           bumper        ThreeWirePort.A
// ---- END VEXCODE CONFIGURED DEVICES ----

#include "vex.h"

std::string Version = "3.0b3-f2 [DEV]";
std::string BuildDate = "2/22/25";

int main()
{
    printf("\033[2J\033[1;1H\033[0m"); // Clears console and Sets color to grey.
    ConfigManager.parseConfig();       // Loads config.cfg (motor ports, gear ratios, etc.)
    constructRobotHardware();          // Builds motors/Drivetrain from the config just loaded
                                        // (previously these were built at static-init time,
                                        // before parseConfig() had run - see robot-config.h)
    Competition.autonomous(autonomous);
    Competition.drivercontrol(userControl);
    vexCodeInit();
    while (Competition.isEnabled())
    {
        vex::this_thread::sleep_for(25);
    }
}
