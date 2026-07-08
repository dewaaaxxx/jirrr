#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include "ScreenTable.h"
#include "PowerSlider.h"

using namespace ImGui;

// ============================================================================
// PROFESSIONAL PHYSICS CONSTANTS & EQUATIONS
// ============================================================================

constexpr double maxAngle = 360.0 / (180.0 / M_PI);
constexpr double GRAVITATIONAL_CONSTANT = 9.81;
constexpr double FRICTION_COEFFICIENT = 0.12;           // Cloth-to-ball friction
constexpr double ROLLING_RESISTANCE = 0.003;            // Advanced rolling dynamics
constexpr double BALL_DECELERATION = 190.0;             // Professional deceleration rate
constexpr double COLLISION_DAMPING = 0.95;              // Energy retention in collisions
constexpr double SPIN_DECAY_RATE = 0.015;               // Topspin/backspin dissipation
constexpr double ENGLISH_MULTIPLIER = 1.15;             // Sidespin power modifier

// Luxury physics thresholds for precise shooting
constexpr double OPTIMAL_VELOCITY_RANGE_MIN = 0.5;
constexpr double OPTIMAL_VELOCITY_RANGE_MAX = 8.5;
constexpr double PRECISION_ANGLE_TOLERANCE = 0.001;     // Sub-degree accuracy

// Explosive algorithm parameters
constexpr int SIMULATION_SUBSTEPS = 8;                  // High-precision simulation
constexpr double IMPACT_FORCE_THRESHOLD = 2.0;          // Detect dynamic collisions

// ============================================================================
// ADVANCED PHYSICS CALCULATION FUNCTIONS
// ============================================================================

/**
 * Revolutionary physics engine: Calculates optimal power with exponential curve
 * based on distance, friction, and spin dynamics.
 * 
 * P(d) = sqrt(2 * μ * g * (d + k*s))
 * Where: μ = friction coefficient, g = gravity, d = distance, s = spin factor, k = english multiplier
 */
inline double CalculateOptimalPowerAdvanced(double distance, double spinFactor = 0.0, double englishInfluence = 1.0) {
    // FIX: use the real engine deceleration constant (196.0) instead of
    // FRICTION_COEFFICIENT * GRAVITATIONAL_CONSTANT (0.12 * 9.81 ≈ 1.18),
    // which was ~83x too weak — causing almost every shot to be massively
    // underpowered and miss the pocket.
    double cueDist   = distance * 0.4;  // rough 40/60 split cue-to-ball / ball-to-pocket
    double ballDist  = distance * 0.6;
    double vForBall  = sqrt(2.0 * BALL_DECELERATION * ballDist);
    double vForCue   = sqrt(2.0 * BALL_DECELERATION * cueDist);
    double optimalPower = (vForCue + vForBall) * 1.25;
    return std::min(std::max(optimalPower, 80.0), 666.0);
}

/**
 * Calculates ball trajectory with advanced physics:
 * - Spin-induced curve (Magnus effect)
 * - Friction dissipation
 * - Rolling resistance
 */
inline Point2D PredictBallTrajectory(Point2D initialPos, Point2D velocity, double spinRate = 0.0) {
    // Apply Magnus effect (curve from spin)
    double magnusCoefficient = spinRate * 0.02;
    velocity.x += magnusCoefficient * velocity.y;
    velocity.y -= magnusCoefficient * velocity.x;
    
    // Apply friction and rolling resistance
    double velocityMagnitude = sqrt(velocity.square());
    double decayFactor = exp(-ROLLING_RESISTANCE * velocityMagnitude);
    
    return initialPos + (velocity * decayFactor);
}

/**
 * Helper: Extract spin magnitude from Vec2d spin vector
 */
inline double ExtractSpinMagnitude(const Point2D& spinVector) {
    return sqrt(spinVector.x * spinVector.x + spinVector.y * spinVector.y);
}

/**
 * Premium scoring function: Evaluates shot quality across multiple dimensions
 * Returns a composite score incorporating distance, angle precision, and collision dynamics
 */
inline double CalculateShotQualityScore(
    double distCueToTarget,
    double distTargetToPocket,
    double angleDeviation,
    double spinQuality,
    bool firstHitValid,
    int ballsRemaining
) {
    // Base distance score (logarithmic for luxury precision)
    double distanceScore = log(1.0 + distCueToTarget + distTargetToPocket);
    
    // Angle precision bonus (exponential reward for accuracy)
    double anglePrecisionBonus = exp(-angleDeviation * 100.0);
    
    // Spin quality contribution
    double spinBonus = (1.0 + abs(spinQuality) * 0.5);
    
    // Collision validity multiplier
    double collisionMultiplier = firstHitValid ? 1.2 : 0.5;
    
    // Strategic depth: fewer balls = higher quality rewards
    double strategicDepth = 1.0 + (16.0 - ballsRemaining) * 0.05;
    
    double compositeScore = distanceScore * anglePrecisionBonus * spinBonus * collisionMultiplier * strategicDepth;
    
    return compositeScore;
}

