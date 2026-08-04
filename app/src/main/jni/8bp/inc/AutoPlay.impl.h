#pragma once

#include "AutoPlayy.h"
#include "8bp/GameManager.h"
#include "Prediction.h"
#include "ButtonClicker.h"
extern ButtonClicker buttonClicker;
#include "PowerSlider.h"
extern PowerSlider powerSlider;
#include <math.h>
#include <random>
#include "ScreenTable.h"

ImVec2 WorldToScreen(Vec2d worldPos) {
    double positionX = worldPos.x + TABLE_HALF_WIDTH;
    double positionY = -(worldPos.y + TABLE_HALF_HEIGHT);
    double scrX = TABLE_LEFT + positionX * TABLE_SCALE;
    double scrY = TABLE_BOTTOM + positionY * TABLE_SCALE;
    return ImVec2(scrX, scrY);
}

// --- Static Helpers ---
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

static double EaseInOutCubic(double t) {
    return t < 0.5 ? 4 * t * t * t : 1.0 - pow(-2.0 * t + 2.0, 3.0) / 2.0;
}

// Global random engine
static std::random_device rd;
static std::mt19937 gen(rd());

// ============================================================
// ── EXPERT HUMAN SIMULATION ──
// ============================================================
static std::uniform_real_distribution<> humanDelayDist(0.15, 0.3);
static std::uniform_real_distribution<> humanOvershootDist(0.5, 1.5);

static bool bAimedThisTurn = false;
static Point2D lastCuePosWhenAimed = { -1000.0, -1000.0 };

// ============================================================
// ── DECISION MAKING VARIABLES ──
// ============================================================
static bool bDecisionActive = false;
static int decisionIndex = 0;
static double decisionStartTime = 0.0;
static std::vector<Candidate> decisionCandidates;
static bool decisionFinished = false;

// ==================== CORE IMPLEMENTATIONS ====================

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static constexpr double maxAngle = 2.0 * M_PI;

static double normalizeAngle(double angle) {
    angle = fmod(angle, 2.0 * M_PI);
    if (angle < 0) angle += 2.0 * M_PI;
    return angle;
}

static double CalculateRequiredPower(double totalDist) {
    double p = sqrt(totalDist * 2.0 * 196.0); 
    if (p < 220.0) p = 220.0;
    if (p > 666.0) p = 666.0;
    return p;
}

ImVec2 GetPocketScreenPos(int pocketIdx) {
    Table table = sharedGameManager.mTable;
    if (!table) return {};

    auto tableProperties = table.mTableProperties();
    if (!tableProperties) return {};

    if (pocketIdx < 0 || pocketIdx >= 6) return {};

    auto& pockets = tableProperties.mPockets();
    return WorldToScreen(pockets[pocketIdx]);
}

void AutoPlay::applyAutoSpin() {
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

std::vector<Point2D> AutoPlay::getPockets() {
    auto pts = ::getPockets();
    return std::vector<Point2D>(pts.begin(), pts.end());
}

static inline double g_lastSyncAngle = -999.0;
static inline double g_shotCooldownEnd = 0.0;
static int fastShotState = 0;
static inline Point2D lastScanSlowCuePos = {-1000, -1000};

static double currentScanAngle = 0.0;
static bool isScanningInProgress = false;
static AutoPlay::FastScanState fs;

static double anim_CurrentPower = 0.0;
static double anim_TargetPower = 0.0;
static double anim_TargetAngle = 0.0;
static bool anim_IsPulling = false;
static long long anim_StartTime = 0;
static bool anim_RotationDone = false;
static bool anim_TouchStarted = false;
static double g_lastFastShotTime = 0.0;
static int g_gamesPlayed = 0;

static bool g_postShotLock = false;
static double g_postShotAngle = 0.0;
static double g_postShotPower = 0.0;
static int g_postShotFrames = 0;

static bool g_postAimLock = false;
static double g_postAimAngle = 0.0;
static double g_postAimPower = 0.0;
static int g_postAimFrames = 0;

void AutoPlay::ClearState() {
    g_CurrentCandidate.idx = -1;
    lastFailedCuePos = {-1000, -1000};
    lastSetCuePos = {-1000, -1000};
    humanNeedsNomination = false;
    humanNominationPocket = -1;
    g_autoPlayCalculating = false;
    g_PredictionLocked = false;
    g_lastSyncAngle = -999.0;
    humanState = HUM_IDLE;
    humanShotLocked = false;
    bShowAutoPlayLines = false;
    state = IDLE;
    fastShotState = 0;
    anim_IsPulling = false;
    anim_RotationDone = false;
    anim_TouchStarted = false;
    fs.isInitiated = false;
    bDecisionActive = false;
    decisionCandidates.clear();
    decisionFinished = false;

    if (!g_postShotLock) {
        setPower(0.0);
    }

    g_gamesPlayed++;

    if (anim_TouchStarted) {
        NativeTouchesEnd(5, Width * 0.83f, Height * 0.82f);
    }

    if (powerSlider.Active) {
        float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
        float sliderX = Width * sliderXPercent;
        if (persistent_int[O("iPowerBarSide")] == 1) {
            sliderX = Width * (1.0f - sliderXPercent);
        }
        float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
        NativeTouchesEnd(powerSlider.TouchIndex, sliderX, sliderYStart);
        powerSlider.Active = false;
        powerSlider.state = PowerSlider::IDLE;
    }

    if (buttonClicker.Active) {
        NativeTouchesEnd(buttonClicker.TouchIndex, buttonClicker.ClickPos.x, buttonClicker.ClickPos.y);
        buttonClicker.Active = false;
        buttonClicker.state = ButtonClicker::IDLE;
    }

    g_shotCooldownEnd = AutoPlay::nowSec() + 2.0;
}

void AutoPlay::setAimAngle(double angle) {
    if (!sharedGameManager) return;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return;
    auto vg = vc.mVisualGuide();
    if (!vg) return;
    lastSetCuePos = gPrediction->guiData.balls[0].initialPosition;
    vg.mAimAngle(angle);
}

void AutoPlay::setPower(double power) {
    if (!sharedGameManager) return;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return;
    vc.mPower(ShotPowerToPower(power));
}

double AutoPlay::getCurrentPower() {
    if (!sharedGameManager) return 0.0;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return 0.0;
    return vc.mPower();
}

void AutoPlay::takeShot(double angle, double power, bool preserveStartAngle) {
    anim_TargetAngle = angle;
    anim_TargetPower = power;
    anim_CurrentPower = 0.0;
    anim_IsPulling = true;
    anim_StartTime = 0;
    fastShotState = 0;
    anim_RotationDone = false;
    anim_TouchStarted = false;
    
    if (!preserveStartAngle) {
        if (sharedGameManager && sharedGameManager.mVisualCue() && sharedGameManager.mVisualCue().mVisualGuide()) {
            startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        } else {
            startAngle = angle;
        }
    }
    stateStartTime = nowSec(); 
}

void AutoPlay::triggerShot() {
    g_postShotLock = true;
    g_postShotAngle = (automationSpeed == SPEED_HUMAN) ? targetAngle : anim_TargetAngle;
    g_postShotPower = (automationSpeed == SPEED_HUMAN) ? pendingShotPower : anim_TargetPower;
    g_postShotFrames = 15;
    M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
}

bool AutoPlay::IsAnimationActive() {
    auto visualCue = sharedGameManager.mVisualCue();
    if (!visualCue) return false;
    auto _powerBarView = F(ptr, visualCue + 0x510);
    if (!_powerBarView) return false;
    return (M(ptr, libmain + 0x2de6f30, ptr)(_powerBarView) != 0);
}

// ═══════════════════════════════════════════════════════════════
// ── Shot Validation ──
// ═══════════════════════════════════════════════════════════════
struct ShotValidationResult {
    bool valid;
    int targetIdx;
    int pocketIndex;
    int pocketedCount;
    double score;
};

static ShotValidationResult ValidateShotSimulation(
    Ball::Classification myclass, 
    uint nominatedPocket, 
    bool isNineBallGame,
    int candidateIdx
) {
    ShotValidationResult result = {false, -1, -1, 0, 1e18};

    if (!gPrediction->guiData.balls[0].onTable) return result;

    if (isNineBallGame) {
        int iFoundLowest = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].originalOnTable) {
                iFoundLowest = i;
                break;
            }
        }
        auto firstHit = gPrediction->guiData.collision.firstHitBall;
        if (!firstHit || firstHit->index != iFoundLowest) return result;

        int bestPotted = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) continue;
                if (i == 9) { bestPotted = 9; break; }
                if (bestPotted == -1 || i == candidateIdx) bestPotted = i;
            }
        }
        if (bestPotted == -1) return result;

        result.valid = true;
        result.targetIdx = bestPotted;
        result.pocketIndex = gPrediction->guiData.balls[bestPotted].pocketIndex;
        result.pocketedCount = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) result.pocketedCount++;
        }
        result.score = -(result.pocketedCount * 10000.0);
        return result;
    }

    // Check 8-ball potted early
    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (ball.classification == Ball::Classification::EIGHT_BALL) {
            if (ball.originalOnTable && !ball.onTable) {
                bool onlyEightBallLeft = AllGroupBallsPocketed();
                if (!onlyEightBallLeft) {
                    return result;
                }
            }
        }
    }

    // Check first hit ball
    auto firstHit = gPrediction->guiData.collision.firstHitBall;
    if (firstHit) {
        if (myclass == Ball::Classification::ANY) {
            if (firstHit->classification == Ball::Classification::EIGHT_BALL) return result;
        } else if (firstHit->classification != myclass) {
            return result;
        }
        if (firstHit->classification == Ball::Classification::EIGHT_BALL) {
            bool onlyEightBallLeft = AllGroupBallsPocketed();
            if (!onlyEightBallLeft) return result;
        }
    }

    // Check 8-ball reference
    auto& eightBallRef = gPrediction->guiData.balls[8];
    if (eightBallRef.originalOnTable && !eightBallRef.onTable) {
        bool onlyEightBallLeft = AllGroupBallsPocketed();
        if (!onlyEightBallLeft) return result;
    }

    // Find pocketed balls
    int pocketed = 0;
    int bestTarget = -1;
    int bestPocket = -1;

    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (!ball.originalOnTable || ball.onTable) continue;
        if (ball.classification == Ball::Classification::CUE_BALL) continue;

        bool isLegal = false;
        if (myclass == Ball::Classification::ANY) {
            isLegal = (ball.classification != Ball::Classification::EIGHT_BALL);
        } else if (myclass == Ball::Classification::EIGHT_BALL) {
            isLegal = (ball.classification == Ball::Classification::EIGHT_BALL && AllGroupBallsPocketed());
        } else {
            isLegal = (ball.classification == myclass);
        }
        if (!isLegal) continue;

        if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) continue;

        pocketed++;
        if (i == candidateIdx) {
            bestTarget = i;
            bestPocket = ball.pocketIndex;
        }
        if (bestTarget == -1) {
            bestTarget = i;
            bestPocket = ball.pocketIndex;
        }
    }

    if (pocketed == 0) return result;

    result.valid = true;
    result.targetIdx = bestTarget;
    result.pocketIndex = bestPocket;
    result.pocketedCount = pocketed;
    result.score = -(pocketed * 10000.0);
    return result;
}

