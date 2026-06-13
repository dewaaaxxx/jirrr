#pragma once

#include "8bp/Types.h"
#include "8bp/FrictionProperties.h"
#include <cmath>
#include <algorithm>

// ============================================================================
// Physics Constants and Thresholds
// ============================================================================

namespace PhysicsConstants {
    constexpr double GRAVITY = 9.81;
    constexpr double VELOCITY_STOP_THRESHOLD = 0.008;  // Minimum velocity before stopping
    constexpr double EPSILON = 1.0e-11;                 // Numerical precision threshold
    constexpr double MAX_TIME_VALUE = 1.79769313e308;   // Maximum representable time
    constexpr double TIME_SCALE = 2.0;                  // Time calculation scale factor
    constexpr double FRICTION_BASELINE = 98.5;          // Baseline friction multiplier
}

// ============================================================================
// Trajectory Prediction Utilities
// ============================================================================

namespace TrajectoryPrediction {
    
    /// Calculate the squared magnitude of a 2D vector
    inline double magnitudeSquared(double vx, double vy) {
        return vx * vx + vy * vy;
    }
    
    /// Calculate the magnitude of a 2D vector
    inline double magnitude(double vx, double vy) {
        return std::sqrt(magnitudeSquared(vx, vy));
    }
    
    /// Calculate dot product of two 2D vectors
    inline double dotProduct(double ax, double ay, double bx, double by) {
        return ax * bx + ay * by;
    }
    
    /// Calculate cross product of two 2D vectors (returns z-component)
    inline double crossProduct(double ax, double ay, double bx, double by) {
        return ax * by - ay * bx;
    }
    
    /// Normalize a 2D vector (returns magnitude)
    inline double normalize(double& vx, double& vy) {
        double mag = magnitude(vx, vy);
        if (mag > PhysicsConstants::EPSILON) {
            vx /= mag;
            vy /= mag;
        }
        return mag;
    }
    
    /// Calculate angle between two 2D vectors in radians
    inline double angleBetween(double ax, double ay, double bx, double by) {
        double mag_a = magnitude(ax, ay);
        double mag_b = magnitude(bx, by);
        
        if (mag_a < PhysicsConstants::EPSILON || mag_b < PhysicsConstants::EPSILON) {
            return 0.0;
        }
        
        double dot = dotProduct(ax, ay, bx, by);
        double cos_angle = dot / (mag_a * mag_b);
        
        // Clamp to [-1, 1] to avoid numerical errors in acos
        cos_angle = std::max(-1.0, std::min(1.0, cos_angle));
        return std::acos(cos_angle);
    }
    
    /// Calculate sine of angle between two 2D vectors
    inline double sineBetween(double ax, double ay, double bx, double by) {
        double cross = crossProduct(ax, ay, bx, by);
        double mag_a = magnitude(ax, ay);
        double mag_b = magnitude(bx, by);
        
        if (mag_a < PhysicsConstants::EPSILON || mag_b < PhysicsConstants::EPSILON) {
            return 0.0;
        }
        
        return cross / (mag_a * mag_b);
    }
    
    /// Project point onto line segment defined by two points
    inline double projectPointToLine(double px, double py, 
                                     double x1, double y1, 
                                     double x2, double y2) {
        double dx = x2 - x1;
        double dy = y2 - y1;
        double mag_sq = magnitudeSquared(dx, dy);
        
        if (mag_sq < PhysicsConstants::EPSILON) {
            return 0.0;  // Points are coincident
        }
        
        double dot = dotProduct(px - x1, py - y1, dx, dy);
        return dot / mag_sq;
    }
}

// ============================================================================
// Advanced Physics Engine
// ============================================================================

namespace AdvancedPhysicsEngine {
    
    using namespace PhysicsConstants;
    using namespace TrajectoryPrediction;
    
