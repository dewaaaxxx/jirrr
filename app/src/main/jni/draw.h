// ======================================================================
// 📁 draw.h - الملف الكامل مع جميع التأثيرات النيونية المتطورة
// ======================================================================

#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>
#include <ctime>

#include "resources.h"

using namespace ImGui;
using namespace std;

#include "include/includes.h"

#include "8bp.h"
#include "8bp/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "keylogin.h"
#include "oxorany/oxorany.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "8bp/inc/PhysicsModel.h"
#include "8bp/inc/GameSpeedControl.h"

// ============================================
// دالة رسم الخطوط المتقطعة
// ============================================
static inline float AddDashedLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col, float thickness, float dash_len = 20.0f, float gap_len = 15.0f, float phase_offset = 0.0f) {
    ImVec2 dir = ImVec2(p2.x - p1.x, p2.y - p1.y);
    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (dist < 1.0f) return phase_offset;
    dir.x /= dist; dir.y /= dist;

    float cycle = dash_len + gap_len;
    float curr = -fmodf(phase_offset, cycle);
    if (curr > 0.0f) curr -= cycle;
    while (curr < dist) {
        float dashStart = curr;
        float dashEnd   = curr + dash_len;
        float drawStart = ImMax(dashStart, 0.0f);
        float drawEnd   = ImMin(dashEnd,   dist);
        if (drawStart < drawEnd) {
            dl->AddLine(
                ImVec2(p1.x + dir.x * drawStart, p1.y + dir.y * drawStart),
                ImVec2(p1.x + dir.x * drawEnd,   p1.y + dir.y * drawEnd),
                col, thickness);
        }
        curr += cycle;
    }
    return fmodf(phase_offset + dist, cycle);
}

// ============================================
// التعريفات الأساسية
// ============================================
extern Candidate g_CurrentCandidate;
#define currentMode               AutoPlay::currentMode
#define MODE_OFF                  AutoPlay::MODE_OFF
#define MODE_AUTO_PLAY            AutoPlay::MODE_AUTO_PLAY
#define MODE_AUTO_AIM             AutoPlay::MODE_AUTO_AIM
#define bShowAutoPlayLines        AutoPlay::bShowAutoPlayLines
#define bCueBallIsMovingOrDragging AutoPlay::bCueBallIsMovingOrDragging
#define g_PredictionLocked        AutoPlay::g_PredictionLocked
#define bAutoPlaySwitch           AutoPlay::bAutoPlaySwitch
#define bAutoAimSwitch            AutoPlay::bAutoAimSwitch
#define NOMINATING_HUMAN          AutoPlay::NOMINATING_HUMAN
#define pendingShotAngle          AutoPlay::pendingShotAngle
#define pendingShotPower          AutoPlay::pendingShotPower
#define targetAngle               AutoPlay::targetAngle

extern int64_t g_ExpiryTime;

static float menuScale = 1.0f;

// ============================================
// المتغيرات الناقصة للـ ESP
// ============================================
static bool DEBUG_BYPASS_LOGIN = true;
static uint64_t g_AuthToken = 0xDEADBEEFCAFEBABE;

// ============================================
// الألوان الأساسية للكرات
// ============================================
static ImU32 ballColors[] = {
    IM_COL32(255, 255, 255, 255),
    IM_COL32(255, 215, 0, 255),
    IM_COL32(0, 0, 255, 255),
    IM_COL32(255, 0, 0, 255),
    IM_COL32(100, 0, 255, 255),    
    IM_COL32(255, 128, 0, 255),
    IM_COL32(0, 128, 0, 255),
    IM_COL32(139, 0, 0, 255),
    IM_COL32(0, 0, 0, 255),
    IM_COL32(255, 215, 0, 255),
    IM_COL32(0, 0, 255, 255),
    IM_COL32(255, 0, 0, 255),
    IM_COL32(100, 0, 255, 255),
    IM_COL32(255, 128, 0, 255),
    IM_COL32(0, 128, 0, 255),
    IM_COL32(139, 0, 0, 255),
};

static ImU32 GetBallColor(int ballIndex) {
    if (ballIndex >= 0 && ballIndex < 16) {
        return ballColors[ballIndex];
    }
    return IM_COL32(255, 255, 255, 255);
}

// ============================================
// ألوان النيون المتطورة
// ============================================
#define NEON_CYAN      IM_COL32(0, 255, 255, 255)
#define NEON_PURPLE    IM_COL32(180, 0, 255, 255)
#define NEON_GOLD      IM_COL32(255, 215, 0, 255)
#define NEON_PINK      IM_COL32(255, 0, 150, 255)
#define NEON_BLUE      IM_COL32(0, 150, 255, 255)
#define NEON_GREEN     IM_COL32(0, 255, 100, 255)
#define NEON_ORANGE    IM_COL32(255, 165, 0, 255)
#define NEON_RED       IM_COL32(255, 0, 0, 255)
#define NEON_WHITE     IM_COL32(255, 255, 255, 255)

#define COL_BG_DEEP      IM_COL32(11, 18, 32, 250)
#define COL_BG_DARK      IM_COL32(16, 26, 44, 250)
#define COL_PANEL        IM_COL32(22, 32, 52, 255)
#define COL_PANEL_SOFT   IM_COL32(28, 40, 62, 255)
#define COL_TEXT         IM_COL32(235, 238, 245, 255)
#define COL_TEXT_DIM     IM_COL32(160, 168, 185, 255)
#define COL_TEXT_FAINT   IM_COL32(120, 128, 145, 255)

// ============================================
// دوال المساعدة
// ============================================
static const char* L(const char* en, const char* ar) {
    return (persistent_int["iLang"] == 1) ? ar : en;
}

static float EaseOutBack(float x) {
    const float c1 = 1.70158f, c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1, 3) + c1 * powf(x - 1, 2);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c1, ImU32 c2, bool horizontal = true) {
    if (horizontal) dl->AddRectFilledMultiColor(a, b, c1, c2, c2, c1);
    else dl->AddRectFilledMultiColor(a, b, c1, c1, c2, c2);
}

static void DrawBoldText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text) {
    const float o = 0.9f;
    dl->AddText(ImVec2(pos.x + o, pos.y), col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y), col, text);
    dl->AddText(ImVec2(pos.x, pos.y + o), col, text);
    dl->AddText(ImVec2(pos.x, pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y + o), col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y + o), col, text);
    dl->AddText(pos, col, text);
}

static void DrawOrnateFrame(ImDrawList* dl, ImVec2 a, ImVec2 b) {
    dl->AddRect(a, b, NEON_CYAN, 12.0f, 0, 2.5f);
    float L_ = 28.0f;
    dl->AddLine(ImVec2(a.x, a.y + L_), ImVec2(a.x + L_, a.y), NEON_CYAN, 3.0f);
    dl->AddLine(ImVec2(b.x - L_, a.y), ImVec2(b.x, a.y + L_), NEON_CYAN, 3.0f);
    dl->AddLine(ImVec2(a.x, b.y - L_), ImVec2(a.x + L_, b.y), NEON_CYAN, 3.0f);
    dl->AddLine(ImVec2(b.x - L_, b.y), ImVec2(b.x, b.y - L_), NEON_CYAN, 3.0f);
    dl->AddRect(a, b, IM_COL32(150, 0, 255, 80), 12.0f, 0, 1.0f);
}

// ============================================
// أيقونات التبويبات المتطورة
// ============================================
static void Icon_Draw(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddLine(ImVec2(c.x - 10, c.y + 8), ImVec2(c.x + 10, c.y - 8), col, 3.0f);
    dl->AddLine(ImVec2(c.x - 10, c.y + 8), ImVec2(c.x - 2, c.y + 12), col, 3.0f);
    dl->AddLine(ImVec2(c.x - 2, c.y + 12), ImVec2(c.x + 10, c.y - 8), col, 3.0f);
    dl->AddCircleFilled(ImVec2(c.x + 10, c.y - 8), 3.0f, col, 16);
}

static void Icon_Play(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 14.0f, col, 48, 2.5f);
    dl->AddCircle(c, 10.0f, IM_COL32(0, 10, 30, 200), 48);
    ImVec2 p1(c.x - 5.0f, c.y - 8.0f);
    ImVec2 p2(c.x - 5.0f, c.y + 8.0f);
    ImVec2 p3(c.x + 9.0f, c.y);
    dl->AddTriangleFilled(p1, p2, p3, col);
}

static void Icon_Queue(ImDrawList* dl, ImVec2 c, ImU32 col) {
    for (int i = 0; i < 3; i++) {
        float y = c.y - 8.0f + i * 8.0f;
        dl->AddRectFilled(ImVec2(c.x - 10, y), ImVec2(c.x + 10, y + 4), col, 2.0f);
    }
}

static void Icon_Extra(ImDrawList* dl, ImVec2 c, ImU32 col) {
    const float r1 = 12.0f, r2 = 8.0f;
    ImVector<ImVec2> pts;
    for (int i = 0; i < 16; i++) {
        float a = (IM_PI * 2.0f / 16.0f) * i;
        float r = (i % 2 == 0) ? r1 : r2;
        pts.push_back(ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r));
    }
    dl->AddConvexPolyFilled(pts.Data, pts.Size, col);
    dl->AddCircleFilled(c, 4.0f, IM_COL32(0, 20, 40, 255), 32);
    dl->AddCircle(c, r1 + 3, col, 32, 1.5f);
}

static void Icon_Info(ImDrawList* dl, ImVec2 c, ImU32 col) {
    dl->AddCircle(c, 13.0f, col, 48, 2.5f);
    dl->AddCircle(c, 9.0f, IM_COL32(0, 10, 30, 200), 48);
    dl->AddCircleFilled(ImVec2(c.x, c.y - 5.0f), 2.5f, col, 16);
    dl->AddRectFilled(ImVec2(c.x - 2.0f, c.y - 1.0f), ImVec2(c.x + 2.0f, c.y + 8.5f), col, 1.5f);
}

static void DrawTabIcon(ImDrawList* dl, int tab, ImVec2 c, ImU32 col) {
    switch (tab) {
        case 0: Icon_Draw(dl, c, col); break;
        case 1: Icon_Play(dl, c, col); break;
        case 2: Icon_Queue(dl, c, col); break;
        case 3: Icon_Extra(dl, c, col); break;
        case 4: Icon_Info(dl, c, col); break;
        default: Icon_Extra(dl, c, col); break;
    }
}

// ============================================
// هيكل القائمة
// ============================================
struct MenuState {
    bool isOpen = false;
    int currentTab = 0;
    float sidebarW = 860.0f;
    float menuAlpha = 0.0f;
    float menuScale = 0.92f;
    bool hideForCapture = false;
};
static MenuState g_menu;

struct ShotApprovalState {
    bool active = false;
    bool approved = false;
    bool turnHandled = false;
    int rejectCount = 0;
    float accuracy = 0.0f;
    float aiPower = 0.75f;
    bool hasShot = false;
    std::chrono::steady_clock::time_point shownAt;
};
static ShotApprovalState g_shotApproval;

static ImFont* g_ArialBlackFont = nullptr;
static ImFont* g_SegoeUIFont = nullptr;
static ImFont* g_IconFont = nullptr;

// ============================================
// دوال القائمة
// ============================================
static const char* CurrentTabTitle() {
    static const char* en[5] = { "Draw", "Play", "Queue", "Extra", "Info" };
    static const char* ar[5] = { "ﻢﺳﺭ", "ﺐﻌﻟ", "ﺭﻭﺩ", "ﻲﻓﺎﺿﺇ", "ﺕﺎﻣﻮﻠﻌﻣ" };
    int t = g_menu.currentTab;
    if (t < 0 || t > 4) t = 0;
    return L(en[t], ar[t]);
}

// ============================================
// زر جانبي محسن مع تأثيرات نيونية متطورة
// ============================================
static bool GoldSidebarButton(const char* label, const char* icon, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width - 16.0f, 86.0f);
    const ImRect bb(pos, pos + size);
    ItemSize(size, g.Style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    static std::map<ImGuiID, float> anim;
    float& t = anim[id];
    float target = (selected ? 1.0f : (hovered ? 0.5f : 0.0f));
    t += (target - t) * g.IO.DeltaTime * 12.0f;

    ImDrawList* dl = window->DrawList;
    static float time = 0.0f;
    time += g.IO.DeltaTime;

    // 6 طبقات من التوهج الخارجي
    for (int i = 6; i > 0; i--) {
        float alpha = (7 - i) * 12.0f * (0.5f + 0.5f * sinf(time * 0.8f + i * 0.5f));
        ImU32 glowCol = selected ? NEON_CYAN : (hovered ? NEON_PURPLE : IM_COL32(100, 0, 200, 80));
        dl->AddRect(ImVec2(bb.Min.x - i * 2 - 2.0f * sinf(time + i), bb.Min.y - i * 2),
                    ImVec2(bb.Max.x + i * 2 + 2.0f * sinf(time * 0.7f + i), bb.Max.y + i * 2),
                    IM_COL32(glowCol >> 16 & 0xFF, glowCol >> 8 & 0xFF, glowCol & 0xFF, (int)alpha), 0, 1.5f);
    }

    if (selected) {
        dl->AddRectFilledMultiColor(bb.Min, bb.Max,
            IM_COL32(0, 40, 70, 255), IM_COL32(0, 30, 50, 255),
            IM_COL32(0, 20, 40, 255), IM_COL32(0, 35, 60, 255));
        dl->AddRect(bb.Min, bb.Max, NEON_CYAN, 12.0f, 0, 2.0f);
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(0, 200, 255, 20), 12.0f);
        dl->AddRectFilled(ImVec2(bb.Max.x - 4, bb.Min.y + 10),
                          ImVec2(bb.Max.x, bb.Max.y - 10), NEON_CYAN, 2.0f);
        
        // نقاط ضوئية متحركة على الزر المحدد
        for (int i = 0; i < 4; i++) {
            float px = bb.Min.x + 10 + i * ((bb.Max.x - bb.Min.x - 20) / 3.0f);
            float py = bb.Max.y - 6 + 4.0f * sinf(time * 2.0f + i * 1.2f);
            dl->AddCircleFilled(ImVec2(px, py), 2.0f + 1.0f * sinf(time * 3.0f + i), NEON_CYAN, 8);
        }
    } else if (t > 0.01f) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(20, 30, 60, (int)(80 * t)), 12.0f);
    }

    ImU32 textCol = selected ? IM_COL32(0, 255, 255, 255) : IM_COL32(180, 200, 220, (int)(200 + 55 * t));
    ImU32 iconCol = selected ? NEON_CYAN : IM_COL32(150, 180, 220, (int)(180 + 75 * t));

    int tabIdx = 0;
    if (icon && icon[0] >= '0' && icon[0] <= '9') tabIdx = icon[0] - '0';
    ImVec2 iconCenter = ImVec2(bb.Min.x + 36.0f, bb.Min.y + size.y * 0.5f);

    // خلفية الأيقونة مع توهج
    dl->AddCircleFilled(iconCenter, 25.0f,
        selected ? IM_COL32(0, 20, 40, 255) : IM_COL32(10, 15, 30, 255), 32);
    dl->AddCircle(iconCenter, 25.0f,
        selected ? NEON_CYAN : IM_COL32(60, 80, 120, 200), 32, 1.8f);
    
    // توهج إضافي للأيقونة عند التحديد
    if (selected) {
        dl->AddCircle(iconCenter, 30.0f + 4.0f * sinf(time * 2.0f), 
                     IM_COL32(0, 255, 255, 60), 32, 1.5f);
    }
    
    DrawTabIcon(dl, tabIdx, iconCenter, iconCol);

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 textPos = ImVec2(bb.Min.x + 78.0f, bb.Min.y + (size.y - textSize.y) * 0.5f);
    DrawBoldText(dl, textPos, textCol, label);

    return pressed;
}

