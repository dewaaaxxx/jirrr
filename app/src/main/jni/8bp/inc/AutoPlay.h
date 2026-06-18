#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "ScreenTable.h"
#include "PhysicsModel.h"
#include "GameSpeedControl.h"
#include "ButtonClicker.h"
#include "PowerSlider.h"

using namespace ImGui;

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================
#ifndef PI
#define PI 3.14159265358979323846
#endif

const double TWO_PI = 2.0 * PI;
const double ANGLE_STEP_FAST = 0.05;
const double ANGLE_STEP_SLOW = 0.02;
const double MIN_POCKET_DIST = 40.0;
const double MAX_POCKET_DIST = 120.0;
const double BALL_SAFETY_MARGIN = 5.0;

enum BallType {
    CUE_BALL = 0,
    SOLIDS = 1,
    STRIPES = 2,
    EIGHT_BALL = 3,
    INVALID = -1
};

constexpr double maxAngle = 360.0 / (180.0 / M_PI);

double normalizeAngle(double angle) {
    double newAngle = angle;
    if (newAngle >= maxAngle) newAngle = fmod(newAngle, maxAngle);
    else if (newAngle < 0) newAngle = maxAngle - fmod(-newAngle, maxAngle);
    return newAngle;
}

Candidate g_CurrentCandidate = { -1 };
extern void DrawEightBallLoading(ImDrawList*);

ImVec2 GetPocketScreenPos(int pocketIdx) {
    Table table = sharedGameManager.mTable;
    if (!table) return {};
    auto tableProperties = table.mTableProperties();
    if (!tableProperties) return {};
    auto& pockets = tableProperties.mPockets();
    return WorldToScreen(pockets[pocketIdx]);
}

// ============================================================================
// PHYSICS ENGINE
// ============================================================================
struct PhysicsEngine {
    static constexpr double BALL_DIAMETER = 2.0 * Physics::BALL_RADIUS;
    static constexpr double GRAVITY = 9.81;

    static double calculatePowerForTargetToPocket(
        double cueToBallDist,
        double ballToPocketDist,
        const FrictionProperties& friction
    ) {
        if (ballToPocketDist < 1.0) ballToPocketDist = 1.0;
        if (cueToBallDist < 1.0) cueToBallDist = 1.0;

        double slidingDeceleration = friction._velocityReductionSlidingFactor;
        if (slidingDeceleration < 1.0) slidingDeceleration = 196.0;

        double velocityForTarget = std::sqrt(2.0 * slidingDeceleration * ballToPocketDist);
        double velocityForCue    = std::sqrt(2.0 * slidingDeceleration * cueToBallDist);

        double power = (velocityForCue + velocityForTarget) * 1.30; // +5% power margin

        return std::min(std::max(power, 100.0), 666.0);
    }

    static Point2D calculateGhostBallPosition(
        const Point2D& targetBallPos,
        const Point2D& aimPoint
    ) {
        Point2D toAim = aimPoint - targetBallPos;
        double distance = std::sqrt(toAim.square());
        if (distance < 0.1) return targetBallPos;

        Point2D direction = toAim * (1.0 / distance);
        return targetBallPos - direction * BALL_DIAMETER;
    }

    static double calculateAngleToAimPoint(
        const Point2D& cueBallPos,
        const Point2D& targetBallPos,
        const Point2D& aimPoint
    ) {
        Point2D ghostBallPos = calculateGhostBallPosition(targetBallPos, aimPoint);
        Point2D cueToGhost = ghostBallPos - cueBallPos;
        double angle = std::atan2(cueToGhost.y, cueToGhost.x);
        if (angle < 0) angle += TWO_PI;
        return angle;
    }