    /// Apply friction and decay to ball velocity and spin
    /// @param vx, vy - Velocity components (modified in-place)
    /// @param sx, sy, sz - Spin components (modified in-place)
    /// @param props - Friction properties from game engine
    void applyFrictionAndDecay(double& vx, double& vy, 
                               double& sx, double& sy, double& sz,
                               const FrictionProperties& props) {
        double speed = magnitude(vx, vy);
        
        // Stop if below threshold
        if (speed < VELOCITY_STOP_THRESHOLD) {
            vx = 0.0; vy = 0.0;
            sx = 0.0; sy = 0.0; sz = 0.0;
            return;
        }
        
        // Get friction coefficients with fallbacks
        double rollingFriction = props._coefficientOfRollingFriction;
        if (rollingFriction <= 0.0) rollingFriction = 0.015;
        
        double spinDecayFactor = props._deltaSpinFactor;
        if (spinDecayFactor <= 0.0) spinDecayFactor = 0.005;
        
        // Apply velocity deceleration
        double deceleration = rollingFriction * GRAVITY * FRICTION_BASELINE;
        double newSpeed = speed - (deceleration * TIME_PER_TICK);
        
        if (newSpeed > VELOCITY_STOP_THRESHOLD) {
            // Scale velocity proportionally
            double velocityRatio = newSpeed / speed;
            vx *= velocityRatio;
            vy *= velocityRatio;
            
            // Apply exponential decay to spin
            double spinDecay = std::exp(-TIME_PER_TICK / (spinDecayFactor + EPSILON));
            sx *= spinDecay;
            sy *= spinDecay;
            sz *= spinDecay;
        } else {
            // Complete stop
            vx = 0.0; vy = 0.0;
            sx = 0.0; sy = 0.0; sz = 0.0;
        }
    }
}

// ============================================================================
// Collision Detection
// ============================================================================

namespace CollisionDetection {
    
    using namespace PhysicsConstants;
    using namespace TrajectoryPrediction;
    
    /// Detect collision between two moving balls
    /// @param smallestTime - Output: time to collision
    /// @param ball1_position - Position of first ball
    /// @param ball1_velocity - Velocity of first ball
    /// @param ball2_position - Position of second ball
    /// @param ball2_velocity - Velocity of second ball
    /// @param combinedRadiusSq - Combined radius squared
    /// @returns - Time to collision (MAX_TIME_VALUE if no collision)
    double calculateBallBallCollisionTime(
        const Vector2D* ball1_position, const Vector2D* ball1_velocity,
        const Vector2D* ball2_position, const Vector2D* ball2_velocity,
        double combinedRadiusSq) {
        
        // Relative position and velocity
        double rel_x = ball2_position->x - ball1_position->x;
        double rel_y = ball2_position->y - ball1_position->y;
        double vel_rel_x = ball2_velocity->x - ball1_velocity->x;
        double vel_rel_y = ball2_velocity->y - ball1_velocity->y;
        
        // Quadratic equation coefficients: at^2 + bt + c = 0
        double a = magnitudeSquared(vel_rel_x, vel_rel_y);
        double b = TIME_SCALE * dotProduct(rel_x, rel_y, vel_rel_x, vel_rel_y);
        double c = magnitudeSquared(rel_x, rel_y) - combinedRadiusSq;
        
        // Check if balls are moving toward each other
        if (b >= 0.0) {
            return MAX_TIME_VALUE;  // Moving apart or parallel
        }
        
        // Calculate discriminant
        double discriminant = b * b - a * c * TIME_SCALE;
        
        if (discriminant < 0.0) {
            return MAX_TIME_VALUE;  // No collision
        }
        
        // Solve for nearest collision time
        double sqrt_disc = std::sqrt(discriminant);
        double t = (-b - sqrt_disc) / (a * TIME_SCALE);
        
        if (t >= 0.0) {
            return t;
        }
        
        return MAX_TIME_VALUE;
    }
    
