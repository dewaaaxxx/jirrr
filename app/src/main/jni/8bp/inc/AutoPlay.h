// نفس الإضافات الأولية كما عندك، ولكن بدون أي تعديل في دوال التنفيذ الأساسية
#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>

#include "ScreenTable.h"
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

namespace AutoPlay {
    double lastSetAngle = 0.f;
    bool didSetAngle = false;
    bool bAutoPlaying = false;

    enum State { IDLE, SCANNING, NOMINATING, EXECUTING } state = IDLE;
    enum ScanMode { FAST, SLOW } scan = FAST;
    
    double pendingShotPower = 0.f;
    double pendingShotAngle = 0.f;
    int nominationFrameCounter = 0;

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
    
    // ================== التحقق مما إذا كانت الكرة الثامنة هي الوحيدة المتبقية ==================
    bool isOnlyEightBallRemaining() {
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        if (myclass == Ball::Classification::ANY) return false;
        if (myclass == Ball::Classification::EIGHT_BALL) return true;
        
        for (int i = 1; i <= 15; i++) {
            if (i == 8) continue;
            auto& ball = gPrediction->guiData.balls[i];
            if (ball.classification == myclass && ball.originalOnTable && ball.onTable) {
                return false;
            }
        }
        return true;
    }
    
    // ========== دوال الحماية الجديدة ==========
    inline bool isCueBallSafe() {
        return gPrediction->guiData.balls[0].onTable;
    }
    
    inline bool isEightBallHitLegal() {
        if (isOnlyEightBallRemaining()) return true;
        auto firstHit = gPrediction->guiData.collision.firstHitBall;
        if (firstHit && firstHit->index == 8) return false;
        return true;
    }
    