    static double calculateShotAccuracy(
        const Point2D& cueBallPos,
        const Point2D& targetBallPos,
        const Point2D& aimPoint
    ) {
        Point2D cueToTarget = targetBallPos - cueBallPos;
        Point2D targetToAim = aimPoint - targetBallPos;

        double cueToTargetLen = std::sqrt(cueToTarget.square());
        double targetToAimLen = std::sqrt(targetToAim.square());

        if (cueToTargetLen < 0.1 || targetToAimLen < 0.1) return 0.0;

        double dotProduct = (cueToTarget.x * targetToAim.x) + (cueToTarget.y * targetToAim.y);
        double accuracy = dotProduct / (cueToTargetLen * targetToAimLen);

        return std::max(0.0, std::min(1.0, accuracy));
    }

    static double calculateShotScore(
        double targetBallTravelDist,
        double accuracy,
        BallType ballType,
        BallType myBallType,
        bool isMyBall,
        double bankPenalty = 1.0
    ) {
        double baseScore = targetBallTravelDist;
        baseScore *= (1.0 - (accuracy * 0.5));

        if (isMyBall) {
            baseScore *= 0.2;
        } else if (ballType != EIGHT_BALL) {
            baseScore *= 3.0;
        }

        baseScore *= bankPenalty;
        return baseScore;
    }

    static bool isPocketReachable(double dist) {
        return dist >= MIN_POCKET_DIST && dist <= MAX_POCKET_DIST;
    }

    // ========================================================================
    // VALIDASI KETAT (FIX UNTUK SCRATCH & NEMBAK SALAH)
    // ========================================================================
    static bool validateCueBallSafety(const Prediction& pred) {
        auto& cueBall = pred.guiData.balls[0];
        if (!cueBall.onTable) return false;
        // FIX: previously required velocity to be EXACTLY {0,0}
        // (`!cueBall.velocity`). Floating-point friction simulation almost
        // never lands on a perfectly exact zero — it leaves a tiny residual
        // (e.g. 0.0003) even when the ball is visually/physically at rest.
        // That exact-match check was rejecting nearly every otherwise-valid
        // candidate, forcing AutoPlay to fall back to worse shots (or none
        // at all). Use a small epsilon-based "is essentially stopped" check
        // instead.
        double speedSq = cueBall.velocity.x * cueBall.velocity.x + cueBall.velocity.y * cueBall.velocity.y;
        return speedSq < 0.01;
    }

    static bool validateEightBallSafety(const Prediction& pred, BallType myBallType) {
        auto& ball8 = pred.guiData.balls[8];
        if (ball8.originalOnTable && !ball8.onTable && myBallType != EIGHT_BALL) {
            return false;
        }
        return true;
    }

    static bool validateFirstHit(const Prediction& pred, BallType myBallType, BallType targetBallType) {
        auto firstHit = pred.guiData.collision.firstHitBall;
        if (!firstHit) return false;

        BallType hitType = INVALID;
        if (firstHit->index == 8) {
            hitType = EIGHT_BALL;
        } else if (firstHit->classification == Ball::Classification::EIGHT_BALL) {
            hitType = EIGHT_BALL;
        } else if (firstHit->index >= 1 && firstHit->index <= 7) {
            hitType = SOLIDS;
        } else if (firstHit->index >= 9 && firstHit->index <= 15) {
            hitType = STRIPES;
        }

        if (hitType == EIGHT_BALL) {
            return myBallType == EIGHT_BALL;
        }
        return hitType == targetBallType;
    }

    static bool validateTargetBallPocketed(const Prediction& pred, int targetIdx) {
        auto& targetBall = pred.guiData.balls[targetIdx];
        return targetBall.originalOnTable && !targetBall.onTable;
    }

    // ========================================================================
    // BANK SHOT GEOMETRY
    // ========================================================================
    struct BankWall { double axis; bool vertical; };

    static void GetBankWalls(BankWall outWalls[4]) {
        Table table = sharedGameManager.mTable;
        double halfLen = 127.0, halfWid = 63.5;
        if (table) {
            auto props = table.mTableProperties();
            if (props) {
                halfLen = props.getLength() / 2.0;
                halfWid = props.getWidth() / 2.0;
            }
        }
        outWalls[0] = { -halfLen, true  };
        outWalls[1] = {  halfLen, true  };
        outWalls[2] = { -halfWid, false };
        outWalls[3] = {  halfWid, false };
    }

