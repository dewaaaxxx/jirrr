# pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>
#include <sys/system_properties.h>
#include <sys/utsname.h>

#include "resources.h"

using namespace ImGui;
using namespace std;

#include "include/includes.h"

#include "8bp.h"
#include "8bp/Ruleset.h"
#include "imgui/inc/8bp.h"
#include "keylogin.h"
#include "oxorany/oxorany.h"
#include "server_monitor.h"
#include "ac_bypass.h"
#include "imgui_bypass_panel.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <elf.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include <algorithm>

#include "ButtonClicker.h"
#include "8bp/inc/AutoPlay.h"

struct MenuState {
    bool isOpen = false;
    int currentTab = 0;
    float sidebarWidth = 300.0f;
    float animProgress = 0.0f;
    float menuAlpha = 0.0f;
    float menuScale = 0.9f;
    ImVec4 accentColor = ImVec4(0.86f, 0.10f, 0.18f, 1.0f);
    bool showSettings = false;
    bool showServerMonitor = false;
    bool showDump = false;
};
static MenuState g_menu;
static bool g_bInGame = false;
static time_t g_LoadTime = time(nullptr);

static float EaseOutBack(float x) {
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return 1.0f + c3 * powf(x - 1.0f, 3.0f) + c1 * powf(x - 1.0f, 2.0f);
}

static float EaseOutQuart(float x) {
    return 1.0f - powf(1.0f - x, 4.0f);
}

static void DrawGradientRect(ImDrawList* dl, ImVec2 p1, ImVec2 p2, ImU32 col1, ImU32 col2, bool horizontal = true) {
    if (horizontal) {
        dl->AddRectFilledMultiColor(p1, p2, col1, col2, col2, col1);
    } else {
        dl->AddRectFilledMultiColor(p1, p2, col1, col1, col2, col2);
    }
}

// ── Draw a perfectly-centred X close button; returns true when clicked ──
static bool DrawCloseButton(const char* id, ImVec2 pos, float size, bool* hovered_out = nullptr) {
    using namespace ImGui;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    SetCursorPos(pos);
    ImVec2 sp = GetCursorScreenPos();
    bool hov = false, held = false;
    bool pressed = InvisibleButton(id, ImVec2(size, size));
    hov = IsItemHovered();
    if (hovered_out) *hovered_out = hov;
    ImDrawList* dl = window->DrawList;
    ImU32 bg = hov ? IM_COL32(255,60,80,200) : IM_COL32(255,255,255,18);
    dl->AddCircleFilled(ImVec2(sp.x+size*0.5f, sp.y+size*0.5f), size*0.5f, bg, 16);
    float m = size * 0.28f;
    ImU32 xc = hov ? IM_COL32(255,255,255,255) : IM_COL32(255,255,255,140);
    dl->AddLine(ImVec2(sp.x+m, sp.y+m), ImVec2(sp.x+size-m, sp.y+size-m), xc, 2.0f);
    dl->AddLine(ImVec2(sp.x+size-m, sp.y+m), ImVec2(sp.x+m, sp.y+size-m), xc, 2.0f);
    return pressed;
}

// ── License countdown from g_ExpTime string ("YYYY-MM-DD HH:MM:SS") ──
struct LicCountdown { int days, hrs, mins, secs; bool expired, valid; };
static LicCountdown ComputeLicCountdown() {
    LicCountdown cd = {0,0,0,0,false,false};
    if (g_ExpTime.empty() || g_ExpTime == "N/A") {
        cd.days = g_DaysLeft; cd.valid = (g_DaysLeft > 0); return cd;
    }
    int Y=0,Mo=0,D=0,h=0,m=0,s=0;
    sscanf(g_ExpTime.c_str(), "%d-%d-%d", &Y, &Mo, &D);
    const char* tp = g_ExpTime.c_str() + 10;
    while (*tp && (*tp < '0' || *tp > '9')) tp++;
    if (*tp) sscanf(tp, "%d:%d:%d", &h, &m, &s);
    struct tm tm = {};
    tm.tm_year=Y-1900; tm.tm_mon=Mo-1; tm.tm_mday=D;
    tm.tm_hour=h; tm.tm_min=m; tm.tm_sec=s; tm.tm_isdst=-1;
    time_t exp = mktime(&tm);
    time_t now = time(nullptr);
    long diff = (long)difftime(exp, now);
    if (diff <= 0) { cd.expired = true; cd.valid = true; return cd; }
    cd.valid = true;
    cd.days = (int)(diff/86400); diff %= 86400;
    cd.hrs  = (int)(diff/3600);  diff %= 3600;
    cd.mins = (int)(diff/60);    cd.secs = (int)(diff%60);
    return cd;
}

static bool SidebarButton(const char* label, const char* icon, bool selected, float width) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(width - 20.0f, 58.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);

    static std::map<ImGuiID, float> hoverAnim;
    float& animT = hoverAnim[id];
    float targetT = (selected || hovered) ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 12.0f;

    ImDrawList* dl = window->DrawList;
    
    if (selected) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(40, 5, 10, 255), 20.0f);
        dl->AddRect(bb.Min, bb.Max, IM_COL32(220, 30, 50, 80), 20.0f, 0, 1.0f);
        dl->AddRectFilled(ImVec2(bb.Min.x, bb.Min.y + 4), ImVec2(bb.Min.x + 4, bb.Max.y - 4), IM_COL32(220, 30, 50, 255), 2.0f);
    } else if (animT > 0.01f) {
        ImU32 hoverCol = IM_COL32(120, 10, 20, (int)(140 * animT));
        dl->AddRectFilled(bb.Min, bb.Max, hoverCol, 12.0f);
    }

    float iconOffset = 8.0f * animT;
    ImVec2 iconPos = ImVec2(bb.Min.x + 22.0f + iconOffset, bb.Min.y + (size.y - g.FontSize) * 0.5f);
    dl->AddText(iconPos, selected ? IM_COL32(220, 30, 50, 255) : IM_COL32(180, 20, 40, (int)(180 + 75 * animT)), icon);
    
    ImVec2 textPos = ImVec2(bb.Min.x + 58.0f + iconOffset, bb.Min.y + (size.y - g.FontSize) * 0.5f);
    dl->AddText(textPos, selected ? IM_COL32(255, 220, 225, 255) : IM_COL32(200, 25, 45, (int)(200 + 55 * animT)), label);

    return pressed;
}

static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    float height = 32.0f;
    float width = 56.0f;
    float radius = height * 0.5f;

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, ImMax(height, textSize.y) + style.FramePadding.y * 2 + 10.0f);

    const ImRect bb(pos, pos + size);
    ItemSize(size, style.FramePadding.y);
    if (!ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) *v = !*v;

    static std::map<ImGuiID, float> switchAnim;
    float& animT = switchAnim[id];
    float targetT = *v ? 1.0f : 0.0f;
    animT += (targetT - animT) * g.IO.DeltaTime * 14.0f;

    ImDrawList* dl = window->DrawList;
    
    if (hovered) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(45, 45, 55, 100), 10.0f);
    }
    
    ImVec2 togglePos = ImVec2(bb.Max.x - width - 15.0f, bb.Min.y + (size.y - height) * 0.5f);
    ImVec2 toggleEnd = ImVec2(togglePos.x + width, togglePos.y + height);
    
    ImVec4 offColor = ImVec4(0.10f, 0.02f, 0.04f, 1.0f);
    ImVec4 onColor  = ImVec4(0.86f, 0.10f, 0.18f,  1.0f);
    ImVec4 bgColorV = ImLerp(offColor, onColor, animT);
    dl->AddRectFilled(togglePos, toggleEnd, ImColor(bgColorV), radius);
    dl->AddRect(togglePos, toggleEnd, IM_COL32(220, 30, 50, (int)(60 + 140 * animT)), radius, 0, 1.0f);

    float knobX = togglePos.x + radius + (width - height) * animT;
    float knobY = togglePos.y + radius;
    float knobR = radius - 4.0f;

    if (animT > 0.01f)
        dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 4.0f, IM_COL32(220, 30, 50, (int)(60 * animT)));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR + 2.0f, IM_COL32(0, 0, 0, 40));
    dl->AddCircleFilled(ImVec2(knobX, knobY), knobR, IM_COL32(255, 255, 255, 255));

    dl->AddText(ImVec2(bb.Min.x + 15.0f, bb.Min.y + (size.y - textSize.y) * 0.5f), IM_COL32(255, 220, 225, 255), label);

    return pressed;
}

// ── Theme-aware palette helper ──────────────────────────────────────
// Fills cr[5], optionally crDim[5] (alpha 45) and crMid[5] (alpha 110).
static const int THEME_COUNT = 10;
static const char* THEME_GRAD_NAMES[THEME_COUNT] = {
    "Chrome Rainbow","Neon Cyan","Deep Purple","Matrix Green","Blood Red",
    "Sunset","Ocean","Golden Hour","Midnight","Rose Gold",
};
static const ImU32 THEME_GRAD_BASE[THEME_COUNT][5] = {
    // 0 Chrome Rainbow
    { IM_COL32(220, 30, 50,255), IM_COL32(255, 80, 90,255),
      IM_COL32(200, 20, 40,255), IM_COL32(240, 60, 70,255),
      IM_COL32(255,200,210,255) },
    // 1 Neon Cyan
    { IM_COL32(  0,230,255,255), IM_COL32(240, 60, 70,255),
      IM_COL32(200, 20, 40,255), IM_COL32(255, 50, 70,255),
      IM_COL32(255, 180, 190,255) },
    // 2 Deep Purple
    { IM_COL32(255, 80, 90,255), IM_COL32(200,160,255,255),
      IM_COL32(130,100,220,255), IM_COL32(220,190,255,255),
      IM_COL32(100, 80,200,255) },
    // 3 Matrix Green
    { IM_COL32(240, 60, 70,255), IM_COL32( 80,255,120,255),
      IM_COL32(  0,200, 80,255), IM_COL32(150,255,200,255),
      IM_COL32( 30,160,100,255) },
    // 4 Blood Red
    { IM_COL32(220, 55, 55,255), IM_COL32(255, 80, 60,255),
      IM_COL32(180, 30, 30,255), IM_COL32(255,120, 80,255),
      IM_COL32(255,160,  0,255) },
    // 5 Sunset
    { IM_COL32(255, 94, 77,255), IM_COL32(255,140, 60,255),
      IM_COL32(255,200, 50,255), IM_COL32(255,120,100,255),
      IM_COL32(230, 60,100,255) },
    // 6 Ocean
    { IM_COL32(  0, 80,180,255), IM_COL32(  0,130,220,255),
      IM_COL32(  0,200,210,255), IM_COL32( 50,160,255,255),
      IM_COL32(  0, 50,150,255) },
    // 7 Golden Hour
    { IM_COL32(255,210,  0,255), IM_COL32(255,165,  0,255),
      IM_COL32(255,120, 20,255), IM_COL32(255,240,100,255),
      IM_COL32(200,140,  0,255) },
    // 8 Midnight
    { IM_COL32( 80, 50,200,255), IM_COL32(120, 80,255,255),
      IM_COL32( 50,  0,150,255), IM_COL32(180,100,255,255),
      IM_COL32( 30, 20,100,255) },
    // 9 Rose Gold
    { IM_COL32(255,130,140,255), IM_COL32(230,100,120,255),
      IM_COL32(200, 80,100,255), IM_COL32(255,200,180,255),
      IM_COL32(210,160,140,255) },
};

static void FillThemePalette(ImU32 cr[5],
                              ImU32 crDim[5] = nullptr,
                              ImU32 crMid[5] = nullptr) {
    // Solid colour mode: override all palette entries with a single hue
    if (persistent_bool["bSolidColor"]) {
        float h = persistent_float["fSolidHue"];
        const float offsets[5] = { 0.f, 0.04f, -0.04f, 0.08f, -0.08f };
        const float vals[5]    = { 1.f, 0.90f,  0.85f, 0.80f,  0.95f };
        for (int i = 0; i < 5; i++) {
            float r, g, b;
            ImGui::ColorConvertHSVtoRGB(ImFmod(h + offsets[i] + 1.f, 1.f), 1.0f, vals[i], r, g, b);
            cr[i] = IM_COL32((int)(r*255),(int)(g*255),(int)(b*255),255);
            if (crDim) crDim[i] = (cr[i] & 0x00FFFFFFu) | 0x2D000000u;
            if (crMid) crMid[i] = (cr[i] & 0x00FFFFFFu) | 0x6E000000u;
        }
        return;
    }
    int t = (int)persistent_float["fTheme"];
    if (t < 0 || t >= THEME_COUNT) t = 0;
    for (int i = 0; i < 5; i++) {
        cr[i] = THEME_GRAD_BASE[t][i];
        if (crDim) crDim[i] = (THEME_GRAD_BASE[t][i] & 0x00FFFFFFu) | 0x2D000000u;
        if (crMid) crMid[i] = (THEME_GRAD_BASE[t][i] & 0x00FFFFFFu) | 0x6E000000u;
    }
}

static void DrawLiveStatusOverlay(ImGuiIO& io) {
    if (!persistent_bool[O("bAutoPlay")]) return;

    const char* stateStr = "Idle";
    switch (AutoPlay::state) {
        case AutoPlay::SCANNING:   stateStr = "Scanning";   break;
        case AutoPlay::NOMINATING: stateStr = "Nominating"; break;
        case AutoPlay::EXECUTING:  stateStr = "Executing";  break;
        default:                   stateStr = "Idle";       break;
    }
    bool isPlaying = AutoPlay::bAutoPlaying;

    const float padH  = 24.0f;  // jarak dari tepi KANAN layar
    const float padV  = 24.0f;  // jarak dari tepi BAWAH layar

    // ANCHOR DIUBAH: dari kiri-bawah (0.0f, 1.0f) jadi kanan-bawah (1.0f, 1.0f)
    SetNextWindowPos(
        ImVec2(io.DisplaySize.x - padH, io.DisplaySize.y - padV),
        ImGuiCond_Always,
        ImVec2(1.0f, 1.0f)   // anchor: kanan-bawah
    );

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(14, 14, 18, 185));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(60, 60, 80, 120));
    PushStyleVar(ImGuiStyleVar_WindowRounding,  12.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(14.0f, 10.0f));

    if (Begin(O("##LiveStatus"), nullptr,
              ImGuiWindowFlags_NoTitleBar   | ImGuiWindowFlags_NoResize    |
              ImGuiWindowFlags_NoMove       | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoInputs     | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 wp = GetWindowPos();
        ImVec2 ws = GetWindowSize();
        ImU32 accentCol = isPlaying
            ? IM_COL32(0, 210, 130, 255)
            : IM_COL32(200, 40, 40, 255);
        
        // Accent bar di KIRI window (tetap, karena window sekarang di kanan)
        dl->AddRectFilled(wp, ImVec2(wp.x + 3.0f, wp.y + ws.y), accentCol, 12.0f, ImDrawFlags_RoundCornersLeft);

        SetWindowFontScale(0.95f);

        ImU32 playCol = isPlaying ? IM_COL32(0, 210, 130, 255) : IM_COL32(200, 60, 60, 255);
        TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(140, 140, 155, 255)), O("Auto Play "));
        SameLine(0, 0);
        TextColored(ImGui::ColorConvertU32ToFloat4(playCol), isPlaying ? O("ON") : O("OFF"));

        ImU32 stateCol = (AutoPlay::state != AutoPlay::IDLE)
            ? IM_COL32(0, 200, 255, 255)
            : IM_COL32(130, 130, 145, 255);
        TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(140, 140, 155, 255)), O("State     "));
        SameLine(0, 0);
        TextColored(ImGui::ColorConvertU32ToFloat4(stateCol), stateStr);
    if (g_CurrentCandidate.pocketIndex >= 0) {
    TextColored(ImGui::ColorConvertU32ToFloat4(IM_COL32(140, 140, 155, 255)), O("Pocket    "));
    SameLine(0, 0);
    
    // Warna pocket beda beda biar lebih jelas
    ImU32 pocketColor;
    switch (g_CurrentCandidate.pocketIndex) {
        case 0: pocketColor = IM_COL32(255, 100, 100, 255); break; // Merah (kiri atas)
        case 1: pocketColor = IM_COL32(100, 255, 100, 255); break; // Hijau (tengah atas)
        case 2: pocketColor = IM_COL32(100, 100, 255, 255); break; // Biru (kanan atas)
        case 3: pocketColor = IM_COL32(255, 200, 100, 255); break; // Kuning (kanan bawah)
        case 4: pocketColor = IM_COL32(200, 100, 255, 255); break; // Ungu (tengah bawah)
        case 5: pocketColor = IM_COL32(100, 255, 200, 255); break; // Toska (kiri bawah)
        default: pocketColor = IM_COL32(150, 150, 150, 255);
    }
    
    char pocketText[32];
    snprintf(pocketText, sizeof(pocketText), " %d", g_CurrentCandidate.pocketIndex);
    TextColored(ImGui::ColorConvertU32ToFloat4(pocketColor), pocketText);
    }

        SetWindowFontScale(1.0f);
    }
    End();
    PopStyleVar(3);
    PopStyleColor(2);
}

// ── Break Predictor ──────────────────────────────────────────────────────────
namespace BreakPredictor {
    static int   sessionWins   = 0;
    static int   sessionLosses = 0;
    static float lastConfidence = -1.f;
    static int   lastGameMode   = -1;

    // Track win/loss dari luar (panggil ini di akhir game)
    void RecordWin()  { sessionWins++;   lastConfidence = -1.f; }
    void RecordLoss() { sessionLosses++; lastConfidence = -1.f; }

    static const char* GetTableName(int mode) {
        switch (mode) {
            case 0:  return "Billiard Cafe";
            case 1:  return "Sydney Saloon";
            case 2:  return "London Pub";
            case 3:  return "Moscow Winter";
            case 4:  return "Paris Chateau";
            case 5:  return "Rio Carnaval";
            case 6:  return "Dubai Desert";
            case 7:  return "Tokyo Bay";
            case 8:  return "Las Vegas High";
            default: return "Pool Table";
        }
    }

    static float CalcConfidence() {
        if (lastConfidence >= 0.f && lastGameMode == sharedGameManager.mGameMode()) 
            return lastConfidence;

        float conf = 50.0f; // base

        // Faktor 1: Session win rate
        int total = sessionWins + sessionLosses;
        if (total > 0) {
            float wr = (float)sessionWins / (float)total;
            conf += (wr - 0.5f) * 30.f; // ±15%
        }

        // Faktor 2: Coins (proxy experience)
        int coins = sharedUserInfo ? (int)sharedUserInfo.coins() : 0;
        if      (coins > 100000000) conf += 12.f;
        else if (coins > 10000000)  conf += 8.f;
        else if (coins > 1000000)   conf += 4.f;
        else if (coins < 100000)    conf -= 5.f;

        // Faktor 3: Sisa session streak
        int streak = sessionWins - sessionLosses;
        if (streak >= 3)       conf += 8.f;
        else if (streak >= 1)  conf += 3.f;
        else if (streak <= -3) conf -= 8.f;
        else if (streak <= -1) conf -= 3.f;

        // Faktor 4: Game mode bonus (high stakes = lebih serius)
        int mode = sharedGameManager.mGameMode();
        if (mode >= 6) conf += 5.f;
        else if (mode >= 3) conf += 2.f;

        // Noise kecil biar keliatan natural (±3%)
        float noise = ((float)(rand() % 600) - 300) * 0.01f;
        conf += noise;

        // Clamp 42–97%
        if (conf < 42.f) conf = 42.f;
        if (conf > 97.f) conf = 97.f;

        lastConfidence = conf;
        lastGameMode   = mode;
        return conf;
    }

    static const char* GetLabel(float conf) {
        if (conf >= 85.f) return "HIGH CONFIDENCE";
        if (conf >= 70.f) return "GOOD CHANCE";
        if (conf >= 55.f) return "MODERATE";
        return "LOW CONFIDENCE";
    }

    static ImU32 GetBarColor(float conf) {
        if (conf >= 85.f) return IM_COL32(50, 210, 80, 255);
        if (conf >= 70.f) return IM_COL32(160, 210, 50, 255);
        if (conf >= 55.f) return IM_COL32(220, 180, 40, 255);
        return IM_COL32(210, 70, 50, 255);
    }

