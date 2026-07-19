#pragma once

#include "Prediction.fastt.h"
#include <imgui/imgui.h>
#include <algorithm>

#include "ScreenTable.h"

// #include "PowerSlider.h"
#include "ButtonClicker.h"

using namespace ImGui;

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

bool IsShotValid() {
    auto& cand = g_CurrentCandidate;
    if (cand.idx == -1) return false;

    Ball::Classification myclass = sharedGameManager.getPlayerClassification();

    uint nominatedPocket = sharedGameManager.getNominatedPocket();
    if (nominatedPocket < 6 && cand.pocketIndex != nominatedPocket) return false;

    if (!gPrediction->guiData.balls[0].onTable) return false; // cue ball should not be pocketed
    if (!gPrediction->guiData.balls[cand.idx].originalOnTable) return false; // target ball was already potted
    if (gPrediction->guiData.balls[cand.idx].onTable) return false; // target ball was not potted
    if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) return false; // target ball did not go into target pocket

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
namespace AutoPlay {
    double lastSetAngle = 0.f;
    bool didSetAngle = false;
    bool bAutoPlaying = false;

    // ── Power / shot type settings ──────────────────────────────────────────
    static inline double powerMax = 666.0;
    static inline double powerMin = 80.0;
    static inline bool   bCushionShot = true; // enable Bank/Kick/Combo/Kiss shots

    // Helper: minimum velocity to cover total distance (physics: v=sqrt(2*a*s), a=196)
    static double CalculateRequiredPower(double totalDist) {
        double p = sqrt(totalDist * 2.0 * 196.0);
        if (p < powerMin) p = powerMin;
        if (p > powerMax) p = powerMax;
        return p;
    }

    // ── FastScanState: per-frame candidate evaluation (lag-free) ─────────────
    struct FastScanStateLocal {
        struct Eval { Candidate c; int tot; int own; bool p9; };
        std::vector<Candidate> raw;
        std::vector<Eval> evals;
        size_t evalIndex = 0;
        bool isInitiated = false;
        Point2D scanCuePos = {-9999, -9999};
    } fs;

    enum State {
        IDLE,           // Waiting for player turn or Autoplay to be enabled
        SCANNING,       // Searching for the best shot candidate (calculating physics)
        NOMINATING,     // Waiting for pocket nomination click to finish
        EXECUTING,      // Setting the angle and spin (waiting for visual cue to update)
    } state = IDLE;
    
    double pendingShotPower = 0.f;
    double pendingShotAngle = 0.f;
    int nominationFrameCounter = 0;
    
    enum ScanMode {
        FAST,
        SLOW,
    } scan = FAST;

    bool shouldAutoPlay() { return !didSetAngle || lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(); }

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    void takeShot(double angle, double power) {
        setAimAngle(angle);
        gPrediction->determineShotResult(false, angle, power);

        sharedGameManager.mVisualCue().mPower(ShotPowerToPower(power));
        M(void, libmain + 0x2dc0c58, void*)(F(void*, sharedGameManager + 0x3b0));
    }
    
