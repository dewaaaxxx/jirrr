#pragma once

#include "include/input.h"
#include <cmath>

class PowerSlider
{
public:
    // ── State Machine Enum ────────────────────────────────────
    enum State
    {
        IDLE     = 0, ///< No active drag sequence
        DRAGGING = 1, ///< Touch-move in progress
        RELEASE  = 2, ///< Touch-up sent, finishing
    };

    PowerSlider() = default;

    // ── Public Interface ─────────────────────────────────────

    /// Begin a simulated drag of the power slider.
    ///
    /// @param sliderRect   Bounding rect of the power bar: ImVec4(x, y, w, h).
    /// @param targetPower  Desired shot power in internal units (0–666).
    /// @param dragSpeed    Fraction of the drag range covered per update (0–1).
    /// @param releaseZone  Fraction from the top at which to release the touch.
    void SimulateDrag(ImVec4 sliderRect, double targetPower,
                      float  dragSpeed,  float  releaseZone);

    /// Per-frame update; drives the drag state machine.
    void Update();

    // ── Public State ─────────────────────────────────────────

    /// True while a drag sequence is in progress.
    bool Active = false;

    /// The touch-slot index used for this slider.
    int TouchIndex = 8;

    /// Current state of the drag sequence.
    State state = IDLE;

private:
    ImVec4 m_rect         = {};
    double m_targetPower  = 0.0;
    float  m_dragSpeed    = 0.5f;
    float  m_releaseZone  = 0.5f;
    float  m_currentY     = 0.0f;
    float  m_targetY      = 0.0f;
    int    m_frameCounter = 0;
};
// ─── Implementation (header-only) ─────────────────────────

inline void PowerSlider::SimulateDrag(ImVec4 sliderRect, double targetPower,
                                      float dragSpeed, float releaseZone)
{
    m_rect        = sliderRect;
    m_targetPower = targetPower;
    m_dragSpeed   = dragSpeed;
    m_releaseZone = releaseZone;
    m_currentY    = sliderRect.y;
    m_targetY     = sliderRect.y + sliderRect.w * releaseZone;
    Active        = true;
    state         = DRAGGING;
    m_frameCounter = 0;
    // Send initial touch-down at the top of the slider (currentY)
    NativeTouchesBegin(TouchIndex, m_rect.x + m_rect.z * 0.5f, m_currentY);
}

inline void PowerSlider::Update()
{
    if (!Active) return;

    m_frameCounter++;

    if (state == DRAGGING)
    {
        // Move a fraction of the remaining distance each frame
        float step = (m_targetY - m_currentY) * m_dragSpeed;
        m_currentY += step;

        NativeTouchesMove(TouchIndex, m_rect.x + m_rect.z * 0.5f, m_currentY);

        // If we're within 1px of target, switch to release state
        if (std::abs(m_currentY - m_targetY) < 1.0f)
        {
            state = RELEASE;
        }
    }
    else if (state == RELEASE)
    {
        // End the touch and finish the drag sequence
        NativeTouchesEnd(TouchIndex, m_rect.x + m_rect.z * 0.5f, m_currentY);
        Active = false;
        state  = IDLE;
    }
}

// Provide a single-definition global instance for linkage.
inline PowerSlider powerSlider;