    INLINE void Draw() {
        if (!sharedGameManager) return;
        if (!persistent_bool["bBreakPredictor"]) return;

        ImGuiIO& io    = GetIO();
        ImDrawList* dl = GetForegroundDrawList();

        float conf     = CalcConfidence();
        int   mode     = sharedGameManager.mGameMode();
        const char* tableName = GetTableName(mode);
        const char* label     = GetLabel(conf);
        ImU32 barCol          = GetBarColor(conf);

        // ── Layout ──────────────────────────────────────────────────────
        const float W       = 340.f;
        const float H       = 110.f;
        const float pad     = 14.f;
        const float rnd     = 12.f;

        float posX = (io.DisplaySize.x - W) * 0.5f;
        float posY = io.DisplaySize.y * 0.82f;

        ImVec2 p0 = ImVec2(posX, posY);
        ImVec2 p1 = ImVec2(posX + W, posY + H);

        // Background
        dl->AddRectFilled(p0, p1, IM_COL32(10, 20, 10, 230), rnd);
        dl->AddRect(p0, p1, barCol, rnd, 0, 2.f);

        // Title row
        char titleBuf[64];
        snprintf(titleBuf, sizeof(titleBuf), "THE BREAK  |  %s", tableName);
        ImVec2 titleSz = CalcTextSize(titleBuf);
        dl->AddText(
            ImVec2(posX + (W - titleSz.x) * 0.5f, posY + pad),
            IM_COL32(255, 255, 255, 230), titleBuf
        );

        // Confidence bar
        float barY  = posY + pad + 22.f;
        float barW  = W - pad * 2.f;
        float barH  = 18.f;
        float fill  = barW * (conf / 100.f);

        // Bar background
        dl->AddRectFilled(
            ImVec2(posX + pad, barY),
            ImVec2(posX + pad + barW, barY + barH),
            IM_COL32(30, 30, 30, 200), barH * 0.5f
        );
        // Bar fill
        dl->AddRectFilled(
            ImVec2(posX + pad, barY),
            ImVec2(posX + pad + fill, barY + barH),
            barCol, barH * 0.5f
        );

        // Confidence percent
        char confBuf[16];
        snprintf(confBuf, sizeof(confBuf), "%.1f%%", conf);
        ImVec2 confSz = CalcTextSize(confBuf);
        dl->AddText(
            ImVec2(posX + (W - confSz.x) * 0.5f, barY + barH + 6.f),
            IM_COL32(255, 255, 255, 220), confBuf
        );

        // Label (HIGH CONFIDENCE dll)
        ImVec2 lblSz = CalcTextSize(label);
        dl->AddText(
            ImVec2(posX + (W - lblSz.x) * 0.5f, barY + barH + 24.f),
            barCol, label
        );

        // Session stats kecil di bawah
        char statBuf[48];
        snprintf(statBuf, sizeof(statBuf), "Session  W:%d  L:%d", sessionWins, sessionLosses);
        ImVec2 stSz = CalcTextSize(statBuf);
        SetWindowFontScale(0.75f);
        dl->AddText(
            ImVec2(posX + (W - stSz.x * 0.75f) * 0.5f, posY + H - 20.f),
            IM_COL32(120, 120, 120, 180), statBuf
        );
        SetWindowFontScale(1.0f);
    }
}

#include "dump_panel.h"

INLINE void DrawAutoQueue() {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        static std::chrono::steady_clock::time_point last_call_time;
        static std::chrono::steady_clock::time_point countdown_start;
        static bool counting = false;

        int aqMode = persistent_int["iAutoQueue_Mode"];

        auto now = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_call_time).count() > 500) {
            counting = false;
        }
        last_call_time = now;

        if (!counting) {
            counting = true;
            countdown_start = now;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - countdown_start).count();

        // Mode 1 (Smart): delay scales with BetPercent — lower coins = longer wait
        int countdown_ms = 3000;
        if (aqMode == 1) {
            int betPct = persistent_int["iAutoQueue_BetPercent"];
            if (betPct <= 0) betPct = 1;
            // 100% bet → 2s delay, 1% bet → 8s delay
            countdown_ms = 2000 + (int)((1.0f - betPct / 100.0f) * 6000.0f);
        }

        int remaining_ms = countdown_ms - elapsed;

        if (remaining_ms <= 0) {
            if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            StartLastMatch();
            counting = false;
            return;
        }

        SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Always);
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.05f, 0.06f, 0.98f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);

        if (Begin(O("##AutoQueue"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImDrawList* dl = GetWindowDrawList();
            ImVec2 winPos = GetWindowPos();
            ImVec2 winSize = GetWindowSize();
            
            DrawGradientRect(dl, winPos, ImVec2(winPos.x + winSize.x, winPos.y + 70), IM_COL32(160, 10, 25, 255), IM_COL32(200, 20, 40, 255), true);
            dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + 20), IM_COL32(160, 10, 25, 255), 20.0f, ImDrawFlags_RoundCornersTop);
            
            ImVec2 titleSize = CalcTextSize(O("Auto Queue"));
            dl->AddText(ImVec2(winPos.x + (winSize.x - titleSize.x) * 0.5f, winPos.y + 22), IM_COL32(255, 255, 255, 255), O("Auto Queue"));

            SetCursorPosY(90);
            float font_scale = 3.5f;
            SetWindowFontScale(font_scale);

            std::string count_str = std::to_string((remaining_ms / 1000) + 1);
            auto text_size = CalcTextSize(count_str.c_str());
            SetCursorPosX((winSize.x - text_size.x) * 0.5f);
            TextColored(ImVec4(0.86f, 0.10f, 0.18f, 1.0f), "%s", count_str.c_str());

            SetWindowFontScale(1.0f);

            SetCursorPosY(winSize.y - 75);
            SetCursorPosX(25);
            PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.25f, 0.25f, 1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
            
            if (Button(O("Cancel"), ImVec2(winSize.x - 50, 50))) {
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

INLINE void DrawESP(ImDrawList* draw) {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        if (!sharedGameManager) return;

        UpdateScreenTable();

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
        g_bInGame = mainStateManager.isInGame();
        if (g_bInGame) {  // ← cuma jalan kalo lagi di dalam match
            AutoAimV2::aa_update();
        }
        if (!g_bInGame) {
            if (persistent_bool[O("bAutoQueue")]) {
                if (!sharedMenuManager.isInQueue()) DrawAutoQueue();
            }
            if (!persistent_bool[O("bESPAlways")]) return;
        }

        auto visualCue = sharedGameManager.mVisualCue();

        Ball::Classification myclass = sharedGameManager.getPlayerClassification();

        Table table = sharedGameManager.mTable;
        if (!table) return;

        auto tableProperties = table.mTableProperties();
        if (!tableProperties) return;

        auto& pockets = tableProperties.mPockets();

        if (persistent_bool[O("bESP_DrawPockets")] && g_bInGame) {
            float pktA = persistent_float["fPocketAlpha"];
            if (pktA < 0.01f) pktA = 1.0f;
            ImU32 pktCol = IM_COL32(255,255,255,(int)(pktA * 255.f));
            for (int i = 0; i < 6; i++) {
            auto screenPos = WorldToScreen(pockets[i]);
            
            char numBuf[4];
            snprintf(numBuf, sizeof(numBuf), "%d", i);
            ImVec2 ts = GImGui->Font->CalcTextSizeA(GImGui->FontSize, FLT_MAX, 0.0f, numBuf);
            draw->AddText(ImVec2(screenPos.x - ts.x * 0.5f, screenPos.y - ts.y * 0.5f), pktCol, numBuf);
                
                
                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), persistent_float["fPocketRad"] * 10.f, pktCol, 0, 3.f);
            }
        }

        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;

        if (persistent_bool[O("bAutoPlay")] && g_bInGame) AutoPlay::Update();
        if (!g_bInGame) BreakPredictor::Draw();

        auto stateId = gameStateManager.getCurrentStateId();
        bool isOpponentTurn = (stateId == 6 || stateId == 7 || stateId == 8);

        // Compute predictions: always when it's our turn (stateId 4),
        // or continuously when ESP Always is active (catches opponent aiming)
        if (stateId == 4 || (isOpponentTurn && persistent_bool[O("bESPAlways")])) {
            gPrediction->determineShotResult(false);
        }

        if (isOpponentTurn) {
            if (!persistent_bool[O("bESPAlways")]) return;
        }

        if (persistent_bool[O("bESP_DrawPocketsShotState")] && g_bInGame) {
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto screenPos = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(screenPos.x, screenPos.y), persistent_float["fPocketRad"] * 10.f, GREEN, 0, 5.f);
                }
            }
        }

        if (persistent_bool[O("bESP_DrawPredictionLine")] && g_bInGame) {
            float predA = persistent_float["fPredAlpha"];
            if (predA < 0.01f) predA = 1.0f;
            auto getCol = [&](int idx) -> ImU32 {
                ImVec4 v = colors[idx].Value;
                v.w = predA;
                return ColorConvertFloat4ToU32(v);
            };

            // Helper: draw a dotted/dashed line between two points
            auto AddDottedLine = [&](ImVec2 a, ImVec2 b, ImU32 col, float thickness, float dotLen = 8.f, float gapLen = 8.f) {
                float dx = b.x - a.x;
                float dy = b.y - a.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < 0.1f) return;
                float nx = dx / dist, ny = dy / dist;
                float t = 0.f;
                bool drawing = true;
                while (t < dist) {
                    float segLen = drawing ? dotLen : gapLen;
                    float end = t + segLen;
                    if (end > dist) end = dist;
                    if (drawing) {
                        draw->AddLine(
                            ImVec2(a.x + nx * t,   a.y + ny * t),
                            ImVec2(a.x + nx * end, a.y + ny * end),
                            col, thickness
                        );
                    }
                    t = end;
                    drawing = !drawing;
                }
            };

            float lineThick = persistent_float["fLineThick"];

            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition != ball.predictedPosition) {
                    ImVec2 lastPos{};
                    ImU32 col = getCol(i);
                    // Stripe balls (index 9-15) use dotted line, solid/cue/8ball use solid line
                    bool isDotted = (i >= 9 && i <= 15);
                    for (int j = 1; j < (int)ball.positions.size(); j++) {
                        auto point = WorldToScreen(ball.positions[j]);
                        if (lastPos.x || lastPos.y) {
                            if (isDotted)
                                AddDottedLine(lastPos, point, col, lineThick);
                            else
                                draw->AddLine(lastPos, point, col, lineThick);
                        }
                        lastPos = point;
                    }
                }
            }
            for (int i = 0; i < gPrediction->guiData.ballsCount; i++) {
                auto& ball = gPrediction->guiData.balls[i];
                if (ball.initialPosition != ball.predictedPosition) {
                    ImU32 col = getCol(i);
                    bool isStripe = (i >= 9 && i <= 15);
                    float circleR = 20.f;

                    // Start circle (hollow)
                    draw->AddCircle(WorldToScreen(ball.initialPosition), circleR, col, 0, 6.f);

                    // End circle (filled)
                    ImVec2 endPos = WorldToScreen(ball.predictedPosition);
                    draw->AddCircleFilled(endPos, circleR, col);

                    // Stripe balls: draw minus sign ( — ) inside end circle
                    if (isStripe) {
                        float minusHalfW = circleR * 0.55f;
                        float minusThick = circleR * 0.28f;
                        draw->AddLine(
                            ImVec2(endPos.x - minusHalfW, endPos.y),
                            ImVec2(endPos.x + minusHalfW, endPos.y),
                            IM_COL32(255, 255, 255, 220),
                            minusThick
                        );
                    }
                }
            }
        }
    }
}

static void DrawSidebar(float sidebarW, float winH) {
    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();

    // Sidebar dark navy background
    DrawGradientRect(dl, winPos, ImVec2(winPos.x + sidebarW, winPos.y + winH), IM_COL32(8, 2, 3, 255), IM_COL32(12, 3, 5, 255), false);

    // Top accent line (cyan)
    dl->AddRectFilled(winPos, ImVec2(winPos.x + sidebarW, winPos.y + 3), IM_COL32(220, 30, 50, 200));

    // Sidebar right divider (dim cyan)
    dl->AddLine(ImVec2(winPos.x + sidebarW, winPos.y), ImVec2(winPos.x + sidebarW, winPos.y + winH), IM_COL32(60, 8, 12, 255), 1.0f);

    SetCursorPos(ImVec2(0, 20));
    BeginGroup();

    Dummy(ImVec2(sidebarW, 5));
    SetCursorPosX(22);

    // Logo — LYN4XP in cyan
    PushStyleColor(ImGuiCol_Text, ImVec4(0.86f, 0.10f, 0.18f, 1.0f));
    SetWindowFontScale(1.2f);
    Text(O("CELZ MODZ"));
    SetWindowFontScale(1.0f);
    PopStyleColor();

    SetCursorPosX(22);
    TextColored(ImVec4(0.60f, 0.60f, 0.65f, 1.0f), O("Targeting Sys v1.0"));

    // Thin separator
    Dummy(ImVec2(sidebarW, 8));
    ImVec2 sepA = ImVec2(winPos.x + 14, GetCursorScreenPos().y);
    ImVec2 sepB = ImVec2(winPos.x + sidebarW - 14, sepA.y);
    dl->AddLine(sepA, sepB, IM_COL32(60, 8, 12, 120), 1.0f);
    Dummy(ImVec2(sidebarW, 18));

    SetCursorPosX(15);
    if (SidebarButton(O("ESP"), O(">"), g_menu.currentTab == 0, sidebarW)) g_menu.currentTab = 0;

    Dummy(ImVec2(0, 5));
    SetCursorPosX(15);
    if (SidebarButton(O("Auto Play"), O(">"), g_menu.currentTab == 1, sidebarW)) g_menu.currentTab = 1;

    Dummy(ImVec2(0, 5));
    SetCursorPosX(15);
    if (SidebarButton(O("Auto Queue"), O(">"), g_menu.currentTab == 2, sidebarW)) g_menu.currentTab = 2;

    Dummy(ImVec2(0, 5));
    SetCursorPosX(15);
    if (SidebarButton(O("Bypass"), O(">"), g_menu.currentTab == 3, sidebarW)) g_menu.currentTab = 3;

    Dummy(ImVec2(0, 5));
    SetCursorPosX(15);
    if (SidebarButton(O("Dump"), O(">"), g_menu.currentTab == 4, sidebarW)) {
        g_menu.currentTab = 4;
        g_menu.showDump = true;
    }

    // ── User info card ─────────────────────────────────────────────────────────
    Dummy(ImVec2(sidebarW, 14));
    {
        ImVec2 sepA2 = ImVec2(winPos.x + 14, GetCursorScreenPos().y);
        ImVec2 sepB2 = ImVec2(winPos.x + sidebarW - 14, sepA2.y);
        dl->AddLine(sepA2, sepB2, IM_COL32(60, 8, 12, 100), 1.0f);
    }
    Dummy(ImVec2(sidebarW, 10));

    // Username row
    SetCursorPosX(22);
    {
        std::string uname = g_Username.empty()
            ? (persistent_string.count("username") ? persistent_string["username"] : "User")
            : g_Username;
        std::string uDisplay = uname.size() > 16 ? uname.substr(0, 14) + ".." : uname;
        TextColored(ImVec4(0.86f, 0.10f, 0.18f, 1.0f), O("  %s"), uDisplay.c_str());
    }

    // Days left row — colour-coded
    Dummy(ImVec2(sidebarW, 3));
    SetCursorPosX(22);
    {
        int dl_val = g_DaysLeft;
        ImVec4 dlColor = (dl_val <= 3)  ? ImVec4(1.0f, 0.35f, 0.35f, 1.0f)
                       : (dl_val <= 7)  ? ImVec4(1.0f, 0.75f, 0.15f, 1.0f)
                       :                  ImVec4(0.25f, 0.85f, 0.45f, 1.0f);
        char dlBuf[32];
        if (dl_val > 0)
            snprintf(dlBuf, sizeof(dlBuf), O("  %d days left"), dl_val);
        else
            snprintf(dlBuf, sizeof(dlBuf), O("  %s"), g_ExpTime.c_str());
        TextColored(dlColor, "%s", dlBuf);
    }

    Dummy(ImVec2(sidebarW, 8));

    EndGroup();
}

