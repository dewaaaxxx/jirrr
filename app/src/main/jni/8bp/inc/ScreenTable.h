#pragma once

// ============================================================
// SCREEN TABLE - FIX UNTUK 720x1600
// ============================================================

// Reference resolution (dari game)
inline constexpr double REF_WIDTH = 1280.0;
inline constexpr double REF_HEIGHT = 640.0;

// Table anchor points in reference coordinates
inline constexpr double REF_TABLE_LEFT = 207.0;
inline constexpr double REF_TABLE_RIGHT = 1072.0;
inline constexpr double REF_TABLE_TOP = 171.0;
inline constexpr double REF_TABLE_BOTTOM = 584.0;
inline constexpr double REF_TABLE_WIDTH = REF_TABLE_RIGHT - REF_TABLE_LEFT; // 865.0

// Global table variables
inline double TABLE_LEFT = 0.0;
inline double TABLE_TOP = 0.0;
inline double TABLE_RIGHT = 0.0;
inline double TABLE_BOTTOM = 0.0;
inline double TABLE_SCALE = 1.0;

// ===== WorldToScreen =====
ImVec2 WorldToScreen(Vec2d worldPos) {
    double positionX = worldPos.x + TABLE_HALF_WIDTH;
    double positionY = -(worldPos.y + TABLE_HALF_HEIGHT);
    double scrX = TABLE_LEFT + positionX * TABLE_SCALE;
    double scrY = TABLE_BOTTOM + positionY * TABLE_SCALE;
    return ImVec2(scrX, scrY);
}

// ===== UpdateScreenTable - FIX UNTUK 720x1600 =====
void UpdateScreenTable() {
    // ===== CEK APAKAH KALIBRASI DIGUNAKAN =====
    if (g_useCalibration && persistent_float["fTableLeft"] > 0.0f && persistent_float["fTableRight"] > 0.0f) {
        TABLE_LEFT = persistent_float["fTableLeft"];
        TABLE_RIGHT = persistent_float["fTableRight"];
        TABLE_TOP = persistent_float["fTableTop"];
        TABLE_BOTTOM = persistent_float["fTableBottom"];
        TABLE_SCALE = (TABLE_RIGHT - TABLE_LEFT) / REF_TABLE_WIDTH;
        LOGI("TABLE [CALIBRATION]: L=%.1f R=%.1f T=%.1f B=%.1f", 
             TABLE_LEFT, TABLE_RIGHT, TABLE_TOP, TABLE_BOTTOM);
        return;
    }

    // ===== DEFAULT: PAKE SCALING DARI REF_TABLE_* =====
    double heightScale = Height / REF_HEIGHT;
    double scaledRefWidth = heightScale * REF_WIDTH;
    double offsetX = (Width - scaledRefWidth) / 2.0;
    
    TABLE_LEFT = offsetX + (heightScale * REF_TABLE_LEFT);
    TABLE_RIGHT = offsetX + (heightScale * REF_TABLE_RIGHT);
    TABLE_TOP = heightScale * REF_TABLE_TOP;
    TABLE_BOTTOM = heightScale * REF_TABLE_BOTTOM;
    TABLE_SCALE = (TABLE_RIGHT - TABLE_LEFT) / REF_TABLE_WIDTH;
    
    LOGI("TABLE [DEFAULT SCALED]: L=%.1f R=%.1f T=%.1f B=%.1f", 
         TABLE_LEFT, TABLE_RIGHT, TABLE_TOP, TABLE_BOTTOM);
}
