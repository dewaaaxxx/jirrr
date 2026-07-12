#pragma once
#include "Prediction.h"
#include <vector>
#include <algorithm>
#include <cmath>

// ============================================================================
// FAST SCANNER - Multi-angle ball landing detection
// Ensures comprehensive coverage of all possible angles to find shots
// ============================================================================

struct FastScannerConfig {
    double angleStepFine = 0.05;      // Fine angle step for detailed scanning
    double angleStepCoarse = 0.2;     // Coarse angle step for quick pre-scan
    int minBallsToFind = 1;           // Minimum balls to consider a valid shot
    bool useAdaptiveStep = true;      // Dynamically adjust step based on results
    bool excludeWhiteBall = true;     // Don't count white ball as pocketed
    bool prioritizeClearShots = true; // Prefer shots that don't bounce rails
};

struct ShotCandidate {
    double angle;
    int ballsHit;
    int ballsPocketed;
    std::vector<int> pocketedBallIndices;
    int railCollisions;
    bool firstHitValid;
    double score; // Aggregate scoring metric
};

class FastScanner {
public:
    static FastScannerConfig config;
    static std::vector<ShotCandidate> lastScanResults;
    
    // ========================================================================
    // FULL 360° MULTI-PASS SCAN
    // Pass 1: Coarse scan (0.2° steps) - Quick overview
    // Pass 2: Fine scan (0.05° steps) - Detailed analysis
    // ========================================================================
    static std::vector<ShotCandidate> scanAllAngles360(double angleStepOverride = -1.0) {
        lastScanResults.clear();
        
        if (!gPrediction) return lastScanResults;
        
        double coarseStep = angleStepOverride > 0 ? angleStepOverride : config.angleStepCoarse;
        double fineStep = config.angleStepFine;
        
        // Get baseline state (no shot)
        gPrediction->determineShotResult(true, 0.0, 0.0);
        std::vector<int> baselinePocketed = getBaselinePocketedBalls();
        
        // ====== PASS 1: COARSE SCAN ======
        std::vector<ShotCandidate> coarseResults;
        double angle = 0.0;
        const double fullCircle = 2.0 * M_PI;
        
        while (angle < fullCircle) {
            ShotCandidate candidate = evaluateShotAtAngle(angle, baselinePocketed);
            if (candidate.ballsPocketed >= config.minBallsToFind) {
                coarseResults.push_back(candidate);
            }
            angle += coarseStep;
        }
        
        // ====== PASS 2: FINE SCAN AROUND PROMISING ANGLES ======
        // Re-scan areas with good coarse results at finer resolution
        for (const auto& coarseHit : coarseResults) {
            double scanStart = coarseHit.angle - coarseStep;
            double scanEnd = coarseHit.angle + coarseStep;
            
            for (double fineAngle = scanStart; fineAngle <= scanEnd; fineAngle += fineStep) {
                ShotCandidate fineCandidate = evaluateShotAtAngle(fineAngle, baselinePocketed);
                
                // Only add if it's actually better or fills a gap
                bool isDuplicate = false;
                for (const auto& existing : lastScanResults) {
                    if (std::abs(existing.angle - fineAngle) < fineStep * 0.5) {
                        isDuplicate = true;
                        break;
                    }
                }
                
                if (!isDuplicate && fineCandidate.ballsPocketed >= config.minBallsToFind) {
                    lastScanResults.push_back(fineCandidate);
                }
            }
        }
        
        // Sort by score (best shots first)
        std::sort(lastScanResults.begin(), lastScanResults.end(),
            [](const ShotCandidate& a, const ShotCandidate& b) {
                return a.score > b.score;
            });
        
        return lastScanResults;
    }
    
    // ========================================================================
    // TARGETED SCAN - Focus on specific angle range
    // Useful for refining around a promising angle
    // ========================================================================
    static std::vector<ShotCandidate> scanAngleRange(double startAngle, double endAngle, double angleStep = -1.0) {
        std::vector<ShotCandidate> results;
        
        if (!gPrediction) return results;
        
        double step = angleStep > 0 ? angleStep : config.angleStepFine;
        
        gPrediction->determineShotResult(true, 0.0, 0.0);
        std::vector<int> baselinePocketed = getBaselinePocketedBalls();
        
        // Normalize angles to 0-2π range
        startAngle = normalizeAngle(startAngle);
        endAngle = normalizeAngle(endAngle);
        
        double angle = startAngle;
        int maxIterations = (int)(6.283185 / step) + 10;
        int iterations = 0;
        
        while (iterations < maxIterations) {
            ShotCandidate candidate = evaluateShotAtAngle(angle, baselinePocketed);
            if (candidate.ballsPocketed >= config.minBallsToFind) {
                results.push_back(candidate);
            }
            
            angle = normalizeAngle(angle + step);
            iterations++;
            
            // Exit if we've looped back past end
            if (angle < step && startAngle > (2.0 * M_PI - step)) break;
        }
        
        // Sort by score
        std::sort(results.begin(), results.end(),
            [](const ShotCandidate& a, const ShotCandidate& b) {
                return a.score > b.score;
            });
        
        return results;
    }
    