static void DrawContentArea(float sidebarW, float winW, float winH, ImGuiIO& io) {
    bool need_save = false;
    
    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    // Content area header — dark navy
    DrawGradientRect(dl, ImVec2(winPos.x + sidebarW, winPos.y), ImVec2(winPos.x + winW, winPos.y + 65), IM_COL32(10, 3, 4, 255), IM_COL32(14, 4, 6, 255), false);

    // Top accent line on content area (matches sidebar)
    dl->AddRectFilled(ImVec2(winPos.x + sidebarW, winPos.y), ImVec2(winPos.x + winW, winPos.y + 3), IM_COL32(220, 30, 50, 200));

    // Bottom separator — cyan dim
    dl->AddLine(ImVec2(winPos.x + sidebarW, winPos.y + 65), ImVec2(winPos.x + winW, winPos.y + 65), IM_COL32(60, 8, 12, 200), 1.0f);

    // Small cyan left accent bar before tab title
    dl->AddRectFilled(ImVec2(winPos.x + sidebarW + 14, winPos.y + 18), ImVec2(winPos.x + sidebarW + 18, winPos.y + 48), IM_COL32(220, 30, 50, 220), 2.0f);

    const char* tabTitles[] = { "ESP Settings", "Auto Play", "Auto Queue", "AC Bypass", "Mem Dump" };

    SetCursorPos(ImVec2(sidebarW + 30, 22));
    SetWindowFontScale(1.15f);
    TextColored(ImVec4(0.86f, 0.10f, 0.18f, 1.0f), "%s", tabTitles[g_menu.currentTab < 5 ? g_menu.currentTab : 0]);
    SetWindowFontScale(1.0f);
    
    SetCursorPos(ImVec2(sidebarW + 15, 80));
    
    PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(0.86f, 0.10f, 0.18f, 0.55f));
    PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.0f, 0.90f, 1.0f, 0.75f));
    PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImVec4(0.0f, 1.00f, 1.0f, 0.95f));
    PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.0f);
    BeginChild(O("##ContentArea"), ImVec2(winW - sidebarW - 30, winH - 95), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    
    switch (g_menu.currentTab) {
        case 0: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Draw Prediction Lines"), &persistent_bool[O("bESP_DrawPredictionLine")]);
            need_save |= ToggleSwitch(O("Draw Pockets"), &persistent_bool[O("bESP_DrawPockets")]);
            need_save |= ToggleSwitch(O("Draw Shot State"), &persistent_bool[O("bESP_DrawPocketsShotState")]);
            Dummy(ImVec2(0, 10));
            break;
        }
        
        case 1: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable Auto Play"), &persistent_bool[O("bAutoPlay")]);
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Break Predictor"), &persistent_bool["bBreakPredictor"]);
            Dummy(ImVec2(0, 20));

            // ── Power Slider Position ─────────────────────────────────────
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Power Slider Position"));
            Dummy(ImVec2(0, 8));

            // Init defaults jika belum ada
            if (persistent_float["fPSliderX"]    == 0.f) persistent_float["fPSliderX"]    = 0.858f;
            if (persistent_float["fPSliderTop"]  == 0.f) persistent_float["fPSliderTop"]  = 0.18f;
            if (persistent_float["fPSliderH"]    == 0.f) persistent_float["fPSliderH"]    = 0.67f;

            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
            PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.86f, 0.10f, 0.18f, 1.0f));
            PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.86f, 0.10f, 0.18f, 1.0f));

            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= SliderFloat("##psx", &persistent_float["fPSliderX"], 0.70f, 1.0f, "X: %.3f");
            Dummy(ImVec2(0, 4));
            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= SliderFloat("##pst", &persistent_float["fPSliderTop"], 0.05f, 0.50f, "Top: %.3f");
            Dummy(ImVec2(0, 4));
            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= SliderFloat("##psh", &persistent_float["fPSliderH"], 0.30f, 0.90f, "Height: %.3f");

            PopStyleColor(3);
            PopStyleVar(2);

            // Reset button
            Dummy(ImVec2(0, 6));
            if (Button(O("Reset Default"), ImVec2(GetContentRegionAvail().x, 0))) {
                persistent_float["fPSliderX"]   = 0.858f;
                persistent_float["fPSliderTop"] = 0.18f;
                persistent_float["fPSliderH"]   = 0.67f;
                need_save = true;
            }

            // Preview: gambar garis slider di layar
            if (persistent_bool["bPSliderPreview"]) {
                auto& io2 = GetIO();
                float px = io2.DisplaySize.x * persistent_float["fPSliderX"];
                float pt = io2.DisplaySize.y * persistent_float["fPSliderTop"];
                float ph = io2.DisplaySize.y * persistent_float["fPSliderH"];
                ImDrawList* fgdl = GetForegroundDrawList();
                fgdl->AddLine(ImVec2(px, pt), ImVec2(px, pt + ph), IM_COL32(255, 80, 80, 200), 3.f);
                fgdl->AddCircleFilled(ImVec2(px, pt), 6.f, IM_COL32(255, 80, 80, 255));
                fgdl->AddCircleFilled(ImVec2(px, pt + ph), 6.f, IM_COL32(80, 255, 80, 255));
            }
            Dummy(ImVec2(0, 4));
            need_save |= ToggleSwitch(O("Preview Slider Position"), &persistent_bool["bPSliderPreview"]);

            Dummy(ImVec2(0, 20));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("Auto play will automatically"));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("aim and shoot for you"));

            Dummy(ImVec2(0, 10));
            break;
        }
        
        case 2: {
            Dummy(ImVec2(0, 10));
            need_save |= ToggleSwitch(O("Enable Auto Queue"), &persistent_bool[O("bAutoQueue")]);
            Dummy(ImVec2(0, 20));
            
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Mode"));
            Dummy(ImVec2(0, 8));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 12));
            PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
            SetNextItemWidth(GetContentRegionAvail().x);
            need_save |= Combo("##mode", &persistent_int["iAutoQueue_Mode"], "Last Selected\0Smart\0");
            PopStyleColor(2);
            PopStyleVar(2);
            
            if (persistent_int["iAutoQueue_Mode"] == 1) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Bet Percent"));
                Dummy(ImVec2(0, 8));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.86f, 0.10f, 0.18f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.86f, 0.10f, 0.18f, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt("##betpercent", &persistent_int["iAutoQueue_BetPercent"], 1, 100, "%d%%");
                PopStyleColor(3);
                PopStyleVar(2);
            }
            
            Dummy(ImVec2(0, 25));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("You will be auto queued to"));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("the last game mode you played"));
            Dummy(ImVec2(0, 10));
            break;
        }

        case 3: {
            // ─────────────────────────────────────────────────────
            // AC BYPASS & SERVER DETECTION
            // Real data only — no manual/fake server list.
            // ─────────────────────────────────────────────────────
            ImDrawList* svdl = GetWindowDrawList();
            float avW = GetContentRegionAvail().x;

            // ── Inline helper: bypass toggle row ─────────────────
            // Returns true if the toggle was clicked (need_save).
            auto AcToggle = [&](const char* uid, const char* label,
                                const char* sub,  const char* key,
                                ImU32 col) -> bool {
                bool val  = persistent_bool[key];
                float rH  = 50.f;
                ImVec2 rp = GetCursorScreenPos();
                // Row bg
                svdl->AddRectFilled(rp, ImVec2(rp.x+avW, rp.y+rH),
                    val ? ((col&0x00FFFFFF)|0x16000000) : IM_COL32(0,15,30,90), 5.f);
                svdl->AddRect(rp, ImVec2(rp.x+avW, rp.y+rH),
                    val ? ((col&0x00FFFFFF)|0x90000000) : IM_COL32(0,60,100,100),
                    5.f, 0, 1.f);
                // Dot
                svdl->AddCircleFilled(ImVec2(rp.x+14, rp.y+rH*0.5f), 5.f,
                    val ? col : IM_COL32(70,75,90,255), 12);
                // Text
                SetWindowFontScale(0.65f);
                svdl->AddText(ImVec2(rp.x+28, rp.y+9),
                    IM_COL32(200,230,255,220), label);
                SetWindowFontScale(0.48f);
                svdl->AddText(ImVec2(rp.x+28, rp.y+27),
                    IM_COL32(120,150,180,160), sub);
                SetWindowFontScale(1.0f);
                // Badge
                const char* bt = val ? "ON" : "OFF";
                ImU32 bfill = val ? col : IM_COL32(45,50,65,220);
                float bW=36.f, bH=18.f;
                float bx=rp.x+avW-bW-8.f, by=rp.y+(rH-bH)*0.5f;
                svdl->AddRectFilled(ImVec2(bx,by),ImVec2(bx+bW,by+bH),bfill,bH*0.5f);
                SetWindowFontScale(0.50f);
                ImVec2 bsz=CalcTextSize(bt);
                svdl->AddText(ImVec2(bx+(bW-bsz.x)*0.5f,by+(bH-bsz.y)*0.5f),
                    IM_COL32(255,255,255,230), bt);
                SetWindowFontScale(1.0f);
                char ibuf[32]; snprintf(ibuf,32,"##acbp%s",uid);
                bool clicked = InvisibleButton(ibuf, ImVec2(avW, rH));
                if (clicked) {
                    persistent_bool[key] ^= 1;
                }
                Dummy(ImVec2(0, 4.f));
                return clicked;
            };

            // ── SECTION 0: LIBPROTECT.SO MEMORY PATCHES ──────────
            AnticheatBypass::DrawBypassSection(svdl, avW);

            // ── SECTION 1: BYPASS SHIELDS ─────────────────────────
            Dummy(ImVec2(0, 8.f));
            SetWindowFontScale(0.60f);
            TextColored(ImVec4(1,1,1,0.28f), O("// AC BYPASS SHIELDS"));
            SetWindowFontScale(1.0f);
            Dummy(ImVec2(0, 5.f));

            need_save |= AcToggle("root",
                "Root Detection Hide",
                "access/stat hook — hides su, magisk, frida dari scan AC",
                "bBypass_Root",  IM_COL32(240, 60, 70,255));
            need_save |= AcToggle("maps",
                "Memory Map Scrub",
                "fopen hook — hapus baris lib kita dari /proc/self/maps",
                "bBypass_Maps",  IM_COL32(200, 20, 40,255));
            need_save |= AcToggle("props",
                "Build Props Spoof",
                "property_get hook — spoof ro.debuggable, ro.build.tags",
                "bBypass_Props", IM_COL32(255, 80, 90,255));

            // ── SECTION 2: AC SCAN ────────────────────────────────
            Dummy(ImVec2(0, 10.f));
            SetWindowFontScale(0.60f);
            TextColored(ImVec4(1,1,1,0.28f), O("// SCAN ANTI-CHEAT SYSTEMS"));
            SetWindowFontScale(1.0f);
            Dummy(ImVec2(0, 5.f));

            // Scan button
            {
                float bH2 = 34.f;
                ImVec2 bsp = GetCursorScreenPos();
                svdl->AddRectFilled(bsp, ImVec2(bsp.x+avW, bsp.y+bH2),
                    IM_COL32(0,55,105,200), 6.f);
                svdl->AddRect(bsp, ImVec2(bsp.x+avW, bsp.y+bH2),
                    IM_COL32(220, 30, 50,160), 6.f, 0, 1.5f);
                const char* lbl = g_acScan.running ? O("SCANNING...")
                    : (g_acScan.done ? O("RE-SCAN PROCESS") : O("SCAN PROCESS NOW"));
                ImU32 lblCol = g_acScan.running
                    ? IM_COL32(255,200,50,200) : IM_COL32(0,220,255,230);
                SetWindowFontScale(0.60f);
                ImVec2 lsz = CalcTextSize(lbl);
                svdl->AddText(
                    ImVec2(bsp.x+(avW-lsz.x)*0.5f, bsp.y+(bH2-lsz.y)*0.5f),
                    lblCol, lbl);
                SetWindowFontScale(1.0f);
                if (InvisibleButton("##acscan", ImVec2(avW, bH2)) && !g_acScan.running)
                    LaunchACScan();
                Dummy(ImVec2(0, 6.f));
            }

            if (g_acScan.done) {
                // AC libs
                SetWindowFontScale(0.54f);
                if (g_acScan.acLibs.empty()) {
                    TextColored(ImVec4(0.25f,0.85f,0.5f,0.85f),
                        O("  OK  No AC libraries detected in process memory"));
                } else {
                    char hdr[64];
                    snprintf(hdr,64,"  !!  %d AC library(s) found in memory:",
                             (int)g_acScan.acLibs.size());
                    TextColored(ImVec4(1.f,0.38f,0.38f,0.9f), "%s", hdr);
                }
                SetWindowFontScale(1.0f);
                Dummy(ImVec2(0, 2.f));
                for (auto& lib : g_acScan.acLibs) {
                    ImVec2 lrp = GetCursorScreenPos();
                    float lhh = 20.f;
                    svdl->AddRectFilled(lrp,ImVec2(lrp.x+avW,lrp.y+lhh),
                        IM_COL32(80,10,10,110),3.f);
                    SetWindowFontScale(0.46f);
                    svdl->AddText(ImVec2(lrp.x+6, lrp.y+(lhh-8.f)*0.5f),
                        IM_COL32(255,120,120,200), lib.c_str());
                    SetWindowFontScale(1.0f);
                    Dummy(ImVec2(avW,lhh)); Dummy(ImVec2(0,2.f));
                }

                // Unknown / unclassified hosts
                Dummy(ImVec2(0, 5.f));
                SetWindowFontScale(0.54f);
                if (g_acScan.newHosts.empty()) {
                    TextColored(ImVec4(0.25f,0.85f,0.5f,0.85f),
                        O("  OK  No unclassified hosts in DNS log"));
                } else {
                    char hdr2[80];
                    snprintf(hdr2,80,"  ??  %d unclassified host(s) — potential AC servers:",
                             (int)g_acScan.newHosts.size());
                    TextColored(ImVec4(1.f,0.75f,0.2f,0.9f), "%s", hdr2);
                }
                SetWindowFontScale(1.0f);
                Dummy(ImVec2(0,2.f));
                for (int ni=0; ni<(int)g_acScan.newHosts.size() && ni<8; ni++) {
                    const auto& nh = g_acScan.newHosts[ni];
                    float nrH=26.f;
                    ImVec2 nrp=GetCursorScreenPos();
                    svdl->AddRectFilled(nrp,ImVec2(nrp.x+avW,nrp.y+nrH),
                        IM_COL32(30,20,0,100),3.f);
                    svdl->AddRect(nrp,ImVec2(nrp.x+avW,nrp.y+nrH),
                        IM_COL32(251,191,36,80),3.f,0,1.f);
                    SetWindowFontScale(0.46f);
                    svdl->AddText(ImVec2(nrp.x+5,nrp.y+(nrH-7.f)*0.5f),
                        IM_COL32(251,191,36,220),"??");
                    svdl->AddText(ImVec2(nrp.x+22,nrp.y+(nrH-7.f)*0.5f),
                        IM_COL32(200,215,235,200), nh.c_str());
                    SetWindowFontScale(1.0f);
                    Dummy(ImVec2(avW,nrH)); Dummy(ImVec2(0,2.f));
                }

                // Scan timestamp
                Dummy(ImVec2(0, 3.f));
                SetWindowFontScale(0.44f);
                long ago=(long)time(nullptr)-g_acScan.scanTime;
                char tsbuf[40];
                if (ago<5)        snprintf(tsbuf,40,"Scanned just now");
                else if (ago<60)  snprintf(tsbuf,40,"Scanned %lds ago",ago);
                else              snprintf(tsbuf,40,"Scanned %ldm ago",ago/60);
                TextColored(ImVec4(1,1,1,0.22f),"%s",tsbuf);
                SetWindowFontScale(1.0f);
            }

            // ── SECTION 3: LIVE DETECTED SERVERS (real DNS log) ───
            Dummy(ImVec2(0, 12.f));
            SetWindowFontScale(0.60f);
            TextColored(ImVec4(1,1,1,0.28f), O("// LIVE DNS — DETECTED SERVERS"));
            SetWindowFontScale(1.0f);
            Dummy(ImVec2(0, 5.f));
            {
                struct LiveHost {
                    char host[128];
                    char tag[16];
                    int  blkIdx;
                    int  count;
                };
                static LiveHost s_lh[24];
                static int      s_lhCount  = 0;
                static float    s_lhTimer  = 0.f;
                s_lhTimer += io.DeltaTime;
                if (s_lhTimer >= 0.5f) {
                    s_lhTimer  = 0.f;
                    s_lhCount  = 0;
                    pthread_mutex_lock(&g_netMux);
                    for (int i = 0; i < g_netLogCount; i++) {
                        int idx=(g_netLogHead-1-i+NET_LOG_MAX)%NET_LOG_MAX;
                        const NetRequest& r=g_netLog[idx];
                        if (strcmp(r.tag,"8BP")==0||strcmp(r.tag,"MCLIP")==0) continue;
                        if (strcmp(r.tag,"BLOCKED")==0) continue;
                        bool found=false;
                        for (int j=0;j<s_lhCount;j++) {
                            if (strcmp(s_lh[j].host,r.host)==0) {
                                s_lh[j].count++; found=true; break;
                            }
                        }
                        if (!found && s_lhCount<24) {
                            strncpy(s_lh[s_lhCount].host,r.host,127);
                            strncpy(s_lh[s_lhCount].tag, r.tag, 15);
                            s_lh[s_lhCount].count=1;
                            int bidx=-1;
                            for (int p=0;p<SRV_PATTERNS_COUNT;p++) {
                                if (strcasestr(r.host,SRV_PATTERNS[p].match)) {
                                    bidx=SRV_PATTERNS[p].blkIdx; break;
                                }
                            }
                            s_lh[s_lhCount].blkIdx=bidx;
                            s_lhCount++;
                        }
                    }
                    pthread_mutex_unlock(&g_netMux);
                }

                if (s_lhCount==0) {
                    SetWindowFontScale(0.54f);
                    TextColored(ImVec4(1,1,1,0.22f),
                        O("Waiting for game network traffic..."));
                    SetWindowFontScale(1.0f);
                }
                for (int si=0;si<s_lhCount;si++) {
                    auto& lh=s_lh[si];
                    bool blocked=(lh.blkIdx>=0)
                        && persistent_bool[BLK_KEYS[lh.blkIdx]];
                    float rH=44.f;
                    ImVec2 rp=GetCursorScreenPos();
                    svdl->AddRectFilled(rp,ImVec2(rp.x+avW,rp.y+rH),
                        blocked?IM_COL32(80,10,10,110):IM_COL32(0,18,38,100),5.f);
                    svdl->AddRect(rp,ImVec2(rp.x+avW,rp.y+rH),
                        blocked?IM_COL32(220,50,50,150):IM_COL32(80, 8, 14, 120),
                        5.f,0,1.f);
                    svdl->AddCircleFilled(ImVec2(rp.x+12,rp.y+rH*0.5f),4.f,
                        blocked?IM_COL32(255,80,80,255):IM_COL32(240, 60, 70,255),12);
                    // Tag
                    SetWindowFontScale(0.44f);
                    ImVec2 tgsz=CalcTextSize(lh.tag);
                    float tbx=rp.x+22,tby=rp.y+7;
                    svdl->AddRectFilled(ImVec2(tbx,tby),
                        ImVec2(tbx+tgsz.x+6,tby+tgsz.y+2),
                        IM_COL32(0,80,140,60),2.f);
                    svdl->AddText(ImVec2(tbx+3,tby+1),
                        IM_COL32(96,165,250,220),lh.tag);
                    SetWindowFontScale(1.0f);
                    // Host
                    SetWindowFontScale(0.50f);
                    svdl->AddText(ImVec2(rp.x+22,rp.y+22),
                        blocked?IM_COL32(255,120,120,200):IM_COL32(180,220,255,200),
                        lh.host);
                    SetWindowFontScale(1.0f);
                    // Request count
                    char cntbuf[12]; snprintf(cntbuf,12,"x%d",lh.count);
                    SetWindowFontScale(0.42f);
                    ImVec2 csz=CalcTextSize(cntbuf);
                    svdl->AddText(ImVec2(rp.x+avW-csz.x-55.f,rp.y+7),
                        IM_COL32(255,255,255,30),cntbuf);
                    SetWindowFontScale(1.0f);
                    // Block button (only if known category)
                    if (lh.blkIdx>=0) {
                        const char* bt2=blocked?"UNBLOCK":"BLOCK";
                        ImU32 bf=blocked?IM_COL32(180,40,40,200):IM_COL32(0,50,95,200);
                        float bW=58.f,bH=20.f;
                        float bx2=rp.x+avW-bW-4.f,by2=rp.y+(rH-bH)*0.5f;
                        svdl->AddRectFilled(ImVec2(bx2,by2),ImVec2(bx2+bW,by2+bH),bf,4.f);
                        svdl->AddRect(ImVec2(bx2,by2),ImVec2(bx2+bW,by2+bH),
                            blocked?IM_COL32(255,100,100,200):IM_COL32(220, 30, 50,150),
                            4.f,0,1.f);
                        SetWindowFontScale(0.44f);
                        ImVec2 btsz2=CalcTextSize(bt2);
                        svdl->AddText(
                            ImVec2(bx2+(bW-btsz2.x)*0.5f,by2+(bH-btsz2.y)*0.5f),
                            IM_COL32(255,255,255,230),bt2);
                        SetWindowFontScale(1.0f);
                    }
                    char hbuf[24]; snprintf(hbuf,24,"##lsrv%d",si);
                    if (InvisibleButton(hbuf,ImVec2(avW,rH)) && lh.blkIdx>=0) {
                        persistent_bool[BLK_KEYS[lh.blkIdx]]^=1;
                        SyncBlockFlags();
                        need_save=true;
                    }
                    Dummy(ImVec2(0,3.f));
                }
            }

            // ── SECTION 4: AC PROBE EVENT LOG ────────────────────
            Dummy(ImVec2(0, 12.f));
            SetWindowFontScale(0.60f);
            TextColored(ImVec4(1,1,1,0.28f), O("// AC PROBE EVENTS"));
            SetWindowFontScale(1.0f);
            Dummy(ImVec2(0, 4.f));
            {
                static ACEvent evSnap[AC_EVT_MAX];
                static int evC=0, evH=0;
                static float evT=0.f;
                evT+=io.DeltaTime;
                if (evT>=0.3f) {
                    evT=0.f;
                    pthread_mutex_lock(&g_acEvtMux);
                    evC=g_acEvtCount; evH=g_acEvtHead;
                    memcpy(evSnap,g_acEvt,sizeof(g_acEvt));
                    pthread_mutex_unlock(&g_acEvtMux);
                }
                if (evC==0) {
                    SetWindowFontScale(0.52f);
                    TextColored(ImVec4(1,1,1,0.22f),
                        O("No AC probe attempts detected yet"));
                    SetWindowFontScale(1.0f);
                }
                for (int ei=0;ei<evC&&ei<12;ei++) {
                    int idx=(evH-1-ei+AC_EVT_MAX)%AC_EVT_MAX;
                    const ACEvent& ev=evSnap[idx];
                    float eH=36.f;
                    ImVec2 erp=GetCursorScreenPos();
                    svdl->AddRectFilled(erp,ImVec2(erp.x+avW,erp.y+eH),
                        ev.bypassed?IM_COL32(0,40,10,100):IM_COL32(80,0,0,100),3.f);
                    // what badge
                    SetWindowFontScale(0.44f);
                    ImVec2 wsz=CalcTextSize(ev.what);
                    ImU32 wc=ev.bypassed?IM_COL32(240, 60, 70,255):IM_COL32(255,80,80,255);
                    svdl->AddRectFilled(ImVec2(erp.x+4,erp.y+5),
                        ImVec2(erp.x+4+wsz.x+6,erp.y+5+wsz.y+2),
                        (wc&0x00FFFFFF)|0x28000000,2.f);
                    svdl->AddText(ImVec2(erp.x+7,erp.y+6),wc,ev.what);
                    // detail
                    svdl->AddText(ImVec2(erp.x+4,erp.y+20),
                        IM_COL32(150,180,215,155),ev.detail);
                    // status
                    const char* st=ev.bypassed?"BYPASSED":"DETECTED";
                    ImVec2 stsz=CalcTextSize(st);
                    svdl->AddText(
                        ImVec2(erp.x+avW-stsz.x-4,erp.y+(eH-stsz.y)*0.5f),
                        ev.bypassed?IM_COL32(52,211,153,180):IM_COL32(255,80,80,180),
                        st);
                    SetWindowFontScale(1.0f);
                    Dummy(ImVec2(avW,eH)); Dummy(ImVec2(0,2.f));
                }
            }

            Dummy(ImVec2(0, 10.f));
            break;
        }

        case 4: {
            // ── MEM DUMP TAB ──────────────────────────────────────
            ImDrawList* dpdl = GetWindowDrawList();
            float dpW = GetContentRegionAvail().x;

            Dummy(ImVec2(0, 10.f));
            SetWindowFontScale(0.60f);
            TextColored(ImVec4(1,1,1,0.28f), "// MEMORY DUMP");
            SetWindowFontScale(1.0f);
            Dummy(ImVec2(0, 6.f));

            // Status row
            {
                float sH = 38.f;
                ImVec2 sp = GetCursorScreenPos();
                bool running = g_dump_running.load();
                bool done    = g_dump_done.load();
                ImU32 sbg = running ? IM_COL32(0,40,80,180) :
                            done    ? IM_COL32(0,50,20,180) :
                                      IM_COL32(10,10,25,150);
                ImU32 sbd = running ? IM_COL32(96,165,250,200) :
                            done    ? IM_COL32(52,211,153,200) :
                                      IM_COL32(80, 8, 14, 120);
                dpdl->AddRectFilled(sp, ImVec2(sp.x+dpW, sp.y+sH), sbg, 6.f);
                dpdl->AddRect(sp, ImVec2(sp.x+dpW, sp.y+sH), sbd, 6.f, 0, 1.5f);
                dpdl->AddCircleFilled(ImVec2(sp.x+14, sp.y+sH*0.5f), 5.f, sbd, 12);
                SetWindowFontScale(0.58f);
                dpdl->AddText(ImVec2(sp.x+27, sp.y+(sH-9.f)*0.5f),
                    IM_COL32(200,230,255,220), g_dump_status);
                SetWindowFontScale(1.0f);
                Dummy(ImVec2(dpW, sH));
            }
            Dummy(ImVec2(0, 6.f));

            // START DUMP button
            {
                float bH = 36.f;
                ImVec2 bp = GetCursorScreenPos();
                bool running = g_dump_running.load();
                ImU32 bbg = running ? IM_COL32(40, 5, 8, 180) : IM_COL32(100, 12, 20, 220);
                ImU32 bbd = running ? IM_COL32(60,80,140,140) : IM_COL32(220, 30, 50,200);
                dpdl->AddRectFilled(bp, ImVec2(bp.x+dpW, bp.y+bH), bbg, 7.f);
                dpdl->AddRect(bp, ImVec2(bp.x+dpW, bp.y+bH), bbd, 7.f, 0, 1.8f);
                const char* blbl = running ? "DUMPING..." : "START DUMP";
                SetWindowFontScale(0.65f);
                ImVec2 bsz = CalcTextSize(blbl);
                dpdl->AddText(ImVec2(bp.x+(dpW-bsz.x)*0.5f, bp.y+(bH-bsz.y)*0.5f),
                    running ? IM_COL32(120,150,200,160) : IM_COL32(0,220,255,240), blbl);
                SetWindowFontScale(1.0f);
                if (InvisibleButton("##dmpStart", ImVec2(dpW, bH)) && !running)
                    TriggerDump();
            }
            Dummy(ImVec2(0, 6.f));

            // CLEAR button — reset dump state
            {
                float bH = 32.f;
                ImVec2 bp = GetCursorScreenPos();
                bool hasDump = g_dump_done.load();
                ImU32 cbg = hasDump ? IM_COL32(50,10,10,180)  : IM_COL32(15,15,30,120);
                ImU32 cbd = hasDump ? IM_COL32(200,60,60,160) : IM_COL32(50,60,90,80);
                dpdl->AddRectFilled(bp, ImVec2(bp.x+dpW, bp.y+bH), cbg, 6.f);
                dpdl->AddRect(bp, ImVec2(bp.x+dpW, bp.y+bH), cbd, 6.f, 0, 1.2f);
                const char* clbl = "CLEAR RESULT";
                SetWindowFontScale(0.58f);
                ImVec2 csz = CalcTextSize(clbl);
                dpdl->AddText(ImVec2(bp.x+(dpW-csz.x)*0.5f, bp.y+(bH-csz.y)*0.5f),
                    hasDump ? IM_COL32(255,120,120,220) : IM_COL32(80, 8, 14, 120), clbl);
                SetWindowFontScale(1.0f);
                if (InvisibleButton("##dmpClear", ImVec2(dpW, bH)) && hasDump) {
                    std::lock_guard<std::mutex> lk(g_dump_mutex);
                    g_dump_result.clear();
                    g_dump_done.store(false);
                    snprintf(g_dump_status, sizeof(g_dump_status), "Ready");
                }
            }
            Dummy(ImVec2(0, 8.f));

            // Dump result preview — mutex-guarded local copy
            if (g_dump_done.load()) {
                std::string snapshot;
                { std::lock_guard<std::mutex> lk(g_dump_mutex); snapshot = g_dump_result; }
                if (!snapshot.empty()) {
                    SetWindowFontScale(0.60f);
                    TextColored(ImVec4(1,1,1,0.28f), "// LAST DUMP PREVIEW");
                    SetWindowFontScale(1.0f);
                    Dummy(ImVec2(0, 4.f));

                    // Collect line pointers into snapshot, show last 8
                    std::vector<size_t> lineOffs;
                    lineOffs.push_back(0);
                    for (size_t i = 0; i < snapshot.size() && lineOffs.size() < 201; i++)
                        if (snapshot[i] == '\n' && i+1 < snapshot.size())
                            lineOffs.push_back(i + 1);
                    int start = (int)lineOffs.size() > 8 ? (int)lineOffs.size() - 8 : 0;
                    for (int li = start; li < (int)lineOffs.size(); li++) {
                        size_t beg = lineOffs[li];
                        size_t end = (li+1 < (int)lineOffs.size()) ? lineOffs[li+1]-1 : snapshot.size();
                        size_t len = (end > beg) ? (end - beg) : 0;
                        if (len > 120) len = 120;
                        char lb[128] = {};
                        memcpy(lb, snapshot.c_str() + beg, len);
                        SetWindowFontScale(0.50f);
                        TextColored(ImVec4(0.6f, 0.8f, 1.0f, 0.75f), "  %s", lb);
                        SetWindowFontScale(1.0f);
                    }
                }
            }

            Dummy(ImVec2(0, 10.f));
            break;
        }
    }
    
    if (need_save) save_persistence();
    
    EndChild();
    PopStyleVar();
    PopStyleColor(5);
}