// ═══════════════════════════════════════════════════════════════
// ── Count pocketed balls for scoring ──
// ═══════════════════════════════════════════════════════════════
static int CountPocketedBalls(Ball::Classification myclass, uint nominatedPocket) {
    int count = 0;
    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (ball.originalOnTable && !ball.onTable) {
            bool valid = false;
            if (myclass == Ball::Classification::ANY) {
                if (ball.classification != Ball::Classification::CUE_BALL &&
                    ball.classification != Ball::Classification::EIGHT_BALL)
                    valid = true;
            } else {
                if (ball.classification == myclass) valid = true;
            }
            if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) valid = false;
            if (valid) count++;
        }
    }
    return count;
}

// ═══════════════════════════════════════════════════════════════
// ── IsBreakShot detection ──
// ═══════════════════════════════════════════════════════════════
static bool IsBreakShot() {
    int ballsOnTable = 0;
    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        if (gPrediction->guiData.balls[i].originalOnTable) ballsOnTable++;
    }
    return (ballsOnTable >= 14);
}

// ═══════════════════════════════════════════════════════════════
// ── Break Shot Scanner ──
// ═══════════════════════════════════════════════════════════════
static double breakScanAngle = 0.0;
static bool breakScanActive = false;
static Point2D breakScanCuePos = { -1000.0, -1000.0 };

struct BreakCandidateData {
    double angle;
    double power;
    int pocketedCount;
    bool eightBallPocketed;
    double score;
};
static std::vector<BreakCandidateData> breakBestCandidates;

static void ResetBreakScan() {
    breakScanAngle = 0.0;
    breakScanActive = false;
    breakScanCuePos = { -1000.0, -1000.0 };
    breakBestCandidates.clear();
}

static void ScanBreakShot() {
    if (AutoPlay::g_CurrentCandidate.idx != -1) return;
    if (gPrediction->guiData.balls[0].initialPosition == AutoPlay::lastFailedCuePos) return;

    auto& cueBall = gPrediction->guiData.balls[0];

    if (!breakScanActive || cueBall.initialPosition != breakScanCuePos) {
        breakScanAngle = 0.0;
        breakScanActive = true;
        breakScanCuePos = cueBall.initialPosition;
        breakBestCandidates.clear();
    }

    constexpr int BATCH_SIZE = 30;
    double coarseStep = 0.05;
    int stepsThisFrame = 0;

    while (stepsThisFrame < BATCH_SIZE && breakScanAngle < maxAngle) {
        double rawAngle = breakScanAngle;
        breakScanAngle += coarseStep;
        stepsThisFrame++;

        double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(rawAngle));
        
        gPrediction->determineShotResult(true, angle, 666.0, sharedGameManager.getShotSpin());
        
        if (!gPrediction->guiData.balls[0].onTable) continue;
        
        int pocketed = 0;
        bool eightBallPocketed = false;
        bool anyGood = false;
        
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                if (ball.classification == Ball::Classification::EIGHT_BALL) {
                    eightBallPocketed = true;
                } else if (ball.classification != Ball::Classification::CUE_BALL) {
                    pocketed++;
                    anyGood = true;
                }
            }
        }
        
        if (anyGood && !eightBallPocketed) {
            double score = -((double)pocketed * 1000.0);
            breakBestCandidates.push_back({angle, 666.0, pocketed, eightBallPocketed, score});
        }
    }

    if (breakScanAngle >= maxAngle && breakBestCandidates.empty()) {
        auto& cueBall = gPrediction->guiData.balls[0];
        Point2D rackCenter = {0.0, 0.0};
        int rackBalls = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (gPrediction->guiData.balls[i].originalOnTable) {
                rackCenter = rackCenter + gPrediction->guiData.balls[i].initialPosition;
                rackBalls++;
            }
        }
        if (rackBalls > 0) rackCenter = rackCenter * (1.0 / rackBalls);
        double breakBaseAngle = atan2(rackCenter.y - cueBall.initialPosition.y,
                                       rackCenter.x - cueBall.initialPosition.x);
        if (breakBaseAngle < 0) breakBaseAngle += 2.0 * M_PI;

        std::vector<double> breakAngles;
        double offsets[] = {0.0, 0.08, -0.08, 0.15, -0.15, 0.22, -0.22, 0.04, -0.04, 0.30, -0.30};
        for (double off : offsets) {
            breakAngles.push_back(breakBaseAngle + off);
        }
        
        for (double ba : breakAngles) {
            double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(ba));
            gPrediction->determineShotResult(true, angle, 666.0, sharedGameManager.getShotSpin());
            
            if (gPrediction->guiData.balls[0].onTable) {
                bool eightBallSafe = true;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (gPrediction->guiData.balls[i].originalOnTable && 
                        !gPrediction->guiData.balls[i].onTable &&
                        gPrediction->guiData.balls[i].classification == Ball::Classification::EIGHT_BALL) {
                        eightBallSafe = false;
                        break;
                    }
                }
                
                if (eightBallSafe) {
                    int breakPocketedIdx = -1;
                    for (int bi = 1; bi < gPrediction->guiData.ballsCount; bi++) {
                        if (gPrediction->guiData.balls[bi].originalOnTable && !gPrediction->guiData.balls[bi].onTable &&
                            gPrediction->guiData.balls[bi].classification != Ball::Classification::EIGHT_BALL) {
                            breakPocketedIdx = bi;
                            break;
                        }
                    }
                    AutoPlay::g_CurrentCandidate.idx = (breakPocketedIdx != -1) ? breakPocketedIdx : 1;
                    AutoPlay::g_CurrentCandidate.angle = angle;
                    AutoPlay::g_CurrentCandidate.power = 666.0;
                    AutoPlay::g_CurrentCandidate.pocketIndex = (breakPocketedIdx != -1) ? gPrediction->guiData.balls[breakPocketedIdx].pocketIndex : 0;
                    AutoPlay::Shoot(angle, 666.0);
                    ResetBreakScan();
                    return;
                }
            }
        }
        
        double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(0.0));
        gPrediction->determineShotResult(true, angle, 666.0, sharedGameManager.getShotSpin());
        AutoPlay::g_CurrentCandidate.idx = 1;
        AutoPlay::g_CurrentCandidate.angle = angle;
        AutoPlay::g_CurrentCandidate.power = 666.0;
        AutoPlay::g_CurrentCandidate.pocketIndex = 0;
        for (int bi = 1; bi < gPrediction->guiData.ballsCount; bi++) {
            if (gPrediction->guiData.balls[bi].originalOnTable && !gPrediction->guiData.balls[bi].onTable) {
                AutoPlay::g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[bi].pocketIndex;
                break;
            }
        }
        AutoPlay::Shoot(angle, 666.0);
        AutoPlay::lastFailedCuePos = cueBall.initialPosition;
        ResetBreakScan();
        return;
    }

    if (breakScanAngle >= maxAngle && !breakBestCandidates.empty()) {
        std::sort(breakBestCandidates.begin(), breakBestCandidates.end(),
            [](const BreakCandidateData& a, const BreakCandidateData& b) { return a.score < b.score; });

        int refineCount = std::min((int)breakBestCandidates.size(), 5);
        BreakCandidateData overallBest = breakBestCandidates[0];

        // Level 1: Medium refinement
        for (int r = 0; r < refineCount; r++) {
            double centerAngle = breakBestCandidates[r].angle;
            for (double offset = -0.05; offset <= 0.05; offset += 0.005) {
                double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(centerAngle + offset));
                gPrediction->determineShotResult(true, testAngle, 666.0, sharedGameManager.getShotSpin());

                if (!gPrediction->guiData.balls[0].onTable) continue;

                int pocketed = 0;
                bool eightBallPocketed = false;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        if (ball.classification == Ball::Classification::EIGHT_BALL) {
                            eightBallPocketed = true;
                        } else if (ball.classification != Ball::Classification::CUE_BALL) {
                            pocketed++;
                        }
                    }
                }

                if (pocketed > 0 && !eightBallPocketed) {
                    double score = -((double)pocketed * 1000.0);
                    if (score < overallBest.score) {
                        overallBest = {testAngle, 666.0, pocketed, false, score};
                    }
                }
            }
        }

        // Level 2: Fine refinement
        {
            double centerAngle2 = overallBest.angle;
            for (double offset = -0.01; offset <= 0.01; offset += 0.001) {
                double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(centerAngle2 + offset));
                gPrediction->determineShotResult(true, testAngle, 666.0, sharedGameManager.getShotSpin());

                if (!gPrediction->guiData.balls[0].onTable) continue;

                int pocketed = 0;
                bool eightBallPocketed = false;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        if (ball.classification == Ball::Classification::EIGHT_BALL) {
                            eightBallPocketed = true;
                        } else if (ball.classification != Ball::Classification::CUE_BALL) {
                            pocketed++;
                        }
                    }
                }

                if (pocketed > 0 && !eightBallPocketed) {
                    double score = -((double)pocketed * 1000.0);
                    if (score < overallBest.score) {
                        overallBest = {testAngle, 666.0, pocketed, false, score};
                    }
                }
            }
        }

        // Level 3: Ultra-fine refinement
        {
            double centerAngle3 = overallBest.angle;
            for (double offset = -0.002; offset <= 0.002; offset += 0.0002) {
                double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(centerAngle3 + offset));
                gPrediction->determineShotResult(true, testAngle, 666.0, sharedGameManager.getShotSpin());

                if (!gPrediction->guiData.balls[0].onTable) continue;

                int pocketed = 0;
                bool eightBallPocketed = false;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) {
                        if (ball.classification == Ball::Classification::EIGHT_BALL) {
                            eightBallPocketed = true;
                        } else if (ball.classification != Ball::Classification::CUE_BALL) {
                            pocketed++;
                        }
                    }
                }

                if (pocketed > 0 && !eightBallPocketed) {
                    double score = -((double)pocketed * 1000.0);
                    if (score < overallBest.score) {
                        overallBest = {testAngle, 666.0, pocketed, false, score};
                    }
                }
            }
        }

        // Final verification and shoot
        gPrediction->determineShotResult(true, overallBest.angle, overallBest.power, sharedGameManager.getShotSpin());

        if (overallBest.pocketedCount > 0 && gPrediction->guiData.balls[0].onTable) {
            int firstPocketed = -1;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                    if (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) {
                        firstPocketed = i;
                        break;
                    }
                }
            }

            if (firstPocketed != -1) {
                AutoPlay::g_CurrentCandidate.idx = firstPocketed;
                AutoPlay::g_CurrentCandidate.angle = overallBest.angle;
                AutoPlay::g_CurrentCandidate.power = overallBest.power;
                AutoPlay::g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[firstPocketed].pocketIndex;
                AutoPlay::Shoot(overallBest.angle, overallBest.power);
            }
        }

        ResetBreakScan();
    }
}
void AutoPlay::OpenPowerHandle() {
    ImVec2 ballScreenPos = WorldToScreen(gPrediction->guiData.balls[0].initialPosition);

    float spinX = ballScreenPos.x;
    float spinY = ballScreenPos.y;
    float radius = 55.0f;

    if (bAutoSpin) {
        switch (spinPreset) {
            case SPIN_TOP:    spinY = ballScreenPos.y - radius; break;
            case SPIN_BOTTOM: spinY = ballScreenPos.y + radius; break;
            case SPIN_LEFT:   spinX = ballScreenPos.x - radius; break;
            case SPIN_RIGHT:  spinX = ballScreenPos.x + radius; break;
            case SPIN_CENTER: break;
        }
    } else {
        return;
    }

    // إصبع 6 منفصل عن الجويستيك (5) — هذا هو الإصلاح الأساسي
    NativeTouchesBegin(6, ballScreenPos.x, ballScreenPos.y);
    std::this_thread::sleep_for(std::chrono::milliseconds(120));
    NativeTouchesMove(6, spinX, spinY);
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    NativeTouchesEnd(6, spinX, spinY);
}
// ═══════════════════════════════════════════════════════════════
// ── ScanSlow ──
// ═══════════════════════════════════════════════════════════════
void AutoPlay::ScanSlow(double angleStep) {
    if (g_CurrentCandidate.idx != -1) { g_autoPlayCalculating = false; return; }

    bShowAutoPlayLines = !persistent_bool[O("bDisableFlicker")];

    Point2D currentCuePos = gPrediction->guiData.balls[0].initialPosition;
    double distSq = (currentCuePos - lastScanSlowCuePos).square();

    if (!isScanningInProgress || distSq > 0.0025) {
        currentScanAngle = 0.0; 
        isScanningInProgress = true; 
        lastScanSlowCuePos = currentCuePos;
    }

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    if ((myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) && AllGroupBallsPocketed()) {
        myclass = Ball::Classification::EIGHT_BALL;
    }
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);
    
    int steps = 0;
    bool foundShot = false;
    
    while (steps < 30 && currentScanAngle < maxAngle) {
        double angle = currentScanAngle;
        currentScanAngle += angleStep;
        steps++;

        std::vector<double> powers;
        {
            double estDist = 100.0; 
            auto& cueBall = gPrediction->guiData.balls[0];
            for (int bi = 1; bi < gPrediction->guiData.ballsCount; bi++) {
                if (!gPrediction->guiData.balls[bi].originalOnTable) continue;
                double d = sqrt((cueBall.initialPosition - gPrediction->guiData.balls[bi].initialPosition).square());
                if (d < estDist) estDist = d;
            }
            double basePow = CalculateRequiredPower(estDist);
            double multipliers[] = {0.70, 0.85, 0.95, 1.0, 1.05, 1.15, 1.30, 0.50, 1.50};
            for (double m : multipliers) {
                double p = basePow * m;
                if (p < 100.0) p = 100.0;
                if (p > 666.0) p = 666.0;
                powers.push_back(p);
            }
        }
        for (double power : powers) {
            gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
            
            if (!gPrediction->guiData.balls[0].onTable) continue;

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

            auto validation = ValidateShotSimulation(myclass, nominatedPocket, false, -1);
            if (validation.valid) {
                g_CurrentCandidate.idx = validation.targetIdx;
                g_CurrentCandidate.angle = angle;
                g_CurrentCandidate.power = power;
                g_CurrentCandidate.pocketIndex = validation.pocketIndex;

                foundShot = true;
                Shoot(angle, power);
                break;
            }
        }
        if (foundShot) break;
    }

    if (!foundShot && currentScanAngle >= maxAngle) {
        isScanningInProgress = false;
        currentScanAngle = 0.0;
        AutoPlay::state = IDLE;
    }
}

