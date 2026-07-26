#include "AutoPlay.h"
#include "8bp/GameManager.h"
#include "Prediction.h"
#include "ButtonClicker.h"
extern ButtonClicker buttonClicker;
#include "PowerSlider.h"
extern PowerSlider powerSlider;
#include <math.h>
#include <random>

// --- Static Helpers ---
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

static constexpr double maxAngle = 2.0 * M_PI;

static double normalizeAngle(double angle) {
    constexpr double maxAngleVal = 2.0 * M_PI;
    double newAngle = angle;
    if (newAngle >= maxAngleVal) newAngle = fmod(newAngle, maxAngleVal);
    else if (newAngle < 0) newAngle = maxAngleVal - fmod(-newAngle, maxAngleVal);
    return newAngle;
}

static double CalculateRequiredPower(double totalDist) {
    double p = sqrt(totalDist * 2.0 * 196.0);
    if (p < 100.0) p = 100.0;
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

    bool wasJoystickTouching = anim_TouchStarted;
    anim_TouchStarted = false;

    if (!g_postShotLock) setPower(0.0);

    if (wasJoystickTouching)
        NativeTouchesEnd(5, Width * 0.83f, Height * 0.82f);

    if (powerSlider.Active) {
        float sliderXPercent = persistent_float[O("fPowerBarXPercent")];
        float sliderX = Width * sliderXPercent;
        if (persistent_int[O("iPowerBarSide")] == 1)
            sliderX = Width * (1.0f - sliderXPercent);
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
        if (sharedGameManager && sharedGameManager.mVisualCue() && sharedGameManager.mVisualCue().mVisualGuide())
            startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        else
            startAngle = angle;
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

static bool NeedsNomination(const Candidate& candidate) {
    if (candidate.idx == -1) return false;
    int nominationMode = sharedGameManager.getPocketNominationMode();
    auto myclass = sharedGameManager.getPlayerClassification();
    if ((nominationMode == 1 && myclass == Ball::Classification::EIGHT_BALL) ||
        (nominationMode == 2 && myclass != Ball::Classification::ANY)) {
        if (sharedGameManager.getNominatedPocket() != (uint)candidate.pocketIndex)
            return true;
    }
    return false;
}

// ============================================================
// SHOT EVALUATION RESULT
// Returned by EvaluateShot so callers know exactly what the
// simulation produced — without any rigid pocket pre-filter.
// ============================================================
struct ShotEvalResult {
    bool valid;          // true = shot is legal and pots a target ball
    bool cueBallSafe;    // cue ball stays on table
    int  pottedBallIdx;  // index of the legal ball that was potted (-1 if none)
    int  actualPocket;   // pocket the potted ball went into (from simulation)
    bool eightBallPotted;// 8-ball was potted (even if not target)
};

// ============================================================
// EvaluateShot
//
// Runs a full simulation for (angle, power) WITH the current
// shot spin already applied by the caller, then determines
// whether the shot is legally valid.
//
// Key design decisions vs the old ConfirmShotIntoPocket():
//   - Does NOT filter by intendedPocketIdx — lets the simulation
//     decide which pocket the ball goes into. This is critical
//     for bank/kick/carom shots where geometry != actual pocket.
//   - Returns the ACTUAL simulated pocket so the caller can
//     store it in the candidate, ensuring confirmation pocket
//     and fired pocket are always identical.
//   - Cue ball safety is checked FIRST (fast-fail).
// ============================================================
static ShotEvalResult EvaluateShot(
    double angle,
    double power,
    int    targetBallIdx,       // the ball we intend to pot
    Ball::Classification myclass,
    bool   onlyEightBallLeft,
    bool   isNineBallGame,
    uint   nominatedPocket)
{
    ShotEvalResult r = {false, false, -1, -1, false};

    gPrediction->forceFullSimulation = true;
    gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
    gPrediction->forceFullSimulation = false;

    // 1. Cue ball must stay on table.
    r.cueBallSafe = gPrediction->guiData.balls[0].onTable;
    if (!r.cueBallSafe) return r;

    // 2. First ball hit must be legal.
    auto firstHit = gPrediction->guiData.collision.firstHitBall;
    if (!firstHit) return r;

    if (isNineBallGame) {
        if (firstHit->index != targetBallIdx) return r;
    } else if (onlyEightBallLeft) {
        if (firstHit->index != 8) return r;
    } else {
        if (myclass == Ball::Classification::ANY) {
            if (firstHit->classification == Ball::Classification::EIGHT_BALL) return r;
        } else {
            if (firstHit->classification != myclass) return r;
        }
    }

    // 3. Scan potted balls.
    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
        auto& ball = gPrediction->guiData.balls[i];
        if (!ball.originalOnTable || ball.onTable) continue;
        if (i == 8) r.eightBallPotted = true;

        bool isLegal = false;
        if (onlyEightBallLeft) {
            isLegal = (i == 8);
        } else if (isNineBallGame) {
            isLegal = true;
        } else {
            isLegal = (myclass == Ball::Classification::ANY)
                ? (ball.classification != Ball::Classification::EIGHT_BALL)
                : (ball.classification == myclass);
        }

        if (!isLegal) continue;

        // Pocket nomination filter (only when a specific pocket is nominated).
        if (nominatedPocket < 6 && ball.pocketIndex != (int)nominatedPocket) continue;

        // Accept the first legal potted ball found.
        if (r.pottedBallIdx == -1 ||
            (isNineBallGame && i < r.pottedBallIdx) ||
            i == targetBallIdx) {
            r.pottedBallIdx = i;
            r.actualPocket  = ball.pocketIndex;
        }
    }

    if (r.pottedBallIdx == -1) return r;

    // 4. 8-ball must not be accidentally potted unless it IS the target.
    if (r.eightBallPotted && !onlyEightBallLeft) return r;

    r.valid = true;
    return r;
}

void AutoPlay::Shoot(double angle, double power) {
    // Apply spin FIRST so every subsequent simulation in this call
    // uses the exact same spin that will be applied on the real shot.
    applyAutoSpin();

    bool isBreakPosition = false;
    if (gPrediction && gPrediction->guiData.ballsCount >= 15) {
        int racked = 0;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& b = gPrediction->guiData.balls[i];
            if (b.initialPosition.x < 70.0 || b.initialPosition.x > 120.0) racked++;
        }
        if (racked >= 13) isBreakPosition = true;
    }
    if (isBreakPosition) power = (double)powerMax;

    angle = NumberUtils::normalizeDoublePrecision(angle);
    power = NumberUtils::normalizeDoublePrecision(power);

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();

    bool onlyEightBallLeft = false;
    if (myclass == Ball::Classification::SOLID) {
        bool has = false;
        for (int k = 1; k < 8; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::STRIPE) {
        bool has = false;
        for (int k = 9; k <= 15; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::EIGHT_BALL) {
        onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::ANY) {
        bool has = false;
        for (int k = 1; k <= 15; k++) { if (k == 8) continue; if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; } }
        if (!has) onlyEightBallLeft = true;
    }
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

    // FINAL GATE: re-evaluate with spin already applied.
    // This catches any spin-induced trajectory difference between scan time and fire time.
    if (!isBreakPosition && g_CurrentCandidate.idx != -1) {
        ShotEvalResult ev = EvaluateShot(angle, power,
            g_CurrentCandidate.idx, myclass, onlyEightBallLeft, isNineBallGame, nominatedPocket);

        if (!ev.valid) {
            LOGI("AutoPlay::Shoot() — final EvaluateShot FAILED ball=%d angle=%.4f power=%.1f. Re-scanning.",
                 g_CurrentCandidate.idx, angle, power);
            g_CurrentCandidate.idx = -1;
            lastFailedCuePos = gPrediction->guiData.balls[0].initialPosition;
            state = IDLE; scan = FAST; g_autoPlayCalculating = false;
            return;
        }

        // Sync candidate pocket with what the simulation ACTUALLY produced.
        // This is the value that will be used for nomination and for the line display.
        g_CurrentCandidate.pocketIndex = ev.actualPocket;
        g_CurrentCandidate.idx         = ev.pottedBallIdx;
    } else if (!isBreakPosition) {
        // No candidate — at minimum guarantee cue ball safety.
        gPrediction->forceFullSimulation = true;
        gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
        gPrediction->forceFullSimulation = false;
        if (!gPrediction->guiData.balls[0].onTable) {
            g_CurrentCandidate.idx = -1;
            lastFailedCuePos = gPrediction->guiData.balls[0].initialPosition;
            state = IDLE; scan = FAST; g_autoPlayCalculating = false;
            return;
        }
    }

    bool nominating = false;
    int nominationMode = sharedGameManager.getPocketNominationMode();

    pendingShotPower = power;
    pendingShotAngle = angle;

    if ((nominationMode == 1 && myclass == Ball::Classification::EIGHT_BALL) ||
        (nominationMode == 2 && myclass != Ball::Classification::ANY)) {
        if (g_CurrentCandidate.idx != -1 &&
            sharedGameManager.getNominatedPocket() != (uint)g_CurrentCandidate.pocketIndex) {
            nominating = true;
        }
    }

    if (nominating) {
        state = NOMINATING;
        nominationFrameCounter = 0;
        humanNeedsNomination = (automationSpeed == SPEED_HUMAN && playStyle != STYLE_INSTANT);
        return;
    }

    // --- AUTO AIM MODE ---
    if (currentMode == MODE_AUTO_AIM) {
        applyAutoSpin();
        if (playStyle == STYLE_INSTANT) {
            setAimAngle(angle); setPower(power);
            bAimedThisTurn = true;
            lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
            g_postAimLock = true; g_postAimAngle = angle; g_postAimPower = power; g_postAimFrames = 20;
            ClearState(); state = IDLE;
        } else if (automationSpeed == SPEED_HUMAN) {
            humanShotLocked = true; state = EXECUTING; humanState = HUM_THINKING;
            stateStartTime = nowSec() + 0.5;
            startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
            targetAngle = angle; pendingShotPower = power;
        } else {
            setAimAngle(angle); setPower(power);
            bAimedThisTurn = true;
            lastCuePosWhenAimed = gPrediction->guiData.balls[0].initialPosition;
            g_postAimLock = true; g_postAimAngle = angle; g_postAimPower = power; g_postAimFrames = 20;
            ClearState(); state = IDLE;
        }
        return;
    }

    // --- AUTO PLAY MODE ---
    if (automationSpeed == SPEED_HUMAN || automationSpeed == SPEED_FAST) {
        applyAutoSpin();
        startAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        targetAngle = angle; pendingShotPower = power;
        g_PredictionLocked = true; bShowAutoPlayLines = false;

        gPrediction->forceFullSimulation = true;
        gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin(), g_CurrentCandidate);
        gPrediction->forceFullSimulation = false;

        if (playStyle == STYLE_INSTANT) {
            takeShot(angle, power); state = EXECUTING;
        } else if (automationSpeed == SPEED_HUMAN) {
            humanShotLocked = true; state = EXECUTING; humanState = HUM_THINKING;
            stateStartTime = nowSec() + 0.5;
        } else {
            takeShot(angle, power); state = EXECUTING;
        }
        return;
    }
}

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
    uint nominatedPocket = sharedGameManager.getNominatedPocket();

    bool onlyEightBallLeft = false;
    if (myclass == Ball::Classification::SOLID) {
        bool has = false;
        for (int k = 1; k < 8; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::STRIPE) {
        bool has = false;
        for (int k = 9; k <= 15; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::EIGHT_BALL) {
        onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::ANY) {
        bool has = false;
        for (int k = 1; k <= 15; k++) { if (k == 8) continue; if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; } }
        if (!has) onlyEightBallLeft = true;
    }
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

    int steps = 0;
    bool found = false;
    static Candidate bestCandidate = {-1, 0, 0, -1, 0, 0};
    static int bestScore = -1;

    if (currentScanAngle == 0.0 || !isScanningInProgress) bestScore = -1;

    while (steps < 1 && currentScanAngle < 2.0 * M_PI) {
        double angle = currentScanAngle;
        sweepAngle = angle;
        currentScanAngle += angleStep;
        steps++;

        std::vector<double> powers = {
            CalculateRequiredPower(150.0),
            CalculateRequiredPower(350.0),
            (double)powerMax
        };

        for (double power : powers) {
            // Apply spin before evaluating so the sim trajectory matches the real shot.
            applyAutoSpin();

            ShotEvalResult ev = EvaluateShot(angle, power,
                onlyEightBallLeft ? 8 : -1,   // targetBallIdx hint (-1 = any legal)
                myclass, onlyEightBallLeft, isNineBallGame, nominatedPocket);

            if (!ev.valid) continue;

            int currentScore = 100; // one legal ball potted
            if (currentScore <= bestScore) continue;

            bestScore = currentScore;
            bestCandidate = {ev.pottedBallIdx, angle, (double)currentScore, ev.actualPocket, power};
            if (cleanTableMode == CLEAN_YOUR_BALLS) { found = true; break; }
        }
        if (found) break;
    }

    if (found || currentScanAngle >= 2.0 * M_PI) {
        isScanningInProgress = false;
        currentScanAngle = 0.0;
        scan = FAST;
        g_autoPlayCalculating = false;
        bShowAutoPlayLines = false;
        if (bestScore != -1) {
            g_CurrentCandidate = bestCandidate;
            Shoot(bestCandidate.angle, bestCandidate.power);
        } else {
            LOGI("AutoPlay::ScanSlow() — no valid shot found. Marking position failed.");
            lastFailedCuePos = gPrediction->guiData.balls[0].initialPosition;
            state = IDLE;
            g_autoPlayCalculating = false;
        }
        bestScore = -1;
    }
}

