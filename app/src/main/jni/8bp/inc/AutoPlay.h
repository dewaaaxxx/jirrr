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
const double ANGLE_STEP_FAST = 0.05;      // 0.05 radians (~2.86 degrees)
const double ANGLE_STEP_SLOW = 0.02;      // 0.02 radians (~1.15 degrees)
const double MIN_POCKET_DIST = 40.0;      // Minimum distance to pocket
const double MAX_POCKET_DIST = 120.0;     // Maximum distance to pocket
const double BALL_SAFETY_MARGIN = 5.0;    // Safety margin around ball

// Ball type classifications
enum BallType {
    CUE_BALL = 0,
    SOLIDS = 1,      // 1-7
    STRIPES = 2,     // 9-15
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
// PHYSICS ENGINE - GHOST BALL GEOMETRY + BANK SHOT MIRROR GEOMETRY
// ============================================================================
struct PhysicsEngine {
    static constexpr double BALL_DIAMETER = 2.0 * Physics::BALL_RADIUS;
    static constexpr double GRAVITY = 9.81;

    // ========================================================================
    // Power needed for the TARGET ball to travel from its position to the
    // pocket, PLUS the power the cue ball loses traveling to the collision
    // point. FIX: the previous version of this function accepted
    // `cueToBallDist` as a parameter but never used it anywhere in the body
    // — meaning a shot where the cue ball has to travel far before even
    // reaching the target ball got the exact same power estimate as a shot
    // where the cue ball is right next to the target. The cue ball would
    // arrive at the target already too slow, transfer too little energy,
    // and the target ball would fall short of the pocket. Both legs of the
    // shot are now accounted for.
    // ========================================================================
    static double calculatePowerForTargetToPocket(
        double cueToBallDist,      // Distance from cue to collision point
        double ballToPocketDist,   // Distance the TARGET ball must travel
        const FrictionProperties& friction
    ) {
        if (ballToPocketDist < 1.0) ballToPocketDist = 1.0;
        if (cueToBallDist < 1.0) cueToBallDist = 1.0;

        // Sliding-phase deceleration constant matching the real engine
        // (see Prediction.fast.h's decompiled calcVelocity reference:
        // _velocityReductionSlidingFactor == 196.0). Using a generic
        // friction*gravity formula here would drastically under/over
        // estimate the power needed.
        double slidingDeceleration = friction._velocityReductionSlidingFactor;
        if (slidingDeceleration < 1.0) slidingDeceleration = 196.0;

        double velocityForTarget = std::sqrt(2.0 * slidingDeceleration * ballToPocketDist);
        double velocityForCue    = std::sqrt(2.0 * slidingDeceleration * cueToBallDist);

        // Combine both legs, plus a margin for collision transfer
        // inefficiency (energy lost in the cue->target collision itself).
        double power = (velocityForCue + velocityForTarget) * 1.25;

        return std::min(std::max(power, 100.0), 666.0);
    }

    // ========================================================================
    // GHOST BALL POSITION: where the CUE BALL's center must be at the
    // moment of contact so the target ball travels toward `aimPoint`
    // (normally the pocket — but for bank shots we pass in a MIRRORED
    // pocket position instead, see ScanFast's bank-shot section below).
    // ========================================================================
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

    // ========================================================================
    // Aim angle for the cue ball: direction from the CUE BALL to the GHOST
    // BALL position (NOT direction from target->pocket, which ignores
    // where the cue ball actually is).
    // ========================================================================
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

    static Point2D calculateCollisionPoint(
        const Point2D& targetBallPos,
        const Point2D& aimPoint
    ) {
        return calculateGhostBallPosition(targetBallPos, aimPoint);
    }

    // ========================================================================
    // Shot accuracy: alignment between cue->target and target->aimPoint.
    // ========================================================================
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

    // ========================================================================
    // Shot score (lower = better/preferred).
    // bankPenalty: multiplier applied to bank-shot candidates so direct
    // shots are always tried first (bank shots are inherently riskier).
    // ========================================================================
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