// ============================================
// مفتاح تبديل محسن مع تأثيرات نيونية متطورة
// ============================================
static bool GoldToggle(const char* label, const char* sub, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    float th = 30.0f, tw = 54.0f, r = th * 0.5f;
    ImVec2 pos = window->DC.CursorPos;
    float rowH = 64.0f;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, rowH);
    const ImRect bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> anim;
    float& t = anim[id];
    float target = *v ? 1.0f : 0.0f;
    t += (target - t) * g.IO.DeltaTime * 14.0f;

    ImDrawList* dl = window->DrawList;
    ImGuiIO& io = ImGui::GetIO();
    static float time = 0.0f;
    time += io.DeltaTime;

    // 5 طبقات من التوهج الخارجي
    for (int i = 5; i > 0; i--) {
        float alpha = (6 - i) * 14.0f * (0.5f + 0.5f * sinf(time * 0.6f + i * 0.7f));
        ImU32 glowCol = *v ? NEON_GREEN : (hovered ? NEON_PURPLE : NEON_PURPLE);
        dl->AddRect(ImVec2(bb.Min.x - i * 2, bb.Min.y - i * 2),
                    ImVec2(bb.Max.x + i * 2, bb.Max.y + i * 2),
                    IM_COL32(glowCol >> 16 & 0xFF, glowCol >> 8 & 0xFF, glowCol & 0xFF, (int)alpha), 0, 1.5f);
    }

    ImU32 bgCol = hovered ? IM_COL32(0, 20, 50, 200) : IM_COL32(0, 10, 30, 180);
    dl->AddRectFilled(bb.Min, bb.Max, bgCol, 12.0f);
    dl->AddRect(bb.Min, bb.Max, *v ? NEON_GREEN : NEON_PURPLE, 12.0f, 0, 1.5f);

    // خط مسح ضوئي متحرك
    static float toggleScan = 0.0f;
    toggleScan += io.DeltaTime * 1.5f;
    float scanP = fmod(toggleScan, 1.0f);
    ImU32 scanCol = *v ? NEON_GREEN : NEON_PURPLE;
    dl->AddLine(ImVec2(bb.Min.x + scanP * bb.GetWidth(), bb.Min.y),
                ImVec2(bb.Min.x + scanP * bb.GetWidth(), bb.Max.y),
                IM_COL32(scanCol >> 16 & 0xFF, scanCol >> 8 & 0xFF, scanCol & 0xFF, 80), 1.5f);

    // النص مع تأثير توهج
    ImVec2 ts = CalcTextSize(label);
    dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12), COL_TEXT, label);
    if (sub && *sub) dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12 + ts.y + 4), COL_TEXT_FAINT, sub);

    // زر التبديل المتطور
    ImVec2 togPos = ImVec2(bb.Max.x - tw - 18.0f, bb.Min.y + (rowH - th) * 0.5f);
    ImVec2 togEnd = ImVec2(togPos.x + tw, togPos.y + th);

    ImU32 offCol = IM_COL32(20, 30, 60, 255);
    ImU32 onCol = NEON_GREEN;
    ImU32 curCol = *v ? onCol : offCol;
    dl->AddRectFilled(togPos, togEnd, curCol, r);
    
    // إطار متوهج للزر
    if (*v) {
        dl->AddRect(togPos, togEnd, NEON_GREEN, r, 0, 1.5f);
        // نقاط ضوئية حول الزر عند التفعيل
        for (int i = 0; i < 3; i++) {
            float px = togPos.x + 6 + i * ((tw - 12) / 2.0f);
            float py = togPos.y - 4 + 3.0f * sinf(time * 2.0f + i * 1.5f);
            dl->AddCircleFilled(ImVec2(px, py), 1.5f, NEON_GREEN, 8);
        }
    }

    // الدائرة المتحركة
    float kx = togPos.x + r + (tw - th) * t;
    float ky = togPos.y + r;
    float kr = r - 4.0f;
    dl->AddCircleFilled(ImVec2(kx, ky + 1), kr + 2.0f, IM_COL32(0, 0, 0, 80));
    dl->AddCircleFilled(ImVec2(kx, ky), kr, IM_COL32(255, 255, 255, 255));
    dl->AddCircle(ImVec2(kx, ky), kr, NEON_CYAN, 16, 1.0f);
    
    // توهج الدائرة
    if (*v) {
        dl->AddCircle(ImVec2(kx, ky), kr + 4.0f + 2.0f * sinf(time * 2.0f), 
                     IM_COL32(0, 255, 100, 60), 16, 1.0f);
    }

    return pressed;
}

// ============================================
// شريط تمرير محسن مع تأثيرات نيونية
// ============================================
static bool GoldSliderFloat(const char* label, const char* sub, float* v, float vmin, float vmax, const char* fmt) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    float rowH = 78.0f;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, rowH);
    const ImRect bb(pos, pos + size);
    ItemSize(size);
    if (!ItemAdd(bb, id)) return false;

    ImDrawList* dl = window->DrawList;
    ImGuiIO& io = ImGui::GetIO();
    static float time = 0.0f;
    time += io.DeltaTime;

    for (int i = 4; i > 0; i--) {
        float alpha = (5 - i) * 12.0f * (0.5f + 0.5f * sinf(time * 0.5f + i * 0.7f));
        dl->AddRect(ImVec2(bb.Min.x - i * 2, bb.Min.y - i * 2),
                    ImVec2(bb.Max.x + i * 2, bb.Max.y + i * 2),
                    IM_COL32(0, 200, 255, (int)alpha), 0, 1.0f);
    }

    dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(0, 10, 30, 200), 12.0f);
    dl->AddRect(bb.Min, bb.Max, NEON_CYAN, 12.0f, 0, 1.5f);

    static float sliderScan = 0.0f;
    sliderScan += io.DeltaTime * 1.2f;
    float scanP = fmod(sliderScan, 1.0f);
    dl->AddLine(ImVec2(bb.Min.x + scanP * bb.GetWidth(), bb.Min.y),
                ImVec2(bb.Min.x + scanP * bb.GetWidth(), bb.Max.y),
                IM_COL32(0, 255, 255, 60), 1.0f);

    ImVec2 ts = CalcTextSize(label);
    dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 10), COL_TEXT, label);
    if (sub && *sub) dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 10 + ts.y + 4), COL_TEXT_FAINT, sub);

    float trackW = bb.GetWidth() - 36.0f - 70.0f;
    ImVec2 trackA = ImVec2(bb.Min.x + 18, bb.Max.y - 18);
    ImVec2 trackB = ImVec2(trackA.x + trackW, trackA.y + 4);

    float tnorm = (*v - vmin) / (vmax - vmin);
    tnorm = ImClamp(tnorm, 0.0f, 1.0f);

    ImRect grabBB(trackA, ImVec2(trackB.x, trackB.y + 16));
    bool hovered = IsMouseHoveringRect(grabBB.Min, ImVec2(grabBB.Max.x, grabBB.Max.y + 8));
    bool changed = false;
    if (hovered && g.IO.MouseDown[0]) SetActiveID(id, window);
    if (g.ActiveId == id) {
        if (g.IO.MouseDown[0]) {
            float nx = (g.IO.MousePos.x - trackA.x) / trackW;
            nx = ImClamp(nx, 0.0f, 1.0f);
            float nv = vmin + nx * (vmax - vmin);
            if (nv != *v) { *v = nv; changed = true; }
        } else ClearActiveID();
    }

    dl->AddRectFilled(trackA, trackB, IM_COL32(10, 20, 50, 255), 2.0f);
    ImVec2 fillEnd = ImVec2(trackA.x + trackW * tnorm, trackB.y);
    dl->AddRectFilledMultiColor(trackA, fillEnd,
        IM_COL32(0, 100, 200, 255), NEON_CYAN,
        NEON_CYAN, IM_COL32(0, 100, 200, 255));

    float kx = trackA.x + trackW * tnorm;
    float ky = (trackA.y + trackB.y) * 0.5f;
    
    // المقبض المتوهج
    dl->AddCircleFilled(ImVec2(kx, ky + 1), 12.0f, IM_COL32(0, 0, 0, 100));
    dl->AddCircleFilled(ImVec2(kx, ky), 11.0f, NEON_CYAN);
    dl->AddCircle(ImVec2(kx, ky), 14.0f, NEON_CYAN, 0, 1.5f);
    dl->AddCircle(ImVec2(kx, ky), 16.0f + 3.0f * sinf(time * 2.0f), 
                 IM_COL32(0, 255, 255, 40), 0, 1.0f);

    char buf[32]; snprintf(buf, sizeof(buf), fmt, *v);
    ImVec2 vs = CalcTextSize(buf);
    dl->AddText(ImVec2(bb.Max.x - 18 - vs.x, ky - vs.y * 0.5f), NEON_CYAN, buf);

    return changed;
}

static bool GoldSliderInt(const char* label, const char* sub, int* v, int vmin, int vmax) {
    float fv = (float)*v;
    if (GoldSliderFloat(label, sub, &fv, (float)vmin, (float)vmax, "%.0f")) {
        *v = (int)(fv + 0.5f);
        return true;
    }
    return false;
}

// ============================================
// قائمة منسدلة محسنة مع تأثيرات نيونية
// ============================================
static bool GoldCombo(const char* label, const char* sub, int* val, const char* items_z) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImVec2 pos = window->DC.CursorPos;
    float rowH = 64.0f;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, rowH);
    const ImRect bb(pos, pos + size);

    ImDrawList* dl = window->DrawList;
    ImGuiIO& io = ImGui::GetIO();
    static float time = 0.0f;
    time += io.DeltaTime;

    for (int i = 4; i > 0; i--) {
        float alpha = (5 - i) * 12.0f * (0.5f + 0.5f * sinf(time * 0.6f + i * 0.5f));
        dl->AddRect(ImVec2(bb.Min.x - i * 2, bb.Min.y - i * 2),
                    ImVec2(bb.Max.x + i * 2, bb.Max.y + i * 2),
                    IM_COL32(150, 0, 255, (int)alpha), 0, 1.0f);
    }

    dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(10, 0, 30, 200), 12.0f);
    dl->AddRect(bb.Min, bb.Max, NEON_PURPLE, 12.0f, 0, 1.5f);

    ImVec2 ts = CalcTextSize(label);
    dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12), COL_TEXT, label);
    if (sub && *sub) dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12 + ts.y + 4), COL_TEXT_FAINT, sub);

    SetCursorScreenPos(ImVec2(bb.Max.x - 180 - 18, bb.Min.y + (rowH - 30) * 0.5f));
    PushStyleColor(ImGuiCol_FrameBg, IM_COL32(10, 0, 30, 255));
    PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(20, 0, 50, 255));
    PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(30, 0, 60, 255));
    PushStyleColor(ImGuiCol_Button, NEON_PURPLE);
    PushStyleColor(ImGuiCol_Text, IM_COL32(200, 150, 255, 255));
    PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 6));
    SetNextItemWidth(180);
    bool changed = Combo((std::string("##cb_") + label).c_str(), val, items_z);
    PopStyleVar(2);
    PopStyleColor(5);

    Dummy(ImVec2(0, 0));
    SetCursorScreenPos(ImVec2(bb.Min.x, bb.Max.y + 4));
    return changed;
}

// ============================================
// نافذة Auto Queue
// ============================================
INLINE void DrawAutoQueue() {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        static std::chrono::steady_clock::time_point last_call_time;
        static std::chrono::steady_clock::time_point countdown_start;
        static bool counting = false;
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_call_time).count() > 500) counting = false;
        last_call_time = now;
        if (!counting) { counting = true; countdown_start = now; }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - countdown_start).count();
        int remaining_ms = 3000 - elapsed;
        if (remaining_ms <= 0) {
            if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            StartLastMatch();
            counting = false;
            return;
        }
        SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSize(ImVec2(360, 240), ImGuiCond_Always);
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.04f, 0.08f, 0.98f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
        if (Begin(O("##AutoQueue"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImDrawList* dl = GetWindowDrawList();
            ImVec2 wp = GetWindowPos();
            ImVec2 ws = GetWindowSize();
            DrawGradientRect(dl, wp, ImVec2(wp.x + ws.x, wp.y + 60), IM_COL32(0, 150, 200, 255), NEON_CYAN, true);
            dl->AddRectFilled(wp, ImVec2(wp.x + ws.x, wp.y + 18), IM_COL32(0, 100, 150, 255), 18.0f, ImDrawFlags_RoundCornersTop);
            const char* t = L("Auto Queue", "ﻲﺋﺎﻘﻠﺘﻟﺍ ﻝﻮﺧﺪﻟﺍ");
            ImVec2 tz = CalcTextSize(t);
            dl->AddText(ImVec2(wp.x + (ws.x - tz.x) * 0.5f, wp.y + 18), NEON_CYAN, t);

            SetCursorPosY(80);
            SetWindowFontScale(3.2f);
            std::string c = std::to_string((remaining_ms / 1000) + 1);
            ImVec2 cs = CalcTextSize(c.c_str());
            SetCursorPosX((ws.x - cs.x) * 0.5f);
            TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "%s", c.c_str());
            SetWindowFontScale(1.0f);

            SetCursorPosY(ws.y - 65);
            SetCursorPosX(20);
            PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.2f, 0.2f, 1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            if (Button(L("Cancel", "ﺀﺎﻐﻟﺇ"), ImVec2(ws.x - 40, 45))) {
                persistent_bool[O("bAutoQueue")] = false;
                counting = false;
            }
            PopStyleVar();
            PopStyleColor(2);
            End();
        }
        PopStyleVar();
        PopStyleColor();
    }
}

// ============================================
// دالة قبول التسديدة
// ============================================
static bool ShotApprovalGate(int stateId) {
    ShotApprovalState& A = g_shotApproval;
    bool ourTurn = (stateId == 4);
    if (!ourTurn) {
        A.active = false;
        A.approved = false;
        A.turnHandled = false;
        A.rejectCount = 0;
        return true;
    }
    if (A.approved) return true;
    if (!A.active) {
        A.active = true;
        A.shownAt = std::chrono::steady_clock::now();
    }
    auto now = std::chrono::steady_clock::now();
    long elapsed = (long)std::chrono::duration_cast<std::chrono::milliseconds>(now - A.shownAt).count();
    if (elapsed >= 3000) {
        A.approved = true;
        A.active = false;
        return true;
    }
    return false;
}

INLINE void DrawShotApprovalPrompt(ImGuiIO& io) {
    if (g_menu.hideForCapture) return;
    if (!g_shotApproval.active) return;
    if (!persistent_bool[O("bAutoPlay")] || !persistent_bool[O("bAutoApproval")]) return;

    ShotApprovalState& A = g_shotApproval;

    auto now = std::chrono::steady_clock::now();
    long elapsedMs = (long)std::chrono::duration_cast<std::chrono::milliseconds>(now - A.shownAt).count();
    int remaining = 3 - (int)(elapsedMs / 1000);
    if (remaining < 1) remaining = 1;

    const float w = 430.0f, h = 246.0f;
    SetNextWindowPos(ImVec2(Width / 2.0f, 70.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 16.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                          ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                          ImGuiWindowFlags_NoFocusOnAppearing;
    if (Begin(O("##ShotApproval"), nullptr, wf)) {
        ImDrawList* dl = GetWindowDrawList();
        ImVec2 wp = GetWindowPos();
        ImVec2 ws = GetWindowSize();
        ImVec2 wmax = ImVec2(wp.x + ws.x, wp.y + ws.y);

        dl->AddRectFilled(ImVec2(wp.x + 3, wp.y + 5), ImVec2(wmax.x + 3, wmax.y + 6), IM_COL32(0, 0, 0, 110), 16.0f);
        dl->AddRectFilled(wp, wmax, COL_BG_DEEP, 16.0f);
        dl->AddRectFilled(wp, wmax, IM_COL32(0, 200, 255, 14), 16.0f);
        dl->AddRect(wp, wmax, NEON_CYAN, 16.0f, 0, 2.0f);
        DrawOrnateFrame(dl, wp, wmax);

        ImVec2 ha = ImVec2(wp.x + 14, wp.y + 14);
        ImVec2 hb = ImVec2(wmax.x - 14, wp.y + 64);
        DrawGradientRect(dl, ha, hb, IM_COL32(0, 100, 150, 255), NEON_CYAN, true);
        dl->AddRect(ha, hb, NEON_CYAN, 8.0f, 0, 1.2f);
        const char* title = L("Approve the shot?", "؟ﺏﺮﻀﻟﺍ ﺪﻳﺮﺗ ﻞﻫ");
        ImVec2 tz = CalcTextSize(title);
        DrawBoldText(dl, ImVec2(wp.x + (ws.x - tz.x) * 0.5f, ha.y + (50 - tz.y) * 0.5f), NEON_CYAN, title);

        char info[128];
        snprintf(info, sizeof(info), "%s %d %s",
                 L("Auto-accept in", "ﺪﻌﺑ ﺔﻴﺋﺎﻘﻠﺗ ﺔﻘﻓﺍﻮﻣ"),
                 remaining,
                 L("s", "ﺔﻴﻧﺎﺛ"));
        ImVec2 iz = CalcTextSize(info);
        dl->AddText(ImVec2(wp.x + (ws.x - iz.x) * 0.5f, wp.y + 72), COL_TEXT_DIM, info);

        float acc = A.accuracy;
        if (acc < 0.0f) acc = 0.0f;
        if (acc > 100.0f) acc = 100.0f;

        ImU32 accCol;
        if (acc >= 85.0f) accCol = IM_COL32(0, 255, 150, 255);
        else if (acc >= 60.0f) accCol = NEON_CYAN;
        else accCol = IM_COL32(255, 50, 50, 255);

        char accTxt[96];
        snprintf(accTxt, sizeof(accTxt), "%s %d%%",
                 L("Accuracy", "ﺐﻳﻮﺼﺘﻟﺍ ﺔﻗﺩ"), (int)(acc + 0.5f));
        ImVec2 accZ = CalcTextSize(accTxt);
        DrawBoldText(dl, ImVec2(wp.x + (ws.x - accZ.x) * 0.5f, wp.y + 96), accCol, accTxt);

        float barPad = 26.0f;
        ImVec2 ba = ImVec2(wp.x + barPad, wp.y + 122);
        ImVec2 bb = ImVec2(wmax.x - barPad, wp.y + 140);
        dl->AddRectFilled(ba, bb, IM_COL32(0, 10, 30, 255), 9.0f);
        float fillW = (bb.x - ba.x) * (acc / 100.0f);
        if (fillW > 1.0f)
            dl->AddRectFilled(ba, ImVec2(ba.x + fillW, bb.y), accCol, 9.0f);
        dl->AddRect(ba, bb, NEON_CYAN, 9.0f, 0, 1.3f);

        float pad = 16.0f, gap = 12.0f;
        float bw = (ws.x - pad * 2 - gap) * 0.5f;
        float bh = 50.0f;
        float by = ws.y - bh - 16.0f;

        PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        SetCursorPos(ImVec2(pad, by));
        PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.6f, 0.8f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.7f, 0.9f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.4f, 0.6f, 1.0f));
        PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (Button(L("Accept", "ﻝﻮﺒﻗ"), ImVec2(bw, bh))) {
            A.approved = true;
            A.active = false;
        }
        PopStyleColor(4);

        SetCursorPos(ImVec2(pad + bw + gap, by));
        PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.15f, 0.15f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.05f, 0.05f, 1.0f));
        PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        if (Button(L("Reject", "ﺾﻓﺭ"), ImVec2(bw, bh))) {
            A.approved = false;
            A.rejectCount++;
            A.shownAt = std::chrono::steady_clock::now();
        }
        PopStyleColor(4);
        PopStyleVar();
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}

