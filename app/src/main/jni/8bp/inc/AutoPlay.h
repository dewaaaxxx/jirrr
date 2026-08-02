#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include "ScreenTable.h"
#include "ButtonClicker.h"
#include "PowerSlider.h"
#include <random>
using namespace ImGui;

// ============================================================================
// PROFESSIONAL PHYSICS CONSTANTS & EQUATIONS
// ============================================================================

constexpr double maxAngle = 360.0 / (180.0 / M_PI);
constexpr double GRAVITATIONAL_CONSTANT = 9.81;
constexpr double FRICTION_COEFFICIENT = 0.12;           // Cloth-to-ball friction
constexpr double ROLLING_RESISTANCE = 0.003;            // Advanced rolling dynamics
constexpr double BALL_DECELERATION = 196.0;             // Professional deceleration rate
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
    double effectiveDistance = distance + (ENGLISH_MULTIPLIER * englishInfluence * abs(spinFactor) * 0.5);
    double optimalPower = sqrt(2.0 * FRICTION_COEFFICIENT * GRAVITATIONAL_CONSTANT * effectiveDistance);
    
    // Apply exponential curve for smoother power delivery
    optimalPower = optimalPower * (1.0 + (spinFactor * 0.1));
    
    // Cap at maximum safe power
    return std::min(optimalPower, 666.0);
}

static double EaseInOutCubic(double t) {
    return t < 0.5 ? 4 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

static std::uniform_real_distribution<> humanDelayDist(0.15, 0.3);
static std::uniform_real_distribution<> humanOvershootDist(0.5, 1.5);

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

    auto& ball8 = gPrediction->guiData.balls[8];
    if (myclass == Ball::Classification::ANY && ball8.originalOnTable && !ball8.onTable) return false;

    auto& firstHit = gPrediction->guiData.collision.firstHitBall;
    if (firstHit) {
        if (myclass == Ball::Classification::ANY) {
            if (firstHit->classification == Ball::Classification::EIGHT_BALL) return false;
        } else if (firstHit->classification != myclass) return false;
    }

    return true;
}