// ════════════════════════════════════════════════════════════════════
//  SETTINGS PANEL  — separate window, NO overlap with main menu
// ════════════════════════════════════════════════════════════════════
INLINE void DrawSettingsPanel(ImGuiIO& io, ImVec2 mainPos, float mainW, float mainH) {
    const float SW  = 310.f;
    const float SH  = mainH;
    const float PAD = 14.f;

    // ── Theme-aware palette ───────────────────────────────────────
    ImU32 CR[5], CR_MID[5];
    FillThemePalette(CR, nullptr, CR_MID);
    const ImU32 SEP = IM_COL32(255,255,255,18);

    // ── Default-init persistent settings once ────────────────────
    // Keys use plain string literals to match persistence.h map keys exactly.
    static bool s_sinit = false;
    if (!s_sinit) { s_sinit = true;
        if (persistent_float["fLineThick"]   < 0.05f) persistent_float["fLineThick"]   = 2.0f;
        if (persistent_float["fPredAlpha"]   < 0.05f) persistent_float["fPredAlpha"]   = 0.8f;
        if (persistent_float["fPocketRad"]   < 0.05f) persistent_float["fPocketRad"]   = 1.0f;
        if (persistent_float["fPocketAlpha"] < 0.05f) persistent_float["fPocketAlpha"] = 0.7f;
        if (persistent_float["fMenuOpacity"] < 0.70f) persistent_float["fMenuOpacity"] = 1.0f;
        if (persistent_float["fTheme"]       < 0.f  ) persistent_float["fTheme"]       = 0.0f;
        if (persistent_float["fSolidHue"]    < 0.f  ) persistent_float["fSolidHue"]    = 0.0f;
    }

    // ── Position: RIGHT of main menu; fallback LEFT ───────────────
    float sx = mainPos.x + mainW + 10.f;
    if (sx + SW > (float)Width)  sx = mainPos.x - SW - 10.f;
    if (sx < 0.f)                sx = mainPos.x + 4.f;
    float sy = mainPos.y;

    float alpha = g_menu.menuAlpha * persistent_float["fMenuOpacity"];
    // Alpha helper — apply opacity to raw DrawList colors
    auto ApA = [alpha](ImU32 c) -> ImU32 {
        if (alpha >= 0.999f) return c;
        ImU32 a = ImU32(float((c >> 24) & 0xFF) * alpha);
        return (c & 0x00FFFFFFu) | (a << 24);
    };

    using namespace ImGui;
    SetNextWindowPos(ImVec2(sx, sy), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(SW, SH), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.02f, 0.03f, alpha));
    PushStyleVar(ImGuiStyleVar_WindowRounding,   5.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    PushStyleVar(ImGuiStyleVar_Alpha,            alpha);

    if (Begin(O("##SettingsWin"), nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoResize)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 wp = GetWindowPos();

        // ── Rainbow top bar ───────────────────────────────────────
        float sw5 = SW / 5.f;
        for (int i = 0; i < 5; i++)
            dl->AddRectFilledMultiColor(
                ImVec2(wp.x+i*sw5, wp.y), ImVec2(wp.x+(i+1)*sw5, wp.y+3.f),
                ApA(CR[i]), ApA(CR[(i+1)%5]), ApA(CR[(i+1)%5]), ApA(CR[i]));

        // ── Rainbow outer border ──────────────────────────────────
        float sh5 = SH / 5.f, th = 1.5f;
        for (int i = 0; i < 5; i++) {
            dl->AddLine(ImVec2(wp.x+i*sw5,   wp.y+1),    ImVec2(wp.x+(i+1)*sw5,wp.y+1),    ApA(CR_MID[i]),       th);
            dl->AddLine(ImVec2(wp.x+i*sw5,   wp.y+SH-1), ImVec2(wp.x+(i+1)*sw5,wp.y+SH-1), ApA(CR_MID[(i+2)%5]), th);
            dl->AddLine(ImVec2(wp.x+1,       wp.y+i*sh5), ImVec2(wp.x+1,       wp.y+(i+1)*sh5), ApA(CR_MID[(i+1)%5]), th);
            dl->AddLine(ImVec2(wp.x+SW-1,    wp.y+i*sh5), ImVec2(wp.x+SW-1,    wp.y+(i+1)*sh5), ApA(CR_MID[(i+3)%5]), th);
        }

        // ── Header ────────────────────────────────────────────────
        const float HDR_H = 64.f;
        // Header gradient bg
        dl->AddRectFilledMultiColor(
            ImVec2(wp.x, wp.y+3.f), ImVec2(wp.x+SW, wp.y+HDR_H),
            (CR[0] & 0x00FFFFFFu) | 0x1A000000u, IM_COL32(0,0,0,0),
            IM_COL32(0,0,0,0), (CR[0] & 0x00FFFFFFu) | 0x1A000000u);
        // Icon dot
        dl->AddCircleFilled(ImVec2(wp.x+PAD+8.f, wp.y+23.f), 7.5f, (CR[0] & 0x00FFFFFFu) | 0x28000000u, 14);
        dl->AddCircleFilled(ImVec2(wp.x+PAD+8.f, wp.y+23.f), 4.f, ApA(CR[0]), 12);
        // Title with glow
        SetWindowFontScale(0.96f);
        dl->AddText(ImVec2(wp.x+PAD+24.f, wp.y+11.f), ApA((CR[0] & 0x00FFFFFFu) | 0x40000000u), O("SETTINGS"));
        dl->AddText(ImVec2(wp.x+PAD+24.f, wp.y+11.f), ApA(CR[0]), O("SETTINGS"));
        SetWindowFontScale(0.50f);
        dl->AddText(ImVec2(wp.x+PAD+24.f, wp.y+38.f), ApA(IM_COL32(200,180,185,85)), O("CONFIGURATION"));
        SetWindowFontScale(1.0f);

        // ── Close (X) button top-right — perfectly centred in HDR ──
        const float xBtnSz = 28.f;
        const float xBtnY = (HDR_H - xBtnSz) * 0.5f;
        if (DrawCloseButton(O("##xset"), ImVec2(SW - xBtnSz - 10.f, xBtnY), xBtnSz))
            g_menu.showSettings = false;

        dl->AddLine(ImVec2(wp.x,wp.y+HDR_H), ImVec2(wp.x+SW,wp.y+HDR_H), SEP, 1.f);

        // ════════════════════════════════════════════════════
        // SCROLLABLE SETTINGS CONTENT
        // ════════════════════════════════════════════════════
        SetCursorPos(ImVec2(0, HDR_H));
        PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0,0,0,0));
        PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0,0,0,0));
        PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(0.86f,0.10f,0.18f,0.55f));
        PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f,0.15f,0.22f,0.75f));
        PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImVec4(0.75f,0.08f,0.15f,0.95f));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PAD,0));
        PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
        PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
        PushStyleVar(ImGuiStyleVar_GrabRounding,  4.f);

        if (BeginChild(O("##scfg"), ImVec2(SW, SH - HDR_H - 8.f), false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

            bool needSave = false;
            ImDrawList* cdl = GetWindowDrawList();

            // ── Helpers ──────────────────────────────────────────
            auto SecLabel = [&](const char* lbl) {
                Dummy(ImVec2(0, 10.f));
                float slAvailW = GetContentRegionAvail().x - 2.f;
                ImVec2 slSp = GetCursorScreenPos();
                SetWindowFontScale(0.56f);
                ImVec2 slSz = CalcTextSize(lbl);
                const float slH = slSz.y + 18.f;
                // Pill background
                cdl->AddRectFilled(slSp, ImVec2(slSp.x+slAvailW, slSp.y+slH), IM_COL32(0,14,32,200), 5.f);
                // Left accent bar — dual-color gradient
                cdl->AddRectFilledMultiColor(slSp, ImVec2(slSp.x+3.5f, slSp.y+slH),
                    CR[0], CR[0], (CR[2]&0x00FFFFFFu)|0x40000000u, (CR[2]&0x00FFFFFFu)|0x40000000u);
                // Top accent line
                cdl->AddRectFilledMultiColor(
                    ImVec2(slSp.x+5.f, slSp.y), ImVec2(slSp.x+slAvailW-5.f, slSp.y+1.5f),
                    IM_COL32(0,0,0,0), (CR[0]&0x00FFFFFFu)|0x90000000u,
                    (CR[2]&0x00FFFFFFu)|0x90000000u, IM_COL32(0,0,0,0));
                // Right fade
                cdl->AddRectFilledMultiColor(
                    ImVec2(slSp.x+slAvailW-36.f, slSp.y), ImVec2(slSp.x+slAvailW, slSp.y+slH),
                    IM_COL32(0,0,0,0), (CR[1] & 0x00FFFFFFu) | 0x18000000u,
                    (CR[1] & 0x00FFFFFFu) | 0x18000000u, IM_COL32(0,0,0,0));
                // Dot icon
                cdl->AddCircleFilled(ImVec2(slSp.x+10.f, slSp.y+slH*0.5f), 3.f, CR[0], 10);
                // Label text
                cdl->AddText(ImVec2(slSp.x+18.f, slSp.y+(slH-slSz.y)*0.5f),
                    (CR[0] & 0x00FFFFFFu) | 0xC0000000u, lbl);
                SetWindowFontScale(1.0f);
                InvisibleButton("##sl_dummy", ImVec2(slAvailW, slH));
                Dummy(ImVec2(0, 4.f));
            };

            // Slider with rainbow grab colour
            auto Slider = [&](const char* id, const char* label, float* v,
                               float vmin, float vmax, const char* fmt, int ci) -> bool {
                SetWindowFontScale(0.70f);
                TextColored(ImVec4(1,1,1,0.70f), "%s", label);
                SetWindowFontScale(1.0f);
                SetNextItemWidth(GetContentRegionAvail().x - 2.f);
                PushStyleColor(ImGuiCol_SliderGrab,       (ImVec4)ImColor(CR[ci%5]));
                PushStyleColor(ImGuiCol_SliderGrabActive, (ImVec4)ImColor(CR[(ci+1)%5]));
                PushStyleColor(ImGuiCol_FrameBg,          ImVec4(1,1,1,0.17f));
                PushStyleColor(ImGuiCol_FrameBgHovered,   ImVec4(1,1,1,0.13f));
                PushStyleColor(ImGuiCol_FrameBgActive,    ImVec4(1,1,1,0.20f));
                bool ch = SliderFloat(id, v, vmin, vmax, fmt);
                PopStyleColor(5);
                Dummy(ImVec2(0, 5.f));
                return ch;
            };

            // Full-row toggle with custom pill
            auto Toggle = [&](const char* bid, const char* label,
                               const char* key, int ci) -> bool {
                float rowH = 26.f;
                float availW = GetContentRegionAvail().x - 2.f;
                float togW = 42.f, togH = 20.f;

                SetCursorPos(ImVec2(GetCursorPos().x, GetCursorPos().y + 1.f));
                ImVec2 rsp = GetCursorScreenPos();
                bool val = persistent_bool[key];

                // label text
                SetWindowFontScale(0.70f);
                cdl->AddText(ImVec2(rsp.x, rsp.y+(rowH-13.f)*0.5f),
                    IM_COL32(255,255,255,180), label);
                SetWindowFontScale(1.0f);

                // pill toggle on right
                float tx = rsp.x + availW - togW;
                float ty = rsp.y + (rowH - togH)*0.5f;
                ImU32 fillC = val ? CR[ci%5] : IM_COL32(60,60,80,255);
                cdl->AddRectFilled(ImVec2(tx,ty), ImVec2(tx+togW,ty+togH), fillC, togH*0.5f);
                float kx   = val ? (tx+togW-togH*0.5f) : (tx+togH*0.5f);
                cdl->AddCircleFilled(ImVec2(kx, ty+togH*0.5f), togH*0.5f-2.f, IM_COL32(255,255,255,230), 14);
                // ON/OFF text inside pill
                const char* bt = val ? "ON" : "OFF";
                SetWindowFontScale(0.48f);
                ImVec2 btsz = CalcTextSize(bt);
                float btx = val ? (tx+3.f) : (tx+togH*0.5f+2.f);
                cdl->AddText(ImVec2(btx, ty+(togH-btsz.y)*0.5f), IM_COL32(255,255,255,200), bt);
                SetWindowFontScale(1.0f);

                char ibid[32]; snprintf(ibid, 32, "##tk%s", bid);
                bool clicked = InvisibleButton(ibid, ImVec2(availW, rowH));
                if (clicked) { persistent_bool[key] ^= 1; }
                Dummy(ImVec2(0, 2.f));
                return clicked;
            };

            // ── // LINES ─────────────────────────────────────────
            SecLabel("// LINES");
            needSave |= Slider("##slt", "Line Thickness",
                &persistent_float["fLineThick"], 0.5f, 8.0f, "%.1f px", 0);
            needSave |= Slider("##sla", "Line Opacity",
                &persistent_float["fPredAlpha"], 0.05f, 1.0f, "%.2f", 1);

            PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,0.15f));
            Separator(); PopStyleColor();

            // ── // POCKETS ────────────────────────────────────────
            SecLabel("// POCKETS");
            needSave |= Slider("##spr", "Pocket Radius",
                &persistent_float["fPocketRad"], 0.5f, 4.0f, "%.2fx", 2);
            needSave |= Slider("##spa", "Pocket Opacity",
                &persistent_float["fPocketAlpha"], 0.05f, 1.0f, "%.2f", 3);

            PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,0.15f));
            Separator(); PopStyleColor();

            // ── // MENU ───────────────────────────────────────────
            SecLabel("// MENU");
            // Opacity stored as 0.70-1.00, displayed as 0.00-0.30
            {
                float dispOp = persistent_float["fMenuOpacity"] - 0.70f;
                if (dispOp < 0.f) dispOp = 0.f;
                if (Slider("##smo", "Menu Opacity", &dispOp, 0.0f, 0.30f, "%.2f", 4)) {
                    persistent_float["fMenuOpacity"] = ImClamp(dispOp + 0.70f, 0.70f, 1.0f);
                    needSave = true;
                }
            }

            PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,0.15f));
            Separator(); PopStyleColor();

            // ── // THEME ──────────────────────────────────────────
            SecLabel("// THEME");
            {
                float availW = GetContentRegionAvail().x - 2.f;
                bool solidActive = persistent_bool["bSolidColor"];

                if (solidActive) {
                    // Auto-minimize: gradient hidden when Solid Color is on
                    ImVec2 rsp = GetCursorScreenPos();
                    const float pillH = 38.f;
                    cdl->AddRectFilled(rsp, ImVec2(rsp.x+availW, rsp.y+pillH),
                        IM_COL32(0,12,28,130), 6.f);
                    cdl->AddRect(rsp, ImVec2(rsp.x+availW, rsp.y+pillH),
                        IM_COL32(255,255,255,16), 6.f, 0, 1.f);
                    cdl->AddRectFilled(rsp, ImVec2(rsp.x+3.f, rsp.y+pillH),
                        IM_COL32(180,180,180,55), 2.f);
                    SetWindowFontScale(0.50f);
                    const char* msg1 = O("Gradient Theme");
                    const char* msg2 = O("(minimized — Solid Color aktif)");
                    ImVec2 m1sz = CalcTextSize(msg1);
                    float centerY = rsp.y + pillH * 0.5f;
                    cdl->AddText(ImVec2(rsp.x+12.f, centerY - m1sz.y - 1.f),
                        IM_COL32(200,220,240,100), msg1);
                    SetWindowFontScale(0.42f);
                    cdl->AddText(ImVec2(rsp.x+12.f, centerY + 1.f),
                        IM_COL32(160,180,210,60), msg2);
                    SetWindowFontScale(1.0f);
                    InvisibleButton("##thcollapsed", ImVec2(availW, pillH));
                    Dummy(ImVec2(0, 6.f));
                } else {
                    // Full 2-column gradient card grid
                    int curTheme = (int)persistent_float["fTheme"];
                    if (curTheme < 0 || curTheme >= THEME_COUNT) curTheme = 0;

                    const float GAP   = 6.f;
                    const float cardW = (availW - GAP) * 0.5f;
                    const float barH  = 28.f;
                    const float lblH  = 16.f;
                    const float cardH = barH + lblH + 4.f;

                    float gridStartY = GetCursorPos().y;
                    for (int i = 0; i < THEME_COUNT; i++) {
                        int col = i % 2;
                        int row = i / 2;
                        float cx = (col == 0) ? 0.f : cardW + GAP;
                        float cy = gridStartY + row * (cardH + GAP);
                        SetCursorPos(ImVec2(cx, cy));
                        ImVec2 sp = GetCursorScreenPos();

                        bool selected = (curTheme == i);

                        cdl->AddRectFilled(sp, ImVec2(sp.x+cardW, sp.y+cardH),
                            IM_COL32(0,14,30,180), 6.f);

                        float segW = cardW / 4.f;
                        for (int s = 0; s < 4; s++) {
                            float x0 = sp.x + s * segW;
                            float x1 = sp.x + (s+1) * segW;
                            cdl->AddRectFilledMultiColor(
                                ImVec2(x0, sp.y+2), ImVec2(x1, sp.y+2+barH),
                                THEME_GRAD_BASE[i][s],   THEME_GRAD_BASE[i][s+1],
                                THEME_GRAD_BASE[i][s+1], THEME_GRAD_BASE[i][s]);
                        }

                        SetWindowFontScale(0.44f);
                        ImVec2 tsz = CalcTextSize(THEME_GRAD_NAMES[i]);
                        float  nameX = sp.x + (cardW - tsz.x) * 0.5f;
                        if (nameX < sp.x + 2.f) nameX = sp.x + 2.f;
                        cdl->PushClipRect(ImVec2(sp.x, sp.y+2+barH),
                                          ImVec2(sp.x+cardW, sp.y+cardH), true);
                        cdl->AddText(
                            ImVec2(nameX, sp.y + 2 + barH + 2),
                            selected ? IM_COL32(255,255,255,240) : IM_COL32(180,200,220,140),
                            THEME_GRAD_NAMES[i]);
                        cdl->PopClipRect();
                        SetWindowFontScale(1.0f);

                        if (selected) {
                            cdl->AddRect(sp, ImVec2(sp.x+cardW, sp.y+cardH),
                                THEME_GRAD_BASE[i][0], 6.f, 0, 2.2f);
                        } else {
                            cdl->AddRect(sp, ImVec2(sp.x+cardW, sp.y+cardH),
                                IM_COL32(255,255,255,18), 6.f, 0, 1.f);
                        }

                        char cid[12]; snprintf(cid, 12, "##thc%d", i);
                        if (InvisibleButton(cid, ImVec2(cardW, cardH))) {
                            persistent_float["fTheme"] = (float)i;
                            curTheme = i;
                            switch_theme(0);
                            needSave = true;
                        }
                    }
                    int numRows = (THEME_COUNT + 1) / 2;
                    SetCursorPos(ImVec2(0, gridStartY + numRows * (cardH + GAP)));
                    Dummy(ImVec2(0, 6.f));
                }
            }

            PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,0.15f));
            Separator(); PopStyleColor();

            // ── // SOLID COLOR ────────────────────────────────────
            SecLabel("// SOLID COLOR");
            needSave |= Toggle("sc0", "Enable Solid Color", "bSolidColor", 0);

            if (persistent_bool["bSolidColor"]) {
                // Hue preview bar
                {
                    ImVec2 barSp = GetCursorScreenPos();
                    float barW = GetContentRegionAvail().x - 2.f;
                    const float barH = 14.f;
                    // Draw rainbow hue bar in 12 segments
                    float segW = barW / 12.f;
                    for (int s = 0; s < 12; s++) {
                        float r0, g0, b0, r1, g1, b1;
                        ImGui::ColorConvertHSVtoRGB(s/12.f,      1.f, 1.f, r0, g0, b0);
                        ImGui::ColorConvertHSVtoRGB((s+1)/12.f,  1.f, 1.f, r1, g1, b1);
                        GetWindowDrawList()->AddRectFilledMultiColor(
                            ImVec2(barSp.x + s*segW, barSp.y),
                            ImVec2(barSp.x + (s+1)*segW, barSp.y + barH),
                            IM_COL32((int)(r0*255),(int)(g0*255),(int)(b0*255),255),
                            IM_COL32((int)(r1*255),(int)(g1*255),(int)(b1*255),255),
                            IM_COL32((int)(r1*255),(int)(g1*255),(int)(b1*255),255),
                            IM_COL32((int)(r0*255),(int)(g0*255),(int)(b0*255),255));
                    }
                    // Cursor line at current hue
                    float cx = barSp.x + persistent_float["fSolidHue"] * barW;
                    GetWindowDrawList()->AddLine(ImVec2(cx, barSp.y-2), ImVec2(cx, barSp.y+barH+2), IM_COL32(255,255,255,220), 2.f);
                    Dummy(ImVec2(barW, barH + 4.f));
                }
                needSave |= Slider("##shue", "Hue",
                    &persistent_float["fSolidHue"], 0.0f, 1.0f, "%.2f", 0);
            }

            PushStyleColor(ImGuiCol_Separator, ImVec4(1,1,1,0.15f));
            Separator(); PopStyleColor();

            // ── // GENERAL ────────────────────────────────────────
            SecLabel("// GENERAL");
            needSave |= Toggle("fps",  "Show FPS Overlay",    "bShowFPS",    0);

            needSave |= Toggle("asv",  "Auto Save Settings",  "bAutoSave",   3);

            Dummy(ImVec2(0, 14.f));
            if (needSave) save_persistence();
        }
        EndChild();
        PopStyleVar(4);
        PopStyleColor(5);
    }
    End();
    PopStyleVar(4);
    PopStyleColor();
}