/**
 * Explosive candidate ranking: Multi-tier evaluation system
 * Ranks candidates with exponential differentiation for clear winners
 */
inline double RankCandidate(
    const Candidate& cand,
    double basePower,
    bool firstHitValid,
    int ballsRemaining,
    double preferredSpinRate = 0.0
) {
    // Calculate angle quality (penalize deviation from optimal)
    double angleQuality = cos(cand.angle - atan2(0, cand.power)) * 0.5 + 0.5;
    
    // Power efficiency: reward shots that use less power
    double powerEfficiency = 1.0 - (cand.power / 666.0) * 0.3;
    
    // Shot quality score integration
    double qualityScore = CalculateShotQualityScore(
        sqrt(cand.score),
        0.5,
        abs(cand.angle - preferredSpinRate) * 0.01,
        preferredSpinRate,
        firstHitValid,
        ballsRemaining
    );
    
    // Explosive ranking: exponential separation
    return qualityScore * angleQuality * powerEfficiency * (firstHitValid ? 1.5 : 0.8);
}

// ============================================================================
// CORE AUTOPLAY IMPLEMENTATION
// ============================================================================

static double EaseInOutCubic(double t) {
    return t < 0.5 ? 4 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

double normalizeAngle(double angle) {
    double newAngle = angle;
    if (newAngle >= maxAngle) newAngle = fmod(newAngle, maxAngle);
    else if (newAngle < 0) newAngle = maxAngle - fmod(-newAngle, maxAngle);
    return newAngle;
}

struct AutoPlayMetrics {
    int totalShotsAttempted = 0;
    int successfulShots = 0;
    double averagePower = 0.0;
    double averageDistance = 0.0;
    double averageQualityScore = 0.0;
};

Candidate g_CurrentCandidate = { -1 };
AutoPlayMetrics g_AutoPlayMetrics;

ImVec2 GetPocketScreenPos(int pocketIdx) {
    Table table = sharedGameManager.mTable;
    if (!table) return {};

    auto tableProperties = table.mTableProperties();
    if (!tableProperties) return {};

    auto& pockets = tableProperties.mPockets();
    return WorldToScreen(pockets[pocketIdx]);
}

bool IsShotValid() {
    auto& cand = g_CurrentCandidate;
    if (cand.idx == -1) return false;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    if (nominatedPocket < 6 && cand.pocketIndex != nominatedPocket) return false;

    if (!gPrediction->guiData.balls[0].onTable) return false;
    if (!gPrediction->guiData.balls[cand.idx].originalOnTable) return false;
    if (gPrediction->guiData.balls[cand.idx].onTable) return false;
    if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) return false;

    // FIX: block premature 8-ball pot for ALL classifications, not just ANY.
    // Previously only blocked when myclass==ANY (open table). If player is
    // assigned SOLID/STRIPE but their balls aren't all cleared yet, potting
    // the 8-ball is still a foul/loss — must block it here too.
    auto& ball8 = gPrediction->guiData.balls[8];
    bool only8BallLeft = false;
    if (myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) {
        bool foundOwn = false;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.originalOnTable && b.classification == myclass) { foundOwn = true; break; }
        }
        if (!foundOwn) only8BallLeft = true;
    }
    if (ball8.originalOnTable && !ball8.onTable && !only8BallLeft &&
        myclass != Ball::Classification::EIGHT_BALL) return false;

    auto& firstHit = gPrediction->guiData.collision.firstHitBall;
    if (firstHit) {
        if (only8BallLeft) {
            if (firstHit->classification != Ball::Classification::EIGHT_BALL) return false;
        } else if (myclass == Ball::Classification::ANY) {
            if (firstHit->classification == Ball::Classification::EIGHT_BALL) return false;
        } else {
            if (firstHit->classification != myclass) return false;
        }
    }

    return true;
}

Point2D lastFailedCuePos = { -1000.0, -1000.0 };
static inline Point2D lastSetCuePos = {-1000, -1000};

namespace AutoPlay {
    double lastSetAngle = 0.f;
    bool didSetAngle = false;
    bool bAutoPlaying = false;
    double luxuryPrecisionModifier = 1.0;  // Adjustable precision for luxury mode

    enum State {
        IDLE,
        SCANNING,
        NOMINATING,
        EXECUTING,
    } state = IDLE;
    
    double pendingShotPower = 0.f;
    double pendingShotAngle = 0.f;
    int nominationFrameCounter = 0;
    