    // ========== Slow Mode (مع إضافة الحماية) ==========
    void ScanSlow(double angleStep = 0.01f) {
        LOGI("🐢 AutoPlay: SLOW mode");
        
        if (g_CurrentCandidate.idx != -1) return;
        
        static double currentScanAngle = 0.0;
        static bool isScanning = false;
        static Point2D lastScanCuePos = { -1000.0, -1000.0 };

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
        
        std::vector<double> powers = {150.0, 220.0, 300.0, 380.0, 460.0, 540.0, 620.0, 666.0 };
        
        while (steps < 25 && currentScanAngle < maxAngle) {
            double angle = currentScanAngle;
            currentScanAngle += angleStep;
            steps++;

            for (double power : powers) {
                gPrediction->determineShotResult(false, angle, power, sharedGameManager.getShotSpin());
                
                // === الحماية المضافة ===
                if (!isCueBallSafe()) continue;
                if (!isEightBallHitLegal()) continue;
                // ======================
                
                int targetIdx = -1;

                // 9-ball logic
                bool bFoundLowestNumberedBall = false;
                int iFoundLowestNumberedBall = -1;
                bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;

                if (isNineBallGame) {
                    // ... (نفس الكود الأصلي، لم يتغير)
                    for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                        auto& ball = gPrediction->guiData.balls[i];
                        if (!ball.originalOnTable) continue;
                        bFoundLowestNumberedBall = true;
                        iFoundLowestNumberedBall = i;
                        break;
                    }

                    auto firstHit = gPrediction->guiData.collision.firstHitBall;
                    if (!firstHit) continue;
                    if (firstHit->index != iFoundLowestNumberedBall) continue;
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

                // 8-ball logic
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

                        if (isValidTarget) {
                            targetIdx = i;
                            break;
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

                    g_CurrentCandidate.idx = targetIdx;
                    g_CurrentCandidate.angle = angle;
                    g_CurrentCandidate.power = power;
                    g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[targetIdx].pocketIndex;
                    
                    LOGI("AutoPlaySlow: Found shot at angle %f power %f", angle, power);
                    foundShot = true;
                    Shoot(angle, power);
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
    
    // ========== Fast Mode (مع إضافة الحماية) ==========
    void ScanFast(double angleStep = 0.1f) {
        if (g_CurrentCandidate.idx != -1) return;
        if (gPrediction->guiData.balls[0].initialPosition == lastFailedCuePos) return;

        double startingAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        
        gPrediction->determineShotResult(true, startingAngle);
        std::vector<int> startingPottedBalls;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                startingPottedBalls.push_back(i);
            }
        }
        
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        uint nominatedPocket = sharedGameManager.getNominatedPocket();
        
        std::vector<Candidate> candidates;
        
        auto pockets = getPockets();
        auto& cueBall = gPrediction->guiData.balls[0];
        
        bool bFoundLowestNumberedBall = false;
        int iFoundLowestNumberedBall = -1;
        bool isNineBallGame = myclass == Ball::Classification::NINE_BALL_RULE;
        
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            if (isNineBallGame && bFoundLowestNumberedBall) break;
            
            // === حماية الكرة الثامنة: لا نضيفها كمرشح إن لم يكن دورها ===
            if (i == 8 && !isOnlyEightBallRemaining()) continue;
            
            auto& ball = gPrediction->guiData.balls[i];
            if (!ball.originalOnTable) continue;

            if (!bFoundLowestNumberedBall) {
                bFoundLowestNumberedBall = true;
                iFoundLowestNumberedBall = i;
            }

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
        
        bool foundShot = false;
        for (const auto& cand : candidates) {
            double angle = NumberUtils::normalizeDoublePrecision(normalizeAngle(cand.angle));
            gPrediction->determineShotResult(true, angle, cand.power, sharedGameManager.getShotSpin(), cand);
            
            // === الحماية المضافة ===
            if (!isCueBallSafe()) continue;
            if (!isEightBallHitLegal()) continue;
            // ======================
            
            if (!gPrediction->firstHitIsTarget) continue;
            if (!gPrediction->guiData.balls[0].onTable) continue;

            if (myclass == Ball::Classification::NINE_BALL_RULE) {
                auto firstHit = gPrediction->guiData.collision.firstHitBall;
                if (!firstHit) continue;
                if (firstHit->index != cand.idx) continue;

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
                int effectiveTargetIdx = bestPottedIdx;

                if (nominatedPocket < 6 && gPrediction->guiData.balls[effectiveTargetIdx].pocketIndex != nominatedPocket) continue;

                LOGI("AutoPlay: 9ball: Found good angle %f with power %f", angle, cand.power);
                g_CurrentCandidate = cand;
                g_CurrentCandidate.idx = effectiveTargetIdx;
                g_CurrentCandidate.pocketIndex = gPrediction->guiData.balls[effectiveTargetIdx].pocketIndex;

                foundShot = true;
                Shoot(angle, cand.power);
                break;
            }

            if (gPrediction->guiData.balls[cand.idx].onTable) continue;
            if (gPrediction->guiData.balls[cand.idx].pocketIndex != cand.pocketIndex) continue;

            std::vector<int> currentPottedBalls;
            bool isAngleGood = false;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                Prediction::Ball& ball = gPrediction->guiData.balls[i];
                bool match = (myclass == Ball::Classification::ANY)
                    ? (ball.classification != Ball::Classification::CUE_BALL && ball.classification != Ball::Classification::EIGHT_BALL)
                    : (ball.classification == myclass);

                if (match && ball.originalOnTable && !ball.onTable) {
                    currentPottedBalls.push_back(i);
                    isAngleGood = true;
                }
            }

            if (isAngleGood && gPrediction->guiData.collision.firstHitBall) {
                 auto firstHit = gPrediction->guiData.collision.firstHitBall;
                 if (myclass != Ball::Classification::ANY && firstHit->classification != myclass) isAngleGood = false;
                 else if (myclass == Ball::Classification::ANY && firstHit->classification == Ball::Classification::EIGHT_BALL) isAngleGood = false;
            }

            auto& cueBallRef = gPrediction->guiData.balls[0];
            if (isAngleGood && cueBallRef.originalOnTable && !cueBallRef.onTable) isAngleGood = false;
            
            auto& eightBallRef = gPrediction->guiData.balls[8];
            bool isEightBallPotted = eightBallRef.originalOnTable && !eightBallRef.onTable;
            if (isAngleGood && isEightBallPotted && myclass != Ball::Classification::EIGHT_BALL) isAngleGood = false;
            
            if (isAngleGood) {
                LOGI("AutoPlay: Found good angle %f with power %f", angle, cand.power);
                g_CurrentCandidate = cand;
                foundShot = true;
                Shoot(angle, cand.power);
                break;
            }
        }

        if (!foundShot) {
            lastFailedCuePos = cueBall.initialPosition;
            LOGI("AutoPlay: No good angle found after smart scan.");
            scan = SLOW;
        }
    }

    void DrawToggleButton() {
        // تماماً كما في الكود الأصلي (لم أغير شيئاً)
        ImGuiIO& io = GetIO();
        float button_size = ImGui::GetFrameHeight() * 2.3f;
        float windowWidth = button_size + GetStyle().WindowPadding.x * 2;
        float windowHeight = button_size + GetStyle().WindowPadding.y * 2;

        SetNextWindowPos(ImVec2(io.DisplaySize.x - 35 - windowWidth, io.DisplaySize.y - 20 - windowHeight), ImGuiCond_Always);
        SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
        
        PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 5.0f);
        
        if (Begin("AutoPlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
            ImVec2 pos = GetCursorScreenPos();
            ImVec2 size(button_size, button_size);
            ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
            
            PushStyleColor(ImGuiCol_Button, IM_COL32(50, 50, 50, 180));
            PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(80, 80, 80, 200));
            PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(100, 100, 100, 200));
            
            bool clicked = Button("##AutoPlayBtn", size);
            
            ImDrawList* dl = GetWindowDrawList();
            float h = size.y * 0.4f;
            float off_x = h * 0.3f;
            dl->AddTriangleFilled(ImVec2(center.x - off_x, center.y - h/2), ImVec2(center.x - off_x, center.y + h/2), ImVec2(center.x + off_x * 1.5f, center.y), IM_COL32(255, 255, 255, 180));
            GetForegroundDrawList()->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(200, 200, 200, 255), 5.0f, 0, 2.0f);
            
            PopStyleColor(3);
            
            if (clicked) {
                bAutoPlaying = !bAutoPlaying;
                if (bAutoPlaying) ClearState();
            }
        }
        End();
        PopStyleVar();
        PopStyleColor(2);
    }

    bool isAnimationActive() {
        auto visualCue = sharedGameManager.mVisualCue();
        if (!visualCue) return true;
        
        auto _powerBarView = F(ptr, visualCue + 0x510);
        if (!_powerBarView) return true;

        auto activeAction = M(ptr, libmain + 0x2de6f30, ptr)(_powerBarView);
        if (activeAction) {
            return true;
        }

        return false;
    }
    
    void Update() {
        buttonClicker.Update();

        if (isAnimationActive()) return;

        if (!bAutoPlaying || !sharedGameManager.mStateManager().isPlayerTurn()) {
            state = IDLE;
            return;
        }

        if (state == IDLE) {
            state = SCANNING;
            scan = FAST;
        } 
        if (state == SCANNING) {
            if (scan == FAST) ScanFast();
            if (scan == SLOW) {
                DrawEightBallLoading(GetForegroundDrawList());
                ScanSlow(0.003f);
            }
        } 
        if (state == NOMINATING) {
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
    }
};
