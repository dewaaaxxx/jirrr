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

// ============================================================
// FORWARD DECLARATION: biar HumanAngleDrag bisa panggil AutoPlay::setAimAngle
// ============================================================
namespace AutoPlay {
    void setAimAngle(double angle);
}

// ============================================================================
// HUMAN ANGLE DRAG (Simulasi gerakan tangan manusia - SMOOTH VERSION)
// ============================================================================
// Flow:
//   1. Baca angle SEKARANG dari memory (getShotAngle)
//   2. Hitung delta ke target angle
//   3. Drag dari titik awal yang natural (area meja tengah-bawah)
//      ke titik akhir = titik awal + (delta * sensitivity) pixels
//   4. Setiap frame: update posisi drag dengan easing quintic + micro-jitter
//      Kirim NativeTouchesMove() → game rotate cue secara real
//   5. Setelah drag selesai, baca ulang angle dari memory
//      Kalau masih ada selisih > tolerance → koreksi kecil (max 4x)
//   6. Done → HumanShootUpdate lanjut ke H_THINK
// ============================================================================
struct HumanAngleDrag {
    enum State { HAD_IDLE, HAD_PRESS_DELAY, HAD_DRAGGING, HAD_LIFT_DELAY, HAD_FINISHED } state = HAD_IDLE;
    int touchIndex = 10;

    double targetAngle = 0.0;
    ImVec2 dragOrigin{};
    ImVec2 dragTo{};
    ImVec2 dragCurrent{};

    float elapsed   = 0.f;
    float duration  = 0.f;
    float pressDelay = 0.f;   // brief pause after touch-down before starting move
    float liftDelay  = 0.f;   // brief pause at end before lifting finger

    int correctionAttempts = 0;
    static constexpr int   MAX_CORRECTIONS   = 4;
    static constexpr double ANGLE_TOLERANCE  = 0.012; // ~0.7 degrees — good enough

    bool active = false;
    bool done   = false;

    // ── Helpers ──────────────────────────────────────────────────────────────
    static double AngleDiff(double a, double b) {
        double d = a - b;
        while (d >  M_PI) d -= 2.0 * M_PI;
        while (d < -M_PI) d += 2.0 * M_PI;
        return d;
    }

    // Tiny natural tremor — amplitude proportional to (ease * (1-ease))
    // so it's zero at start/end and peaks in the middle of the drag
    static float Jitter(float t, float seed, float amp) {
        float env = t * (1.0f - t) * 4.0f; // bell curve, peaks at t=0.5
        return amp * env * (
            sinf(t * 13.7f + seed)         * 0.50f +
            sinf(t * 27.3f + seed * 1.6f)  * 0.30f +
            sinf(t * 43.1f + seed * 2.4f)  * 0.20f
        );
    }

    // Ease-in-out quintic — smooth start, smooth stop
    static float EaseInOutQuint(float t) {
        return t < 0.5f
            ? 16.0f * t * t * t * t * t
            : 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 32.0f;
    }

    // ── Pick a drag origin: natural hand position for rotating cue ───────────
    // In 8BP, players drag roughly in the LOWER-CENTER of the visible table
    // (below the cue ball, where the thumb would rest). We stay away from
    // the power slider (right edge) and UI elements (top).
    static ImVec2 PickDragOrigin() {
        float cx = (TABLE_LEFT + TABLE_RIGHT)  * 0.50f;
        float cy = (TABLE_TOP  + TABLE_BOTTOM) * 0.62f; // slightly below center
        // Small random spread so consecutive shots don't look identical
        cx += (float)((rand() % 60) - 30);
        cy += (float)((rand() % 30) - 15);
        return ImVec2(cx, cy);
    }

    // ── Start one drag segment toward `target` from the current angle ────────
    void BeginDrag(double currentAngle, double target) {
        float sens = persistent_float["fAngleDragSensitivity"];
        if (sens <= 1.0f) sens = 280.0f; // px / radian — sane default

        double delta = AngleDiff(target, currentAngle);

        dragOrigin  = PickDragOrigin();
        dragCurrent = dragOrigin;

        // Horizontal drag maps to angle change; tiny vertical drift adds
        // realism (a real hand doesn't move in a perfectly straight line)
        float dx = (float)(delta * (double)sens);
        float dy = dx * 0.06f * ((rand() % 2) ? 1.f : -1.f); // ±6% vertical drift
        dragTo = ImVec2(dragOrigin.x + dx, dragOrigin.y + dy);

        // Duration: 0.30–0.65 s for small moves, up to ~1.0 s for large ones
        float absDelta = fabsf((float)delta);
        duration  = 0.30f + absDelta * 0.45f + (rand() % 120) * 0.001f;
        duration  = std::min(duration, 1.0f);

        // Tiny delay after touch-down (finger lands before it starts moving)
        pressDelay = 0.04f + (rand() % 40) * 0.001f;
        // Tiny delay before lift (finger slows to a stop)
        liftDelay  = 0.03f + (rand() % 30) * 0.001f;

        elapsed = 0.f;
        NativeTouchesBegin(touchIndex, dragOrigin.x, dragOrigin.y);
        state = HAD_PRESS_DELAY;
    }

