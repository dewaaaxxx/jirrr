#pragma once

#include <cmath>
#include "Types.h"
#include "FrictionProperties.h"

// 🎱 Robust Physics-Based Trajectory Correction
namespace RobustPhysics {
    // Constants for ball physics
    constexpr double GRAVITY = 9.81;           // m/s²
    constexpr double BALL_RADIUS = 0.028575;   // Standard ball radius
    constexpr double EPSILON = 1e-10;          // Numerical precision threshold
    
    // Enhanced friction model with realistic deceleration
    struct FrictionModel {
        double slidingFriction;     // μ_s: coefficient of sliding friction
        double rollingFriction;     // μ_r: coefficient of rolling friction
        double spinningFriction;    // μ_spin: coefficient of spinning friction
        double timeToEquilibrium;   // Time constant for sliding → rolling transition
        double velocityReductionSliding;   // Sliding friction reduction factor
        double velocityReductionRolling;   // Rolling friction reduction factor
        double deltaSpinFactor;     // Spin dampening factor
        
        // Initialize from game's FrictionProperties
        static FrictionModel fromGameProperties(const FrictionProperties& props) {
            return {
                props._coefficientOfSlidingFriction,
                props._coefficientOfRollingFriction,
                props._coefficientOfSpinningFriction,
                props._timeOfequilibriumFactor,
                props._velocityReductionSlidingFactor,
                props._velocityReductionRollingFactor,
                props._deltaSpinFactor
            };
        }
    };
    
    // 🎱 Calculate velocity deceleration due to friction
    // Uses proper kinematic equations: v(t) = v₀ - a*t
    // where a = μ*g (deceleration due to friction)
    inline void applyFrictionDeceleration(
        double& vx, double& vy,
        const FrictionModel& friction,
        double deltaTime
    ) {
        double speed = std::sqrt(vx * vx + vy * vy);
        
        // Skip if already stopped
        if (speed < EPSILON) {
            vx = 0.0;
            vy = 0.0;
            return;
        }
        
        // Calculate deceleration based on friction
        // Using rolling friction as primary: a = μ_r * g
        double deceleration = friction.rollingFriction * GRAVITY;
        
        // Maximum distance ball can decelerate to zero
        // Using: v² = v₀² - 2*a*s → s = v₀²/(2*a)
        double stoppingDistance = (speed * speed) / (2.0 * deceleration);
        
        // Calculate velocity after time interval
        // New speed: v = v₀ - a*t (clamped to zero if would overshoot)
        double newSpeed = std::max(0.0, speed - deceleration * deltaTime);
        
        // Preserve direction, apply new magnitude
        if (newSpeed > EPSILON && speed > EPSILON) {
            double speedRatio = newSpeed / speed;
            vx *= speedRatio;
            vy *= speedRatio;
        } else {
            vx = 0.0;
            vy = 0.0;
        }
    }
    
    // 🎱 Calculate spin-induced trajectory deviation (Magnus effect)
    // Moving ball with spin experiences lateral force: F_magnus = C_l * ω × v
    // This creates a curved trajectory
    inline void applySpinDeviation(
        double& vx, double& vy,
        double spinX, double spinY, double spinZ,
        double deltaTime
    ) {
        // Magnus coefficient (depends on ball surface roughness and air resistance)
        // Typical value: ~0.5 for billiard balls
        constexpr double MAGNUS_COEFFICIENT = 0.5;
        
        double speed = std::sqrt(vx * vx + vy * vy);
        if (speed < EPSILON) return;
        
        // Total spin magnitude
        double spinMag = std::sqrt(spinX * spinX + spinY * spinY + spinZ * spinZ);
        if (spinMag < EPSILON) return;
        
        // Lateral acceleration due to spin (perpendicular to velocity)
        // a_lateral = Magnus coefficient * spin * v / radius
        double lateralAccel = MAGNUS_COEFFICIENT * spinMag * speed / BALL_RADIUS;
        
        // Perpendicular direction to velocity (in 2D: rotate velocity 90°)
        double perpX = -vy / speed;
        double perpY = vx / speed;
        
        // Apply lateral deviation
        double deviationX = perpX * lateralAccel * deltaTime;
        double deviationY = perpY * lateralAccel * deltaTime;
        
        vx += deviationX;
        vy += deviationY;
    }
    
