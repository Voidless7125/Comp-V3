#include "vex.h"

namespace
{
    struct SelfTestResult
    {
        std::string motorName;
        bool passed;
        std::string detail;
    };

    // Briefly spins each configured drive motor at low voltage and confirms
    // its own encoder actually moved. Motors are tested one at a time so a
    // failure is unambiguous about which corner is at fault.
    std::vector<SelfTestResult> testDriveMotors()
    {
        struct Entry
        {
            const char *name;
            std::unique_ptr<vex::motor> &motor;
        };
        Entry entries[] = {
            {"FrontLeft", frontLeftMotor},
            {"FrontRight", frontRightMotor},
            {"RearLeft", rearLeftMotor},
            {"RearRight", rearRightMotor},
        };

        constexpr double testVolts = 3.0;   // TUNE ME on hardware - just enough to move freely, not enough to lurch
        constexpr int spinMs = 300;
        constexpr double minDeltaDeg = 5.0; // TUNE ME on hardware

        std::vector<SelfTestResult> results;
        for (auto &e : entries)
        {
            SelfTestResult r{e.name, false, ""};
            if (!e.motor || !e.motor->installed())
            {
                r.detail = "not installed";
                results.push_back(r);
                continue;
            }

            double before = e.motor->position(vex::rotationUnits::deg);
            e.motor->spin(vex::directionType::fwd, testVolts, vex::voltageUnits::volt);
            vex::this_thread::sleep_for(spinMs);
            e.motor->stop(vex::brakeType::coast);
            vex::this_thread::sleep_for(100); // let it settle before reading the encoder
            double after = e.motor->position(vex::rotationUnits::deg);

            double delta = std::abs(after - before);
            r.passed = delta >= minDeltaDeg;
            r.detail = std::format("moved {:.1f} deg", delta);
            results.push_back(r);
        }
        return results;
    }

    // Heuristic, not a rigorous test: captures yaw, waits briefly with all
    // drive motors stopped (from testDriveMotors() just above), and checks
    // it didn't drift. A real bump/vibration during this window can cause a
    // false "FAIL" - that's an acceptable false-positive rate for a
    // pre-match sanity check, not a certified sensor test.
    bool checkGyroDrift(double maxDriftDeg, int windowMs)
    {
        if (!InertialGyro)
        {
            return true; // nothing to check
        }
        double startYaw = InertialGyro->yaw(vex::rotationUnits::deg);
        vex::this_thread::sleep_for(windowMs);
        double endYaw = InertialGyro->yaw(vex::rotationUnits::deg);
        return std::abs(endYaw - startYaw) <= maxDriftDeg;
    }
} // namespace

/**
 * Runs a short (~2s) pre-match self-test: spins each drive motor and
 * confirms its encoder moved, then checks the gyro isn't drifting while
 * stationary. Meant to catch a dead motor, a disconnected encoder, or a
 * gyro that needs recalibrating BEFORE the match starts, instead of
 * discovering it mid-match.
 *
 * Refuses to run if Competition.isEnabled() - this moves the drivetrain,
 * and must never do so once a match has actually started or while sitting
 * on a competition field outside of an explicit, human-confirmed moment
 * (see the "Run self-test?" prompt in vexCodeInit()).
 */
void runPreMatchSelfTest()
{
    if (Competition.isEnabled())
    {
        logHandler("selfTest", "Refusing to self-test while Competition is enabled.", Log::Level::Error, 3);
        return;
    }

    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print("Running self-test...");
    primaryController.Screen.clearScreen();
    primaryController.Screen.setCursor(1, 1);
    primaryController.Screen.print("Self-test running...");

    auto results = testDriveMotors();
    bool gyroOk = checkGyroDrift(1.5, 200); // TUNE ME on hardware

    bool allPassed = gyroOk;
    std::string summary;
    for (auto &r : results)
    {
        summary += std::format("{}: {} ({})\n", r.motorName, r.passed ? "OK" : "FAIL", r.detail);
        allPassed = allPassed && r.passed;
    }
    summary += std::format("Gyro drift: {}\n", gyroOk ? "OK" : "FAIL");

    Brain.Screen.clearScreen();
    Brain.Screen.setCursor(1, 1);
    Brain.Screen.print(summary.c_str());

    if (allPassed)
    {
        logHandler("selfTest", "Pre-match self-test passed.", Log::Level::Info, 2);
    }
    else
    {
        logHandler("selfTest", "Pre-match self-test FAILED:\n" + summary, Log::Level::Warn, 6);
    }
}