    /// Detect collision between ball and point on table
    /// @returns - Time to collision (MAX_TIME_VALUE if no collision)
    double calculateBallPointCollisionTime(
        const Vector2D* position, const Vector2D* velocity,
        const Vector2D* tableShapePoint, double radius) {
        
        double dx = tableShapePoint->x - position->x;
        double dy = tableShapePoint->y - position->y;
        double vx = velocity->x;
        double vy = velocity->y;
        
        // Project closest point on trajectory to point
        double vel_mag_sq = magnitudeSquared(vx, vy);
        
        if (vel_mag_sq < EPSILON) {
            return MAX_TIME_VALUE;  // Ball not moving
        }
        
        double dot = dotProduct(dx, dy, vx, vy);
        
        // Ball moving away from point
        if (dot < 0.0) {
            return MAX_TIME_VALUE;
        }
        
        // Distance from trajectory to point
        double t = dot / vel_mag_sq;
        double closest_x = position->x + vx * t;
        double closest_y = position->y + vy * t;
        
        double dist_x = tableShapePoint->x - closest_x;
        double dist_y = tableShapePoint->y - closest_y;
        double distance = magnitude(dist_x, dist_y);
        
        if (distance <= radius) {
            return t;
        }
        
        return MAX_TIME_VALUE;
    }
    
    /// Detect collision between ball and line segment on table
    /// @returns - Time to collision (MAX_TIME_VALUE if no collision)
    double calculateBallLineCollisionTime(
        const Vector2D* position, const Vector2D* velocity,
        const Vector2D* lineStart, const Vector2D* lineEnd, double radius) {
        
        double px = position->x;
        double py = position->y;
        double vx = velocity->x;
        double vy = velocity->y;
        
        double line_x1 = lineStart->x;
        double line_y1 = lineStart->y;
        double line_x2 = lineEnd->x;
        double line_y2 = lineEnd->y;
        
        // Line direction
        double line_dx = line_x2 - line_x1;
        double line_dy = line_y2 - line_y1;
        double line_len_sq = magnitudeSquared(line_dx, line_dy);
        
        if (line_len_sq < EPSILON) {
            return calculateBallPointCollisionTime(position, velocity, lineStart, radius);
        }
        
        // Ball position relative to line start
        double rel_x = px - line_x1;
        double rel_y = py - line_y1;
        
        // Perpendicular distance from ball to line at t=0
        double cross = crossProduct(line_dx, line_dy, rel_x, rel_y);
        double perp_dist_sq = (cross * cross) / line_len_sq;
        
        if (perp_dist_sq > radius * radius) {
            return MAX_TIME_VALUE;  // Line too far from ball
        }
        
        // Project ball velocity onto line
        double vel_parallel = dotProduct(vx, vy, line_dx, line_dy);
        
        if (std::abs(vel_parallel) < EPSILON) {
            return MAX_TIME_VALUE;  // Ball not moving toward/along line
        }
        
        // Time to closest approach
        double dot_rel = dotProduct(rel_x, rel_y, line_dx, line_dy);
        double t = -dot_rel / line_len_sq;
        
        if (t < 0.0) {
            t = 0.0;  // Clamp to segment start
        } else if (t > 1.0) {
            t = 1.0;  // Clamp to segment end
        }
        
        // Time when ball touches line
        double perp_dist = std::sqrt(perp_dist_sq);
        double remaining_dist = radius - perp_dist;
        
        if (remaining_dist >= 0.0 && std::abs(vel_parallel) > EPSILON) {
            double collision_time = -dot_rel / vel_parallel + remaining_dist / vel_parallel;
            
            if (collision_time >= 0.0) {
                return collision_time;
            }
        }
        
        return MAX_TIME_VALUE;
    }
}

// ============================================================================
// Ball Prediction Methods (Member Functions)
// ============================================================================

bool Prediction::Ball::isBallBallCollision(double *smallestTime, Prediction::Ball &otherBall) const {
    auto& ball1 = *this;
    auto& ball2 = otherBall;
    
    double combined_radius = BALL_RADIUS + BALL_RADIUS;
    double combined_radius_sq = combined_radius * combined_radius;
    
    double collision_time = CollisionDetection::calculateBallBallCollisionTime(
        &ball1.predictedPosition, &ball1.velocity,
        &ball2.predictedPosition, &ball2.velocity,
        combined_radius_sq
    );
    
    if (collision_time < PhysicsConstants::MAX_TIME_VALUE) {
        *smallestTime = collision_time;
        return true;
    }
    
    return false;
}