    enum ScanMode {
        FAST,
        SLOW,
        PRECISION,  // New luxury precision mode
    } scan = FAST;
    
    enum HumanState {
        HUM_IDLE,
        HUM_THINKING,
        HUM_OVERSHOOTING,
        HUM_CORRECTING,
        HUM_HOLDING,
        HUM_STABILIZING,
        HUM_PULLING,
        HUM_DELAY_BEFORE_SHOT,
    };

    enum SpinPreset { SPIN_TOP = 0, SPIN_BOTTOM, SPIN_LEFT, SPIN_RIGHT, SPIN_CENTER };
    
    // ── Variabel Human State ──
    static inline HumanState humanState = HUM_IDLE;
    static inline double stateStartTime = 0;
    static inline double targetAngle = 0, startAngle = 0, currentOvershootTarget = 0;
    static inline double overshootOffset = 0;
    static inline double aimDuration = 0.8, pullDuration = 0.6;
    static inline double stabilizeDuration = 0.3;
    static inline double startPower = 0, targetPower = 0;
    static inline bool humanShotLocked = false;
    static inline bool g_PredictionLocked = false;
    static inline bool humanNeedsNomination = false;
    static inline int humanNominationPocket = -1;
    static inline SpinPreset spinPreset = SPIN_CENTER;
    static inline bool bAutoSpin = true;
    static bool g_postShotLock = false;
    static double g_postShotAngle = 0.0;
    static double g_postShotPower = 0.0;
    static int g_postShotFrames = 0;
    
    // ── Random engine ──
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> humanDelayDist(0.15, 0.4);
    