// ═══════════════════════════════════════════════════════════════
// ── ScanFast ──
// ═══════════════════════════════════════════════════════════════
void AutoPlay::ScanFast(double angleStep) {
    if (g_CurrentCandidate.idx != -1) return;
    
    bShowAutoPlayLines = !persistent_bool[O("bDisableFlicker")];

    auto& cueBall = gPrediction->guiData.balls[0];
    double distSq = (cueBall.initialPosition - fs.scanCuePos).square();
    
    if (!fs.isInitiated || distSq > 0.0025) {
        fs.raw.clear();
        fs.evals.clear();
        fs.evalIndex = 0;
        fs.scanCuePos = cueBall.initialPosition;
        fs.isInitiated = true;

        if (automationSpeed == SPEED_HUMAN && humanShotLocked)
            return;
        
        if (currentMode == MODE_AUTO_AIM && bAimedThisTurn) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if ((myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) && AllGroupBallsPocketed()) {
            myclass = Ball::Classification::EIGHT_BALL;
        }
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);
        bool isBreak = IsBreakShot();
        bool canTargetEight = (myclass == Ball::Classification::EIGHT_BALL);

        auto pockets = getPockets();

        bool bFoundLowestNumberedBall = false;
        int iFoundLowestNumberedBall = -1;

        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (isNineBallGame && bFoundLowestNumberedBall) break;
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;
            if (!bFoundLowestNumberedBall) {
                bFoundLowestNumberedBall = true;
                iFoundLowestNumberedBall = i;
            }

            if (!isNineBallGame) {
                bool isCandidate = false;
                if (canTargetEight && ball.classification == Ball::Classification::EIGHT_BALL) {
                    isCandidate = true;
                } else if (myclass == Ball::Classification::ANY) {
                    isCandidate = (ball.classification != Ball::Classification::EIGHT_BALL);
                } else if (myclass == Ball::Classification::NINE_BALL_RULE) {
                    isCandidate = true;
                } else {
                    isCandidate = (ball.classification == myclass);
                }
                if (!isCandidate) continue;
            }

            for (int pocketIdx = 0; pocketIdx < (int)pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;
                
                Point2D pocket = pockets[pocketIdx];
                Point2D toPocket = pocket - ball.initialPosition;
                double distTargetToPocket = sqrt(toPocket.square());
                if (distTargetToPocket < 0.1) continue;
                
                Point2D direction = toPocket * (1.0 / distTargetToPocket);
                Point2D ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);
                
                {
                    Point2D shotDir = ghostBallPos - cueBall.initialPosition;
                    double shotLenSq = shotDir.square();
                    if (shotLenSq > 0.01) {
                        bool obstructed = false;
                        for (int k = 1; k < gPrediction->guiData.ballsCount; k++) {
                            if (k == i) continue;
                            auto& obstBall = gPrediction->guiData.balls[k];
                            if (!obstBall.originalOnTable) continue;
                            Point2D toObst = obstBall.initialPosition - cueBall.initialPosition;
                            double proj = (toObst.x * shotDir.x + toObst.y * shotDir.y) / shotLenSq;
                            if (proj < 0.05 || proj > 0.95) continue;
                            Point2D closest = cueBall.initialPosition + shotDir * proj;
                            double perpDistSq = (obstBall.initialPosition - closest).square();
                            double minClearance = (2.0 * BALL_RADIUS) * (2.0 * BALL_RADIUS);
                            if (perpDistSq < minClearance) {
                                obstructed = true;
                                break;
                            }
                        }
                        if (obstructed) continue;
                    }
                }

                Point2D shotLine = ghostBallPos - cueBall.initialPosition;
                double distCueToTarget = sqrt(shotLine.square());
                if (distCueToTarget < 0.1) continue;
                double angle = atan2(shotLine.y, shotLine.x);
                if (angle < 0) angle += 2 * M_PI;
                
                Point2D cueToBall = ball.initialPosition - cueBall.initialPosition;
                double distCueToBall = sqrt(cueToBall.square());
                double cutAngleCos = 0.0;
                if (distCueToBall > 0.1 && distCueToTarget > 0.1) {
                    cutAngleCos = (shotLine.x * cueToBall.x + shotLine.y * cueToBall.y) /
                                  (distCueToTarget * distCueToBall);
                    if (cutAngleCos > 1.0) cutAngleCos = 1.0;
                    if (cutAngleCos < -1.0) cutAngleCos = -1.0;
                }
                
                if (cutAngleCos < 0.342) continue;
                
                double cutAnglePenalty = (1.0 - cutAngleCos) * 50.0;
                
                double score = distCueToTarget + distTargetToPocket + cutAnglePenalty;
                constexpr double slidingDeceleration = 196.0;
                double power = sqrt(2.0 * slidingDeceleration * (distCueToTarget + distTargetToPocket));
                
                double powerShortfall = 0.0;
                if (power > 666.0) {
                    powerShortfall = (power - 666.0) * 2.0;
                    power = 666.0;
                }
                score += powerShortfall;

                bool isEightBall = (ball.classification == Ball::Classification::EIGHT_BALL);
                if (!isEightBall || canTargetEight) {
                    fs.raw.push_back({i, angle, score, pocketIdx, power, distCueToTarget + distTargetToPocket, false, cutAngleCos});
                }
                
                // Bank shots
                if (bCushionShot && distTargetToPocket >= 0.1) {
                    constexpr double TBL_HW = 130.8;
                    constexpr double TBL_HH = 72.0;
                    struct Cushion { Point2D normal; bool horizontal; double pos; };
                    Cushion cushions[] = {
                        { Point2D(0, -1), true,  -TBL_HH },
                        { Point2D(0,  1), true,   TBL_HH },
                        { Point2D(-1, 0), false, -TBL_HW },
                        { Point2D( 1, 0), false,  TBL_HW },
                    };

                    for (const auto& cushion : cushions) {
                        Point2D mirrorPocket;
                        if (cushion.horizontal) {
                            mirrorPocket.x = pocket.x;
                            mirrorPocket.y = 2.0 * cushion.pos - pocket.y;
                        } else {
                            mirrorPocket.x = 2.0 * cushion.pos - pocket.x;
                            mirrorPocket.y = pocket.y;
                        }

                        Point2D toMirrorPocket = mirrorPocket - ball.initialPosition;
                        double distTargetToMirror = sqrt(toMirrorPocket.square());
                        if (distTargetToMirror < 0.1) continue;

                        Point2D mirDir = toMirrorPocket * (1.0 / distTargetToMirror);
                        Point2D ghostBallPosBank = ball.initialPosition - mirDir * (2.0 * BALL_RADIUS);

                        Point2D cueToGhost = ghostBallPosBank - cueBall.initialPosition;
                        double cueToGhostLen = sqrt(cueToGhost.square());
                        if (cueToGhostLen < 0.1) continue;
                        
                        Point2D reflPoint;
                        bool validReflection = false;
                        if (cushion.horizontal) {
                            if (cueToGhost.y == 0.0) continue;
                            double t = (cushion.pos - cueBall.initialPosition.y) / cueToGhost.y;
                            if (t < 0.05 || t > 0.95) continue;
                            reflPoint.x = cueBall.initialPosition.x + cueToGhost.x * t;
                            reflPoint.y = cushion.pos;
                            if (reflPoint.x < -TBL_HW || reflPoint.x > TBL_HW) continue;
                            validReflection = true;
                        } else {
                            if (cueToGhost.x == 0.0) continue;
                            double t = (cushion.pos - cueBall.initialPosition.x) / cueToGhost.x;
                            if (t < 0.05 || t > 0.95) continue;
                            reflPoint.x = cushion.pos;
                            reflPoint.y = cueBall.initialPosition.y + cueToGhost.y * t;
                            if (reflPoint.y < -TBL_HH || reflPoint.y > TBL_HH) continue;
                            validReflection = true;
                        }
                        if (!validReflection) continue;

                        double distCueToRefl = sqrt((reflPoint - cueBall.initialPosition).square());
                        double bankAngle = atan2((reflPoint - cueBall.initialPosition).y, 
                                                (reflPoint - cueBall.initialPosition).x);
                        if (bankAngle < 0) bankAngle += 2 * M_PI;

                        double totalDist = distCueToRefl + sqrt((reflPoint - ball.initialPosition).square()) + distTargetToPocket;
                        double bankPower = sqrt(2.0 * slidingDeceleration * totalDist);
                        if (bankPower > 666.0) bankPower = 666.0;

                        double bankScore = totalDist + 80.0;
                        fs.raw.push_back({i, bankAngle, bankScore, pocketIdx, bankPower, totalDist, true, 0.0});
                    }
                }

                // Combo shots
                if (!isBreak && distTargetToPocket >= 0.1) {
                    for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                        if (j == i) continue;
                        auto& ballB = gPrediction->guiData.balls[j];
                        if (!ballB.originalOnTable) continue;

                        Point2D toPocketB = pocket - ballB.initialPosition;
                        double distBToPocket = sqrt(toPocketB.square());
                        if (distBToPocket < 0.1) continue;
                        
                        Point2D directionB = toPocketB * (1.0 / distBToPocket);
                        Point2D ghostB = ballB.initialPosition - directionB * (2.0 * BALL_RADIUS);
                        
                        Point2D aToGhostB = ghostB - ball.initialPosition;
                        double distAToGhostB = sqrt(aToGhostB.square());
                        if (distAToGhostB < 0.1) continue;
                        Point2D aToGhostBDir = aToGhostB * (1.0 / distAToGhostB);
                        Point2D ghostA = ball.initialPosition - aToGhostBDir * (2.0 * BALL_RADIUS);
                        
                        Point2D shotLineCombo = ghostA - cueBall.initialPosition;
                        double distCueToA = sqrt(shotLineCombo.square());
                        if (distCueToA < 0.1) continue;
                        
                        double comboAngle = atan2(shotLineCombo.y, shotLineCombo.x);
                        if (comboAngle < 0) comboAngle += 2 * M_PI;
                        
                        double totalDist = distCueToA + distAToGhostB + distBToPocket;
                        double comboPower = sqrt(2.0 * 196.0 * totalDist);
                        if (comboPower > 666.0) comboPower = 666.0;
                        
                        double comboScore = totalDist + 200.0;
                        fs.raw.push_back({i, comboAngle, comboScore, pocketIdx, comboPower, totalDist, true, 0.0});
                    }
                }
            }
        }
        
        std::sort(fs.raw.begin(), fs.raw.end());
        
        if (fs.raw.size() > 150) {
            fs.raw.resize(150);
        }
    }

    if (automationSpeed == SPEED_HUMAN && humanShotLocked) return;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

    struct ScoredCandidate {
        Candidate cand;
        double finalScore;
        int pocketedCount;
        int pocketIndex;
        int targetIdx;
    };
    std::vector<ScoredCandidate> scoredCandidates;

    int stepsInThisFrame = 0;
    const int maxStepsPerFrame = 60;

    while (stepsInThisFrame < maxStepsPerFrame && fs.evalIndex < fs.raw.size()) {
        auto raw = fs.raw[fs.evalIndex++];
        stepsInThisFrame++;

        double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(raw.angle));
        gPrediction->determineShotResult(true, angle, raw.power, sharedGameManager.getShotSpin(), raw);
        
        if (!gPrediction->guiData.balls[0].onTable) continue;
        if (!gPrediction->firstHitIsTarget && !isNineBallGame) continue;

        if (isNineBallGame) {
            auto firstHit = gPrediction->guiData.collision.firstHitBall;
            if (!firstHit || firstHit->index != raw.idx) continue;

            int bestPottedIdx = -1;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) continue;
                    if (i == 9) { bestPottedIdx = 9; break; }
                    if (bestPottedIdx == -1 || i == raw.idx) bestPottedIdx = i;
                }
            }
            if (bestPottedIdx == -1) continue;
            
            int pocketed = CountPocketedBalls(myclass, nominatedPocket);
            double finalScore = -(pocketed * 10000.0) + raw.score;
            scoredCandidates.push_back({raw, finalScore, pocketed, gPrediction->guiData.balls[bestPottedIdx].pocketIndex, bestPottedIdx});
            continue;
        }

        if (gPrediction->guiData.balls[raw.idx].onTable) continue;

        if (!raw.isBandShot) {
            if (gPrediction->guiData.balls[raw.idx].pocketIndex != raw.pocketIndex) continue;
        }

        auto validation = ValidateShotSimulation(myclass, nominatedPocket, false, raw.idx);
        if (validation.valid) {
            double finalScore = -(validation.pocketedCount * 10000.0) + raw.score;
            scoredCandidates.push_back({raw, finalScore, validation.pocketedCount, validation.pocketIndex, validation.targetIdx});
        }
    }

    if (fs.evalIndex >= fs.raw.size()) {
        if (scoredCandidates.empty()) {
            int rescueCount = std::min(10, (int)fs.raw.size());
            for (int ci = 0; ci < rescueCount && scoredCandidates.empty(); ci++) {
                const auto& cand = fs.raw[ci];
                for (double offset = -0.06; offset <= 0.06; offset += 0.004) {
                    double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(cand.angle + offset));
                    double basePowers[] = {cand.power * 0.85, cand.power * 0.93, cand.power, cand.power * 1.07, cand.power * 1.15};
                    for (int pi = 0; pi < 5; pi++) {
                        double testPower = basePowers[pi];
                        if (testPower < 100.0) testPower = 100.0;
                        if (testPower > 666.0) testPower = 666.0;

                        gPrediction->determineShotResult(true, testAngle, testPower, sharedGameManager.getShotSpin(), cand);
                        if (!gPrediction->guiData.balls[0].onTable) continue;

                        auto validation = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, cand.idx);
                        if (validation.valid) {
                            double finalScore = -(validation.pocketedCount * 10000.0) + cand.score;
                            Candidate rescueCand = cand;
                            rescueCand.angle = testAngle;
                            rescueCand.power = testPower;
                            scoredCandidates.push_back({rescueCand, finalScore, validation.pocketedCount, validation.pocketIndex, validation.targetIdx});
                        }
                    }
                    if (!scoredCandidates.empty()) break;
                }
            }
        }

        std::sort(scoredCandidates.begin(), scoredCandidates.end(), 
            [](const ScoredCandidate& a, const ScoredCandidate& b) {
                if (a.finalScore != b.finalScore) return a.finalScore < b.finalScore;
                if (a.pocketedCount != b.pocketedCount) return a.pocketedCount > b.pocketedCount;
                return a.cand.cutAngleCos > b.cand.cutAngleCos;
            });

        if (!scoredCandidates.empty()) {
            const auto& best = scoredCandidates[0];
            double bestAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(best.cand.angle));

            Candidate fineCand = best.cand;
            double fineTunedAngle = bestAngle;
            double bestFineScore = best.finalScore;

            // Level 1: Wide scan
            {
                constexpr double L1_STEP = 0.004;
                constexpr double L1_RANGE = 0.04;
                double l1Powers[] = {fineCand.power * 0.80, fineCand.power * 0.90, fineCand.power,
                                     fineCand.power * 1.10, fineCand.power * 1.20};
                for (double offset = -L1_RANGE; offset <= L1_RANGE; offset += L1_STEP) {
                    double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(bestAngle + offset));
                    for (double rawP : l1Powers) {
                        double testPower = rawP;
                        if (testPower < 50.0) testPower = 50.0;
                        if (testPower > 666.0) testPower = 666.0;

                        gPrediction->determineShotResult(true, testAngle, testPower, sharedGameManager.getShotSpin(), fineCand);
                        if (!gPrediction->firstHitIsTarget && !isNineBallGame) continue;
                        if (!gPrediction->guiData.balls[0].onTable) continue;

                        auto validation = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, fineCand.idx);
                        if (validation.valid) {
                            double testScore = -(validation.pocketedCount * 10000.0) + fineCand.score;
                            if (testScore < bestFineScore) {
                                bestFineScore = testScore;
                                fineTunedAngle = testAngle;
                                fineCand = best.cand;
                                fineCand.angle = testAngle;
                                fineCand.power = testPower;
                                fineCand.pocketIndex = validation.pocketIndex;
                                fineCand.idx = validation.targetIdx;
                            }
                        }
                    }
                }
            }

            // Level 2: Medium scan
            {
                constexpr double L2_STEP = 0.001;
                constexpr double L2_RANGE = 0.008;
                double l2Powers[] = {fineCand.power * 0.92, fineCand.power, fineCand.power * 1.08};
                for (double offset = -L2_RANGE; offset <= L2_RANGE; offset += L2_STEP) {
                    double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(fineTunedAngle + offset));
                    for (double rawP : l2Powers) {
                        double testPower = rawP;
                        if (testPower < 50.0) testPower = 50.0;
                        if (testPower > 666.0) testPower = 666.0;

                        gPrediction->determineShotResult(true, testAngle, testPower, sharedGameManager.getShotSpin(), fineCand);
                        if (!gPrediction->firstHitIsTarget && !isNineBallGame) continue;
                        if (!gPrediction->guiData.balls[0].onTable) continue;

                        auto validation = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, fineCand.idx);
                        if (validation.valid) {
                            double testScore = -(validation.pocketedCount * 10000.0) + fineCand.score;
                            if (testScore < bestFineScore) {
                                bestFineScore = testScore;
                                fineTunedAngle = testAngle;
                                fineCand.power = testPower;
                                fineCand.pocketIndex = validation.pocketIndex;
                                fineCand.idx = validation.targetIdx;
                            }
                        }
                    }
                }
            }

            // Level 3: Ultra-precise scan
            {
                constexpr double L3_STEP = 0.0002;
                constexpr double L3_RANGE = 0.0015;
                double l3Powers[] = {fineCand.power * 0.95, fineCand.power, fineCand.power * 1.05};
                for (double offset = -L3_RANGE; offset <= L3_RANGE; offset += L3_STEP) {
                    double testAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(fineTunedAngle + offset));
                    for (double rawP : l3Powers) {
                        double testPower = rawP;
                        if (testPower < 50.0) testPower = 50.0;
                        if (testPower > 666.0) testPower = 666.0;

                        gPrediction->determineShotResult(true, testAngle, testPower, sharedGameManager.getShotSpin(), fineCand);
                        if (!gPrediction->firstHitIsTarget && !isNineBallGame) continue;
                        if (!gPrediction->guiData.balls[0].onTable) continue;

                        auto validation = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, fineCand.idx);
                        if (validation.valid) {
                            double testScore = -(validation.pocketedCount * 10000.0) + fineCand.score;
                            if (testScore < bestFineScore) {
                                bestFineScore = testScore;
                                fineTunedAngle = testAngle;
                                fineCand.power = testPower;
                                fineCand.pocketIndex = validation.pocketIndex;
                                fineCand.idx = validation.targetIdx;
                            }
                        }
                    }
                }
            }

            // Final verification
            double finalAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(fineTunedAngle));
            gPrediction->determineShotResult(true, finalAngle, fineCand.power, sharedGameManager.getShotSpin(), fineCand);
            
            auto finalValidation = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, fineCand.idx);
            
            if (finalValidation.valid) {
                g_CurrentCandidate = fineCand;
                g_CurrentCandidate.angle = finalAngle;
                g_CurrentCandidate.idx = finalValidation.targetIdx;
                g_CurrentCandidate.pocketIndex = finalValidation.pocketIndex;
                g_CurrentCandidate.power = fineCand.power;
                Shoot(finalAngle, fineCand.power);
            } else {
                for (const auto& sc : scoredCandidates) {
                    double scAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(sc.cand.angle));
                    gPrediction->determineShotResult(true, scAngle, sc.cand.power, sharedGameManager.getShotSpin(), sc.cand);
                    auto v = ValidateShotSimulation(myclass, nominatedPocket, isNineBallGame, sc.targetIdx);
                    if (v.valid) {
                        g_CurrentCandidate = sc.cand;
                        g_CurrentCandidate.angle = scAngle;
                        g_CurrentCandidate.idx = v.targetIdx;
                        g_CurrentCandidate.pocketIndex = v.pocketIndex;
                        g_CurrentCandidate.power = sc.cand.power;
                        Shoot(scAngle, sc.cand.power);
                        break;
                    }
                }
            }
        } else {
            lastFailedCuePos = cueBall.initialPosition;
            scan = SLOW;
        }
    }
}

