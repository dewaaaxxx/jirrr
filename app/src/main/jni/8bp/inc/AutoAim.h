#pragma once

#include "Prediction.fast.h"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>

using namespace ImGui;

constexpr double ANGLE_PRECISION = 0.01;      // Fine-tuned angle step
constexpr double POWER_PRECISION = 5.0;       // Power search granularity
constexpr double MIN_POWER = 10.0;            // Minimum shot power
constexpr double MAX_POWER = 666.0;           // Maximum shot power
constexpr double OPTIMAL_POWER_FACTOR = 1.2;  // Power boost for harder hits

struct ShotCandidate {
    double angle = 0.0;
    double power = 0.0;
    int pottedBallCount = 0;
    int targetBallIndex = -1;
    bool isValid = false;
    double score = 0.0; // Quality metric for ranking

    bool operator<(const ShotCandidate& other) const {
        if (pottedBallCount != other.pottedBallCount) {
            return pottedBallCount > other.pottedBallCount;
        }
        return score > other.score;
    }
};

bool ix = true;

namespace AutoAim {
    double lastSetAngle = 0.f;
    double lastSetPower = 0.f;
    bool didSetAngle = false;
    std::vector<ShotCandidate> candidates;

    bool shouldAutoAIM() { 
        return !didSetAngle || 
               lastSetAngle == sharedGameManager.mVisualCue().mVisualGuide().mAimAngle() ||
               lastSetPower == sharedGameManager.mVisualCue().getShotPower();
    }

    void setAimAngle(double angle) {
        lastSetAngle = angle;
        sharedGameManager.mVisualCue().mVisualGuide().mAimAngle(angle);
    }

    void setShotPower(double power) {
        lastSetPower = power;
        sharedGameManager.mVisualCue().setShotPower(power);
    }

    // Evaluate shot quality based on predicted outcome
    double evaluateShotQuality(const Prediction& pred, double power) {
        double score = 0.0;
        
        // Bonus for not scratching (cueball potted)
        if (pred.guiData.balls[0].onTable) score += 100.0;
        
        // Bonus for hitting the right ball first
        if (pred.guiData.collision.firstHitBall != nullptr) {
            auto myclass = ix ? Ball::Classification::SOLID : Ball::Classification::STRIPE;
            if (pred.guiData.collision.firstHitBall->classification == myclass) {
                score += 200.0;
            }
        }
        
        // Penalty for hitting 8-ball early
        if (pred.guiData.collision.firstHitBall != nullptr && 
            pred.guiData.collision.firstHitBall->classification == Ball::Classification::EIGHT_BALL) {
            score -= 500.0;
        }
        
        // Adjust score based on power efficiency
        if (power > 0) {
            score += (power / MAX_POWER) * 50.0;
        }
        
        return score;
    }

    // Find optimal power for a given angle
    double findOptimalPower(double angle) {
        double bestPower = MIN_POWER;
        int bestPottedCount = 0;
        double bestScore = -1e9;

        for (double power = MIN_POWER; power <= MAX_POWER; power += POWER_PRECISION) {
            gPrediction->determineShotResult(true, angle, power, {0, 0});
            
            int pottedCount = 0;
            for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                Prediction::Ball& ball = gPrediction->guiData.balls[i];
                if (ball.originalOnTable && !ball.onTable) {
                    pottedCount++;
                }
            }
            
            double score = evaluateShotQuality(*gPrediction, power);
            
            if (pottedCount > bestPottedCount || 
                (pottedCount == bestPottedCount && score > bestScore)) {
                bestPottedCount = pottedCount;
                bestScore = score;
                bestPower = power;
            }
        }
        