// ============================================
// دالة ESP - مع تأثيرات نيونية على الكرات
// ============================================
// ============================================
// دالة ESP
// ============================================
INLINE void DrawESP(ImDrawList* draw) {
    if (g_menu.hideForCapture) return;
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        if (!sharedGameManager) return;

        UpdateScreenTable();

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (gameStateManager) {
            auto stateId = gameStateManager.getCurrentStateId();
            if (stateId != 4) {
                if (persistent_bool[O("bAutoQueue")]) {
                    if (!sharedMenuManager.isInQueue()) {
                        MainStateManager mainStateManager = sharedMainManager.mStateManager;
                        if (!mainStateManager.isInGame()) {
                            DrawAutoQueue();
                        }
                    }
                }
                return;
            }
        }

        sharedDirector = F(ptr, libmain + O(0x4f06288));
        if (!sharedDirector) return;
        sharedUserInfo = F(ptr, libmain + O(0x4e9feb8));
        if (!sharedUserInfo) return;
        F(bool, sharedUserInfo + 0x340) = true;
        sharedMainManager = F(ptr, libmain + O(0x4dde3e0));
        if (!sharedMainManager) return;
        sharedMenuManager = F(ptr, libmain + O(0x4dfe838));
        if (!sharedMenuManager) return;
        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;

        if (!mainStateManager.isInGame()) {
            if (persistent_bool[O("bAutoQueue")]) {
                if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
            }
            return;
        }

        auto visualCue = sharedGameManager.mVisualCue();
        Ball::Classification myclass = sharedGameManager.getPlayerClassification();
        Table table = sharedGameManager.mTable;
        if (!table) return;
        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;
        auto& pockets = tableProperties.mPockets();

        if (persistent_bool[O("bESP_DrawPockets")]) {
            for (int i = 0; i < 6; i++) {
                auto sp = WorldToScreen(pockets[i]);
                draw->AddCircle(ImVec2(sp.x, sp.y), 40, NEON_CYAN, 0, 3.f);
                draw->AddCircle(ImVec2(sp.x, sp.y), 50, IM_COL32(0, 200, 255, 60), 0, 1.5f);
            }
        }

        GameStateManager gameStateManager2 = sharedGameManager.mStateManager;
        if (!gameStateManager2) return;
        auto stateId2 = gameStateManager2.getCurrentStateId();

        if (stateId2 == 4) {
            // حساب الـ AI هنا
            int aiSpeed = persistent_int["iAutoPlaySpeed"];
            int aiStyle = persistent_int["iAutoPlayStyle"];

            int reviewPasses = (aiSpeed == 0) ? 1 : (aiSpeed == 1) ? 2 : 2;
            if (aiStyle == 0) reviewPasses += 2;
            else if (aiStyle == 1) reviewPasses += 1;
            else if (aiStyle == 2) reviewPasses = 1;
            for (int p = 0; p < reviewPasses; p++)
                gPrediction->determineShotResult(false);

            auto dist2D = [](auto a, auto b) -> float {
                float dx = a.x - b.x, dy = a.y - b.y;
                return sqrtf(dx * dx + dy * dy);
            };

            float bestScore = -1.0f;
            float bestAcc = 0.0f;
            float bestPower = 0.75f;
            int bestBall = -1;
            bool shotFound = false;

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition == ball.predictedPosition) continue;

                int bp = -1;
                float bd = 1e9f;
                for (int k = 0; k < 6; k++) {
                    float d = dist2D(ball.predictedPosition, pockets[k]);
                    if (d < bd) { bd = d;
                        bp = k; }
                }
                if (bp < 0) continue;

                float potConf = 1.0f - ((bd - 70.0f) / 450.0f);
                if (potConf > 1.0f) potConf = 1.0f;
                if (potConf < 0.0f) potConf = 0.0f;

                float aimConf = potConf;
                if (ball.positions.size() >= 2) {
                    auto p0 = ball.positions[0];
                    auto p1 = ball.positions[ball.positions.size() - 1];
                    float tvx = p1.x - p0.x, tvy = p1.y - p0.y;
                    float pvx = pockets[bp].x - p0.x, pvy = pockets[bp].y - p0.y;
                    float tl = sqrtf(tvx * tvx + tvy * tvy);
                    float pl = sqrtf(pvx * pvx + pvy * pvy);
                    if (tl > 1.0f && pl > 1.0f) {
                        float dot = (tvx * pvx + tvy * pvy) / (tl * pl);
                        if (dot > 1.0f) dot = 1.0f;
                        if (dot < -1.0f) dot = -1.0f;
                        aimConf = (dot + 1.0f) * 0.5f;
                        aimConf = aimConf * aimConf;
                    }
                }

                float pocketBonus = (bp >= 0 && bp < 6 && Prediction::pocketStatus[bp]) ? 0.12f : 0.0f;
                float acc = (potConf * 0.50f + aimConf * 0.50f + pocketBonus) * 100.0f;
                if (acc > 100.0f) acc = 100.0f;

                float cueTravel = dist2D(ball.initialPosition, ball.predictedPosition);
                float distEase = 1.0f - (cueTravel / 2600.0f);
                if (distEase < 0.0f) distEase = 0.0f;
                if (distEase > 1.0f) distEase = 1.0f;
                float ease = aimConf * 0.65f + distEase * 0.35f;

                float easeW = (aiStyle == 0) ? 0.80f : (aiStyle == 2) ? 0.08f : 0.32f;
                float score = acc * ((1.0f - easeW) + easeW * ease);

                if (aiStyle == 0) {
                    if (ease < 0.55f) score *= 0.10f;
                    if (potConf < 0.75f) score *= 0.35f;
                }
                if (aiStyle == 2 && potConf > 0.55f) score *= 1.15f;

                float travel = cueTravel + bd;
                float pw;
                if (travel < 700.0f) pw = 0.30f + travel / 3500.0f;
                else if (travel < 1500.0f) pw = 0.50f + (travel - 700.0f) / 2600.0f;
                else pw = 0.78f + (travel - 1500.0f) / 4200.0f;

                if (aiStyle == 0) {
                    pw *= 0.58f;
                    if (pw > 0.55f) pw = 0.55f;
                    if (travel > 1800.0f) pw = 0.62f;
                } else if (aiStyle == 1) {
                    float jitter = ((rand() % 100) / 100.0f - 0.5f) * 0.08f;
                    pw = pw * (1.0f + jitter);
                } else if (aiStyle == 2) {
                    pw = pw * 1.25f + 0.08f;
                }
                if (pw < 0.22f) pw = 0.22f;
                if (pw > 1.00f) pw = 1.00f;

                if (score > bestScore) {
                    bestScore = score;
                    bestAcc = acc;
                    bestPower = pw;
                    bestBall = i;
                    shotFound = true;
                }
            }

            if (shotFound && bestBall >= 0 && bestBall < gPrediction->guiData.ballsCount) {
                const int VERIFY_PASSES = 2;
                int hits = 0;
                float avgDist = 0.0f;
                float worstDist = 0.0f;
                bool cueBallDanger = false;
                bool collisionRisk = false;
                for (int vp = 0; vp < VERIFY_PASSES; vp++) {
                    gPrediction->determineShotResult(false);
                    auto& cb = gPrediction->guiData.balls[bestBall];
                    float cd = 1e9f;
                    int cp = -1;
                    for (int k = 0; k < 6; k++) {
                        float d = dist2D(cb.predictedPosition, pockets[k]);
                        if (d < cd) { cd = d;
                            cp = k; }
                    }
                    avgDist += cd;
                    if (cd > worstDist) worstDist = cd;
                    if (cd < 220.0f) hits++;

                    if (gPrediction->guiData.ballsCount > 0) {
                        auto& cue = gPrediction->guiData.balls[0];
                        for (int k = 0; k < 6; k++) {
                            if (dist2D(cue.predictedPosition, pockets[k]) < 200.0f) {
                                cueBallDanger = true;
                                break;
                            }
                        }
                    }
                    for (int bi = 1; bi < gPrediction->guiData.ballsCount; bi++) {
                        if (bi == bestBall) continue;
                        auto& ob = gPrediction->guiData.balls[bi];
                        if (ob.initialPosition == ob.predictedPosition) continue;
                        for (int k = 0; k < 6; k++) {
                            if (dist2D(ob.predictedPosition, pockets[k]) < 180.0f) {
                                collisionRisk = true;
                                break;
                            }
                        }
                    }
                }
                avgDist /= (float)VERIFY_PASSES;
                float potProbability = (float)hits / (float)VERIFY_PASSES;

                float requiredProb = (aiStyle == 0) ? 0.85f
                                   : (aiStyle == 2) ? 0.65f
                                   : 0.70f;
                bool vetoed = false;
                if (potProbability < requiredProb) vetoed = true;
                if (cueBallDanger) vetoed = true;
                if (collisionRisk && aiStyle != 2) vetoed = true;
                if (worstDist > 700.0f) vetoed = true;
                if (avgDist > 320.0f) vetoed = true;

                if (vetoed) {
                    shotFound = false;
                    bestAcc = 0.0f;
                } else {
                    bestAcc = 100.0f * potProbability;
                    if (bestAcc > 100.0f) bestAcc = 100.0f;
                }
            }

            if (shotFound) {
                bestAcc = 100.0f;
                if (aiSpeed == 2) bestAcc = 100.0f;
                else if (aiSpeed == 1) bestAcc = 100.0f;
                else if (aiSpeed == 0) bestAcc = 100.0f;

                if (aiStyle == 0) {
                    bestAcc = 100.0f;
                } else if (aiStyle == 1) {
                    float jitterAcc = (float)(rand() % 40) / 10.0f;
                    bestAcc = 95.0f + jitterAcc;
                } else if (aiStyle == 2) {
                    bestAcc = 100.0f;
                }
            }
            if (bestAcc > 100.0f) bestAcc = 100.0f;

            g_shotApproval.accuracy = shotFound ? bestAcc : 0.0f;
            g_shotApproval.aiPower = bestPower;
            g_shotApproval.hasShot = shotFound;

            if (shotFound) persistent_float["fShotPower"] = bestPower;
        }

        if (persistent_bool[O("bAutoPlay")]) {
            bool approvalOn = persistent_bool[O("bAutoApproval")];
            if (!approvalOn || ShotApprovalGate(stateId2))
                AutoPlay::Update();
        }
        if (stateId2 == 6 || stateId2 == 7 || stateId2 == 8) return;

        if (persistent_bool[O("bESP_DrawPocketsShotState")]) {
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto sp = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(sp.x, sp.y), 40, IM_COL32(0, 255, 100, 255), 0, 5.f);
                    draw->AddCircle(ImVec2(sp.x, sp.y), 55, IM_COL32(0, 255, 100, 60), 0, 2.0f);
                }
            }
        }

        // إعدادات سمك وشفافية الخطوط
        int _tk = persistent_int["iLineThickness"];
        if (_tk <= 0) _tk = 5;
        int _al = persistent_int["iLineAlpha"];
        if (_al <= 0) _al = 10;
        float lineThick = (float)_tk * 2.0f;
        float thickScale = (float)_tk / 5.0f;
        float alphaScale = (float)_al / 10.0f;
        auto MOD_A = [&](ImU32 c) -> ImU32 {
            int a = (int)(((c >> 24) & 0xFF) * alphaScale);
            if (a < 0) a = 0;
            if (a > 255) a = 255;
            return (c & 0x00FFFFFFu) | ((ImU32)a << 24);
        };

        int lineStyle = persistent_int["iLineStyle"];

        if (persistent_bool[O("bESP_DrawPredictionLine")]) {
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 lastPos{};
                    ImU32 ballColor = MOD_A(GetBallColor(i));
                    for (int j = 1; j < ball.positions.size(); j++) {
                        auto point = WorldToScreen(ball.positions[j]);
                        if (lastPos.x || lastPos.y) {
                            if (lineStyle == 1) {
                                // رسم خط متقطع حقيقي
                                static float dashPhase = 0.0f;
                                dashPhase = AddDashedLine(draw, lastPos, point, ballColor, lineThick, 20.0f, 15.0f, dashPhase);
                            } else {
                                draw->AddLine(lastPos, point, ballColor, lineThick);
                            }
                        }
                        lastPos = point;
                    }
                }
            }

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 pos = WorldToScreen(ball.predictedPosition);
                    ImVec2 startPos = WorldToScreen(ball.initialPosition);
                    float ballSize = 20.0f * thickScale;
                    if (ballSize < 10.0f) ballSize = 10.0f;
                    if (ballSize > 40.0f) ballSize = 40.0f;
                    ImU32 color = GetBallColor(i);
                    
                    // نقطة البداية (دائرة صغيرة)
                    draw->AddCircle(startPos, 8.0f * thickScale, color, 0, 2.0f * thickScale);
                    
                    // رسم الكرة بالألوان
                    draw->AddCircleFilled(pos, ballSize, color);
                    
                    // تأثير الإضاءة (ثلاثي الأبعاد)
                    draw->AddCircleFilled(
                        ImVec2(pos.x - ballSize * 0.15f, pos.y - ballSize * 0.15f),
                        ballSize * 0.35f,
                        IM_COL32(255, 255, 255, 60)
                    );
                    
                    // إطار الكرة
                    draw->AddCircle(pos, ballSize, IM_COL32(0, 0, 0, 80), 0, 1.5f);
                    
                    // رسم الرقم (للكرات 1-15)
                    if (i > 0) {
                        char buf[8];
                        sprintf(buf, "%d", i);
                        
                        // خلفية بيضاء للرقم
                        float innerR = ballSize * 0.55f;
                        draw->AddCircleFilled(pos, innerR, IM_COL32(255, 255, 255, 255));
                        draw->AddCircle(pos, innerR, IM_COL32(0, 0, 0, 50), 0, 1.0f);
                        
                        // الرقم باللون الأسود
                        ImFont* font = ImGui::GetFont();
                        float fontSize = ImGui::GetFontSize() * 0.55f;
                        if (fontSize < 8.0f) fontSize = 8.0f;
                        if (fontSize > 24.0f) fontSize = 24.0f;
                        
                        ImVec2 txtSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buf);
                        ImVec2 txtPos = pos - (txtSize * 0.5f);
                        draw->AddText(font, fontSize, txtPos, IM_COL32(0, 0, 0, 255), buf);
                    }
                }
            }
        }
    }
} // نهاية دالة DrawESP