void AutoPlay::Shoot(double angle, double power) {
    // أولاً: تطبيق الإسبن المطلوب إذا كان مفعلاً
    if (bAutoSpin) {
        applyAutoSpin();
    }
    
    angle = NumberUtils::normalizeDoublePrecision(angle);
    power = NumberUtils::normalizeDoublePrecision(power);

    gPrediction->forceFullSimulation = true;
    gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin(), g_CurrentCandidate);
    gPrediction->forceFullSimulation = false;

    bool nominating = false;
    int nominationMode = sharedGameManager.getPocketNominationMode();
    auto myclass = sharedGameManager.getPlayerClassification();
    if ((myclass == Ball::Classification::SOLID || myclass == Ball::Classification::STRIPE) && AllGroupBallsPocketed()) {
        myclass = Ball::Classification::EIGHT_BALL;
    }
    
    pendingShotPower = power;
    pendingShotAngle = angle;
    
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
        humanNeedsNomination = (automationSpeed == SPEED_HUMAN && playStyle != STYLE_INSTANT);
        return; 
    }

    // AUTO AIM MODE
    if (currentMode == MODE_AUTO_AIM) {
        if (playStyle == STYLE_INSTANT) {
            if (bAutoSpin) OpenPowerHandle();

            setAimAngle(angle);
            setPower(power);
            bAimedThisTurn = true;
            lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
            g_postAimLock = true;
            g_postAimAngle = angle;
            g_postAimPower = power;
            g_postAimFrames = 20; 
            ClearState();
            state = IDLE;
        } else {
            humanShotLocked = false;
            state = EXECUTING;
            humanState = HUM_THINKING;
            stateStartTime = nowSec() + 0.3;
            startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
            targetAngle = angle;
            pendingShotPower = power;
            bAimedThisTurn = true;
            lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
        }
        return;
    }

    // AUTO PLAY MODE
    if (automationSpeed == SPEED_HUMAN || automationSpeed == SPEED_FAST) {
        startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        targetAngle = angle;
        pendingShotPower = power;
        
        g_PredictionLocked = true;
        bShowAutoPlayLines = false; 

        gPrediction->forceFullSimulation = true;
        gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin(), g_CurrentCandidate);
        gPrediction->forceFullSimulation = false;

        if (playStyle == STYLE_INSTANT) {
            if (bAutoSpin) OpenPowerHandle();

            takeShot(angle, power);
            state = EXECUTING;
        } else if (automationSpeed == SPEED_HUMAN) {
            humanShotLocked = true;
            state = EXECUTING;
            humanState = HUM_THINKING;
            stateStartTime = nowSec() + 0.5;
        } else {
            takeShot(angle, power);
            state = EXECUTING; 
        }
        return;
    }
}