    // ── Entry point ──────────────────────────────────────────────────────────
    void Begin(double angle) {
        if (active) return;
        targetAngle        = angle;
        correctionAttempts = 0;
        active             = true;
        done               = false;

        double currentAngle = sharedGameManager.mVisualCue().getShotAngle();
        BeginDrag(currentAngle, targetAngle);
    }

    // ── Per-frame update ─────────────────────────────────────────────────────
    void Update() {
        if (!active) return;

        float dt = ImGui::GetIO().DeltaTime;

        switch (state) {

        case HAD_PRESS_DELAY: {
            // Hold finger still for a moment after touch-down
            elapsed += dt;
            NativeTouchesMove(touchIndex, dragOrigin.x, dragOrigin.y);
            if (elapsed >= pressDelay) {
                elapsed = 0.f;
                state   = HAD_DRAGGING;
            }
            break;
        }

        case HAD_DRAGGING: {
            elapsed += dt;
            float t    = std::min(1.f, elapsed / duration);
            float ease = EaseInOutQuint(t);

            // Natural micro-jitter (zero at start/end, peak mid-drag)
            float seed = (float)touchIndex * 2.3f;
            float jx = Jitter(t, seed,        1.5f);
            float jy = Jitter(t, seed + 5.f,  0.6f);

            dragCurrent = ImVec2(
                dragOrigin.x + (dragTo.x - dragOrigin.x) * ease + jx,
                dragOrigin.y + (dragTo.y - dragOrigin.y) * ease + jy
            );
            NativeTouchesMove(touchIndex, dragCurrent.x, dragCurrent.y);

#ifdef DEBUG_TOUCH
            if (dynamic_bool["DebugTouch"]) {
                auto* fg = ImGui::GetForegroundDrawList();
                fg->AddCircleFilled(dragCurrent, 12.f, IM_COL32(80, 200, 255, 160));
                fg->AddLine(dragOrigin, dragTo, IM_COL32(80, 200, 255, 100), 1.5f);
                char buf[48];
                snprintf(buf, sizeof(buf), "drag %.0f%%", t * 100.f);
                fg->AddText(ImVec2(dragCurrent.x + 18, dragCurrent.y - 10),
                            IM_COL32(255, 255, 255, 200), buf);
            }
#endif

            if (t >= 1.f) {
                elapsed = 0.f;
                state   = HAD_LIFT_DELAY;
            }
            break;
        }

        case HAD_LIFT_DELAY: {
            // Hold at endpoint — finger decelerates to a stop
            elapsed += dt;
            NativeTouchesMove(touchIndex, dragCurrent.x, dragCurrent.y);
            if (elapsed >= liftDelay) {
                NativeTouchesEnd(touchIndex, dragCurrent.x, dragCurrent.y);
                OnDragEnd();
            }
            break;
        }

        default: break;
        }
    }

    // ── Called when one drag segment finishes ────────────────────────────────
    void OnDragEnd() {
        double actualAngle  = sharedGameManager.mVisualCue().getShotAngle();
        double remaining    = AngleDiff(targetAngle, actualAngle);

        if (std::abs(remaining) <= ANGLE_TOLERANCE || correctionAttempts >= MAX_CORRECTIONS) {
            // Close enough (or ran out of correction attempts)
            active = false;
            done   = true;
            state  = HAD_FINISHED;
            LOGI("HumanDrag: DONE target=%.4f actual=%.4f err=%.4f attempts=%d",
                 targetAngle, actualAngle, remaining, correctionAttempts);
        } else {
            // Small follow-up correction drag (same flow, shorter duration)
            correctionAttempts++;
            LOGI("HumanDrag: correction %d remaining=%.4f", correctionAttempts, remaining);
            BeginDrag(actualAngle, targetAngle);
        }
    }
};

static HumanAngleDrag humanAngleDrag;

Point2D lastFailedCuePos = { -1000.0, -1000.0 };

namespace AutoPlay {
    double lastSetAngle = 0.f;
    bool didSetAngle = false;
    bool bAutoPlaying = false;
    double luxuryPrecisionModifier = 1.0;  // Adjustable precision for luxury mode
    