// ============================================
// الشريط الجانبي مع تأثيرات نيونية
// ============================================
static void DrawSidebar(float sidebarW, float winH, ImVec2 winPos) {
    ImDrawList* dl = GetWindowDrawList();
    static float time = 0.0f;
    time += ImGui::GetIO().DeltaTime;

    // 6 طبقات من التوهج الخارجي للشريط الجانبي
    for (int i = 6; i > 0; i--) {
        float alpha = (7 - i) * 18.0f * (0.5f + 0.5f * sinf(time * 0.6f + i * 0.8f));
        ImVec2 a = ImVec2(winPos.x + 16 - i - 2.0f * sinf(time + i), winPos.y + 80 - i);
        ImVec2 b = ImVec2(winPos.x + sidebarW + 16 + i + 2.0f * cosf(time * 0.7f + i), winPos.y + winH - 20 + i);
        dl->AddRect(a, b, IM_COL32(0, 200, 255, (int)alpha), 14.0f, 0, 1.5f + i * 0.1f);
    }

    ImVec2 a = ImVec2(winPos.x + 16, winPos.y + 80);
    ImVec2 b = ImVec2(winPos.x + sidebarW + 16, winPos.y + winH - 20);
    
    // خلفية متدرجة للشريط الجانبي
    dl->AddRectFilledMultiColor(a, b,
        IM_COL32(0, 10, 30, 255),
        IM_COL32(0, 15, 40, 255),
        IM_COL32(0, 20, 50, 255),
        IM_COL32(0, 10, 35, 255));
    
    // إطار خارجي متوهج
    dl->AddRect(a, b, NEON_CYAN, 14.0f, 0, 2.0f);
    dl->AddRect(a + ImVec2(4, 4), b - ImVec2(4, 4), IM_COL32(0, 255, 255, 40), 10.0f, 0, 1.0f);
    
    // نقاط ضوئية على الشريط الجانبي
    for (int i = 0; i < 5; i++) {
        float x = a.x + 20 + i * ((b.x - a.x - 40) / 4.0f);
        float y = a.y + 10 + 4.0f * sinf(time * 1.5f + i * 1.2f);
        dl->AddCircleFilled(ImVec2(x, y), 2.0f + 1.0f * sinf(time * 2.0f + i), 
                           IM_COL32(0, 200, 255, 100 + 80 * (0.5f + 0.5f * sinf(time * 1.8f + i))), 8);
    }

    SetCursorPos(ImVec2(22, 96));
    BeginGroup();

    struct Tab { const char* en; const char* ar; const char* icon; };
    Tab tabs[] = {
        { "Draw", "ﻢﺳﺭ", "0" },
        { "Play", "ﺐﻌﻟ", "1" },
        { "Queue", "ﺭﻭﺩ", "2" },
        { "Extra", "ﻲﻓﺎﺿﺇ", "3" },
        { "Info", "ﺕﺎﻣﻮﻠﻌﻣ", "4" },
    };
    int n = (int)(sizeof(tabs) / sizeof(tabs[0]));
    for (int i = 0; i < n; i++) {
        SetCursorPosX(22);
        if (GoldSidebarButton(L(tabs[i].en, tabs[i].ar), tabs[i].icon, g_menu.currentTab == i, sidebarW))
            g_menu.currentTab = i;
        Dummy(ImVec2(0, 6));
    }
    EndGroup();
}