void AutoPlay::AutoPlaceCueBall() {
    if (!gPrediction || gPrediction->guiData.balls[0].onTable) return;

    auto& cueBall = gPrediction->guiData.balls[0];
    auto myclass = sharedGameManager.getPlayerClassification();
    auto pockets = getPockets();

    double bestScore = -1e18;
    Point2D bestPosition = { 0.0, 0.0 };
    bool foundPerfect = false;

    // --- 1. حساب المكان الذكي ---
    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (!ball.originalOnTable) continue;

        if (myclass == Ball::Classification::SOLID && ball.classification == Ball::Classification::STRIPE) continue;
        if (myclass == Ball::Classification::STRIPE && ball.classification == Ball::Classification::SOLID) continue;
        if (myclass != Ball::Classification::EIGHT_BALL && ball.classification == Ball::Classification::EIGHT_BALL) continue;

        for (int p = 0; p < pockets.size(); p++) {
            Point2D pocket = pockets[p];
            
            Point2D dirToPocket = pocket - ball.initialPosition;
            double distToPocket = sqrt(dirToPocket.x * dirToPocket.x + dirToPocket.y * dirToPocket.y);
            if (distToPocket < 0.001) continue;
            dirToPocket.x /= distToPocket;
            dirToPocket.y /= distToPocket;

            double distances[] = { BALL_RADIUS * 5.0, BALL_RADIUS * 10.0, BALL_RADIUS * 18.0 };
            
            for (double d : distances) {
                Point2D suggestedPos = ball.initialPosition - dirToPocket * d;
                
                if (suggestedPos.x < -TABLE_HALF_WIDTH + 5.0 || suggestedPos.x > TABLE_HALF_WIDTH - 5.0) continue;
                if (suggestedPos.y < -TABLE_HALF_HEIGHT + 5.0 || suggestedPos.y > TABLE_HALF_HEIGHT - 5.0) continue;

                Point2D diff = suggestedPos - ball.initialPosition;
                double distance = sqrt(diff.x * diff.x + diff.y * diff.y);
                
                double distanceScore = 0.0;
                if (distance >= 30.0 && distance <= 60.0) {
                    distanceScore = 100.0;
                } else {
                    distanceScore = 50.0 - (std::abs(distance - 45.0));
                }

                Point2D cueToBallDir = ball.initialPosition - suggestedPos;
                double distCueToBall = sqrt(cueToBallDir.x * cueToBallDir.x + cueToBallDir.y * cueToBallDir.y);
                if (distCueToBall < 0.001) continue;
                cueToBallDir.x /= distCueToBall;
                cueToBallDir.y /= distCueToBall;
                
                double angleScore = (cueToBallDir.x * dirToPocket.x + cueToBallDir.y * dirToPocket.y);
                double totalScore = distanceScore + (angleScore * 50.0);

                if (totalScore > bestScore) {
                    bestScore = totalScore;
                    bestPosition = suggestedPos;
                    foundPerfect = true;
                }
            }
        }
    }

    if (!foundPerfect) {
        float randX = (rand() % 100) / 100.0f * 80.0f - 40.0f;
        float randY = (rand() % 100) / 100.0f * 30.0f + 10.0f;
        bestPosition = { randX, randY };
    }

    // --- 2. تحويل المكان إلى إحداثيات الشاشة ---
    ImVec2 targetScreenPos = WorldToScreen(bestPosition);
    
    // الحصول على مكان الكرة الحالي على الشاشة (نقطة البداية)
    ImVec2 currentScreenPos = WorldToScreen(gPrediction->guiData.balls[0].initialPosition);

    // --- 3. محاكاة السحب البطيء (الحركة البشرية) ---
    NativeTouchesBegin(5, currentScreenPos.x, currentScreenPos.y);
    
    // عدد الخطوات (كلما زاد الرقم، كانت الحركة أبطأ وأكثر سلاسة)
    const int steps = 30; 
    for (int step = 1; step <= steps; step++) {
        // حساب النقطة الحالية بناءً على التقدم (الاستيفاء الخطي)
        float t = (float)step / steps;
        float currentX = currentScreenPos.x + (targetScreenPos.x - currentScreenPos.x) * t;
        float currentY = currentScreenPos.y + (targetScreenPos.y - currentScreenPos.y) * t;
        
        NativeTouchesMove(5, currentX, currentY);
        
        // تأخير بسيط جداً بين كل خطوة (محاكاة سرعة الإصبع البشري)
        // 5 مللي ثانية = حركة سريعة طبيعية. إذا أردت أبطأ، زد الرقم.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    // إفلات الكرة في المكان النهائي
    NativeTouchesEnd(5, targetScreenPos.x, targetScreenPos.y);

    gPrediction->guiData.balls[0].onTable = true;
}

