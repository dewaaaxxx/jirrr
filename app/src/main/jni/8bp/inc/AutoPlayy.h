#pragma once

#include "Prediction.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <deque>
#include <limits>
#include <random>
#include <chrono>

#include "ScreenTable.h"
#include "PhysicsModel.h"
#include "GameSpeedControl.h"
#include "8bp/FrictionProperties.h"
#include "ButtonClicker.h"
#include "AutoAim.h"

// Global variables shared with PowerSlider.h (must be defined before PowerSlider include)
inline Candidate g_CurrentCandidate = {-1, 0.0, 0.0, -1, 0.0, 0.0};
inline Point2D lastFailedCuePos = {-1000.0, -1000.0};

#include "../../PowerSlider.h"

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

static double DistToSegmentSq(const Point2D& p, const Point2D& a, const Point2D& b) {
    Point2D v = b - a;
    Point2D w = p - a;
    double c1 = w.x * v.x + w.y * v.y;
    if (c1 <= 0) return (p - a).square();
    double c2 = v.x * v.x + v.y * v.y;
    if (c2 <= c1) return (p - b).square();
    double t = c1 / c2;
    Point2D closest = { a.x + t * v.x, a.y + t * v.y };
    return (p - closest).square();
}

static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<> humanDelayDist(0.15, 0.4);

static bool bAimedThisTurn = false;
static Point2D lastCuePosWhenAimed = { -1000.0, -1000.0 };

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct AutoPlay {
    enum State {
        IDLE,
        SCANNING,
        EXECUTING,
        NOMINATING,
        NOMINATING_HUMAN,
        WAITING_FOR_USER_POCKET
    };

    enum HumanState {
        HUM_IDLE,
        HUM_THINKING,
        HUM_OVERSHOOTING,
        HUM_CORRECTING,
        HUM_HOLDING,
        HUM_STABILIZING,
        HUM_PULLING,
        HUM_DELAY_BEFORE_SHOT
    };

    enum Mode        { MODE_OFF, MODE_AUTO_PLAY, MODE_AUTO_AIM };
    enum AutomationSpeed { SPEED_FAST, SPEED_HUMAN };
    enum PlayStyle   { STYLE_HUMAN, STYLE_INSTANT };
    enum CleanTableMode { CLEAN_OFF, CLEAN_YOUR_BALLS, CLEAN_ALL_BALLS };
    enum SpinPreset  { SPIN_CENTER, SPIN_TOP, SPIN_BOTTOM, SPIN_LEFT, SPIN_RIGHT };
    enum ScanMode    { FAST, SLOW };
    enum NineBallStrategy { NINEBALL_BEST_SHOT, NINEBALL_SNIPE_9 };

    struct FastScanState {
        struct Eval {
            Candidate c  = {-1, 0.0, 0.0, -1, 0.0, 0.0};
            int tot      = 0;
            int own      = 0;
            bool p9      = false;
        };
        bool isInitiated       = false;
        Point2D scanCuePos     = {0.0, 0.0};
        std::vector<Candidate> raw;
        std::vector<Eval>      evals;
        size_t evalIndex       = 0;
    };

    inline static State          state                   = IDLE;
    inline static HumanState     humanState              = HUM_IDLE;
    inline static ScanMode       scan                    = FAST;
    inline static bool           g_PredictionLocked      = false;
    inline static bool           g_autoPlayCalculating   = false;
    inline static bool           humanShotLocked         = false;
    inline static bool           humanNeedsNomination    = false;
    inline static bool           bShowAutoPlayLines      = false;
    inline static bool           bAutoPlaying            = false;
    inline static bool           bCueBallIsMovingOrDragging = false;
    inline static bool           bCushionShot            = false;
    inline static bool           bAutoSpin               = false;
    inline static bool           bAutoPlaySwitch         = false;
    inline static bool           bAutoAimSwitch          = false;
    inline static Mode           currentMode             = MODE_AUTO_PLAY;
    inline static AutomationSpeed automationSpeed        = SPEED_FAST;
    inline static PlayStyle      playStyle               = STYLE_HUMAN;
    inline static CleanTableMode cleanTableMode          = CLEAN_OFF;
    inline static NineBallStrategy nineBallStrategy      = NINEBALL_BEST_SHOT;
    inline static SpinPreset     spinPreset              = SPIN_CENTER;
    inline static int            powerMax                = 666;
    inline static int            powerMin                = 200;
    inline static int            nominationFrameCounter  = 0;
    inline static int            humanNominationPocket   = -1;
    inline static int            frameCounter            = 0;
    inline static double         targetAngle             = 0.0;
    inline static double         startAngle              = 0.0;
    inline static double         pendingShotAngle        = 0.0;
    inline static double         pendingShotPower        = 0.0;
    inline static double         startPower              = 0.0;
    inline static double         targetPower             = 0.0;
    inline static double         stateStartTime          = 0.0;
    inline static double         sweepAngle              = 0.0;
    inline static double         overshootOffset         = 0.0;
    inline static double         currentOvershootTarget  = 0.0;
    inline static Point2D        lastSetCuePos           = {-1000.0, -1000.0};

    static double nowSec() {
        using namespace std::chrono;
        return duration_cast<duration<double>>(steady_clock::now().time_since_epoch()).count();
    }

    // ===== ALL METHODS ARE NOW STATIC =====
    static void applyAutoSpin();
    static std::vector<Point2D> getPockets();
    static void ClearState();
    static void setAimAngle(double angle);
    static void setPower(double power);
    static double getCurrentPower();
    static void takeShot(double angle, double power, bool preserveStartAngle = false);
    static void triggerShot();
    static bool IsAnimationActive();
    static void Shoot(double angle, double power);
    static void ScanSlow(double angleStep = 0.01);
    static void ScanFast(double angleStep = 0.01);
    static void Update();
    static bool AreBallsMoving();
};