    // 🎱 Calculate spin dampening (spin gradually decreases)
    // Angular deceleration: ω(t) = ω₀ * e^(-t/τ)
    // where τ is the time constant
    inline void dampSpin(
        double& spinX, double& spinY, double& spinZ,
        const FrictionModel& friction,
        double deltaTime
    ) {
        // Spin time constant (how long spin lasts)
        // Inverse of friction effect: higher friction = shorter spin duration
        double spinTimeConstant = 1.0 / (friction.deltaSpinFactor + EPSILON);
        
        // Exponential decay: ω(t) = ω₀ * e^(-t/τ)
        double decayFactor = std::exp(-deltaTime / spinTimeConstant);
        
        spinX *= decayFactor;
        spinY *= decayFactor;
        spinZ *= decayFactor;
    }
    
    // 🎱 Complete trajectory update with physics
    // Combines friction deceleration, spin deviation, and spin dampening
    inline void updateTrajectoryWithPhysics(
        double& vx, double& vy,
        double& spinX, double& spinY, double& spinZ,
        const FrictionModel& friction,
        double deltaTime
    ) {
        // Order of operations matters:
        // 1. Apply spin-induced deviation to velocity
        // 2. Apply friction deceleration
        // 3. Dampen spin for next frame
        
        applySpinDeviation(vx, vy, spinX, spinY, spinZ, deltaTime);
        applyFrictionDeceleration(vx, vy, friction, deltaTime);
        dampSpin(spinX, spinY, spinZ, friction, deltaTime);
    }
    
    // 🎱 Calculate expected travel distance with friction
    // Useful for prediction: s = v₀²/(2*a)
    inline double calculateStoppingDistance(
        double initialSpeed,
        const FrictionModel& friction
    ) {
        if (initialSpeed < EPSILON) return 0.0;
        
        double deceleration = friction.rollingFriction * GRAVITY;
        if (deceleration < EPSILON) return 1e6; // Very large distance if no friction
        
        return (initialSpeed * initialSpeed) / (2.0 * deceleration);
    }
    
    // 🎱 Validate ball motion (check for numerical issues)
    inline bool isValidVelocity(double vx, double vy, double spinX, double spinY, double spinZ) {
        // Check for NaN or infinite values
        if (!std::isfinite(vx) || !std::isfinite(vy)) return false;
        if (!std::isfinite(spinX) || !std::isfinite(spinY) || !std::isfinite(spinZ)) return false;
        
        // Check for unreasonable speeds (>1000 units/s)
        double speed = std::sqrt(vx * vx + vy * vy);
        if (speed > 1000.0) return false;
        
        // Check for unreasonable spin (>1000 rad/s)
        double spinMag = std::sqrt(spinX * spinX + spinY * spinY + spinZ * spinZ);
        if (spinMag > 1000.0) return false;
        
        return true;
    }
}

// 🎱 Original collision detection functions (unchanged)
#define DAT_04c8b9a8 2.0
#define DAT_04c8b998 0.0
#define DAT_04c8bc78 1.0e-11
#define DAT_04c8b9c0 1.79769313e308

#define NAN std::isnan