void AutoPlay::Update() {
    frameCounter++;
    buttonClicker.Update();
    powerSlider.Update();

    // Track cue ball movement
    static Point2D lastFrameCuePos = {-1000.0, -1000.0};
    static int framesCueBallStill = 10;
    Point2D currentCuePos = {0.0, 0.0};
    bool hasCueBall = false;
    if (sharedGameManager) {
        Table table = sharedGameManager.mTable;
        if (table) {
            auto& balls = table.mBalls();
            if (balls && balls.Count > 0) {
                currentCuePos = balls[0].position();
                hasCueBall = true;
            }
        }
    }
    if (hasCueBall) {
        if (lastFrameCuePos.x == -1000.0) {
            lastFrameCuePos = currentCuePos;
        }
        double dx = currentCuePos.x - lastFrameCuePos.x;
        double dy = currentCuePos.y - lastFrameCuePos.y;
        double distSq = dx * dx + dy * dy;
        if (distSq > 0.0001) {
            framesCueBallStill = 0;
        } else {
            if (framesCueBallStill < 10) framesCueBallStill++;
        }
        lastFrameCuePos = currentCuePos;
    } else {
        framesCueBallStill = 10;
        lastFrameCuePos = {-1000.0, -1000.0};
    }
    bCueBallIsMovingOrDragging = (framesCueBallStill < 5);

    if (g_postShotLock) {
        if (g_postShotFrames > 0 && sharedGameManager) {
            setAimAngle(g_postShotAngle);
            setPower(g_postShotPower);
            g_postShotFrames--;
        } else {
            g_postShotLock = false;
            ClearState();
        }
        g_autoPlayCalculating = false;
        return;
    }

    if (g_postAimLock) {
        if (g_postAimFrames > 0 && sharedGameManager) {
            setAimAngle(g_postAimAngle);
            setPower(g_postAimPower);
            g_postAimFrames--;
        } else {
            g_postAimLock = false;
            ClearState();
        }
        g_autoPlayCalculating = false;
        return;
    }

    bool humanRunning = (automationSpeed == SPEED_HUMAN && (humanState != HUM_IDLE || humanShotLocked));
    bool executingShot = anim_IsPulling || humanRunning;

    if (AreBallsMoving() && !executingShot) {
        if (state == SCANNING || state == NOMINATING) {
            ClearState();
            state = IDLE;
        }
        g_autoPlayCalculating = false;
        return;
    }

    // --- ANIMATION (FAST MODE) ---
    if (anim_IsPulling) {
        float jX = Width * 0.83f; 
        float jY = Height * 0.82f; 
        float jR = 65.0f;

        double now_anim = nowSec();
        double elapsed = now_anim - stateStartTime;

        const double t1_pullback = 0.30;
        const double t2_sweep    = 1.00;
        const double t3_correct  = 1.40;
        const double t4_adjust   = 1.90;
        const double t5_hold     = 2.20;

        if (fastShotState == 0) {
            if (playStyle == STYLE_INSTANT) {
                setAimAngle(anim_TargetAngle);
                NativeTouchesBegin(5, jX, jY);
                NativeTouchesMove(5, jX + (float)cos(anim_TargetAngle) * jR, 
                                     jY + (float)sin(anim_TargetAngle) * jR);
                anim_RotationDone = true;
                anim_TouchStarted = true;
                stateStartTime = nowSec();
                fastShotState = 1;
                return;
            }

            double normalizedStart = normalizeAngle(startAngle);
            double normalizedTarget = normalizeAngle(anim_TargetAngle);
            double delta = normalizedTarget - normalizedStart;
            if (delta > M_PI)  delta -= 2.0 * M_PI;
            if (delta < -M_PI) delta += 2.0 * M_PI;

            double dir = (delta > 0) ? 1.0 : -1.0;
            double oppositeAngle  = normalizedStart - dir * (15.0 * M_PI / 180.0);
            double overshootAngle = normalizedTarget + dir * (10.0 * M_PI / 180.0);
            double nudgeAngle     = normalizedTarget - dir * (0.8 * M_PI / 180.0);

            double curAngle = normalizedTarget;

            if (elapsed < t1_pullback) {
                double t = elapsed / t1_pullback;
                t = EaseInOutCubic(t); 
                curAngle = normalizedStart + (oppositeAngle - normalizedStart) * t;
                if (!anim_TouchStarted) {
                    anim_TouchStarted = true;
                    NativeTouchesBegin(5, jX, jY);
                }
            } else if (elapsed < t2_sweep) {
                double t = (elapsed - t1_pullback) / (t2_sweep - t1_pullback);
                t = t * t * (3.0 - 2.0 * t);
                curAngle = oppositeAngle + (overshootAngle - oppositeAngle) * t;
            } else if (elapsed < t3_correct) {
                double t = (elapsed - t2_sweep) / (t3_correct - t2_sweep);
                t = t * t * (3.0 - 2.0 * t);
                curAngle = overshootAngle + (nudgeAngle - overshootAngle) * t;
            } else if (elapsed < t4_adjust) {
                double t = (elapsed - t3_correct) / (t4_adjust - t3_correct);
                t = sin(t * M_PI_2);
                curAngle = nudgeAngle + (normalizedTarget - nudgeAngle) * t;
            } else if (elapsed < t5_hold) {
                curAngle = normalizedTarget;
                if (!anim_RotationDone) {
                    if (elapsed > t5_hold - 0.05) {
                        anim_RotationDone = true;
                        setAimAngle(anim_TargetAngle);
                    }
                }
            }

            if (elapsed < t5_hold) {
                setAimAngle(curAngle);
                NativeTouchesMove(5, jX + (float)cos(curAngle) * jR, 
                                     jY + (float)sin(curAngle) * jR);
                return;
            }

            setAimAngle(anim_TargetAngle);
            NativeTouchesMove(5, jX + (float)cos(anim_TargetAngle) * jR, 
                                 jY + (float)sin(anim_TargetAngle) * jR);
            stateStartTime = nowSec();
            fastShotState = 1;
            return;
        }

        setAimAngle(anim_TargetAngle);
        double elapsed_shot = nowSec() - stateStartTime;

        if (fastShotState == 1) {
            NativeTouchesMove(5, jX + (float)cos(anim_TargetAngle) * jR, 
                                 jY + (float)sin(anim_TargetAngle) * jR);
            setAimAngle(anim_TargetAngle);

            bool shouldTriggerPower = false;
            if (playStyle == STYLE_INSTANT) {
                shouldTriggerPower = true;
            } else if (elapsed_shot >= 0.15) {
                shouldTriggerPower = true;
            }

            if (shouldTriggerPower) {
                setAimAngle(anim_TargetAngle);
                NativeTouchesEnd(5, jX + (float)cos(anim_TargetAngle) * jR, 
                                    jY + (float)sin(anim_TargetAngle) * jR);
                setAimAngle(anim_TargetAngle);

                float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
                float sliderX = Width * sliderXPercent;
                if (persistent_int[O("iPowerBarSide")] == 1) {
                    sliderX = Width * (1.0f - sliderXPercent);
                }
                float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
                float sliderYEnd = Height * persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sliderX - 20.0f, sliderYStart, 40.0f, sliderYEnd - sliderYStart);
                if (playStyle == STYLE_INSTANT) {
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.40f, 0.50f);
                } else {
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.85f, 0.50f);
                }

                stateStartTime = nowSec();
                fastShotState = 2;
            }
            return;
        }

        if (fastShotState == 2) {
            setAimAngle(anim_TargetAngle);
            if (powerSlider.Active) return;

            gPrediction->forceFullSimulation = true;
            gPrediction->determineShotResult(true, anim_TargetAngle, anim_TargetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gPrediction->forceFullSimulation = false;

            stateStartTime = nowSec();
            fastShotState = 3;
            return;
        }

        if (fastShotState == 3) {
            setAimAngle(anim_TargetAngle);

            static double s_ballsStoppedAt = -1.0;
            if (s_ballsStoppedAt < stateStartTime) s_ballsStoppedAt = stateStartTime;

            bool timedOut = (nowSec() - stateStartTime > 12.0);
            if (AreBallsMoving() && !timedOut) {
                s_ballsStoppedAt = nowSec();
                return;
            }

            double settledFor = nowSec() - s_ballsStoppedAt;
            if (settledFor < 0.5 && !timedOut) return;

            s_ballsStoppedAt = -1.0;
            anim_IsPulling = false;
            anim_RotationDone = false;
            anim_TouchStarted = false;
            fastShotState = 0;
            ClearState();
            state = IDLE;
            g_lastFastShotTime = nowSec();
            return;
        }
    }

    // Pocket visual
    if (persistent_bool.count(O("bPocketTargetVisual")) == 0 || persistent_bool[O("bPocketTargetVisual")]) {
        int nomPocket = sharedGameManager.getNominatedPocket();
        if (nomPocket >= 0 && nomPocket < 6) {
            ImVec2 pktPos = GetPocketScreenPos(nomPocket);
            ImDrawList* fg = ImGui::GetBackgroundDrawList();
            float pulse = (sin(ImGui::GetTime() * 8.0f) + 1.0f) * 0.5f;
            float r = 35.0f + (pulse * 8.0f);
            fg->AddCircleFilled(pktPos, r, IM_COL32(255, 120, 0, 70));
            fg->AddCircle(pktPos, r, IM_COL32(255, 200, 0, 255), 0, 3.5f);
            fg->AddLine(ImVec2(pktPos.x - 18, pktPos.y), ImVec2(pktPos.x + 18, pktPos.y), IM_COL32(255, 255, 255, 180), 2.5f);
            fg->AddLine(ImVec2(pktPos.x, pktPos.y - 18), ImVec2(pktPos.x, pktPos.y + 18), IM_COL32(255, 255, 255, 180), 2.5f);
        }
    }

    static bool wasPlayerTurn = false;