// ════════════════════════════════════════════════════════════════════
//  INFO PANEL — Device hardware & runtime information
// ════════════════════════════════════════════════════════════════════
INLINE void DrawServerMonitorPanel(ImGuiIO& io, ImVec2 mainPos, float mainW, float mainH) {
    const float SW  = 360.f;
    const float SH  = mainH;
    const float PAD = 12.f;

    // ── Theme-aware palette ───────────────────────────────────────
    ImU32 CR[5], CR_MID[5];
    FillThemePalette(CR, nullptr, CR_MID);

    // Position: LEFT of main menu; fallback RIGHT
    float sx = mainPos.x - SW - 10.f;
    if (sx < 0.f) sx = mainPos.x + mainW + 10.f;
    if (sx + SW > (float)Width) sx = mainPos.x + 4.f;
    float sy = mainPos.y;

    float alpha = g_menu.menuAlpha * persistent_float["fMenuOpacity"];
    // Alpha helper — apply opacity to raw DrawList colors
    auto ApA = [alpha](ImU32 c) -> ImU32 {
        if (alpha >= 0.999f) return c;
        ImU32 a = ImU32(float((c >> 24) & 0xFF) * alpha);
        return (c & 0x00FFFFFFu) | (a << 24);
    };

    SetNextWindowPos(ImVec2(sx, sy), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(SW, SH), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.02f, 0.03f, 0.97f * alpha));
    PushStyleVar(ImGuiStyleVar_WindowRounding,   5.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0,0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    PushStyleVar(ImGuiStyleVar_Alpha,            alpha);

    if (Begin(O("##SrvMonWin"), nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
        ImGuiWindowFlags_NoMove      | ImGuiWindowFlags_NoResize)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 wp = GetWindowPos();

        // Rainbow top bar
        float sw5 = SW / 5.f;
        for (int i = 0; i < 5; i++)
            dl->AddRectFilledMultiColor(
                ImVec2(wp.x+i*sw5, wp.y), ImVec2(wp.x+(i+1)*sw5, wp.y+3.f),
                ApA(CR[i]), ApA(CR[(i+1)%5]), ApA(CR[(i+1)%5]), ApA(CR[i]));

        // Rainbow border
        float sh5 = SH / 5.f, th = 1.5f;
        for (int i = 0; i < 5; i++) {
            dl->AddLine(ImVec2(wp.x+i*sw5,   wp.y+1),    ImVec2(wp.x+(i+1)*sw5, wp.y+1),    ApA(CR_MID[i]),       th);
            dl->AddLine(ImVec2(wp.x+i*sw5,   wp.y+SH-1), ImVec2(wp.x+(i+1)*sw5, wp.y+SH-1), ApA(CR_MID[(i+2)%5]), th);
            dl->AddLine(ImVec2(wp.x+1,       wp.y+i*sh5), ImVec2(wp.x+1,         wp.y+(i+1)*sh5), ApA(CR_MID[(i+1)%5]), th);
            dl->AddLine(ImVec2(wp.x+SW-1,    wp.y+i*sh5), ImVec2(wp.x+SW-1,      wp.y+(i+1)*sh5), ApA(CR_MID[(i+3)%5]), th);
        }

        // Header
        const float HDR_H = 58.f;
        dl->AddRectFilled(ImVec2(wp.x, wp.y+3), ImVec2(wp.x+SW, wp.y+HDR_H),
            ApA(IM_COL32(18, 4, 6, 200)));
        SetWindowFontScale(0.90f);
        dl->AddText(ImVec2(wp.x+PAD+4, wp.y+12), ApA(CR[0]), O("DEVICE INFO"));
        SetWindowFontScale(0.52f);
        dl->AddText(ImVec2(wp.x+PAD+4, wp.y+36), ApA(IM_COL32(200,180,185,120)), O("HARDWARE & RUNTIME"));
        SetWindowFontScale(1.0f);

        // Close button — perfectly centred in HDR
        const float xBtnSz2 = 28.f;
        const float xBtnY2  = (HDR_H - xBtnSz2) * 0.5f;
        if (DrawCloseButton(O("##xsrv"), ImVec2(SW - xBtnSz2 - 10.f, xBtnY2), xBtnSz2))
            g_menu.showServerMonitor = false;

        dl->AddLine(ImVec2(wp.x, wp.y+HDR_H), ImVec2(wp.x+SW, wp.y+HDR_H),
                    ApA(IM_COL32(255,255,255,18)), 1.f);

        // ── Scrollable content ─────────────────────────────────────
        SetCursorPos(ImVec2(0, HDR_H));
        PushStyleColor(ImGuiCol_ChildBg,              ImVec4(0,0,0,0));
        PushStyleColor(ImGuiCol_ScrollbarBg,          ImVec4(0,0,0,0));
        PushStyleColor(ImGuiCol_ScrollbarGrab,        ImVec4(0.86f,0.10f,0.18f,0.55f));
        PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f,0.15f,0.22f,0.75f));
        PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImVec4(0.75f,0.08f,0.15f,0.95f));
        PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(PAD, 0));
        PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);
        PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);

        if (BeginChild(O("##infoscr"), ImVec2(SW, SH - HDR_H - 4.f), false,
                ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

            ImDrawList* cdl = GetWindowDrawList();
            float avW = GetContentRegionAvail().x;

            // ── Cached device info ─────────────────────────────────
            static char sCpuModel[192]  = {};
            static char sKernel[192]    = {};
            static char sAndVer[64]     = {};
            static char sModel[128]     = {};
            static long sMemTotalKB     = 0;
            static long sMemAvailKB     = 0;
            static bool sInfoRead       = false;
            if (!sInfoRead) {
                sInfoRead = true;
                // CPU — try Hardware first, fallback to Processor
                FILE* f = fopen("/proc/cpuinfo", "r");
                if (f) {
                    char line[256];
                    while (fgets(line, sizeof(line), f)) {
                        if (strncmp(line, "Hardware", 8) == 0) {
                            char* c = strchr(line, ':');
                            if (c) { c++; while(*c==' ') c++;
                                strncpy(sCpuModel, c, 191);
                                sCpuModel[strcspn(sCpuModel,"\n")] = 0; break; }
                        }
                    }
                    if (!sCpuModel[0]) {
                        rewind(f);
                        while (fgets(line, sizeof(line), f)) {
                            if (strncmp(line, "Processor", 9)==0 || strncmp(line,"model name",10)==0) {
                                char* c = strchr(line, ':');
                                if (c) { c++; while(*c==' ') c++;
                                    strncpy(sCpuModel, c, 191);
                                    sCpuModel[strcspn(sCpuModel,"\n")] = 0; break; }
                            }
                        }
                    }
                    fclose(f);
                }
                if (!sCpuModel[0]) strncpy(sCpuModel, "Unknown CPU", 191);
                // RAM
                FILE* m = fopen("/proc/meminfo", "r");
                if (m) {
                    char line[128];
                    while (fgets(line, sizeof(line), m)) {
                        if (strncmp(line,"MemTotal",8)==0)     sscanf(line,"MemTotal: %ld kB",&sMemTotalKB);
                        if (strncmp(line,"MemAvailable",12)==0) sscanf(line,"MemAvailable: %ld kB",&sMemAvailKB);
                    }
                    fclose(m);
                }
                // Kernel version
                FILE* kf = fopen("/proc/version", "r");
                if (kf) {
                    char line[256];
                    if (fgets(line, sizeof(line), kf)) {
                        // trim at first '(' and newline
                        char* par = strchr(line, '(');
                        if (par) *par = 0;
                        char* nl = strchr(line, '\n');
                        if (nl) *nl = 0;
                        strncpy(sKernel, line, 191);
                    }
                    fclose(kf);
                }
                // Kernel fallback via uname() syscall
                if (!sKernel[0]) {
                    struct utsname uts;
                    if (uname(&uts) == 0 && uts.release[0]) {
                        snprintf(sKernel, 191, "%s", uts.release);
                    }
                }
                if (!sKernel[0]) strncpy(sKernel, "Unknown Kernel", 191);
                // Android version — try __system_property_get first (most reliable)
                {
                    char propVal[PROP_VALUE_MAX] = {};
                    if (__system_property_get("ro.build.version.release", propVal) > 0) {
                        strncpy(sAndVer, propVal, 63);
                        sAndVer[63] = 0;
                    }
                }
                // Fallback: build.prop
                if (!sAndVer[0]) {
                    const char* propPaths[] = {
                        "/system/build.prop",
                        "/vendor/build.prop",
                        "/odm/build.prop",
                        nullptr
                    };
                    for (int pi = 0; propPaths[pi] && !sAndVer[0]; pi++) {
                        FILE* bp = fopen(propPaths[pi], "r");
                        if (bp) {
                            char line[256];
                            while (fgets(line, sizeof(line), bp)) {
                                if (strncmp(line, "ro.build.version.release=", 25)==0) {
                                    strncpy(sAndVer, line+25, 63);
                                    sAndVer[strcspn(sAndVer,"\n")] = 0; break;
                                }
                            }
                            fclose(bp);
                        }
                    }
                }
                if (!sAndVer[0]) strncpy(sAndVer, "Unknown", 63);
                // Device Model
                {
                    char propVal[PROP_VALUE_MAX] = {};
                    if (__system_property_get("ro.product.model", propVal) > 0)
                        strncpy(sModel, propVal, 127);
                    if (!sModel[0]) __system_property_get("ro.product.name", sModel);
                    if (!sModel[0]) strncpy(sModel, "Unknown", 127);
                }
            }
            // Refresh MemAvailable every 2 s
            static float ramRefT = 0.f;
            ramRefT += io.DeltaTime;
            if (ramRefT >= 2.f) {
                ramRefT = 0.f;
                FILE* m = fopen("/proc/meminfo", "r");
                if (m) {
                    char line[128];
                    while (fgets(line, sizeof(line), m))
                        if (strncmp(line,"MemAvailable",12)==0) { sscanf(line,"MemAvailable: %ld kB",&sMemAvailKB); break; }
                    fclose(m);
                }
            }

            // Format values
            char bufRes[64];   snprintf(bufRes, 63, "%d x %d px", Width, Height);
            char bufFps[32];   snprintf(bufFps, 31, "%.0f fps", io.Framerate);
            char bufRamU[48];  snprintf(bufRamU, 47, "%.0f / %.0f MB",
                (sMemTotalKB - sMemAvailKB)/1024.f, sMemTotalKB/1024.f);
            char bufRamF[32];  snprintf(bufRamF, 31, "%.0f MB free", sMemAvailKB/1024.f);
            char bufAndV[64];  snprintf(bufAndV, 63, "Android %s", sAndVer);

            struct InfoRow { const char* label; const char* value; ImU32 valCol; };
            const InfoRow rows[] = {
                { "Model",           sModel,    CR[0] },
                { "Resolution",      bufRes,    CR[1] },
                { "Frame Rate",      bufFps,    CR[3] },
                { "Android Version", bufAndV,   CR[0] },
                { "CPU / SoC",       sCpuModel, CR[2] },
                { "Kernel",          sKernel,   CR[4] },
                { "RAM Usage",       bufRamU,   CR[1] },
                { "RAM Free",        bufRamF,   CR[3] },
            };
            constexpr int ROW_COUNT = 8;

            Dummy(ImVec2(0, 8.f));

            // ── Device info rows — redesigned ─────────────────────
            for (int ri = 0; ri < ROW_COUNT; ri++) {
                const float rowH = 58.f;
                ImVec2 rsp = GetCursorScreenPos();

                // Gradient background: left slightly lighter, right darker
                uint8_t ar = (uint8_t)((rows[ri].valCol >>  0) & 0xFF);
                uint8_t ag = (uint8_t)((rows[ri].valCol >>  8) & 0xFF);
                uint8_t ab = (uint8_t)((rows[ri].valCol >> 16) & 0xFF);
                cdl->AddRectFilledMultiColor(
                    rsp, ImVec2(rsp.x+avW, rsp.y+rowH),
                    IM_COL32(ar/8,ag/8,ab/8+8, 180),  IM_COL32(4,12,28, 120),
                    IM_COL32(4,12,28, 120),             IM_COL32(ar/8,ag/8,ab/8+8, 180));
                // Outer border — theme-coloured, dim
                cdl->AddRect(rsp, ImVec2(rsp.x+avW, rsp.y+rowH),
                    (rows[ri].valCol & 0x00FFFFFFu) | 0x40000000u, 5.f, 0, 1.f);

                // Left accent bar — gradient top-bright, bottom-fade
                cdl->AddRectFilledMultiColor(
                    rsp, ImVec2(rsp.x+4.f, rsp.y+rowH),
                    rows[ri].valCol, rows[ri].valCol,
                    (rows[ri].valCol & 0x00FFFFFFu) | 0x18000000u,
                    (rows[ri].valCol & 0x00FFFFFFu) | 0x18000000u);

                // Small dot before label
                cdl->AddCircleFilled(
                    ImVec2(rsp.x+13.f, rsp.y+12.f), 2.5f,
                    (rows[ri].valCol & 0x00FFFFFFu) | 0xB0000000u, 8);

                // Label
                SetWindowFontScale(0.46f);
                cdl->AddText(ImVec2(rsp.x+20.f, rsp.y+7.f),
                    IM_COL32(150,195,225,160), rows[ri].label);

                // Value — clipped, bright
                SetWindowFontScale(0.62f);
                char vbuf[200]; strncpy(vbuf, rows[ri].value, 199);
                while (strlen(vbuf) > 3 && CalcTextSize(vbuf).x > avW - 18.f)
                    vbuf[strlen(vbuf)-1] = 0;
                if (strlen(vbuf) < strlen(rows[ri].value)) {
                    size_t l = strlen(vbuf);
                    if (l > 3) { vbuf[l-1]='.'; vbuf[l-2]='.'; vbuf[l-3]='.'; }
                }
                cdl->AddText(ImVec2(rsp.x+12.f, rsp.y+28.f), rows[ri].valCol, vbuf);
                SetWindowFontScale(1.0f);

                // Thin bottom separator (except last)
                if (ri < ROW_COUNT-1)
                    cdl->AddLine(
                        ImVec2(rsp.x+8, rsp.y+rowH-1),
                        ImVec2(rsp.x+avW-8, rsp.y+rowH-1),
                        IM_COL32(255,255,255,8), 1.f);

                char hid[24]; snprintf(hid, 24, "##inforow%d", ri);
                InvisibleButton(hid, ImVec2(avW, rowH));
                Dummy(ImVec2(0, 4.f));
            }

            Dummy(ImVec2(0, 14.f));
        }
        EndChild();
        PopStyleVar(3);
        PopStyleColor(5);
    }
    End();
    PopStyleVar(4);
    PopStyleColor();
}