void AutoPlay::ScanFast(double angleStep) {
    if (g_CurrentCandidate.idx != -1) return;

    bShowAutoPlayLines = !persistent_bool[O("bDisableFlicker")];
    static double fastSweepAngle = 0.0;

    Point2D scanCueBallPos = gPrediction->guiData.balls[0].initialPosition;
    Prediction::SceneData savedGuiData = gPrediction->guiData;

    double distSq = (scanCueBallPos - fs.scanCuePos).square();

    if (!fs.isInitiated || distSq > 0.0025) {
        fs.raw.clear();
        fs.evals.clear();
        fs.evalIndex = 0;
        fs.scanCuePos = scanCueBallPos;
        fs.isInitiated = true;
        fastSweepAngle = 0.0;

        if (automationSpeed == SPEED_HUMAN && humanShotLocked) return;
        if (currentMode == MODE_AUTO_AIM && bAimedThisTurn) return;

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);
        auto pockets = getPockets();

        bool onlyEightBallLeft = false;
        if (myclass == Ball::Classification::SOLID) {
            bool has = false;
            for (int k = 1; k < 8; k++) if (savedGuiData.balls[k].originalOnTable) { has = true; break; }
            if (!has) onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::STRIPE) {
            bool has = false;
            for (int k = 9; k <= 15; k++) if (savedGuiData.balls[k].originalOnTable) { has = true; break; }
            if (!has) onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::EIGHT_BALL) {
            onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::ANY) {
            bool has = false;
            for (int k = 1; k <= 15; k++) { if (k == 8) continue; if (savedGuiData.balls[k].originalOnTable) { has = true; break; } }
            if (!has) onlyEightBallLeft = true;
        }

        const auto& savedBalls = savedGuiData.balls;
        std::vector<Candidate> directRaw, specialRaw;

        bool bFoundLowest = false;
        for (int i = 1; i < savedGuiData.ballsCount; i++) {
            if (isNineBallGame && bFoundLowest) break;
            const auto& ball = savedBalls[i];
            if (!ball.originalOnTable) continue;
            if (!bFoundLowest) bFoundLowest = true;

            if (!isNineBallGame) {
                bool isACandidate = onlyEightBallLeft ? (i == 8) :
                    ((myclass == Ball::Classification::ANY)
                        ? (ball.classification != Ball::Classification::EIGHT_BALL)
                        : (ball.classification == myclass));
                if (!isACandidate) continue;
            }

            for (int pocketIdx = 0; pocketIdx < (int)pockets.size(); pocketIdx++) {
                if (nominatedPocket < 6 && pocketIdx != (int)nominatedPocket) continue;
                Point2D pocket = pockets[pocketIdx];
                Point2D toPocket = pocket - ball.initialPosition;
                double distTargetToPocket = sqrt(toPocket.square());
                if (distTargetToPocket < 0.1) continue;
                Point2D direction = toPocket * (1.0 / distTargetToPocket);
                Point2D ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);

                // 1. Direct Shot
                {
                    Point2D shotLine = ghostBallPos - scanCueBallPos;
                    double distCueToTarget = sqrt(shotLine.square());
                    double angle = atan2(shotLine.y, shotLine.x);
                    if (angle < 0) angle += 2 * M_PI;
                    double score = distCueToTarget + distTargetToPocket;
                    double power = CalculateRequiredPower(score);
                    directRaw.push_back({i, angle, score, pocketIdx, power, score});
                }

                // 2. Bank Shot
                if (bCushionShot) {
                    for (int side = 0; side < 4; side++) {
                        Point2D mp;
                        switch(side) {
                            case 0: mp = {pocket.x, -134.6 - pocket.y}; break;
                            case 1: mp = {pocket.x,  134.6 - pocket.y}; break;
                            case 2: mp = {-261.6 - pocket.x, pocket.y}; break;
                            case 3: mp = { 261.6 - pocket.x, pocket.y}; break;
                        }
                        Point2D toMir = mp - ball.initialPosition;
                        double distP = sqrt(toMir.square());
                        if (distP > 0.1) {
                            Point2D ghost = ball.initialPosition - (toMir * (1.0 / distP)) * (2.0 * BALL_RADIUS);
                            Point2D shot = ghost - scanCueBallPos;
                            double distC = sqrt(shot.square());
                            double angle = atan2(shot.y, shot.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double score = distC + distP + 100.0;
                            double power = CalculateRequiredPower(distC + distP) * 1.25;
                            if (power > (double)powerMax) power = (double)powerMax;
                            if (power < (double)powerMin) power = (double)powerMin;
                            specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                        }
                    }
                }

                // 3. Kick Shot
                if (bCushionShot) {
                    for (int side = 0; side < 4; side++) {
                        Point2D mg;
                        switch(side) {
                            case 0: mg = {ghostBallPos.x, -134.6 - ghostBallPos.y}; break;
                            case 1: mg = {ghostBallPos.x,  134.6 - ghostBallPos.y}; break;
                            case 2: mg = {-261.6 - ghostBallPos.x, ghostBallPos.y}; break;
                            case 3: mg = { 261.6 - ghostBallPos.x, ghostBallPos.y}; break;
                        }
                        Point2D shot = mg - scanCueBallPos;
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

                // 4. Combination Shot
                for (int j = 1; j < savedGuiData.ballsCount; j++) {
                    if (j == i) continue;
                    const auto& ballB = savedBalls[j];
                    if (!ballB.originalOnTable) continue;
                    bool isB_Valid = isNineBallGame ? true :
                        (!onlyEightBallLeft && ((myclass == Ball::Classification::ANY)
                            ? (ballB.classification != Ball::Classification::EIGHT_BALL)
                            : (ballB.classification == myclass)));
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
                    Point2D shotLine = ghostA - scanCueBallPos;
                    double distCueToA = sqrt(shotLine.square());
                    double angle = atan2(shotLine.y, shotLine.x);
                    if (angle < 0) angle += 2 * M_PI;
                    double score = distCueToA + distAToGhostB + distBToPocket + 80.0;
                    double power = CalculateRequiredPower(distCueToA + distAToGhostB + distBToPocket) * 1.1;
                    if (power > (double)powerMax) power = (double)powerMax;
                    if (power < (double)powerMin) power = (double)powerMin;
                    specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                }

                // 5. Kiss / Carom Shot
                for (int j = 1; j < savedGuiData.ballsCount; j++) {
                    if (j == i) continue;
                    const auto& ballB = savedBalls[j];
                    if (!ballB.originalOnTable) continue;
                    bool isB_Valid = isNineBallGame ? true :
                        (!onlyEightBallLeft && ((myclass == Ball::Classification::ANY)
                            ? (ballB.classification != Ball::Classification::EIGHT_BALL)
                            : (ballB.classification == myclass)));
                    if (!isB_Valid) continue;

                    Point2D toPocketB = pocket - ballB.initialPosition;
                    double distBToPocket = sqrt(toPocketB.square());
                    if (distBToPocket < 0.1) continue;
                    Point2D directionB = toPocketB * (1.0 / distBToPocket);
                    Point2D ghostB = ballB.initialPosition - directionB * (2.0 * BALL_RADIUS);
                    Point2D d = ghostB - ball.initialPosition;
                    double distD = sqrt(d.square());
                    if (distD < 2.0 * BALL_RADIUS) continue;
                    double ratio = (2.0 * BALL_RADIUS) / distD;
                    if (ratio > 1.0) ratio = 1.0;
                    double theta = acos(ratio);
                    double angleD = atan2(d.y, d.x);
                    for (int sign : {-1, 1}) {
                        double angleU = angleD + sign * theta;
                        Point2D u = {cos(angleU), sin(angleU)};
                        Point2D ghostA = ball.initialPosition + u * (2.0 * BALL_RADIUS);
                        Point2D shotLine = ghostA - scanCueBallPos;
                        double distCueToA = sqrt(shotLine.square());
                        double angle = atan2(shotLine.y, shotLine.x);
                        if (angle < 0) angle += 2 * M_PI;
                        double score = distCueToA + distD + distBToPocket + 120.0;
                        double power = CalculateRequiredPower(distCueToA + distD + distBToPocket) * 1.2;
                        if (power > (double)powerMax) power = (double)powerMax;
                        if (power < (double)powerMin) power = (double)powerMin;
                        specialRaw.push_back({i, angle, score, pocketIdx, power, score});
                    }
                }
            }
        }

        fs.raw.clear();
        fs.raw.insert(fs.raw.end(), directRaw.begin(), directRaw.end());
        fs.raw.insert(fs.raw.end(), specialRaw.begin(), specialRaw.end());
        std::sort(fs.raw.begin(), fs.raw.end(), [](const Candidate& a, const Candidate& b) { return a.score < b.score; });
        if (fs.raw.size() > 60) fs.raw.resize(60);
        std::sort(fs.raw.begin(), fs.raw.end(), [](const Candidate& a, const Candidate& b) { return a.angle < b.angle; });
    }

    if (automationSpeed == SPEED_HUMAN && humanShotLocked) return;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();
    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

    bool onlyEightBallLeft = false;
    if (myclass == Ball::Classification::SOLID) {
        bool has = false;
        for (int k = 1; k < 8; k++) if (savedGuiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::STRIPE) {
        bool has = false;
        for (int k = 9; k <= 15; k++) if (savedGuiData.balls[k].originalOnTable) { has = true; break; }
        if (!has) onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::EIGHT_BALL) {
        onlyEightBallLeft = true;
    } else if (myclass == Ball::Classification::ANY) {
        bool has = false;
        for (int k = 1; k <= 15; k++) { if (k == 8) continue; if (savedGuiData.balls[k].originalOnTable) { has = true; break; } }
        if (!has) onlyEightBallLeft = true;
    }

    while (fs.evalIndex < fs.raw.size()) {
        auto raw = fs.raw[fs.evalIndex++];

        double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(raw.angle));

        // Apply spin so evaluation matches the real shot trajectory.
        applyAutoSpin();

        double testPower = (automationSpeed == SPEED_FAST) ? (double)powerMax : raw.power;

        ShotEvalResult ev = EvaluateShot(angle, testPower,
            raw.idx, myclass, onlyEightBallLeft, isNineBallGame, nominatedPocket);

        // Fallback to calculated power if powerMax failed in fast mode.
        if (!ev.valid && automationSpeed == SPEED_FAST && raw.power != testPower) {
            applyAutoSpin();
            ev = EvaluateShot(angle, raw.power,
                raw.idx, myclass, onlyEightBallLeft, isNineBallGame, nominatedPocket);
            if (ev.valid) testPower = raw.power;
        }

        if (!ev.valid) continue;

        // Build confirmed candidate using ACTUAL simulated pocket (not geometric intended pocket).
        Candidate cf = raw;
        cf.idx        = ev.pottedBallIdx;
        cf.pocketIndex = ev.actualPocket;
        cf.power       = testPower;

        // GENIUS MODE: fire immediately (no nomination needed path only).
        if (!isNineBallGame && (cleanTableMode == CLEAN_OFF || onlyEightBallLeft)) {
            if (NeedsNomination(cf)) {
                fs.evals.push_back({cf, 1, 1, false});
                continue;
            }
            fs.isInitiated = false;
            g_CurrentCandidate = cf;
            gPrediction->guiData = savedGuiData;
            Shoot(cf.angle, cf.power);
            return;
        }

        // Count own/total potted for scoring (clean table modes).
        int totalPotted = 0, ownPotted = 0;
        bool pots9 = false;
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable || ball.onTable) continue;
            totalPotted++;
            if (i == 9) pots9 = true;
            bool match = (myclass == Ball::Classification::ANY)
                ? (ball.classification != Ball::Classification::CUE_BALL &&
                   ball.classification != Ball::Classification::EIGHT_BALL)
                : (ball.classification == myclass);
            if (match || (onlyEightBallLeft && i == 8)) ownPotted++;
        }

        fs.evals.push_back({cf, totalPotted, ownPotted, pots9});
        break; // one valid eval per frame
    }

    gPrediction->guiData = savedGuiData;

    if (fs.evalIndex >= fs.raw.size()) {
        FastScanState::Eval* best = nullptr;

        if (isNineBallGame) {
            switch (nineBallStrategy) {
                case NINEBALL_BEST_SHOT:
                    for (auto& ev : fs.evals) { if (!best || ev.tot > best->tot) best = &ev; }
                    break;
                case NINEBALL_SNIPE_9:
                    for (auto& ev : fs.evals) {
                        if (!best) best = &ev;
                        else if (ev.p9 && !best->p9) best = &ev;
                        else if (ev.p9 == best->p9 && ev.tot > best->tot) best = &ev;
                    }
                    break;
                default:
                    if (!fs.evals.empty()) best = &fs.evals[0];
            }
        } else {
            if (onlyEightBallLeft) {
                for (auto& ev : fs.evals) { if (ev.c.idx == 8) { best = &ev; break; } }
                if (!best && !fs.evals.empty()) best = &fs.evals[0];
            } else {
                switch (cleanTableMode) {
                    case CLEAN_OFF:
                        if (!fs.evals.empty()) best = &fs.evals[0]; break;
                    case CLEAN_ALL_BALLS:
                        for (auto& ev : fs.evals) { if (!best || ev.tot > best->tot) best = &ev; } break;
                    case CLEAN_YOUR_BALLS:
                        for (auto& ev : fs.evals) { if (!best || ev.own > best->own) best = &ev; } break;
                }
            }
        }

        if (best) {
            fs.isInitiated = false;
            g_CurrentCandidate = best->c;
            Shoot(best->c.angle, best->c.power);
        } else {
            fs.isInitiated = false;
            lastFailedCuePos = scanCueBallPos;
            scan = SLOW;
            g_autoPlayCalculating = true;
        }
    }

    if (fs.evalIndex < fs.raw.size()) {
        fastSweepAngle = normalizeAngle(fastSweepAngle + 0.15);
        if (!persistent_bool[O("bDisableFlicker")]) {
            gPrediction->forceFullSimulation = true;
            gPrediction->determineShotResult(true, fastSweepAngle, 400.0f, sharedGameManager.getShotSpin());
            gPrediction->forceFullSimulation = false;
        }
        sweepAngle = fastSweepAngle;
    }
}