bool isPlayerTurn = sharedGameManager.mStateManager().isPlayerTurn();
if (isPlayerTurn && bAutoSpin) applyAutoSpin();

bool turnJustStarted = !wasPlayerTurn && isPlayerTurn;

// ============================================================
// ── فتح الإسبن في بداية أول دور فقط ──
// ============================================================
if (turnJustStarted) {
    static bool bFirstTurnOpened = false; // متغير يمنع التكرار في نفس اللعبة
    
    if (!bFirstTurnOpened) {
        // انتظر 800 مللي ثانية حتى تظهر العناصر
        std::this_thread::sleep_for(std::chrono::milliseconds(800)); 
        
        // افتح الإسبن
        AutoPlay::OpenPowerHandle(); 
        
        bFirstTurnOpened = true; // نمنع فتح الإسبن مرة أخرى في نفس اللعبة
    }
}
// ============================================================
// ============================================================

if (wasPlayerTurn && !isPlayerTurn) { g_autoPlayCalculating = false; ClearState(); bAimedThisTurn = false; }
if (turnJustStarted) { bAimedThisTurn = false; lastFailedCuePos = {-1000.0, -1000.0}; }
    wasPlayerTurn = isPlayerTurn;

    static double turnStartTime = 0.0;
    if (turnJustStarted || (isPlayerTurn && turnStartTime == 0.0)) turnStartTime = nowSec();
    if (!isPlayerTurn) turnStartTime = 0.0;

    humanRunning = (automationSpeed == SPEED_HUMAN && (humanState != HUM_IDLE || humanShotLocked));

    // Animation active check
    static int animationStuckCounter = 0;
    humanRunning = (automationSpeed == SPEED_HUMAN && (humanState != HUM_IDLE || humanShotLocked));
    if (IsAnimationActive() && !humanRunning && currentMode != MODE_AUTO_AIM) {
        animationStuckCounter++;
        if (animationStuckCounter < 200) { 
            g_autoPlayCalculating = false; return;
        }
    } else {
        animationStuckCounter = 0;
    }

    // Shot cooldown
    if (nowSec() - g_lastFastShotTime < 2.5) {
        g_autoPlayCalculating = false;
        return;
    }
    if (AutoPlay::nowSec() < g_shotCooldownEnd) {
        g_autoPlayCalculating = false;
        return;
    }

    // State timeout safety
    static double lastStateChangeTime = 0;
    static State lastState = IDLE;
    if (state != lastState) {
        lastState = state;
        lastStateChangeTime = AutoPlay::nowSec();
    } else if (state != IDLE && (AutoPlay::nowSec() - lastStateChangeTime > 10.0)) {
        ClearState();
        return;
    }

    if (turnJustStarted && bAutoPlaying) {
        state = IDLE;
        scan = FAST;
        currentScanAngle = 0.0;
    }

    // ============================================================
    // ── EXPERT HUMAN STATE MACHINE ──
    // ============================================================
    if (automationSpeed == SPEED_HUMAN && humanState != HUM_IDLE) {
        if (state == NOMINATING_HUMAN) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 15) buttonClicker.Click(GetPocketScreenPos(humanNominationPocket));
            if (nominationFrameCounter > 35 && !buttonClicker.Active) {
                humanState = HUM_THINKING; 
                stateStartTime = nowSec() + 0.2;
                state = EXECUTING; humanNeedsNomination = false;
            }
            return;
        }

        double now = nowSec();

        auto UpdateJoystickVisuals = [&](double angle) {
            float jX = Width * 0.83f;
            float jY = Height * 0.82f;
            float jR = 65.0f;
            float tX = jX + cos(angle) * jR;
            float tY = jY + sin(angle) * jR;
            NativeTouchesMove(5, tX, tY);
        };

        // EXPERT THINKING PHASE
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

        // EXPERT OVERSHOOTING PHASE
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

        // EXPERT CORRECTING PHASE
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

        // EXPERT HOLDING PHASE (subtle jitter)
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

        // EXPERT STABILIZING PHASE
        if (humanState == HUM_STABILIZING) {
            float jX = Width * 0.83f;
            float jY = Height * 0.82f;
            float jR = 65.0f;
            NativeTouchesMove(5, jX + (float)cos(targetAngle) * jR, 
                                 jY + (float)sin(targetAngle) * jR);
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.2) {
                if (currentMode == MODE_AUTO_PLAY) {
                    setAimAngle(targetAngle);
                    NativeTouchesEnd(5, jX + (float)cos(targetAngle) * jR, 
                                        jY + (float)sin(targetAngle) * jR);
                    setAimAngle(targetAngle);
                    stateStartTime = now;
                    startPower = getCurrentPower();
                    targetPower = pendingShotPower;
                    humanState = HUM_PULLING;
                } else {
                    NativeTouchesEnd(5, jX + (float)cos(targetAngle) * jR, 
                                        jY + (float)sin(targetAngle) * jR);
                    bAimedThisTurn = true;
                    lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
                    g_postAimLock = true;
                    g_postAimAngle = targetAngle;
                    g_postAimPower = pendingShotPower;
                    g_postAimFrames = 20;
                    state = IDLE; humanState = HUM_IDLE;
                }
            }
            return;
        }

        // EXPERT PULLING PHASE
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

        // EXPERT DELAY BEFORE SHOT
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

    // ABORT HANDLER
    if (!bAutoPlaying || !isPlayerTurn) {
        if (humanShotLocked || anim_IsPulling || state == SCANNING || state == NOMINATING) {
            if (humanState == HUM_OVERSHOOTING || humanState == HUM_CORRECTING || humanState == HUM_HOLDING || humanState == HUM_STABILIZING) {
                float jX = Width * 0.83f;
                float jY = Height * 0.82f;
                NativeTouchesEnd(5, jX, jY);
            }
            
            if (powerSlider.Active) {
                float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
                float sliderX = Width * sliderXPercent;
                if (persistent_int[O("iPowerBarSide")] == 1) {
                    sliderX = Width * (1.0f - sliderXPercent);
                }
                float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
                NativeTouchesEnd(powerSlider.TouchIndex, sliderX, sliderYStart);
                powerSlider.Active = false;
                powerSlider.state = PowerSlider::IDLE;
            }

            if (sharedGameManager) {
                double cur = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
                sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(cur);
            }

            gPrediction->forceFullSimulation = false;
            humanShotLocked = false;
            anim_IsPulling = false;
            fastShotState = 0;
            humanState = HUM_IDLE;
            ClearState();
            state = IDLE;
            g_autoPlayCalculating = false;
        }
        g_autoPlayCalculating = false;
        return; 
    }

    if (currentMode == MODE_AUTO_AIM && bAimedThisTurn && sharedGameManager) {
        auto& cueBall = gPrediction->guiData.balls[0];
        double distSq2 = (cueBall.initialPosition - lastCuePosWhenAimed).square();
        if (distSq2 > 0.0025) {
            bAimedThisTurn = false;
            lastFailedCuePos = {-1000.0, -1000.0};
            state = IDLE;
        }
    }

    if (state == IDLE) {
    // === إضافة: فتح وتحريك الدائرة قبل كل ضربة (سريعة) ===
    if (gPrediction && gPrediction->guiData.balls[0].onTable) {
        // انتظر 250 مللي ثانية فقط لتكون سريعة
        std::this_thread::sleep_for(std::chrono::milliseconds(250)); 
        AutoPlay::OpenPowerHandle(); // تفتح وتحرك وتغلق وتضرب

        // === هذه الإضافة الجديدة: نوقف الهاك عن البحث لمدة ثانية ===
        // حتى ينتهي من تحريك الإسبن بشكل كامل
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        // ===============================================
    }
    // =======================================================

    // === إضافة: وضع الكرة إذا كانت خارج الطاولة ===
    if (gPrediction && !gPrediction->guiData.balls[0].onTable) {
        AutoPlaceCueBall();
        return;
    }
    // =======================================================

    bool shouldScan = (currentMode != MODE_AUTO_AIM) || !bAimedThisTurn;
    if (shouldScan) {
        state = SCANNING;
        scan = FAST;
        g_autoPlayCalculating = false;
    }
}

    if (state == SCANNING) {
        if (IsBreakShot()) {
            ScanBreakShot();
            return;
        }
        if (scan == FAST) ScanFast();
        if (scan == SLOW) {
            g_autoPlayCalculating = true;
            float level = persistent_float.count(O("fScannerLevel")) ? persistent_float[O("fScannerLevel")] : 50.0f;
            double step = 0.005 + (double(level) / 100.0) * 0.035;
            ScanSlow(step);
        }
    }

    if (state == NOMINATING) {
        setAimAngle(pendingShotAngle);
        nominationFrameCounter++;
        if (nominationFrameCounter == 15) {
            buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
        }
        if (nominationFrameCounter > 20 && !buttonClicker.Active) {
            uint nominatedPocket = sharedGameManager.getNominatedPocket();
            if (nominatedPocket == g_CurrentCandidate.pocketIndex) {
                targetAngle = pendingShotAngle;
                g_PredictionLocked = true;
                
                gPrediction->forceFullSimulation = true;
                gPrediction->determineShotResult(true, pendingShotAngle, pendingShotPower,
                                                sharedGameManager.getShotSpin(), g_CurrentCandidate);
                gPrediction->forceFullSimulation = false;
                if (g_CurrentCandidate.idx >= 0 && g_CurrentCandidate.idx < gPrediction->guiData.ballsCount) {
                    int freshPocket = gPrediction->guiData.balls[g_CurrentCandidate.idx].pocketIndex;
                    if (freshPocket >= 0 && freshPocket < 6) {
                        g_CurrentCandidate.pocketIndex = freshPocket;
                    }
                }

                if (currentMode == MODE_AUTO_AIM) {
                    applyAutoSpin();
                    bAimedThisTurn = true;
                    lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
                    g_postAimLock = true;
                    g_postAimAngle = pendingShotAngle;
                    g_postAimPower = pendingShotPower;
                    g_postAimFrames = 20;
                    ClearState();
                    state = IDLE;
                } else {
                    if (automationSpeed == SPEED_HUMAN && playStyle != STYLE_INSTANT) {
                        applyAutoSpin();
                        humanShotLocked = true;
                        humanState = HUM_THINKING;
                        stateStartTime = nowSec() + 0.2;
                        startAngle = pendingShotAngle;
                        state = EXECUTING;
                    } else {
                        startAngle = pendingShotAngle;
                        takeShot(pendingShotAngle, pendingShotPower, true);
                        state = EXECUTING;
                    }
                }
            } else {
                if (nominationFrameCounter > 40) nominationFrameCounter = 0;
            }
        }
    }

    if (state == WAITING_FOR_USER_POCKET) {
        setAimAngle(pendingShotAngle);
        setPower(pendingShotPower);
        int currentNom = sharedGameManager.getNominatedPocket();
        if (currentNom == g_CurrentCandidate.pocketIndex && currentNom < 6) {
            takeShot(pendingShotAngle, pendingShotPower); 
            ClearState(); 
            state = IDLE;
        }
    }

    // Real-time manual tracking
    if (bShowAutoPlayLines && isPlayerTurn && state != EXECUTING && state != NOMINATING && state != WAITING_FOR_USER_POCKET && state != SCANNING && !g_autoPlayCalculating && g_CurrentCandidate.idx == -1) {
        double curAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        double curPower = getCurrentPower();
        if (curPower < 100.0) curPower = 800.0;
        gPrediction->forceFullSimulation = true;
        gPrediction->determineShotResult(true, curAngle, curPower, sharedGameManager.getShotSpin());
        gPrediction->forceFullSimulation = false;
    }

    // ============================================================
    // ── DECISION MAKING UI ──
    // ============================================================
    if (bDecisionActive && !decisionCandidates.empty() && !decisionFinished) {
        double now = nowSec();
        double evalTime = now - decisionStartTime;
        double evalDuration = 0.6; // مدة التقييم لكل كرة

        // عرض نافذة القرار
        ImGui::Begin("Bot Decision", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        ImGui::SetWindowPos(ImVec2(Width / 2 - 150, 50));
        ImGui::SetWindowSize(ImVec2(300, 100));

        // عرض الكرة الحالية
        if (decisionIndex < decisionCandidates.size()) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Evaluating Ball %d...", 
                     decisionCandidates[decisionIndex].idx);
            ImGui::Text("%s", buf);

            // شريط التقدم
            float progress = evalTime / evalDuration;
            ImGui::ProgressBar(progress, ImVec2(200, 20));

            // عرض درجة الكرة
            double score = decisionCandidates[decisionIndex].score;
            ImGui::Text("Score: %.1f", score);
            ImGui::Text("Decision: %s", (score < 300.0) ? "Good" : "Bad");
        }

        // عملية التقييم
        if (evalTime >= evalDuration) {
            if (decisionIndex < decisionCandidates.size()) {
                double score = decisionCandidates[decisionIndex].score;
                bool isGood = (score < 300.0);

                if (isGood) {
                    // الكرة مناسبة → اضرب
                    ImGui::Text("Good shot! Shooting...");
                    g_CurrentCandidate = decisionCandidates[decisionIndex];
                    Shoot(decisionCandidates[decisionIndex].angle, 
                          decisionCandidates[decisionIndex].power);
                    bDecisionActive = false;
                    decisionCandidates.clear();
                    decisionFinished = true;
                    ImGui::End();
                    return;
                } else {
                    // الكرة غير مناسبة → انتقل إلى التالية
                    ImGui::Text("Bad shot. Switching...");
                    decisionIndex++;
                    if (decisionIndex >= decisionCandidates.size()) {
                        // لا يوجد كرات مناسبة → ضربة آمنة
                        ImGui::Text("No good shot. Playing safe...");
                        double safeAngle = 0.0;
                        double safePower = 300.0;
                        if (gPrediction->guiData.ballsCount > 1) {
                            auto& cueBall = gPrediction->guiData.balls[0];
                            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                                if (gPrediction->guiData.balls[i].originalOnTable) {
                                    Point2D dir = gPrediction->guiData.balls[i].initialPosition - cueBall.initialPosition;
                                    safeAngle = atan2(dir.y, dir.x);
                                    if (safeAngle < 0) safeAngle += 2 * M_PI;
                                    break;
                                }
                            }
                        }
                        setAimAngle(safeAngle);
                        setPower(safePower);
                        triggerShot();
                        ClearState();
                        bDecisionActive = false;
                        decisionCandidates.clear();
                        decisionFinished = true;
                        ImGui::End();
                        return;
                    }
                    // الانتقال إلى الكرة التالية
                    g_CurrentCandidate = decisionCandidates[decisionIndex];
                    setAimAngle(decisionCandidates[decisionIndex].angle);
                    setPower(decisionCandidates[decisionIndex].power);
                    decisionStartTime = now;
                    ImGui::End();
                    return;
                }
            }
        }

        ImGui::End();
        g_autoPlayCalculating = false;
        return;
    }
}

