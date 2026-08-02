#pragma once

#include "include/input.h"
#include "AutoPlay.h" // agar bisa mematikan AutoPlay saat simulate drag

extern struct Candidate;
extern Candidate g_CurrentCandidate;

#define ifl(cond) if ([&](){ bool b = (cond); if (b) LOGI(#cond); return b; }())
// #define ifln(cond) if ([&](){ bool b = (cond); if (!b) LOGI("!("#cond")"); return b; }())

extern Point2D lastFailedCuePos;

extern bool IsShotValid();

struct PowerSlider {
    bool Active = false;
    float ElapsedTime = 0.f, Duration = 0.f;
    float HoldTime = 0.f, HoldDuration = 0.f;
    ImVec2 StartPos;
    ImVec2 EndPos; // Max drag position
    ImVec2 TargetPos; // Actual target based on power
    ImVec2 CurrentPos;

    float ShotPower = 666.0f;
    int TouchIndex = 10; // High touch index

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

        this->Active = true;
        this->state = STARTING;
    }
    
    void Start(ImVec4 Rect) {
        float center = Rect.x + Rect.z / 2.0f;
        Start(ImVec2(center, Rect.y), ImVec2(center, Rect.y + Rect.w));
    }

    void End() {
        // pastikan power final dipaksa ke nilai yang diinginkan
        sharedGameManager.mVisualCue().mPower(ShotPowerToPower(this->ShotPower));
        // final move untuk memastikan posisi & event touch terakhir diterima
        NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
        LOGI("ending at power %f", sharedGameManager.mVisualCue().getShotPower(true));
        NativeTouchesEnd(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);

        this->Active = false;
        this->state = IDLE;
        g_CurrentCandidate.idx = -1;

        // jangan otomatis resume AutoPlay di sini; biarkan keputusan di caller jika perlu
    }

    void Cancel() {
        LOGI("Canceling power slider at power %f", sharedGameManager.mVisualCue().getShotPower(true));
        // Start return animation from current pos back to start
        this->EndPos = this->CurrentPos; // Use EndPos as start of return animation
        this->TargetPos = this->StartPos; // Return to origin
        this->ElapsedTime = 0.f;
        this->HoldTime = 0.f;
        this->Duration = 0.3f; // Fast return
        this->state = RETURNING;

        g_CurrentCandidate.idx = -1;
        lastFailedCuePos = { -1000.0, -1000.0 };
    }
    
    // DragTime is time to reach MAX power (666.0f)
    void SimulateDrag(ImVec4 Rect, float ShotPowerParam = 0.f, float DragTime = .7f, float HoldTime = 0.35f) {
        if (this->Active) return;
        
        // gunakan ShotPower yang diberikan, atau 666 jika 0
        this->ShotPower = (ShotPowerParam > 0.f) ? ShotPowerParam : 666.0f;
        float powerRatio = std::min(this->ShotPower / 666.0f, 1.0f);
        
        Start(Rect);

        // MATIKAN AutoPlay supaya tidak saling berebut input
        AutoPlay::bAutoPlaying = false;
        AutoPlay::didSetAngle = true;
        AutoPlay::lastSetAngle = sharedGameManager.mVisualCue().mVisualGuide().mAimAngle();
        
        // Calculate exact target position for the requested power
        this->TargetPos = ImVec2(
            this->StartPos.x + (this->EndPos.x - this->StartPos.x) * powerRatio,
            this->StartPos.y + (this->EndPos.y - this->StartPos.y) * powerRatio
        );
        
        // duration skala dengan ratio sehingga drag pendek untuk small power
        this->Duration = std::max(0.01f, DragTime * powerRatio);
        this->HoldDuration = HoldTime;
    }

    void Update() {
        if (!this->Active) return;
        
        float dt = ImGui::GetIO().DeltaTime;
        
        if (this->state == STARTING) {
            this->HoldTime += dt;
            // force set power tiap frame supaya game UI tidak mereset power
            sharedGameManager.mVisualCue().mPower(ShotPowerToPower(this->ShotPower));
            if (this->HoldTime >= this->HoldDuration * 2.f) {
                NativeTouchesBegin(this->TouchIndex, this->StartPos.x, this->StartPos.y);
                this->state = MOVING;
                this->HoldTime = 0.f;
            }
        }
        
        if (this->state == MOVING) {
            // Force set power tiap frame
            sharedGameManager.mVisualCue().mPower(ShotPowerToPower(this->ShotPower));

            this->ElapsedTime += dt;
            
            if (this->ElapsedTime < this->Duration) {
                // Calculate interpolated position
                float t = this->ElapsedTime / this->Duration;
                this->CurrentPos = ImVec2(
                    this->StartPos.x + (this->TargetPos.x - this->StartPos.x) * t,
                    this->StartPos.y + (this->TargetPos.y - this->StartPos.y) * t
                );

                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
            } else {
                // Pastikan kita mencapai target persis (jangan langsung cancel)
                this->CurrentPos = this->TargetPos;
                // paksa nilai power exact
                sharedGameManager.mVisualCue().mPower(ShotPowerToPower(this->ShotPower));
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
                this->state = ENDING;
            }

            if (dynamic_bool["DebugTouch"]) {
                ImDrawList* fg = ImGui::GetForegroundDrawList();
                fg->AddCircleFilled(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 100)); // Semi-transparent white circle
                fg->AddCircle(this->CurrentPos, 15.0f, IM_COL32(255, 255, 255, 200), 0.0f, 2.0f); // White outline
            }
        }

        if (this->state == ENDING) {
            this->HoldTime += dt;
            if (this->HoldTime >= this->HoldDuration) {
                if (IsShotValid()) {
                    this->End();
                } else {
                    LOGI("Shot invalid before release. Canceling.");
                    this->Cancel();
                }
            }
        }

        if (this->state == RETURNING) {
            this->ElapsedTime += dt;

            if (this->ElapsedTime < this->Duration) {
                float t = this->ElapsedTime / this->Duration;
                // Interpolate from EndPos (captured CurrentPos) to TargetPos (StartPos)
                this->CurrentPos = ImVec2(
                    this->EndPos.x + (this->TargetPos.x - this->EndPos.x) * t,
                    this->EndPos.y + (this->TargetPos.y - this->EndPos.y) * t
                );
                NativeTouchesMove(this->TouchIndex, this->CurrentPos.x, this->CurrentPos.y);
            } else {
                // Finished returning
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