    void ClearState() {
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
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
        } else {
            takeShot(angle, power);
            ClearState();
            state = IDLE;
        }
    }
    
    void ScanSlow(double angleStep = 0.01f) {
        static double currentScanAngle = 0.0;
        static bool isScanning = false;
        static Point2D lastScanCuePos = { -1000.0, -1000.0 };

        if (g_CurrentCandidate.idx != -1) return;
        
        // Reset if we just started or wrapped around, or if table changed
        if (!isScanning || gPrediction->guiData.balls[0].initialPosition != lastScanCuePos) {
            currentScanAngle = 0.0;
            isScanning = true;
            lastScanCuePos = gPrediction->guiData.balls[0].initialPosition;
        }

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        auto& cueBall = gPrediction->guiData.balls[0];
        
        int steps = 0;
        bool foundShot = false;
        
        // Scan 10 angles per frame
        while (steps < 20 && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            // Power sweep ringan: 4 nilai cukup untuk cover range
            std::vector<double> powers = {566.0, 400.0, 250.0, 120.0};
            for (double power : powers) {
                gPrediction->forceFullSimulation = true;
                gPrediction->determineShotResult(true, angle, power, sharedGameManager.getShotSpin());
                gPrediction->forceFullSimulation = false;
                
                bool isPotentiallyValid = false;
                int targetIdx = -1;

                bool bFoundLowestNumberedBall = false;
                int iFoundLowestNumberedBall = -1;
                bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;

                if (isNineBallGame) {
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        auto& ball = gPrediction->guiData.balls[i];
                        if (!ball.originalOnTable) continue; // skip already potted

                        bFoundLowestNumberedBall = true;
                        iFoundLowestNumberedBall = i;
                        break;
                    }

                    auto firstHit = gPrediction->guiData.collision.firstHitBall;
                    if (!firstHit) continue;
                    
                    // Must hit lowest numbered ball first
                    if (firstHit->index != iFoundLowestNumberedBall) continue;

                    // Cue ball must stay on table
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
                    targetIdx = bestPottedIdx;

                    LOGI("AutoPlay: 9ball: Found good angle %f with power %f", angle, power);
                    
                    g_CurrentCandidate.idx = targetIdx;
                    g_CurrentCandidate.angle = angle;
                    g_CurrentCandidate.power = power;
                    g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[targetIdx].pocketIndex;

                    foundShot = true;
                    Shoot(angle, power);
                    break;
                }

                // Check if ANY valid ball is potted
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    auto& ball = gPrediction->guiData.balls[i];
                    if (ball.originalOnTable && !ball.onTable) { // Ball was potted
                        bool isValidTarget = false;
                        // Logic for valid target (Simplified)
                        // If table is open (ANY), any ball except 8-ball and Cue-ball is valid.
                        // If class is assigned, only that class is valid.
                        // Note: If on 8-ball, usually class logic might differ, but assuming standard flow:
                        
                        if (myclass == Ball::Classification::ANY) {
                            if (ball.classification != Ball::Classification::CUE_BALL && ball.classification != Ball::Classification::EIGHT_BALL) isValidTarget = true;
                        } else {
                            if (ball.classification == myclass) isValidTarget = true;
                        }
                        
                        if (nominatedPocket < 6 && ball.pocketIndex != nominatedPocket) isValidTarget = false;

                        if (isValidTarget) {
                            targetIdx = i;
                            break; // Found at least one valid potted ball
                        }
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
                    // Store candidate info
                    g_CurrentCandidate.idx = targetIdx;
                    g_CurrentCandidate.angle = angle;
                    g_CurrentCandidate.power = power;
                    g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[targetIdx].pocketIndex;
                }

                if (isPotentiallyValid) {
                    LOGI("AutoPlaySlow: Found shot at angle %f power %f", angle, power);
                    foundShot = true;
                    Shoot(angle, power);
                    // Do not reset scanning here, so we can resume if this shot fails
                    break;
                }
            }

            if (foundShot) break;
        }

        if (!foundShot && currentScanAngle >= maxAngle) {
            LOGI("AutoPlaySlow: Finished scan, nothing found.");
            isScanning = false;
            currentScanAngle = 0.0;
            state = IDLE;
        }
    }
    
    void ScanFast(double angleStep = 0.1f) {
        if (g_CurrentCandidate.idx != -1) return;

        static double fastSweepAngle = 0.0;

        Prediction::SceneData savedGuiData = gPrediction->guiData;

        auto& cueBall = gPrediction->guiData.balls[0];
        double distSq = (cueBall.initialPosition - fs.scanCuePos).square();

        // ── GENERATE CANDIDATES (only once per turn / per cue ball position) ──
        if (!fs.isInitiated || distSq > 0.0025) {
            fs.raw.clear();
            fs.evals.clear();
            fs.evalIndex = 0;
            fs.scanCuePos = cueBall.initialPosition;
            fs.isInitiated = true;
            fastSweepAngle = 0.0;

            Ball::Classification myclass = sharedGameManager.getPlayerClassification();
            uint nominatedPocket = sharedGameManager.getNominatedPocket();
            bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

            auto pockets = getPockets();

            // onlyEightBallLeft: hitung sekali
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

            std::vector<Candidate> directRaw;
            std::vector<Candidate> specialRaw;

            bool bFoundLowestNumberedBall = false;
            int iFoundLowestNumberedBall = -1;

            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                if (isNineBallGame && bFoundLowestNumberedBall) break;
                auto& ball = gPrediction->guiData.balls[i];
                if (!ball.originalOnTable) continue;
                if (!bFoundLowestNumberedBall) { bFoundLowestNumberedBall = true; iFoundLowestNumberedBall = i; }

                if (!isNineBallGame) {
                    bool isACandidate = onlyEightBallLeft
                        ? (i == 8)
                        : ((myclass == Ball::Classification::ANY)
                            ? (ball.classification != Ball::Classification::EIGHT_BALL)
                            : (ball.classification == myclass));
                    if (!isACandidate) continue;
                }

                for (int pocketIdx = 0; pocketIdx < (int)pockets.size(); pocketIdx++) {
                    if (nominatedPocket < 6 && pocketIdx != nominatedPocket) continue;
                    Point2D pocket = pockets[pocketIdx];
                    Point2D toPocket = pocket - ball.initialPosition;
                    double distTargetToPocket = sqrt(toPocket.square());
                    if (distTargetToPocket < 0.1) continue;
                    Point2D direction = toPocket * (1.0 / distTargetToPocket);
                    Point2D ghostBallPos = ball.initialPosition - direction * (2.0 * BALL_RADIUS);

                    // ── 1. Direct Shot ──────────────────────────────────────
                    {
                        Point2D shotLine = ghostBallPos - cueBall.initialPosition;
                        double distCueToTarget = sqrt(shotLine.square());
                        double angle = atan2(shotLine.y, shotLine.x);
                        if (angle < 0) angle += 2 * M_PI;
                        double score = distCueToTarget + distTargetToPocket;
                        double power = CalculateRequiredPower(score);
                        directRaw.push_back({i, angle, score, pocketIdx, power});
                    }

                    // ── 2. Bank Shot (cushion → target → pocket) ───────────
                    if (bCushionShot) {
                        for (int side = 0; side < 4; side++) {
                            Point2D mp;
                            switch (side) {
                                case 0: mp = {pocket.x,  -134.6 - pocket.y}; break;
                                case 1: mp = {pocket.x,   134.6 - pocket.y}; break;
                                case 2: mp = {-261.6 - pocket.x, pocket.y};  break;
                                case 3: mp = { 261.6 - pocket.x, pocket.y};  break;
                            }
                            Point2D toMir = mp - ball.initialPosition;
                            double distP = sqrt(toMir.square());
                            if (distP < 0.1) continue;
                            Point2D ghost = ball.initialPosition - (toMir * (1.0 / distP)) * (2.0 * BALL_RADIUS);
                            Point2D shot  = ghost - cueBall.initialPosition;
                            double distC  = sqrt(shot.square());
                            double angle  = atan2(shot.y, shot.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double score  = distC + distP + 100.0;
                            double power  = std::min(CalculateRequiredPower(distC + distP) * 1.25, powerMax);
                            power = std::max(power, powerMin);
                            specialRaw.push_back({i, angle, score, pocketIdx, power});
                        }
                    }

                    // ── 3. Kick Shot (cue → cushion → target) ─────────────
                    if (bCushionShot) {
                        for (int side = 0; side < 4; side++) {
                            Point2D mg;
                            switch (side) {
                                case 0: mg = {ghostBallPos.x,  -134.6 - ghostBallPos.y}; break;
                                case 1: mg = {ghostBallPos.x,   134.6 - ghostBallPos.y}; break;
                                case 2: mg = {-261.6 - ghostBallPos.x, ghostBallPos.y};  break;
                                case 3: mg = { 261.6 - ghostBallPos.x, ghostBallPos.y};  break;
                            }
                            Point2D shot  = mg - cueBall.initialPosition;
                            double distC  = sqrt(shot.square());
                            double angle  = atan2(shot.y, shot.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double score  = distC + distTargetToPocket + 150.0;
                            double power  = std::min(CalculateRequiredPower(distC + distTargetToPocket) * 1.30, powerMax);
                            power = std::max(power, powerMin);
                            specialRaw.push_back({i, angle, score, pocketIdx, power});
                        }
                    }

                    // ── 4. Combination Shot (cue → A → B → pocket) ────────
                    for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                        if (j == i) continue;
                        auto& ballB = gPrediction->guiData.balls[j];
                        if (!ballB.originalOnTable) continue;
                        bool isB_Valid = isNineBallGame ? true
                            : (!onlyEightBallLeft && ((myclass == Ball::Classification::ANY)
                                ? (ballB.classification != Ball::Classification::EIGHT_BALL)
                                : (ballB.classification == myclass)));
                        if (!isB_Valid) continue;

                        Point2D toPocketB   = pocket - ballB.initialPosition;
                        double  distBToPocket = sqrt(toPocketB.square());
                        if (distBToPocket < 0.1) continue;
                        Point2D dirB  = toPocketB * (1.0 / distBToPocket);
                        Point2D ghostB = ballB.initialPosition - dirB * (2.0 * BALL_RADIUS);

                        Point2D toGhostB    = ghostB - ball.initialPosition;
                        double  distAToGhostB = sqrt(toGhostB.square());
                        if (distAToGhostB < 0.1) continue;
                        Point2D dirA  = toGhostB * (1.0 / distAToGhostB);
                        Point2D ghostA = ball.initialPosition - dirA * (2.0 * BALL_RADIUS);

                        Point2D shotLine    = ghostA - cueBall.initialPosition;
                        double  distCueToA  = sqrt(shotLine.square());
                        double  angle  = atan2(shotLine.y, shotLine.x);
                        if (angle < 0) angle += 2 * M_PI;
                        double  score  = distCueToA + distAToGhostB + distBToPocket + 80.0;
                        double  power  = std::min(CalculateRequiredPower(distCueToA + distAToGhostB + distBToPocket) * 1.1, powerMax);
                        power = std::max(power, powerMin);
                        specialRaw.push_back({i, angle, score, pocketIdx, power});
                    }

                    // ── 5. Kiss / Carom Shot (cue → A deflects → B → pocket)
                    for (int j = 1; j < gPrediction->guiData.ballsCount; j++) {
                        if (j == i) continue;
                        auto& ballB = gPrediction->guiData.balls[j];
                        if (!ballB.originalOnTable) continue;
                        bool isB_Valid = isNineBallGame ? true
                            : (!onlyEightBallLeft && ((myclass == Ball::Classification::ANY)
                                ? (ballB.classification != Ball::Classification::EIGHT_BALL)
                                : (ballB.classification == myclass)));
                        if (!isB_Valid) continue;

                        Point2D toPocketB   = pocket - ballB.initialPosition;
                        double  distBToPocket = sqrt(toPocketB.square());
                        if (distBToPocket < 0.1) continue;
                        Point2D dirB  = toPocketB * (1.0 / distBToPocket);
                        Point2D ghostB = ballB.initialPosition - dirB * (2.0 * BALL_RADIUS);

                        Point2D d     = ghostB - ball.initialPosition;
                        double  distD = sqrt(d.square());
                        if (distD < 2.0 * BALL_RADIUS) continue;
                        double ratio  = std::min((2.0 * BALL_RADIUS) / distD, 1.0);
                        double theta  = acos(ratio);
                        double angleD = atan2(d.y, d.x);

                        for (int sign : {-1, 1}) {
                            double  angleU = angleD + sign * theta;
                            Point2D u      = {cos(angleU), sin(angleU)};
                            Point2D ghostA = ball.initialPosition + u * (2.0 * BALL_RADIUS);
                            Point2D shotLine   = ghostA - cueBall.initialPosition;
                            double  distCueToA = sqrt(shotLine.square());
                            double  angle  = atan2(shotLine.y, shotLine.x);
                            if (angle < 0) angle += 2 * M_PI;
                            double  score  = distCueToA + distD + distBToPocket + 120.0;
                            double  power  = std::min(CalculateRequiredPower(distCueToA + distD + distBToPocket) * 1.2, powerMax);
                            power = std::max(power, powerMin);
                            specialRaw.push_back({i, angle, score, pocketIdx, power});
                        }
                    }
                }
            }

            // Merge: direct dulu, lalu special
            fs.raw.clear();
            fs.raw.insert(fs.raw.end(), directRaw.begin(), directRaw.end());
            fs.raw.insert(fs.raw.end(), specialRaw.begin(), specialRaw.end());

            // Sort by score ascending (shot terpendek / termudah dulu)
            std::sort(fs.raw.begin(), fs.raw.end(), [](const Candidate& a, const Candidate& b) {
                return a.score < b.score;
            });

            // Batasi 60 kandidat terbaik (cukup untuk semua tipe shot)
            if (fs.raw.size() > 60) fs.raw.resize(60);

            // Re-sort by angle untuk smooth sweep visual
            std::sort(fs.raw.begin(), fs.raw.end(), [](const Candidate& a, const Candidate& b) {
                return a.angle < b.angle;
            });
        }

        // ── PER-FRAME EVALUATION (1 kandidat per frame = lag-free) ────────────
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        bool isNineBallGame = (myclass == Ball::Classification::NINE_BALL_RULE);

        bool onlyEightBallLeft = false;
        if (myclass == Ball::Classification::SOLID) {
            bool has = false; for (int k = 1; k < 8; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; } if (!has) onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::STRIPE) {
            bool has = false; for (int k = 9; k <= 15; k++) if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; } if (!has) onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::EIGHT_BALL) {
            onlyEightBallLeft = true;
        } else if (myclass == Ball::Classification::ANY) {
            bool has = false; for (int k = 1; k <= 15; k++) { if (k == 8) continue; if (gPrediction->guiData.balls[k].originalOnTable) { has = true; break; } } if (!has) onlyEightBallLeft = true;
        }

        // Angle refinement: ±0.5°, ±1° → 5 angle × 3 power = 15 sim per kandidat
        static const double kDA[] = {0.0, -0.0175, +0.0175, -0.035, +0.035}; // 0°, ±1°, ±2°
        static const double kPF[] = {1.0, 1.2, 0.8, 1.4, 0.6, 1.6, 0.5};
        
        bool foundShot = false;
        while (!foundShot && fs.evalIndex < fs.raw.size()) {
            auto raw = fs.raw[fs.evalIndex++];
            double baseAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(raw.angle));
            bool simOk = false;
            double usedAngle = baseAngle, usedPower = raw.power;

            for (double dA : kDA) {
                if (simOk) break;
                double tryAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(baseAngle + dA));
                for (double pf : kPF) {
                    double tryPower = std::min(std::max(raw.power * pf, powerMin), powerMax);
                    gPrediction->forceFullSimulation = true;
                    gPrediction->determineShotResult(true, tryAngle, tryPower, sharedGameManager.getShotSpin(), raw);
                    gPrediction->forceFullSimulation = false;

                    if (!gPrediction->firstHitIsTarget) continue;
                    if (!gPrediction->guiData.balls[0].onTable) continue;

                    if (isNineBallGame) {
                        auto fh = gPrediction->guiData.collision.firstHitBall;
                        if (!fh || fh->index != raw.idx) continue;
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
                        if (nominatedPocket < 6 && gPrediction->guiData.balls[bestPottedIdx].pocketIndex != nominatedPocket) continue;
                        g_CurrentCandidate = raw;
                        g_CurrentCandidate.idx = bestPottedIdx;
                        g_CurrentCandidate.angle = tryAngle;
                        g_CurrentCandidate.power = tryPower;
                        g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[bestPottedIdx].pocketIndex;
                        fs.isInitiated = false;
                        foundShot = true;
                        Shoot(tryAngle, tryPower);
                        goto scanFastDone;
                    }

                    // 8-ball path
                    if (gPrediction->guiData.balls[raw.idx].onTable) continue;
                    if (gPrediction->guiData.balls[raw.idx].pocketIndex != raw.pocketIndex) continue;

                    {
                        bool isAngleGood = false;
                        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                            auto& ball = gPrediction->guiData.balls[i];
                            bool match = onlyEightBallLeft ? (i == 8)
                                : ((myclass == Ball::Classification::ANY)
                                    ? (ball.classification != Ball::Classification::CUE_BALL && ball.classification != Ball::Classification::EIGHT_BALL)
                                    : (ball.classification == myclass));
                            if (match && ball.originalOnTable && !ball.onTable) { isAngleGood = true; break; }
                        }
                        if (!isAngleGood) continue;

                        if (gPrediction->guiData.collision.firstHitBall) {
                            auto fh = gPrediction->guiData.collision.firstHitBall;
                            if (onlyEightBallLeft) {
                                if (fh->classification != Ball::Classification::EIGHT_BALL) continue;
                            } else if (myclass == Ball::Classification::ANY) {
                                if (fh->classification == Ball::Classification::EIGHT_BALL) continue;
                            } else {
                                if (fh->classification != myclass) continue;
                            }
                        }

                        // 8-ball premature
                        auto& b8 = gPrediction->guiData.balls[8];
                        if (b8.originalOnTable && !b8.onTable && !onlyEightBallLeft && myclass != Ball::Classification::EIGHT_BALL) continue;
                    }

                    usedAngle = tryAngle; usedPower = tryPower;
                    simOk = true;
                    break;
                }
            }
            if (!simOk) continue;

            g_CurrentCandidate = raw;
            g_CurrentCandidate.angle = usedAngle;
            g_CurrentCandidate.power = usedPower;
            fs.isInitiated = false;
            foundShot = true;
            Shoot(usedAngle, usedPower);
        }

        scanFastDone:
        gPrediction->guiData = savedGuiData;

        // Sweep visual saat masih evaluating
        if (!foundShot && fs.evalIndex < fs.raw.size()) {
            fastSweepAngle = normalizeAngle(fastSweepAngle + 0.15);
            if (!persistent_bool[O("bDisableFlicker")]) {
                gPrediction->forceFullSimulation = true;
                gPrediction->determineShotResult(true, fastSweepAngle, 400.0, sharedGameManager.getShotSpin());
                gPrediction->forceFullSimulation = false;
            }
        }

        if (!foundShot && fs.evalIndex >= fs.raw.size()) {
            fs.isInitiated = false;
            lastFailedCuePos = cueBall.initialPosition;
            LOGI("AutoPlay: ScanFast exhausted %zu candidates, fallback to ScanSlow", fs.raw.size());
            scan = SLOW;
        }
    }

    void DrawToggleButton() {
        ImGuiIO& io = GetIO();
        float padding = 30.0f;
        int buttons = 1;
        float button_size = ImGui::GetFrameHeight() * 2.3f;
        float windowWidth = button_size * buttons + (buttons > 1 ? GetStyle().ItemSpacing.x * (buttons - 1) : 0) + GetStyle().WindowPadding.x * 2;
        float windowHeight = button_size + GetStyle().WindowPadding.y * 2;

        SetNextWindowPos(ImVec2(io.DisplaySize.x - 35 - windowWidth, io.DisplaySize.y - 20 - windowHeight), ImGuiCond_Always);
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
                
                // Since we are in a window with the bg set, we can just use standard button with colors
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
                // if (!bAutoPlaying && powerSlider.Active) powerSlider.Cancel();
            }
        } End();

        PopStyleVar();
        PopStyleColor(2);
    }

    bool isAnimationActive() {
        auto visualCue = sharedGameManager.mVisualCue();
        if (!visualCue) return true;
        
        auto _powerBarView = F(ptr, visualCue + 0x510);
        if (!_powerBarView) return true;

        auto activeAction = M(ptr, libmain + 0x2de6f30, ptr)(_powerBarView); // CCAction getActiveAction
        if (activeAction) {
            // auto tag = F(uint, activeAction + 0x18); // 668 hiding 667 showing
            // LOGI("tag %u %d %p", tag, tag, tag);
            return true;
        }

        return false;
    }
    
    void Update() {
        buttonClicker.Update();
       // DrawToggleButton();

        if (isAnimationActive()) return;

        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            state = IDLE;
            return;
        }

        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
        } if (state == SCANNING) {
            if (scan == FAST) ScanFast();
            if (scan == SLOW) {
                DrawEightBallLoading(GetForegroundDrawList());
                ScanSlow(0.003f);
            }
        } if (state == NOMINATING) {
            nominationFrameCounter++;
            if (nominationFrameCounter == 10) {
                buttonClicker.Click(GetPocketScreenPos(g_CurrentCandidate.pocketIndex));
            }
            if (nominationFrameCounter > 20 && !buttonClicker.Active) {
                takeShot(pendingShotAngle, pendingShotPower);
                ClearState();
                state = IDLE;
            }
        }

        /* if (bAutoPlaying && sharedGameManager.mStateManager().isPlayerTurn()) {
            if (powerSlider.Active) {
                UpdateTouchSimulation();
                powerSlider.Update();
            } else Start();
        } */

        // if (!bAutoPlaying && powerSlider.Active) powerSlider.Update(); // for TestAutoPlay
    }
};