void FUN_02b1b2d0(double *smallestTime, const Vector2D *ball1_position, const Vector2D *ball1_velocity, const Vector2D *ball2_position, const Vector2D *ball2_velocity, double *combinedBallRadiusSquared, double *param_7) {
    Vector2D relativePosition;
    double dVar2;
    double param_7_1;
    double dVar1;
    Vector2D velocityDelta;
    double velocityDeltaSquared;
    
    relativePosition.x = ball2_position->x - ball1_position->x;
    relativePosition.y = ball2_position->y - ball1_position->y;
    velocityDelta.x = ball2_velocity->x - ball1_velocity->x;
    velocityDelta.y = ball2_velocity->y - ball1_velocity->y;
    dVar1 = DAT_04c8b9a8 * (relativePosition.x * velocityDelta.x + relativePosition.y * velocityDelta.y);
    if (dVar1 < DAT_04c8b998 != (NAN(dVar1) || NAN(DAT_04c8b998))) {
        velocityDeltaSquared = velocityDelta.x * velocityDelta.x + velocityDelta.y * velocityDelta.y;
        dVar2 = ((relativePosition.x * relativePosition.x + relativePosition.y * relativePosition.y) - *combinedBallRadiusSquared) * velocityDeltaSquared * 4.0;
        if (dVar2 <= dVar1 * dVar1) {
            dVar2 = sqrt(dVar1 * dVar1 - dVar2);
            dVar1 = (-dVar1 - dVar2) / (DAT_04c8b9a8 * velocityDeltaSquared);
            if (DAT_04c8b998 <= dVar1) {
                param_7_1 = *param_7;
                dVar2 = dVar1 - DAT_04c8bc78;
                if (dVar2 == param_7_1 || dVar2 < param_7_1 != (NAN(dVar2) || NAN(param_7_1))) goto LAB_02b1b3b0;
            }
        }
    }
    dVar1 = DAT_04c8b9c0;
LAB_02b1b3b0:
    *smallestTime = dVar1;
    return;
}

bool Prediction::Ball::isBallBallCollision(double *smallestTime, Prediction::Ball &otherBall) const {
    auto& ball1 = *this;
    auto& ball2 = otherBall;

    double balls_radius = BALL_RADIUS + BALL_RADIUS;
    double combinedBallRadiusSquared = balls_radius * balls_radius;

    double tempTime = *smallestTime;
    
    FUN_02b1b2d0(&tempTime, &ball1.predictedPosition, &ball1.velocity, &ball2.predictedPosition, &ball2.velocity, &combinedBallRadiusSquared, &tempTime);
    
    if (tempTime != DAT_04c8b9c0) {
        *smallestTime = tempTime;
        return true;
    }

    return false;
}

bool FUN_03606c80(const Vector2D *position, const Vector2D *velocity, const double *smallestTime, const Vector4D *tableBounds, const double *radius) {
    Vector2D predicted(
        position->x + velocity->x * *smallestTime,
        position->y + velocity->y * *smallestTime
    ); double leftX, rightX, topY, bottomY;

    if (velocity->x > 0.0) {
        leftX = position->x;
        rightX = predicted.x;
    } else {
        leftX = predicted.x;
        rightX = position->x;
    }
    
    if (velocity->y > 0.0) {
        topY = position->y;
        bottomY = predicted.y;
    } else {
        topY = predicted.y;
        bottomY = position->y;
    }

    static auto FUN_034f8f20 = M(bool, libmain + 0x35f8a40, double*, double*, double*, double*, const Vector4D*, const double*);
    return FUN_034f8f20(&leftX, &topY, &rightX, &bottomY, tableBounds, radius);
}

bool Prediction::Ball::willCollideWithTable(const double *smallestTime) const {
    return FUN_03606c80(&this->predictedPosition, &this->velocity, smallestTime, &table_bounds, &BALL_RADIUS);
}

struct pos_vel_rad {
    Vector2D pos;
    Vector2D vel;
    double rad;
};