    // ========================================================================
    // ADAPTIVE SCAN - Smart scanning that adapts based on table state
    // ========================================================================
    static std::vector<ShotCandidate> adaptiveScan() {
        int remainingBalls = 0;
        for (int i = 1; i < 16; i++) {
            if (gPrediction->guiData.balls[i].originalOnTable) {
                remainingBalls++;
            }
        }
        
        // Adjust step size based on remaining balls
        // More balls = more precision needed
        double adaptiveStep = config.angleStepCoarse;
        if (remainingBalls < 4) {
            adaptiveStep = config.angleStepFine; // Fine scan when few balls remain
        } else if (remainingBalls > 10) {
            adaptiveStep = config.angleStepCoarse * 1.5; // Slightly coarser for many balls
        }
        
        return scanAllAngles360(adaptiveStep);
    }
    
    // ========================================================================
    // DETAILED ANGLE EVALUATION
    // ========================================================================
    static ShotCandidate evaluateShotAtAngle(double angle, const std::vector<int>& baselinePocketed) {
        ShotCandidate candidate;
        candidate.angle = angle;
        candidate.ballsHit = 0;
        candidate.ballsPocketed = 0;
        candidate.railCollisions = 0;
        candidate.firstHitValid = false;
        
        // Simulate shot at this angle with max power (to find all possible pockets)
        const double testPower = 100.0; // Sufficient power to reach all balls
        gPrediction->determineShotResult(true, angle, testPower);
        
        // Analyze results
        for (int i = 1; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            
            // Check if ball was potted (wasn't on table before, isn't now)
            if (ball.originalOnTable && !ball.onTable) {
                // Exclude if it wasn't in baseline
                bool wasInBaseline = std::find(baselinePocketed.begin(), baselinePocketed.end(), i) != baselinePocketed.end();
                if (!wasInBaseline) {
                    candidate.ballsPocketed++;
                    candidate.pocketedBallIndices.push_back(i);
                }
            }
        }
        
        // Check first hit validity
        if (gPrediction->guiData.collision.firstHitBall != nullptr) {
            candidate.ballsHit = 1;
            candidate.firstHitValid = true;
        }
        
        // Count rail collisions
        candidate.railCollisions = gPrediction->guiData.collision.railCollisions;
        
        // ====== SCORING SYSTEM ======
        // Higher score = better shot
        candidate.score = 0.0;
        
        // Base score: number of balls pocketed
        candidate.score += candidate.ballsPocketed * 10.0;
        
        // Bonus: Direct shots (no rails)
        if (config.prioritizeClearShots && candidate.railCollisions == 0 && candidate.ballsPocketed > 0) {
            candidate.score += 5.0;
        }
        
        // Penalty: Too many rail bounces
        if (candidate.railCollisions > 3) {
            candidate.score *= 0.7;
        }
        
        // Bonus: First hit is valid (not wrong ball type)
        if (candidate.firstHitValid) {
            candidate.score += 2.0;
        }
        
        return candidate;
    }
    
    // ========================================================================
    // HELPER FUNCTIONS
    // ========================================================================
    
    // Get balls that are already pocketed (baseline)
    static std::vector<int> getBaselinePocketedBalls() {
        std::vector<int> pocketed;
        for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
            Prediction::Ball& ball = gPrediction->guiData.balls[i];
            if (ball.originalOnTable && !ball.onTable) {
                pocketed.push_back(i);
            }
        }
        return pocketed;
    }
    
    // Normalize angle to 0-2π range
    static double normalizeAngle(double angle) {
        constexpr double fullCircle = 2.0 * M_PI;
        if (angle >= fullCircle) {
            angle = std::fmod(angle, fullCircle);
        } else if (angle < 0) {
            angle = fullCircle - std::fmod(-angle, fullCircle);
        }
        return angle;
    }
    
    // Find best shot from scan results
    static ShotCandidate* findBestShot(std::vector<ShotCandidate>& results) {
        if (results.empty()) return nullptr;
        return &results[0]; // Already sorted by score
    }
    
    // Find all shots that pocket specific balls
    static std::vector<ShotCandidate> findShotsForBalls(
        std::vector<ShotCandidate>& results,
        const std::vector<int>& targetBalls) {
        
        std::vector<ShotCandidate> matching;
        for (auto& candidate : results) {
            bool hasAll = true;
            for (int target : targetBalls) {
                if (std::find(candidate.pocketedBallIndices.begin(),
                             candidate.pocketedBallIndices.end(),
                             target) == candidate.pocketedBallIndices.end()) {
                    hasAll = false;
                    break;
                }
            }
            if (hasAll) {
                matching.push_back(candidate);
            }
        }
        return matching;
    }
    
    // Get angle range for a specific ball
    static std::pair<double, double> getAngleRangeForBall(int ballIndex, double rangeWidth = 0.5) {
        // Quick scan to find angle range where this ball gets hit
        std::vector<int> baseline = getBaselinePocketedBalls();
        
        double foundAngle = -1;
        for (double angle = 0; angle < 2 * M_PI; angle += 0.1) {
            ShotCandidate candidate = evaluateShotAtAngle(angle, baseline);
            if (std::find(candidate.pocketedBallIndices.begin(),
                         candidate.pocketedBallIndices.end(),
                         ballIndex) != candidate.pocketedBallIndices.end()) {
                foundAngle = angle;
                break;
            }
        }
        
        if (foundAngle < 0) {
            return {-1, -1}; // Ball not reachable
        }
        
        return {foundAngle - rangeWidth, foundAngle + rangeWidth};
    }
};

// Initialize statics
FastScannerConfig FastScanner::config;
std::vector<ShotCandidate> FastScanner::lastScanResults;