inline bool AutoPlay::AreBallsMoving() {
    if (!sharedGameManager) return false;
    Table table = sharedGameManager.mTable;
    if (!table) return false;
    auto& balls = table.mBalls();
    if (!balls) return false;
    for (int i = 0; i < balls.Count; i++) {
        Ball ball = balls[i];
        if (ball && ball.isOnTable()) {
            auto vel = ball.velocity();
            if (vel.x * vel.x + vel.y * vel.y > 0.000001) return true;
            auto spin = ball.spin();
            if (spin.x * spin.x + spin.y * spin.y + spin.z * spin.z > 0.000001) return true;
        }
    }
    return false;
}

inline bool isTouchLockedByBot() {
    return (AutoPlay::g_PredictionLocked && AutoPlay::g_CurrentCandidate.idx != -1) || (AutoPlay::state == AutoPlay::NOMINATING);
}

inline bool AllGroupBallsPocketed() {
    if (!gPrediction) return false;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();

    if (myclass == Ball::Classification::EIGHT_BALL) {
        bool solidsAllPocketed = true;
        bool stripesAllPocketed = true;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == Ball::Classification::EIGHT_BALL) continue;
            if (ball.classification == Ball::Classification::CUE_BALL) continue;
            if (ball.classification == Ball::Classification::ANY) continue;
            if (ball.classification == Ball::Classification::NINE_BALL_RULE) continue;
            if (ball.originalOnTable && ball.onTable) {
                if (ball.classification == Ball::Classification::SOLID) solidsAllPocketed = false;
                if (ball.classification == Ball::Classification::STRIPE) stripesAllPocketed = false;
            }
        }
        return solidsAllPocketed || stripesAllPocketed;
    }

    if (myclass == Ball::Classification::ANY) return false;

    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (ball.classification != myclass) continue;
        if (ball.originalOnTable && ball.onTable) {
            return false;
        }
    }
    return true;
}
