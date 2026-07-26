#pragma once

#include "include/input.h"

extern struct Candidate;
extern Candidate g_CurrentCandidate;

#define ifl(cond) if ([&](){ bool b = (cond); if (b) LOGI(#cond); return b; }())

extern Point2D lastFailedCuePos;

struct PowerSlider {
    bool Active = false;
    float ElapsedTime = 0.f, Duration = 0.f;
    float HoldTime = 0.f, HoldDuration = 0.f;
    ImVec2 StartPos;
    ImVec2 EndPos;
    ImVec2 TargetPos;
    ImVec2 CurrentPos;

    float ShotPower = 666.0f;
    int TouchIndex = 10;
    int _correctionAttempts = 0; // FIX: counter untuk read-back correction

    enum State {
        IDLE,
        STARTING,
        MOVING,
        ENDING,
        RETURNING,
    } state = IDLE;
    
    void Start(ImVec2 start, ImVec2 end) {
        this->StartPos = start;
        this->EndPos = end;
        this->CurrentPos = this->StartPos;
        this->ElapsedTime = 0.f;
        this->HoldTime = 0.f;
        this->_correctionAttempts = 0;
        this->Active = true;
        this->state = STARTING;
    }
    
    void Start(ImVec4 Rect) {
        float center = Rect.x + Rect.z / 2.0f;
        Start(ImVec2(center, Rect.y), ImVec2(center, Rect.y + Rect.w));
    }

    void End() {
        LOGI("ending at power %f", sharedGameManager.mVisualCue().getShotPower(true));
        NativeTouchesEnd(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
        this->Active = false;
        this->state = IDLE;
        this->_correctionAttempts = 0;
        g_CurrentCandidate.idx = -1;
    }

    void Cancel() {
        LOGI("Canceling power slider at power %f", sharedGameManager.mVisualCue().getShotPower(true));
        this->EndPos = this->CurrentPos;
        this->TargetPos = this->StartPos;
        this->ElapsedTime = 0.f;
        this->HoldTime = 0.f;
        this->Duration = 0.3f;
        this->state = RETURNING;
        this->_correctionAttempts = 0;
        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
    }
    
    void SimulateDrag(ImVec4 Rect, float ShotPower = 0.f, float DragTime = .7f, float HoldTime = 0.35f) {
        if (this->Active) return;
        
        this->ShotPower = ShotPower > 0.f ? ShotPower : 666.0f;

        // FIX BUG 1: Game 8BP mapping slider bisa non-linear.
        // Dari observasi: game pakai mapping mendekati sqrt untuk power → posisi.
        // powerRatio_visual = (targetPower / 666)^(1/curve) dimana curve ≈ 1.35
        // Ini mengkompensasi game yang set power lebih tinggi dari posisi linear.
        float linearRatio = std::min(this->ShotPower / 666.0f, 1.0f);
        // Apply inverse curve: kalau game pakai p = ratio^1.35 * 666,
        // maka ratio_drag = (p/666)^(1/1.35) = linearRatio^0.741
        float powerRatio = powf(linearRatio, 0.741f);
        
        Start(Rect);
        
        this->TargetPos = ImVec2(
            this->StartPos.x + (this->EndPos.x - this->StartPos.x) * powerRatio,
            this->StartPos.y + (this->EndPos.y - this->StartPos.y) * powerRatio
        );
        
        this->Duration = DragTime * powerRatio;
        this->HoldDuration = HoldTime;
    }

    void Update() {
        if (!this->Active) return;
        
        float dt = ImGui::GetIO().DeltaTime;
        
        if (this->state == STARTING) {
            this->HoldTime += dt;
            if (this->HoldTime >= this->HoldDuration * 2.f) {
                NativeTouchesBegin(this->TouchIndex, this->StartPos.x, this->StartPos.y);
                this->state = MOVING;
                this->HoldTime = 0.f;
            }
        }
        
        if (this->state == MOVING) {
            this->ElapsedTime += dt;
            
            if (this->ElapsedTime < this->Duration) {
                float t = this->ElapsedTime / this->Duration;
                this->CurrentPos = ImVec2(
                    this->StartPos.x + (this->TargetPos.x - this->StartPos.x) * t,
                    this->StartPos.y + (this->TargetPos.y - this->StartPos.y) * t
                );
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
            } else {
                this->CurrentPos = this->TargetPos;
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
                this->HoldTime = 0.f;
                this->state = ENDING;
            }

            if (dynamic_bool["DebugTouch"]) {
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                fg->AddCircleFilled(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 100));
                fg->AddCircle(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 200), 0.0f, 2.0f);
            }
        }

        if (this->state == ENDING) {
            this->HoldTime += dt;
            if (this->HoldTime >= this->HoldDuration) {
                // ── FIX BUG 2: READ-BACK POWER VERIFICATION ────────────────
                // Baca power aktual dari game setelah drag selesai.
                // Kalau beda > tolerance → koreksi posisi slider, tunggu lagi.
                // Max 5 koreksi supaya tidak infinite loop.
                double actualPower = (double)sharedGameManager.mVisualCue().getShotPower(true);
                double error       = actualPower - (double)this->ShotPower;
                double tolerance   = 8.0; // world units ≈ 1.2% dari max power

                if (std::abs(error) > tolerance && this->_correctionAttempts < 5) {
                    // Koreksi proporsional: error positif = terlalu kuat → geser ke atas (Y berkurang)
                    // error negatif = terlalu lemah → geser ke bawah (Y bertambah)
                    float posRange = this->EndPos.y - this->StartPos.y;
                    if (std::abs(posRange) > 1.0f) {
                        // Delta posisi berdasarkan ratio error terhadap full range
                        float correctionY = (float)(error / 666.0) * posRange * 0.85f;
                        this->CurrentPos.y -= correctionY;
                        // Clamp ke range valid slider
                        float minY = std::min(this->StartPos.y, this->EndPos.y);
                        float maxY = std::max(this->StartPos.y, this->EndPos.y);
                        this->CurrentPos.y = std::max(minY, std::min(maxY, this->CurrentPos.y));
                        NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
                        this->HoldTime = 0.f; // reset, tunggu game update power
                        this->_correctionAttempts++;
                        LOGI("PowerSlider: correction %d actual=%.1f target=%.1f err=%.1f",
                             this->_correctionAttempts, actualPower, this->ShotPower, error);
                        return;
                    }
                }

                LOGI("PowerSlider: END actual=%.1f target=%.1f attempts=%d",
                     actualPower, this->ShotPower, this->_correctionAttempts);
                this->_correctionAttempts = 0;
                this->End();
            }
        }

        if (this->state == RETURNING) {
            this->ElapsedTime += dt;
            if (this->ElapsedTime < this->Duration) {
                float t = this->ElapsedTime / this->Duration;
                this->CurrentPos = ImVec2(
                    this->EndPos.x + (this->TargetPos.x - this->EndPos.x) * t,
                    this->EndPos.y + (this->TargetPos.y - this->EndPos.y) * t
                );
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
            } else {
                this->CurrentPos = this->TargetPos;
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
                End();
            }

            if (dynamic_bool["DebugTouch"]) {
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                fg->AddCircleFilled(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 100));
                fg->AddCircle(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 200), 0.0f, 2.0f);
            }
        }
    }
};

PowerSlider powerSlider;