    static Point2D MirrorAcrossWall(const Point2D& point, const BankWall& wall) {
        return wall.vertical
            ? Point2D(2.0 * wall.axis - point.x, point.y)
            : Point2D(point.x, 2.0 * wall.axis - point.y);
    }
};

// ============================================================================
// GAME STATE & HELPER FUNCTIONS
// ============================================================================
Point2D lastFailedCuePos = { -1000.0, -1000.0 };
Point2D fullyExhaustedCuePos = { -1000.0, -1000.0 };

BallType getBallType(int ballIndex) {
    if (ballIndex == 0) return CUE_BALL;
    if (ballIndex == 8) return EIGHT_BALL;
    if (ballIndex >= 1 && ballIndex <= 7) return SOLIDS;
    if (ballIndex >= 9 && ballIndex <= 15) return STRIPES;
    return INVALID;
}

BallType getPlayerBallType(Ball::Classification classification) {
    if (classification == Ball::Classification::STRIPE) return STRIPES;
    if (classification == Ball::Classification::EIGHT_BALL) return EIGHT_BALL;
    return SOLIDS;
}

// ============================================================================
// HUMAN ANGLE DRAG
// ============================================================================
// ============================================================================
// HUMAN ANGLE DRAG - SMOOTH DARI AWAL KE TARGET (seperti video 2)
// ============================================================================
struct HumanAngleDrag {
    enum State { HAD_IDLE, HAD_DRAGGING, HAD_FINISHED } state = HAD_IDLE;
    int touchIndex = 9;

    double targetAngle = 0.0;
    double startAngle = 0.0;  // SIMPAN SUDUT AWAL
    ImVec2 dragOrigin{};
    ImVec2 dragTo{};
    ImVec2 dragCurrent{};

    float elapsed = 0.f;
    float duration = 0.f;
    int correctionAttempts = 0;
    static constexpr int MAX_CORRECTIONS = 4;
    static constexpr double ANGLE_TOLERANCE = 0.01;

    bool active = false;
    bool done = false;

    static double AngleDiff(double a, double b) {
        double d = a - b;
        while (d > M_PI) d -= 2.0 * M_PI;
        while (d < -M_PI) d += 2.0 * M_PI;
        return d;
    }

    // === EASING YANG SMOOTH ===
    static float SmoothEase(float t) {
        return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    }

    static float NaturalJitter(float t, float seed, float amp) {
        return amp * (
            sinf(t * 12.7f + seed)         * 0.6f +
            sinf(t * 23.3f + seed * 1.7f)  * 0.25f +
            sinf(t * 41.9f + seed * 2.3f)  * 0.1f
        );
    }

    // ===== BEGIN: DRAG DARI POSISI AWAL KE TARGET =====
    void Begin(double angle) {
        if (active) return;

        targetAngle = angle;
        startAngle = sharedGameManager.mVisualCue().getShotAngle(); // SIMPAN SUDUT AWAL
        correctionAttempts = 0;
        active = true;
        done = false;

        float sens = persistent_float["fAngleDragSensitivity"];
        if (sens <= 1.0f) sens = 220.0f;

        // === AMBIL POSISI CUE BALL DI LAYAR ===
        auto& cueBall = gPrediction->guiData.balls[0];
        ImVec2 cueScreen = WorldToScreen(cueBall.initialPosition);

        // START: dekat dengan cue ball
        float originX = cueScreen.x + (float)((rand() % 40) - 20);
        float originY = cueScreen.y + (float)((rand() % 30) - 15) - 25.0f;
        dragOrigin = ImVec2(originX, originY);
        dragCurrent = dragOrigin;

        // === HITUNG DELTA SUDUT DARI AWAL KE TARGET ===
        double delta = AngleDiff(targetAngle, startAngle);
        
        // END POSITION: berdasarkan delta sudut
        float dx = (float)(delta * sens);
        float dy = (float)(delta * sens * 0.12f); // sedikit miring
        dragTo = ImVec2(dragOrigin.x + dx, dragOrigin.y + dy);

        elapsed = 0.f;
        duration = 0.7f + (rand() % 300) * 0.001f; // 0.7 - 1.0 detik

        NativeTouchesBegin(touchIndex, dragOrigin.x, dragOrigin.y);
        state = HAD_DRAGGING;
    }