        return bestPower;
    }

    // Enhanced AIM with fine-tuning and power optimization
    void AIM(double angleStep = ANGLE_PRECISION) {
        auto startingAngle = NumberUtils::normalizeDoublePrecision(
            sharedGameManager.mVisualCue().mVisualGuide().mAimAngle()
        );
        
        candidates.clear();
        auto myclass = ix ? Ball::Classification::SOLID : Ball::Classification::STRIPE;
        
        // Coarse search: find promising angles
        std::vector<double> promisingAngles;
        for (double angle = 0; angle < maxAngle; angle += 0.05) {
            angle = NumberUtils::normalizeDoublePrecision(angle);
            
            double testPower = sharedGameManager.mVisualCue().getShotPower();
            if (testPower < MIN_POWER) testPower = 300.0; // Use reasonable default
            
            gPrediction->determineShotResult(true, angle, testPower, {0, 0});
            
            bool isGoodTarget = false;
            if (gPrediction->guiData.collision.firstHitBall != nullptr) {
                if (gPrediction->guiData.collision.firstHitBall->classification == myclass ||
                    gPrediction->guiData.collision.firstHitBall->classification == Ball::Classification::EIGHT_BALL) {
                    isGoodTarget = true;
                }
            }
            
            if (isGoodTarget) {
                promisingAngles.push_back(angle);
            }
        }
        
        // Fine search: refine promising angles and optimize power
        for (double angle : promisingAngles) {
            for (double fineAngle = angle - 0.05; fineAngle <= angle + 0.05; fineAngle += angleStep) {
                double normalizedAngle = NumberUtils::normalizeDoublePrecision(normalizeAngle(fineAngle));
                
                // Find optimal power for this angle
                double optimalPower = findOptimalPower(normalizedAngle);
                
                // Boost power for harder hits
                optimalPower = std::min(optimalPower * OPTIMAL_POWER_FACTOR, MAX_POWER);
                
                gPrediction->determineShotResult(true, normalizedAngle, optimalPower, {0, 0});
                
                // Count potted balls
                std::vector<int> currentPottedBalls;
                bool isAngleGood = false;
                
                for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
                    Prediction::Ball& ball = gPrediction->guiData.balls[i];
                    if (ball.classification == myclass && ball.originalOnTable && !ball.onTable) {
                        currentPottedBalls.push_back(i);
                        isAngleGood = true;
                    }
                }
                
                // Validation checks
                if (isAngleGood && gPrediction->guiData.collision.firstHitBall &&
                    gPrediction->guiData.collision.firstHitBall->classification != myclass) {
                    isAngleGood = false;
                }
                
                auto& cueBall = gPrediction->guiData.balls[0];
                auto& eightBall = gPrediction->guiData.balls[8];
                if (isAngleGood && cueBall.originalOnTable && !cueBall.onTable) isAngleGood = false;
                if (isAngleGood && !ix && eightBall.originalOnTable && !eightBall.onTable) isAngleGood = false;
                
                if (isAngleGood) {
                    double score = evaluateShotQuality(*gPrediction, optimalPower);
                    candidates.push_back({
                        normalizedAngle,
                        optimalPower,
                        (int)currentPottedBalls.size(),
                        currentPottedBalls.empty() ? -1 : currentPottedBalls[0],
                        true,
                        score
                    });
                }
            }
        }
        
        // Select best candidate
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end());
            ShotCandidate best = candidates[0];
            
            setAimAngle(best.angle);
            setShotPower(best.power);
            
            // Optional: log the best shot found
            // LOGI("Best shot: angle=%.2f, power=%.1f, potted=%d, score=%.1f", 
            //      best.angle, best.power, best.pottedBallCount, best.score);
        }
    }

    void Draw() {
        ImGuiIO& io = GetIO();
        float padding = 30.0f;
        SetNextWindowPos(ImVec2(io.DisplaySize.x - persistent_int["iAIM_WindowX"], persistent_int["iAIM_WindowY"]), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        SetNextWindowSize(ImVec2(280, 100), ImGuiCond_FirstUseEver);

        if (Begin("AutoAim Enhanced", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings)) {
            ImVec2 windowSize = GetWindowSize();
            float availableWidth = windowSize.x - GetStyle().WindowPadding.x * 2 - GetStyle().ItemSpacing.x * 2;
            float buttonWidth = availableWidth / 4;
            float availableHeight = windowSize.y - GetStyle().WindowPadding.y * 2;
            float buttonSize = (buttonWidth < availableHeight / 2) ? buttonWidth : availableHeight / 2;
            
            // First row: Ball selection
            if (Button(ix ? "SOLID" : "STRIPE", ImVec2(availableWidth, buttonSize))) ix = !ix;
            
            // Second row: Controls
            if (Button("FIRE!", ImVec2(availableWidth / 2 - GetStyle().ItemSpacing.x * 0.5f, buttonSize))) {
                AIM(ANGLE_PRECISION);
            }
            SameLine();
            if (Button("SEARCH", ImVec2(availableWidth / 2 - GetStyle().ItemSpacing.x * 0.5f, buttonSize))) {
                AIM(0.02); // Finer search
            }
            
            // Display best candidate info
            if (!candidates.empty()) {
                Text("Best: %.1f° @ %.0f power", candidates[0].angle, candidates[0].power);
                Text("Potted: %d balls (score: %.0f)", candidates[0].pottedBallCount, candidates[0].score);
            }
        } 
        End();
    }
};