void FUN_02b1b664(double *smallestTime, pos_vel_rad *pos_vel_rad, const Vector2D *tableShapePoint, double *smallestTime_2) {
    double dVar1;
    double dVar2;
    double dVar3;
    double dVar4;
    double dVar5;
    double dVar6;
    
    dVar3 = (pos_vel_rad->vel).x;
    dVar4 = (pos_vel_rad->vel).y;
    dVar2 = tableShapePoint->x - (pos_vel_rad->pos).x;
    dVar5 = tableShapePoint->y - (pos_vel_rad->pos).y;
    dVar1 = DAT_04c8b9a8 * -dVar3 * dVar2 - dVar4 * DAT_04c8b9a8 * dVar5;
    if (dVar1 < DAT_04c8b998 != (NAN(dVar1) || NAN(DAT_04c8b998))) {
        dVar6 = dVar3 * dVar3 + dVar4 * dVar4;
        dVar2 = dVar2 * dVar2 + dVar5 * dVar5;
        dVar3 = dVar6 * 4.0;
        dVar4 = pos_vel_rad->rad * pos_vel_rad->rad;
        dVar5 = dVar2 - (dVar1 * dVar1) / dVar3;
        if (dVar5 < dVar4 != (NAN(dVar5) || NAN(dVar4))) {
            dVar2 = sqrt(dVar1 * dVar1 - dVar3 * (dVar2 - dVar4));
            dVar1 = (-dVar1 - dVar2) / (dVar6 * DAT_04c8b9a8);
            if (DAT_04c8b998 <= dVar1) {
                dVar3 = *smallestTime_2;
                dVar2 = dVar1 - DAT_04c8bc78;
                if (dVar2 == dVar3 || dVar2 < dVar3 != (NAN(dVar2) || NAN(dVar3))) goto LAB_02b1b754;
            }
        }
    }
    dVar1 = DAT_04c8b9c0;
LAB_02b1b754:
    *smallestTime = dVar1;
    return;
}

bool Prediction::Ball::isBallPointCollision(double *smallestTime, const Point2D &tableShapePoint) const {
    pos_vel_rad pos_vel_rad;
    pos_vel_rad.pos = this->predictedPosition;
    pos_vel_rad.vel = this->velocity;
    pos_vel_rad.rad = BALL_RADIUS;
    
    double tempTime = *smallestTime;
    
    FUN_02b1b664(&tempTime, &pos_vel_rad, &tableShapePoint, &tempTime);

    if (tempTime != DAT_04c8b9c0) {
        *smallestTime = tempTime;
        return true;
    }

    return false;
}

#define DAT_04c8b9a0 1.0

void FUN_02b1b3cc(double *param_1, pos_vel_rad *pos_vel_rad, const Vector2D *param_3, const Vector2D *param_4, double *param_5) {
    bool bVar1;
    bool bVar2;
    double dVar3;
    double dVar4;
    double dVar5;
    double dVar6;
    double dVar7;
    double dVar8;
    double dVar9;
    double dVar10;
    double dVar11;
    double dVar12;
    
    dVar11 = (pos_vel_rad->pos).x;
    dVar12 = (pos_vel_rad->pos).y;
    dVar9 = param_4->x - param_3->x;
    dVar10 = param_4->y - param_3->y;
    dVar8 = (pos_vel_rad->vel).x;
    dVar7 = (pos_vel_rad->vel).y;
    dVar3 = sqrt(dVar9 * dVar9 + dVar10 * dVar10);
    dVar5 = dVar8 * dVar10 - dVar7 * dVar9;
    if (dVar5 != DAT_04c8b998) {
        dVar4 = dVar10 * (DAT_04c8b9a0 / dVar3);
        dVar3 = (DAT_04c8b9a0 / dVar3) * -dVar9;
        dVar11 = (dVar11 - param_3->x) - dVar4 * pos_vel_rad->rad;
        dVar12 = (dVar12 - param_3->y) - dVar3 * pos_vel_rad->rad;
        dVar6 = (dVar8 * dVar12 - dVar7 * dVar11) / dVar5;
        bVar1 = false;
        bVar2 = false;
        if (DAT_04c8b998 < dVar6) {
            bVar1 = false;
            bVar2 = true;
            if (!NAN(dVar6) && !NAN(DAT_04c8b9a0)) {
                bVar1 = dVar6 < DAT_04c8b9a0;
                bVar2 = false;
            }
        }
        if ((bVar1 != bVar2) && (dVar5 = (dVar9 * dVar12 - dVar10 * dVar11) / dVar5, DAT_04c8b998 < dVar5)) {
            dVar10 = *param_5;
            dVar9 = dVar5 - DAT_04c8bc78;
            if ((dVar9 == dVar10 || dVar9 < dVar10 != (NAN(dVar9) || NAN(dVar10))) && (dVar3 = dVar8 * dVar4 + dVar7 * dVar3, dVar3 == DAT_04c8b998 || dVar3 < DAT_04c8b998 != (NAN(dVar3) || NAN(DAT_04c8b9a0)))) {
                goto LAB_02b1b4dc;
            }
        }
    }
    dVar5 = DAT_04c8b9c0;
LAB_02b1b4dc:
    *param_1 = dVar5;
    return;
}