    static bool validateCueBallSafety(const Prediction& pred) {
        return pred.guiData.balls[0].onTable;
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
    // BANK SHOT GEOMETRY: mirror a pocket across one of the table's 4 flat
    // rail walls. A ball aimed at the MIRRORED pocket position will, upon
    // colliding with the real wall, naturally redirect toward the REAL
    // pocket (standard mirror-image bank-shot trick). This is only an
    // INITIAL angle estimate — the actual simulation (determineShotResult)
    // still has to validate that the ball really ends up in the correct
    // pocket, since this simplified rectangular-wall approximation ignores
    // the table's rounded corners / pocket cutout shape.
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
        outWalls[0] = { -halfLen, true  }; // left rail
        outWalls[1] = {  halfLen, true  }; // right rail
        outWalls[2] = { -halfWid, false }; // top rail
        outWalls[3] = {  halfWid, false }; // bottom rail
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
// Set when BOTH ScanFast and the exhaustive ScanSlow have failed to find any
// shot for this exact cue ball position. Prevents AutoPlay from endlessly
// re-running the same doomed scan cycle (which would otherwise look exactly
// like a permanent freeze) — it only retries once the cue ball moves.
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
// HUMAN ANGLE DRAG — self-correcting touch-drag cue rotation
// ============================================================================
// Drags a finger across the table to rotate the cue toward `targetAngle`,
// the same way a human player would. Because the exact pixels-per-radian
// relationship can't be perfectly calibrated without testing on the real
// device/resolution, this performs the drag, then READS BACK the actual
// resulting angle from memory (mVisualGuide().mAimAngle()) and performs a
// small corrective follow-up drag if there's still meaningful error — up to
// a few attempts. This makes the system self-calibrating: even if the
// initial sensitivity guess is off, it converges on the correct angle.
// Tune `fAngleDragSensitivity` (pixels per radian) in the settings menu if
// you want fewer correction passes needed in practice.
// ============================================================================
struct HumanAngleDrag {
    enum State { HAD_IDLE, HAD_DRAGGING, HAD_FINISHED } state = HAD_IDLE;
    int touchIndex = 9; // distinct from PowerSlider(10) and ButtonClicker(11)

    double targetAngle = 0.0;
    ImVec2 dragOrigin{};
    ImVec2 dragTo{};
    ImVec2 dragCurrent{};

    float elapsed = 0.f;
    float duration = 0.f;
    int correctionAttempts = 0;
    static constexpr int MAX_CORRECTIONS = 4;
    static constexpr double ANGLE_TOLERANCE = 0.01; // ~0.57 degrees

    bool active = false;
    bool done = false;

    static double AngleDiff(double a, double b) {
        double d = a - b;
        while (d > M_PI) d -= 2.0 * M_PI;
        while (d < -M_PI) d += 2.0 * M_PI;
        return d;
    }

    static float Jitter(float t, float seed, float amp) {
        return amp * (
            sinf(t * 19.1f + seed)        * 0.5f +
            sinf(t * 37.3f + seed * 1.4f) * 0.3f
        );
    }

    void BeginSegment(double angleDelta) {
        float sens = persistent_float["fAngleDragSensitivity"];
        if (sens <= 1.0f) sens = 220.0f; // default px/rad — tune in settings if needed

        // Drag starting point: roughly the center of the table on screen,
        // with a touch of randomization so consecutive shots don't look
        // robotically identical.
        float originX = (float)((TABLE_LEFT + TABLE_RIGHT) * 0.5) + (float)((rand() % 40) - 20);
        float originY = (float)((TABLE_TOP + TABLE_BOTTOM) * 0.5) + (float)((rand() % 20) - 10);
        dragOrigin = ImVec2(originX, originY);
        dragCurrent = dragOrigin;

        float dx = (float)(angleDelta * sens);
        dragTo = ImVec2(dragOrigin.x + dx, dragOrigin.y);

        elapsed = 0.f;
        duration = 0.45f + (rand() % 250) * 0.001f; // ~450-700ms (lebih pelan & natural)

        NativeTouchesBegin(touchIndex, dragOrigin.x, dragOrigin.y);
        state = HAD_DRAGGING;
    }

    void Begin(double angle) {
        if (active) return;
        targetAngle = angle;
        correctionAttempts = 0;
        active = true;
        done = false;

        double currentAngle = sharedGameManager.mVisualCue().getShotAngle();
        double delta = AngleDiff(targetAngle, currentAngle);
        BeginSegment(delta);
    }

    void Update() {
        if (!active || state != HAD_DRAGGING) return;

        float dt = ImGui::GetIO().DeltaTime;
        elapsed += dt;
        float t = std::min(1.f, elapsed / duration);

        // ease-in-out cubic
        float ease = (t < 0.5f) ? (4.f * t * t * t) : (1.f - powf(-2.f * t + 2.f, 3.f) / 2.f);
        float jamp = 0.8f * (1.f - ease * 0.5f);

        dragCurrent = ImVec2(
            dragOrigin.x + (dragTo.x - dragOrigin.x) * ease + Jitter(t, (float)touchIndex, jamp),
            dragOrigin.y + Jitter(t, (float)touchIndex + 3.f, jamp * 0.4f)
        );
        NativeTouchesMove(touchIndex, dragCurrent.x, dragCurrent.y);

        if (dynamic_bool["DebugTouch"]) {
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            fg->AddCircleFilled(dragCurrent, 14.f, IM_COL32(80, 200, 255, 150));
            fg->AddLine(dragOrigin, dragTo, IM_COL32(80, 200, 255, 90), 2.f);
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
                correctionAttempts++;
                BeginSegment(remaining); // small follow-up correction drag
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
    // Drives: angle drag -> short "thinking" pause -> power drag/release
    // (reuses the existing PowerSlider for the power phase, which already
    // does a natural press->drag->hold->release with timing variance).
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
    // JANGAN SET AIM LANGSUNG — biar humanAngleDrag yang gerakin pelan
    humanAngleDrag.targetAngle = angle;
    humanAngleDrag.active = true;
    humanAngleDrag.done = false;
    humanAngleDrag.correctionAttempts = 0;
    
    double currentAngle = sharedGameManager.mVisualCue().getShotAngle();
    double delta = humanAngleDrag.AngleDiff(angle, currentAngle);
    humanAngleDrag.BeginSegment(delta);
    
    humanExecState = H_ANGLE;
}

    void HumanShootUpdate() {
        switch (humanExecState) {
            case H_POWER: {
    if (!powerSlider.Active) {
        // Power drag selesai, ambil posisi akhir power
        double finalPower = powerSlider.CurrentPower * 666.0;
        if (finalPower < 100.0) finalPower = 100.0;
        if (finalPower > 666.0) finalPower = 666.0;
        
        // Eksekusi tembakan
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
            
            case H_ANGLE: {
    humanAngleDrag.Update();
    if (humanAngleDrag.done) {
        // HAPUS setAimAngle() — biar aim tetap di posisi hasil drag
        // setAimAngle(humanAngleDrag.targetAngle); // ← COMMENT ATAU HAPUS
        humanThinkTimer = 0.50f + (rand() % 400) * 0.001f;
        humanExecState = H_THINK;
    }
    break;
}
            case H_THINK: {
    humanThinkTimer -= ImGui::GetIO().DeltaTime;
    if (humanThinkTimer <= 0.f) {
        // ... kode setup slider (biarkan sama) ...
        float dragTime = 0.80f + (rand() % 300) * 0.001f; // 800-1100ms (lebih pelan)
        float holdTime = 0.30f + (rand() % 150) * 0.001f; // 300-450ms (tahan lebih lama)
        powerSlider.SimulateDrag(rect, (float)humanPendingPower, dragTime, holdTime);
        humanExecState = H_POWER;
    }
    break;
}
            default: break;
        }
    }

    // ========================================================================
    // HELPER: Set aim angle (instant, memory-only — used by non-human mode)
    // ========================================================================
  /*  void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    void setShotPower(double power) {
        lastSetPower = power;
        sharedGameManager.mVisualCue().setShotPower(power);
    }*/

    // ========================================================================
    // HELPER: Execute shot (instant / non-human path)
    // ========================================================================
    void takeShot(double angle, double power) {
        setAimAngle(angle);
        setShotPower(power);
        gPrediction->determineShotResult(false, angle, power);
        sharedGameManager.mVisualCue().mPower(ShotPowerToPower(power));
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }

/*void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
        fullyExhaustedCuePos = { -1000.0, -1000.0 };
    }*/

    // ========================================================================
    // MAIN: Execute shot with nomination + human/instant branching
    // ========================================================================
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
    // SCAN FAST: Direct shots + bank shots (1-cushion), with small angle/
    // power refinement window for robustness
    // ========================================================================
    void ScanFast(double angleStep = ANGLE_STEP_FAST) {
        if (g_CurrentCandidate.idx != -1) return;
        if (gPrediction->guiData.balls[0].initialPosition == lastFailedCuePos) {
            // FIX: previously this returned WITHOUT setting scan=SLOW, so on
            // the next IDLE->SCANNING transition `scan` got reset to FAST
            // again, hit this same early-return, and got stuck forever.
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

        // ====================================================================
        // ITERATE: All balls on table
        // ====================================================================
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

            // ================================================================
            // ITERATE: All pockets
            // ================================================================
            for (int pocketIdx = 0; pocketIdx < (int)pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;

                Point2D pocket = pockets[pocketIdx];

                // ---- DIRECT SHOT candidate ----
                Point2D ballToPocket = pocket - ball.initialPosition;
                double ballToPocketDist = std::sqrt(ballToPocket.square());

                if (PhysicsEngine::isPocketReachable(ballToPocketDist)) {
                    Point2D collisionPoint = PhysicsEngine::calculateCollisionPoint(ball.initialPosition, pocket);
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

                // ---- BANK SHOT candidates (1-cushion, one per wall) ----
                // Mirror-image trick: aim the target ball at the pocket's
                // reflection across a rail; after bouncing off that real
                // rail it heads toward the real pocket. This is only an
                // initial estimate (table corners/pockets aren't perfectly
                // rectangular) — the refinement step below and the real
                // physics validation confirm whether it actually works.
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
                    // Bank shots are riskier than direct shots -> penalized
                    // so direct shots always get tried first when available.
                    double score = PhysicsEngine::calculateShotScore(ballToMirrorDist, accuracy, ballType, myBallType, isMyBall, 2.5);
                    double power = PhysicsEngine::calculatePowerForTargetToPocket(cueToGhostDist, ballToMirrorDist, cachedFriction);

                    candidates.push_back({i, angle, score, pocketIdx, power});
                }
            }
        }

        std::sort(candidates.begin(), candidates.end());

        bool foundShot = false;

        // ====================================================================
        // VALIDATE: Each candidate, with a small refinement window around
        // the computed angle/power (handles imprecision in the power
        // formula and, especially for bank shots, imprecision from the
        // simplified rectangular-wall mirror estimate).
        // ====================================================================
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
    // SCAN SLOW: Exhaustive angle search (last resort, direct shots only)
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
            // FIX: remember that this exact cue position has been fully
            // exhausted (both ScanFast AND ScanSlow failed), so Update()
            // won't endlessly restart the same doomed scan cycle.
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

        // Small "Human Autoplay" indicator badge under the main button so
        // it's visible at a glance whether human-style execution is on.
        if (persistent_bool["bHumanAutoplay"]) {
            ImDrawList* fg = GetForegroundDrawList();
            ImVec2 badgePos(io.DisplaySize.x - 155 - windowWidth, io.DisplaySize.y - 20 - windowHeight - 22);
            fg->AddRectFilled(badgePos, ImVec2(badgePos.x + windowWidth, badgePos.y + 18), IM_COL32(40, 120, 200, 200), 4.0f);
            ImVec2 textSz = CalcTextSize("HUMAN");
            fg->AddText(ImVec2(badgePos.x + (windowWidth - textSz.x) * 0.5f, badgePos.y + 1), IM_COL32(255, 255, 255, 255), "HUMAN");
        }
    }

    // ========================================================================
    // CHECK: Is animation active?
    // ========================================================================
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
        HumanShootUpdate(); // selalu jalan tiap frame
        DrawToggleButton();

        if (isAnimationActive()) return;
        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            if (!HumanShootBusy()) state = IDLE;
            return;
        }

        // Kalau masih eksekusi shot human mode, jangan scan
        if (HumanShootBusy()) return;

        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
        }
        if (state == SCANNING) {
            // If we've already exhaustively searched (both fast AND slow)
            // this exact cue ball position and found nothing, don't
            // restart the whole search — wait for the cue ball to move.
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
            // HumanShootUpdate() sudah handle ClearState + state = IDLE
            // waktu shot selesai, jadi di sini tinggal tunggu
        }
    }
};