void AutoPlay::Update() {
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
            if (balls && balls.Count > 0) { currentCuePos = balls[0].position(); hasCueBall = true; }
        }
    }
    if (hasCueBall) {
        if (lastFrameCuePos.x == -1000.0) lastFrameCuePos = currentCuePos;
        double dx = currentCuePos.x - lastFrameCuePos.x;
        double dy = currentCuePos.y - lastFrameCuePos.y;
        if (dx*dx + dy*dy > 0.0001) framesCueBallStill = 0;
        else if (framesCueBallStill < 10) framesCueBallStill++;
        lastFrameCuePos = currentCuePos;
    } else {
        framesCueBallStill = 10;
        lastFrameCuePos = {-1000.0, -1000.0};
    }
    bCueBallIsMovingOrDragging = (framesCueBallStill < 5);

    if (g_postShotLock) {
        if (g_postShotFrames > 0 && sharedGameManager) {
            setAimAngle(g_postShotAngle); setPower(g_postShotPower); g_postShotFrames--;
        } else { g_postShotLock = false; ClearState(); }
        g_autoPlayCalculating = false; return;
    }

    if (g_postAimLock) {
        if (g_postAimFrames > 0 && sharedGameManager) {
            setAimAngle(g_postAimAngle); setPower(g_postAimPower); g_postAimFrames--;
        } else { g_postAimLock = false; ClearState(); }
        g_autoPlayCalculating = false; return;
    }

    bool humanRunning = (automationSpeed == SPEED_HUMAN && (humanState != HUM_IDLE || humanShotLocked));
    bool executingShot = anim_IsPulling || humanRunning;

    if (AreBallsMoving() && !executingShot) {
        if (state == SCANNING || state == NOMINATING) { ClearState(); state = IDLE; }
        g_autoPlayCalculating = false; return;
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
        float jX = Width * 0.83f, jY = Height * 0.82f, jR = 65.0f;
        double now_anim = nowSec();
        double elapsed = now_anim - stateStartTime;

        const double t1_pullback = 0.20, t2_sweep = 0.75, t3_correct = 1.00,
                     t4_adjust = 1.40, t5_hold = 1.60;

        if (fastShotState == 0) {
            if (playStyle == STYLE_INSTANT) {
                setAimAngle(anim_TargetAngle);
                NativeTouchesBegin(5, jX, jY);
                NativeTouchesMove(5, jX + (float)cos(anim_TargetAngle)*jR,
                                     jY + (float)sin(anim_TargetAngle)*jR);
                anim_RotationDone = true; anim_TouchStarted = true;
                stateStartTime = nowSec(); fastShotState = 1; return;
            }

            double ns = normalizeAngle(startAngle), nt = normalizeAngle(anim_TargetAngle);
            double delta = nt - ns;
            if (delta > M_PI) delta -= 2.0*M_PI;
            if (delta < -M_PI) delta += 2.0*M_PI;
            double dir = (delta > 0) ? 1.0 : -1.0;
            double opp  = ns - dir*(30.0*M_PI/180.0);
            double over = nt + dir*(20.0*M_PI/180.0);
            double nudge = nt - dir*(1.5*M_PI/180.0);
            double curAngle = nt;

            if (elapsed < t1_pullback) {
                double t = 1.0 - pow(1.0 - elapsed/t1_pullback, 3.0);
                curAngle = ns + (opp - ns)*t;
                if (!anim_TouchStarted) { anim_TouchStarted = true; NativeTouchesBegin(5, jX, jY); }
            } else if (elapsed < t2_sweep) {
                double t = (elapsed-t1_pullback)/(t2_sweep-t1_pullback); t = t*t*(3.0-2.0*t);
                curAngle = opp + (over - opp)*t;
            } else if (elapsed < t3_correct) {
                double t = (elapsed-t2_sweep)/(t3_correct-t2_sweep); t = t*t*(3.0-2.0*t);
                curAngle = over + (nudge - over)*t;
            } else if (elapsed < t4_adjust) {
                double t = sin((elapsed-t3_correct)/(t4_adjust-t3_correct)*M_PI_2);
                curAngle = nudge + (nt - nudge)*t;
            } else if (elapsed < t5_hold) {
                curAngle = nt;
                if (!anim_RotationDone && elapsed > t5_hold - 0.05) {
                    anim_RotationDone = true; setAimAngle(anim_TargetAngle);
                }
            }

            if (elapsed < t5_hold) {
                setAimAngle(curAngle);
                NativeTouchesMove(5, jX+(float)cos(curAngle)*jR, jY+(float)sin(curAngle)*jR);
                return;
            }

            setAimAngle(anim_TargetAngle);
            NativeTouchesMove(5, jX+(float)cos(anim_TargetAngle)*jR, jY+(float)sin(anim_TargetAngle)*jR);
            stateStartTime = nowSec(); fastShotState = 1; return;
        }

        setAimAngle(anim_TargetAngle);
        double elapsed_shot = nowSec() - stateStartTime;

        if (fastShotState == 1) {
            NativeTouchesMove(5, jX+(float)cos(anim_TargetAngle)*jR, jY+(float)sin(anim_TargetAngle)*jR);
            setAimAngle(anim_TargetAngle);
            if (playStyle == STYLE_INSTANT || elapsed_shot >= 0.15) {
                NativeTouchesEnd(5, jX+(float)cos(anim_TargetAngle)*jR, jY+(float)sin(anim_TargetAngle)*jR);
                float sXPct = persistent_float[O("fPowerBarXPercent")];
                float sX = Width * sXPct;
                if (persistent_int[O("iPowerBarSide")] == 1) sX = Width*(1.0f-sXPct);
                float sYS = Height*persistent_float[O("fPowerBarYStartPercent")];
                float sYE = Height*persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sX-20.0f, sYS, 40.0f, sYE-sYS);
                if (playStyle == STYLE_INSTANT)
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.40f, 0.20f);
                else
                    powerSlider.SimulateDrag(sliderRect, anim_TargetPower, 0.85f, 0.40f);
                stateStartTime = nowSec(); fastShotState = 2;
            }
            return;
        }

        if (fastShotState == 2) {
            gPrediction->forceFullSimulation = true;
            gPrediction->determineShotResult(true, anim_TargetAngle, anim_TargetPower,
                                             sharedGameManager.getShotSpin(), g_CurrentCandidate);
            gPrediction->forceFullSimulation = false;
            if (powerSlider.Active) return;
            stateStartTime = nowSec(); fastShotState = 3; return;
        }

        if (fastShotState == 3) {
            setAimAngle(anim_TargetAngle);
            static double s_ballsStoppedAt = -1.0;
            if (s_ballsStoppedAt < stateStartTime) s_ballsStoppedAt = stateStartTime;
            bool timedOut = (nowSec() - stateStartTime > 12.0);
            if (AreBallsMoving() && !timedOut) { s_ballsStoppedAt = nowSec(); return; }
            if (nowSec() - s_ballsStoppedAt < 0.5 && !timedOut) return;
            s_ballsStoppedAt = -1.0;
            anim_IsPulling = false; anim_RotationDone = false; anim_TouchStarted = false;
            fastShotState = 0; ClearState(); state = IDLE; g_lastFastShotTime = nowSec(); return;
        }
    }

    if (persistent_bool.count(O("bPocketTargetVisual")) == 0 || persistent_bool[O("bPocketTargetVisual")]) {
        int nomPocket = sharedGameManager.getNominatedPocket();
        if (nomPocket >= 0 && nomPocket < 6) {
            ImVec2 pktPos = GetPocketScreenPos(nomPocket);
            ImDrawList* fg = ImGui::GetBackgroundDrawList();
            float pulse = (sin(ImGui::GetTime()*8.0f)+1.0f)*0.5f;
            float r = 35.0f + pulse*8.0f;
            fg->AddCircleFilled(pktPos, r, IM_COL32(255,120,0,70));
            fg->AddCircle(pktPos, r, IM_COL32(255,200,0,255), 0, 3.5f);
            fg->AddLine({pktPos.x-18,pktPos.y},{pktPos.x+18,pktPos.y},IM_COL32(255,255,255,180),2.5f);
            fg->AddLine({pktPos.x,pktPos.y-18},{pktPos.x,pktPos.y+18},IM_COL32(255,255,255,180),2.5f);
        }
    }

    static bool wasPlayerTurn = false;
    bool isPlayerTurn = sharedGameManager.mStateManager().isPlayerTurn();
    if (isPlayerTurn && bAutoSpin) applyAutoSpin();

    bool turnJustStarted = !wasPlayerTurn && isPlayerTurn;
    if (wasPlayerTurn && !isPlayerTurn) { g_autoPlayCalculating = false; ClearState(); bAimedThisTurn = false; }
    if (turnJustStarted) { bAimedThisTurn = false; lastFailedCuePos = {-1000.0,-1000.0}; }
    wasPlayerTurn = isPlayerTurn;

    static double turnStartTime = 0.0;
    if (turnJustStarted || (isPlayerTurn && turnStartTime == 0.0)) turnStartTime = nowSec();
    if (!isPlayerTurn) turnStartTime = 0.0;

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
        if (++animationStuckCounter < 200) { g_autoPlayCalculating = false; return; }
    } else animationStuckCounter = 0;

    if (nowSec() - g_lastFastShotTime < 2.5) { g_autoPlayCalculating = false; return; }
    if (AutoPlay::nowSec() < g_shotCooldownEnd) { g_autoPlayCalculating = false; return; }

    static double lastStateChangeTime = 0;
    static State lastState = IDLE;
    if (state != lastState) { lastState = state; lastStateChangeTime = AutoPlay::nowSec(); }
    else if (state != IDLE && AutoPlay::nowSec() - lastStateChangeTime > 10.0) {
        LOGI("AutoPlay::Update() — state %d timed out. Resetting.", (int)state);
        if (gPrediction && gPrediction->guiData.ballsCount > 0)
            lastFailedCuePos = gPrediction->guiData.balls[0].initialPosition;
        ClearState(); return;
    }

    if (turnJustStarted && bAutoPlaying) { state = IDLE; scan = FAST; currentScanAngle = 0.0; }

    // =====================================================================
    // HUMAN STATE MACHINE
    // =====================================================================
    if (automationSpeed == SPEED_HUMAN && humanState != HUM_IDLE) {
        if (state == NOMINATING_HUMAN) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 15) buttonClicker.Click(GetPocketScreenPos(humanNominationPocket));
            if (nominationFrameCounter > 35 && !buttonClicker.Active) {
                humanState = HUM_THINKING; stateStartTime = nowSec()+0.35;
                state = EXECUTING; humanNeedsNomination = false;
            }
            return;
        }

        double now = nowSec();
        auto UpdateJoystick = [&](double angle) {
            float jX=Width*0.83f, jY=Height*0.82f, jR=65.0f;
            NativeTouchesMove(5, jX+cos(angle)*jR, jY+sin(angle)*jR);
        };

        if (humanState == HUM_THINKING) {
            if (now >= stateStartTime) {
                overshootOffset = (gen()%2==0?1:-1)*0.058;
                currentOvershootTarget = targetAngle + overshootOffset;
                stateStartTime = now; humanState = HUM_OVERSHOOTING;
                NativeTouchesBegin(5, Width*0.83f, Height*0.82f);
            }
            return;
        }

        if (humanState == HUM_OVERSHOOTING) {
            double t = (now-stateStartTime)/1.1;
            if (t >= 1.0) {
                setAimAngle(currentOvershootTarget); UpdateJoystick(currentOvershootTarget);
                stateStartTime = now; humanState = HUM_CORRECTING;
            } else {
                double ease = EaseInOutCubic(t);
                double ns=normalizeAngle(startAngle), nt=normalizeAngle(currentOvershootTarget);
                double d=nt-ns; if(d>M_PI)d-=2*M_PI; if(d<-M_PI)d+=2*M_PI;
                double cur=ns+d*ease; setAimAngle(cur); UpdateJoystick(cur);
            }
            gPrediction->forceFullSimulation=true;
            gPrediction->determineShotResult(true,targetAngle,pendingShotPower,sharedGameManager.getShotSpin(),g_CurrentCandidate);
            gPrediction->forceFullSimulation=false;
            return;
        }

        if (humanState == HUM_CORRECTING) {
            double t=(now-stateStartTime)/0.35;
            double dirSign=(overshootOffset>0)?1.0:-1.0;
            double nudge=targetAngle+dirSign*(1.5*M_PI/180.0);
            if (t >= 1.0) {
                setAimAngle(nudge); UpdateJoystick(nudge);
                stateStartTime=now; humanState=HUM_HOLDING;
            } else {
                double ease=EaseInOutCubic(t);
                double ns=normalizeAngle(currentOvershootTarget), nt=normalizeAngle(nudge);
                double d=nt-ns; if(d>M_PI)d-=2*M_PI; if(d<-M_PI)d+=2*M_PI;
                double cur=ns+d*ease; setAimAngle(cur); UpdateJoystick(cur);
            }
            gPrediction->forceFullSimulation=true;
            gPrediction->determineShotResult(true,targetAngle,pendingShotPower,sharedGameManager.getShotSpin(),g_CurrentCandidate);
            gPrediction->forceFullSimulation=false;
            return;
        }

        if (humanState == HUM_HOLDING) {
            double t=(now-stateStartTime)/0.40;
            double dirSign=(overshootOffset>0)?1.0:-1.0;
            double nudge=targetAngle+dirSign*(1.5*M_PI/180.0);
            if (t >= 1.0) {
                setAimAngle(targetAngle); UpdateJoystick(targetAngle);
                float jX=Width*0.83f, jY=Height*0.82f, jR=65.0f;
                NativeTouchesMove(5, jX+(float)cos(targetAngle)*jR, jY+(float)sin(targetAngle)*jR);
                stateStartTime=now; humanState=HUM_STABILIZING;
            } else {
                double ease=sin(t*M_PI_2);
                double ns=normalizeAngle(nudge), nt=normalizeAngle(targetAngle);
                double d=nt-ns; if(d>M_PI)d-=2*M_PI; if(d<-M_PI)d+=2*M_PI;
                double cur=ns+d*ease; setAimAngle(cur); UpdateJoystick(cur);
            }
            gPrediction->forceFullSimulation=true;
            gPrediction->determineShotResult(true,targetAngle,pendingShotPower,sharedGameManager.getShotSpin(),g_CurrentCandidate);
            gPrediction->forceFullSimulation=false;
            return;
        }

        if (humanState == HUM_STABILIZING) {
            float jX=Width*0.83f, jY=Height*0.82f, jR=65.0f;
            NativeTouchesMove(5, jX+(float)cos(targetAngle)*jR, jY+(float)sin(targetAngle)*jR);
            setAimAngle(targetAngle);
            if (now-stateStartTime >= 0.4) {
                if (currentMode == MODE_AUTO_PLAY) {
                    NativeTouchesEnd(5, jX+(float)cos(targetAngle)*jR, jY+(float)sin(targetAngle)*jR);
                    stateStartTime=now; startPower=getCurrentPower(); targetPower=pendingShotPower;
                    humanState=HUM_PULLING;
                } else {
                    NativeTouchesEnd(5, jX+(float)cos(targetAngle)*jR, jY+(float)sin(targetAngle)*jR);
                    bAimedThisTurn=true;
                    lastCuePosWhenAimed=gPrediction->guiData.balls[0].initialPosition;
                    g_postAimLock=true; g_postAimAngle=targetAngle;
                    g_postAimPower=pendingShotPower; g_postAimFrames=20;
                    state=IDLE; humanState=HUM_IDLE;
                }
            }
            return;
        }

        if (humanState == HUM_PULLING) {
            setAimAngle(targetAngle);
            if (!powerSlider.Active) {
                float sXPct=persistent_float[O("fPowerBarXPercent")];
                float sX=Width*sXPct;
                if (persistent_int[O("iPowerBarSide")]==1) sX=Width*(1.0f-sXPct);
                float sYS=Height*persistent_float[O("fPowerBarYStartPercent")];
                float sYE=Height*persistent_float[O("fPowerBarYEndPercent")];
                ImVec4 sliderRect(sX-20.0f,sYS,40.0f,sYE-sYS);
                powerSlider.SimulateDrag(sliderRect, targetPower, 0.85f, 0.4f);
            }
            gPrediction->forceFullSimulation=true;
            gPrediction->determineShotResult(true,targetAngle,targetPower,sharedGameManager.getShotSpin(),g_CurrentCandidate);
            gPrediction->forceFullSimulation=false;
            if (powerSlider.Active) return;
            stateStartTime=now; humanState=HUM_DELAY_BEFORE_SHOT; return;
        }

        if (humanState == HUM_DELAY_BEFORE_SHOT) {
            setAimAngle(targetAngle);
            if (now-stateStartTime >= 0.4) {
                humanShotLocked=false; ClearState(); state=IDLE; humanState=HUM_IDLE;
            }
            return;
        }
    }

    if (!bAutoPlaying || !isPlayerTurn) {
        if (humanShotLocked || anim_IsPulling || state==SCANNING || state==NOMINATING) {
            if (humanState==HUM_OVERSHOOTING || humanState==HUM_CORRECTING ||
                humanState==HUM_HOLDING || humanState==HUM_STABILIZING)
                NativeTouchesEnd(5, Width*0.83f, Height*0.82f);
            if (powerSlider.Active) {
                float sXPct=persistent_float[O("fPowerBarXPercent")];
                float sX=Width*sXPct;
                if (persistent_int[O("iPowerBarSide")]==1) sX=Width*(1.0f-sXPct);
                NativeTouchesEnd(powerSlider.TouchIndex, sX, Height*persistent_float[O("fPowerBarYStartPercent")]);
                powerSlider.Active=false; powerSlider.state=PowerSlider::IDLE;
            }
            if (sharedGameManager) {
                double cur=sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
                sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(cur);
            }
            gPrediction->forceFullSimulation=false;
            humanShotLocked=false; anim_IsPulling=false;
            fastShotState=0; humanState=HUM_IDLE;
            ClearState(); state=IDLE; g_autoPlayCalculating=false;
        }
        g_autoPlayCalculating=false; return;
    }

    if (currentMode==MODE_AUTO_AIM && bAimedThisTurn && sharedGameManager) {
        auto& cueBall=gPrediction->guiData.balls[0];
        if ((cueBall.initialPosition-lastCuePosWhenAimed).square() > 0.0025) {
            bAimedThisTurn=false; lastFailedCuePos={-1000.0,-1000.0}; state=IDLE;
        }
    }

    if (state==IDLE) {
        bool shouldScan=(currentMode!=MODE_AUTO_AIM)||!bAimedThisTurn;
        if (shouldScan && sharedGameManager) {
            auto& cueBall=gPrediction->guiData.balls[0];
            if ((cueBall.initialPosition-lastFailedCuePos).square()<=0.0025) shouldScan=false;
        }
        if (shouldScan) { state=SCANNING; scan=FAST; g_autoPlayCalculating=false; }
    }

    if (state==SCANNING) {
        if (scan==FAST) ScanFast();
        if (scan==SLOW) {
            g_autoPlayCalculating=true;
            float level=persistent_float.count(O("fScannerLevel"))?persistent_float[O("fScannerLevel")]:50.0f;
            ScanSlow(0.005+(double(level)/100.0)*0.035);
        }
    }

    if (state==NOMINATING) {
        setAimAngle(pendingShotAngle);
        nominationFrameCounter++;
        if (nominationFrameCounter==10)
            buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));

        if (nominationFrameCounter>20 && !buttonClicker.Active) {
            uint nominatedPocket=sharedGameManager.getNominatedPocket();
            if (nominatedPocket==(uint)g_CurrentCandidate.pocketIndex) {
                targetAngle=pendingShotAngle; g_PredictionLocked=true;

                // Re-evaluate with spin after nomination is confirmed.
                Ball::Classification myclass=sharedGameManager.getPlayerClassification();
                bool oEBL=(myclass==Ball::Classification::EIGHT_BALL);
                if (!oEBL) {
                    if (myclass==Ball::Classification::SOLID){bool h=false;for(int k=1;k<8;k++)if(gPrediction->guiData.balls[k].originalOnTable){h=true;break;}if(!h)oEBL=true;}
                    else if(myclass==Ball::Classification::STRIPE){bool h=false;for(int k=9;k<=15;k++)if(gPrediction->guiData.balls[k].originalOnTable){h=true;break;}if(!h)oEBL=true;}
                    else if(myclass==Ball::Classification::ANY){bool h=false;for(int k=1;k<=15;k++){if(k==8)continue;if(gPrediction->guiData.balls[k].originalOnTable){h=true;break;}}if(!h)oEBL=true;}
                }
                bool isNine=(myclass==Ball::Classification::NINE_BALL_RULE);

                applyAutoSpin();
                ShotEvalResult ev=EvaluateShot(pendingShotAngle,pendingShotPower,
                    g_CurrentCandidate.idx,myclass,oEBL,isNine,nominatedPocket);

                if (!ev.valid) {
                    LOGI("AutoPlay NOMINATING — post-nomination eval FAILED. Re-scanning.");
                    g_CurrentCandidate.idx=-1;
                    lastFailedCuePos=gPrediction->guiData.balls[0].initialPosition;
                    state=IDLE; scan=FAST; nominationFrameCounter=0;
                    g_PredictionLocked=false; g_autoPlayCalculating=false; return;
                }

                // Sync from simulation result.
                g_CurrentCandidate.idx        = ev.pottedBallIdx;
                g_CurrentCandidate.pocketIndex = ev.actualPocket;

                if (currentMode==MODE_AUTO_AIM) {
                    applyAutoSpin(); bAimedThisTurn=true;
                    lastCuePosWhenAimed=gPrediction->guiData.balls[0].initialPosition;
                    g_postAimLock=true; g_postAimAngle=pendingShotAngle;
                    g_postAimPower=pendingShotPower; g_postAimFrames=20;
                    ClearState(); state=IDLE;
                } else if (automationSpeed==SPEED_HUMAN && playStyle!=STYLE_INSTANT) {
                    applyAutoSpin(); humanShotLocked=true; humanState=HUM_THINKING;
                    stateStartTime=nowSec()+0.3; startAngle=pendingShotAngle; state=EXECUTING;
                } else {
                    startAngle=pendingShotAngle;
                    takeShot(pendingShotAngle,pendingShotPower,true); state=EXECUTING;
                }
            } else {
                if (nominationFrameCounter>40) {
                    LOGI("AutoPlay NOMINATING — pocket %d not confirmed. Re-scanning.",g_CurrentCandidate.pocketIndex);
                    g_CurrentCandidate.idx=-1; state=IDLE; scan=FAST;
                    nominationFrameCounter=0; g_PredictionLocked=false; g_autoPlayCalculating=false;
                }
            }
        }
    }

    if (state==WAITING_FOR_USER_POCKET) {
        setAimAngle(pendingShotAngle); setPower(pendingShotPower);
        int currentNom=sharedGameManager.getNominatedPocket();
        if (currentNom==g_CurrentCandidate.pocketIndex && currentNom<6) {
            takeShot(pendingShotAngle,pendingShotPower); ClearState(); state=IDLE;
        }
    }

    if (bShowAutoPlayLines && isPlayerTurn &&
        state!=EXECUTING && state!=NOMINATING &&
        state!=WAITING_FOR_USER_POCKET && state!=SCANNING &&
        !g_autoPlayCalculating && g_CurrentCandidate.idx==-1) {
        double curAngle=sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        double curPower=getCurrentPower();
        if (curPower<100.0) curPower=800.0;
        gPrediction->forceFullSimulation=true;
        gPrediction->determineShotResult(true,curAngle,curPower,sharedGameManager.getShotSpin());
        gPrediction->forceFullSimulation=false;
    }
}

bool AutoPlay::AreBallsMoving() {
    if (!sharedGameManager) return false;
    Table table=sharedGameManager.mTable;
    if (!table) return false;
    auto& balls=table.mBalls();
    if (!balls) return false;
    for (int i=0; i<balls.Count; i++) {
        Ball ball=balls[i];
        if (ball && ball.isOnTable()) {
            auto vel=ball.velocity();
            if (vel.x*vel.x+vel.y*vel.y>0.000001) return true;
            auto spin=ball.spin();
            if (spin.x*spin.x+spin.y*spin.y+spin.z*spin.z>0.000001) return true;
        }
    }
    return false;
}

bool isTouchLockedByBot() {
    return (AutoPlay::g_PredictionLocked && AutoPlay::g_CurrentCandidate.idx!=-1) ||
           (AutoPlay::state==AutoPlay::NOMINATING);
}