bool Prediction::Ball::isBallLineCollision(double *smallestTime, const Point2D &tableShapePointA, const Point2D &tableShapePointB) const {
    if (!this->velocity) return false;

    pos_vel_rad pos_vel_rad;
    pos_vel_rad.pos = this->predictedPosition;
    pos_vel_rad.vel = this->velocity;
    pos_vel_rad.rad = BALL_RADIUS;
    
    double tempTime = *smallestTime;
    
    FUN_02b1b3cc(&tempTime, &pos_vel_rad, &tableShapePointA, &tableShapePointB, &tempTime);

    if (tempTime != DAT_04c8b9c0) {
        *smallestTime = tempTime;
        return true;
    }

    return false;
}

// 🎱 FIXED: Robust physics-based velocity calculation
void Prediction::Ball::calcVelocity() {
    Table table = sharedGameManager.mTable;
    if (!table) return;

    auto& balls = table.mBalls();
    auto ball = balls[this->index];
    auto& _frictionProperties = table._frictionProperties();

    // Initialize physics model with game friction properties
    auto frictionModel = RobustPhysics::FrictionModel::fromGameProperties(_frictionProperties);
    
    // Apply realistic physics updates
    // Using TIME_PER_TICK as delta time (typically 0.005s or 5ms)
    RobustPhysics::updateTrajectoryWithPhysics(
        this->velocity.x,
        this->velocity.y,
        this->spin.x,
        this->spin.y,
        this->spin.z,
        frictionModel,
        TIME_PER_TICK
    );
    
    // Validate results to catch numerical errors
    if (!RobustPhysics::isValidVelocity(
        this->velocity.x, this->velocity.y,
        this->spin.x, this->spin.y, this->spin.z
    )) {
        // Fallback: reset to zero if invalid
        this->velocity.nullify();
        this->spin.nullify();
    }
}

void Prediction::Ball::calcVelocityPostCollision(const double &angle) {
    Table table = sharedGameManager.mTable;
    if (!table) return;

    auto& balls = table.mBalls();
    auto ball = balls[this->index];
    auto& _frictionProperties = table._frictionProperties();

    auto bak_velocity = ball.velocity();
    auto bak_spin = ball.spin();

    ball.velocity() = this->velocity;
    ball.spin() = this->spin;

    static auto FUN_02b1bb3c = M(int64_t, libmain + 0x2ca7064, uintptr_t, FrictionProperties*, const double*);
    FUN_02b1bb3c(ball.instance, &_frictionProperties, &angle);

    if (ball.velocity() != this->velocity || ball.spin() != this->spin) {
        this->velocity = ball.velocity();
        this->spin = ball.spin();
    }

    ball.velocity() = bak_velocity;
    ball.spin() = bak_spin;
}
