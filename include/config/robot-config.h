#pragma once
#include <memory>

extern vex::brain Brain;

// NOTE: these used to be plain vex::motor/vex::inertial/vex::bumper value
// globals initialized directly from ConfigManager at file-scope. That ran
// during static initialization, BEFORE main() called
// ConfigManager.parseConfig() - so the config file was never actually read
// yet and these always got garbage/default ports (a static-initialization-
// order bug). They're now built explicitly by constructRobotHardware()
// after parseConfig() has run, and held as pointers so they can also be
// rebound at runtime for the motor hot-swap feature.
extern std::unique_ptr<vex::motor> frontLeftMotor;
extern std::unique_ptr<vex::motor> frontRightMotor;

extern std::unique_ptr<vex::motor> rearLeftMotor;
extern std::unique_ptr<vex::motor> rearRightMotor;

extern std::unique_ptr<vex::motor_group> LeftDriveSmart;
extern std::unique_ptr<vex::motor_group> RightDriveSmart;

extern std::unique_ptr<vex::smartdrive> Drivetrain;

extern vex::controller primaryController;
extern vex::controller partnerController;

extern std::unique_ptr<vex::inertial> InertialGyro;

extern std::unique_ptr<vex::bumper> RearBumper;

extern vex::competition Competition;

void vexCodeInit(void);
bool isDiagnosticMode();
void initializeDiagnosticMode();
void displayTask();

// --- Hardware construction (fixes the static-init-order bug) ---

/**
 * Builds every port-dependent device (drive motors, inertial, bumper) from
 * the already-loaded ConfigManager state, then builds the motor groups and
 * Drivetrain. MUST be called after ConfigManager.parseConfig() returns and
 * before anything else touches frontLeftMotor/InertialGyro/Drivetrain/etc.
 */
void constructRobotHardware();

/// Rebuilds LeftDriveSmart/RightDriveSmart/Drivetrain to reference whichever
/// motor objects are currently bound. Call after any hot-swap rebind.
void rebuildDriveGroups();

// --- Motor hot-swap (port relearn / auto failover) ---

enum class MotorRole
{
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight
};

/**
 * Checks whether `role`'s currently-bound motor has stopped reporting
 * installed(). If so, and a BACKUP_PORT was configured for it in
 * config.cfg and a motor is now detected there, rebinds the role to the
 * backup port and rebuilds the drive groups.
 * @return true if a swap happened this call.
 */
bool checkAndHotSwapMotor(MotorRole role);

/**
 * Interactive port-relearn: prompts on `controllerRef`'s screen, then waits
 * (up to timeoutMs) for a motor to newly appear on any smart port that
 * wasn't already reporting one, and binds `role` to it. Use this after
 * physically moving a motor's cable to a different open port mid-match.
 * @return true if a motor was found and bound.
 */
bool learnNewPortForRole(MotorRole role, vex::controller &controllerRef, int timeoutMs = 15000);