    enum HumanExecState { H_IDLE, H_ANGLE, H_THINK, H_POWER } humanExecState = H_IDLE;
    float humanThinkTimer = 0.f;
    double humanPendingPower = 0.f;

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

    bool shouldAutoPlay() { 
        return !didSetAngle || lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(); 
    }

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    void takeShot(double angle, double power) {
        setAimAngle(angle);
        gPrediction->determineShotResult(false, angle, power);
        sharedGameManager.mVisualCue().mPower(ShotPowerToPower(power));
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
        
        // Log metrics for luxury tracking
        g_AutoPlayMetrics.totalShotsAttempted++;
        g_AutoPlayMetrics.averagePower = (g_AutoPlayMetrics.averagePower * (g_AutoPlayMetrics.totalShotsAttempted - 1) + power) / g_AutoPlayMetrics.totalShotsAttempted;
    }
    
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
    }
    
    void setShotPower(double power) {
    sharedGameManager.mVisualCue().setShotPower(power);
}

bool HumanShootBusy() { return humanExecState != H_IDLE; }

void HumanShootBegin(double angle, double power) {
    humanPendingPower = power;
    humanAngleDrag.Begin(angle);
    humanExecState = H_ANGLE;
}

void HumanShootUpdate() {
    switch (humanExecState) {
        case H_ANGLE: {
            humanAngleDrag.Update();
            if (humanAngleDrag.done) {
                // No setAimAngle() here — HumanAngleDrag already verified
                // the angle via getShotAngle() readback and corrected if
                // needed. Overwriting via memory right after risks
                // conflicting with the engine state from the touch event.
                humanThinkTimer = 0.40f + (rand() % 300) * 0.001f;
                humanExecState = H_THINK;
            }
            break;
        }
        case H_THINK: {
            humanThinkTimer -= ImGui::GetIO().DeltaTime;
            if (humanThinkTimer <= 0.f) {
                // Gunakan power slider simulation
                float sliderX   = persistent_float["fPSliderX"];
                float sliderTop = persistent_float["fPSliderTop"];
                float sliderH   = persistent_float["fPSliderH"];
                if (sliderX <= 0.f)   sliderX   = 0.858f;
                if (sliderTop <= 0.f) sliderTop = 0.18f;
                if (sliderH <= 0.f)   sliderH   = 0.67f;

                ImGuiIO& io = ImGui::GetIO();
                ImVec4 rect(
                    io.DisplaySize.x * sliderX,
                    io.DisplaySize.y * sliderTop,
                    io.DisplaySize.x * 0.04f,
                    io.DisplaySize.y * sliderH
                );

                float dragTime = 0.80f + (rand() % 300) * 0.001f;
                float holdTime = 0.30f + (rand() % 150) * 0.001f;
                powerSlider.SimulateDrag(rect, (float)humanPendingPower, dragTime, holdTime);
                humanExecState = H_POWER;
            }
            break;
        }
        case H_POWER: {
            if (!powerSlider.Active) {
                double finalPower = humanPendingPower;
                if (finalPower < 100.0) finalPower = 100.0;
                if (finalPower > 666.0) finalPower = 666.0;
                
                setAimAngle(humanAngleDrag.targetAngle);
                setShotPower(finalPower);
                gPrediction->determineShotResult(false, humanAngleDrag.targetAngle, finalPower);
                sharedGameManager.mVisualCue().mPower(ShotPowerToPower(finalPower));
                M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
                
                humanExecState = H_IDLE;
                ClearState();
                state = IDLE;
            }
            break;
        }
        default: break;
    }
}
    
    void Shoot(double angle, double power = 0.f) {
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
    } else if (persistent_bool["bHumanAutoplay"]) {  // <-- CEK SETTING HUMAN MODE
        HumanShootBegin(angle, power);
        state = EXECUTING;
    } else {
        takeShot(angle, power);
        ClearState();
        state = IDLE;
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
        powerSlider.Update();
        HumanShootUpdate();
        
        if (!persistent_bool[O("bAutoPlay")] || !bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            if (!HumanShootBusy()) state = IDLE;
            return;
        }
        
        buttonClicker.Update();

        if (isAnimationActive()) return;

        if (HumanShootBusy()) return;  // <-- TAMBAHKAN INI


        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
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
            if (persistent_bool["bHumanAutoplay"]) {
                HumanShootBegin(g_CurrentCandidate.angle, g_CurrentCandidate.power);
                state = EXECUTING;
            } else {
                takeShot(pendingShotAngle, pendingShotPower);
                ClearState();
                state = IDLE;
            }
        }
    } else if (state == EXECUTING) {
        // HumanShootUpdate() handles the rest
    }
    }
};
