#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <vector>

#include "ScreenTable.h"
#include "PhysicsModel.h"
#include "GameSpeedControl.h"
//#include "8bp/FrictionProperties.h"
//#include "ButtonClicker.h"

using namespace ImGui;

// ============================================================================
// CONSTANTS & CONFIGURATION
// ============================================================================
#ifndef PI
#define PI 3.14159265358979323846
#endif

constexpr double maxAngle = 360.0 / (180.0 / M_PI);
const double TWO_PI = 2.0 * PI;
const double ANGLE_STEP_FAST = 0.05;      // 0.05 radians (~2.86 degrees)
const double ANGLE_STEP_SLOW = 0.02;      // 0.02 radians (~1.15 degrees)
const double MIN_POCKET_DIST = 5.0;       // Minimum distance to pocket (was 40 — rejected easy tap-ins)
const double MAX_POCKET_DIST = 300.0;     // Maximum distance to pocket (was 120 — table diagonal ~280+)
const double BALL_SAFETY_MARGIN = 5.0;    // Safety margin around ball

static double EaseInOutCubic(double t) {
    return t < 0.5 ? 4 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

double normalizeAngle(double angle) {
    double newAngle = angle;
    if (newAngle >= maxAngle) newAngle = fmod(newAngle, maxAngle);
    else if (newAngle < 0) newAngle = maxAngle - fmod(-newAngle, maxAngle);
    return newAngle;
}

// Ball type classifications
enum BallType {
    CUE_BALL = 0,
    SOLIDS = 1,      // 1-7
    STRIPES = 2,     // 9-15
    EIGHT_BALL = 3,
    ANY = 4,        // ← TAMBAHKAN!
    INVALID = -1
};

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

BallType getBallType(int ballIndex) {
    if (ballIndex == 0) return CUE_BALL;
    if (ballIndex == 8) return EIGHT_BALL;
    if (ballIndex >= 1 && ballIndex <= 7) return SOLIDS;
    if (ballIndex >= 9 && ballIndex <= 15) return STRIPES;
    return INVALID;
}

// ============================================================================
// PHYSICS ENGINE - CORRECTED FOR PROPER BALL COLLISION
// ============================================================================
struct PhysicsEngine {
    static constexpr double BALL_DIAMETER = 2.0 * 3.800475;
    static constexpr double GRAVITY = 9.81;
    
    // ========================================================================
    // EQUATION 1: Calculate cue power to transfer momentum to target ball
    // Physics: Elastic collision - cue ball transfers energy to target
    // v_target = (2 * m_cue / (m_cue + m_target)) * v_cue
    // Simplified: v_target = v_cue (equal mass)
    // Required: Power = Distance * Friction_Coefficient
    // ========================================================================
    static double calculatePowerForTargetToPocket(
        double cueToBallDist,      // Distance from cue to target ball
        double ballToPocketDist,   // Distance from target ball to pocket (ACTUAL SHOT)
        const FrictionProperties& friction
    ) {
        // The target ball needs to travel from its position to the pocket
        // This is the ACTUAL distance the ball must cover
        if (ballToPocketDist < 1.0) return 80.0;

        constexpr double POWER_SCALING = 35.0;
        
        // Friction deceleration: a = -mu * g
        double friction_coeff = friction._velocityReductionRollingFactor;
        double deceleration = GRAVITY * friction_coeff;
        
        // CORRECTED: Power needed for TARGET BALL to reach pocket
        // v^2 = 2 * a * s  =>  v = sqrt(2 * a * s)
        double requiredVelocity = std::sqrt(2.0 * deceleration * ballToPocketDist);

        double cueVelocity = requiredVelocity / 0.9;
        
        // Add extra power to overcome collision loss (elastic collision efficiency ~90%)
        double powerWithCollisionLoss = requiredVelocity / 0.9;  // Account for energy loss
        
        // Map to power scale (0-666)
        double power = cueVelocity * POWER_SCALING;

        if (ballToPocketDist > 150.0) power *= 1.1;
        if (ballToPocketDist > 250.0) power *= 1.15;
        
        return std::min(std::max(power, 80.0), 666.0);
    }
    
    // ========================================================================
    // GHOST BALL POSITION: where the CUE BALL's center must be at the moment
    // of contact so the target ball travels toward the pocket. This is
    // BALL_DIAMETER (2 * radius) away from the target ball's center, along
    // the line from the pocket through the target ball.
    // ========================================================================
    static Point2D calculateGhostBallPosition(
        const Point2D& targetBallPos,
        const Point2D& pocketPos
    ) {
        Point2D toPocket = pocketPos - targetBallPos;
        double distance = std::sqrt(toPocket.square());
        if (distance < 0.1) return targetBallPos;

        Point2D direction = toPocket * (1.0 / distance);
        return targetBallPos - direction * BALL_DIAMETER;
    }

    // ========================================================================
    // EQUATION 2: Calculate the AIM ANGLE for the cue ball.
    //
    // FIX: previously this returned atan2(pocket - targetBall) — the
    // direction the TARGET ball needs to travel, completely ignoring where
    // the CUE BALL actually is. That's only correct by coincidence (cue,
    // target, and pocket happen to be collinear). The real aim angle is the
    // direction from the CUE BALL to the GHOST BALL position.
    // ========================================================================
    static double calculateAngleToPocket(
        const Point2D& cueBallPos,
        const Point2D& targetBallPos,
        const Point2D& pocketPos
    ) {
        Point2D ghostBallPos = calculateGhostBallPosition(targetBallPos, pocketPos);

        Point2D cueToGhost = ghostBallPos - cueBallPos;
        double angle = std::atan2(cueToGhost.y, cueToGhost.x);

        if (angle < 0) angle += TWO_PI;
        return angle;
    }
    
    // ========================================================================
    // EQUATION 3: Collision point == ghost ball position (kept for backward
    // compatibility with call sites that use it for distance calculations).
    // FIX: now uses BALL_DIAMETER via calculateGhostBallPosition (was
    // BALL_RADIUS — half the correct distance).
    // ========================================================================
    static Point2D calculateCollisionPoint(
        const Point2D& targetBallPos,
        const Point2D& pocketPos
    ) {
        return calculateGhostBallPosition(targetBallPos, pocketPos);
    }
    
    // ========================================================================
    // EQUATION 4: Calculate shot accuracy (how direct is the path?)
    // Physics: Accuracy = alignment between target->pocket and cue->target
    // Direct path = 1.0, off-angle = 0.0
    // ========================================================================
    static double calculateShotAccuracy(
        const Point2D& cueBallPos,
        const Point2D& targetBallPos,
        const Point2D& pocketPos
    ) {
        // Vector from cue to target
        Point2D cueToTarget = targetBallPos - cueBallPos;
        // Vector from target to pocket
        Point2D targetToPocket = pocketPos - targetBallPos;
        
        double cueToTargetLen = std::sqrt(cueToTarget.square());
        double targetToPocketLen = std::sqrt(targetToPocket.square());
        
        if (cueToTargetLen < 0.1 || targetToPocketLen < 0.1) return 0.0;
        
        // Dot product: measures alignment
        double dotProduct = (cueToTarget.x * targetToPocket.x) + 
                           (cueToTarget.y * targetToPocket.y);
        
        // Normalize to [0, 1]
        double accuracy = dotProduct / (cueToTargetLen * targetToPocketLen);
        
        return std::max(0.0, std::min(1.0, accuracy));
    }
    
    // ========================================================================
    // EQUATION 5: Calculate shot score (lower = better)
    // Score: (distance_to_pocket) * (1 - accuracy) * ball_priority
    // ========================================================================
    static double calculateShotScore(
        double targetBallToPocketDist,  // ACTUAL distance ball travels
        double accuracy,
        BallType ballType,
        BallType myBallType,
        bool isMyBall
    ) {
        // Base: favor close pockets
        double baseScore = targetBallToPocketDist;
        
        // Reward accuracy (direct shots are better)
        baseScore *= (1.0 - (accuracy * 0.5));  // High accuracy = lower score
        
        // Ball type priority
        if (isMyBall) {
            // MY BALLS: 0.2x multiplier (STRONG PRIORITY - chosen first)
            baseScore *= 0.2;
        } else if (ballType != EIGHT_BALL) {
            // OPPONENT BALLS: 3.0x multiplier (LOW PRIORITY - fallback only)
            baseScore *= 3.0;
        }
        
        return baseScore;
    }
    
    // ========================================================================
    // Validate pocket is reachable from target ball
    // ========================================================================
    static bool isPocketReachable(double targetBallToPocketDist) {
        return targetBallToPocketDist >= MIN_POCKET_DIST && 
               targetBallToPocketDist <= MAX_POCKET_DIST;
    }
    
    // ========================================================================
    // Validate cue ball won't scratch (won't be potted)
    // ========================================================================
    static bool validateCueBallSafety(const Prediction& pred) {
        return pred.guiData.balls[0].onTable;  // Cue ball still on table
    }
    
    // ========================================================================
    // Validate 8-ball won't be potted prematurely
    // ========================================================================
    static bool validateEightBallSafety(const Prediction& pred, BallType myBallType) {
        auto& ball8 = pred.guiData.balls[8];
        
        // 8-ball must stay on table until it's your turn (you've cleared all yours)
        if (ball8.originalOnTable && !ball8.onTable && myBallType != EIGHT_BALL) {
            return false;  // 8-ball was knocked in prematurely!
        }
        
        return true;
    }
    
    // ========================================================================
    // Validate first ball hit matches player's ball type
    // ========================================================================
    static bool validateFirstHit(const Prediction& pred, BallType myBallType, BallType targetBallType) {
        auto firstHit = pred.guiData.collision.firstHitBall;
        if (!firstHit) return false;
        
        BallType hitType = getBallType(firstHit->index);
        
        // Kalau open table (belum tahu jenis bola)
        if (myBallType == ANY) {
            return hitType != EIGHT_BALL && hitType != CUE_BALL;
        }
        
        // Kalau lagi giliran 8-ball
        if (myBallType == EIGHT_BALL) {
            return hitType == EIGHT_BALL;
        }
        
        // Kalau udah tahu jenis bola → first hit HARUS bola sendiri!
        if (myBallType == SOLIDS || myBallType == STRIPES) {
            return hitType == myBallType;
        }
        
        return false;
    }
    
    // ========================================================================
    // Validate target ball actually gets potted (not opponent ball)
    // ========================================================================
    static bool validateTargetBallPocketed(const Prediction& pred, int targetIdx) {
        // CORRECTED: Check that ONLY the target ball was potted
        // Not opponent balls by accident
        auto& targetBall = pred.guiData.balls[targetIdx];
        
        return targetBall.originalOnTable && !targetBall.onTable;
    }
};

// ============================================================================
// GAME STATE & HELPER FUNCTIONS
// ============================================================================
Point2D lastFailedCuePos = { -1000.0, -1000.0 };
static inline Point2D lastSetCuePos = {-1000, -1000};

/*BallType getPlayerBallType(Ball::Classification classification) {
    if (classification == Ball::Classification::STRIPE) return STRIPES;
    if (classification == Ball::Classification::EIGHT_BALL) return EIGHT_BALL;
    // SOLID atau ANY (open table) → default ke SOLIDS
    return SOLIDS;
}*/

BallType getPlayerBallType(Ball::Classification classification) {
    if (classification == Ball::Classification::ANY) return ANY;
    if (classification == Ball::Classification::SOLID) return SOLIDS;
    if (classification == Ball::Classification::STRIPE) return STRIPES;
    if (classification == Ball::Classification::EIGHT_BALL) return EIGHT_BALL;
    return INVALID;
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
    
    // ── Random engine ──
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<> humanDelayDist(0.15, 0.4);
    
    static double nowSec() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration<double>(duration).count();
    }

    // ========================================================================
    // HELPER: Set aim angle
    // ========================================================================
    void setAimAngle(double angle) {
        if (!sharedGameManager) return;
        auto vc = sharedGameManager.mVisualCue();
        if (!vc) return;
        auto vg = vc.mVisualGuide();
        if (!vg) return;
        lastSetCuePos = gPrediction->guiData.balls[0].initialPosition;
        vg.mAimAngle(angle);
    }

    // ========================================================================
    // HELPER: Set shot power
    // ========================================================================
    void setShotPower(double power) {
        lastSetPower = power;
        sharedGameManager.mVisualCue().setShotPower(power);
    }

    // ========================================================================
    // HELPER: Execute shot
    // ========================================================================
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
        // BUG FIX: pendingShotPower tidak selalu sync dengan targetPower.
        // takeShot() set targetPower, tapi triggerShot() baca pendingShotPower → power salah/lebih pelan.
        // Pakai targetPower yang di-set oleh takeShot() dan di-hold sepanjang human state machine.
        g_postShotPower = targetPower;
        g_postShotFrames = 15;
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }
    
    // ========================================================================
    // HELPER: Clear state
    // ========================================================================
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
        state = IDLE;
        humanState = HUM_IDLE;
        spinIsLocked = false; // Unlock spin supaya scan berikutnya lock spin fresh
    }
    
    // ========================================================================
    // MAIN: Execute shot with nomination
    // ========================================================================
    void Shoot(double angle, double power = 0.f) {
        setAimAngle(angle);
        setShotPower(power);
        gPrediction->determineShotResult(false, angle, power);

        bool nominating = false;
        int nominationMode = sharedGameManager.getPocketNominationMode();
        auto myclass = sharedGameManager.getPlayerClassification();
        
        if ((nominationMode == 1 && myclass == Ball::Classification::EIGHT_BALL) || 
            (nominationMode == 2 && myclass != Ball::Classification::ANY)) {
            // FIX: guard against safety-shot candidates (idx=-2,
            // pocketIndex=-1, set by ScanSlow's fallback). Without these
            // extra checks, `getNominatedPocket() != -1` is almost always
            // true, so `nominating` becomes true and
            // GetPocketScreenPos(-1) reads pockets[-1] — out of bounds —
            // producing a garbage screen position that buttonClicker then
            // clicks on.
            if (g_CurrentCandidate.idx > 0 && g_CurrentCandidate.pocketIndex >= 0
                && sharedGameManager.getNominatedPocket() != g_CurrentCandidate.pocketIndex) {
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
    
    // ========================================================================
    // SCAN FAST: Quick scan with physics-corrected angle calculation
    // ========================================================================
    void ScanFast(double angleStep = ANGLE_STEP_FAST) {
        if (g_CurrentCandidate.idx != -1) return;
        if (gPrediction->guiData.balls[0].initialPosition == lastFailedCuePos) return;
        
        if (!spinIsLocked) {
            lockedShotSpin = sharedGameManager.getShotSpin();
            spinIsLocked = true;
        }

        Ball::Classification playerClass = sharedGameManager.getPlayerClassification();
        BallType myBallType = getPlayerBallType(playerClass);
        bool isOpenTable = (playerClass == Ball::Classification::ANY);
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        
        std::vector<Candidate> candidates;
        auto pockets = getPockets();
        auto& cueBall = gPrediction->guiData.balls[0];
        
        // ====================================================================
        // ITERATE: All balls on table
        // ====================================================================
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;
            
            BallType ballType = getBallType(i);
            bool isMyBall = false;
            if (isOpenTable) {
                // OPEN TABLE: semua bola (kecuali 8-ball & cue) dianggap "milik sendiri"
                isMyBall = (ballType != EIGHT_BALL && ballType != CUE_BALL);
            } else {
                // UDAH TAHU JENIS BOLA: cuma bola dengan jenis yang sama yang dianggap milik sendiri
                isMyBall = (ballType == myBallType);
            }
            
            bool isCandidate = false;
            if (isMyBall) {
                isCandidate = true;  // Bola sendiri → PRIORITAS
            } else if (!isOpenTable && ballType != EIGHT_BALL && ballType != CUE_BALL) {
                isCandidate = true;  // Bola lawan → FALLBACK (cuma kalau udah tahu jenis bola)
            }
            
            if (!isCandidate) continue;

            // ================================================================
            // ITERATE: All pockets
            // ================================================================
            for (int pocketIdx = 0; pocketIdx < pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;

                Point2D pocket = pockets[pocketIdx];
                
                // CORRECTED: Distance from TARGET BALL to POCKET (not cue to target)
                Point2D ballToPocket = pocket - ball.initialPosition;
                double ballToPocketDist = std::sqrt(ballToPocket.square());
                
                if (!PhysicsEngine::isPocketReachable(ballToPocketDist)) continue;
                
                // CORRECTED: Calculate where cue should hit the target ball
                // So that target ball goes DIRECTLY toward pocket
                Point2D collisionPoint = PhysicsEngine::calculateCollisionPoint(
                    ball.initialPosition,
                    pocket
                );
                
                // Calculate angle from cue to collision point
                Point2D cueToCollision = collisionPoint - cueBall.initialPosition;
                double cueToCollisionDist = std::sqrt(cueToCollision.square());
                if (cueToCollisionDist < 0.1) continue;
                
                double angle = PhysicsEngine::calculateAngleToPocket(
                    cueBall.initialPosition,
                    ball.initialPosition,
                    pocket
                );
                
                // Calculate accuracy
                double accuracy = PhysicsEngine::calculateShotAccuracy(
                    cueBall.initialPosition,
                    ball.initialPosition,
                    pocket
                );
                
                // Calculate score
                double score = PhysicsEngine::calculateShotScore(
                    ballToPocketDist,
                    accuracy,
                    ballType,
                    myBallType,
                    isMyBall
                );
                
                // CORRECTED: Calculate power needed for TARGET BALL to reach pocket
                double power = PhysicsEngine::calculatePowerForTargetToPocket(
                    cueToCollisionDist,
                    ballToPocketDist,
                    cachedFriction
                );
                
                candidates.push_back({i, angle, score, pocketIdx, power});
            }
        }
       std::sort(candidates.begin(), candidates.end());
        
        bool foundShot = false;
        
        // ====================================================================
        // VALIDATE: Each candidate
        // ====================================================================
        for (const auto& cand : candidates) {
            double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(cand.angle));
            gPrediction->determineShotResult(true, angle, cand.power, lockedShotSpin, cand);
                     
            // Safety checks
            if (!PhysicsEngine::validateCueBallSafety(*gPrediction)) continue;
            if (!PhysicsEngine::validateEightBallSafety(*gPrediction, myBallType)) continue;
            if (!PhysicsEngine::validateFirstHit(*gPrediction, myBallType, getBallType(cand.idx))) continue;
            if (!PhysicsEngine::validateTargetBallPocketed(*gPrediction, cand.idx)) continue;
            
            // Verify target ball is in correct pocket
            if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) continue;
            
            LOGI("AutoPlay: FAST - Ball %d angle %f power %f", cand.idx, angle, cand.power);
            g_CurrentCandidate = cand;
            foundShot = true;
            Shoot(angle, cand.power);
            break;
        }

        if (!foundShot) {
            lastFailedCuePos = cueBall.initialPosition;
            LOGI("AutoPlay: ScanFast failed, switching to ScanSlow");
            scan = SLOW;
        }
    }

    // ========================================================================
    // SCAN SLOW: Exhaustive angle search for any possible shot
    // ========================================================================
    void ScanSlow(double angleStep = ANGLE_STEP_SLOW) {
        static double currentScanAngle = 0.0;
        static bool isScanning = false;
        static Point2D lastScanCuePos = { -1000.0, -1000.0 };

        // Safety fallback: first angle/power found during this scan that
        // legally hits OUR ball first (doesn't scratch, doesn't pot the
        // 8-ball prematurely), even if nothing actually goes in a pocket.
        // Used so the bot ALWAYS shoots something rather than freezing.

        if (g_CurrentCandidate.idx != -1) return;
        
        if (!isScanning || gPrediction->guiData.balls[0].initialPosition != lastScanCuePos) {
            currentScanAngle = 0.0;
            isScanning = true;
            lastScanCuePos = gPrediction->guiData.balls[0].initialPosition;
            // Lock spin saat mulai scan baru
            if (!spinIsLocked) {
                lockedShotSpin = sharedGameManager.getShotSpin();
                spinIsLocked = true;
            }
        }

        Ball::Classification playerClass = sharedGameManager.getPlayerClassification();
        BallType myBallType = getPlayerBallType(playerClass);
        bool isOpenTable = (playerClass == Ball::Classification::ANY);
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        auto& cueBall = gPrediction->guiData.balls[0];
        
        int steps = 0;
        int stepsPerFrame = (int)(20 * GameSpeed::getAnimationMultiplier());
        
        // ====================================================================
        // ITERATE: All angles
        // ====================================================================
        while (steps < stepsPerFrame && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            // Strategic power levels for testing
           // std::vector<double> powers = {666.0, 500.0, 350.0, 200.0, 100.0};
          //  std::vector<double> powers = {666.0, 500.0, 350.0, 200.0, 100.0};
            std::vector<double> powers = {666.0, 550.0, 450.0, 350.0, 250.0, 150.0, 100.0, 80.0};
            for (double power : powers) {
                gPrediction->determineShotResult(true, angle, power, lockedShotSpin);
                
                // Safety checks FIRST
                if (!PhysicsEngine::validateCueBallSafety(*gPrediction)) continue;
                if (!PhysicsEngine::validateEightBallSafety(*gPrediction, myBallType)) continue;
                if (!PhysicsEngine::validateFirstHit(*gPrediction, myBallType, myBallType)) continue;
               // if (!PhysicsEngine::validateFirstHit(*gPrediction, myBallType, myBallType)) continue;
                
                // This angle/power legally hits our own ball first without
                // fouling — remember it as a fallback "safety" shot (use the
                // Find what was potted
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
            LOGI("AutoPlaySlow: Exhaustive scan complete, no potting shot found");
            isScanning = false;
            currentScanAngle = 0.0;
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
        DrawToggleButton();

        if (isAnimationActive()) return;
        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            if (humanState != HUM_IDLE) return;
            // Kalau sedang EXECUTING (nomination → shot), jangan reset
            if (state == EXECUTING) return;
            NativeTouchesEnd(5, 0, 0);
            state = IDLE;
            return;
        }

        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
        } 
        if (state == SCANNING) {
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
                uint nominatedPocket = sharedGameManager.getNominatedPocket();
                if (nominatedPocket == (uint)g_CurrentCandidate.pocketIndex) {
                    // Nominasi confirmed — re-validasi shot
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
                stateStartTime = now;
                humanState = HUM_PULLING;
            }
            return;
        }
    
        // 6. HUM_PULLING (wait for slider to finish)
        if (humanState == HUM_PULLING) {
          //  if (powerSlider.Active) return;
            // Slider selesai — set angle+power di memory sekali lagi biar sync
            setAimAngle(targetAngle);
            sharedGameManager.mVisualCue().mPower(ShotPowerToPower(targetPower));
            stateStartTime = now;
            humanState = HUM_DELAY_BEFORE_SHOT;
            return;
        }
    
        // 7. HUM_DELAY_BEFORE_SHOT (0.4s cooldown, lalu fire shot)
        if (humanState == HUM_DELAY_BEFORE_SHOT) {
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.4) {
                // Set angle + power di memory sekali lagi biar tidak drift
                setAimAngle(targetAngle);
                sharedGameManager.mVisualCue().mPower(ShotPowerToPower(targetPower));
                // FIRE SHOT
                triggerShot();
                humanShotLocked = false;
                humanState = HUM_IDLE;
                ClearState();
                state = IDLE;
            }
            return;
        }
        
        bool isPlayerTurn = sharedGameManager.mStateManager().isPlayerTurn();
        
        if (humanState != HUM_IDLE) {
        // Lines hilang selama human state machine jalan.
        // Pakai lockedShotSpin supaya simulasi konsisten dengan yang dipilih saat scan.
        gPrediction->determineShotResult(true, targetAngle, targetPower, lockedShotSpin);
    } else if (isPlayerTurn && g_CurrentCandidate.idx == -1) {
        // Setelah shot selesai: unlock spin supaya scan berikutnya bisa lock fresh.
        spinIsLocked = false;
        // Lines tampil lagi, ikuti aim angle real-time.
        if (gPrediction && sharedGameManager) {
            double curAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
            // FIX POWER: mPower() return skala game (0.0–1.0), sedangkan
            // determineShotResult expect skala simulasi (0–666 = langsung velocity).
            // getShotPower() sudah return skala simulasi yang benar.
            // mPower() mentah menyebabkan display lines pakai power jauh lebih kecil
            // dari yang dipakai saat scan → trajectory berbeda → lines meleset.
            double curPower = sharedGameManager.mVisualCue().getShotPower();
            if (curPower < 10.0) curPower = 400.0; // fallback kalau belum ada power
            gPrediction->determineShotResult(false, curAngle, curPower, sharedGameManager.getShotSpin());
        }
    }
    
    }
};