    static double nowSec() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }

    void applyAutoSpin() {
    if (!bAutoSpin) return;
    Vec2d spin = {0.0, 0.0};
    constexpr double s = 0.7;
    switch (spinPreset) {
        case SPIN_TOP:    spin = {0.0,  s}; break;
        case SPIN_BOTTOM: spin = {0.0, -s}; break;
        case SPIN_LEFT:   spin = {-s,  0.0}; break;
        case SPIN_RIGHT:  spin = { s,  0.0}; break;
        case SPIN_CENTER: spin = {0.0, 0.0}; break;
    }
    sharedGameManager.mVisualEnglishControl().mEnglish(spin);
    }

    bool shouldAutoPlay() { 
        return !didSetAngle || lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(); 
    }

    void setAimAngle(double angle) {
        if (!sharedGameManager) return;
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        lastSetCuePos = gPrediction->guiData.balls[0].initialPosition;
        vg.mAimAngle(angle);
    }

    void takeShot(double angle, double power) {
        targetAngle = angle;
        targetPower = power;
        startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        stateStartTime = nowSec();
        humanState = HUM_THINKING;
    }
    
    void triggerShot() {
        g_postShotLock = true;
        g_postShotAngle = targetAngle;
        g_postShotPower = pendingShotPower;
        g_postShotFrames = 15;
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }
    
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
        state = IDLE;
        humanState = HUM_IDLE;
    }
    
    void Shoot(double angle, double power = 0.f) {
        applyAutoSpin(); // AIMX SYNC: Apply spin BEFORE simulation so visuals match
        
        setAimAngle(angle);
        gPrediction->determineShotResult(false, angle, power);

        bool nominating = false;
        int nominationMode = sharedGameManager.getPocketNominationMode();
        auto myclass = sharedGameManager.getPlayerClassification();
        if ((nominationMode == 1 && myclass == Ball::Classification::EIGHT_BALL) || (nominationMode == 2 && myclass != Ball::Classification::ANY)) {
            if (g_CurrentCandidate.idx != -1 && sharedGameManager.getNominatedPocket() != g_CurrentCandidate.pocketIndex) {
                nominating = true;
            }
        }

        if (nominating) {
            pendingShotPower = power;
            pendingShotAngle = angle;
            state = NOMINATING;
            nominationFrameCounter = 0;
        } else {
            // BUG FIX #1: JANGAN panggil ClearState() di sini.
            // takeShot() set humanState = HUM_THINKING, tapi ClearState() langsung
            // mereset humanState = HUM_IDLE → human state machine tidak pernah jalan.
            // State cleanup dilakukan di HUM_DELAY_BEFORE_SHOT setelah shot selesai.
            pendingShotPower = power;
            pendingShotAngle = angle;
            state = IDLE; // kembali ke IDLE supaya IDLE→SCANNING guard bisa cek humanState
            takeShot(angle, power);
        }
    }
    
    void ScanPrecision(double angleStep = 0.005f) {
    static double currentScanAngle = 0.0;
    static bool isScanning = false;
    static Point2D lastScanCuePos = { -1000.0, -1000.0 };

    if (g_CurrentCandidate.idx != -1) return;

    if (!isScanning || gPrediction->guiData.balls[0].initialPosition != lastScanCuePos) {
        currentScanAngle = 0.0;
        isScanning = true;
        lastScanCuePos = gPrediction->guiData.balls[0].initialPosition;
    }

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();

    // FIX: Detect if all our balls are gone (only 8-ball left to shoot)
    bool only8BallLeft = false;
    if (myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) {
        bool foundOwnBall = false;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.originalOnTable && b.classification == myclass) {
                foundOwnBall = true;
                break;
            }
        }
        if (!foundOwnBall) only8BallLeft = true;
    }

    int steps = 0;

    while (steps < 15 && currentScanAngle < maxAngle) {
        double angle = currentScanAngle;
        currentScanAngle += angleStep;
        steps++;

        std::vector<double> powers = {666.0, 555.0, 444.0, 333.0, 222.0, 111.0};
        for (double power : powers) {
            gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());

            if (!gPrediction->guiData.balls[0].onTable) continue;

            // FIX: firstHit validation — handle 8-ball turn correctly
            auto firstHit = gPrediction->guiData.collision.firstHitBall;
            if (!firstHit) continue;

            if (only8BallLeft) {
                // When it's time to shoot the 8-ball, first hit MUST be the 8-ball
                if (firstHit->classification != Ball::Classification::EIGHT_BALL) continue;
            } else if (myclass != Ball::Classification::ANY) {
                // Normal turn: first hit must be our own group
                if (firstHit->classification != myclass) continue;
            } else {
                // Open table: can't hit 8-ball first
                if (firstHit->classification == Ball::Classification::EIGHT_BALL) continue;
            }

            // Find valid target that was potted
            int targetIdx = -1;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (!ball.originalOnTable || ball.onTable) continue;

                bool isValid = false;

                if (only8BallLeft) {
                    // FIX: Only 8-ball is valid target now
                    if (ball.classification == Ball::Classification::EIGHT_BALL) isValid = true;
                } else if (myclass == Ball::Classification::ANY) {
                    if (ball.classification != Ball::Classification::EIGHT_BALL &&
                        ball.classification != Ball::Classification::CUE_BALL) {
                        isValid = true;
                    }
                } else {
                    // Match our group only (exclude 8-ball unless only8BallLeft)
                    if (ball.classification == myclass) isValid = true;
                }

                if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) isValid = false;
                if (isValid) { targetIdx = i; break; }
            }

            if (targetIdx == -1) continue;

            // FIX: prevent premature 8-ball pot
            if (targetIdx == 8 && !only8BallLeft) continue;

            // FIX: make sure the specific candidate ball actually went in
            if (gPrediction->guiData.balls[targetIdx].onTable) continue;

            LOGI("ScanPrecision: Found shot - Ball %d, angle %.4f, power %.1f", targetIdx, angle, power);
            g_CurrentCandidate.idx = targetIdx;
            g_CurrentCandidate.angle = angle;
            g_CurrentCandidate.power = power;
            g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[targetIdx].pocketIndex;
            isScanning = false;
            currentScanAngle = 0.0;
            Shoot(angle, power);
            return;
        }
    }

    if (currentScanAngle >= maxAngle) {
        LOGI("ScanPrecision: No shot found, switching to ScanSlow");
        isScanning = false;
        currentScanAngle = 0.0;
        scan = SLOW;
        state = IDLE;
    }
}

    
    void ScanSlow(double angleStep = 0.01f) {
        static double currentScanAngle = 0.0;
        static bool isScanning = false;
        static Point2D lastScanCuePos = { -1000.0, -1000.0 };

        if (g_CurrentCandidate.idx != -1) return;
        
        if (!isScanning || gPrediction->guiData.balls[0].initialPosition != lastScanCuePos) {
            currentScanAngle = 0.0;
            isScanning = true;
            lastScanCuePos = gPrediction->guiData.balls[0].initialPosition;
        }

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        
        int steps = 0;
        bool foundShot = false;
        
        while (steps < 10 && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            std::vector<double> powers = {666.0, 466.0, 266.0, 100.0};
            for (double power : powers) {
                gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
                
                bool isPotentiallyValid = false;
                int targetIdx = -1;
                bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;

                if (isNineBallGame) {
                    int iFoundLowestNumberedBall = -1;
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        if (gPrediction->guiData.balls[i].originalOnTable) {
                            iFoundLowestNumberedBall = i;
                            break;
                        }
                    }

                    auto firstHit = gPrediction->guiData.collision.firstHitBall;
                    if (!firstHit || firstHit->index != iFoundLowestNumberedBall) continue;
                    if (!gPrediction->guiData.balls[0].onTable) continue;

                    int bestPottedIdx = -1;
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        auto& ball = gPrediction->guiData.balls[i];
                        if (ball.originalOnTable && !ball.onTable) {
                            if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) continue;
                            if (i == 9) { bestPottedIdx = 9; break; }
                            if (bestPottedIdx == -1 || i == firstHit->index) bestPottedIdx = i;
                        }
                    }

                    if (bestPottedIdx == -1) continue;
                    g_CurrentCandidate.idx = bestPottedIdx;
                    g_CurrentCandidate.angle = angle;
                    g_CurrentCandidate.power = power;
                    g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[bestPottedIdx].pocketIndex;

                    foundShot = true;
                    Shoot(angle, power);
                    break;
                }

                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        bool isValidTarget = false;
                        if (myclass == Ball::Classification::ANY) {
                            if (ball.classification != Ball::Classification::CUE_BALL && ball.classification != Ball::Classification::EIGHT_BALL) isValidTarget = true;
                        } else {
                            if (ball.classification == myclass) isValidTarget = true;
                        }
                        if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) isValidTarget = false;
                        if (isValidTarget) { targetIdx = i; break; }
                    }
                }

                if (targetIdx != -1) {
                    if (!gPrediction->guiData.balls[0].onTable) continue;
                    if (!gPrediction->guiData.balls[8].onTable && myclass != Ball::Classification::EIGHT_BALL) continue;
                    auto firstHit = gPrediction->guiData.collision.firstHitBall;
                    if (!firstHit) continue;
                    if (myclass == Ball::Classification::ANY) {
                        if (firstHit->classification == Ball::Classification::EIGHT_BALL) continue;
                    } else if (firstHit->classification != myclass) continue;

                    isPotentiallyValid = true;
                    g_CurrentCandidate.idx = targetIdx;
                    g_CurrentCandidate.angle = angle;
                    g_CurrentCandidate.power = power;
                    g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[targetIdx].pocketIndex;
                }

                if (isPotentiallyValid) {
                    // Power refinement: cari power minimum yang masih valid
                    // supaya bola tidak overshoot pocket.
                    // Binary search antara power/2 dan power dalam 5 iterasi.
                    float refinedPower = power;
                    float lo = power * 0.5f, hi = power;
                    for (int r = 0; r < 5; r++) {
                        float mid = (lo + hi) * 0.5f;
                        gPrediction->forceFullSimulation = true;
                        gPrediction->determineShotResult(true, angle, mid, sharedGameManager.getShotSpin());
                        gPrediction->forceFullSimulation = false;
                        bool stillValid = gPrediction->guiData.balls[0].onTable &&
                                         !gPrediction->guiData.balls[targetIdx].onTable == false &&
                                         gPrediction->guiData.balls[targetIdx].pocketIndex == g_CurrentCandidate.pocketIndex;
                        // Benar: bola target harus onTable=false (masuk pocket)
                        stillValid = gPrediction->guiData.balls[0].onTable &&
                                     (!gPrediction->guiData.balls[targetIdx].onTable) &&
                                     (gPrediction->guiData.balls[targetIdx].pocketIndex == g_CurrentCandidate.pocketIndex);
                        if (stillValid) { refinedPower = mid; hi = mid; }
                        else { lo = mid; }
                    }
                    g_CurrentCandidate.power = refinedPower;
                    foundShot = true;
                    Shoot(angle, refinedPower);
                    break;
                }
            }
            if (foundShot) break;
        }

        if (!foundShot && currentScanAngle >= maxAngle) {
            isScanning = false;
            currentScanAngle = 0.0;
            state = IDLE;
        }
    }
    
    void ScanFast(double angleStep = 0.1f) {
        /**
         * EXPLOSIVE FAST SCAN: Dynamic multi-candidate ranking algorithm
         * Uses advanced physics-based scoring to identify explosive shot combinations
         */
        if (g_CurrentCandidate.idx != -1) return;
        if (gPrediction->guiData.balls[0].initialPosition == lastFailedCuePos) return;

        double startingAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        std::vector<Candidate> candidates;
        auto pockets = getPockets();
        auto& cueBall = gPrediction->guiData.balls[0];
        double spinMagnitude = ExtractSpinMagnitude(sharedGameManager.getShotSpin());
        
        bool bFoundLowestNumberedBall = false;
        int iFoundLowestNumberedBall = -1;
        bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;
        int ballsRemaining = gPrediction->guiData.ballsCount;

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (isNineBallGame && bFoundLowestNumberedBall) break;
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;
            if (!bFoundLowestNumberedBall) { bFoundLowestNumberedBall = true; iFoundLowestNumberedBall = i; }

            if (!isNineBallGame) {
                bool isACandidate = myclass == Ball::Classification::ANY ? ball.classification != Ball::Classification::EIGHT_BALL : ball.classification == myclass;
                if (!isACandidate) continue;
            }

            for (int pocketIdx = 0; pocketIdx < pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;
                Point2D pocket = pockets[pocketIdx];
                Point2D toPocket = pocket - ball.initialPosition;
                double distTargetToPocket = sqrt(toPocket.square());
                if (distTargetToPocket < 0.1) continue;
                
                Point2D direction = toPocket * (1.0 / distTargetToPocket);
                Point2D ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);
                Point2D shotLine = ghostBallPos - cueBall.initialPosition;
                double distCueToTarget = sqrt(shotLine.square());
                double angle = atan2(shotLine.y, shotLine.x);
                if (angle < 0) angle += 2 * M_PI;
                
                // REVOLUTIONARY PHYSICS: Advanced power calculation
                double power = CalculateOptimalPowerAdvanced(distCueToTarget + distTargetToPocket, spinMagnitude, 1.0);
                double compositeScore = distCueToTarget + distTargetToPocket;
                
                if (power > 666.0) power = 666.0;
                candidates.push_back({i, angle, compositeScore, pocketIdx, power});
            }
        }
        
        // Sort by total distance (cue->ball + ball->pocket) — closest/easiest
        // shot first. The previous RankCandidate() function used
        // `cos(cand.angle - atan2(0, cand.power))` as "angle quality", but
        // atan2(0, positiveNumber) is always 0, so this was really just
        // cos(cand.angle) — a bonus/penalty based on the shot's absolute
        // direction in world space, completely unrelated to shot difficulty.
        // That caused shots aimed roughly rightward (~0 rad) to score higher
        // than easy direct shots aimed in other directions, and sometimes
        // preferred bank shots that happened to have a "good" absolute angle
        // over simpler direct shots.
        std::sort(candidates.begin(), candidates.end());
        
        bool foundShot = false;
        for (const auto& cand : candidates) {
            double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(cand.angle));
            gPrediction->determineShotResult(true, angle, cand.power, sharedGameManager.getShotSpin(), cand);
            if (!gPrediction->firstHitIsTarget) continue;
            if (!gPrediction->guiData.balls[0].onTable) continue;

            if (isNineBallGame) {
                auto firstHit = gPrediction->guiData.collision.firstHitBall;
                if (!firstHit || firstHit->index != cand.idx) continue;

                int bestPottedIdx = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) continue;
                        if (i == 9) { bestPottedIdx = 9; break; }
                        if (bestPottedIdx == -1 || i == cand.idx) bestPottedIdx = i;
                    }
                }
                if (bestPottedIdx == -1) continue;
                g_CurrentCandidate = cand;
                g_CurrentCandidate.idx = bestPottedIdx;
                g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[bestPottedIdx].pocketIndex;
                foundShot = true;
                Shoot(angle, cand.power);
                break;
            }

            if (gPrediction->guiData.balls[cand.idx].onTable) continue;
            if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) continue;

            // FIX: previously checked if ANY ball of our group was potted,
            // meaning a shot that potted an OPPONENT's ball could still pass
            // if one of our balls happened to also go in during the same
            // simulation. Now we specifically verify that `cand.idx` (the
            // intended target ball) is the one that actually got potted.
            auto& targetBall = gPrediction->guiData.balls[cand.idx];
            bool isAngleGood = (targetBall.originalOnTable && !targetBall.onTable);

            if (isAngleGood && gPrediction->guiData.collision.firstHitBall) {
                 auto firstHit = gPrediction->guiData.collision.firstHitBall;
                 if (myclass != Ball::Classification::ANY && firstHit->classification != myclass) isAngleGood = false;
                 else if (myclass == Ball::Classification::ANY && firstHit->classification == Ball::Classification::EIGHT_BALL) isAngleGood = false;
            }

            if (isAngleGood && !gPrediction->guiData.balls[0].onTable) isAngleGood = false;
            
            // FIX: check only8BallLeft same way as IsShotValid — don't reject
            // the 8-ball pot when it IS the right time to shoot it.
            auto& eightBallRef = gPrediction->guiData.balls[8];
            bool only8Left = false;
            if (myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) {
                bool foundOwn = false;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& b = gPrediction->guiData.balls[i];
                    if (b.originalOnTable && b.classification == myclass) { foundOwn = true; break; }
                }
                if (!foundOwn) only8Left = true;
            }
            if (isAngleGood && eightBallRef.originalOnTable && !eightBallRef.onTable
                && !only8Left && myclass != Ball::Classification::EIGHT_BALL) {
                isAngleGood = false;
            }
            
            if (isAngleGood) {
                g_CurrentCandidate = cand;
                foundShot = true;
                Shoot(angle, cand.power);
                break;
            }
        }

        if (!foundShot) {
            lastFailedCuePos = cueBall.initialPosition;
            scan = SLOW;
        }
    }

    bool isAnimationActive() {
        auto visualCue = sharedGameManager.mVisualCue();
        if (!visualCue) return true;
        auto _powerBarView = F(ptr, visualCue + 0x510);
        if (!_powerBarView) return true;
        
        uintptr_t activeAction = M(uintptr_t, libmain + 0x2de6f30, ptr)(_powerBarView);
        return (activeAction != 0); 
    }
    
    void Update() {
        // FIX: check both conditions:
        // 1. bAutoPlay = "Enable Auto Play" toggle in menu (feature enabled at all)
        // 2. bAutoPlaying = play/pause button on screen (user pressed play)
        // Previously only bAutoPlay was checked, so the play/pause button had
        // no effect on whether scanning/shooting actually happened.
        if (!persistent_bool[O("bAutoPlay")] || !bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            NativeTouchesEnd(5, 0, 0);      // joystick
            NativeTouchesEnd(powerSlider.TouchIndex, 0, 0); // slider
            state = IDLE;
            return;
        }
        
        buttonClicker.Update();
        powerSlider.Update();

        if (isAnimationActive()) return;

        /*if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            state = IDLE;
            return;
        }*/

        if (state == IDLE) {
            // BUG FIX #2: Kalau humanState != HUM_IDLE, artinya human state machine
            // sedang jalan (aim, pull, dll). Jangan langsung SCANNING lagi —
            // biarkan human machine selesai dulu. Tanpa guard ini, IDLE langsung
            // jadi SCANNING → ScanFast lagi → Shoot lagi → loop selamanya.
            if (humanState == HUM_IDLE) {
                state = SCANNING;
                scan = FAST;
            }
        } else if (state == SCANNING) {
            if (scan == FAST) ScanFast();
            else if (scan == SLOW) ScanSlow(0.003f);
        //    else if (scan == PRECISION) ScanPrecision(0.005f);
        } else if (state == NOMINATING) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 10) {
                buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
            }
            if (nominationFrameCounter > 20 && !buttonClicker.Active) {
                uint nominatedPocket = sharedGameManager.getNominatedPocket();
                if (nominatedPocket == g_CurrentCandidate.pocketIndex) {
                    targetAngle = pendingShotAngle;
                    g_PredictionLocked = true;
        
                    // 🔥 Re-validasi pocketIndex
                    gPrediction->determineShotResult(true, pendingShotAngle, pendingShotPower,
                                                      sharedGameManager.getShotSpin(), g_CurrentCandidate);
                    if (g_CurrentCandidate.idx >= 0 && g_CurrentCandidate.idx < gPrediction->guiData.ballsCount) {
                        int freshPocket = gPrediction->guiData.balls[g_CurrentCandidate.idx].pocketIndex;
                        if (freshPocket >= 0 && freshPocket < 6) {
                            g_CurrentCandidate.pocketIndex = freshPocket;
                        }
                    }
        
                    // 🔥 Transisi ke Human State Machine (tanpa EXECUTING)
                    applyAutoSpin(); // AIMX SYNC: Apply spin BEFORE simulation so visuals match
                    humanShotLocked = true;
                    humanState = HUM_THINKING;
                    stateStartTime = nowSec() + 0.3;
                    startAngle = pendingShotAngle;
                    state = IDLE; // ← Kembali ke IDLE, tapi humanState tetap jalan
                } else {
                    if (nominationFrameCounter > 40) {
                        nominationFrameCounter = 0;
                    }
                }
            }
        }
            // ─── HUMAN STATE MACHINE ────────────────────────────────────────────
        // ─── HIDE PREDICTION LINES DURING HUMAN STATE ──────────────────────────