// ============================================
// منطقة المحتوى مع تأثيرات نيونية متطورة
// ============================================
static void DrawContentArea(float sidebarW, float winW, float winH, ImVec2 winPos) {
    bool need_save = false;
    ImDrawList* dl = GetWindowDrawList();
    static float time = 0.0f;
    time += ImGui::GetIO().DeltaTime;

    // ======================================================================
    // 12 طبقة من التوهج الخارجي للمحتوى
    // ======================================================================
    // ======================================================================
// 12 طبقة من التوهج الخارجي للمحتوى (مصحح)
// ======================================================================
for (int i = 12; i > 0; i--) {
    float alpha = (13 - i) * 20.0f * (0.5f + 0.5f * sinf(time * 0.5f + i * 0.6f));
    float radiusOffset = i * 2.0f + 6.0f * sinf(time * 0.7f + i * 0.4f);
    
    int hue = (i * 37 + (int)(time * 20)) % 360;
    float r = 0.5f + 0.5f * sinf(hue * 0.0174f);
    float g = 0.5f + 0.5f * sinf(hue * 0.0174f + 2.094f);
    float b_float = 0.5f + 0.5f * sinf(hue * 0.0174f + 4.188f);  // ✅ تغيير الاسم
    
    ImU32 glowColor = IM_COL32(
        (int)(r * 255),
        (int)(g * 255),
        (int)(b_float * 255),  // ✅ استخدام الاسم الجديد
        (int)alpha
    );
    
    ImVec2 a = ImVec2(winPos.x + sidebarW + 32 - radiusOffset + 4.0f * sinf(time * 0.6f + i),
                      winPos.y + 80 - radiusOffset + 4.0f * cosf(time * 0.8f + i * 0.5f));
    ImVec2 b = ImVec2(winPos.x + winW - 16 + radiusOffset + 4.0f * cosf(time * 0.7f + i * 0.3f),
                      winPos.y + winH - 20 + radiusOffset + 4.0f * sinf(time * 0.9f + i * 0.7f));
    dl->AddRect(a, b, glowColor, 14.0f + i * 1.0f + 2.0f * sinf(time + i * 0.5f), 0, 1.5f + i * 0.1f);
}

    // ======================================================================
    // خلفية المحتوى مع تدرج نيوني متحرك
    // ======================================================================
    ImVec2 a = ImVec2(winPos.x + sidebarW + 32, winPos.y + 80);
    ImVec2 b = ImVec2(winPos.x + winW - 16, winPos.y + winH - 20);
    
    float quantumPulse1 = 0.5f + 0.5f * sinf(time * 0.7f);
    float quantumPulse2 = 0.5f + 0.5f * sinf(time * 1.3f + 0.8f);
    float quantumPulse3 = 0.5f + 0.5f * sinf(time * 0.9f + 1.6f);
    
    ImU32 bg1 = IM_COL32(
        (int)(5 + 25 * quantumPulse1),
        (int)(2 + 15 * quantumPulse2),
        (int)(20 + 40 * quantumPulse3),
        255
    );
    ImU32 bg2 = IM_COL32(
        (int)(10 + 20 * quantumPulse2),
        (int)(5 + 18 * quantumPulse3),
        (int)(30 + 35 * quantumPulse1),
        255
    );
    ImU32 bg3 = IM_COL32(
        (int)(20 + 15 * quantumPulse3),
        (int)(8 + 20 * quantumPulse1),
        (int)(40 + 30 * quantumPulse2),
        255
    );
    ImU32 bg4 = IM_COL32(
        (int)(8 + 20 * quantumPulse2),
        (int)(3 + 15 * quantumPulse3),
        (int)(25 + 35 * quantumPulse1),
        255
    );
    dl->AddRectFilledMultiColor(a, b, bg1, bg2, bg3, bg4);
    
    // ======================================================================
    // موجات الطاقة النانوية داخل المحتوى
    // ======================================================================
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    for (int wave = 0; wave < 8; wave++) {
        float wavePhase = time * 1.2f + wave * 0.8f;
        float waveY = a.y + 30 + wave * ((b.y - a.y - 60) / 7.0f);
        waveY += 10.0f * sinf(wavePhase + (mousePos.x - winPos.x) * 0.01f + wave * 0.5f);
        
        float waveX = a.x + 20 + 10.0f * sinf(wavePhase * 0.6f + wave);
        
        int hueWave = (wave * 45 + (int)(time * 25)) % 360;
        float rw = 0.5f + 0.5f * sinf(hueWave * 0.0174f);
        float gw = 0.5f + 0.5f * sinf(hueWave * 0.0174f + 2.094f);
        float bw = 0.5f + 0.5f * sinf(hueWave * 0.0174f + 4.188f);
        
        ImU32 waveColor = IM_COL32(
            (int)(80 + 175 * rw),
            (int)(30 + 150 * gw),
            (int)(120 + 135 * bw),
            (int)(30 + 60 * (0.5f + 0.5f * sinf(wavePhase * 0.5f)))
        );
        
        dl->AddLine(
            ImVec2(waveX, waveY - 10.0f * sinf(wavePhase * 0.8f + 0.3f)),
            ImVec2(b.x - 20, waveY + 15.0f * sinf(wavePhase * 0.9f + 0.7f)),
            waveColor, 1.5f + 0.5f * sinf(wavePhase * 0.3f)
        );
        
        if (wave % 2 == 0) {
            float dotX = a.x + 30 + (b.x - a.x - 60) * (0.5f + 0.5f * sinf(wavePhase * 0.7f + wave));
            float dotY = waveY + 3.0f * sinf(wavePhase * 0.5f + wave * 1.1f);
            dl->AddCircleFilled(
                ImVec2(dotX, dotY),
                2.0f + 2.0f * (0.5f + 0.5f * sinf(time * 1.8f + wave)),
                IM_COL32(255, 200, 255, (int)(120 + 80 * (0.5f + 0.5f * sinf(time + wave)))),
                8
            );
        }
    }

    // ======================================================================
    // الجسيمات الهولوجرافية داخل المحتوى
    // ======================================================================
    static struct Particle {
        float x, y, vx, vy, size, phase;
    } particles[40];
    static bool particlesInit = false;
    
    if (!particlesInit) {
        for (int i = 0; i < 40; i++) {
            particles[i].x = (float)(rand() % 1000) / 1000.0f;
            particles[i].y = (float)(rand() % 1000) / 1000.0f;
            particles[i].vx = (float)(rand() % 200 - 100) / 800.0f;
            particles[i].vy = (float)(rand() % 200 - 100) / 800.0f;
            particles[i].size = 1.0f + (float)(rand() % 30) / 10.0f;
            particles[i].phase = (float)(rand() % 1000) / 100.0f;
        }
        particlesInit = true;
    }

    float width = b.x - a.x;
    float height = b.y - a.y;
    
    for (int i = 0; i < 40; i++) {
        particles[i].x += particles[i].vx * ImGui::GetIO().DeltaTime * 0.3f;
        particles[i].y += particles[i].vy * ImGui::GetIO().DeltaTime * 0.3f;
        
        if (particles[i].x < 0 || particles[i].x > 1) particles[i].vx *= -1;
        if (particles[i].y < 0 || particles[i].y > 1) particles[i].vy *= -1;
        
        float px = a.x + particles[i].x * width;
        float py = a.y + particles[i].y * height;
        float pulse = 0.5f + 0.5f * sinf(time * 1.5f + particles[i].phase);
        float size = particles[i].size * (0.5f + 0.5f * pulse);
        
        int hueP = (i * 30 + (int)(time * 25)) % 360;
        float rp = 0.5f + 0.5f * sinf(hueP * 0.0174f);
        float gp = 0.5f + 0.5f * sinf(hueP * 0.0174f + 2.094f);
        float bp = 0.5f + 0.5f * sinf(hueP * 0.0174f + 4.188f);
        
        ImU32 col = IM_COL32(
            (int)(rp * 255),
            (int)(gp * 255),
            (int)(bp * 255),
            (int)(100 + 155 * pulse)
        );
        
        dl->AddCircleFilled(ImVec2(px, py), size, col, 12);
        
        if (pulse > 0.6f) {
            dl->AddCircle(ImVec2(px, py), size * 2.0f, 
                         IM_COL32(255, 255, 255, (int)(20 * pulse)), 12, 1.0f);
        }
    }

    // ======================================================================
    // الإطار الداخلي مع تأثير نيوني
    // ======================================================================
    ImVec2 frameA = a + ImVec2(4, 4);
    ImVec2 frameB = b - ImVec2(4, 4);
    dl->AddRectFilled(frameA, frameB, IM_COL32(0, 0, 0, 20), 12.0f);
    dl->AddRect(frameA, frameB, IM_COL32(180, 0, 255, 100), 12.0f, 0, 1.5f);
    dl->AddRect(frameA + ImVec2(2, 2), frameB - ImVec2(2, 2), IM_COL32(150, 0, 255, 40), 10.0f, 0, 1.0f);

    // ======================================================================
    // عنوان التبويب مع تأثير نيوني متطور
    // ======================================================================
    const char* titlesEn[] = { "Draw", "Play", "Queue", "Extra", "Info" };
    const char* titlesAr[] = { "ﻢﺳﺭ", "ﺐﻌﻟ", "ﺭﻭﺩ", "ﻲﻓﺎﺿﺇ", "ﺕﺎﻣﻮﻠﻌﻣ" };
    int idx = g_menu.currentTab;
    
    // خلفية العنوان
    ImVec2 titleBgA = ImVec2(a.x + 14, a.y + 6);
    ImVec2 titleBgB = ImVec2(b.x - 14, a.y + 44);
    dl->AddRectFilled(titleBgA, titleBgB, IM_COL32(0, 0, 0, 150), 8.0f);
    dl->AddRect(titleBgA, titleBgB, IM_COL32(255, 0, 150, 120), 8.0f, 0, 1.5f);
    
    // نص العنوان مع توهج
    char tabTitle[128];
    snprintf(tabTitle, sizeof(tabTitle), " ✦ %s ✦ ", L(titlesEn[idx], titlesAr[idx]));
    ImVec2 ts = CalcTextSize(tabTitle);
    ImVec2 tp = ImVec2((a.x + b.x) * 0.5f - ts.x * 0.5f, a.y + 10);
    
    // طبقات توهج النص
    for (int i = 4; i > 0; i--) {
        float alpha = (5 - i) * 20.0f * (0.5f + 0.5f * sinf(time * 0.8f + i * 0.5f));
        ImU32 glowCol = IM_COL32(
            200 + 55 * (i % 3 == 0 ? 1 : 0),
            50 + 150 * (i % 3 == 1 ? 1 : 0),
            255,
            (int)alpha
        );
        dl->AddText(ImVec2(tp.x + 1.5f * sinf(time * 0.7f + i), tp.y - i * 0.8f), glowCol, tabTitle);
    }
    
    // النص الأساسي
    dl->AddText(tp, IM_COL32(255, 255, 255, 255), tabTitle);
    
    // خط زخرفي تحت العنوان
    float lineY = a.y + 48;
    float lineStart = a.x + 30;
    float lineEnd = b.x - 30;
    
    // خط الطاقة الرئيسي
    for (int i = 0; i < 60; i++) {
        float t = (float)i / 59.0f;
        float x = lineStart + (lineEnd - lineStart) * t;
        float yOffset = 2.0f * sinf(time * 2.5f + t * 6.0f + 1.2f);
        float pulse = 0.5f + 0.5f * sinf(time * 1.8f + t * 4.0f);
        
        int hueLine = (i * 20 + (int)(time * 30)) % 360;
        float rl = 0.5f + 0.5f * sinf(hueLine * 0.0174f);
        float gl = 0.5f + 0.5f * sinf(hueLine * 0.0174f + 2.094f);
        float bl = 0.5f + 0.5f * sinf(hueLine * 0.0174f + 4.188f);
        
        dl->AddLine(
            ImVec2(x, lineY + yOffset),
            ImVec2(x + 2.0f, lineY + yOffset + 2.0f * sinf(time + t * 3.0f)),
            IM_COL32((int)(rl * 255), (int)(gl * 255), (int)(bl * 255), (int)(150 + 100 * pulse)),
            1.5f + 1.5f * pulse
        );
    }
    
    // نقاط زخرفية على الخط
    for (int i = 0; i < 7; i++) {
        float t = (float)i / 6.0f;
        float x = lineStart + (lineEnd - lineStart) * t;
        float pulse = 0.5f + 0.5f * sinf(time * 2.0f + i * 1.2f + 0.3f);
        float yOff = 3.0f * sinf(time * 1.5f + i * 1.0f);
        
        int hueDot = (i * 50 + (int)(time * 25)) % 360;
        float rd = 0.5f + 0.5f * sinf(hueDot * 0.0174f);
        float gd = 0.5f + 0.5f * sinf(hueDot * 0.0174f + 2.094f);
        float bd = 0.5f + 0.5f * sinf(hueDot * 0.0174f + 4.188f);
        
        dl->AddCircleFilled(
            ImVec2(x, lineY + yOff),
            3.0f + 2.0f * pulse,
            IM_COL32((int)(rd * 255), (int)(gd * 255), (int)(bd * 255), (int)(150 + 100 * pulse)),
            12
        );
        
        if (pulse > 0.7f) {
            dl->AddCircle(ImVec2(x, lineY + yOff), 6.0f + 4.0f * pulse,
                         IM_COL32(255, 255, 255, (int)(20 * pulse)), 12, 1.0f);
        }
    }

    // ======================================================================
    // بداية المحتوى
    // ======================================================================
    SetCursorScreenPos(ImVec2(a.x + 16, a.y + 58));
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    BeginChild(O("##Content"), ImVec2(b.x - a.x - 32, b.y - a.y - 76), false);

    // ======================================================================
    // تبويب Draw
    // ======================================================================
    switch (idx) {
        case 0: { // Draw Tab
            Dummy(ImVec2(0, 4));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ LANGUAGE", "✦ ﺔﻐﻠﻟﺍ"));
            Dummy(ImVec2(0, 8));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            int curLang = persistent_int["iLang"];
            float bw = (GetContentRegionAvail().x - 10) * 0.5f;
            
            bool isEng = (curLang == 0);
            PushStyleColor(ImGuiCol_Button, isEng ? ImVec4(1.0f, 0.0f, 0.8f, 1.0f) : ImVec4(0.05f, 0.02f, 0.12f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.6f, 1.0f));
            PushStyleColor(ImGuiCol_Text, isEng ? ImVec4(1, 1, 1, 1) : ImVec4(0.6f, 0.7f, 0.8f, 1));
            if (Button("✦ English ✦", ImVec2(bw, 44))) { persistent_int["iLang"] = 0; need_save = true; }
            PopStyleColor(3);
            SameLine();
            
            bool isAr = (curLang == 1);
            PushStyleColor(ImGuiCol_Button, isAr ? ImVec4(1.0f, 0.0f, 0.8f, 1.0f) : ImVec4(0.05f, 0.02f, 0.12f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.0f, 0.6f, 1.0f));
            PushStyleColor(ImGuiCol_Text, isAr ? ImVec4(1, 1, 1, 1) : ImVec4(0.6f, 0.7f, 0.8f, 1));
            if (Button("✦ ﺔﻴﺑﺮﻌﻟﺍ ✦", ImVec2(bw, 44))) { persistent_int["iLang"] = 1; need_save = true; }
            PopStyleColor(3);
            PopStyleVar();
            
            Dummy(ImVec2(0, 16));
            
            need_save |= GoldToggle(L("✦ Draw Prediction Lines", "✦ ﻁﻮﻄﺧ ﻢﺳﺭ"), L("Quantum path visualization", "ﻲﻤﻜﻟﺍ ﺭﺎﺴﻤﻟﺍ ﺾﻳﻮﻌﺗ"), &persistent_bool[O("bESP_DrawPredictionLine")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Draw Pockets", "✦ ﺏﻮﻴﺠﻟﺍ ﻢﺳﺭ"), L("Neon pocket markers", "ﺔﻴﻣﻮﻜﻟﺍ ﺏﻮﻴﺠﻟﺍ"), &persistent_bool[O("bESP_DrawPockets")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Show Ball Numbers", "✦ ﺕﺍﺮﻜﻟﺍ ﻡﺎﻗﺭﺃ ﺭﺎﻬﻇﺇ"), L("Ball identification", "ﺕﺍﺮﻜﻟﺍ ﻒﻳﺮﻌﺗ"), &persistent_bool[O("bESP_ShowBallNumbers")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Bounce Sync", "✦ ﺩﺍﺪﺗﺭﻻﺍ ﺔﻨﻣﺍﺰﻣ"), L("Bounce markers", "ﺩﺍﺪﺗﺭﻻﺍ ﻡﺎﻗﺭﺃ"), &persistent_bool[O("bESP_BounceMarkers")]);
            
            Dummy(ImVec2(0, 16));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Line Style", "✦ ﻂﻴﻄﺨﻟﺍ ﻂﻤﻧ"));
            Dummy(ImVec2(0, 8));
            const char* items = "✦ SOLID ✦\0✦ STRIPE ✦\0✦ COLORS ✦\0";
            const char* itemsAr = "✦ ﻞﺼﺘﻣ ✦\0✦ ﻊﻄﻘﺘﻣ ✦\0✦ ﻥﺍﻮﻟﺃ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int["iLineStyle"],
                                   persistent_int["iLang"] == 1 ? itemsAr : items);
            Dummy(ImVec2(0, 8));
            
            if (persistent_int["iLineThickness"] <= 0) persistent_int["iLineThickness"] = 5;
            if (persistent_int["iLineAlpha"] <= 0) persistent_int["iLineAlpha"] = 10;
            if (persistent_int["iMenuSize"] <= 0) persistent_int["iMenuSize"] = 5;

            {
                float fv = (float)persistent_int["iLineThickness"];
                if (GoldSliderFloat(L("✦ Line Thickness", "✦ ﻂﺨﻟﺍ ﻚﻤﺳ"),
                                    L("", ""), &fv, 1.0f, 15.0f, "%.0f")) {
                    persistent_int["iLineThickness"] = (int)(fv + 0.5f);
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            {
                float fv = (float)persistent_int["iLineAlpha"];
                if (GoldSliderFloat(L("✦ Line Transparency", "✦ ﻂﺨﻟﺍ ﺔﻴﻓﺎﻔﺷ"),
                                    L("", ""), &fv, 1.0f, 10.0f, "%.0f")) {
                    persistent_int["iLineAlpha"] = (int)(fv + 0.5f);
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            {
                float fv = persistent_float["fESP_PocketSize"];
                if (fv < 10.0f) fv = 30.0f;
                if (GoldSliderFloat(L("✦ Pocket Size", "✦ ﺐﻴﺠﻟﺍ ﻢﺠﺣ"),
                                    L("", ""), &fv, 10.0f, 100.0f, "%.0f")) {
                    persistent_float["fESP_PocketSize"] = fv;
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            {
                float fv = persistent_float["fESP_BallSize"];
                if (fv < 5.0f) fv = 20.0f;
                if (GoldSliderFloat(L("✦ Ball Predicted Size", "✦ ﺔﻌﻗﻮﺘﻤﻟﺍ ﺓﺮﻜﻟﺍ ﻢﺠﺣ"),
                                    L("", ""), &fv, 5.0f, 40.0f, "%.0f")) {
                    persistent_float["fESP_BallSize"] = fv;
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            {
                float fv = persistent_float["fESP_StartPointSize"];
                if (fv < 2.0f) fv = 8.0f;
                if (GoldSliderFloat(L("✦ Start Point Size", "✦ ﺔﻳﺍﺪﺒﻟﺍ ﺔﻄﻘﻧ ﻢﺠﺣ"),
                                    L("", ""), &fv, 2.0f, 40.0f, "%.0f")) {
                    persistent_float["fESP_StartPointSize"] = fv;
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            {
                float fv = (float)persistent_int["iMenuSize"];
                if (GoldSliderFloat(L("✦ Fix Menu Size", "✦ ﺔﻤﺋﺎﻘﻟﺍ ﻢﺠﺣ ﻞﻳﺪﻌﺗ"),
                                    L("", ""), &fv, 1.0f, 10.0f, "%.0f")) {
                    persistent_int["iMenuSize"] = (int)(fv + 0.5f);
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            float sliderX = persistent_float["fPowerBarXPercent"];
    if (GoldSliderFloat("X Position", "Horizontal", &sliderX, 0.00f, 0.50f, "%.3f")) {
    persistent_float["fPowerBarXPercent"] = sliderX;
    need_save = true;
    }
    Dummy(ImVec2(0, 4));

    // ── Slider Top ──
    float sliderTop = persistent_float["fPowerBarYStartPercent"];
    if (GoldSliderFloat(O("Top Position"), O("Vertical start"), &sliderTop, 0.05f, 0.50f, "%.3f")) {
        persistent_float["fPowerBarYStartPercent"] = sliderTop;
        need_save = true;
    }
    Dummy(ImVec2(0, 4));

    // ── Slider Height ──
    float sliderH = persistent_float["fPowerBarYEndPercent"];
    if (GoldSliderFloat(O("Height"), O("Slider height"), &sliderH, 0.30f, 100.0f, "%.3f")) {
        persistent_float["fPowerBarYEndPercent"] = sliderH;
        need_save = true;
    }
    Dummy(ImVec2(0, 10));

    // ── GoldToggle Preview ──
    need_save |= GoldToggle(O("Preview Power Slider"), O("Show guide line on screen"), &persistent_bool["bPSliderPreview"]);
    Dummy(ImVec2(0, 10));

    // ── PREVIEW GAMBAR ──
    if (persistent_bool["bPSliderPreview"]) {
        ImDrawList* fgdl = GetForegroundDrawList();
        if (fgdl) {
            ImGuiIO& io = GetIO();
            float px = io.DisplaySize.x * persistent_float["fPowerBarXPercent"];
            float pt = io.DisplaySize.y * persistent_float["fPowerBarYStartPercent"];
            float ph = io.DisplaySize.y * persistent_float["fPowerBarYEndPercent"];

            fgdl->AddLine(ImVec2(px, pt), ImVec2(px, pt + ph), IM_COL32(255, 80, 80, 220), 3.5f);
            fgdl->AddCircleFilled(ImVec2(px, pt), 7.f, IM_COL32(80, 255, 80, 240));
            fgdl->AddCircleFilled(ImVec2(px, pt + ph), 7.f, IM_COL32(80, 255, 80, 240));
        }
    }
            break;
        }
        case 1: { // Play Tab
            Dummy(ImVec2(0, 4));
            
            // ======================================================================
            // زر AutoPlay المتطور جداً - 10 طبقات توهج
            // ======================================================================
            ImVec2 btnPos = GetCursorScreenPos();
            float btnW = GetContentRegionAvail().x;
            float btnH = 80.0f;

            bool isAutoPlayOn = persistent_bool[O("bAutoPlay")];
            ImDrawList* dl2 = GetWindowDrawList();
            ImGuiIO& io2 = ImGui::GetIO();

            // 10 طبقات من التوهج الخارجي
            for (int i = 10; i > 0; i--) {
                float pulse = 0.5f + 0.5f * sinf(time * 1.5f + i * 0.7f);
                float alpha = (11 - i) * 20.0f * pulse;
                float offset = i * 4.0f + 8.0f * sinf(time * 1.2f + i * 0.5f);
                
                int hue = (i * 37 + (int)(time * 25)) % 360;
                float r = 0.5f + 0.5f * sinf(hue * 0.0174f);
                float g = 0.5f + 0.5f * sinf(hue * 0.0174f + 2.094f);
                float b = 0.5f + 0.5f * sinf(hue * 0.0174f + 4.188f);
                
                ImU32 glowCol = IM_COL32(
                    (int)(r * 255),
                    (int)(g * 255),
                    (int)(b * 255),
                    (int)alpha
                );
                dl2->AddRect(
                    ImVec2(btnPos.x - offset, btnPos.y - offset),
                    ImVec2(btnPos.x + btnW + offset, btnPos.y + btnH + offset),
                    glowCol, 0, 2.0f
                );
            }

            // خلفية الزر مع تدرج متحرك
            float bgPulse1 = 0.5f + 0.5f * sinf(time * 0.8f);
            float bgPulse2 = 0.5f + 0.5f * sinf(time * 1.2f + 0.7f);
            
            ImU32 bg1 = isAutoPlayOn ? 
                IM_COL32((int)(60 + 30 * bgPulse1), (int)(45 + 20 * bgPulse2), 15, 230) :
                IM_COL32((int)(40 + 20 * bgPulse2), (int)(10 + 15 * bgPulse1), (int)(60 + 30 * bgPulse1), 230);
            ImU32 bg2 = isAutoPlayOn ? 
                IM_COL32((int)(40 + 25 * bgPulse2), (int)(30 + 15 * bgPulse1), 10, 240) :
                IM_COL32((int)(30 + 15 * bgPulse1), (int)(5 + 10 * bgPulse2), (int)(40 + 25 * bgPulse2), 240);
            dl2->AddRectFilledMultiColor(
                ImVec2(btnPos.x, btnPos.y),
                ImVec2(btnPos.x + btnW, btnPos.y + btnH),
                bg1, bg1, bg2, bg2
            );

            // إطار الزر المتوهج
            ImU32 borderCol = isAutoPlayOn ? 
                IM_COL32(255, 215 + 40 * (0.5f + 0.5f * sinf(time * 1.5f)), 0, 255) :
                IM_COL32(180 + 75 * (0.5f + 0.5f * sinf(time * 1.3f)), 0, 255, 255);
            dl2->AddRect(
                ImVec2(btnPos.x, btnPos.y),
                ImVec2(btnPos.x + btnW, btnPos.y + btnH),
                borderCol, 0, 3.0f
            );

            // خطوط المسح الضوئي المتقدمة
            static float scanTime = 0.0f;
            scanTime += io2.DeltaTime * 2.5f;
            float scanPos = fmod(scanTime, 1.0f);
            float scanPos2 = fmod(scanTime + 0.5f, 1.0f);

            ImU32 lineCol1 = isAutoPlayOn ? IM_COL32(255, 215, 0, 150) : IM_COL32(180, 0, 255, 150);
            ImU32 lineCol2 = isAutoPlayOn ? IM_COL32(255, 215, 0, 80) : IM_COL32(180, 0, 255, 80);
            
            dl2->AddLine(
                ImVec2(btnPos.x + scanPos * btnW, btnPos.y),
                ImVec2(btnPos.x + scanPos * btnW, btnPos.y + btnH),
                lineCol1, 2.5f
            );
            dl2->AddLine(
                ImVec2(btnPos.x, btnPos.y + scanPos * btnH),
                ImVec2(btnPos.x + btnW, btnPos.y + scanPos * btnH),
                lineCol1, 2.5f
            );
            dl2->AddLine(
                ImVec2(btnPos.x + scanPos2 * btnW, btnPos.y),
                ImVec2(btnPos.x + scanPos2 * btnW, btnPos.y + btnH),
                lineCol2, 1.5f
            );
            dl2->AddLine(
                ImVec2(btnPos.x, btnPos.y + scanPos2 * btnH),
                ImVec2(btnPos.x + btnW, btnPos.y + scanPos2 * btnH),
                lineCol2, 1.5f
            );

            // نقاط LED متقدمة
            ImU32 ledCol = isAutoPlayOn ? IM_COL32(255, 215, 0, 255) : IM_COL32(255, 50, 50, 255);
            static float ledTime = 0.0f;
            ledTime += io2.DeltaTime * 5.0f;
            
            for (int i = 0; i < 6; i++) {
                float alpha = isAutoPlayOn ? 
                    (100 + 155 * (0.5f + 0.5f * sinf(ledTime + i * 1.2f + 0.5f))) :
                    80 + 40 * (0.5f + 0.5f * sinf(ledTime * 0.5f + i * 0.8f));
                float xPos = btnPos.x + btnW - 45 - i * 14;
                float yPos = btnPos.y + btnH * 0.5f + 10.0f * sinf(time * 2.0f + i * 0.7f);
                
                dl2->AddCircleFilled(
                    ImVec2(xPos, yPos),
                    5.0f,
                    IM_COL32(ledCol >> 16 & 0xFF, ledCol >> 8 & 0xFF, ledCol & 0xFF, (int)alpha)
                );
                
                if (alpha > 200) {
                    dl2->AddCircle(
                        ImVec2(xPos, yPos),
                        10.0f,
                        IM_COL32(255, 255, 255, (int)(20 * alpha / 255)),
                        16,
                        1.0f
                    );
                }
            }

            // الأيقونة الرئيسية
            const char* icon = isAutoPlayOn ? "✦" : "◇";
            ImVec2 iconSize = CalcTextSize(icon);
            dl2->AddText(
                ImVec2(btnPos.x + 25, btnPos.y + (btnH - iconSize.y) * 0.5f),
                isAutoPlayOn ? IM_COL32(255, 215, 0, 255) : IM_COL32(180, 0, 255, 255),
                icon
            );

            // النص الرئيسي مع طبقات توهج متعددة
            const char* btnText = isAutoPlayOn ? 
                L("✦ AUTO PLAY ACTIVE ✦", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻌﻠﻟﺍ ﻞﻴﻌﻔﻣ ✦") :
                L("✦ AUTO PLAY OFFLINE ✦", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻌﻠﻟﺍ ﻞﻄﻌﻣ ✦");
            ImVec2 textSize = CalcTextSize(btnText);
            ImVec2 textPos = ImVec2(
                btnPos.x + 55 + (btnW - 55 - textSize.x) * 0.5f,
                btnPos.y + (btnH - textSize.y) * 0.5f
            );

            for (int i = 5; i > 0; i--) {
                float alpha = i * 25.0f * (0.5f + 0.5f * sinf(time * 1.0f + i * 0.5f));
                ImU32 glowText = isAutoPlayOn ? 
                    IM_COL32(255, 215, 0, (int)alpha) :
                    IM_COL32(180, 0, 255, (int)alpha);
                dl2->AddText(
                    ImVec2(textPos.x + 2.0f * sinf(time * 0.5f + i * 0.3f), textPos.y - i * 0.8f),
                    glowText,
                    btnText
                );
            }
            dl2->AddText(textPos, IM_COL32(255, 255, 255, 255), btnText);

            // زر غير مرئي
            SetCursorScreenPos(btnPos);
            PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
            PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

            if (Button(O("##AutoPlayBtn"), ImVec2(btnW, btnH))) {
                persistent_bool[O("bAutoPlay")] = !persistent_bool[O("bAutoPlay")];
                AutoPlay::bAutoPlaying = persistent_bool[O("bAutoPlay")];
                if (AutoPlay::bAutoPlaying) {
                    AutoPlay::ClearState();
                    AutoPlay::state = AutoPlay::IDLE;
                } else {
                    AutoPlay::ClearState();
                }
                need_save = true;
            }

            PopStyleVar();
            PopStyleColor(3);

            Dummy(ImVec2(0, 16));
            
            need_save |= GoldToggle(L("✦ Auto Play Mode", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻌﻠﻟﺍ ﻊﺿﻭ"), L("", ""), &persistent_bool[O("bAutoPlaySwitch")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Auto Aim Mode", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻳﻮﺼﺘﻟﺍ ﻊﺿﻭ"), L("", ""), &persistent_bool[O("bAutoAimSwitch")]);
            
            Dummy(ImVec2(0, 12));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Automation Speed", "✦ ﺔﻴﺋﺎﻘﻠﺘﻟﺍ ﺔﻋﺮﺳ"));
            Dummy(ImVec2(0, 4));
            if (!persistent_int.count(O("iAutoSpeed"))) persistent_int[O("iAutoSpeed")] = 0;
            const char* speedItems = "✦ Maximum Fast ✦\0✦ Minimum Slow ✦\0";
            const char* speedItemsAr = "✦ ﻰﺼﻗﺃ ﺔﻋﺮﺳ ✦\0✦ ﻰﻧﺩﺃ ﺀﻂﺑ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int[O("iAutoSpeed")],
                                   persistent_int["iLang"] == 1 ? speedItemsAr : speedItems);
            
            Dummy(ImVec2(0, 8));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Play Style", "✦ ﺐﻌﻠﻟﺍ ﻂﻤﻧ"));
            Dummy(ImVec2(0, 4));
            if (!persistent_int.count(O("iPlayStyle"))) persistent_int[O("iPlayStyle")] = 0;
            const char* styleItems = "✦ Natural Play ✦\0✦ Instant Mode ✦\0";
            const char* styleItemsAr = "✦ ﻲﻌﻴﺒﻃ ﺐﻌﻟ ✦\0✦ ﻱﺭﻮﻔﻟﺍ ﻊﺿﻮﻟﺍ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int[O("iPlayStyle")],
                                   persistent_int["iLang"] == 1 ? styleItemsAr : styleItems);
            
            Dummy(ImVec2(0, 8));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ 9-Ball Strategy", "✦ ﺕﺍﺮﻛ 9 ﺔﻴﺠﻴﺗﺍﺮﺘﺳﺍ"));
            Dummy(ImVec2(0, 4));
            if (!persistent_int.count(O("iNineMode"))) persistent_int[O("iNineMode")] = 1;
            const char* nineItems = "✦ Best Shot ✦\0✦ Snipe 9 ✦\0";
            const char* nineItemsAr = "✦ ﺔﺑﺮﺿ ﻞﻀﻓﺃ ✦\0✦ 9 ﺪﻴﺻ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int[O("iNineMode")],
                                   persistent_int["iLang"] == 1 ? nineItemsAr : nineItems);
            
            Dummy(ImVec2(0, 12));
            need_save |= GoldToggle(L("✦ Auto Call Pocket", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻴﺠﻟﺍ ﺮﻴﻴﻐﺗ"), L("", ""), &persistent_bool[O("bAutoPocket")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Cushion Shot (8-Ball)", "✦ ﺕﺍﺮﻛ 8 ﻪﻓﺎﺤﻟﺍ ﺔﺑﺮﺿ"), L("", ""), &persistent_bool[O("bCushionShot")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Show Pocket Target Visual", "✦ ﻑﺪﻬﻟﺍ ﺐﻴﺠﻟﺍ ﺽﺮﻋ"), L("", ""), &persistent_bool[O("bPocketTargetVisual")]);
            
            Dummy(ImVec2(0, 12));
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Power Bar Side", "✦ ﻩﻮﻘﻟﺍ ﻂﻳﺮﺷ ﻊﻗﻮﻣ"));
            Dummy(ImVec2(0, 4));
            if (!persistent_int.count(O("iPowerBarSide"))) persistent_int[O("iPowerBarSide")] = 0;
            const char* powerItems = "✦ Left Side ✦\0✦ Right Side ✦\0";
            const char* powerItemsAr = "✦ ﺮﺴﻳﻷﺍ ﺐﻧﺎﺠﻟﺍ ✦\0✦ ﻦﻤﻳﻷﺍ ﺐﻧﺎﺠﻟﺍ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int[O("iPowerBarSide")],
                                   persistent_int["iLang"] == 1 ? powerItemsAr : powerItems);
            
            Dummy(ImVec2(0, 8));
            float power = persistent_float["fShotPower"];
            if (power < 0.1f) power = 0.75f;
            if (GoldSliderFloat(L("✦ Auto Shot Power", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﺐﻳﻮﺼﺘﻟﺍ ﺓﻮﻗ"),
                                L("", ""), &power, 0.10f, 1.0f, "✦ %.0f%% ✦")) {
                persistent_float["fShotPower"] = power;
                need_save = true;
            }
            break;
        }
        case 2: { // Queue Tab
            Dummy(ImVec2(0, 4));
            
            need_save |= GoldToggle(L("✦ Enable AutoQueue", "✦ ﻲﺋﺎﻘﻠﺘﻟﺍ ﻝﻮﺧﺪﻟﺍ ﻞﻴﻌﻔﺗ"),
                                    L("", ""), &persistent_bool[O("bAutoQueue")]);
            Dummy(ImVec2(0, 12));
            
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Mode", "✦ ﻊﺿﻮﻟﺍ"));
            Dummy(ImVec2(0, 4));
            if (!persistent_int.count(O("iAutoQueue_Mode"))) persistent_int[O("iAutoQueue_Mode")] = 0;
            const char* qItems = "✦ Last Selected ✦\0✦ Smart ✦\0✦ Fix Table ✦\0";
            const char* qItemsAr = "✦ ﺮﺧﺁ ﺭﺎﻴﺘﺧﺍ ✦\0✦ ﻲﻛﺫ ✦\0✦ ﺖﺑﺎﺛ ﺔﻟﻭﺎﻃ ✦\0";
            need_save |= GoldCombo(L("", ""), L("", ""), &persistent_int[O("iAutoQueue_Mode")],
                                   persistent_int["iLang"] == 1 ? qItemsAr : qItems);
            
            if (persistent_int[O("iAutoQueue_Mode")] == 1) {
                Dummy(ImVec2(0, 8));
                {
                    float fv = (float)persistent_int[O("iAutoQueue_BetPercent")];
                    if (fv < 1.0f) fv = 50.0f;
                    if (GoldSliderFloat(L("✦ Bet Percent", "✦ ﻥﺎﻫﺮﻟﺍ ﺔﺒﺴﻧ"),
                                        L("", ""), &fv, 1.0f, 100.0f, "✦ %.0f%% ✦")) {
                        persistent_int[O("iAutoQueue_BetPercent")] = (int)(fv + 0.5f);
                        need_save = true;
                    }
                }
            }
            
            if (persistent_int[O("iAutoQueue_Mode")] == 2) {
                Dummy(ImVec2(0, 12));
                TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Select Table", "✦ ﺔﻟﻭﺎﻄﻟﺍ ﺮﺘﺧﺍ"));
                Dummy(ImVec2(0, 4));
                
                static const char* tableLabels[] = {
                    "100", "200", "1k", "2.5k", "10k", "50k", "100k", "500k",
                    "1M", "2M", "5M", "8M", "10M", "20M", "30M", "50M", "200M"
                };
                int& selected = persistent_int[O("iAutoQueue_FixTable")];
                float avail = GetContentRegionAvail().x;
                int cols = 3;
                float gap = 10.0f;
                float btnW = (avail - gap * (cols - 1)) / cols;
                float btnH = 65.0f;

                PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);

                for (int i = 0; i < 17; i++) {
                    if (i % cols != 0) SameLine(0, gap);

                    ImVec2 p = GetCursorScreenPos();
                    ImVec2 size = ImVec2(btnW, btnH);
                    
                    char btnId[64];
                    snprintf(btnId, sizeof(btnId), "%s##table_tile_%d", tableLabels[i], i);
                    ImGuiID id = GetID(btnId);
                    
                    ImRect btnBb(p, p + size);
                    bool hovered, held;
                    bool pressed = ButtonBehavior(btnBb, id, &hovered, &held);

                    bool isSel = (selected == i);
                    ImDrawList* dl2 = GetWindowDrawList();
                    float rounding = 14.0f;
                    float pulse = 0.5f + 0.5f * sinf(time * 2.0f + i * 0.5f);

                    if (isSel) {
                        ImU32 selBg1 = IM_COL32(255, 0, 150, 255);
                        ImU32 selBg2 = IM_COL32(180, 0, 255, 255);
                        ImU32 selBg3 = IM_COL32(0, 200, 255, 255);
                        ImU32 selBg4 = IM_COL32(255, 0, 150, 255);
                        dl2->AddRectFilledMultiColor(p, p + size, selBg1, selBg2, selBg3, selBg4);
                        
                        for (int j = 3; j > 0; j--) {
                            float alpha = (4 - j) * 30 * pulse;
                            ImU32 glowSel = IM_COL32(255, 0, 150, (int)alpha);
                            dl2->AddRect(
                                p - ImVec2(j * 2, j * 2),
                                p + size + ImVec2(j * 2, j * 2),
                                glowSel, rounding + j, 0, 2.0f
                            );
                        }
                        
                        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
                        ImVec2 ts2 = CalcTextSize(tableLabels[i]);
                        dl2->AddText(
                            ImVec2(p.x + (btnW - ts2.x) * 0.5f, p.y + (btnH - ts2.y) * 0.5f),
                            IM_COL32(255, 255, 255, 255),
                            tableLabels[i]
                        );
                        if (g_ArialBlackFont) PopFont();
                    } else {
                        ImU32 bgCol = IM_COL32(25, 15, 45, 200);
                        dl2->AddRectFilled(p, p + size, bgCol, rounding);
                        
                        if (hovered) {
                            for (int j = 2; j > 0; j--) {
                                float alpha = (3 - j) * 20 * pulse;
                                dl2->AddRect(
                                    p - ImVec2(j, j),
                                    p + size + ImVec2(j, j),
                                    IM_COL32(255, 0, 150, (int)alpha),
                                    rounding + j, 0, 1.5f
                                );
                            }
                            dl2->AddRectFilled(p, p + size, IM_COL32(255, 0, 150, 30), rounding);
                        }
                        
                        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
                        ImVec2 ts2 = CalcTextSize(tableLabels[i]);
                        ImU32 textCol = hovered ? IM_COL32(255, 200, 255, 255) : IM_COL32(200, 195, 220, 200);
                        dl2->AddText(
                            ImVec2(p.x + (btnW - ts2.x) * 0.5f, p.y + (btnH - ts2.y) * 0.5f),
                            textCol,
                            tableLabels[i]
                        );
                        if (g_ArialBlackFont) PopFont();
                    }

                    if (pressed) {
                        selected = i;
                        need_save = true;
                    }
                    Dummy(size);
                }
                PopStyleVar();
            }
            break;
        }
        case 3: { // Extra Tab
            Dummy(ImVec2(0, 4));
            
            if (!persistent_bool.count(O("bDisableAds"))) persistent_bool[O("bDisableAds")] = true;
            need_save |= GoldToggle(L("✦ Disable Ads", "✦ ﺕﺎﻧﻼﻋﻹﺍ ﻑﺎﻘﻳﺇ"), L("", ""), &persistent_bool[O("bDisableAds")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ Hide Resolution Text", "✦ ﺔﻗﺪﻟﺍ ﺺﻧ ﺀﺎﻔﺧﺇ"), L("", ""), &persistent_bool[O("bHideResText")]);
            Dummy(ImVec2(0, 8));
            need_save |= GoldToggle(L("✦ FPS Counter", "✦ ﺩﺍﺪﻋ FPS"), L("", ""), &persistent_bool[O("bFpsCounter")]);
            
            Dummy(ImVec2(0, 12));
            
            if (!persistent_float.count(O("fScannerLevel"))) persistent_float[O("fScannerLevel")] = 50.0f;
            {
                float fv = persistent_float[O("fScannerLevel")];
                if (GoldSliderFloat(L("✦ Scanner Level", "✦ ﺢﺴﻤﻟﺍ ﻯﻮﺘﺴﻣ"),
                                    L("", ""), &fv, 0.0f, 100.0f, "✦ %.0f%% ✦")) {
                    persistent_float[O("fScannerLevel")] = fv;
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            
            if (!persistent_float.count(O("fPredictionFps"))) persistent_float[O("fPredictionFps")] = 30.0f;
            {
                float fv = persistent_float[O("fPredictionFps")];
                if (GoldSliderFloat(L("✦ Prediction FPS", "✦ ﻊﻗﻮﺗ ﺔﻋﺮﺳ FPS"),
                                    L("", ""), &fv, 15.0f, 30.0f, "✦ %.0f FPS ✦")) {
                    persistent_float[O("fPredictionFps")] = fv;
                    need_save = true;
                }
            }
            Dummy(ImVec2(0, 4));
            
            if (!persistent_bool.count(O("bPredictionAfterShot"))) persistent_bool[O("bPredictionAfterShot")] = false;
            need_save |= GoldToggle(L("✦ Prediction After Shot", "✦ ﺔﺑﺮﻀﻟﺍ ﺪﻌﺑ ﻊﻗﻮﺘﻟﺍ"), L("", ""), &persistent_bool[O("bPredictionAfterShot")]);
            Dummy(ImVec2(0, 4));
            
            if (!persistent_bool.count(O("bDisableFlicker"))) persistent_bool[O("bDisableFlicker")] = false;
            need_save |= GoldToggle(L("✦ Disable Flicker", "✦ ﺾﻴﻣﻮﻟﺍ ﻊﻨﻣ"), L("", ""), &persistent_bool[O("bDisableFlicker")]);
            break;
        }
        case 4: { // Info Tab
            Dummy(ImVec2(0, 4));
            
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ Device Information", "✦ ﺯﺎﻬﺠﻟﺍ ﺕﺎﻣﻮﻠﻌﻣ"));
            if (g_ArialBlackFont) PopFont();
            Dummy(ImVec2(0, 8));
            
            static char s_manufacturer[PROP_VALUE_MAX] = {};
            static char s_model[PROP_VALUE_MAX] = {};
            static char s_abi[PROP_VALUE_MAX] = {};
            static bool s_props_loaded = false;
            if (!s_props_loaded) {
                __system_property_get("ro.product.manufacturer", s_manufacturer);
                __system_property_get("ro.product.model", s_model);
                __system_property_get("ro.product.cpu.abi", s_abi);
                s_props_loaded = true;
            }
            
            ImVec4 labelColor = ImVec4(0.7f, 0.6f, 0.9f, 1.0f);
            ImVec4 valueColor = ImVec4(1.0f, 0.3f, 0.8f, 1.0f);
            
            TextColored(labelColor, "%s", L("✦ Manufacturer: ", "✦ ﺔﻛﺮﺸﻟﺍ: "));
            SameLine();
            TextColored(valueColor, "%s", s_manufacturer);
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ Model: ", "✦ ﺝﺫﻮﻤﻨﻟﺍ: "));
            SameLine();
            TextColored(valueColor, "%s", s_model);
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ ABI: ", "✦ ABI: "));
            SameLine();
            TextColored(valueColor, "%s", s_abi);
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ Game: ", "✦ ﺔﺒﻌﻠﻟﺍ: "));
            SameLine();
            TextColored(valueColor, "%s", "8 Ball Pool 56.23.2");
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ Type: ", "✦ ﻉﻮﻨﻟﺍ: "));
            SameLine();
            TextColored(valueColor, "%s", L("✦ WATAN - Spidermod", "✦ QP_ENO"));
            
            Dummy(ImVec2(0, 16));
            
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            TextColored(ImVec4(1.0f, 0.0f, 0.8f, 1.0f), "%s", L("✦ License Information", "✦ ﺺﻴﺧﺮﺘﻟﺍ ﺕﺎﻣﻮﻠﻌﻣ"));
            if (g_ArialBlackFont) PopFont();
            Dummy(ImVec2(0, 8));
            
            TextColored(labelColor, "%s", L("✦ Status: ", "✦ ﺔﻟﺎﺤﻟﺍ: "));
            SameLine();
            TextColored(ImVec4(0.0f, 1.0f, 0.4f, 1.0f), "%s", L("✦ Activated ✦", "✦ ﻞﻌﻔﻣ ✦"));
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ Expiry: ", "✦ ﺀﺎﻬﺘﻧﻻﺍ: "));
            SameLine();
            TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", g_ExpTime.c_str());
            Dummy(ImVec2(0, 4));
            
            TextColored(labelColor, "%s", L("✦ Token: ", "✦ ﺰﻣﺮﻟﺍ: "));
            SameLine();
            TextColored(valueColor, "%s", g_Token.c_str());
            
            Dummy(ImVec2(0, 16));
            
            PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.15f, 0.15f, 1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 14.0f);
            if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
            if (Button(L("✦ LOGOUT ✦", "✦ ﺝﻭﺮﺨﻟﺍ ✦"), ImVec2(GetContentRegionAvail().x - 90.0f, 55.0f))) {
                persistent_string["key"] = "";
                logged_in = false;
                bValid = false;
                g_Token = "";
                g_Auth = "";
                save_persistence();
            }
            if (g_ArialBlackFont) PopFont();
            PopStyleVar();
            PopStyleColor(2);
            if (g_ArialBlackFont) PopFont();
            break;
        }
    }

    if (need_save) save_persistence();
    EndChild();
    PopStyleVar();
    PopStyleColor();
}

// ============================================
// القائمة الرئيسية
// ============================================
INLINE void DrawMenu(ImGuiIO& io) {
    if (g_menu.hideForCapture) return;
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        if (g_menu.isOpen) g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        else g_menu.menuAlpha = 0.0f;

        if (g_menu.menuAlpha > 0.01f) {
            int _msz = persistent_int["iMenuSize"];
            if (_msz <= 0) _msz = 5;
            float _mscale = 0.6f + (float)_msz * 0.08f;
            float winW = 1320.0f * _mscale;
            float winH = 820.0f * _mscale;

            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.04f, 0.08f, 0.98f * g_menu.menuAlpha));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);

            ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            if (Begin(O("##ENOMenu"), &g_menu.isOpen, wf)) {
                ImDrawList* dl = GetWindowDrawList();
                ImVec2 wp = GetWindowPos();
                DrawOrnateFrame(dl, wp, ImVec2(wp.x + winW, wp.y + winH));

                ImVec2 a = ImVec2(wp.x + 40, wp.y + 10);
                ImVec2 b = ImVec2(wp.x + winW - 40, wp.y + 56);
                dl->AddRectFilled(a, b, IM_COL32(0, 8, 20, 255), 8.0f);
                dl->AddRect(a, b, NEON_GOLD, 8.0f, 0, 2.0f);

                float cs = 10.0f;
                dl->AddLine(ImVec2(a.x, a.y + cs), ImVec2(a.x + cs, a.y), NEON_GOLD, 2.5f);
                dl->AddLine(ImVec2(b.x, a.y + cs), ImVec2(b.x - cs, a.y), NEON_GOLD, 2.5f);
                dl->AddLine(ImVec2(a.x, b.y - cs), ImVec2(a.x + cs, b.y), NEON_GOLD, 2.5f);
                dl->AddLine(ImVec2(b.x, b.y - cs), ImVec2(b.x - cs, b.y), NEON_GOLD, 2.5f);

                const char* t = CurrentTabTitle();
                char fullTitle[128];
                snprintf(fullTitle, sizeof(fullTitle), " %s ", t);
                ImVec2 ts = CalcTextSize(fullTitle);
                ImVec2 tp = ImVec2(wp.x + (winW - ts.x) * 0.5f, a.y + (46 - ts.y) * 0.5f);
                DrawBoldText(dl, tp, NEON_GOLD, fullTitle);

                float sidebarW = 330.0f;
                DrawSidebar(sidebarW, winH, wp);
                DrawContentArea(sidebarW, winW, winH, wp);
            }
            End();
            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

// ============================================
// زر عائم (Floating Button)
// ============================================
static void DrawFloatingButton(ImGuiIO& io) {
    if (g_menu.hideForCapture) return;
    static ImVec2 buttonPos = ImVec2(80, 160);
    static bool isDragging = false;
    static float hoverAnim = 0.0f;
    static float time = 0.0f;
    time += io.DeltaTime * 0.8f;

    const float BASE_RADIUS = 44.0f;
    const float SIZE = BASE_RADIUS * 2.0f + 30.0f;
    
    ImGui::SetNextWindowPos(buttonPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(SIZE, SIZE), ImGuiCond_Always);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    if (ImGui::Begin(O("##NEON_Float"), nullptr, 
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | 
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 center = ImVec2(buttonPos.x + SIZE/2, buttonPos.y + SIZE/2);
        
        ImGui::SetCursorPos(ImVec2(0, 0));
        ImGui::InvisibleButton(O("##NEON_FloatHit"), ImVec2(SIZE, SIZE));
        
        bool hovered = ImGui::IsItemHovered();
        bool active = ImGui::IsItemActive();
        
        float hoverTarget = hovered ? 1.0f : 0.0f;
        hoverAnim += (hoverTarget - hoverAnim) * io.DeltaTime * 12.0f;
        float scale = 1.0f + hoverAnim * 0.12f;
        float radius = BASE_RADIUS * scale;
        
        // 14 حلقة توهج خارجية
        for (int i = 0; i < 14; i++) {
            float r = radius + i * 4.0f + sinf(time * 0.4f + i * 0.6f) * 2.0f;
            float alpha = (14 - i) * 10.0f * (0.3f + 0.7f * (0.5f + 0.5f * sinf(time * 0.25f + i * 0.5f)));
            alpha = ImClamp(alpha, 0.0f, 140.0f);
            
            ImU32 color;
            if (i < 5) color = IM_COL32(255, 0, 150, (int)alpha);
            else if (i < 10) color = IM_COL32(180, 0, 255, (int)alpha);
            else color = IM_COL32(0, 200, 255, (int)alpha);
            
            dl->AddCircle(center, r, color, 64, 1.2f + i * 0.06f);
        }
        
        // الخلفية الداخلية
        float coreSize = radius * 0.75f;
        for (int i = 0; i < 3; i++) {
            float r = coreSize * (1.0f - i * 0.2f);
            float alpha = 80 - i * 25;
            ImU32 glowColor;
            if (i == 0) glowColor = IM_COL32(255, 0, 150, (int)alpha);
            else if (i == 1) glowColor = IM_COL32(180, 0, 255, (int)alpha);
            else glowColor = IM_COL32(0, 200, 255, (int)alpha);
            dl->AddCircleFilled(center, r, glowColor, 48);
        }
        
        // القلب المتوهج
        float pulseCore = 0.3f + 0.7f * (0.5f + 0.5f * sinf(time * 2.5f));
        float coreAlpha = 180 + 60 * sinf(time * 2.0f);
        float hueShift = fmod(time * 0.15f, 3.0f);
        ImU32 coreColor;
        if (hueShift < 1.0f) {
            float t = hueShift;
            coreColor = IM_COL32(255 - (int)(75 * t), (int)(150 * t), 150 + (int)(105 * t), (int)coreAlpha);
        } else if (hueShift < 2.0f) {
            float t = hueShift - 1.0f;
            coreColor = IM_COL32(180 - (int)(80 * t), (int)(200 * t), 255, (int)coreAlpha);
        } else {
            float t = hueShift - 2.0f;
            coreColor = IM_COL32((int)(100 * t), 255 - (int)(55 * t), 255, (int)coreAlpha);
        }
        float coreR = coreSize * (0.5f + 0.15f * pulseCore);
        dl->AddCircleFilled(center, coreR, coreColor, 48);
        dl->AddCircleFilled(center, coreR * 0.3f, IM_COL32(255, 255, 255, (int)(120 + 80 * pulseCore)), 32);
        
        // تأثير hover
        if (hovered) {
            float glowPulse = 0.5f + 0.5f * sinf(time * 4.0f);
            dl->AddCircle(center, radius + 8.0f, IM_COL32(255, 0, 150, (int)(60 + 60 * glowPulse)), 48, 2.5f);
            dl->AddCircle(center, radius + 16.0f, IM_COL32(180, 0, 255, (int)(40 + 40 * glowPulse)), 48, 1.8f);
            dl->AddCircle(center, radius + 24.0f, IM_COL32(0, 200, 255, (int)(25 + 25 * glowPulse)), 48, 1.2f);
            
            for (int i = 0; i < 12; i++) {
                float angle = time * 0.6f + i * IM_PI * 2.0f / 12.0f;
                float dist = radius + 12.0f + 6.0f * sinf(time * 0.8f + i);
                ImVec2 dotPos = ImVec2(center.x + cosf(angle) * dist, center.y + sinf(angle) * dist);
                float dotAlpha = 0.5f + 0.5f * sinf(time * 2.0f + i * 1.2f);
                ImU32 dotColor;
                if (i % 3 == 0) dotColor = IM_COL32(255, 0, 150, (int)(dotAlpha * 200));
                else if (i % 3 == 1) dotColor = IM_COL32(180, 0, 255, (int)(dotAlpha * 200));
                else dotColor = IM_COL32(0, 200, 255, (int)(dotAlpha * 200));
                dl->AddCircleFilled(dotPos, 3.0f + 2.0f * sinf(time * 1.5f + i), dotColor, 8);
            }
        }
        
        // السحب والضغط
        if (active && ImGui::IsMouseDragging(0)) {
            isDragging = true;
            buttonPos.x += io.MouseDelta.x;
            buttonPos.y += io.MouseDelta.y;
            buttonPos.x = ImClamp(buttonPos.x, 0.0f, (float)Width - SIZE);
            buttonPos.y = ImClamp(buttonPos.y, 0.0f, (float)Height - SIZE);
        }
        
        if (hovered && ImGui::IsMouseReleased(0) && !isDragging) {
            g_menu.isOpen = !g_menu.isOpen;
        }
        
        if (!active) isDragging = false;
    }
    ImGui::End();
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

// ============================================
// زر التقاط الشاشة
// ============================================
static void DrawCaptureButton(ImGuiIO& io) {
    const float HOT_W = 110.0f;
    const float HOT_H = 110.0f;

    SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(HOT_W, HOT_H), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ENOCapBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##ENOCapHit"), ImVec2(HOT_W, HOT_H));

        if (g_menu.hideForCapture) {
            if (IsItemHovered() && IsMouseReleased(0)) {
                g_menu.hideForCapture = false;
            }
        }
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}

// ============================================
// عداد FPS
// ============================================
static void DrawFPS(ImGuiIO& io) {
    if (!persistent_bool[O("bFpsCounter")]) return;
    if (g_menu.hideForCapture) return;
    
    static float fps = 0.0f;
    static int frameCount = 0;
    static float timeAccum = 0.0f;
    
    frameCount++;
    timeAccum += io.DeltaTime;
    
    if (timeAccum >= 0.5f) {
        fps = frameCount / timeAccum;
        frameCount = 0;
        timeAccum = 0.0f;
    }
    
    ImVec2 pos = ImVec2(20.0f, 20.0f);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    
    char fpsText[32];
    snprintf(fpsText, sizeof(fpsText), "✦ FPS: %.1f ✦", fps);
    ImVec2 textSize = CalcTextSize(fpsText);
    
    ImVec2 bgMin = ImVec2(pos.x - 10, pos.y - 6);
    ImVec2 bgMax = ImVec2(pos.x + textSize.x + 10, pos.y + textSize.y + 6);
    dl->AddRectFilled(bgMin, bgMax, IM_COL32(0, 0, 0, 180), 8.0f);
    dl->AddRect(bgMin, bgMax, IM_COL32(0, 200, 255, 100), 8.0f, 0, 1.0f);
    
    ImU32 color;
    if (fps >= 55.0f) color = IM_COL32(0, 255, 100, 255);
    else if (fps >= 30.0f) color = IM_COL32(255, 215, 0, 255);
    else if (fps >= 15.0f) color = IM_COL32(255, 150, 0, 255);
    else color = IM_COL32(255, 50, 50, 255);
    
    dl->AddText(pos, color, fpsText);
    
    float barWidth = 80.0f;
    float barHeight = 4.0f;
    float barY = pos.y + textSize.y + 10.0f;
    float barX = pos.x;
    float fpsPercent = ImClamp(fps / 60.0f, 0.0f, 1.0f);
    
    dl->AddRectFilled(ImVec2(barX, barY), ImVec2(barX + barWidth, barY + barHeight), 
                      IM_COL32(255, 255, 255, 30), 2.0f);
    
    ImU32 barColor;
    if (fpsPercent >= 0.9f) barColor = IM_COL32(0, 255, 100, 200);
    else if (fpsPercent >= 0.5f) barColor = IM_COL32(255, 215, 0, 200);
    else barColor = IM_COL32(255, 50, 50, 200);
    
    dl->AddRectFilled(ImVec2(barX, barY), 
                      ImVec2(barX + barWidth * fpsPercent, barY + barHeight), 
                      barColor, 2.0f);
}
// ============================================
// زر تشغيل Auto Play
// ============================================
static void DrawAutoPlayToggleButton(ImGuiIO& io) {
    if (g_menu.hideForCapture) return;

    static ImVec2 pos = ImVec2(0, 0);
    static bool inited = false;
    static bool dragging = false;
    static float pulse = 0.0f;

    float R = 64.0f, S = R * 2.0f;
    if (!inited) {
        pos.x = (float)Width - S - 40.0f;
        pos.y = (float)Height - S - 90.0f;
        inited = true;
    }

    pos.x = ImClamp(pos.x, 0.0f, (float)Width - S);
    pos.y = ImClamp(pos.y, 0.0f, (float)Height - S);

    SetNextWindowPos(pos, ImGuiCond_Always);
    SetNextWindowSize(ImVec2(S + 10, S + 10), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ENOAutoBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        ImDrawList* dl = GetWindowDrawList();
        ImVec2 c = ImVec2(pos.x + R + 2, pos.y + R + 2);
        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##ENOAutoHit"), ImVec2(S, S));

        bool isOn = persistent_bool[O("bAutoPlay")];
        pulse += io.DeltaTime * (isOn ? 4.5f : 2.0f);

        ImU32 ring = isOn ? NEON_GOLD : IM_COL32(255, 80, 80, 255);
        ImU32 ringDim = isOn ? IM_COL32(255, 215, 0, 60) : IM_COL32(255, 80, 80, 60);
        ImU32 inner1 = isOn ? IM_COL32(60, 45, 10, 240) : IM_COL32(45, 10, 10, 240);
        ImU32 inner2 = isOn ? IM_COL32(30, 22, 5, 240) : IM_COL32(25, 5, 5, 240);

        dl->AddCircleFilled(ImVec2(c.x + 2, c.y + 4), R, IM_COL32(0, 0, 0, 140), 56);

        float glowK = 0.55f + 0.45f * sinf(pulse);
        for (int i = 5; i > 0; --i) {
            float a = (6 - i) * 14.0f * glowK;
            if (a > 255) a = 255;
            ImU32 gcol = isOn ? IM_COL32(255, 215, 0, (int)a) : IM_COL32(255, 80, 80, (int)a);
            dl->AddCircle(c, R + i * 2.0f, gcol, 56, 2.0f);
        }

        dl->AddCircleFilled(c, R, inner1, 56);
        dl->AddCircleFilled(c, R - 4, inner2, 56);
        dl->AddCircle(c, R, ring, 56, 2.6f);
        dl->AddCircle(c, R - 8, ringDim, 56, 1.2f);

        ImU32 icCol = IM_COL32(255, 255, 255, 255);
        if (isOn) {
            float bw = R * 0.14f, bh = R * 0.46f, gap = R * 0.14f;
            dl->AddRectFilled(ImVec2(c.x - gap - bw, c.y - bh), ImVec2(c.x - gap, c.y + bh), icCol, 3.0f);
            dl->AddRectFilled(ImVec2(c.x + gap, c.y - bh), ImVec2(c.x + gap + bw, c.y + bh), icCol, 3.0f);
        } else {
            ImVec2 p1(c.x - R * 0.22f, c.y - R * 0.36f);
            ImVec2 p2(c.x - R * 0.22f, c.y + R * 0.36f);
            ImVec2 p3(c.x + R * 0.36f, c.y);
            dl->AddTriangleFilled(p1, p2, p3, icCol);
        }

        const char* label = isOn ? "AUTO ON" : "AUTO OFF";
        ImU32 txtCol = isOn ? NEON_GOLD : IM_COL32(255, 120, 120, 255);
        SetWindowFontScale(0.85f);
        ImVec2 ts = CalcTextSize(label);

        dl->AddRectFilled(ImVec2(c.x - ts.x * 0.5f - 6, c.y + R + 4),
                          ImVec2(c.x + ts.x * 0.5f + 6, c.y + R + 6 + ts.y),
                          IM_COL32(0, 0, 0, 160), 4.0f);
        dl->AddText(ImVec2(c.x - ts.x * 0.5f, c.y + R + 5), txtCol, label);
        SetWindowFontScale(1.0f);

        if (IsItemActive() && IsMouseDragging(0)) {
            dragging = true;
            pos.x += io.MouseDelta.x;
            pos.y += io.MouseDelta.y;
            pos.x = ImClamp(pos.x, 0.0f, (float)Width - S);
            pos.y = ImClamp(pos.y, 0.0f, (float)Height - S);
        }

        if (IsItemHovered() && IsMouseReleased(0) && !dragging) {
            bool newVal = !persistent_bool[O("bAutoPlay")];
            persistent_bool[O("bAutoPlay")] = newVal;
            AutoPlay::bAutoPlaying = newVal;
            if (newVal) {
                AutoPlay::ClearState();
                AutoPlay::state = AutoPlay::IDLE;
            } else {
                AutoPlay::ClearState();
            }
            save_persistence();
        }
        if (!IsItemActive()) dragging = false;
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}
// ============================================
// شاشة تسجيل الدخول
// ============================================
static bool first_time = true;

INLINE void DrawLogin(ImGuiIO& io) {
    if (logged_in) return DrawMenu(io);

    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.02f, 0.02f, 0.03f, 0.92f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = ImMin(860.0f, io.DisplaySize.x * 0.92f);
    float cardH = ImMin(820.0f, io.DisplaySize.y * 0.92f);
    cardH = ImMax(cardH, 600.0f);

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 30.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    ImVec2 winSize = GetWindowSize();
    
    dl->AddRectFilledMultiColor(winPos, winPos + winSize,
        IM_COL32(15, 15, 25, 255),
        IM_COL32(25, 25, 40, 255),
        IM_COL32(30, 30, 45, 255),
        IM_COL32(20, 20, 35, 255));
    
    dl->AddRect(winPos, winPos + winSize, IM_COL32(0, 127, 255, 200), 30.0f, 0, 2.0f);
    dl->AddRect(winPos + ImVec2(4, 4), winPos + winSize - ImVec2(4, 4), IM_COL32(0, 127, 255, 30), 28.0f, 0, 1.0f);
    
    float cornerSize = 40.0f;
    ImU32 cornerColor = IM_COL32(0, 127, 255, 150);
    dl->AddLine(winPos + ImVec2(cornerSize, 0), winPos + ImVec2(cornerSize * 2, 0), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(0, cornerSize), winPos + ImVec2(0, cornerSize * 2), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(winSize.x - cornerSize, 0), winPos + ImVec2(winSize.x - cornerSize * 2, 0), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(winSize.x, cornerSize), winPos + ImVec2(winSize.x, cornerSize * 2), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(cornerSize, winSize.y), winPos + ImVec2(cornerSize * 2, winSize.y), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(0, winSize.y - cornerSize), winPos + ImVec2(0, winSize.y - cornerSize * 2), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(winSize.x - cornerSize, winSize.y), winPos + ImVec2(winSize.x - cornerSize * 2, winSize.y), cornerColor, 2.0f);
    dl->AddLine(winPos + ImVec2(winSize.x, winSize.y - cornerSize), winPos + ImVec2(winSize.x, winSize.y - cornerSize * 2), cornerColor, 2.0f);

    if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
    SetCursorPosY(cardH * 0.08f);
    SetWindowFontScale(1.8f);
    ImVec2 titleSize = CalcTextSize(O("WATAN - QP ENO"));
    SetCursorPosX((cardW - titleSize.x) * 0.5f);
    
    ImVec2 titlePos = GetCursorScreenPos();
    dl->AddText(NULL, 0.0f, titlePos + ImVec2(2, 2), IM_COL32(0, 127, 255, 40), O("WATAN - QP ENO"));
    TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), O("WATAN - QP ENO"));
    
    ImVec2 titleEndPos = GetCursorScreenPos();
    float lineY = titleEndPos.y + 15.0f;
    float lineWidth = titleSize.x * 0.8f;
    float lineX = winPos.x + (winSize.x - lineWidth) * 0.5f;
    
    dl->AddRectFilled(ImVec2(lineX, lineY), ImVec2(lineX + lineWidth, lineY + 2.0f), IM_COL32(0, 127, 255, 100));
    dl->AddRectFilled(ImVec2(lineX + lineWidth * 0.1f, lineY + 3.0f), 
                      ImVec2(lineX + lineWidth * 0.9f, lineY + 4.0f), IM_COL32(0, 127, 255, 40));
    
    SetWindowFontScale(1.0f);
    if (g_ArialBlackFont) PopFont();

    if (is_logging_in) {
        SetCursorPosY(cardH * 0.40f);
        
        static float spinnerAngle = 0.0f;
        spinnerAngle += io.DeltaTime * 6.0f;
        ImVec2 center = winPos + ImVec2(cardW * 0.5f, cardH * 0.45f);
        
        dl->AddCircleFilled(center, 65.0f, IM_COL32(0, 127, 255, 20));
        dl->AddCircle(center, 65.0f, IM_COL32(0, 127, 255, 60), 0, 3.0f);
        
        int dotCount = 12;
        float radius = 55.0f;
        for (int i = 0; i < dotCount; i++) {
            float a = spinnerAngle + (i * 3.1415f * 2.0f / dotCount);
            float progress = (float)i / dotCount;
            ImVec2 dotPos = center + ImVec2(cosf(a), sinf(a)) * radius;
            
            float dotSize = 6.0f + 4.0f * (1.0f - progress);
            float alpha = 255.0f * (0.3f + 0.7f * (1.0f - progress));
            
            ImU32 color = IM_COL32(
                255 - (int)(100 * progress),
                127 - (int)(50 * progress),
                (int)(100 * progress),
                (int)alpha
            );
            
            dl->AddCircleFilled(dotPos, dotSize, color);
        }
        
        SetCursorPosY(cardH * 0.65f);
        ImVec2 authTextSize = CalcTextSize(L("Authenticating...", "ﺔﻗﺩﺎﺼﻤﻟﺍ ﻱﺭﺎﺟ..."));
        SetCursorPosX((cardW - authTextSize.x) * 0.5f);
        
        float pulse = 0.7f + 0.3f * sinf(io.DeltaTime * 30.0f);
        TextColored(ImVec4(0.7f * pulse, 0.7f * pulse, 0.7f * pulse, 1.0f), 
                   L("Authenticating...", "ﺔﻗﺩﺎﺼﻤﻟﺍ ﻱﺭﺎﺟ..."));
    } else {
        if (!ERROR_MESSAGE.empty()) {
            SetCursorPosY(cardH * 0.19f);
            SetWindowFontScale(0.9f);
            ImVec2 errSize = CalcTextSize(ERROR_MESSAGE.c_str());
            SetCursorPosX((cardW - errSize.x) * 0.5f);
            
            ImVec2 errPos = GetCursorScreenPos();
            dl->AddRectFilled(errPos - ImVec2(20, 10), 
                             errPos + ImVec2(errSize.x + 20, errSize.y + 20),
                             IM_COL32(255, 50, 50, 30), 10.0f);
            dl->AddRect(errPos - ImVec2(20, 10), 
                       errPos + ImVec2(errSize.x + 20, errSize.y + 20),
                       IM_COL32(255, 50, 50, 80), 10.0f, 0, 1.0f);
            
            TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", ERROR_MESSAGE.c_str());
            SetWindowFontScale(1.0f);
        }

        SetCursorPosY(cardH * 0.24f);
        float inputWidth = cardW - 160;
        float inputHeight = cardH * 0.16f;
        ImVec2 inputSize = ImVec2(inputWidth, inputHeight);
        SetCursorPosX(80);
        ImVec2 inputPos = GetCursorScreenPos();
        
        dl->AddRectFilled(inputPos, inputPos + inputSize, IM_COL32(20, 20, 35, 230), 20.0f);
        dl->AddRectFilled(inputPos, inputPos + inputSize, IM_COL32(0, 127, 255, 15), 20.0f);
        dl->AddRect(inputPos, inputPos + inputSize, IM_COL32(0, 127, 255, 80), 20.0f, 0, 2.0f);
        dl->AddRectFilled(inputPos + ImVec2(5, 5), 
                         inputPos + inputSize - ImVec2(5, 5),
                         IM_COL32(0, 127, 255, 5), 18.0f);
        
        SetCursorPosY(cardH * 0.24f + (inputHeight - 45) / 2.0f); 
        SetCursorPosX(110);
        std::string displayKey = persistent_string["key"];
        if (displayKey.empty()) displayKey = O("XXXXX-XXXXX-XXXXX-XXXXX-XXXXX");
        
        if (persistent_string["key"].empty()) {
            SetWindowFontScale(1.15f);
            TextColored(ImVec4(0.4f, 0.4f, 0.5f, 1.0f), "%s", displayKey.c_str());
        } else {
            SetWindowFontScale(1.4f);
            TextColored(ImVec4(0.9f, 0.9f, 0.95f, 1.0f), "%s", displayKey.c_str());
        }
        SetWindowFontScale(1.0f);

        float iconSize = inputSize.y * 0.5f;
        ImVec2 iconPos = inputPos + ImVec2(inputSize.x - iconSize - 15, (inputSize.y - iconSize) / 2.0f);
        
        dl->AddRectFilled(iconPos, iconPos + ImVec2(iconSize, iconSize), 
                         IM_COL32(0, 127, 255, 30), 12.0f);
        dl->AddRect(iconPos, iconPos + ImVec2(iconSize, iconSize), 
                   IM_COL32(0, 127, 255, 60), 12.0f, 0, 1.0f);
        
        if (g_IconFont) {
            PushFont(g_IconFont);
            SetWindowFontScale(1.5f);
            ImVec2 faClipboardSize = CalcTextSize("\uF46D");
            ImVec2 iconDrawPos = ImVec2(
                iconPos.x + (iconSize - faClipboardSize.x) * 0.5f,
                iconPos.y + (iconSize - faClipboardSize.y) * 0.5f
            );
            dl->AddText(iconDrawPos, IM_COL32(0, 127, 255, 220), "\uF46D");
            SetWindowFontScale(1.0f);
            PopFont();
        }

        SetCursorScreenPos(iconPos);
        if (InvisibleButton(O("##PasteBtn"), ImVec2(iconSize, iconSize))) {
            JNIEnv* env;
            jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (getEnvResult == JNI_EDETACHED) VM->AttachCurrentThread(&env, nullptr);
            
            std::string pasted = getClipboard(env);
            if (!pasted.empty()) {
                pasted.erase(pasted.find_last_not_of(" \n\r\t") + 1);
                persistent_string["key"] = pasted;
                ERROR_MESSAGE = "";
                save_persistence();
            }
        }

        SetCursorPosY(cardH * 0.50f);
        SetCursorPosX(80);
        PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.9f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.1f, 0.5f, 1.0f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.3f, 0.7f, 1.0f));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        
        bool AutoLogin = first_time && !persistent_string["key"].empty();
        
        if (AutoLogin || Button(O("##AuthBtn"), ImVec2(inputWidth, inputHeight * 1.1f))) {
            JNIEnv* env;
            jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (getEnvResult == JNI_EDETACHED) VM->AttachCurrentThread(&env, nullptr);
            
            std::string keyToUse = AutoLogin ? persistent_string["key"] : getClipboard(env);
            
            ERROR_MESSAGE = "";
            std::thread([](std::string id, std::string key) {
                Login(id, key);
            }, getAndroidID(env), keyToUse).detach();
            
            first_time = false;
        }
        
        ImVec2 btnPos = GetItemRectMin();
        ImVec2 btnSize = GetItemRectSize();
        
        dl->AddRectFilled(btnPos - ImVec2(10, 10), 
                         btnPos + btnSize + ImVec2(10, 10),
                         IM_COL32(0, 127, 255, 20), 25.0f);
        
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(1.5f);
        ImVec2 authTextSize = CalcTextSize(L("AUTHENTICATE", "ﻝﻮﺧﺪﻟﺍ ﻞﻴﺠﺴﺗ"));
        dl->AddText(ImVec2(btnPos.x + (btnSize.x - authTextSize.x) * 0.5f + 35, 
                          btnPos.y + (btnSize.y - authTextSize.y) * 0.5f), 
                   IM_COL32(255, 255, 255, 255), 
                   L("AUTHENTICATE", "ﻝﻮﺧﺪﻟﺍ ﻞﻴﺠﺴﺗ"));
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();

        PopStyleVar();
        PopStyleColor(3);

        SetCursorPosY(cardH * 0.73f);
        if (g_ArialBlackFont) PushFont(g_ArialBlackFont);
        SetWindowFontScale(0.9f);
        
        ImVec2 commSize = CalcTextSize(L("Join Community", "ﺎﻨﻌﻤﺘﺠﻣ ﻰﻟﺍ ﻢﻀﻧﺍا"));
        SetCursorPosX((cardW - commSize.x) * 0.5f);
        
        ImVec2 commPos = GetCursorScreenPos();
        TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), L("Join Community", "ﺎﻨﻌﻤﺘﺠﻣ ﻰﻟﺍ ﻢﻀﻧﺍا"));
        
        dl->AddLine(ImVec2(commPos.x - 20, commPos.y + commSize.y + 5),
                   ImVec2(commPos.x + commSize.x + 20, commPos.y + commSize.y + 5),
                   IM_COL32(0, 127, 255, 40), 1.0f);
        
        SetWindowFontScale(1.0f);
        if (g_ArialBlackFont) PopFont();
        
        float iconSide = cardH * 0.134f;
        float iconY = cardH * 0.88f;

        auto DrawTelegramIcon = [&](float centerX, const char* url, const char* label) {
            ImVec2 center = winPos + ImVec2(centerX, iconY);
            float radius = iconSide * 0.5f;
            
            dl->AddCircleFilled(center, radius, IM_COL32(0, 127, 255, 25));
            dl->AddCircle(center, radius, IM_COL32(0, 127, 255, 60), 0, 2.0f);
            
            ImVec2 p1 = center + ImVec2(-radius * 0.35f, radius * 0.15f);
            ImVec2 p2 = center + ImVec2(radius * 0.45f, -radius * 0.5f);
            ImVec2 p3 = center + ImVec2(radius * 0.3f, radius * 0.55f);
            
            dl->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 150, 255, 220));
            dl->AddLine(p1, p2, IM_COL32(0, 100, 200, 150), 1.5f);
            ImVec2 wing1 = center + ImVec2(-radius * 0.2f, radius * 0.3f);
            ImVec2 wing2 = center + ImVec2(radius * 0.1f, radius * 0.5f);
            dl->AddLine(wing1, wing2, IM_COL32(0, 100, 200, 120), 2.0f);
            dl->AddCircleFilled(center + ImVec2(-radius * 0.1f, -radius * 0.1f), 
                               radius * 0.08f, IM_COL32(255, 255, 255, 80));
            
            ImVec2 labelSize = CalcTextSize(label);
            ImVec2 labelPos = ImVec2(center.x - labelSize.x * 0.5f, center.y + radius + 15.0f);
            SetWindowFontScale(0.7f);
            dl->AddText(labelPos, IM_COL32(150, 150, 160, 200), label);
            SetWindowFontScale(1.0f);
            
            SetCursorScreenPos(center - ImVec2(radius + 10, radius + 10));
            if (InvisibleButton((O("##TeleBtn_") + std::to_string((int)centerX)).c_str(), 
                               ImVec2((radius + 10) * 2, (radius + 10) * 2 + 25))) {
                std::string cmd = "am start -a android.intent.action.VIEW -d " + std::string(url);
                system(cmd.c_str());
            }
        };

        DrawTelegramIcon(cardW * 0.30f, O("https://t.me/Qst_30"), O("Community"));
        DrawTelegramIcon(cardW * 0.50f, O("https://t.me/Qst_30"), O("Updates"));
        DrawTelegramIcon(cardW * 0.70f, O("https://t.me/Qst_30"), O("Support"));
    }

    End();
    PopStyleVar(3);
    PopStyleColor();

    End();
}

// ============================================
// إعدادات ImGui
// ============================================
INLINE void SetupImgui() {
    PACKAGE_NAME = string(getcmdline());
    ImGui::CreateContext();
    auto& style = ImGui::GetStyle();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_IsTouchScreen;
    switch_theme(current_theme);
    load_persistence();
    load_imgui_style();

    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    ImFontConfig font_cfg;
    font_cfg.SizePixels = persistent_float["fFontScale"];
    if (font_cfg.SizePixels < 8.0f) font_cfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&font_cfg);

    static const ImWchar arabic_ranges[] = {
        0x0020, 0x00FF,
        0x0600, 0x06FF,
        0x0750, 0x077F,
        0xFB50, 0xFDFF,
        0xFE70, 0xFEFF,
        0,
    };
    ImFontConfig ar_cfg;
    ar_cfg.MergeMode = true;
    ar_cfg.PixelSnapH = true;
    ar_cfg.SizePixels = font_cfg.SizePixels;
    const char* arabic_fonts[] = {
        "/system/fonts/NotoNaskhArabic-Regular.ttf",
        "/system/fonts/NotoNaskhArabicUI-Regular.ttf",
        "/system/fonts/DroidNaskh-Regular.ttf",
        "/system/fonts/Roboto-Regular.ttf",
    };
    for (auto path : arabic_fonts) {
        FILE* f = fopen(path, "rb");
        if (f) {
            fclose(f);
            io.Fonts->AddFontFromFileTTF(path, font_cfg.SizePixels, &ar_cfg, arabic_ranges);
            break;
        }
    }

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));
    bImguiSetup = true;
}

// ============================================
// دالة الرسم الرئيسية
// ============================================
DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);
    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);
    screenCenter = Vector2(Width / 2, Height / 2);
    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();

    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        DrawCaptureButton(io);
        DrawAutoPlayToggleButton(io);
        DrawFloatingButton(io);
        DrawMenu(io);
        DrawShotApprovalPrompt(io);
        DrawFPS(io);
    } else {
        DrawLogin(io);
        DrawFPS(io);
    }
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    ImGui_ClearHoverEffect();
    return _Draw(dpy, surface);
}

void __IMGUI__() {
    create_directory_recursive(CONC(O("/data/user_de/0/"), PACKAGE_NAME.c_str(), O("/no_backup")));
}

// ======================================================================
// 🔚 نهاية الملف
// ======================================================================