    void Update() {
        if (!active || state != HAD_DRAGGING) return;

        float dt = ImGui::GetIO().DeltaTime;
        elapsed += dt;
        float t = std::min(1.f, elapsed / duration);

        // === SMOOTH EASING ===
        float eased = SmoothEase(t);

        // === JITTER NATURAL ===
        float jitterAmp = 1.2f * (eased * (1.0f - eased) * 4.0f);
        float jitter = NaturalJitter(t, (float)touchIndex + 7.3f, jitterAmp * 0.6f);

        // === POSISI DRAG (bergerak dari origin ke target) ===
        dragCurrent = ImVec2(
            dragOrigin.x + (dragTo.x - dragOrigin.x) * eased + jitter,
            dragOrigin.y + (dragTo.y - dragOrigin.y) * eased + jitter * 0.3f + eased * 5.0f
        );

        NativeTouchesMove(touchIndex, dragCurrent.x, dragCurrent.y);

        // === DEBUG ===
        if (dynamic_bool["DebugTouch"]) {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            fg->AddCircleFilled(dragCurrent, 14.f, IM_COL32(80, 200, 255, 180));
            fg->AddLine(dragOrigin, dragTo, IM_COL32(80, 200, 255, 70), 2.f);
            // Tampilkan progress
            char buf[32];
            sprintf(buf, "%.0f%%", t * 100);
            fg->AddText(ImVec2(dragCurrent.x + 20, dragCurrent.y - 10), IM_COL32(255, 255, 255, 200), buf);
        }

        if (t >= 1.f) {
            NativeTouchesEnd(touchIndex, dragCurrent.x, dragCurrent.y);

            double actualAngle = sharedGameManager.mVisualCue().getShotAngle();
            double remaining = AngleDiff(targetAngle, actualAngle);

            if (std::abs(remaining) < ANGLE_TOLERANCE || correctionAttempts >= MAX_CORRECTIONS) {
                active = false;
                done = true;
                state = HAD_FINISHED;
            } else {
                // === KALAU MASIH OFF, KOREKSI ===
                correctionAttempts++;
                // TIDAK LANGSUNG BEGIN ULANG, TAPI KOREKSI KECIL
                float korx = (float)(remaining * 220.0f * 0.6f);
                float kory = (float)(remaining * 220.0f * 0.6f * 0.12f);
                dragTo = ImVec2(dragCurrent.x + korx, dragCurrent.y + kory);
                dragOrigin = dragCurrent;
                elapsed = 0.f;
                duration = 0.3f + (rand() % 150) * 0.001f; // koreksi lebih cepat
                NativeTouchesMove(touchIndex, dragCurrent.x, dragCurrent.y);
                // state tetap HAD_DRAGGING
            }
        }
    }
};

static HumanAngleDrag humanAngleDrag;

bool IsShotValid() {
    if (!sharedGameManager || !gPrediction) return false;
    return sharedGameManager.mStateManager().isPlayerTurn() &&
           gPrediction->guiData.balls[0].onTable;
}

// ============================================================================
// AUTOPLAY NAMESPACE
// ============================================================================
namespace AutoPlay {
    double lastSetAngle = 0.f;
    double lastSetPower = 0.f;
    bool bAutoPlaying = false;

    static FrictionProperties cachedFriction = {0.2, 0.0111, 0.025, 0.0014577259475218659, 196, 10.878, 9.8};

    enum State { IDLE, SCANNING, NOMINATING, EXECUTING } state = IDLE;
    enum ScanMode { FAST, SLOW } scan = FAST;

    double pendingShotPower = 0.f;
    double pendingShotAngle = 0.f;
    int nominationFrameCounter = 0;