bool Prediction::Ball::willCollideWithTable(const double *smallestTime) const {
    // This calls external table collision detection
    // Implementation depends on game engine integration
    static auto FUN_03606c80 = 
        M(bool, libmain + 0x35f8a40, const Vector2D*, const Vector2D*, 
          const double*, const Vector4D*, const double*);
    
    return FUN_03606c80(&this->predictedPosition, &this->velocity, 
                        smallestTime, &table_bounds, &BALL_RADIUS);
}

bool Prediction::Ball::isBallPointCollision(double *smallestTime, const Point2D &tableShapePoint) const {
    double collision_time = CollisionDetection::calculateBallPointCollisionTime(
        &this->predictedPosition, &this->velocity,
        &tableShapePoint, BALL_RADIUS
    );
    
    if (collision_time < PhysicsConstants::MAX_TIME_VALUE) {
        *smallestTime = collision_time;
        return true;
    }
    
    return false;
}

bool Prediction::Ball::isBallLineCollision(double *smallestTime, 
                                           const Point2D &tableShapePointA, 
                                           const Point2D &tableShapePointB) const {
    if (!this->velocity) return false;
    
    double collision_time = CollisionDetection::calculateBallLineCollisionTime(
        &this->predictedPosition, &this->velocity,
        &tableShapePointA, &tableShapePointB,
        BALL_RADIUS
    );
    
    if (collision_time < PhysicsConstants::MAX_TIME_VALUE) {
        *smallestTime = collision_time;
        return true;
    }
    
    return false;
}

// ============================================================================
// Velocity Calculations
// ============================================================================

void Prediction::Ball::calcVelocity() {
    Table table = sharedGameManager.mTable;
    if (!table) return;
    
    auto& balls = table.mBalls();
    auto ball = balls[this->index];
    auto& friction_props = table._frictionProperties();
    
    // Backup original state
    auto backup_velocity = ball.velocity();
    auto backup_spin = ball.spin();
    
    // Apply predicted state to ball
    ball.velocity() = this->velocity;
    ball.spin() = this->spin;
    
    // Call game engine friction calculation
    static auto calcEngineVelocity = 
        M(void, libmain + 0x3725a34, uintptr_t, FrictionProperties*, const double*);
    calcEngineVelocity(ball.instance, &friction_props, &TIME_PER_TICK);
    
    // Retrieve updated state
    if (ball.velocity() != this->velocity || ball.spin() != this->spin) {
        this->velocity = ball.velocity();
        this->spin = ball.spin();
    }
    
    // Apply advanced physics refinement
    AdvancedPhysicsEngine::applyFrictionAndDecay(
        this->velocity.x, this->velocity.y,
        this->spin.x, this->spin.y, this->spin.z,
        friction_props
    );
    
    // Restore original ball state
    ball.velocity() = backup_velocity;
    ball.spin() = backup_spin;
}

void Prediction::Ball::calcVelocityPostCollision(const double &collision_angle) {
    Table table = sharedGameManager.mTable;
    if (!table) return;
    
    auto& balls = table.mBalls();
    auto ball = balls[this->index];
    auto& friction_props = table._frictionProperties();
    
    // Backup original state
    auto backup_velocity = ball.velocity();
    auto backup_spin = ball.spin();
    
    // Apply predicted state
    ball.velocity() = this->velocity;
    ball.spin() = this->spin;
    
    // Call game engine collision response calculation
    static auto calcEngineCollision = 
        M(int64_t, libmain + 0x2ca7064, uintptr_t, FrictionProperties*, const double*);
    calcEngineCollision(ball.instance, &friction_props, &collision_angle);
    
    // Retrieve updated state
    if (ball.velocity() != this->velocity || ball.spin() != this->spin) {
        this->velocity = ball.velocity();
        this->spin = ball.spin();
    }
    
    // Restore original ball state
    ball.velocity() = backup_velocity;
    ball.spin() = backup_spin;
}
