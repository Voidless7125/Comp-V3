#include "vex.h"

/**
 * Drives straight for a distance using vex::smartdrive's own built-in
 * closed-loop control (drive motor encoders - no distance/vision sensor
 * needed). Positive distance = forward, negative = backward.
 * @return false if the drivetrain isn't built yet or the motion timed out
 * (Drivetrain->setTimeout() is set in rebuildDriveGroups()).
 */
bool driveStraightMm(double distanceMm, double velocityPct)
{
    if (!Drivetrain)
    {
        logHandler("driveStraightMm", "Drivetrain not built yet - call after constructRobotHardware().", Log::Level::Error, 3);
        return false;
    }
    logHandler("driveStraightMm", std::format("Driving {:.0f}mm at {:.0f}%", distanceMm, velocityPct), Log::Level::Trace);
    return Drivetrain->driveFor(distanceMm, vex::distanceUnits::mm, velocityPct, vex::velocityUnits::pct);
}

/**
 * Turns in place to an absolute heading (0-359.9 deg) using
 * vex::smartdrive's built-in gyro-based turnToHeading(). For a *relative*
 * turn from wherever the robot currently is, use turnByDeg() instead.
 */
bool turnToHeadingDeg(double headingDeg, double velocityPct)
{
    if (!Drivetrain)
    {
        logHandler("turnToHeadingDeg", "Drivetrain not built yet - call after constructRobotHardware().", Log::Level::Error, 3);
        return false;
    }
    logHandler("turnToHeadingDeg", std::format("Turning to heading {:.1f} deg at {:.0f}%", headingDeg, velocityPct), Log::Level::Trace);
    return Drivetrain->turnToHeading(headingDeg, vex::rotationUnits::deg, velocityPct, vex::velocityUnits::pct);
}

/// Turns by a relative amount (positive = clockwise) from the current heading.
bool turnByDeg(double relativeDeg, double velocityPct)
{
    if (!Drivetrain)
    {
        logHandler("turnByDeg", "Drivetrain not built yet - call after constructRobotHardware().", Log::Level::Error, 3);
        return false;
    }
    logHandler("turnByDeg", std::format("Turning {:.1f} deg (relative) at {:.0f}%", relativeDeg, velocityPct), Log::Level::Trace);
    return Drivetrain->turnFor(relativeDeg, vex::rotationUnits::deg, velocityPct, vex::velocityUnits::pct);
}