    // ========================================================================
    // HUMAN-STYLE SHOT EXECUTION
    // ========================================================================
    enum HumanExecState { H_IDLE, H_ANGLE, H_THINK, H_POWER } humanExecState = H_IDLE;
    float humanThinkTimer = 0.f;
    double humanPendingPower = 0.f;

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }
    
    void setShotPower(double power) {
        lastSetPower = power;
        sharedGameManager.mVisualCue().setShotPower(power);
    }
    
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
        fullyExhaustedCuePos = { -1000.0, -1000.0 };
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
                    // FIX: removed the redundant setAimAngle() memory write
                    // here. HumanAngleDrag already verifies (via reading
                    // back getShotAngle() after each drag segment) that the
                    // REAL in-game angle converged to within tolerance of
                    // the target, using genuine touch-drag input — that's
                    // the whole point of "human mode". Overwriting it again
                    // via direct memory right after risked conflicting with
                    // the engine's own per-frame state from the touch event
                    // it had just processed.
                    humanThinkTimer = 0.50f + (rand() % 400) * 0.001f;
                    humanExecState = H_THINK;
                }
                break;
            }
            case H_THINK: {
                humanThinkTimer -= ImGui::GetIO().DeltaTime;
                if (humanThinkTimer <= 0.f) {
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

    void takeShot(double angle, double power) {
        setAimAngle(angle);
        setShotPower(power);
        gPrediction->determineShotResult(false, angle, power);
        sharedGameManager.mVisualCue().mPower(ShotPowerToPower(power));
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }

    void Shoot(double angle, double power = 0.f) {
        setAimAngle(angle);
        gPrediction->determineShotResult(false, angle, power);

        bool nominating = false;
        int nominationMode = sharedGameManager.getPocketNominationMode();
        auto myclass = sharedGameManager.getPlayerClassification();

        if ((nominationMode == 1 && myclass == Ball::Classification::EIGHT_BALL) ||
            (nominationMode == 2 && myclass != Ball::Classification::ANY)) {
            if (g_CurrentCandidate.idx != -1 && sharedGameManager.getNominatedPocket() != g_CurrentCandidate.pocketIndex) {
                nominating = true;
            }
        }

        if (nominating) {
            pendingShotPower = power;
            pendingShotAngle = angle;
            state = NOMINATING;
            nominationFrameCounter = 0;
        } else if (persistent_bool["bHumanAutoplay"]) {
            HumanShootBegin(angle, power);
            state = EXECUTING;
        } else {
            takeShot(angle, power);
            ClearState();
            state = IDLE;
        }
    }

    // ========================================================================
    // SCAN FAST
    // ========================================================================
    void ScanFast(double angleStep = ANGLE_STEP_FAST) {
        if (g_CurrentCandidate.idx != -1) return;
        if (gPrediction->guiData.balls[0].initialPosition == lastFailedCuePos) {
            scan = SLOW;
            return;
        }

        Ball::Classification playerClass = sharedGameManager.getPlayerClassification();
        BallType myBallType = getPlayerBallType(playerClass);
        bool isOpenTable = (playerClass == Ball::Classification::ANY);
        uint nominatedPocket = sharedGameManager.getNominatedPocket();

        std::vector<Candidate> candidates;
        auto pockets = getPockets();
        auto& cueBall = gPrediction->guiData.balls[0];

        PhysicsEngine::BankWall bankWalls[4];
        PhysicsEngine::GetBankWalls(bankWalls);

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;

            BallType ballType = getBallType(i);
            bool isMyBall = (ballType == myBallType);

            bool isCandidate = false;
            if (isMyBall) {
                isCandidate = true;
            } else if (isOpenTable && ballType != EIGHT_BALL && ballType != CUE_BALL) {
                isCandidate = true;
            }

            if (!isCandidate) continue;

            for (int pocketIdx = 0; pocketIdx < (int)pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;

                Point2D pocket = pockets[pocketIdx];

                Point2D ballToPocket = pocket - ball.initialPosition;
                double ballToPocketDist = std::sqrt(ballToPocket.square());

                if (PhysicsEngine::isPocketReachable(ballToPocketDist)) {
                    Point2D collisionPoint = PhysicsEngine::calculateGhostBallPosition(ball.initialPosition, pocket);
                    Point2D cueToCollision = collisionPoint - cueBall.initialPosition;
                    double cueToCollisionDist = std::sqrt(cueToCollision.square());

                    if (cueToCollisionDist >= 0.1) {
                        double angle = PhysicsEngine::calculateAngleToAimPoint(cueBall.initialPosition, ball.initialPosition, pocket);
                        double accuracy = PhysicsEngine::calculateShotAccuracy(cueBall.initialPosition, ball.initialPosition, pocket);
                        double score = PhysicsEngine::calculateShotScore(ballToPocketDist, accuracy, ballType, myBallType, isMyBall, 1.0);
                        double power = PhysicsEngine::calculatePowerForTargetToPocket(cueToCollisionDist, ballToPocketDist, cachedFriction);

                        candidates.push_back({i, angle, score, pocketIdx, power});
                    }
                }

                // Bank shots
                for (int w = 0; w < 4; w++) {
                    Point2D mirrorPocket = PhysicsEngine::MirrorAcrossWall(pocket, bankWalls[w]);

                    Point2D ballToMirror = mirrorPocket - ball.initialPosition;
                    double ballToMirrorDist = std::sqrt(ballToMirror.square());
                    if (!PhysicsEngine::isPocketReachable(ballToMirrorDist)) continue;

                    Point2D ghostPos = PhysicsEngine::calculateGhostBallPosition(ball.initialPosition, mirrorPocket);
                    Point2D cueToGhost = ghostPos - cueBall.initialPosition;
                    double cueToGhostDist = std::sqrt(cueToGhost.square());
                    if (cueToGhostDist < 0.1) continue;

                    double angle = PhysicsEngine::calculateAngleToAimPoint(cueBall.initialPosition, ball.initialPosition, mirrorPocket);
                    double accuracy = PhysicsEngine::calculateShotAccuracy(cueBall.initialPosition, ball.initialPosition, mirrorPocket);
                    double score = PhysicsEngine::calculateShotScore(ballToMirrorDist, accuracy, ballType, myBallType, isMyBall, 2.5);
                    double power = PhysicsEngine::calculatePowerForTargetToPocket(cueToGhostDist, ballToMirrorDist, cachedFriction);

                    candidates.push_back({i, angle, score, pocketIdx, power});
                }
            }
        }

        std::sort(candidates.begin(), candidates.end());

        bool foundShot = false;

        for (const auto& cand : candidates) {
            double baseAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(cand.angle));

            bool validated = false;
            double bestAngle = baseAngle;
            double bestPower = cand.power;

            double powerVariants[5] = {
                cand.power, cand.power * 1.08, cand.power * 0.92,
                cand.power * 1.16, cand.power * 0.85
            };
            double angleOffsets[5] = { 0.0, 0.015, -0.015, 0.03, -0.03 };

            for (int ai = 0; ai < 5 && !validated; ai++) {
                double tryAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(baseAngle + angleOffsets[ai]));
                for (int pi = 0; pi < 5; pi++) {
                    double tryPower = powerVariants[pi];
                    if (tryPower < 100.0) tryPower = 100.0;
                    if (tryPower > 666.0) tryPower = 666.0;

                    gPrediction->determineShotResult(true, tryAngle, tryPower, sharedGameManager.getShotSpin(), cand);

                    // ========== VALIDASI KETAT ==========
                    if (!PhysicsEngine::validateCueBallSafety(*gPrediction)) continue;
                    if (!PhysicsEngine::validateEightBallSafety(*gPrediction, myBallType)) continue;
                    if (!PhysicsEngine::validateFirstHit(*gPrediction, myBallType, getBallType(cand.idx))) continue;
                    if (!PhysicsEngine::validateTargetBallPocketed(*gPrediction, cand.idx)) continue;
                    if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) continue;

                    bestAngle = tryAngle;
                    bestPower = tryPower;
                    validated = true;
                    break;
                }
            }

            if (!validated) continue;

            LOGI("AutoPlay: FAST - Ball %d angle %f power %f", cand.idx, bestAngle, bestPower);
            g_CurrentCandidate = cand;
            g_CurrentCandidate.angle = bestAngle;
            g_CurrentCandidate.power = bestPower;
            foundShot = true;
            Shoot(bestAngle, bestPower);
            break;
        }

        if (!foundShot) {
            lastFailedCuePos = cueBall.initialPosition;
            LOGI("AutoPlay: ScanFast failed, switching to ScanSlow");
            scan = SLOW;
        }
    }

    // ========================================================================
    // SCAN SLOW
    // ========================================================================
    void ScanSlow(double angleStep = ANGLE_STEP_SLOW) {
        static double currentScanAngle = 0.0;
        static bool isScanning = false;
        static Point2D lastScanCuePos = { -1000.0, -1000.0 };

        if (g_CurrentCandidate.idx != -1) return;

        if (!isScanning || gPrediction->guiData.balls[0].initialPosition != lastScanCuePos) {
            currentScanAngle = 0.0;
            isScanning = true;
            lastScanCuePos = gPrediction->guiData.balls[0].initialPosition;
        }

        Ball::Classification playerClass = sharedGameManager.getPlayerClassification();
        BallType myBallType = getPlayerBallType(playerClass);
        bool isOpenTable = (playerClass == Ball::Classification::ANY);
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        auto& cueBall = gPrediction->guiData.balls[0];

        int steps = 0;
        int stepsPerFrame = (int)(20 * GameSpeed::getAnimationMultiplier());

        while (steps < stepsPerFrame && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            std::vector<double> powers = {666.0, 350.0};
            for (double power : powers) {
                gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());

                // ========== VALIDASI KETAT ==========
                if (!PhysicsEngine::validateCueBallSafety(*gPrediction)) continue;
                if (!PhysicsEngine::validateEightBallSafety(*gPrediction, myBallType)) continue;
                if (!PhysicsEngine::validateFirstHit(*gPrediction, myBallType, myBallType)) continue;

                int targetIdx = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (!ball.originalOnTable || ball.onTable) continue;

                    BallType ballType = getBallType(i);
                    bool isMyBall = (ballType == myBallType);

                    bool isValid = isMyBall || (isOpenTable && ballType != EIGHT_BALL && ballType != CUE_BALL);
                    if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) isValid = false;

                    if (isValid) { targetIdx = i; break; }
                }

                if (targetIdx == -1) continue;

                LOGI("AutoPlay: SLOW - Ball %d angle %f power %f", targetIdx, angle, power);
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
            LOGI("AutoPlaySlow: Exhaustive scan complete, no shot found");
            isScanning = false;
            currentScanAngle = 0.0;
            fullyExhaustedCuePos = cueBall.initialPosition;
            state = IDLE;
        }
    }

    // ========================================================================
    // UI: Draw toggle button
    // ========================================================================
    void DrawToggleButton() {
        ImGuiIO& io = GetIO();
        float button_size = ImGui::GetFrameHeight() * 2.3f;
        float windowWidth = button_size + GetStyle().WindowPadding.x * 2;
        float windowHeight = button_size + GetStyle().WindowPadding.y * 2;

        SetNextWindowPos(ImVec2(io.DisplaySize.x - 155 - windowWidth, io.DisplaySize.y - 20 - windowHeight), ImGuiCond_Always);
        SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);

        PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);

        if (Begin("AutoPlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
            auto DrawPlayPauseButton = [&](bool isPause) -> bool {
                ImVec2 pos = GetCursorScreenPos();
                ImVec2 size(button_size, button_size);
                ImVec2 end(pos.x + size.x, pos.y + size.y);
                ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

                PushStyleColor(ImGuiCol_Button, IM_COL32(50, 50, 50, 180));
                PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 80, 200));
                PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 100, 100, 200));
                PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

                bool clicked = Button("##AutoPlayBtn", size);

                ImDrawList* dl = GetWindowDrawList();
                float h = size.y * 0.4f;
                float w = h * 0.8f;

                if (isPause) {
                    float bar_w = w * 0.35f;
                    float gap = w * 0.3f;
                    dl->AddRectFilled(ImVec2(center.x - gap/2 - bar_w, center.y - h/2), ImVec2(center.x - gap/2, center.y + h/2), IM_COL32(255, 255, 255, 180));
                    dl->AddRectFilled(ImVec2(center.x + gap/2, center.y - h/2), ImVec2(center.x + gap/2 + bar_w, center.y + h/2), IM_COL32(255, 255, 255, 180));
                } else {
                    float off_x = h * 0.3f;
                    dl->AddTriangleFilled(ImVec2(center.x - off_x, center.y - h/2), ImVec2(center.x - off_x, center.y + h/2), ImVec2(center.x + off_x * 1.5f, center.y), IM_COL32(255, 255, 255, 180));
                }

                GetForegroundDrawList()->AddRect(pos, end, IM_COL32(200, 200, 200, 255), 5.0f, 0, 2.0f);
                PopStyleColor(4);
                return clicked;
            };

            if (DrawPlayPauseButton(bAutoPlaying)) {
                bAutoPlaying = !bAutoPlaying;
                if (bAutoPlaying) ClearState();
            }
        }
        End();

        PopStyleVar();
        PopStyleColor(2);

        if (persistent_bool["bHumanAutoplay"]) {
            ImDrawList* fg = GetForegroundDrawList();
            ImVec2 badgePos(io.DisplaySize.x - 155 - windowWidth, io.DisplaySize.y - 20 - windowHeight - 22);
            fg->AddRectFilled(badgePos, ImVec2(badgePos.x + windowWidth, badgePos.y + 18), IM_COL32(40, 120, 200, 200), 4.0f);
            ImVec2 textSz = CalcTextSize("HUMAN");
            fg->AddText(ImVec2(badgePos.x + (windowWidth - textSz.x) * 0.5f, badgePos.y + 1), IM_COL32(255, 255, 255, 255), "HUMAN");
        }
    }

    bool isAnimationActive() {
        auto visualCue = sharedGameManager.mVisualCue();
        if (!visualCue) return true;

        auto _powerBarView = F(ptr, visualCue + 0x510);
        if (!_powerBarView) return true;

        auto activeAction = M(ptr, libmain + 0x2de6f30, ptr)(_powerBarView);
        return activeAction != 0;
    }

    // ========================================================================
    // MAIN: Update loop
    // ========================================================================
    void Update() {
        buttonClicker.Update();
        powerSlider.Update();
        HumanShootUpdate();
        DrawToggleButton();

        if (isAnimationActive()) return;
        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            if (!HumanShootBusy()) state = IDLE;
            return;
        }

        if (HumanShootBusy()) return;

        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
        }
        if (state == SCANNING) {
            if (gPrediction->guiData.balls[0].initialPosition == fullyExhaustedCuePos) {
                return;
            }

            if (scan == FAST) {
                ScanFast(ANGLE_STEP_FAST);
            } else {
                DrawEightBallLoading(GetForegroundDrawList());
                ScanSlow(ANGLE_STEP_SLOW);
            }
        }
        if (state == NOMINATING) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 10) {
                buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
            }
            if (nominationFrameCounter > 20 && !buttonClicker.Active) {
                if (persistent_bool["bHumanAutoplay"]) {
                    HumanShootBegin(g_CurrentCandidate.angle, g_CurrentCandidate.power);
                    state = EXECUTING;
                } else {
                    takeShot(g_CurrentCandidate.angle, g_CurrentCandidate.power);
                    ClearState();
                    state = IDLE;
                }
            }
        }
        if (state == EXECUTING) {
            // HumanShootUpdate() handles the rest
        }
    }
};
