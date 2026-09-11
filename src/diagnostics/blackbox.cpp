#include "vex.h"
#include <fstream>

namespace
{
    std::ofstream blackBoxFile;
    bool blackBoxOpen = false;

    // The V5 brain has no real-time clock, so filenames can't be timestamped
    // meaningfully. Instead this keeps a small counter file on the SD card
    // so each run gets its own numbered CSV instead of overwriting the
    // last one.
    int nextMatchNumber()
    {
        constexpr const char *counterFile = "blackbox_count.txt";
        int n = 0;
        {
            std::ifstream in(counterFile);
            if (in.is_open())
            {
                in >> n;
            }
        }
        ++n;
        std::ofstream out(counterFile);
        if (out.is_open())
        {
            out << n;
        }
        return n;
    }
} // namespace

/**
 * Opens a new "match_NNNN.csv" file on the SD card and writes its header.
 * Call once at the start of a driver-control/autonomous period (currently
 * called from motorMonitor(), which itself is only started from
 * userControl() - see the note in CHANGES about autonomous-only matches).
 * Silently no-ops if there's no SD card, so this never blocks a match on
 * missing storage.
 */
void blackBoxStartMatch()
{
    if (!Brain.SDcard.isInserted())
    {
        blackBoxOpen = false;
        return;
    }

    int n = nextMatchNumber();
    std::string filename = std::format("match_{:04d}.csv", n);
    blackBoxFile.open(filename);
    blackBoxOpen = blackBoxFile.is_open();

    if (blackBoxOpen)
    {
        blackBoxFile << "time_ms,fl_temp_c,fr_temp_c,rl_temp_c,rr_temp_c,"
                        "fl_current_a,fr_current_a,rl_current_a,rr_current_a,"
                        "fl_pos_deg,fr_pos_deg,rl_pos_deg,rr_pos_deg,"
                        "battery_v,event\n";
        blackBoxFile.flush();
        logHandler("blackBox", "Logging this run to " + filename, Log::Level::Info, 2);
    }
    else
    {
        logHandler("blackBox", "Could not open black box log file.", Log::Level::Warn, 2);
    }
}

/// Appends one telemetry row. No-ops if the file isn't open or hardware
/// isn't built yet - safe to call unconditionally on a timer.
void blackBoxLogSample()
{
    if (!blackBoxOpen || !frontLeftMotor || !frontRightMotor || !rearLeftMotor || !rearRightMotor)
    {
        return;
    }

    blackBoxFile << Brain.Timer.time(vex::timeUnits::msec) << ","
                 << frontLeftMotor->temperature(vex::temperatureUnits::celsius) << ","
                 << frontRightMotor->temperature(vex::temperatureUnits::celsius) << ","
                 << rearLeftMotor->temperature(vex::temperatureUnits::celsius) << ","
                 << rearRightMotor->temperature(vex::temperatureUnits::celsius) << ","
                 << frontLeftMotor->current(vex::currentUnits::amp) << ","
                 << frontRightMotor->current(vex::currentUnits::amp) << ","
                 << rearLeftMotor->current(vex::currentUnits::amp) << ","
                 << rearRightMotor->current(vex::currentUnits::amp) << ","
                 << frontLeftMotor->position(vex::rotationUnits::deg) << ","
                 << frontRightMotor->position(vex::rotationUnits::deg) << ","
                 << rearLeftMotor->position(vex::rotationUnits::deg) << ","
                 << rearRightMotor->position(vex::rotationUnits::deg) << ","
                 << Brain.Battery.voltage() << ","
                 << "\n";
    // Flushed on every sample rather than buffered: matches are short (a
    // couple minutes), and a crash/disconnect without a flush would lose
    // the whole log, which defeats the point of a black box.
    blackBoxFile.flush();
}

/// Appends a one-off event row (e.g. a hot-swap failover) with a timestamp,
/// so it lines up on the same timeline as the telemetry samples.
void blackBoxLogEvent(const std::string &event)
{
    if (!blackBoxOpen)
    {
        return;
    }
    blackBoxFile << Brain.Timer.time(vex::timeUnits::msec) << ",,,,,,,,,,,,,,\"" << event << "\"\n";
    blackBoxFile.flush();
}

/// Closes the current match's log file. Safe to call even if nothing was
/// ever opened.
void blackBoxEndMatch()
{
    if (blackBoxOpen)
    {
        blackBoxFile.close();
        blackBoxOpen = false;
    }
}