static double CalculateRequiredPower(double totalDist) {
    double p = sqrt(totalDist * 2.0 * 210.0); 
    if (p < 220.0) p = 220.0;
    if (gPrediction && gPrediction->guiData.balls[8].originalOnTable) {
        bool onlyEightLeft = false;
        auto myclass = sharedGameManager.getPlayerClassification();
        if (myclass == Ball::Classification::SOLID) {
            bool hasSolids = false;
            for (int k = 1; k < 8; k++) if (gPrediction->guiData.balls[k].originalOnTable) { hasSolids = true; break; }
            if (!hasSolids) onlyEightLeft = true;
        } else if (myclass == Ball::Classification::STRIPE) {
            bool hasStripes = false;
            for (int k = 9; k <= 15; k++) if (gPrediction->guiData.balls[k].originalOnTable) { hasStripes = true; break; }
            if (!hasStripes) onlyEightLeft = true;
        } else if (myclass == Ball::Classification::EIGHT_BALL || myclass == Ball::Classification::ANY) {
            bool hasOthers = false;
            for (int k = 1; k <= 15; k++) {
                if (k == 8) continue;
                if (gPrediction->guiData.balls[k].originalOnTable) { hasOthers = true; break; }
            }
            if (!hasOthers) onlyEightLeft = true;
        }
        if (onlyEightLeft && p < 250.0) p = 250.0; 
    }
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

// ===== STATIC METHOD DEFINITIONS =====

static constexpr double POCKET_SAFETY_DISTANCE = 15.0;

static bool IsCueBallSafe() {
    if (!gPrediction) return false;
    auto allPockets = AutoPlay::getPockets();
    const Point2D& cuePos = gPrediction->guiData.balls[0].initialPosition;
    for (auto& p : allPockets) {
        if ((cuePos - p).square() < (POCKET_SAFETY_DISTANCE * POCKET_SAFETY_DISTANCE))
            return false;
    }
    return true;
}

inline void AutoPlay::applyAutoSpin() {
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

inline std::vector<Point2D> AutoPlay::getPockets() {
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

inline void AutoPlay::ClearState() {
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

inline void AutoPlay::setAimAngle(double angle) {
    if (!sharedGameManager) return;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return;
    auto vg = vc.mVisualGuide();
    if (!vg) return;
    lastSetCuePos = gPrediction->guiData.balls[0].initialPosition;
    vg.mAimAngle(angle);
}

inline void AutoPlay::setPower(double power) {
    if (!sharedGameManager) return;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return;
    vc.mPower(ShotPowerToPower(power));
}

inline double AutoPlay::getCurrentPower() {
    if (!sharedGameManager) return 0.0;
    auto vc = sharedGameManager.mVisualCue();
    if (!vc) return 0.0;
    return vc.mPower();
}

inline void AutoPlay::takeShot(double angle, double power, bool preserveStartAngle) {
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

inline void AutoPlay::triggerShot() {
    g_postShotLock = true;
    g_postShotAngle = (automationSpeed == SPEED_HUMAN) ? targetAngle : anim_TargetAngle;
    g_postShotPower = (automationSpeed == SPEED_HUMAN) ? pendingShotPower : anim_TargetPower;
    g_postShotFrames = 15;
    M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
}

inline bool AutoPlay::IsAnimationActive() {
    auto visualCue = sharedGameManager.mVisualCue();
    if (!visualCue) return false;
    auto _powerBarView = F(ptr, visualCue + 0x510);
    if (!_powerBarView) return false;
    return (M(ptr, libmain + 0x2de6f30, ptr)(_powerBarView) != 0);
}

inline void AutoPlay::Shoot(double angle, double power) {
    applyAutoSpin();
    
    bool isBreakPosition = false;
    if (gPrediction && gPrediction->guiData.ballsCount >= 15) {
        int racked = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.initialPosition.x > 75.0 && b.initialPosition.x < 115.0) racked++;
        }
        if (racked >= 10) isBreakPosition = true;
    }

    if (isBreakPosition) {
        struct BreakVariant { double angle_off; double spin_y; };
        std::vector<BreakVariant> variants = {
            {0.015, -0.8}, {-0.015, -0.8}, {0.0, -0.5}, {0.025, -0.9},
            {-0.025, -0.9}, {0.010, -0.7}, {-0.010, -0.7}, {0.0, -0.6},
            {0.030, -0.95}, {-0.030, -0.95}
        };

        bool foundBreak = false;
        int maxBallsPocketed = 0;
        BreakVariant bestBreakVariant = {0.0, -0.7};
        double bestBreakAngle = angle;
        double bestBreakSpinY = -0.7;

        for (auto& v : variants) {
            double testAngle = angle + v.angle_off;
            double testPower = 666.0;
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, testAngle, testPower, {0.0, v.spin_y});
            gForceFullSimulation = false;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                    if (gPrediction->guiData.balls[0].onTable) {
                        int currentBallsPocketed = 0;
                        for (int k = 1; k < gPrediction->guiData.ballsCount; k++) {
                            if (gPrediction->guiData.balls[k].originalOnTable && !gPrediction->guiData.balls[k].onTable) {
                                currentBallsPocketed++;
                            }
                        }
                        if (currentBallsPocketed > 0 && currentBallsPocketed >= maxBallsPocketed) {
                            maxBallsPocketed = currentBallsPocketed;
                            bestBreakVariant = v;
                            bestBreakAngle = testAngle;
                            bestBreakSpinY = v.spin_y;
                            foundBreak = true;
                        }
                    }
                }
            }
        }

        angle = bestBreakAngle;
        power = 666.0;
        sharedGameManager.mVisualEnglishControl().mEnglish({0.0, bestBreakSpinY});
        if (maxBallsPocketed == 0) {
            angle = angle + 0.02;
            sharedGameManager.mVisualEnglishControl().mEnglish({0.0, -0.9});
        }
    }

    angle = NumberUtils::normalizeDoublePrecision(angle);
    power = NumberUtils::normalizeDoublePrecision(power);

    gForceFullSimulation = true;
    gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
    gForceFullSimulation = false;

    bool nominating = false;
    int nominationMode = sharedGameManager.getPocketNominationMode();
    auto myclass = sharedGameManager.getPlayerClassification();
    
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

    if (currentMode == MODE_AUTO_AIM) {
        applyAutoSpin();
        if (playStyle == STYLE_INSTANT) {
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

    if (automationSpeed == SPEED_HUMAN || automationSpeed == SPEED_FAST) {
        applyAutoSpin();
        
        startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        targetAngle = angle;
        pendingShotPower = power;
        
        g_PredictionLocked = true;
        bShowAutoPlayLines = false; 

        gForceFullSimulation = true;
        gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin(), g_CurrentCandidate);
        gForceFullSimulation = false;

        if (playStyle == STYLE_INSTANT) {
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

inline void AutoPlay::ScanSlow(double angleStep) {
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
    uint nominatedPocket = sharedGameManager.getNominatedPocket();

    bool onlyEightBallLeft = false;
    if (myclass == Ball::Classification::SOLID) {
        bool hasSolids = false;
        for (int k = 1; k < 8; k++) {
            if (gPrediction->guiData.balls[k].originalOnTable) { hasSolids = true; break; }
        }
        if (!hasSolids) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::STRIPE) {
        bool hasStripes = false;
        for (int k = 9; k <= 15; k++) {
            if (gPrediction->guiData.balls[k].originalOnTable) { hasStripes = true; break; }
        }
        if (!hasStripes) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::EIGHT_BALL) {
        onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::ANY) {
        bool hasOthers = false;
        for (int k = 1; k <= 15; k++) {
            if (k == 8) continue;
            if (gPrediction->guiData.balls[k].originalOnTable) { hasOthers = true; break; }
        }
        if (!hasOthers) onlyEightBallLeft = true;
    }

    int steps = 0;
    bool found = false;
    static Candidate bestCandidate = {-1, 0, 0, -1, 0, 0};
    static int bestScore = -1;
    
    if (currentScanAngle == 0.0 || !isScanningInProgress) {
        bestScore = -1;
    }

    int maxSteps = (onlyEightBallLeft ? 10 : 1);

    while (steps < maxSteps && currentScanAngle < 2.0 * M_PI) {
        double angle = currentScanAngle;
        sweepAngle = angle;
        currentScanAngle += (onlyEightBallLeft ? angleStep * 0.25 : angleStep);
        steps++;

        std::vector<double> powers = { CalculateRequiredPower(150.0), CalculateRequiredPower(350.0), (double)powerMax };
        for (double power : powers) {
            bool doSim = true;
            if (cleanTableMode == CLEAN_ALL_BALLS) {
                doSim = (automationSpeed == SPEED_HUMAN);
            }
            gForceFullSimulation = doSim;
            gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
            gForceFullSimulation = false;

            if (!gPrediction->guiData.balls[0].onTable) {
                continue;
            }

            if (!IsCueBallSafe()) continue;

            int tot = 0, own = 0; bool hasLegal = false, p8 = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                    tot++; if (i == 8) p8 = true;
                    if (onlyEightBallLeft) {
                        if (i == 8) {
                            if (nominatedPocket >= 6 || gPrediction->guiData.balls[i].pocketIndex == nominatedPocket) {
                                hasLegal = true; own++;
                            }
                        }
                    } else {
                        bool m = (myclass == Ball::Classification::ANY) ? (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : (gPrediction->guiData.balls[i].classification == myclass);
                        if (m) {
                            if (nominatedPocket >= 6 || gPrediction->guiData.balls[i].pocketIndex == nominatedPocket) {
                                hasLegal = true; own++;
                            }
                        }
                    }
                }
            }

            if (!hasLegal) continue;
            auto firstHit = gPrediction->guiData.collision.firstHitBall;
            if (!firstHit) continue;

            if (onlyEightBallLeft) {
                if (firstHit->index != 8) continue;
            } else {
                if (myclass == Ball::Classification::ANY && firstHit->classification == Ball::Classification::EIGHT_BALL) continue;
                if (myclass != Ball::Classification::ANY && firstHit->classification != myclass) continue;
                if (p8) continue;
            }

            int currentScore = (own * 100);

            if (currentScore > bestScore) {
                int bestPottedIdx = -1;
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                        if (nominatedPocket < 6 && gPrediction->guiData.balls[i].pocketIndex != nominatedPocket) continue;
                        if (onlyEightBallLeft) {
                            if (i == 8) {
                                if (gPrediction->guiData.balls[0].onTable) {
                                    bestPottedIdx = 8;
                                }
                            }
                        } else {
                            bool m = (myclass == Ball::Classification::ANY) ? (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : (gPrediction->guiData.balls[i].classification == myclass);
                            if (m) {
                                if (bestPottedIdx == -1 || i == gPrediction->guiData.collision.firstHitBall->index) bestPottedIdx = i;
                            }
                        }
                    }
                }

                if (bestPottedIdx != -1) {
                    bestScore = currentScore;
                    bestCandidate = {bestPottedIdx, angle, (double)currentScore, (int)gPrediction->guiData.balls[bestPottedIdx].pocketIndex, power};
                    if (cleanTableMode == CLEAN_YOUR_BALLS && own >= 1) { found = true; break; }
                }
            }
        }
        if (found) break;
    }
    
    if (found || currentScanAngle >= 2.0 * M_PI) {
        if (onlyEightBallLeft && currentScanAngle < 2.0 * M_PI && !found) {
            return;
        }
        isScanningInProgress = false;  
        currentScanAngle = 0.0; 
        scan = FAST; 
        g_autoPlayCalculating = false;
        bShowAutoPlayLines = false;
        if (bestScore != -1) {
            g_CurrentCandidate = bestCandidate;
            Shoot(bestCandidate.angle, bestCandidate.power);
        } else {
            state = IDLE;
        }
        bestScore = -1;
    }
}

inline void AutoPlay::ScanFast(double angleStep) {
    if (g_CurrentCandidate.idx != -1) return;
    
    bShowAutoPlayLines = !persistent_bool[O("bDisableFlicker")];
    static double fastSweepAngle = 0.0;
    
    Prediction::SceneData savedGuiData = gPrediction->guiData;
    
    auto& cueBall = gPrediction->guiData.balls[0];
    double distSq = (cueBall.initialPosition - fs.scanCuePos).square();
    
    if (!fs.isInitiated || distSq > 0.0025) {
        fs.raw.clear();
        fs.evals.clear();
        fs.evalIndex = 0;
        fs.scanCuePos = cueBall.initialPosition;
        fs.isInitiated = true;
        fastSweepAngle = 0.0;

        if (automationSpeed == SPEED_HUMAN && humanShotLocked)
            return;
        
        if (currentMode == MODE_AUTO_AIM && bAimedThisTurn) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);
        
        bool mustHitCushion = false;
        if (sharedGameManager && sharedGameManager._rules()) {
            auto rules = sharedGameManager._rules();
            if (F(bool, rules + 0x112) || F(bool, rules + 0x113)) {
                mustHitCushion = true;
            }
        }

        auto pockets = getPockets();

        bool onlyEightBallLeft = false;
        int remainingOwnBalls = 0;
        
        if (myclass == Ball::Classification::SOLID) {
            for (int k = 1; k < 8; k++) {
                if (gPrediction->guiData.balls[k].originalOnTable) remainingOwnBalls++;
            }
        } else if (myclass == Ball::Classification::STRIPE) {
            for (int k = 9; k <= 15; k++) {
                if (gPrediction->guiData.balls[k].originalOnTable) remainingOwnBalls++;
            }
        } else if (myclass == Ball::Classification::EIGHT_BALL) {
            remainingOwnBalls = 0;
        } else if (myclass == Ball::Classification::ANY) {
            for (int k = 1; k <= 15; k++) {
                if (k == 8) continue;
                if (gPrediction->guiData.balls[k].originalOnTable) remainingOwnBalls++;
            }
        }
        
        if (remainingOwnBalls == 0 && gPrediction->guiData.balls[8].originalOnTable) {
            onlyEightBallLeft = true;
        }

        std::vector<Candidate> directRaw;
        std::vector<Candidate> specialRaw;

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
                bool isACandidate = false;
                if (onlyEightBallLeft) {
                    isACandidate = (i == 8);
                } else {
                    isACandidate = (myclass == Ball::Classification::ANY) ? (ball.classification != Ball::Classification::EIGHT_BALL) : (ball.classification == myclass);
                }
                if (!isACandidate) continue;
            }
            for (int pocketIdx = 0; pocketIdx < pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;
                Point2D pocket = pockets[pocketIdx];
                Point2D toPocket = pocket - ball.initialPosition;
                double distTargetToPocket = sqrt(toPocket.square());
                
                Point2D direction;
                Point2D ghostBallPos;
                
                if (distTargetToPocket < 0.1) {
                    Point2D cueToBall = ball.initialPosition - cueBall.initialPosition;
                    double distCueToBall = sqrt(cueToBall.square());
                    if (distCueToBall < 0.1) continue;
                    direction = cueToBall * (1.0 / distCueToBall);
                    ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);
                    distTargetToPocket = 0.05;
                } else {
                    direction = toPocket * (1.0 / distTargetToPocket);
                    ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);
                }

                {
                    Point2D shotLine = ghostBallPos - cueBall.initialPosition;
                    double distCueToTarget = sqrt(shotLine.square());
                    double angle = atan2(shotLine.y, shotLine.x);
                    if (angle < 0) angle += 2 * M_PI;
                    double totalDist = distCueToTarget + distTargetToPocket;
                    double power = CalculateRequiredPower(totalDist);
                    if (totalDist > 800.0) power *= 1.05; 
                    if (power > 666.0) power = 666.0;
                    directRaw.push_back({i, angle, totalDist, pocketIdx, power, totalDist});
                }

                if (bCushionShot && distTargetToPocket >= 0.1) {
                    for (int side = 0; side < 4; side++) {
                        Point2D mp;
                        switch(side) {
                            case 0: mp = {pocket.x, -134.6 - pocket.y}; break;
                            case 1: mp = {pocket.x, 134.6 - pocket.y}; break;
                            case 2: mp = {-261.6 - pocket.x, pocket.y}; break;
                            case 3: mp = {261.6 - pocket.x, pocket.y}; break;
                        }
                        Point2D toMir = mp - ball.initialPosition;
                        double distP = sqrt(toMir.square());
                        if (distP > 0.1 && distP < 1200.0) {
                            Point2D ghost = ball.initialPosition - (toMir * (1.0 / distP)) * (2.0 * BALL_RADIUS);
                            Point2D shot = ghost - cueBall.initialPosition;
                            double distC = sqrt(shot.square());
                            if (distC > 1000.0) continue;
                            double angle = atan2(shot.y, shot.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double score = distC + distP + 200.0;
                            double power = CalculateRequiredPower(distC + distP) * 1.30;
                            if (power > (double)powerMax) power = (double)powerMax;
                            if (power < (double)powerMin) power = (double)powerMin;
                            specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                        }
                    }
                }

                if (bCushionShot && distTargetToPocket >= 0.1) {
                    for (int side = 0; side < 4; side++) {
                        Point2D mg;
                        switch(side) {
                            case 0: mg = {ghostBallPos.x, -134.6 - ghostBallPos.y}; break;
                            case 1: mg = {ghostBallPos.x, 134.6 - ghostBallPos.y}; break;
                            case 2: mg = {-261.6 - ghostBallPos.x, ghostBallPos.y}; break;
                            case 3: mg = {261.6 - ghostBallPos.x, ghostBallPos.y}; break;
                        }
                        Point2D shot = mg - cueBall.initialPosition;
                        double distC = sqrt(shot.square());
                        double angle = atan2(shot.y, shot.x);
                        if (angle < 0) angle += 2 * M_PI;
                        double score = distC + distTargetToPocket + 150.0;
                        double power = CalculateRequiredPower(distC + distTargetToPocket) * 1.30;
                        if (power > (double)powerMax) power = (double)powerMax;
                        if (power < (double)powerMin) power = (double)powerMin;
                        specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                    }
                }

                if (distTargetToPocket >= 0.1) {
                    for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                        if (j == i) continue;
                        auto& ballB = gPrediction->guiData.balls[j];
                        if (!ballB.originalOnTable) continue;
                        
                        bool isB_Valid = false;
                        if (isNineBallGame) {
                            isB_Valid = true; 
                        } else {
                            if (onlyEightBallLeft) {
                                isB_Valid = false;
                            } else {
                                isB_Valid = (myclass == Ball::Classification::ANY) ? 
                                             (ballB.classification != Ball::Classification::EIGHT_BALL) : 
                                             (ballB.classification == myclass);
                            }
                        }
                        if (!isB_Valid) continue;

                        Point2D toPocketB = pocket - ballB.initialPosition;
                        double distBToPocket = sqrt(toPocketB.square());
                        if (distBToPocket < 0.1) continue;
                        Point2D directionB = toPocketB * (1.0 / distBToPocket);
                        Point2D ghostB = ballB.initialPosition - directionB * (2.0 * BALL_RADIUS);
                        
                        Point2D toGhostB = ghostB - ball.initialPosition;
                        double distAToGhostB = sqrt(toGhostB.square());
                        if (distAToGhostB < 0.1) continue;
                        Point2D directionA = toGhostB * (1.0 / distAToGhostB);
                        Point2D ghostA = ball.initialPosition - directionA * (2.0 * BALL_RADIUS);
                        
                        Point2D shotLine = ghostA - cueBall.initialPosition;
                        double distCueToA = sqrt(shotLine.square());
                        double angle = atan2(shotLine.y, shotLine.x);
                        if (angle < 0) angle += 2 * M_PI;
                        double score = distCueToA + distAToGhostB + distBToPocket + 80.0;
                        double power = CalculateRequiredPower(distCueToA + distAToGhostB + distBToPocket) * 1.1;
                        if (power > (double)powerMax) power = (double)powerMax;
                        if (power < (double)powerMin) power = (double)powerMin;
                        specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                    }
                }

                if (distTargetToPocket >= 0.1) {
                    for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                        if (j == i) continue;
                        auto& ballB = gPrediction->guiData.balls[j];
                        if (!ballB.originalOnTable) continue;
                        
                        bool isB_Valid = false;
                        if (isNineBallGame) {
                            isB_Valid = true; 
                        } else {
                            if (onlyEightBallLeft) {
                                isB_Valid = false;
                            } else {
                                isB_Valid = (myclass == Ball::Classification::ANY) ? 
                                             (ballB.classification != Ball::Classification::EIGHT_BALL) : 
                                             (ballB.classification == myclass);
                            }
                        }
                        if (!isB_Valid) continue;

                        Point2D toPocketB = pocket - ballB.initialPosition;
                        double distBToPocket = sqrt(toPocketB.square());
                        if (distBToPocket < 0.1 || distBToPocket > 800.0) continue;
                        
                        Point2D directionB = toPocketB * (1.0 / distBToPocket);
                        Point2D ghostB = ballB.initialPosition - directionB * (2.0 * BALL_RADIUS);
                        
                        Point2D d = ghostB - ball.initialPosition;
                        double distD = sqrt(d.square());
                        if (distD < 2.0 * BALL_RADIUS || distD > 600.0) continue;

                        double ratio = (2.0 * BALL_RADIUS) / distD;
                        if (ratio > 1.0) ratio = 1.0;
                        double theta = acos(ratio);
                        double angleD = atan2(d.y, d.x);

                        for (int sign : {-1, 1}) {
                            double angleU = angleD + sign * theta;
                            Point2D u = {cos(angleU), sin(angleU)};
                            Point2D ghostA = ball.initialPosition + u * (2.0 * BALL_RADIUS);
                            
                            Point2D shotLine = ghostA - cueBall.initialPosition;
                            double distCueToA = sqrt(shotLine.square());
                            if (distCueToA > 1000.0) continue;

                            double angle = atan2(shotLine.y, shotLine.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double score = distCueToA + distD + distBToPocket + 150.0;
                            double power = CalculateRequiredPower(distCueToA + distD + distBToPocket) * 1.25;
                            if (power > (double)powerMax) power = (double)powerMax;
                            if (power < (double)powerMin) power = (double)powerMin;
                            specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                        }
                    }
                }
            }
        }

        fs.raw.clear();
        fs.raw.insert(fs.raw.end(), directRaw.begin(), directRaw.end());
        fs.raw.insert(fs.raw.end(), specialRaw.begin(), specialRaw.end());
        
        std::sort(fs.raw.begin(), fs.raw.end(), [mustHitCushion, onlyEightBallLeft](const Candidate& a, const Candidate& b) {
            double scoreA = a.score;
            double scoreB = b.score;
            bool aIsSpecial = (a.score > 79.0);
            bool bIsSpecial = (b.score > 79.0);
            if (!mustHitCushion) {
                if (aIsSpecial && !bIsSpecial) scoreA += 500.0;
                if (!aIsSpecial && bIsSpecial) scoreB += 500.0;
            } else {
                if (aIsSpecial && !bIsSpecial) scoreA -= 500.0;
                if (!aIsSpecial && bIsSpecial) scoreB -= 500.0;
            }
            if (onlyEightBallLeft) {
                if (!aIsSpecial && !bIsSpecial) {
                    return a.score < b.score;
                }
            }
            return scoreA < scoreB;
        });
        
        if (fs.raw.size() > (onlyEightBallLeft ? 300 : 120)) {
            fs.raw.resize(onlyEightBallLeft ? 300 : 120);
        }

        std::sort(fs.raw.begin(), fs.raw.end(), [](const Candidate& a, const Candidate& b) {
            return a.angle < b.angle;
        });
    }

    if (automationSpeed == SPEED_HUMAN && humanShotLocked) return;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

    bool onlyEightBallLeft = false;
    if (myclass == Ball::Classification::SOLID) {
        bool hasSolids = false;
        for (int k = 1; k < 8; k++) {
            if (gPrediction->guiData.balls[k].originalOnTable) { hasSolids = true; break; }
        }
        if (!hasSolids) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::STRIPE) {
        bool hasStripes = false;
        for (int k = 9; k <= 15; k++) {
            if (gPrediction->guiData.balls[k].originalOnTable) { hasStripes = true; break; }
        }
        if (!hasStripes) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::EIGHT_BALL) {
        onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::ANY) {
        bool hasOthers = false;
        for (int k = 1; k <= 15; k++) {
            if (k == 8) continue;
            if (gPrediction->guiData.balls[k].originalOnTable) { hasOthers = true; break; }
        }
        if (!hasOthers) onlyEightBallLeft = true;
    }

    int stepsInThisFrame = 0;
    const int maxStepsPerFrame = 60;

    while (stepsInThisFrame < maxStepsPerFrame && fs.evalIndex < fs.raw.size()) {
        auto raw = fs.raw[fs.evalIndex++];
        stepsInThisFrame++;

        double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(raw.angle));
        int originalIdx = raw.idx;
        
        bool evaluatedValid = false;
        double testPower = raw.power;
        if (automationSpeed == SPEED_FAST) {
            auto pockets = getPockets();
            Point2D pocketPos = pockets[raw.pocketIndex];
            auto& targetBall = gPrediction->guiData.balls[raw.idx];
            double distToPocket = sqrt((pocketPos - targetBall.initialPosition).square());
            if (raw.score > 600.0) {
                testPower = 666.0;
            }
        }
        
        gForceFullSimulation = true;
        gPrediction->determineShotResult(true, angle, testPower, sharedGameManager.getShotSpin(), raw);
        gForceFullSimulation = false;
        
        bool isPotted = false;
        if (isNineBallGame) {
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                    isPotted = true; break;
                }
            }
        } else {
            if (onlyEightBallLeft) {
                isPotted = !gPrediction->guiData.balls[8].onTable && (nominatedPocket >= 6 || gPrediction->guiData.balls[8].pocketIndex == nominatedPocket);
                if (isPotted) {
                    raw.idx = 8;
                }
            } else {
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (i == 8) continue;
                    bool match = (myclass == Ball::Classification::ANY) ? 
                                 (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : 
                                 (gPrediction->guiData.balls[i].classification == myclass);
                    if (match && !gPrediction->guiData.balls[i].onTable) {
                        if (nominatedPocket >= 6 || gPrediction->guiData.balls[i].pocketIndex == nominatedPocket) {
                            isPotted = true;
                            raw.idx = i;
                            break;
                        }
                    }
                }
            }
        }

        if (gPrediction->guiData.collision.firstHitBall && gPrediction->guiData.balls[0].onTable && isPotted) {
            raw.power = testPower;
            evaluatedValid = true;
        }
        
        if (!evaluatedValid && automationSpeed == SPEED_FAST) {
            raw.idx = originalIdx;
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, angle, raw.power, sharedGameManager.getShotSpin(), raw);
            gForceFullSimulation = false;
            
            bool isPottedFallback = false;
            if (isNineBallGame) {
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (gPrediction->guiData.balls[i].originalOnTable && !gPrediction->guiData.balls[i].onTable) {
                        isPottedFallback = true; break;
                    }
                }
            } else {
                if (onlyEightBallLeft) {
                    isPottedFallback = !gPrediction->guiData.balls[8].onTable && (nominatedPocket >= 6 || gPrediction->guiData.balls[8].pocketIndex == nominatedPocket);
                    if (isPottedFallback) {
                        raw.idx = 8;
                    }
                } else {
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        if (i == 8) continue;
                        bool match = (myclass == Ball::Classification::ANY) ? 
                                     (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : 
                                     (gPrediction->guiData.balls[i].classification == myclass);
                        if (match && !gPrediction->guiData.balls[i].onTable) {
                            if (nominatedPocket >= 6 || gPrediction->guiData.balls[i].pocketIndex == nominatedPocket) {
                                isPottedFallback = true;
                                raw.idx = i;
                                break;
                            }
                        }
                    }
                }
            }

            if (gPrediction->guiData.collision.firstHitBall && gPrediction->guiData.balls[0].onTable && isPottedFallback) {
                evaluatedValid = true;
            }
        }

        if (!evaluatedValid) continue;

        if (isNineBallGame) {
            auto firstHit = gPrediction->guiData.collision.firstHitBall;
            if (!firstHit) continue;
            if (firstHit->index != raw.idx) continue;

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
            int effectiveTargetIdx = bestPottedIdx;
            if (nominatedPocket < 6 && gPrediction->guiData.balls[effectiveTargetIdx].pocketIndex != nominatedPocket) continue;

            if (!IsCueBallSafe()) continue;

            int potCount = 0;
            bool pots9 = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    potCount++;
                    if (i == 9) pots9 = true;
                }
            }
            
            Candidate cf = raw;
            cf.idx = effectiveTargetIdx;
            cf.pocketIndex = gPrediction->guiData.balls[effectiveTargetIdx].pocketIndex;
            fs.evals.push_back({cf, potCount, potCount, pots9});
            continue;
        }

        int pottedBallIdx = -1;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                bool match = false;
                if (onlyEightBallLeft) {
                    if (i == 8) match = true;
                } else {
                    match = (myclass == Ball::Classification::ANY) ?
                            (ball.classification != Ball::Classification::CUE_BALL &&
                             ball.classification != Ball::Classification::EIGHT_BALL) :
                            (ball.classification == myclass);
                }
                if (match) {
                    pottedBallIdx = i;
                    break;
                }
            }
        }

        if (pottedBallIdx == -1) continue;

        if (gPrediction->guiData.balls[pottedBallIdx].pocketIndex != raw.pocketIndex) continue;
        if (nominatedPocket < 6 && gPrediction->guiData.balls[pottedBallIdx].pocketIndex != nominatedPocket) continue;

        int totalPotted = 0;
        int ownPotted = 0;
        bool hasLegalPot = false;
        bool eightBallPotted = false;
        
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                totalPotted++;
                bool match = (myclass == Ball::Classification::ANY) ?
                             (ball.classification != Ball::Classification::CUE_BALL &&
                              ball.classification != Ball::Classification::EIGHT_BALL) :
                              (ball.classification == myclass);
                if (match) {
                    hasLegalPot = true;
                    ownPotted++;
                }
                if (i == 8) eightBallPotted = true;
            }
        }
        
        if (onlyEightBallLeft) {
            if (eightBallPotted) {
                hasLegalPot = true;
                ownPotted = 1;
                totalPotted = 1;
                ownPotted += 5; 
            } else {
                hasLegalPot = false;
            }
        }
        
        if (!hasLegalPot) continue;

        auto firstHit = gPrediction->guiData.collision.firstHitBall;
        if (firstHit) {
            if (myclass != Ball::Classification::ANY && firstHit->classification != myclass) {
                if (!onlyEightBallLeft || firstHit->index != 8) continue;
            }
            if (myclass == Ball::Classification::ANY && firstHit->classification == Ball::Classification::EIGHT_BALL) {
                if (!onlyEightBallLeft) continue;
            }
        }
        if (!gPrediction->guiData.balls[0].onTable) continue;
        
        if (!IsCueBallSafe()) continue;
        if (eightBallPotted && !onlyEightBallLeft) continue;

        Candidate cf = raw;
        cf.idx = pottedBallIdx;
        cf.pocketIndex = gPrediction->guiData.balls[pottedBallIdx].pocketIndex;
        if (!isNineBallGame && (cleanTableMode == CLEAN_OFF || onlyEightBallLeft)) {
            if (currentMode == MODE_AUTO_AIM) {
                auto pockets = getPockets();
                Point2D pocketPos = pockets[cf.pocketIndex];
                auto& targetBall = gPrediction->guiData.balls[cf.idx];
                double distToPocket = sqrt((pocketPos - targetBall.initialPosition).square());
                Point2D cueToTarget = targetBall.initialPosition - cueBall.initialPosition;
                double distCueToTarget = sqrt(cueToTarget.square());
                double totalDist = distCueToTarget + distToPocket;
                
                double finalPower = CalculateRequiredPower(totalDist);
                if (finalPower < (double)powerMin) finalPower = (double)powerMin;
                if (finalPower > (double)powerMax) finalPower = (double)powerMax;
                
                cf.power = finalPower;
                setAimAngle(cf.angle);
                setPower(cf.power);
                bAimedThisTurn = true;
                lastCuePosWhenAimed = cueBall.initialPosition;
                g_postAimLock = true;
                g_postAimAngle = cf.angle;
                g_postAimPower = cf.power;
                g_postAimFrames = 999;
                fs.isInitiated = false;
                ClearState();
                state = IDLE;
                return;
            }
            
            bool ballPocketed = false;
            if (!gPrediction->guiData.balls[cf.idx].onTable && 
                gPrediction->guiData.balls[cf.idx].originalOnTable) {
                if (gPrediction->guiData.balls[cf.idx].pocketIndex == cf.pocketIndex) {
                    ballPocketed = true;
                }
            }
            if (!ballPocketed) {
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    if (gPrediction->guiData.balls[i].originalOnTable && 
                        !gPrediction->guiData.balls[i].onTable) {
                        bool isLegal;
                        if (onlyEightBallLeft) {
                            isLegal = (i == 8);
                        } else {
                            isLegal = (myclass == Ball::Classification::ANY) ? 
                                (gPrediction->guiData.balls[i].classification != Ball::Classification::EIGHT_BALL) : 
                                (gPrediction->guiData.balls[i].classification == myclass);
                        }
                        if (isLegal && gPrediction->guiData.balls[i].pocketIndex == cf.pocketIndex) {
                            ballPocketed = true;
                            cf.idx = i;
                            cf.pocketIndex = gPrediction->guiData.balls[i].pocketIndex;
                            break;
                        }
                    }
                }
            }
            if (ballPocketed) {
                auto pockets = getPockets();
                Point2D pocketPos = pockets[cf.pocketIndex];
                auto& targetBall = gPrediction->guiData.balls[cf.idx];
                double distToPocket = sqrt((pocketPos - targetBall.initialPosition).square());
                Point2D cueToTarget = targetBall.initialPosition - cueBall.initialPosition;
                double distCueToTarget = sqrt(cueToTarget.square());
                double totalDist = distCueToTarget + distToPocket;
                
                double finalPower = CalculateRequiredPower(totalDist);
                if (finalPower < (double)powerMin) finalPower = (double)powerMin;
                if (finalPower > (double)powerMax) finalPower = (double)powerMax;
                
                cf.power = finalPower;
                fs.isInitiated = false;
                g_CurrentCandidate = cf;
                Shoot(cf.angle, cf.power);
                return;
            }
        }

        double initialClusterScore = CalculateTableClusterScore(savedGuiData);
        double finalClusterScore = CalculateTableClusterScore(gPrediction->guiData);
        double clusterImprovement = initialClusterScore - finalClusterScore;
        int openingBonus = (clusterImprovement > 0) ? (int)(clusterImprovement * 50) : 0;
        
        cf.idx = pottedBallIdx;
        cf.pocketIndex = gPrediction->guiData.balls[pottedBallIdx].pocketIndex;
        fs.evals.push_back({cf, totalPotted, ownPotted + openingBonus, false});
    }
    gPrediction->guiData = savedGuiData;

    if (fs.evalIndex >= fs.raw.size()) {
        FastScanState::Eval* best = nullptr;
        if (onlyEightBallLeft) {
            for (auto& ev : fs.evals) {
                if (ev.c.idx == 8) {
                    if (!best || ev.tot > best->tot) best = &ev;
                }
            }
        }
        if (!best && isNineBallGame) {
            switch (nineBallStrategy) {
                case NINEBALL_BEST_SHOT:
                    for (auto& ev : fs.evals) {
                        if (!best || ev.tot > best->tot) best = &ev;
                    }
                    break;
                case NINEBALL_SNIPE_9:
                    for (auto& ev : fs.evals) {
                        if (!best) best = &ev;
                        else {
                            if (ev.p9 && !best->p9) best = &ev;
                            else if (ev.p9 == best->p9 && ev.tot > best->tot) best = &ev;
                        }
                    }
                    break;
                default:
                    if (!fs.evals.empty()) best = &fs.evals[0];
                    break;
            }
        } else {
            if (onlyEightBallLeft) {
                for (auto& ev : fs.evals) {
                    if (ev.c.idx == 8) { best = &ev; break; }
                }
                if (!best && !fs.evals.empty()) best = &fs.evals[0];
            } else {
                switch (cleanTableMode) {
                    case CLEAN_OFF:
                        if (!fs.evals.empty()) best = &fs.evals[0];
                        break;
                    case CLEAN_ALL_BALLS:
                        for (auto& ev : fs.evals) {
                            if (!best || ev.tot > best->tot) best = &ev;
                        }
                        break;
                    case CLEAN_YOUR_BALLS:
                        for (auto& ev : fs.evals) {
                            if (!best || ev.own > best->own) best = &ev;
                        }
                        break;
                }
            }
        }

        if (best) {
            fs.isInitiated = false;
            g_CurrentCandidate = best->c;
            Shoot(best->c.angle, best->c.power);
        } else {
            static int fastScanRetries = 0;
            fastScanRetries++;
            if (fastScanRetries >= 2) {
                fastScanRetries = 0;
                fs.isInitiated = false;
                lastFailedCuePos = cueBall.initialPosition;
                scan = SLOW;
                g_autoPlayCalculating = true;
            } else {
                fs.isInitiated = false;
                fs.raw.clear();
                fs.evals.clear();
                fs.evalIndex = 0;
            }
        }

        if (fs.evalIndex < fs.raw.size()) {
            fastSweepAngle = normalizeAngle(fastSweepAngle + 0.15);
            if (!persistent_bool[O("bDisableFlicker")]) {
                gForceFullSimulation = true;
                gPrediction->determineShotResult(true, fastSweepAngle, 400.0f, sharedGameManager.getShotSpin());
                gForceFullSimulation = false;
            }
            sweepAngle = fastSweepAngle;
        }
    }
}