INLINE void DrawMenu(ImGuiIO& io) {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        // ── Update toast (floating, no menu needed) ────────────────────
        if (g_UpdateReady && !g_UpdateSavePath.empty()) {
            SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.08f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.20f, 0.05f, 0.93f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 10.0f));
            if (Begin(O("##UpdateToast"), nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
                TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), O("Update downloaded! Restart game to apply."));
            }
            End();
            PopStyleVar(2);
            PopStyleColor();
        } else if (g_UpdateDownloading) {
            SetNextWindowPos(ImVec2(Width * 0.5f, Height * 0.08f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            SetNextWindowSize(ImVec2(0, 0), ImGuiCond_Always);
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.10f, 0.22f, 0.90f));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 10.0f));
            if (Begin(O("##DownloadToast"), nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
                TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), O("Downloading update..."));
            }
            End();
            PopStyleVar(2);
            PopStyleColor();
        }

        // Menu only visible inside a match
        /*if (!g_bInGame) {
            g_menu.isOpen    = false;
            g_menu.menuAlpha = 0.0f;
            return;
        }*/

        if (g_menu.isOpen) {
            g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        } else {
            g_menu.menuAlpha = 0.0f;
        }

        if (g_menu.menuAlpha > 0.01f) {
            // ── Responsive scaling based on screen resolution ─────────
            float _mscale = ImClamp((float)Width / 1080.f, 0.72f, 1.30f);
            const float W   = 560.0f * _mscale;
            const float H   = 520.0f * _mscale;
            const float PAD = 16.0f * _mscale;

            // ── Theme-aware palette ───────────────────────────────
            if (persistent_float["fMenuOpacity"] < 0.70f) persistent_float["fMenuOpacity"] = 1.0f;
            ImU32 CR[5], CR_DIM[5], CR_MID[5];
            FillThemePalette(CR, CR_DIM, CR_MID);
            const ImU32 SEP = IM_COL32(255,255,255, 18);

            // Allow user to move + resize; first-time size only
            SetNextWindowSize(ImVec2(W, H), ImGuiCond_FirstUseEver);
            SetNextWindowSizeConstraints(ImVec2(380.f*_mscale, 420.f*_mscale),
                                         ImVec2(700.f*_mscale, 800.f*_mscale));
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

            float menuOp = g_menu.menuAlpha * persistent_float["fMenuOpacity"];
            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.059f, 0.059f, 0.118f, menuOp));
            PushStyleVar(ImGuiStyleVar_WindowRounding,  5.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, menuOp);

            if (Begin(O("##ChromeMenu"), &g_menu.isOpen,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

                ImDrawList* dl   = GetWindowDrawList();
                ImVec2      wp   = GetWindowPos();
                ImVec2      wSz  = GetWindowSize();   // actual window size (respects resize)
                float       WW   = wSz.x;
                float       HH   = wSz.y;
                bool        need_save = false;

                // ── Rainbow top bar ───────────────────────────────────
                {
                    float sw = WW / 5.0f;
                    for (int i = 0; i < 5; i++) {
                        ImU32 cL = CR[i], cR = CR[(i+1)%5];
                        dl->AddRectFilledMultiColor(
                            ImVec2(wp.x + i*sw,     wp.y),
                            ImVec2(wp.x + (i+1)*sw, wp.y + 3.f),
                            cL, cR, cR, cL);
                    }
                }

                // ── Rainbow outer border — animated colour cycling ────
                {
                    float sw = WW / 5.f, sh = HH / 5.f, th = 1.5f;
                    int bOff = (int)(ImGui::GetTime() * 1.8f) % 5;
                    for (int i = 0; i < 5; i++) {
                        int a = (i + bOff) % 5;
                        dl->AddLine(ImVec2(wp.x+i*sw,    wp.y+1),    ImVec2(wp.x+(i+1)*sw, wp.y+1),    CR_MID[a],          th);
                        dl->AddLine(ImVec2(wp.x+i*sw,    wp.y+HH-1), ImVec2(wp.x+(i+1)*sw, wp.y+HH-1), CR_MID[(a+2)%5],    th);
                        dl->AddLine(ImVec2(wp.x+1,       wp.y+i*sh), ImVec2(wp.x+1,        wp.y+(i+1)*sh), CR_MID[(a+1)%5], th);
                        dl->AddLine(ImVec2(wp.x+WW-1,    wp.y+i*sh), ImVec2(wp.x+WW-1,     wp.y+(i+1)*sh), CR_MID[(a+3)%5], th);
                    }
                }

                // ════════════════════════════════════════════════════
                // HEADER  (110 px)
                // ════════════════════════════════════════════════════
                const float HDR_H = 110.0f;

                float rcX = wp.x + PAD + 24.f, rcY = wp.y + HDR_H*0.5f;
                static float hRot = 0.f;
                hRot += io.DeltaTime * 2.5f;
                for (int i = 0; i < 5; i++) {
                    float a0 = hRot + i*(6.2832f/5.f);
                    float a1 = a0 + (6.2832f/5.f)*0.75f;
                    dl->PathArcTo(ImVec2(rcX,rcY), 25.f, a0, a1, 12);
                    dl->PathStroke(CR[i], 0, 2.5f);
                }
                dl->AddCircleFilled(ImVec2(rcX,rcY), 17.f, IM_COL32(12,12,28,255), 24);
                SetWindowFontScale(0.65f);
                {
                    ImVec2 fhSz = CalcTextSize("CM");
                    dl->AddText(ImVec2(rcX-fhSz.x*0.5f, rcY-fhSz.y*0.5f), IM_COL32(255,255,255,230), "CM");
                }
                SetWindowFontScale(1.0f);

                float tX = wp.x + PAD + 60.f;
                SetWindowFontScale(1.18f);
                // Title glow (shadow layer)
                dl->AddText(ImVec2(tX+1.f, wp.y+11), (CR[0] & 0x00FFFFFFu) | 0x50000000u, O("CELZ MODZ"));
                dl->AddText(ImVec2(tX, wp.y+10), CR[0], O("CELZ MODZ"));
                SetWindowFontScale(0.62f);
                dl->AddText(ImVec2(tX, wp.y+48), IM_COL32(200,220,255,120), O("v1.0"));
                SetWindowFontScale(1.0f);

                // ── LICENSE INFO CARD ──────────────────────────────
                {
                    LicCountdown cd = ComputeLicCountdown();

                    // HDR_H=110, so cY = wp.y+4 (badge always inside window)
                    const float hdrS = 26.f;
                    const float cW   = ImClamp(WW * 0.52f, 194.f, 258.f);
                    const float cH   = 102.f;
                    const float cX   = wp.x + WW - PAD - cW - 2.f;
                    const float cY   = wp.y + (HDR_H - cH) * 0.5f;
                    const float cR   = 8.f;

                    ImU32 stCol = cd.expired      ? IM_COL32(255, 70,  70,  255)
                                : (cd.days < 3)   ? IM_COL32(255, 205, 40,  255)
                                :                   IM_COL32(50,  225, 140, 255);
                    ImU32 bdCol = cd.expired      ? IM_COL32(220, 60,  60,  180)
                                : (cd.days < 3)   ? IM_COL32(255, 180, 30,  150)
                                :                   (CR[2] & 0x00FFFFFFu) | 0x88000000u;

                    // Outer glow
                    dl->AddRectFilled(ImVec2(cX-3.f, cY-3.f), ImVec2(cX+cW+3.f, cY+cH+3.f),
                        (bdCol & 0x00FFFFFFu) | 0x28000000u, cR+3.f);

                    // Card base
                    dl->AddRectFilled(ImVec2(cX, cY), ImVec2(cX+cW, cY+cH),
                        IM_COL32(3, 9, 20, 248), cR);

                    // Header gradient strip
                    dl->AddRectFilledMultiColor(
                        ImVec2(cX, cY), ImVec2(cX+cW, cY+hdrS),
                        (CR[0] & 0x00FFFFFFu) | 0x30000000u, (CR[2] & 0x00FFFFFFu) | 0x26000000u,
                        (CR[2] & 0x00FFFFFFu) | 0x14000000u, (CR[0] & 0x00FFFFFFu) | 0x1C000000u);

                    // Bottom fade
                    dl->AddRectFilledMultiColor(
                        ImVec2(cX, cY+cH-14.f), ImVec2(cX+cW, cY+cH),
                        IM_COL32(0,0,0,0), IM_COL32(0,0,0,0),
                        (bdCol & 0x00FFFFFFu) | 0x1C000000u, (bdCol & 0x00FFFFFFu) | 0x1C000000u);

                    // Border
                    dl->AddRect(ImVec2(cX, cY), ImVec2(cX+cW, cY+cH), bdCol, cR, 0, 1.3f);

                    // Top accent line
                    dl->AddRectFilledMultiColor(
                        ImVec2(cX+cR, cY), ImVec2(cX+cW-cR, cY+2.f),
                        IM_COL32(0,0,0,0), CR[0], CR[2], IM_COL32(0,0,0,0));

                    // Left accent bar
                    dl->AddRectFilledMultiColor(
                        ImVec2(cX, cY+cR), ImVec2(cX+2.5f, cY+cH-cR),
                        CR[0], CR[0], CR[2], CR[2]);

                    // Header-strip bottom separator
                    dl->AddLine(ImVec2(cX+8.f, cY+hdrS), ImVec2(cX+cW-8.f, cY+hdrS),
                        IM_COL32(255,255,255,20), 1.f);

                    // ── Badges (right side of header strip) ──
                    // Both ACTIVE and UPDATE badges are drawn right-to-left, no overlap
                    SetWindowFontScale(0.38f);
                    {
                        const char* stTxt = cd.expired ? "EXPIRED"
                                          : (cd.days < 3) ? "WARN" : "ACTIVE";
                        ImVec2 stSz = CalcTextSize(stTxt);
                        float bW = stSz.x + 12.f, bH = stSz.y + 6.f;
                        float bX = cX + cW - bW - 8.f;
                        float bY = cY + (hdrS - bH) * 0.5f;
                        dl->AddRectFilled(ImVec2(bX, bY), ImVec2(bX+bW, bY+bH),
                            (stCol & 0x00FFFFFFu) | 0x40000000u, bH*0.5f);
                        dl->AddRect(ImVec2(bX, bY), ImVec2(bX+bW, bY+bH),
                            (stCol & 0x00FFFFFFu) | 0xA0000000u, bH*0.5f, 0, 1.f);
                        dl->AddCircleFilled(ImVec2(bX+6.5f, bY+bH*0.5f), 2.3f, stCol, 8);
                        dl->AddText(ImVec2(bX+11.f, bY+(bH-stSz.y)*0.5f), stCol, stTxt);

                        // UPDATE badge — placed to the LEFT of status badge, no overlap
                        if (g_UpdateAvailable) {
                            ImVec2 uSz = CalcTextSize("UPDATE");
                            float uW = uSz.x + 12.f, uH = uSz.y + 6.f;
                            float uX = bX - uW - 5.f;
                            float uY = cY + (hdrS - uH) * 0.5f;
                            dl->AddRectFilled(ImVec2(uX, uY), ImVec2(uX+uW, uY+uH),
                                IM_COL32(255, 200, 30, 55), uH*0.5f);
                            dl->AddRect(ImVec2(uX, uY), ImVec2(uX+uW, uY+uH),
                                IM_COL32(255, 210, 40, 180), uH*0.5f, 0, 1.f);
                            dl->AddCircleFilled(ImVec2(uX+6.5f, uY+uH*0.5f), 2.3f,
                                IM_COL32(255, 210, 40, 255), 8);
                            dl->AddText(ImVec2(uX+11.f, uY+(uH-uSz.y)*0.5f),
                                IM_COL32(255, 225, 60, 255), "UPDATE");
                        }
                    }
                    SetWindowFontScale(1.0f);

                    // ── Info rows ──
                    const float LX    = cX + 12.f;
                    const float VX    = cX + 52.f;
                    const float mVW   = cX + cW - 12.f - VX;
                    const float rY0   = cY + hdrS + 4.f;
                    const float rH    = 18.f;
                    const float sepX2 = cX + cW - 10.f;

                    SetWindowFontScale(0.44f);
                    ImU32 lblCol = IM_COL32(130, 175, 220, 195);
                    ImU32 valCol = IM_COL32(225, 242, 255, 242);
                    ImU32 sepC   = IM_COL32(255, 255, 255, 14);

                    // Row 1 — USER
                    dl->AddText(ImVec2(LX, rY0), lblCol, "USER:");
                    {
                        std::string un = g_Username.empty() ? "User" : g_Username;
                        while (un.size() > 1 && CalcTextSize(un.c_str()).x > mVW)
                            un = un.substr(0, un.size()-1);
                        dl->AddText(ImVec2(VX, rY0), valCol, un.c_str());
                    }
                    dl->AddLine(ImVec2(LX, rY0+rH-1.f), ImVec2(sepX2, rY0+rH-1.f), sepC, 1.f);

                    // Row 2 — HUID
                    dl->AddText(ImVec2(LX, rY0+rH), lblCol, "HUID:");
                    {
                        char hb[24];
                        const std::string& hw = g_HWID.empty() ? persistent_string["androidID"] : g_HWID;
                        if (hw.size() >= 6)
                            snprintf(hb, sizeof(hb), "%.4s****%.4s", hw.c_str(), hw.c_str()+(int)hw.size()-4);
                        else if (!hw.empty()) snprintf(hb, sizeof(hb), "%.8s", hw.c_str());
                        else                  snprintf(hb, sizeof(hb), "N/A");
                        dl->AddText(ImVec2(VX, rY0+rH), IM_COL32(195, 225, 255, 220), hb);
                    }
                    dl->AddLine(ImVec2(LX, rY0+rH*2-1.f), ImVec2(sepX2, rY0+rH*2-1.f), sepC, 1.f);

                    // Row 3 — KEYS
                    dl->AddText(ImVec2(LX, rY0+rH*2), lblCol, "KEYS:");
                    {
                        char kb[24];
                        const std::string& ky = persistent_string["key"];
                        if (ky.size() >= 6)
                            snprintf(kb, sizeof(kb), "%.3s****%.4s", ky.c_str(), ky.c_str()+(int)ky.size()-4);
                        else if (!ky.empty()) snprintf(kb, sizeof(kb), "****");
                        else                  snprintf(kb, sizeof(kb), "N/A");
                        dl->AddText(ImVec2(VX, rY0+rH*2), IM_COL32(195, 225, 255, 220), kb);
                    }
                    dl->AddLine(ImVec2(LX, rY0+rH*3-1.f), ImVec2(sepX2, rY0+rH*3-1.f), sepC, 1.f);

                    // Row 4 — EXP countdown
                    dl->AddText(ImVec2(LX, rY0+rH*3), lblCol, "EXP:");
                    {
                        char eb[32];
                        if (cd.expired)
                            snprintf(eb, sizeof(eb), "EXPIRED");
                        else if (cd.valid) {
                            if (cd.days >= 1)
                                snprintf(eb, sizeof(eb), "%dd %02dh %02dm %02ds",
                                    cd.days, cd.hrs, cd.mins, cd.secs);
                            else if (cd.hrs >= 1)
                                snprintf(eb, sizeof(eb), "%02dh %02dm %02ds",
                                    cd.hrs, cd.mins, cd.secs);
                            else
                                snprintf(eb, sizeof(eb), "%02dm %02ds",
                                    cd.mins, cd.secs);
                        } else snprintf(eb, sizeof(eb), "N/A");
                        ImU32 eCol = cd.expired    ? IM_COL32(255, 80,  80,  255)
                                   : (cd.days < 3) ? IM_COL32(255, 205, 40,  255)
                                   :                 IM_COL32(90,  235, 175, 235);
                        dl->AddText(ImVec2(VX, rY0+rH*3), eCol, eb);
                    }

                    SetWindowFontScale(1.0f);
                }

                // Title area gradient (left side)
                  dl->AddRectFilledMultiColor(
                      ImVec2(wp.x, wp.y+4.f), ImVec2(wp.x + WW*0.55f, wp.y+HDR_H-2.f),
                      (CR[0] & 0x00FFFFFFu) | 0x10000000u, IM_COL32(0,0,0,0),
                      IM_COL32(0,0,0,0), (CR[0] & 0x00FFFFFFu) | 0x10000000u);
                  dl->AddLine(ImVec2(wp.x,wp.y+HDR_H), ImVec2(wp.x+WW,wp.y+HDR_H), SEP, 1.f);

                  // ════════════════════════════════════════════════════
                  // REAL-TIME STATS (computed before child window)
                // ════════════════════════════════════════════════════
                // FPS — directly from ImGui framerate
                char rtFps[16];  snprintf(rtFps,  sizeof(rtFps),  "%d",   (int)io.Framerate);
                // PING — smoothed frame-delta (render latency in ms)
                static float sPing = 15.f;
                sPing += (io.DeltaTime * 1000.f - sPing) * io.DeltaTime * 4.f;
                char rtPing[16]; snprintf(rtPing, sizeof(rtPing), "%dms", ImMax(1, (int)sPing));
                // PROC — CPU load estimate: lower FPS relative to 60fps target → higher load
                static float sProc = 65.f;
                float rawProc = ImClamp(100.f - (io.Framerate / 60.f) * 40.f, 40.f, 99.f);
                sProc += (rawProc - sProc) * io.DeltaTime * 2.f;
                char rtProc[16]; snprintf(rtProc, sizeof(rtProc), "%.0f%%", sProc);
                const char* STAT_VAL_RT[3] = { rtPing, rtFps, rtProc };
                static const char* const STAT_LBL[3] = {"PING","FPS","PROC"};
                const ImU32 STAT_COL[3] = { CR[2], CR[3], CR[4] };

                // ════════════════════════════════════════════════════
                // HELPERS (declared before child, safe O() usage)
                // ════════════════════════════════════════════════════
                static const char* const MOD_LBL[8] = {
                    "PREDICTION LINES", "ESP ALWAYS", "ESP POCKETS", "SHOT STATE", "AUTO PLAY", "AUTO QUEUE"
                };
                auto modRead = [&](int idx) -> bool {
                    if (idx == 0) return persistent_bool[O("bESP_DrawPredictionLine")];
                    if (idx == 1) return persistent_bool[O("bESPAlways")];
                    if (idx == 2) return persistent_bool[O("bESP_DrawPockets")];
                    if (idx == 3) return persistent_bool[O("bESP_DrawPocketsShotState")];
                    if (idx == 4) return persistent_bool[O("bAutoPlay")];
                    if (idx == 5) return persistent_bool[O("bAutoQueue")];
                    return AutoAimV2::aa_enabled;
                };
                auto modToggle = [&](int idx) {
                    if (idx == 0) persistent_bool[O("bESP_DrawPredictionLine")] ^= 1;
                    else if (idx == 1) persistent_bool[O("bESPAlways")] ^= 1;
                    else if (idx == 2) persistent_bool[O("bESP_DrawPockets")] ^= 1;
                    else if (idx == 3) persistent_bool[O("bESP_DrawPocketsShotState")] ^= 1;
                    else if (idx == 4) persistent_bool[O("bAutoPlay")] ^= 1;
                    else if (idx == 5) persistent_bool[O("bAutoQueue")] ^= 1;
                    else if (idx == 6) {
    AutoAimV2::aa_enabled ^= 1;
    if (AutoAimV2::aa_enabled) {
        AutoAimV2::aa_needScan = true;  // ← trigger scan di frame berikutnya
    }
}
                    need_save = true;
                };

                const float ROW_H   = 40.f, ROW_GAP = 9.f;
                const float ROW_W   = WW - PAD*2.f;
                const float BTN_H   = 40.f, BTN_GAP = 6.f;
                const float FOOTER_H = BTN_H + 16.f; // sep(6) + btn(40) + pad(10)

                // ════════════════════════════════════════════════════
                // SCROLLABLE CONTENT AREA (BeginChild)
                // ════════════════════════════════════════════════════
                float contentH = HH - HDR_H - FOOTER_H;
                SetCursorPos(ImVec2(0, HDR_H));
                PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
                PushStyleColor(ImGuiCol_ScrollbarBg,    ImVec4(0,0,0,0));
                PushStyleColor(ImGuiCol_ScrollbarGrab,  ImVec4(0.86f,0.10f,0.18f,0.55f));
                PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.95f,0.15f,0.22f,0.75f));
                PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImVec4(0.75f,0.08f,0.15f,0.95f));
                PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
                PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.f);

                if (BeginChild(O("##cscr"), ImVec2(WW, contentH), false,
                    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

                    ImDrawList* cdl = GetWindowDrawList();
                    float cy = 8.f;  // child-local y cursor

                    // ── Stats bar ────────────────────────────────────
                    const float STATS_H = 52.f, BOX_GAP = 6.f;
                    float bW3 = (WW - PAD*2.f - BOX_GAP*2.f) / 3.f;
                    for (int i = 0; i < 3; i++) {
                        SetCursorPos(ImVec2(PAD + i*(bW3+BOX_GAP), cy));
                        ImVec2 bsp = GetCursorScreenPos();
                        uint8_t _sr=(uint8_t)((CR[i+2]>>0)&0xFF),_sg=(uint8_t)((CR[i+2]>>8)&0xFF),_sb=(uint8_t)((CR[i+2]>>16)&0xFF);
                        cdl->AddRectFilledMultiColor(bsp, ImVec2(bsp.x+bW3, bsp.y+STATS_H),
                            IM_COL32(_sr/7,_sg/7,_sb/7+12,210), IM_COL32(6,12,30,140),
                            IM_COL32(6,12,30,140), IM_COL32(_sr/7,_sg/7,_sb/7+12,210));
                        cdl->AddRect(bsp, ImVec2(bsp.x+bW3, bsp.y+STATS_H),
                            (CR[i+2]&0x00FFFFFFu)|0x80000000u, 4.f, 0, 1.f);
                        cdl->AddRectFilledMultiColor(
                            bsp, ImVec2(bsp.x+bW3, bsp.y+2.f),
                            IM_COL32(0,0,0,0), CR[i+2], CR[i+2], IM_COL32(0,0,0,0));
                        SetWindowFontScale(0.57f);
                        ImVec2 ls = CalcTextSize(STAT_LBL[i]);
                        cdl->AddText(ImVec2(bsp.x+(bW3-ls.x)*0.5f, bsp.y+8),
                            IM_COL32(255, 200, 210, 140), STAT_LBL[i]);
                        SetWindowFontScale(0.62f);
                        ImVec2 vs = CalcTextSize(STAT_VAL_RT[i]);
                        float vx = bsp.x + (bW3 - vs.x) * 0.5f;
                        if (vx < bsp.x + 2.f) vx = bsp.x + 2.f;
                        cdl->AddText(ImVec2(vx, bsp.y+29),
                            STAT_COL[i], STAT_VAL_RT[i]);
                        SetWindowFontScale(1.0f);
                    }
                    // Advance cursor past stats row
                    SetCursorPos(ImVec2(PAD, cy));
                    Dummy(ImVec2(ROW_W, STATS_H));
                    cy += STATS_H + 10.f;

                    // ── Feature section — styled card label ──────────
                    cy += 10.f;
                    {
                        SetCursorPos(ImVec2(PAD, cy));
                        ImVec2 pilSp = GetCursorScreenPos();
                        const float pilH = 34.f;
                        SetWindowFontScale(0.58f);
                        ImVec2 ftSz = CalcTextSize("FEATURE");
                          const float pilW = ImClamp(22.f + ftSz.x + 22.f, 80.f, ROW_W);
                        // Card background
                        cdl->AddRectFilled(pilSp, ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                              IM_COL32(0,18,42,230), 6.f);
                          // Right glow
                          cdl->AddRectFilledMultiColor(
                              ImVec2(pilSp.x+pilW-16.f, pilSp.y), ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                              IM_COL32(0,0,0,0), (CR[0] & 0x00FFFFFFu) | 0x22000000u,
                              (CR[0] & 0x00FFFFFFu) | 0x22000000u, IM_COL32(0,0,0,0));
                        // Left accent gradient
                        cdl->AddRectFilledMultiColor(
                            pilSp, ImVec2(pilSp.x+4.f, pilSp.y+pilH),
                            CR[0], CR[0], IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
                        // Border
                        cdl->AddRect(pilSp, ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                              (CR[0] & 0x00FFFFFFu) | 0x70000000u, 6.f, 0, 1.f);
                        // Small dot icon (glow + solid)
                        cdl->AddCircleFilled(ImVec2(pilSp.x+12.f, pilSp.y+pilH*0.5f),
                            6.f, (CR[0]&0x00FFFFFFu)|0x50000000u, 12);
                        cdl->AddCircleFilled(ImVec2(pilSp.x+12.f, pilSp.y+pilH*0.5f),
                            4.f, CR[0], 12);
                        // "FEATURE" text
                        cdl->AddText(ImVec2(pilSp.x+22.f, pilSp.y+(pilH-ftSz.y)*0.5f),
                            (CR[0] & 0x00FFFFFFu) | 0xF0000000u, "FEATURE");
                        SetWindowFontScale(1.0f);
                        Dummy(ImVec2(ROW_W, pilH));
                        cy += pilH + 10.f;
                    }

                    // ── Module rows ───────────────────────────────────
                    for (int i = 0; i < 7; i++) {
                        bool  active = modRead(i);
                        bool  isGameOnly  = true;
                        bool  canActivate = (i == 6) ? true : g_bInGame; // BREAK PREDICTOR bisa aktif kapan saja
                        ImU32 mc  = CR[i % 5];
                        ImU32 mcd = CR_DIM[i % 5];
                        ImU32 mcm = CR_MID[i % 5];
                        uint8_t mr = (uint8_t)((mc>>0)&0xFF),
                                mg = (uint8_t)((mc>>8)&0xFF),
                                mb = (uint8_t)((mc>>16)&0xFF);

                        SetCursorPos(ImVec2(PAD, cy));
                        ImVec2 rsp = GetCursorScreenPos();  // screen pos = scroll-aware

                        // Row bg + border
                        cdl->AddRectFilled(rsp, ImVec2(rsp.x+ROW_W, rsp.y+ROW_H),
                            active ? IM_COL32(mr,mg,mb,22) : IM_COL32(255,255,255,4), 4.f);
                        cdl->AddRect(rsp, ImVec2(rsp.x+ROW_W, rsp.y+ROW_H),
                            active ? mcm : mcd, 4.f, 0, 1.f);
                        if (active)
                            cdl->AddRectFilledMultiColor(rsp, ImVec2(rsp.x+3.f, rsp.y+ROW_H),
                                mc, mc, (mc&0x00FFFFFFu)|0x10000000u, (mc&0x00FFFFFFu)|0x10000000u);

                        // Hit area (advances cursor automatically)
                        char bid[16]; snprintf(bid, sizeof(bid), "##mod%d", i);
                        if (InvisibleButton(bid, ImVec2(ROW_W, ROW_H))) modToggle(i);

                        // Coloured dot
                        float dotX = rsp.x+14.f, dotY = rsp.y+ROW_H*0.5f;
                        if (active) cdl->AddCircleFilled(ImVec2(dotX,dotY), 8.f, IM_COL32(mr,mg,mb,50), 14);
                        cdl->AddCircleFilled(ImVec2(dotX,dotY), 4.f,
                            active ? mc : IM_COL32(255,255,255,40), 12);

                        // Label
                        SetWindowFontScale(0.75f);
                        ImVec2 lsz = CalcTextSize(MOD_LBL[i]);
                        cdl->AddText(ImVec2(rsp.x+28, rsp.y+(ROW_H-lsz.y)*0.5f),
                            active ? IM_COL32(255,255,255,220) : IM_COL32(255,255,255,50),
                            MOD_LBL[i]);

                        // ON/OFF badge
                        const char* badge = active ? "ON" : "OFF";
                        SetWindowFontScale(0.62f);
                        ImVec2 bsz = CalcTextSize(badge);
                        float  bpx = 8.f, bpy = 2.f;
                        float  bW2 = bsz.x+bpx*2, bH2 = bsz.y+bpy*2;
                        float  bgx = rsp.x+ROW_W-12.f-bW2, bgy = rsp.y+(ROW_H-bH2)*0.5f;
                        cdl->AddRectFilled(ImVec2(bgx,bgy), ImVec2(bgx+bW2,bgy+bH2),
                            active ? IM_COL32(mr,mg,mb,45) : 0, 3.f);
                        cdl->AddRect(ImVec2(bgx,bgy), ImVec2(bgx+bW2,bgy+bH2),
                            active ? mcm : IM_COL32(255,255,255,25), 3.f, 0, 1.f);
                        cdl->AddText(ImVec2(bgx+bpx,bgy+bpy),
                            active ? mc : IM_COL32(255,255,255,50), badge);
                        SetWindowFontScale(1.0f);

                        cy += ROW_H + ROW_GAP;
                    }
                    cy += 8.f;

                    // ── AC BYPASS section label ───────────────────────
                    cy += 10.f;
                    {
                        SetCursorPos(ImVec2(PAD, cy));
                        ImVec2 pilSp = GetCursorScreenPos();
                        const float pilH = 34.f;
                        SetWindowFontScale(0.58f);
                        ImVec2 ftSz = CalcTextSize("AC BYPASS");
                        const float pilW = ImClamp(22.f + ftSz.x + 22.f, 80.f, ROW_W);
                        cdl->AddRectFilled(pilSp, ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                            IM_COL32(0,18,42,230), 6.f);
                        cdl->AddRectFilledMultiColor(
                            ImVec2(pilSp.x+pilW-16.f, pilSp.y), ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                            IM_COL32(0,0,0,0), (CR[3] & 0x00FFFFFFu) | 0x22000000u,
                            (CR[3] & 0x00FFFFFFu) | 0x22000000u, IM_COL32(0,0,0,0));
                        cdl->AddRectFilledMultiColor(
                            pilSp, ImVec2(pilSp.x+4.f, pilSp.y+pilH),
                            CR[3], CR[3], IM_COL32(0,0,0,0), IM_COL32(0,0,0,0));
                        cdl->AddRect(pilSp, ImVec2(pilSp.x+pilW, pilSp.y+pilH),
                            (CR[3] & 0x00FFFFFFu) | 0x70000000u, 6.f, 0, 1.f);
                        cdl->AddCircleFilled(ImVec2(pilSp.x+12.f, pilSp.y+pilH*0.5f),
                            6.f, (CR[3]&0x00FFFFFFu)|0x50000000u, 12);
                        cdl->AddCircleFilled(ImVec2(pilSp.x+12.f, pilSp.y+pilH*0.5f),
                            4.f, CR[3], 12);
                        cdl->AddText(ImVec2(pilSp.x+22.f, pilSp.y+(pilH-ftSz.y)*0.5f),
                            (CR[3] & 0x00FFFFFFu) | 0xF0000000u, "AC BYPASS");
                        SetWindowFontScale(1.0f);
                        Dummy(ImVec2(ROW_W, pilH));
                        cy += pilH + 10.f;
                    }

                    // ── AC Bypass toggle rows ─────────────────────────
                    auto bpRead = [&](int idx) -> bool {
                        if (idx == 0) return persistent_bool[O("bBypass_Root")];
                        if (idx == 1) return persistent_bool[O("bBypass_Maps")];
                        return persistent_bool[O("bBypass_Props")];
                    };
                    auto bpToggle = [&](int idx) {
                        if (idx == 0) persistent_bool[O("bBypass_Root")] ^= 1;
                        else if (idx == 1) persistent_bool[O("bBypass_Maps")] ^= 1;
                        else persistent_bool[O("bBypass_Props")] ^= 1;
                        SyncBypassFlags();
                        need_save = true;
                    };
                    static const char* const BP_LBL[3] = {
                        "ROOT DETECTION HIDE",
                        "MEMORY MAP SCRUB",
                        "BUILD PROPS SPOOF"
                    };
                    static const char* const BP_SUB[3] = {
                        "sembunyikan su/magisk/frida dari scan AC",
                        "filter /proc/self/maps - hapus baris lib kita",
                        "spoof ro.debuggable, ro.build.tags, dll"
                    };
                    for (int i = 0; i < 3; i++) {
                        bool active = bpRead(i);
                        ImU32 mc  = CR[(i + 2) % 5];
                        ImU32 mcd = CR_DIM[(i + 2) % 5];
                        ImU32 mcm = CR_MID[(i + 2) % 5];
                        uint8_t mr = (uint8_t)((mc>>0)&0xFF),
                                mg = (uint8_t)((mc>>8)&0xFF),
                                mb = (uint8_t)((mc>>16)&0xFF);

                        SetCursorPos(ImVec2(PAD, cy));
                        ImVec2 rsp = GetCursorScreenPos();

                        cdl->AddRectFilled(rsp, ImVec2(rsp.x+ROW_W, rsp.y+ROW_H),
                            active ? IM_COL32(mr,mg,mb,22) : IM_COL32(255,255,255,4), 4.f);
                        cdl->AddRect(rsp, ImVec2(rsp.x+ROW_W, rsp.y+ROW_H),
                            active ? mcm : mcd, 4.f, 0, 1.f);
                        if (active)
                            cdl->AddRectFilledMultiColor(rsp, ImVec2(rsp.x+3.f, rsp.y+ROW_H),
                                mc, mc, (mc&0x00FFFFFFu)|0x10000000u, (mc&0x00FFFFFFu)|0x10000000u);

                        char bid2[16]; snprintf(bid2, sizeof(bid2), "##bpmod%d", i);
                        if (InvisibleButton(bid2, ImVec2(ROW_W, ROW_H))) bpToggle(i);

                        float dotX = rsp.x+14.f, dotY = rsp.y+ROW_H*0.5f;
                        if (active) cdl->AddCircleFilled(ImVec2(dotX,dotY), 8.f, IM_COL32(mr,mg,mb,50), 14);
                        cdl->AddCircleFilled(ImVec2(dotX,dotY), 4.f,
                            active ? mc : IM_COL32(255,255,255,40), 12);

                        SetWindowFontScale(0.70f);
                        cdl->AddText(ImVec2(rsp.x+28, rsp.y+8),
                            active ? IM_COL32(255,255,255,220) : IM_COL32(255,255,255,50),
                            BP_LBL[i]);
                        SetWindowFontScale(0.46f);
                        cdl->AddText(ImVec2(rsp.x+28, rsp.y+24),
                            IM_COL32(120,150,185,130), BP_SUB[i]);
                        SetWindowFontScale(1.0f);

                        const char* badge = active ? "ON" : "OFF";
                        SetWindowFontScale(0.62f);
                        ImVec2 bsz2 = CalcTextSize(badge);
                        float bpx2 = 8.f, bpy2 = 2.f;
                        float bW2 = bsz2.x+bpx2*2, bH2 = bsz2.y+bpy2*2;
                        float bgx2 = rsp.x+ROW_W-12.f-bW2, bgy2 = rsp.y+(ROW_H-bH2)*0.5f;
                        cdl->AddRectFilled(ImVec2(bgx2,bgy2), ImVec2(bgx2+bW2,bgy2+bH2),
                            active ? IM_COL32(mr,mg,mb,45) : 0, 3.f);
                        cdl->AddRect(ImVec2(bgx2,bgy2), ImVec2(bgx2+bW2,bgy2+bH2),
                            active ? mcm : IM_COL32(255,255,255,25), 3.f, 0, 1.f);
                        cdl->AddText(ImVec2(bgx2+bpx2,bgy2+bpy2),
                            active ? mc : IM_COL32(255,255,255,50), badge);
                        SetWindowFontScale(1.0f);

                        cy += ROW_H + ROW_GAP;
                    }

                    // ── PATCH ALL libprotect.so button ────────────────
                    cy += 4.f;
                    {
                        int pact = 0;
                        for (int k = 0; k < AnticheatBypass::FUNC_COUNT; k++)
                            if (*AnticheatBypass::GetFuncs(k).active) pact++;
                        bool allPatched = (pact == AnticheatBypass::FUNC_COUNT);

                        SetCursorPos(ImVec2(PAD, cy));
                        ImVec2 bsp2 = GetCursorScreenPos();
                        const float bH3 = 36.f;
                        cdl->AddRectFilled(bsp2, ImVec2(bsp2.x+ROW_W, bsp2.y+bH3),
                            allPatched ? IM_COL32(0,40,15,200) : IM_COL32(0,30,60,200), 6.f);
                        cdl->AddRect(bsp2, ImVec2(bsp2.x+ROW_W, bsp2.y+bH3),
                            allPatched ? IM_COL32(52,211,153,200) : CR[3], 6.f, 0, 1.5f);
                        char plbl[48];
                        snprintf(plbl, sizeof(plbl),
                            allPatched ? "LIBPROTECT PATCHED  %d/8" : "PATCH ALL  libprotect.so  %d/8", pact);
                        SetWindowFontScale(0.60f);
                        ImVec2 psz = CalcTextSize(plbl);
                        cdl->AddText(ImVec2(bsp2.x+(ROW_W-psz.x)*0.5f, bsp2.y+(bH3-psz.y)*0.5f),
                            allPatched ? IM_COL32(240, 60, 70,255) : IM_COL32(255, 220, 225,220), plbl);
                        SetWindowFontScale(1.0f);
                        if (InvisibleButton("##bpAll2", ImVec2(ROW_W, bH3)))
                            AnticheatBypass::PatchAll();
                        cy += bH3 + ROW_GAP;
                    }

                    // Bottom padding so user can scroll to see last item
                    Dummy(ImVec2(0, 14.f));
                }
                EndChild();
                PopStyleVar(2);
                PopStyleColor(5);

                // ════════════════════════════════════════════════════
                // FOOTER — pinned to bottom of window
                // ════════════════════════════════════════════════════
                // ── Update available notification strip ──────────────
                if (g_UpdateAvailable) {
                    float stripH = 22.f;
                    float stripY = HH - FOOTER_H - stripH - 2.f;
                    dl->AddRectFilled(ImVec2(wp.x, wp.y+stripY),
                                      ImVec2(wp.x+WW, wp.y+stripY+stripH),
                                      IM_COL32(200,140,0,220));
                    SetWindowFontScale(0.55f);
                    char updbuf[64];
                    if (!g_LatestVersion.empty())
                        snprintf(updbuf, sizeof(updbuf), O("  UPDATE AVAILABLE: v%s"), g_LatestVersion.c_str());
                    else
                        snprintf(updbuf, sizeof(updbuf), O("  UPDATE AVAILABLE"));
                    ImVec2 us = CalcTextSize(updbuf);
                    dl->AddText(ImVec2(wp.x+(WW-us.x)*0.5f, wp.y+stripY+(stripH-us.y)*0.5f),
                        IM_COL32(20,15,0,255), updbuf);
                    SetWindowFontScale(1.0f);
                }

                float footerY = HH - FOOTER_H;
                dl->AddLine(ImVec2(wp.x, wp.y+footerY+1), ImVec2(wp.x+WW, wp.y+footerY+1), SEP, 1.f);

                float fWW     = WW - PAD*2.f;
                float cfgW    = fWW * 0.22f;
                float srvW    = fWW * 0.22f;
                float dmpW    = fWW * 0.20f;
                float iniW    = fWW - cfgW - srvW - dmpW - BTN_GAP*3.f;
                SetCursorPos(ImVec2(PAD, footerY + 8.f));

                // SETTINGS button — highlighted when panel is open
                bool sOpen = g_menu.showSettings;
                ImVec4 sBtnClr = sOpen ? ImVec4(0.60f,0.30f,0.95f,0.85f) : ImVec4(0,0,0,0);
                ImVec4 sBtnHov = sOpen ? ImVec4(0.65f,0.35f,1.0f,0.90f)  : ImVec4(1.f,1.f,1.f,0.08f);
                PushStyleColor(ImGuiCol_Button,        sBtnClr);
                PushStyleColor(ImGuiCol_ButtonHovered, sBtnHov);
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f,0.54f,0.98f,0.90f));
                PushStyleColor(ImGuiCol_Text,          sOpen ? ImVec4(1.f,1.f,1.f,1.0f) : ImVec4(1.f,1.f,1.f,0.55f));
                PushStyleColor(ImGuiCol_Border,        sOpen ? ImVec4(0.65f,0.40f,1.0f,0.80f) : ImVec4(1.f,1.f,1.f,0.20f));
                PushStyleVar(ImGuiStyleVar_FrameRounding,   4.f);
                PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                SetWindowFontScale(0.60f);
                if (Button(O("SETTINGS"), ImVec2(cfgW, BTN_H)))
                    g_menu.showSettings = !g_menu.showSettings;
                SetWindowFontScale(1.0f);
                PopStyleColor(5); PopStyleVar(2);

                SameLine(0, BTN_GAP);

                // SERVER button — highlighted when panel is open
                bool svOpen = g_menu.showServerMonitor;
                ImVec4 svBtnClr = svOpen ? ImVec4(0.0f,0.35f,0.65f,0.85f) : ImVec4(0,0,0,0);
                ImVec4 svBtnHov = svOpen ? ImVec4(0.0f,0.45f,0.75f,0.90f)  : ImVec4(1.f,1.f,1.f,0.08f);
                PushStyleColor(ImGuiCol_Button,        svBtnClr);
                PushStyleColor(ImGuiCol_ButtonHovered, svBtnHov);
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.0f,0.55f,0.85f,0.90f));
                PushStyleColor(ImGuiCol_Text,          svOpen ? ImVec4(0.4f,0.9f,1.f,1.0f) : ImVec4(1.f,1.f,1.f,0.55f));
                PushStyleColor(ImGuiCol_Border,        svOpen ? ImVec4(0.0f,0.78f,1.0f,0.80f) : ImVec4(1.f,1.f,1.f,0.20f));
                PushStyleVar(ImGuiStyleVar_FrameRounding,   4.f);
                PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                SetWindowFontScale(0.58f);
                if (Button(O("INFO"), ImVec2(srvW, BTN_H)))
                    g_menu.showServerMonitor = !g_menu.showServerMonitor;
                SetWindowFontScale(1.0f);
                PopStyleColor(5); PopStyleVar(2);

                // DUMP button — highlighted when panel is open
               /* {
                    bool dOpen = g_menu.showDump;
                    ImVec4 dBtnClr = dOpen ? ImVec4(0.0f,0.42f,0.60f,0.85f) : ImVec4(0,0,0,0);
                    ImVec4 dBtnHov = dOpen ? ImVec4(0.0f,0.55f,0.78f,0.90f) : ImVec4(1.f,1.f,1.f,0.08f);
                    PushStyleColor(ImGuiCol_Button,        dBtnClr);
                    PushStyleColor(ImGuiCol_ButtonHovered, dBtnHov);
                    PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.0f,0.70f,1.0f,0.90f));
                    PushStyleColor(ImGuiCol_Text,          dOpen ? ImVec4(0.3f,0.9f,1.f,1.0f) : ImVec4(1.f,1.f,1.f,0.55f));
                    PushStyleColor(ImGuiCol_Border,        dOpen ? ImVec4(0.0f,0.70f,1.0f,0.80f) : ImVec4(1.f,1.f,1.f,0.20f));
                    PushStyleVar(ImGuiStyleVar_FrameRounding,   4.f);
                    PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                    SetWindowFontScale(0.58f);
                    if (Button(O("DUMP"), ImVec2(dmpW, BTN_H)))
                        g_menu.showDump = !g_menu.showDump;
                    SetWindowFontScale(1.0f);
                    PopStyleColor(5); PopStyleVar(2);
                }*/

                SameLine(0, BTN_GAP);

                PushStyleColor(ImGuiCol_Button,        ImVec4(1.f,0.43f,0.78f,0.75f));
                PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f,0.55f,0.85f,0.85f));
                PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.65f,0.54f,0.98f,0.90f));
                PushStyleColor(ImGuiCol_Text,          ImVec4(1.f,1.f,1.f,1.0f));
                PushStyleColor(ImGuiCol_Border,        ImVec4(1.f,0.43f,0.78f,1.0f));
                PushStyleVar(ImGuiStyleVar_FrameRounding,   4.f);
                PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
                SetWindowFontScale(0.65f);
                if (Button(O("CLOSE"), ImVec2(iniW, BTN_H))) {
                    save_persistence();
                    g_menu.isOpen = false;
                }
                SetWindowFontScale(1.0f);
                PopStyleColor(5); PopStyleVar(2);

                if (need_save) save_persistence();
            }

            // Save the main window position+size for settings panel placement
            ImVec2 menuPos = GetWindowPos();
            ImVec2 menuSz  = GetWindowSize();
            End();

            PopStyleVar(4);
            PopStyleColor();

            // Draw panels AFTER main End() — separate windows, no overlap
            if (g_menu.showSettings && g_menu.menuAlpha > 0.01f)
                DrawSettingsPanel(io, menuPos, menuSz.x, menuSz.y);
            if (g_menu.showServerMonitor && g_menu.menuAlpha > 0.01f)
                DrawServerMonitorPanel(io, menuPos, menuSz.x, menuSz.y);
            if (g_menu.showDump && g_menu.menuAlpha > 0.01f)
                DrawDumpPanel(io, menuPos, menuSz.x, menuSz.y);
        }

        // ── bShowFPS: FPS overlay (top-left corner, always visible) ──
        if (persistent_bool[O("bShowFPS")]) {
            ImDrawList* fgdl = ImGui::GetForegroundDrawList();
            char fpsBuf[32];
            snprintf(fpsBuf, sizeof(fpsBuf), "FPS  %d", (int)io.Framerate);
            ImVec2 fsz = ImGui::CalcTextSize(fpsBuf);
            fgdl->AddRectFilled(ImVec2(6.f, 6.f),
                                ImVec2(14.f + fsz.x, 14.f + fsz.y),
                                IM_COL32(0, 0, 0, 170), 4.f);
            fgdl->AddText(ImVec2(10.f, 10.f), IM_COL32(52, 211, 153, 255), fpsBuf);
        }


        // ── bAutoSave: save persistence every 30 seconds ──────────────
        {
            static float s_asSaveTimer = 0.f;
            s_asSaveTimer += io.DeltaTime;
            if (persistent_bool[O("bAutoSave")] && s_asSaveTimer >= 30.f) {
                s_asSaveTimer = 0.f;
                save_persistence();
            }
        }

    }
}