if (humanState != HUM_IDLE) {
    persistent_bool["bESP_DrawPredictionLine"] = false;  // Hilang saat drag
} else {
    persistent_bool["bESP_DrawPredictionLine"] = true;   // Muncul saat idle
}
        if (humanState != HUM_IDLE) {
        double now = nowSec();

        auto UpdateJoystickVisuals = [&](double angle) {
            float jX = Width * 0.83f;
            float jY = Height * 0.82f;
            float jR = 65.0f;
            float tX = jX + cos(angle) * jR;
            float tY = jY + sin(angle) * jR;
            NativeTouchesMove(5, tX, tY);

            ImDrawList* fg = ImGui::GetForegroundDrawList();
    if (fg) {
        fg->AddCircleFilled(ImVec2(tX, tY), 25.0f, IM_COL32(255, 255, 255, 100));
        fg->AddCircle(ImVec2(tX, tY), 25.0f, IM_COL32(255, 255, 255, 200), 0.0f, 2.0f);
    }
        };
    
        // 1. HUM_THINKING (0.5s pause)
        if (humanState == HUM_THINKING) {
            if (now >= stateStartTime) {
                overshootOffset = (gen() % 2 == 0 ? 1 : -1) * 0.058;
                currentOvershootTarget = targetAngle + overshootOffset;
                stateStartTime = now;
                humanState = HUM_OVERSHOOTING;
                NativeTouchesBegin(5, Width * 0.83f, Height * 0.82f);
            }
            return;
        }
    
        // 2. HUM_OVERSHOOTING (1.1s overshoot)
        if (humanState == HUM_OVERSHOOTING) {
            double t = (now - stateStartTime) / 1.1;
            if (t >= 1.0) {
                setAimAngle(currentOvershootTarget);
                UpdateJoystickVisuals(currentOvershootTarget);
                stateStartTime = now;
                humanState = HUM_CORRECTING;
            } else {
                double ease = EaseInOutCubic(t);
                double curAngle = startAngle + (currentOvershootTarget - startAngle) * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }
    
        // 3. HUM_CORRECTING (0.35s snap back)
        if (humanState == HUM_CORRECTING) {
            double t = (now - stateStartTime) / 0.35;
            double nudgeAngle = targetAngle + (overshootOffset > 0 ? 1 : -1) * (1.5 * M_PI / 180.0);
            if (t >= 1.0) {
                setAimAngle(nudgeAngle);
                UpdateJoystickVisuals(nudgeAngle);
                stateStartTime = now;
                humanState = HUM_HOLDING;
            } else {
                double ease = EaseInOutCubic(t);
                double curAngle = currentOvershootTarget + (nudgeAngle - currentOvershootTarget) * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }
    
        // 4. HUM_HOLDING (0.4s hold at target)
        if (humanState == HUM_HOLDING) {
            double t = (now - stateStartTime) / 0.40;
            double nudgeAngle = targetAngle + (overshootOffset > 0 ? 1 : -1) * (1.5 * M_PI / 180.0);
            if (t >= 1.0) {
                setAimAngle(targetAngle);
                NativeTouchesMove(5, Width * 0.83f + cos(targetAngle) * 65.0f,
                                     Height * 0.82f + sin(targetAngle) * 65.0f);
                stateStartTime = now;
                humanState = HUM_STABILIZING;
            } else {
                double ease = sin(t * M_PI_2);
                double curAngle = nudgeAngle + (targetAngle - nudgeAngle) * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }
    
        // 5. HUM_STABILIZING (0.4s stabilize + start slider)
        if (humanState == HUM_STABILIZING) {
            NativeTouchesMove(5, Width * 0.83f + cos(targetAngle) * 65.0f,
                                 Height * 0.82f + sin(targetAngle) * 65.0f);
            setAimAngle(targetAngle);
    
            if (now - stateStartTime >= 0.4) {
                NativeTouchesEnd(5, Width * 0.83f + cos(targetAngle) * 65.0f,
                                    Height * 0.82f + sin(targetAngle) * 65.0f);
    
                float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
                float sliderX = Width * sliderXPercent;
                if (persistent_int[O("iPowerBarSide")] == 1)
                    sliderX = Width * (1.0f - sliderXPercent);
                float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
                float sliderYEnd = Height * persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sliderX - 20.0f, sliderYStart, 40.0f, sliderYEnd - sliderYStart);
    
                powerSlider.SimulateDrag(sliderRect, targetPower, 1.5f, 0.7f);
                stateStartTime = now;
                humanState = HUM_PULLING;
            }
            return;
        }
    
        // 6. HUM_PULLING (wait for slider to finish)
        if (humanState == HUM_PULLING) {
            if (powerSlider.Active) return;
            // Sync gPrediction dengan angle+power aktual sebelum slider release.
            // Ini penting supaya state gPrediction fresh dan konsisten dengan
            // angle yang sekarang di-apply ke game, bukan sisa state dari scan.
            gPrediction->forceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, targetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gPrediction->forceFullSimulation = false;
            stateStartTime = now;
            humanState = HUM_DELAY_BEFORE_SHOT;
            return;
        }
    
        // 7. HUM_DELAY_BEFORE_SHOT (0.4s cooldown, lalu cleanup)
        if (humanState == HUM_DELAY_BEFORE_SHOT) {
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.4) {
                humanShotLocked = false;
                ClearState();
                state = IDLE;
                humanState = HUM_IDLE;
            }
            return;
        }
    }
    bool isPlayerTurn = sharedGameManager.mStateManager().isPlayerTurn();
    if (isPlayerTurn && bAutoSpin) applyAutoSpin();
}
};