inline void AutoPlay::Update() {
    frameCounter++;
    buttonClicker.Update();
    powerSlider.Update();

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
            if (framesCueBallStill < 10) {
                framesCueBallStill++;
            }
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

    if (sharedGameManager && frameCounter % 120 == 0) {
        auto rules = sharedGameManager._rules();
        if (rules) {
            LOGI("Ruleset State: 0x58=%d, 0x108=%d, 0x112=%d, 0x113=%d, 0x114=%d, 0x128=%d",
                 F(bool, rules + 0x58), F(bool, rules + 0x108), F(bool, rules + 0x112),
                 F(bool, rules + 0x113), F(bool, rules + 0x114), F(bool, rules + 0x128));
        }
    }

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
                NativeTouchesEnd(5, jX + (float)cos(anim_TargetAngle) * jR, 
                                    jY + (float)sin(anim_TargetAngle) * jR);

                float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
                float sliderX = Width * sliderXPercent;
                if (persistent_int[O("iPowerBarSide")] == 1) {
                    sliderX = Width * (1.0f - sliderXPercent);
                }
                float sliderYStart = Height * persistent_float[O("fPowerBarYStartPercent")];
                float sliderYEnd = Height * persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sliderX - 20.0f, sliderYStart, 40.0f, sliderYEnd - sliderYStart);
                if (playStyle == STYLE_INSTANT) {
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.40f, 0.20f);
                } else {
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.85f, 0.40f);
                }

                stateStartTime = nowSec();
                fastShotState = 2;
            }
            return;
        }

        if (fastShotState == 2) {
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, anim_TargetAngle, anim_TargetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gForceFullSimulation = false;

            if (powerSlider.Active) {
                return;
            }

            stateStartTime = nowSec();
            fastShotState = 3;
            return;
        }

        if (fastShotState == 3) {
            setAimAngle(anim_TargetAngle);

            static double s_ballsStoppedAt = -1.0;
            if (s_ballsStoppedAt < stateStartTime) {
                s_ballsStoppedAt = stateStartTime;
            }

            bool timedOut = (nowSec() - stateStartTime > 12.0);

            if (AreBallsMoving() && !timedOut) {
                s_ballsStoppedAt = nowSec();
                return;
            }

            double settledFor = nowSec() - s_ballsStoppedAt;
            if (settledFor < 0.5 && !timedOut) {
                return;
            }

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
    if (wasPlayerTurn && !isPlayerTurn) { g_autoPlayCalculating = false; ClearState(); bAimedThisTurn = false; }
    if (turnJustStarted) { bAimedThisTurn = false; lastFailedCuePos = {-1000.0, -1000.0}; }
    wasPlayerTurn = isPlayerTurn;

    static double turnStartTime = 0.0;
    if (turnJustStarted || (isPlayerTurn && turnStartTime == 0.0)) {
        turnStartTime = nowSec();
    }
    if (!isPlayerTurn) {
        turnStartTime = 0.0;
    }

    bool humanActive = (automationSpeed == SPEED_HUMAN && humanState != HUM_IDLE);
    bool isBreakPosition = false;
    if (gPrediction->guiData.ballsCount >= 15) {
        int racked = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.initialPosition.x < 70.0 || b.initialPosition.x > 120.0) racked++;
        }
        if (racked >= 13) isBreakPosition = true;
    }

    static int animationStuckCounter = 0;
    humanRunning = (automationSpeed == SPEED_HUMAN && (humanState != HUM_IDLE || humanShotLocked));
    if (IsAnimationActive() && !humanRunning && currentMode != MODE_AUTO_AIM && !isBreakPosition) {
        animationStuckCounter++;
        if (animationStuckCounter < 200) { 
            g_autoPlayCalculating = false; return;
        }
    } else {
        animationStuckCounter = 0;
    }

    if (nowSec() - g_lastFastShotTime < 2.5) {
        g_autoPlayCalculating = false;
        return;
    }

    if (AutoPlay::nowSec() < g_shotCooldownEnd) {
        g_autoPlayCalculating = false;
        return;
    }

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

    if (automationSpeed == SPEED_HUMAN && humanState != HUM_IDLE) {
        if (state == NOMINATING_HUMAN) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 15) buttonClicker.Click(GetPocketScreenPos(humanNominationPocket));
            if (nominationFrameCounter > 35 && !buttonClicker.Active) {
                humanState = HUM_THINKING; 
                stateStartTime = nowSec() + 0.35;
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

        if (humanState == HUM_OVERSHOOTING) {
            double t = (now - stateStartTime) / 1.1;
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
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, pendingShotPower, sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gForceFullSimulation = false;
            return;
        }

        if (humanState == HUM_CORRECTING) {
            double t = (now - stateStartTime) / 0.35;
            double dirSign = (overshootOffset > 0) ? 1.0 : -1.0;
            double nudgeAngle = targetAngle + dirSign * (1.5 * M_PI / 180.0);
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
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, pendingShotPower, sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gForceFullSimulation = false;
            return;
        }

        if (humanState == HUM_HOLDING) {
            double t = (now - stateStartTime) / 0.40;
            double dirSign = (overshootOffset > 0) ? 1.0 : -1.0;
            double nudgeAngle = targetAngle + dirSign * (1.5 * M_PI / 180.0);
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
                double curAngle = normalizedStart + delta * ease;
                setAimAngle(curAngle);
                UpdateJoystickVisuals(curAngle);
            }
            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, pendingShotPower, sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gForceFullSimulation = false;
            return;
        }

        if (humanState == HUM_STABILIZING) {
            float jX = Width * 0.83f;
            float jY = Height * 0.82f;
            float jR = 65.0f;
            NativeTouchesMove(5, jX + (float)cos(targetAngle) * jR, 
                                 jY + (float)sin(targetAngle) * jR);
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.4) {
                if (currentMode == MODE_AUTO_PLAY) {
                    NativeTouchesEnd(5, jX + (float)cos(targetAngle) * jR, 
                                        jY + (float)sin(targetAngle) * jR);
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
                powerSlider.SimulateDrag(sliderRect, targetPower, 0.85f, 0.4f);
            }

            gForceFullSimulation = true;
            gPrediction->determineShotResult(true, targetAngle, targetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gForceFullSimulation = false;

            if (powerSlider.Active) {
                return;
            }

            stateStartTime = now;
            humanState = HUM_DELAY_BEFORE_SHOT;
            return;
        }

        if (humanState == HUM_DELAY_BEFORE_SHOT) {
            setAimAngle(targetAngle);
            if (now - stateStartTime >= 0.4) {
                humanShotLocked = false;
                ClearState();
                state = IDLE; humanState = HUM_IDLE;
            }
            return;
        }
    }

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
            gForceFullSimulation = false;
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
        double distSq = (cueBall.initialPosition - lastCuePosWhenAimed).square();
        if (distSq > 0.0025) {
            bAimedThisTurn = false;
            lastFailedCuePos = {-1000.0, -1000.0};
            state = IDLE;
        }
    }

    if (state == IDLE) {
        bool shouldScan = (currentMode != MODE_AUTO_AIM) || !bAimedThisTurn;
        if (shouldScan) {
            state = SCANNING;
            scan = FAST;
            g_autoPlayCalculating = false;
        }
    }
    if (state == SCANNING) {
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
                if (sharedGameManager.getNominatedPocket() != g_CurrentCandidate.pocketIndex) {
                    nominationFrameCounter = 0;
                    return;
                }
                {
                    gForceFullSimulation = true;
                    gPrediction->determineShotResult(true, pendingShotAngle, pendingShotPower,
                                                    sharedGameManager.getShotSpin(), g_CurrentCandidate);
                    gForceFullSimulation = false;
                    if (g_CurrentCandidate.idx >= 0 && g_CurrentCandidate.idx < gPrediction->guiData.ballsCount) {
                        int freshPocket = gPrediction->guiData.balls[g_CurrentCandidate.idx].pocketIndex;
                        if (freshPocket >= 0 && freshPocket < 6) {
                            g_CurrentCandidate.pocketIndex = freshPocket;
                        }
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
                        stateStartTime = nowSec() + 0.3;
                        startAngle = pendingShotAngle;
                        state = EXECUTING;
                    } else {
                        startAngle = pendingShotAngle;
                        takeShot(pendingShotAngle, pendingShotPower, true);
                        state = EXECUTING;
                    }
                }
            } else {
                if (nominationFrameCounter > 40) {
                    nominationFrameCounter = 0;
                }
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

    if (bShowAutoPlayLines && isPlayerTurn && state != EXECUTING && state != NOMINATING && state != WAITING_FOR_USER_POCKET && state != SCANNING && !g_autoPlayCalculating && g_CurrentCandidate.idx == -1) {
        double curAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        double curPower = getCurrentPower();
        if (curPower < 100.0) curPower = 800.0;
        gForceFullSimulation = true;
        gPrediction->determineShotResult(true, curAngle, curPower, sharedGameManager.getShotSpin());
        gForceFullSimulation = false;
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
            if (vel.x * vel.x + vel.y * vel.y > 0.000001) {
                return true;
            }
            auto spin = ball.spin();
            if (spin.x * spin.x + spin.y * spin.y + spin.z * spin.z > 0.000001) {
                return true;
            }
        }
    }
    return false;
}

bool isTouchLockedByBot() {
    return (AutoPlay::g_PredictionLocked && g_CurrentCandidate.idx != -1) || (AutoPlay::state == AutoPlay::NOMINATING);
}