Point2D lastFailedCuePos = { -1000.0, -1000.0 };
Point2D lastSetCuePos = { -1000.0, -1000.0 };

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
    static inline bool bAutoSpin = false;
    // FIX ROOT CAUSE: Spin yang dipakai saat scan HARUS SAMA dengan spin saat tembak.
    // Kalau berbeda: simulasi scan → hasil A, simulasi display dengan spin lain → hasil B
    // → prediction line kelihatan masuk sebelum tembak, tapi setelah tembak meleset.
    // Solusi: lock spin pada saat scan dimulai, gunakan spin yang sama untuk semua
    // determineShotResult call (scan, display, tembak) sampai shot selesai.
    static inline Vec2d lockedShotSpin = {0.0, 0.0};
    static inline bool spinIsLocked = false;
    // FIX POWER: Power yang dipakai scan harus sama persis dengan yang ditembak.
    // confirmedPower dari scan disimpan di g_CurrentCandidate.power dan di targetPower.
    // Konversi ke mPower() via ShotPowerToPower() sudah benar — tidak perlu diubah.
    // Yang penting: power sweep di scan harus cover range yang realistis
    // berdasarkan getShotPower() (skala simulasi), bukan mPower() (skala 0-1).
    static bool g_postShotLock = false;
    static double g_postShotAngle = 0.0;
    static double g_postShotPower = 0.0;
    static int g_postShotFrames = 0;
    static inline float powerMin = 100.0f;
    static inline float powerMax = 666.0f;
    
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

    bool shouldAutoPlay() { return !didSetAngle || lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(); }

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
   //     setAimAngle(angle);
        stateStartTime = nowSec();
        humanState = HUM_THINKING;
    }
    
    void triggerShot() {
        g_postShotLock = true;
        g_postShotAngle = targetAngle;
        // BUG FIX: pendingShotPower tidak selalu sync dengan targetPower.
        // takeShot() set targetPower, tapi triggerShot() baca pendingShotPower → power salah/lebih pelan.
        // Pakai targetPower yang di-set oleh takeShot() dan di-hold sepanjang human state machine.
        g_postShotPower = targetPower;
        g_postShotFrames = 15;
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }
    
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
        state = IDLE;
        humanState = HUM_IDLE;
        spinIsLocked = false; // Unlock spin supaya scan berikutnya lock spin fresh
        humanShotLocked = false;
    }
    
    void Shoot(double angle, double power = 0.f) {
        if (!spinIsLocked) {
            lockedShotSpin = sharedGameManager.getShotSpin();
            spinIsLocked = true;
        }
        gPrediction->determineShotResult(true, angle, power, lockedShotSpin);

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
            pendingShotPower = power;
            pendingShotAngle = angle;
            takeShot(angle, power);
        }
    }

    
    
    static double calcAdaptivePower(double distCG, double totalDist) {
        double rawPow = sqrt(2.0 * 196.0 * totalDist);
        double mult   = 1.0;
        if (distCG < 25.0) {
            // Linear: short distance → lower power, prevents overshoot
            mult = 0.62 + (distCG / 25.0) * 0.38;
        } else if (totalDist > 160.0) {
            // Long / over-rail shot: gentle power boost
            double boost = ((totalDist - 160.0) / 100.0) * 0.18;
            if (boost > 0.18) boost = 0.18;
            mult = 1.0 + boost;
        }
        double power = rawPow * mult;
        if (power > 480.0) power = 480.0;
        if (power < 80.0)  power = 80.0;
        return power;
    }
    
    static double CalculateTableClusterScore(const Prediction::SceneData& data) {
        double clusterScore = 0.0;
        for (int i = 1; i < data.ballsCount; i++) {
            if (!data.balls[i].onTable) continue;
            for (int j = i + 1; j < data.ballsCount; j++) {
                if (!data.balls[j].onTable) continue;
                double distSq = (data.balls[i].initialPosition - data.balls[j].initialPosition).square();
                if (distSq < (4.5 * BALL_RADIUS * 4.5 * BALL_RADIUS)) {
                    clusterScore += 1.0;
                }
            }
        }
        return clusterScore;
    }
    
    void ScanPrecision(double angleStep = 0.002f) {
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
        double spinMagnitude = ExtractSpinMagnitude(sharedGameManager.getShotSpin());
        
        int steps = 0;
        bool foundShot = false;
        double bestQualityScore = -1.0;
        Candidate bestCandidate = { -1 };
        
        while (steps < 25 && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;
    
            std::vector<double> powers = {150.0, 200.0, 275.0, 350.0, 425.0, 500.0, 580.0, 666.0};
            for (double power : powers) {
                gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
                
                bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;
                auto& cueBall = gPrediction->guiData.balls[0];
                auto& pockets = getPockets();
    
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
                    if (!cueBall.onTable) continue;
    
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
    
                    // fix: pakai bestPottedIdx bukan targetIdx
                    auto& targetBall = gPrediction->guiData.balls[bestPottedIdx];
                    double dx1 = targetBall.initialPosition.x - cueBall.initialPosition.x;
                    double dy1 = targetBall.initialPosition.y - cueBall.initialPosition.y;
                    double distCueToTarget = sqrt(dx1*dx1 + dy1*dy1);
    
                    double dx2 = pockets[targetBall.pocketIndex].x - targetBall.predictedPosition.x;
                    double dy2 = pockets[targetBall.pocketIndex].y - targetBall.predictedPosition.y;
                    double distTargetToPocket = sqrt(dx2*dx2 + dy2*dy2);
                    double angleDeviation = distTargetToPocket / 10.0;
    
                    double qualityScore = CalculateShotQualityScore(
                        distCueToTarget, distTargetToPocket, angleDeviation,
                        spinMagnitude, true, gPrediction->guiData.ballsCount - 1
                    );
                    
                    if (qualityScore > bestQualityScore) {
                        bestQualityScore = qualityScore;
                        bestCandidate = {bestPottedIdx, angle, (double)power,
                                        (uint)targetBall.pocketIndex};
                    }
                    continue; // fix: jangan fallthrough ke 8-ball
                }
    
                int targetIdx = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        bool isValidTarget = false;
                        if (myclass == Ball::Classification::ANY) {
                            if (ball.classification != Ball::Classification::CUE_BALL &&
                                ball.classification != Ball::Classification::EIGHT_BALL)
                                isValidTarget = true;
                        } else {
                            if (ball.classification == myclass) isValidTarget = true;
                        }
                        if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) isValidTarget = false;
                        if (isValidTarget) { targetIdx = i; break; }
                    }
                }
    
                if (targetIdx == -1) continue;
                if (!cueBall.onTable) continue;
                if (!gPrediction->guiData.balls[8].onTable && myclass != Ball::Classification::EIGHT_BALL) continue;
                auto firstHit = gPrediction->guiData.collision.firstHitBall;
                if (!firstHit) continue;
                if (myclass == Ball::Classification::ANY) {
                    if (firstHit->classification == Ball::Classification::EIGHT_BALL) continue;
                } else if (firstHit->classification != myclass) continue;
    
                auto& targetBall = gPrediction->guiData.balls[targetIdx];
                double dx1 = targetBall.initialPosition.x - cueBall.initialPosition.x;
                double dy1 = targetBall.initialPosition.y - cueBall.initialPosition.y;
                double distCueToTarget = sqrt(dx1*dx1 + dy1*dy1);
    
                double dx2 = pockets[targetBall.pocketIndex].x - targetBall.predictedPosition.x;
                double dy2 = pockets[targetBall.pocketIndex].y - targetBall.predictedPosition.y;
                double distTargetToPocket = sqrt(dx2*dx2 + dy2*dy2);
                double angleDeviation = distTargetToPocket / 10.0;
    
                double qualityScore = CalculateShotQualityScore(
                    distCueToTarget, distTargetToPocket, angleDeviation,
                    spinMagnitude, true, gPrediction->guiData.ballsCount - 1
                );
                
                if (qualityScore > bestQualityScore) {
                    bestQualityScore = qualityScore;
                    bestCandidate = {targetIdx, angle, (double)power,
                                    (uint)targetBall.pocketIndex};
                }
            }
        }
    
        if (bestCandidate.idx != -1) {
            g_CurrentCandidate = bestCandidate;
            g_AutoPlayMetrics.averageQualityScore = bestQualityScore;
            Shoot(bestCandidate.angle, bestCandidate.power);
            foundShot = true;
        }
    
        if (!foundShot && currentScanAngle >= maxAngle) {
            isScanning = false;
            currentScanAngle = 0.0;
            state = IDLE;
        }
    }
    
    void ScanSlow(double angleStep = 0.005f) {
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
        
        while (steps < 25 && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            std::vector<double> powers = {150.0, 250.0, 350.0, 450.0, 550.0, 666.0};
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
                    foundShot = true;
                    Shoot(angle, power);
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

                Point2D pocket  = pockets[pocketIdx];
                Point2D toPock  = pocket - ball.initialPosition;
                double  distTP  = sqrt(toPock.square());
                if (distTP < 0.1) continue;

                Point2D dir   = toPock * (1.0 / distTP);
                Point2D ghost = ball.initialPosition - dir * (2.0 * BALL_RADIUS);
                Point2D sVec  = ghost - cueBall.initialPosition;
                double  distCG = sqrt(sVec.square());
                if (distCG < 0.01) continue;

                double angle = atan2(sVec.y, sVec.x);
                if (angle < 0) angle += 2.0 * M_PI;

                // Cut-angle penalty: cosTheta=1 (full hit) = no penalty
                //                    cosTheta=0 (90° cut) = +2×distCG penalty
                double cosTheta = (sVec.x * dir.x + sVec.y * dir.y) / distCG;
                double score    = distCG * (2.0 - cosTheta) + distTP;

                // Auto-adaptive power: reduces near-ball overshoot, boosts long shots
                double power = calcAdaptivePower(distCG, distCG + distTP);
                
                candidates.push_back({i, angle, score, pocketIdx, power});
            }
        }
        
        std::sort(candidates.begin(), candidates.end());

        // Simpan state meja sebelum simulasi untuk hitung cluster improvement
        Prediction::SceneData savedGuiData = gPrediction->guiData;
        double initialClusterScore = CalculateTableClusterScore(savedGuiData);

        bool foundShot = false;
        Candidate bestClusterCand;
        int bestClusterScore = INT_MIN;
        bool hasBestCluster = false;
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

            bool isAngleGood = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                Prediction::Ball& ball = gPrediction->guiData.balls[i];
                bool match = (myclass == Ball::Classification::ANY) ? (ball.classification != Ball::Classification::CUE_BALL && ball.classification != Ball::Classification::EIGHT_BALL) : (ball.classification == myclass);
                if (match && ball.originalOnTable && !ball.onTable) isAngleGood = true;
            }

            if (isAngleGood && gPrediction->guiData.collision.firstHitBall) {
                 auto firstHit = gPrediction->guiData.collision.firstHitBall;
                 if (myclass != Ball::Classification::ANY && firstHit->classification != myclass) isAngleGood = false;
                 else if (myclass == Ball::Classification::ANY && firstHit->classification == Ball::Classification::EIGHT_BALL) isAngleGood = false;
            }

            if (isAngleGood && !gPrediction->guiData.balls[0].onTable) isAngleGood = false;
            
            auto& eightBallRef = gPrediction->guiData.balls[8];
            if (isAngleGood && (eightBallRef.originalOnTable && !eightBallRef.onTable) && myclass != Ball::Classification::EIGHT_BALL) isAngleGood = false;
            
            if (isAngleGood) {
                double finalClusterScore = CalculateTableClusterScore(gPrediction->guiData);
                double clusterImprovement = initialClusterScore - finalClusterScore;
                int openingBonus = (clusterImprovement > 0) ? (int)(clusterImprovement * 50) : 0;
                int totalScore = openingBonus;

                if (!hasBestCluster || totalScore > bestClusterScore) {
                    bestClusterScore = totalScore;
                    bestClusterCand = cand;
                    hasBestCluster = true;
                }
                // Tidak break — terus scan semua kandidat untuk cari yang terbaik
            }
        }

        if (hasBestCluster) {
            double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(bestClusterCand.angle));
            LOGI("AutoPlay: Found best angle %f power %f clusterScore=%d", angle, bestClusterCand.power, bestClusterScore);
            g_CurrentCandidate = bestClusterCand;
            foundShot = true;
            Shoot(angle, bestClusterCand.power);
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
        buttonClicker.Update();
        
        static int animStuckCounter = 0;
        if (isAnimationActive()) {
            animStuckCounter++;
            if (animStuckCounter > 300) { // ~5 detik di 60fps
                animStuckCounter = 0;
                ClearState(); // reset candidate juga
            }
            return;
        }
        animStuckCounter = 0;

        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            if (humanState != HUM_IDLE) return;
            if (state == EXECUTING) return;
            g_CurrentCandidate.idx = -1;
            lastFailedCuePos = { -1000.0, -1000.0 };
            spinIsLocked = false;
            state = IDLE;
            scan = FAST;
            NativeTouchesEnd(5, 0, 0);
            ClearState();
            return;
        }

        if (state == IDLE) {
            if (humanState == HUM_IDLE) {
                state = SCANNING;
                scan = FAST;
            }
        } else if (state == SCANNING) {
            if (scan == FAST) ScanFast();
            else if (scan == SLOW) ScanSlow(0.003f);
            else if (scan == PRECISION) ScanPrecision(0.005f);
        } else if (state == NOMINATING) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 10) {
                buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
            }
            
            // TAMBAH INI — timeout paksa setelah 120 frame (~2 detik)
            if (nominationFrameCounter > 120) {
                buttonClicker.Active = false; // paksa reset
                nominationFrameCounter = 0;
                ClearState();
                state = IDLE;
                return;
            }
            
            if (nominationFrameCounter > 20 && !buttonClicker.Active) {
                uint nominatedPocket = sharedGameManager.getNominatedPocket();
                if (nominatedPocket == (uint)g_CurrentCandidate.pocketIndex) {
                    // Nominasi confirmed — re-validasi shot dengan full simulation
                    
                    gPrediction->determineShotResult(true, pendingShotAngle, pendingShotPower,
                                                     lockedShotSpin, g_CurrentCandidate);
                    

                    // Scratch check setelah nominasi
                    if (!gPrediction->guiData.balls[0].onTable) {
                        LOGI("[AUTOPLAY] Post-nomination scratch detected, cancelling");
                        ClearState();
                        return;
                    }

                    // Update pocketIndex dari simulasi fresh
                    if (g_CurrentCandidate.idx >= 0 && g_CurrentCandidate.idx < gPrediction->guiData.ballsCount) {
                        int freshPocket = gPrediction->guiData.balls[g_CurrentCandidate.idx].pocketIndex;
                        if (freshPocket >= 0 && freshPocket < 6) {
                            g_CurrentCandidate.pocketIndex = freshPocket;
                        }
                    }

                    // Cek bola target masih valid ke pocket yang dinominasi
                    if (g_CurrentCandidate.pocketIndex != (int)nominatedPocket) {
                        LOGI("[AUTOPLAY] Target pocket mismatch after nomination, cancelling");
                        ClearState();
                        return;
                    }

                    // Start human state machine
                    applyAutoSpin();
                    startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
                    targetAngle = pendingShotAngle;
                    targetPower = pendingShotPower;
                    humanShotLocked = true;
                    humanState = HUM_THINKING;
                    stateStartTime = nowSec() + 0.3;
                    state = EXECUTING; // EXECUTING biar tidak di-reset oleh isPlayerTurn check
                } else {
                    // Retry setiap 30 frame, timeout 150 frame
                    if (nominationFrameCounter % 30 == 0) {
                        if (nominationFrameCounter > 150) {
                            LOGI("[AUTOPLAY] Nomination timeout, resetting");
                            ClearState();
                            lastFailedCuePos = gPrediction->guiData.balls[0].initialPosition;
                        } else {
                            LOGI("[AUTOPLAY] Nomination retry #%d", nominationFrameCounter / 30);
                            buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
                        }
                    }
                }
            }
        }
            // ─── HUMAN STATE MACHINE ────────────────────────────────────────────
        // ─── HIDE PREDICTION LINES DURING HUMAN STATE ──────────────────────────
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
        fg->AddCircleFilled(ImVec2(tX, tY), 10.0f, IM_COL32(255, 255, 255, 100));
        fg->AddCircle(ImVec2(tX, tY), 10.0f, IM_COL32(255, 255, 255, 200), 0.0f, 2.0f);
    }
        };
    
        // 1. HUM_THINKING (0.5s pause)
        if (humanState == HUM_THINKING) {
            if (now >= stateStartTime + humanDelayDist(gen)) {
                double overshootDeg = humanOvershootDist(gen);
                overshootOffset = (gen() % 2 == 0 ? 1.0 : -1.0) * (overshootDeg * PI / 180.0);
                currentOvershootTarget = targetAngle + overshootOffset;
                stateStartTime = now;
                humanState = HUM_OVERSHOOTING;
                NativeTouchesBegin(5, Width * 0.83f, Height * 0.82f);
            }
            return;
        }

        // 2. ROTATION (1.1s smooth sweep to overshoot)
        if (humanState == HUM_OVERSHOOTING) {
            double t = (now - stateStartTime) / 0.4;
            if (t >= 1.0) {
                setAimAngle(currentOvershootTarget);
                UpdateJoystickVisuals(currentOvershootTarget);
                stateStartTime = now;
                humanState = HUM_CORRECTING;
            } else {
                double ease = EaseInOutCubic(t);
                double normalizedStart = normalizeAngle(startAngle);
                double normalizedTarget = normalizeAngle(currentOvershootTarget);
                double delta = normalizedTarget - normalizedStart;
                if (delta > M_PI) delta -= 2.0 * M_PI; if (delta < -M_PI) delta += 2.0 * M_PI;
                double curAngle = normalizedStart + delta * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }

        // 3. ELASTIC SNAP BACK (0.35s)
        if (humanState == HUM_CORRECTING) {
            double t = (now - stateStartTime) / 0.25;
            double dirSign = (overshootOffset > 0) ? 1.0 : -1.0;
            double nudgeAngle = targetAngle - dirSign * (0.3 * PI / 180.0);
            
            if (t >= 1.0) {
                setAimAngle(nudgeAngle);
                UpdateJoystickVisuals(nudgeAngle);
                stateStartTime = now;
                humanState = HUM_HOLDING;
            } else {
                double ease = EaseInOutCubic(t);
                double normalizedStart = normalizeAngle(currentOvershootTarget);
                double normalizedTarget = normalizeAngle(nudgeAngle);
                double delta = normalizedTarget - normalizedStart;
                if (delta > M_PI) delta -= 2.0 * M_PI; if (delta < -M_PI) delta += 2.0 * M_PI;
                double curAngle = normalizedStart + delta * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }

        // 3b. HOLD TOUCH AT TARGET (0.40s slow nudge/adjustment to exact target + hold)
        if (humanState == HUM_HOLDING) {
            double t = (now - stateStartTime) / 0.2;
            double dirSign = (overshootOffset > 0) ? 1.0 : -1.0;
            double nudgeAngle = targetAngle - dirSign * (0.3 * PI / 180.0);
            
            if (t >= 1.0) {
                setAimAngle(targetAngle);
                UpdateJoystickVisuals(targetAngle);
                float jX = Width * 0.83f;
                float jY = Height * 0.82f;
                float jR = 65.0f;
                NativeTouchesMove(5, jX + (float)cos(targetAngle) * jR, 
                                     jY + (float)sin(targetAngle) * jR);
                stateStartTime = now;
                humanState = HUM_STABILIZING;
            } else {
                double ease = sin(t * M_PI_2);
                double normalizedStart = normalizeAngle(nudgeAngle);
                double normalizedTarget = normalizeAngle(targetAngle);
                double delta = normalizedTarget - normalizedStart;
                if (delta > M_PI) delta -= 2.0 * M_PI; if (delta < -M_PI) delta += 2.0 * M_PI;
                // Subtle micro-jitter (almost invisible)
                double jitter = 0.0;
                if (t > 0.3) {
                    jitter = ((gen() % 100) / 100.0 - 0.5) * (0.15 * PI / 180.0);
                }
                double curAngle = normalizedStart + delta * ease + jitter;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            return;
        }

        // 4. STABILIZE & LOCK (0.4s) - joystick still held from HUM_HOLDING
        if (humanState == HUM_STABILIZING) {
            // Lock angle ke engine setiap frame
            setAimAngle(targetAngle);
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(targetAngle);
            NativeTouchesMove(5, Width * 0.83f + cos(targetAngle) * 65.0f,
                                 Height * 0.82f + sin(targetAngle) * 65.0f);

            if (now - stateStartTime >= 0.4) {
                NativeTouchesEnd(5, Width * 0.83f + cos(targetAngle) * 65.0f,
                                    Height * 0.82f + sin(targetAngle) * 65.0f);
                // Set angle + power di memory sebelum pindah state
                setAimAngle(targetAngle);
                sharedGameManager.mVisualCue().mPower(ShotPowerToPower(targetPower));
                stateStartTime = now;
                humanState = HUM_PULLING;
            }
            return;
        }

        // 5. POWER PULL — set angle+power lagi biar engine tidak drift, lalu lanjut delay
        if (humanState == HUM_PULLING) {
            setAimAngle(targetAngle);
            if (!powerSlider.Active) {
                float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
                float sliderX = Width * sliderXPercent;
                if (persistent_int[O("iPowerBarSide")] == 1) {
                    sliderX = Width * (1.0f - sliderXPercent);
                }
                float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
                float sliderYEnd = Height * persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sliderX - 20.0f, sliderYStart, 40.0f, sliderYEnd - sliderYStart);
                powerSlider.SimulateDrag(sliderRect, targetPower, 0.35f, 0.5f);
            }

            if (powerSlider.Active) return;

            gPrediction->forceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, targetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gPrediction->forceFullSimulation = false;

            stateStartTime = now;
            humanState = HUM_DELAY_BEFORE_SHOT;
            return;
        }

        // 6. FINAL HUMAN PAUSE (0.4s) then FIRE!
        if (humanState == HUM_DELAY_BEFORE_SHOT) {
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.1) {
                humanShotLocked = false;
                ClearState();
                state = IDLE; humanState = HUM_IDLE;
            }
            return;
        }
    }
    bool isPlayerTurn = sharedGameManager.mStateManager().isPlayerTurn();
    if (isPlayerTurn && bAutoSpin) applyAutoSpin();

    // PREDICTION LINES:
    // Di Prediction ini, lines digambar berdasarkan parameter `isAuto` di determineShotResult:
    //   isAuto=false → fastCalc=false → positions di-track → lines TAMPIL
    //   isAuto=true  → fastCalc=true  → positions tidak di-track → lines HILANG
    //
    // - Saat humanState aktif (lagi aiming/pulling): panggil isAuto=true → lines hilang
    // - Saat humanState HUM_IDLE (setelah shot selesai): panggil isAuto=false → lines tampil
    if (humanState != HUM_IDLE) {
        // Lines hilang selama human state machine jalan.
        gPrediction->determineShotResult(true, targetAngle, targetPower, lockedShotSpin);
    } else if (isPlayerTurn && g_CurrentCandidate.idx == -1) {
        spinIsLocked = false;
        if (gPrediction && sharedGameManager) {
            double curAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
            double curPower = sharedGameManager.mVisualCue().getShotPower();
            if (curPower < 10.0) curPower = 400.0;
            gPrediction->determineShotResult(false, curAngle, curPower, lockedShotSpin);
        }
    } else if (isPlayerTurn && g_CurrentCandidate.idx != -1) {
        // FIX PREDICTION LINES: Tampilkan simulasi dengan angle+power YANG SAMA
        // dengan yang akan ditembak (confirmedAngle/confirmedPower dari scan).
        // Sebelumnya display pakai getShotPower() dari visual cue yang bisa beda
        // dengan confirmedPower dari scan → lines keliatan masuk tapi shot nyatanya miss.
        gPrediction->determineShotResult(false,
            g_CurrentCandidate.angle,
            g_CurrentCandidate.power,
            lockedShotSpin,
            g_CurrentCandidate);
    }
    }
};