static void DrawFloatingButton(ImGuiIO& io) {
    static ImVec2 buttonPos = ImVec2(80, 60);
    static bool isDragging = false;
    static float hoverAnim = 0.0f;
    
    float buttonRadius = 38.0f;
    float buttonSize = buttonRadius * 2.0f;
    float totalWidth = buttonSize + 8.0f;
    float totalHeight = buttonSize + 8.0f;
    
    bool isHovered = false;
    
    SetNextWindowPos(buttonPos, ImGuiCond_Always);
    SetNextWindowSize(ImVec2(totalWidth, totalHeight), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    if (Begin(O("##FloatBtn"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
        ImDrawList* dl = GetWindowDrawList();
        
        ImVec2 center = ImVec2(buttonPos.x + buttonRadius + 2, buttonPos.y + buttonRadius + 2);
        
        SetCursorPos(ImVec2(0, 0));
        InvisibleButton(O("##FloatBtnHit"), ImVec2(totalWidth, totalHeight));
        isHovered = IsItemHovered();
        
        float targetHover = isHovered ? 1.0f : 0.0f;
        hoverAnim += (targetHover - hoverAnim) * io.DeltaTime * 10.0f;
        
        float currentRadius = buttonRadius + hoverAnim * 4.0f;
        
        // Shadow
        dl->AddCircleFilled(ImVec2(center.x + 2, center.y + 3), currentRadius, IM_COL32(0, 0, 0, 80), 32);
        // Outer glow ring (rainbow)
        if (hoverAnim > 0.01f)
            dl->AddCircleFilled(center, currentRadius + 6.0f * hoverAnim, IM_COL32(255,110,199, (int)(40 * hoverAnim)), 32);
        // Dark indigo fill
        dl->AddCircleFilled(center, currentRadius, IM_COL32(15, 15, 30, 255), 32);
        // Rainbow spinning arcs (5 segments)
        static float btnRot = 0.0f;
        btnRot += io.DeltaTime * 2.4f;
        const ImU32 BRC[5] = {
            IM_COL32(255,110,199,220), IM_COL32(167,139,250,220),
            IM_COL32( 96,165,250,220), IM_COL32( 52,211,153,220),
            IM_COL32(251,191, 36,220)
        };
        for (int bi = 0; bi < 5; bi++) {
            float a0 = btnRot + bi * (6.2832f / 5.f);
            float a1 = a0 + (6.2832f / 5.f) * 0.6f;
            dl->PathArcTo(center, currentRadius - 4, a0, a1, 10);
            dl->PathStroke(BRC[bi], 0, 2.0f);
        }
        
        float iconSize = 12.0f;
        ImU32 iconColor = IM_COL32(255, 255, 255, 255);
        
        if (g_menu.isOpen) {
            dl->AddLine(ImVec2(center.x - iconSize, center.y - iconSize), ImVec2(center.x + iconSize, center.y + iconSize), iconColor, 3.5f);
            dl->AddLine(ImVec2(center.x + iconSize, center.y - iconSize), ImVec2(center.x - iconSize, center.y + iconSize), iconColor, 3.5f);
        } else {
            float barW = 14.0f;
            float barH = 3.0f;
            float gap = 6.0f;
            dl->AddRectFilled(ImVec2(center.x - barW, center.y - gap - barH), ImVec2(center.x + barW, center.y - gap), iconColor, 2.0f);
            dl->AddRectFilled(ImVec2(center.x - barW, center.y - barH * 0.5f), ImVec2(center.x + barW, center.y + barH * 0.5f), iconColor, 2.0f);
            dl->AddRectFilled(ImVec2(center.x - barW, center.y + gap), ImVec2(center.x + barW, center.y + gap + barH), iconColor, 2.0f);
        }
        
        
        if (IsItemActive() && IsMouseDragging(0)) {
            isDragging = true;
            buttonPos.x += io.MouseDelta.x;
            buttonPos.y += io.MouseDelta.y;
            buttonPos.x = ImClamp(buttonPos.x, 0.0f, (float)Width - totalWidth);
            buttonPos.y = ImClamp(buttonPos.y, 0.0f, (float)Height - totalHeight);
        }
        
        if (IsItemHovered() && IsMouseReleased(0) && !isDragging) {
            g_menu.isOpen = !g_menu.isOpen;
        }
        
        if (!IsItemActive()) isDragging = false;
    }
    End();
    
    PopStyleVar(2);
    PopStyleColor();
}


static bool first_time = true;
INLINE void DrawLogin(ImGuiIO& io) {
    if (logged_in) return DrawMenu(io);

    SetNextWindowPos(ImVec2(0, 0));
    SetNextWindowSize(io.DisplaySize);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.04f, 0.04f, 0.06f, 0.96f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus);
    PopStyleColor();

    float cardW = 580;
    float cardH = 420;

    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.14f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 winPos = GetWindowPos();
    
    // Header: dark navy gradient
    DrawGradientRect(dl, winPos, ImVec2(winPos.x + cardW, winPos.y + 110), IM_COL32(0, 20, 40, 255), IM_COL32(0, 35, 60, 255), true);
    dl->AddRectFilled(winPos, ImVec2(winPos.x + cardW, winPos.y + 20), IM_COL32(0, 20, 40, 255), 20.0f, ImDrawFlags_RoundCornersTop);

    // Top cyan accent bar
    dl->AddRectFilled(winPos, ImVec2(winPos.x + cardW, winPos.y + 3), IM_COL32(220, 30, 50, 220), 20.0f, ImDrawFlags_RoundCornersTop);

    // HUD corner brackets on card
    const float cbS = 16.0f; const float cbTh = 2.0f;
    const ImU32 cbC = IM_COL32(220, 30, 50, 200);
    dl->AddLine(winPos, ImVec2(winPos.x + cbS, winPos.y), cbC, cbTh);
    dl->AddLine(winPos, ImVec2(winPos.x, winPos.y + cbS),  cbC, cbTh);
    ImVec2 cbTR = ImVec2(winPos.x + cardW, winPos.y);
    dl->AddLine(ImVec2(cbTR.x - cbS, cbTR.y), cbTR, cbC, cbTh);
    dl->AddLine(cbTR, ImVec2(cbTR.x, cbTR.y + cbS), cbC, cbTh);

    // Rotating HUD ring
    static float loginRot = 0.0f;
    loginRot += ImGui::GetIO().DeltaTime * 1.8f;
    ImVec2 ringC = ImVec2(winPos.x + cardW * 0.5f, winPos.y + 55);
    dl->AddCircle(ringC, 30.0f, IM_COL32(220, 30, 50, 40), 32, 1.0f);
    dl->PathArcTo(ringC, 30.0f, loginRot, loginRot + 2.0f, 20);
    dl->PathStroke(IM_COL32(220, 30, 50, 220), 0, 2.0f);

    SetWindowFontScale(1.4f);
    ImVec2 titleSize = CalcTextSize("Paste Your Key");
    dl->AddText(ImVec2(winPos.x + (cardW - titleSize.x) * 0.5f, winPos.y + 28), IM_COL32(220, 30, 50, 255), "Paste Your Key");
    SetWindowFontScale(1.0f);

    ImVec2 subSize = CalcTextSize("");
    dl->AddText(ImVec2(winPos.x + (cardW - subSize.x) * 0.5f, winPos.y + 72), IM_COL32(200, 25, 45, 200), "");

    SetCursorPosY(130);

    if (!ERROR_MESSAGE.empty()) {
        SetCursorPosX(30);
        PushTextWrapPos(cardW - 30);
        TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ERROR_MESSAGE.c_str());
        PopTextWrapPos();
        Dummy(ImVec2(0, 15));
    }

    if (is_logging_in) {
        SetCursorPosY(180);
        
        static float spinner_angle = 0.0f;
        spinner_angle += io.DeltaTime * 5.0f;

        float spinner_size = 40.0f;
        ImVec2 spinnerCenter = ImVec2(winPos.x + cardW * 0.5f, winPos.y + 220);

        for (int i = 0; i < 12; i++) {
            float angle = spinner_angle + (i * PI * 2.0f / 12.0f);
            float alpha = (float)(12 - i) / 12.0f;
            ImVec2 dotPos = ImVec2(
                spinnerCenter.x + cosf(angle) * spinner_size,
                spinnerCenter.y + sinf(angle) * spinner_size
            );
            dl->AddCircleFilled(dotPos, 6.0f, IM_COL32(100, 180, 255, (int)(alpha * 255)));
        }

        ImVec2 loadingSize = CalcTextSize("Authenticating...");
        SetCursorPosX((cardW - loadingSize.x) * 0.5f);
        SetCursorPosY(290);
        TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "Authenticating...");
    } else {
        SetCursorPosY(160);
        
        ImVec2 infoSize = CalcTextSize("Copy your license key and tap login");
        SetCursorPosX((cardW - infoSize.x) * 0.5f);
        TextColored(ImVec4(0.55f, 0.55f, 0.6f, 1.0f), "Copy your license key and tap login");
        
        Dummy(ImVec2(0, 50));
        
        bool AutoLogin = first_time && !persistent_string["key"].empty();
        
        SetCursorPosX(40);
        PushStyleColor(ImGuiCol_Button,        ImVec4(0.0f, 0.35f, 0.55f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.50f, 0.72f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.0f, 0.65f, 0.90f, 1.0f));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        if (AutoLogin || Button("[ INITIALIZE HUD ]", ImVec2(cardW - 80, 65))) {
            JNIEnv* env;
            jint getEnvResult = VM->GetEnv((void**)&env, JNI_VERSION_1_6);
            if (getEnvResult == JNI_EDETACHED) {
                if (VM->AttachCurrentThread(&env, nullptr) != 0) ERROR_MESSAGE = O("Failed to attach thread to JVM");
            } else if (getEnvResult != JNI_OK) {
                ERROR_MESSAGE = O("Failed to get JNIEnv");
            } else {
                std::thread([](std::string androidId, std::string key) {
                    Login(androidId, key);
                }, getAndroidID(env), AutoLogin ? persistent_string["key"] : getClipboard(env)).detach();
            }

            first_time = false;
        }
        
        PopStyleVar();
        PopStyleColor(3);
        
        Dummy(ImVec2(0, 35));
        
        ImVec2 helpSize = CalcTextSize("Your key will be read from clipboard");
        SetCursorPosX((cardW - helpSize.x) * 0.5f);
        TextColored(ImVec4(0.42f, 0.42f, 0.48f, 1.0f), "Your key will be read from clipboard");
    }

    End();
    PopStyleVar(3);
    PopStyleColor();
    
    End();
}


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
    io.Fonts->AddFontDefault(&font_cfg);
    
    if (persistent_float["fLineThick"]   < 0.05f) persistent_float["fLineThick"]   = 2.0f;
    if (persistent_float["fPredAlpha"]   < 0.05f) persistent_float["fPredAlpha"]   = 0.8f;

    ImGui_ImplAndroid_Init();
    ImGui_ImplOpenGL3_Init(O("#version 300 es"));

    bImguiSetup = true;
}

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
        DrawFloatingButton(io);
    {
    DrawLiveStatusOverlay(io);
    time_t elapsed = time(nullptr) - g_LoadTime;
    int h = elapsed / 3600;
    int m = (elapsed % 3600) / 60;
    int s = elapsed % 60;

    char timerStr[12];
    snprintf(timerStr, sizeof(timerStr), "%02d:%02d:%02d", h, m, s);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImFont* font = ImGui::GetIO().Fonts->Fonts[0];
    float fontSize = 18.0f;

    ImVec2 ts = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, timerStr);
    float x = Width - ts.x - 16.0f;
    float y = 16.0f;

    // Shadow
    dl->AddText(font, fontSize, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 150), timerStr);
    // Teks kuning
    dl->AddText(font, fontSize, ImVec2(x, y), IM_COL32(255, 255, 255, 255), timerStr);
    }
        DrawMenu(io);
    } else {
        DrawLogin(io);
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
