#pragma once

#include <Vector/Vectors.h>
#include <imgui/imgui.h>

ImFont* fontShotFound = nullptr; // ← Deklarasi global

void InitImGui() {
    ImGuiIO& io = ImGui::GetIO();
    fontShotFound = io.Fonts->AddFontFromFileTTF("font/1.ttf", 15.0f);
}

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

#include "8bp/FrictionProperties.h"
#include "ButtonClicker.h"
#include "8bp/inc/AutoPlay.h"
#include "8bp/inc/AutoAim.h"
#include "logo.h"
#include "on.h"
#include "off.h"

static float g_sideBtnsY      = 0.0f;
static bool g_GameReady = false; // Penanda game sudah siap
static float g_sideBtnsX      = 0.0f;
static bool g_aqCounting = false;
static std::chrono::steady_clock::time_point g_aqLastCall;
static std::chrono::steady_clock::time_point g_aqCountdownStart;
static bool DEBUG_BYPASS_LOGIN = false;
static const int64_t EXPIRY_TS = O(1784426596);

static const char* L(const char* en, const char* ar);


struct MenuState {
    bool  isOpen        = false;
    int   currentTab    = 0;
    float sidebarWidth = 750.0f;
    float menuAlpha     = 0.0f;
    float menuScale     = 0.92f;
    bool  hideForCapture = false;
};
static MenuState g_menu;


struct ShotApprovalState {
    bool  active      = false;   
    bool  approved    = false;   
    bool  turnHandled = false;
    int   rejectCount = 0;
    float accuracy    = 0.0f;
    float aiPower     = 0.75f;   
    bool  hasShot     = false;   
    std::chrono::steady_clock::time_point shownAt;
};
static ShotApprovalState g_shotApproval;


static bool ShotApprovalGate(int stateId) {
    ShotApprovalState& A = g_shotApproval;


    bool ourTurn = (stateId == 4);
    if (!ourTurn) {

        A.active      = false;
        A.approved    = false;
        A.turnHandled = false;
        A.rejectCount = 0;
        return true; 
    }

    if (A.approved) return true;

    if (!A.hasShot) {
        A.active = false;
        return false;
    }

    if (!A.active) {
        A.active  = true;
        A.shownAt = std::chrono::steady_clock::now();
    }

    auto now = std::chrono::steady_clock::now();
    long elapsed = (long)std::chrono::duration_cast<std::chrono::milliseconds>(now - A.shownAt).count();
    if (elapsed >= 3000) {
        A.approved = true;
        A.active   = false;
        return true;
    }

    return false;
}



static float EaseOutBack(float x){ const float c1=1.70158f, c3=c1+1.0f; return 1.0f + c3*powf(x-1,3) + c1*powf(x-1,2); }
static float EaseOutQuart(float x){ return 1.0f - powf(1.0f - x, 4.0f); }

static void DrawGradientRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 c1, ImU32 c2, bool horizontal=true){
    if (horizontal) dl->AddRectFilledMultiColor(a,b,c1,c2,c2,c1);
    else            dl->AddRectFilledMultiColor(a,b,c1,c1,c2,c2);
}


#define COL_BG_DEEP      IM_COL32(11, 18, 32, 250)
#define COL_BG_DARK      IM_COL32(16, 26, 44, 250)
#define COL_PANEL        IM_COL32(22, 32, 52, 255)
#define COL_PANEL_SOFT   IM_COL32(28, 40, 62, 255)
//#define COL_GOLD         IM_COL32(212, 175, 75, 255)
//#define COL_GOLD_BRIGHT  IM_COL32(245, 210, 110, 255)
//#define COL_GOLD_DEEP    IM_COL32(160, 120, 40, 255)
#define COL_TEXT         IM_COL32(235, 238, 245, 255)
#define COL_TEXT_DIM     IM_COL32(160, 168, 185, 255)
#define COL_TEXT_FAINT   IM_COL32(120, 128, 145, 255)
#define COL_LINE         IM_COL32(60, 75, 100, 255)
//#define NEON_GOLD       IM_COL32(255, 215, 0, 255)

#define COL_GOLD         IM_COL32(46, 204, 113, 255)
#define COL_GOLD_BRIGHT  IM_COL32(76, 255, 150, 255)
#define COL_GOLD_DEEP    IM_COL32(30, 150, 80, 255)
#define NEON_GOLD        IM_COL32(57, 255, 20, 255)



static void Icon_Gear(ImDrawList* dl, ImVec2 c, ImU32 col){
    const float r1 = 12.0f, r2 = 8.0f;
    ImVector<ImVec2> pts;
    for (int i = 0; i < 16; i++){
        float a = (IM_PI * 2.0f / 16.0f) * i;
        float r = (i % 2 == 0) ? r1 : r2;
        pts.push_back(ImVec2(c.x + cosf(a)*r, c.y + sinf(a)*r));
    }
    dl->AddConvexPolyFilled(pts.Data, pts.Size, col);
    dl->AddCircleFilled(c, 4.0f, IM_COL32(14, 22, 38, 255), 32);
}
static void Icon_Play(ImDrawList* dl, ImVec2 c, ImU32 col){
    dl->AddCircle(c, 12.0f, col, 48, 1.8f);
    ImVec2 p1(c.x - 4.0f, c.y - 6.5f);
    ImVec2 p2(c.x - 4.0f, c.y + 6.5f);
    ImVec2 p3(c.x + 7.0f, c.y);
    dl->AddTriangleFilled(p1, p2, p3, col);
}
static void Icon_Table(ImDrawList* dl, ImVec2 c, ImU32 col){
    ImVec2 a(c.x - 13.0f, c.y - 8.5f);
    ImVec2 b(c.x + 13.0f, c.y + 8.5f);
    dl->AddRect(a, b, col, 3.0f, 0, 2.0f);
    dl->AddCircleFilled(ImVec2(a.x + 2.0f, a.y + 2.0f), 2.3f, col, 16);
    dl->AddCircleFilled(ImVec2(b.x - 2.0f, b.y - 2.0f), 2.3f, col, 16);
    dl->AddCircleFilled(ImVec2(b.x - 2.0f, a.y + 2.0f), 2.3f, col, 16);
    dl->AddCircleFilled(ImVec2(a.x + 2.0f, b.y - 2.0f), 2.3f, col, 16);
    dl->AddCircleFilled(c, 2.0f, col, 16);
}
static void Icon_Account(ImDrawList* dl, ImVec2 c, ImU32 col){
    dl->AddCircleFilled(ImVec2(c.x, c.y - 4.5f), 5.0f, col, 32);
    ImVector<ImVec2> arc;
    ImVec2 sa(c.x - 9.0f, c.y + 10.5f);
    ImVec2 sb(c.x + 9.0f, c.y + 10.5f);
    for (int i = 0; i <= 18; ++i){
        float t = (float)i / 18.0f;
        float x = sa.x + (sb.x - sa.x) * t;
        float y = c.y + 2.0f + sinf(t * IM_PI) * -6.5f;
        arc.push_back(ImVec2(x, y));
    }
    arc.push_back(sb);
    arc.push_back(sa);
    dl->AddConvexPolyFilled(arc.Data, arc.Size, col);
}
static void Icon_Bell(ImDrawList* dl, ImVec2 c, ImU32 col){
    ImVector<ImVec2> body;
    for (int i = 0; i <= 14; ++i){
        float t = (float)i / 14.0f;
        float ang = IM_PI - t * IM_PI;
        body.push_back(ImVec2(c.x + cosf(ang) * 8.5f, c.y - 3.0f + sinf(ang) * -6.5f));
    }
    body.push_back(ImVec2(c.x + 8.5f, c.y + 6.0f));
    body.push_back(ImVec2(c.x - 8.5f, c.y + 6.0f));
    dl->AddConvexPolyFilled(body.Data, body.Size, col);
    dl->AddRectFilled(ImVec2(c.x - 10.0f, c.y + 5.5f), ImVec2(c.x + 10.0f, c.y + 8.0f), col, 1.5f);
    dl->AddCircleFilled(ImVec2(c.x, c.y + 11.0f), 2.0f, col, 16);
}
static void Icon_Info(ImDrawList* dl, ImVec2 c, ImU32 col){
    dl->AddCircle(c, 11.5f, col, 48, 2.0f);
    dl->AddCircleFilled(ImVec2(c.x, c.y - 4.5f), 2.0f, col, 16);
    dl->AddRectFilled(ImVec2(c.x - 1.8f, c.y - 1.0f), ImVec2(c.x + 1.8f, c.y + 7.5f), col, 1.0f);
}
static void Icon_Camera(ImDrawList* dl, ImVec2 c, ImU32 col){
    dl->AddRectFilled(ImVec2(c.x - 9.0f, c.y - 5.0f), ImVec2(c.x + 9.0f, c.y + 6.5f), col, 2.0f);
    dl->AddRectFilled(ImVec2(c.x - 3.0f, c.y - 7.5f), ImVec2(c.x + 3.0f, c.y - 4.5f), col, 1.0f);
    dl->AddCircleFilled(c, 3.2f, IM_COL32(14, 22, 38, 255), 24);
    dl->AddCircle(c, 3.2f, col, 24, 1.2f);
}
static void DrawTabIcon(ImDrawList* dl, int tab, ImVec2 c, ImU32 col){
    switch (tab){
        case 0: Icon_Gear   (dl, c, col); break;
        case 1: Icon_Play   (dl, c, col); break;
        case 2: Icon_Table  (dl, c, col); break;
        case 3: Icon_Account(dl, c, col); break;
        case 4: Icon_Bell   (dl, c, col); break;
        case 5: Icon_Info   (dl, c, col); break;
        default: Icon_Gear(dl, c, col); break;
    }
}


static const char* CurrentTabTitle(){
    static const char* en[6] = {
        "General",
        "Auto Play",
        "Table",
        "Account",
        "Notifications",
        "About"
    };

    static const char* ar[6] = {
        "ﺕﺍﺩﺍﺪﻋﻻﺍ",
        "ﻲﺋﺎﻘﻠﺘﻟﺍ",
        "ﺔﻟﻭﺎﻄﻟﺍ",
        "ﺏﺎﺴﺤﻟﺍ",
        "ﺕﺍﺭﺎﻌﺷﻻﺍ",
        "ﺕﺎﻣﻮﻠﻌﻤﻟﺍ"
    };

    int t = g_menu.currentTab;
    if (t < 0 || t > 5) t = 0;

    return L(en[t], ar[t]);
}


static void DrawBoldText(ImDrawList* dl, ImVec2 pos, ImU32 col, const char* text){
    const float o = 0.9f;
    dl->AddText(ImVec2(pos.x + o, pos.y),     col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y),     col, text);
    dl->AddText(ImVec2(pos.x,     pos.y + o), col, text);
    dl->AddText(ImVec2(pos.x,     pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y + o), col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x + o, pos.y - o), col, text);
    dl->AddText(ImVec2(pos.x - o, pos.y + o), col, text);
    dl->AddText(pos,                            col, text);
}




static const char* L(const char* en, const char* ar){
    return (persistent_int["iLang"] == 1) ? ar : en;
}




static bool GoldSidebarButton(const char* label, const char* icon, bool selected, float width){
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);

    ImVec2 pos  = window->DC.CursorPos;
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

    // ===== FIX: WARNA HIJAU =====
    if (selected) {
        dl->AddRectFilledMultiColor(bb.Min, bb.Max,
            IM_COL32(30, 150, 80, 255),     // Hijau tua
            IM_COL32(20, 100, 55, 255),      // Hijau gelap
            IM_COL32(15, 70, 40, 255),       // Hijau sangat gelap
            IM_COL32(25, 120, 65, 255));     // Hijau sedang
        dl->AddRect(bb.Min, bb.Max, COL_GOLD_BRIGHT, 12.0f, 0, 1.8f);

        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(46, 204, 113, 35), 12.0f);

        dl->AddRectFilled(ImVec2(bb.Max.x - 4, bb.Min.y + 10),
                          ImVec2(bb.Max.x,     bb.Max.y - 10), COL_GOLD_BRIGHT, 2.0f);
    } else if (t > 0.01f) {
        dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(46, 204, 113, (int)(95*t)), 12.0f);
    }

    ImU32 textCol = selected ? IM_COL32(220, 255, 220, 255) : IM_COL32(160, 200, 180, (int)(210+45*t));
    ImU32 iconCol = selected ? COL_GOLD_BRIGHT : IM_COL32(46, 204, 113, (int)(190+65*t));

    int tabIdx = 0;
    if (icon && icon[0] >= '0' && icon[0] <= '9') tabIdx = icon[0] - '0';
    ImVec2 iconCenter = ImVec2(bb.Min.x + 36.0f, bb.Min.y + size.y * 0.5f);
    dl->AddCircleFilled(iconCenter, 21.0f,
        selected ? IM_COL32(10, 50, 25, 255) : IM_COL32(22, 32, 52, 255), 32);
    dl->AddCircle(iconCenter, 21.0f,
        selected ? COL_GOLD_BRIGHT : IM_COL32(46, 204, 113, 200), 32, 1.4f);
    DrawTabIcon(dl, tabIdx, iconCenter, iconCol);

    ImVec2 textSize = CalcTextSize(label);
    ImVec2 textPos  = ImVec2(bb.Min.x + 78.0f, bb.Min.y + (size.y - textSize.y) * 0.5f);
    DrawBoldText(dl, textPos, textCol, label);

    return pressed;
}

static bool IsExpired() {
    return (int64_t)time(nullptr) >= EXPIRY_TS;
}

INLINE void DrawExpired(ImGuiIO& io) {
    float winW = g_menu.sidebarWidth;

    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    SetNextWindowSize(ImVec2(winW, 0), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(21, 21, 21, 255));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(30.0f, 30.0f));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##ExpiredWin"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
              ImGuiWindowFlags_AlwaysAutoResize)) {

        SetWindowFontScale(1.6f);
        ImVec2 titleSz = CalcTextSize(O("MOD EXPIRED"));
        SetCursorPosX((winW - 60.0f - titleSz.x) * 0.5f);
        TextColored(ImVec4(1.0f, 0.1f, 0.1f, 1.0f), "%s", O("MOD EXPIRED"));
        SetWindowFontScale(1.0f);

        Dummy(ImVec2(0, 16));

        PushTextWrapPos(GetCursorPosX() + winW - 60.0f);
        TextColored(ImVec4(0.85f, 0.85f, 0.90f, 1.0f), "%s",
            O("Beta Version Expired. Update on our Telegram t.me/xabi666"));
        PopTextWrapPos();

        Dummy(ImVec2(0, 10));
    }
    End();
    PopStyleVar(3);
    PopStyleColor();
}


static bool GoldToggle(const char* label, const char* sub, bool* v){
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

    ImU32 bgCol = hovered ? IM_COL32(35, 50, 75, 200) : IM_COL32(26, 38, 58, 180);
    dl->AddRectFilled(bb.Min, bb.Max, bgCol, 12.0f);
    dl->AddRect(bb.Min, bb.Max, IM_COL32(55, 70, 95, 120), 12.0f, 0, 1.0f);

    ImVec2 ts = CalcTextSize(label);
    dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12), COL_TEXT, label);
    if (sub && *sub) dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12 + ts.y + 4), COL_TEXT_FAINT, sub);

    ImVec2 togPos = ImVec2(bb.Max.x - tw - 18.0f, bb.Min.y + (rowH - th) * 0.5f);
    ImVec2 togEnd = ImVec2(togPos.x + tw, togPos.y + th);

    // ===== FIX: Warna aktif jadi HIJAU =====
    ImVec4 off = ImVec4(0.18f, 0.22f, 0.30f, 1.0f);
    ImVec4 on  = ImVec4(0.18f, 0.80f, 0.44f, 1.0f);  // Hijau zamrud (#2ECC71)
    ImVec4 cur = ImLerp(off, on, t);
    dl->AddRectFilled(togPos, togEnd, ImColor(cur), r);
    
    // Border hijau saat aktif
    if (t > 0.05f) dl->AddRect(togPos, togEnd, IM_COL32(46, 204, 113, (int)(255*t)), r, 0, 1.2f);

    float kx = togPos.x + r + (tw - th) * t;
    float ky = togPos.y + r;
    float kr = r - 4.0f;
    dl->AddCircleFilled(ImVec2(kx, ky + 1), kr + 1.5f, IM_COL32(0,0,0,60));
    dl->AddCircleFilled(ImVec2(kx, ky),     kr,        IM_COL32(255,255,255,255));

    return pressed;
}


static bool GoldSliderFloat(const char* label, const char* sub, float* v, float vmin, float vmax, const char* fmt){
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
    dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(26, 38, 58, 180), 12.0f);
    dl->AddRect(bb.Min, bb.Max, IM_COL32(55, 70, 95, 120), 12.0f, 0, 1.0f);

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
    if (hovered && g.IO.MouseDown[0]) {
        SetActiveID(id, window);
    }
    if (g.ActiveId == id) {
        if (g.IO.MouseDown[0]) {
            float nx = (g.IO.MousePos.x - trackA.x) / trackW;
            nx = ImClamp(nx, 0.0f, 1.0f);
            float nv = vmin + nx * (vmax - vmin);
            if (nv != *v) { *v = nv; changed = true; }
        } else {
            ClearActiveID();
        }
    }


    dl->AddRectFilled(trackA, trackB, IM_COL32(50, 60, 80, 255), 2.0f);
    ImVec2 fillEnd = ImVec2(trackA.x + trackW * tnorm, trackB.y);
    dl->AddRectFilledMultiColor(trackA, fillEnd, COL_GOLD_DEEP, COL_GOLD_BRIGHT, COL_GOLD_BRIGHT, COL_GOLD_DEEP);

    float kx = trackA.x + trackW * tnorm;
    float ky = (trackA.y + trackB.y) * 0.5f;
    dl->AddCircleFilled(ImVec2(kx, ky + 1), 10.0f, IM_COL32(0,0,0,90));
    dl->AddCircleFilled(ImVec2(kx, ky),     9.0f,  COL_GOLD_BRIGHT);
    dl->AddCircle(      ImVec2(kx, ky),     9.0f,  IM_COL32(255,240,200,255), 0, 1.5f);


    char buf[32]; snprintf(buf, sizeof(buf), fmt, *v);
    ImVec2 vs = CalcTextSize(buf);
    dl->AddText(ImVec2(bb.Max.x - 18 - vs.x, ky - vs.y * 0.5f), COL_GOLD_BRIGHT, buf);

    return changed;
}


static bool GoldCombo(const char* label, const char* sub, int* val, const char* items_z){
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return false;
    ImVec2 pos = window->DC.CursorPos;
    float rowH = 64.0f;
    ImVec2 size = ImVec2(GetContentRegionAvail().x, rowH);
    const ImRect bb(pos, pos + size);

    ImDrawList* dl = window->DrawList;
    dl->AddRectFilled(bb.Min, bb.Max, IM_COL32(26, 38, 58, 180), 12.0f);
    dl->AddRect(bb.Min, bb.Max, IM_COL32(55, 70, 95, 120), 12.0f, 0, 1.0f);

    ImVec2 ts = CalcTextSize(label);
    dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12), COL_TEXT, label);
    if (sub && *sub) dl->AddText(ImVec2(bb.Min.x + 18, bb.Min.y + 12 + ts.y + 4), COL_TEXT_FAINT, sub);

    SetCursorScreenPos(ImVec2(bb.Max.x - 180 - 18, bb.Min.y + (rowH - 30) * 0.5f));
    PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(35, 50, 75, 255));
    PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(45, 62, 90, 255));
    PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(50, 68, 100, 255));
    PushStyleColor(ImGuiCol_Button,         COL_GOLD_DEEP);
    PushStyleColor(ImGuiCol_Text,           COL_GOLD_BRIGHT);
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



INLINE void DrawAutoQueue() {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        static std::chrono::steady_clock::time_point last_call_time;
        static std::chrono::steady_clock::time_point countdown_start;
        static bool counting = false;

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
        int remaining_ms = 3000 - elapsed;

        if (remaining_ms <= 0) {
            if (sharedMenuManager.getMenuStateId() == 13) PopMenuState(13);
            StartLastMatch();
            counting = false;
            return;
        }

        SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Always);
        PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.10f, 0.10f, 0.12f, 0.98f));
        PushStyleVar(ImGuiStyleVar_WindowRounding, 20.0f);

        if (Begin(O("##AutoQueue"), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
            ImDrawList* dl = GetWindowDrawList();
            ImVec2 winPos = GetWindowPos();
            ImVec2 winSize = GetWindowSize();
            
            DrawGradientRect(dl, winPos, ImVec2(winPos.x + winSize.x, winPos.y + 70), IM_COL32(40, 100, 180, 255), IM_COL32(60, 140, 200, 255), true);
            dl->AddRectFilled(winPos, ImVec2(winPos.x + winSize.x, winPos.y + 20), IM_COL32(40, 100, 180, 255), 20.0f, ImDrawFlags_RoundCornersTop);
            
            ImVec2 titleSize = CalcTextSize(O("Auto Queue"));
            dl->AddText(ImVec2(winPos.x + (winSize.x - titleSize.x) * 0.5f, winPos.y + 22), IM_COL32(255, 255, 255, 255), O("Auto Queue"));

            SetCursorPosY(90);
            float font_scale = 3.5f;
            SetWindowFontScale(font_scale);

            std::string count_str = std::to_string((remaining_ms / 1000) + 1);
            auto text_size = CalcTextSize(count_str.c_str());
            SetCursorPosX((winSize.x - text_size.x) * 0.5f);
            TextColored(ImVec4(0.35f, 0.7f, 1.0f, 1.0f), "%s", count_str.c_str());

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

static void DrawOrnateFrame(ImDrawList* dl, ImVec2 a, ImVec2 b);

INLINE void DrawShotApprovalPrompt(ImGuiIO& io) {
    if (g_menu.hideForCapture) return;
    if (!g_shotApproval.active)  return;
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

    ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoTitleBar|
                          ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|
                          ImGuiWindowFlags_NoFocusOnAppearing;
    if (Begin(O("##ShotApproval"), nullptr, wf)) {
        ImDrawList* dl = GetWindowDrawList();
        ImVec2 wp = GetWindowPos();
        ImVec2 ws = GetWindowSize();
        ImVec2 wmax = ImVec2(wp.x + ws.x, wp.y + ws.y);


        dl->AddRectFilled(ImVec2(wp.x + 3, wp.y + 5), ImVec2(wmax.x + 3, wmax.y + 6), IM_COL32(0,0,0,110), 16.0f);
        dl->AddRectFilled(wp, wmax, COL_BG_DEEP, 16.0f);
        dl->AddRectFilled(wp, wmax, IM_COL32(245, 210, 110, 14), 16.0f);
        dl->AddRect(wp, wmax, COL_GOLD, 16.0f, 0, 2.0f);
        DrawOrnateFrame(dl, wp, wmax);


        ImVec2 ha = ImVec2(wp.x + 14, wp.y + 14);
        ImVec2 hb = ImVec2(wmax.x - 14, wp.y + 64);
        DrawGradientRect(dl, ha, hb, COL_GOLD_DEEP, COL_GOLD_BRIGHT, true);
        dl->AddRect(ha, hb, COL_GOLD_BRIGHT, 8.0f, 0, 1.2f);
        const char* title = L("Approve the shot?", "؟ﺏﺮﻀﻟﺍ ﺪﻳﺮﺗ ﻞﻫ");
        ImVec2 tz = CalcTextSize(title);
        DrawBoldText(dl, ImVec2(wp.x + (ws.x - tz.x) * 0.5f, ha.y + (50 - tz.y) * 0.5f), IM_COL32(18,24,38,255), title);


        char info[128];
        snprintf(info, sizeof(info), "%s %d %s",
                 L("Auto-accept in", "ﺪﻌﺑ ﺔﻴﺋﺎﻘﻠﺗ ﺔﻘﻓﺍﻮﻣ"),
                 remaining,
                 L("s", "ﺔﻴﻧﺎﺛ"));
        ImVec2 iz = CalcTextSize(info);
        dl->AddText(ImVec2(wp.x + (ws.x - iz.x) * 0.5f, wp.y + 72), COL_TEXT_DIM, info);


        float acc = A.accuracy;
        if (acc < 0.0f)   acc = 0.0f;
        if (acc > 100.0f) acc = 100.0f;


        ImU32 accCol;
        if (acc >= 85.0f)      accCol = IM_COL32( 80, 220, 120, 255);
        else if (acc >= 60.0f) accCol = IM_COL32(245, 210, 110, 255);
        else                   accCol = IM_COL32(225, 110,  90, 255);


        char accTxt[96];
        snprintf(accTxt, sizeof(accTxt), "%s %d%%",
                 L("Accuracy", "ﺐﻳﻮﺼﺘﻟﺍ ﺔﻗﺩ"), (int)(acc + 0.5f));
        ImVec2 accZ = CalcTextSize(accTxt);
        DrawBoldText(dl, ImVec2(wp.x + (ws.x - accZ.x) * 0.5f, wp.y + 96), accCol, accTxt);


        float barPad = 26.0f;
        ImVec2 ba = ImVec2(wp.x + barPad, wp.y + 122);
        ImVec2 bb = ImVec2(wmax.x - barPad, wp.y + 140);
        dl->AddRectFilled(ba, bb, IM_COL32(18, 26, 42, 255), 9.0f);
        float fillW = (bb.x - ba.x) * (acc / 100.0f);
        if (fillW > 1.0f)
            dl->AddRectFilled(ba, ImVec2(ba.x + fillW, bb.y), accCol, 9.0f);
        dl->AddRect(ba, bb, COL_GOLD, 9.0f, 0, 1.3f);



        float pad = 16.0f, gap = 12.0f;
        float bw = (ws.x - pad * 2 - gap) * 0.5f;
        float bh = 50.0f;
        float by = ws.y - bh - 16.0f;

        PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);


        SetCursorPos(ImVec2(pad, by));
        PushStyleColor(ImGuiCol_Button,        (ImVec4)ImColor(COL_GOLD_DEEP));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.83f, 0.68f, 0.29f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.42f, 0.16f, 1.0f));
        PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
        if (Button(L("Accept", "ﻝﻮﺒﻗ"), ImVec2(bw, bh))) {
            A.approved = true;
            A.active   = false;
        }
        PopStyleColor(4);


        SetCursorPos(ImVec2(pad + bw + gap, by));
        PushStyleColor(ImGuiCol_Button,        ImVec4(0.62f, 0.18f, 0.18f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.24f, 0.24f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.13f, 0.13f, 1.0f));
        PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
        if (Button(L("Reject", "ﺾﻓﺭ"), ImVec2(bw, bh))) {
            A.approved = false;
            A.rejectCount++;
            A.shownAt  = std::chrono::steady_clock::now();
        }
        PopStyleColor(4);

        PopStyleVar();
    }
    End();

    PopStyleVar(3);
    PopStyleColor();
}

static void DrawToggleButton(); // forward declaration — defined after DrawFloatingButton

INLINE void DrawESP(ImDrawList* draw) {
    if (!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) {
        if (!sharedGameManager) return;
        UpdateScreenTable();
        sharedDirector = F(ptr, libmain + O(0x4f06288));   if (!sharedDirector) return;
        sharedUserInfo = F(ptr, libmain + O(0x4e9feb8));   if (!sharedUserInfo) return;
        F(bool, sharedUserInfo + 0x340) = true;
        sharedMainManager = F(ptr, libmain + O(0x4dde3e0));if (!sharedMainManager) return;
        sharedMenuManager = F(ptr, libmain + O(0x4dfe838));if (!sharedMenuManager) return;
        MainStateManager mainStateManager = sharedMainManager.mStateManager;
        if (!mainStateManager) return;
        if (!mainStateManager.isInGame()) {
            if (persistent_bool[O("bAutoQueue")]) { if (!sharedMenuManager.isInQueue()) DrawAutoQueue(); }
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
                draw->AddCircle(ImVec2(sp.x, sp.y), 40, WHITE, 0, 3.f);
            }
        }
        GameStateManager gameStateManager = sharedGameManager.mStateManager;
        if (!gameStateManager) return;
        auto stateId = gameStateManager.getCurrentStateId();

// ========== BACA MODE HANYA SAAT IDLE (JANGAN OVERWRITE SCAN STATE) ==========

        // Cek 2 kondisi: Status ON, DAN user sudah sengaja menekan tombol (GameReady)
        if (persistent_bool[O("bAutoPlay")]) {
         //   DrawToggleButton();
            AutoPlay::Update(); // FIX: was never called — toggle button had no effect
        }

        if (persistent_bool[O("bAutoAim")]) {
            AutoAim::Draw(); // FIX: was never called — toggle button had no effect
        }

    //    AutoPlay::UpdateScanMode();

        if (stateId == 4) {
    //gPrediction->forceFullSimulation = true;
    gPrediction->determineShotResult(false);
        }
        if (stateId == 6 || stateId == 7 || stateId == 8) return;

        if (persistent_bool[O("bESP_DrawPocketsShotState")]) {
            for (int i = 0; i < 6; i++) {
                if (Prediction::pocketStatus[i]) {
                    auto sp = WorldToScreen(pockets[i]);
                    draw->AddCircle(ImVec2(sp.x, sp.y), 40, GREEN, 0, 5.f);
                }
            }
        }

        int lineStyle = persistent_int["iLineStyle"];

         if (persistent_bool[O("bESP_DrawPredictionLine")]) {
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

static void DrawToggleButton() {
    ImGuiIO& io = GetIO();

    static GLuint play_on_tex  = LoadTextureFromMemory(play_on_png,  play_on_png_len);
    static GLuint play_off_tex = LoadTextureFromMemory(play_off_png, play_off_png_len);

    float button_size   = 80.f;
    float winPadX       = GetStyle().WindowPadding.x;
    float winPadY       = GetStyle().WindowPadding.y;
    float windowWidth   = button_size + winPadX * 2.0f;
    float windowHeight  = button_size + winPadY * 2.0f;

    const float rightMargin  = 20.0f;
    float fixedX = io.DisplaySize.x - rightMargin - windowWidth;

    if (g_sideBtnsY == 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 20.0f - windowHeight;

    SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
    PushStyleColor(ImGuiCol_Border,   IM_COL32(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 99.0f);

    if (Begin(O("##ToggleBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
              ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {

        ImVec2 pos    = GetCursorScreenPos();
        ImVec2 size(button_size, button_size);
        ImVec2 center(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);

        bool clicked = InvisibleButton(O("##TglBtnHit"), size);

        // Pilih tekstur berdasarkan status AutoPlay
        GLuint tex = AutoPlay::bAutoPlaying ? play_on_tex : play_off_tex;

        float r = size.x * 0.5f;
        ImDrawList* dl = GetWindowDrawList();
        dl->AddImage((void*)(intptr_t)tex,
            ImVec2(center.x - r, center.y - r),
            ImVec2(center.x + r, center.y + r));

        // Vertical-only drag (Geser tombol)
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsY += io.MouseDelta.y;
            g_sideBtnsY = ImClamp(g_sideBtnsY, 0.0f, io.DisplaySize.y - windowHeight);
            SetWindowPos(ImVec2(fixedX, g_sideBtnsY), ImGuiCond_Always);
        }

        // Logika Klik (Toggle ON/OFF)
        if (clicked) {
            AutoPlay::bAutoPlaying = !AutoPlay::bAutoPlaying;
            if (AutoPlay::bAutoPlaying) AutoPlay::ClearState();
        }
    }
    End();
    PopStyleVar();
    PopStyleColor(2);
}


static void DrawTitleFrame(ImDrawList* dl, ImVec2 winPos, float winW){

    ImVec2 a = ImVec2(winPos.x + 40, winPos.y + 10);
    ImVec2 b = ImVec2(winPos.x + winW - 40, winPos.y + 56);
    dl->AddRectFilled(a, b, IM_COL32(8, 14, 26, 255), 8.0f);
    dl->AddRect(a, b, COL_GOLD, 8.0f, 0, 1.5f);

    float cs = 10.0f;
    dl->AddLine(ImVec2(a.x, a.y+cs), ImVec2(a.x+cs, a.y), COL_GOLD_BRIGHT, 2.0f);
    dl->AddLine(ImVec2(b.x, a.y+cs), ImVec2(b.x-cs, a.y), COL_GOLD_BRIGHT, 2.0f);
    dl->AddLine(ImVec2(a.x, b.y-cs), ImVec2(a.x+cs, b.y), COL_GOLD_BRIGHT, 2.0f);
    dl->AddLine(ImVec2(b.x, b.y-cs), ImVec2(b.x-cs, b.y), COL_GOLD_BRIGHT, 2.0f);


    const char* t = CurrentTabTitle();
    ImVec2 ts = CalcTextSize(t);
    ImVec2 tp = ImVec2(winPos.x + (winW - ts.x) * 0.5f, a.y + (46 - ts.y) * 0.5f);
    DrawBoldText(dl, tp, COL_GOLD_BRIGHT, t);
}


static void DrawOrnateFrame(ImDrawList* dl, ImVec2 a, ImVec2 b){
    dl->AddRect(a, b, COL_GOLD, 12.0f, 0, 2.0f);
    float L_ = 28.0f;

    dl->AddLine(ImVec2(a.x, a.y + L_), ImVec2(a.x + L_, a.y), COL_GOLD_BRIGHT, 3.0f);
    dl->AddLine(ImVec2(b.x - L_, a.y), ImVec2(b.x, a.y + L_), COL_GOLD_BRIGHT, 3.0f);
    dl->AddLine(ImVec2(a.x, b.y - L_), ImVec2(a.x + L_, b.y), COL_GOLD_BRIGHT, 3.0f);
    dl->AddLine(ImVec2(b.x - L_, b.y), ImVec2(b.x, b.y - L_), COL_GOLD_BRIGHT, 3.0f);
}


static void DrawSidebar(float sidebarW, float winH, ImVec2 winPos){
    ImDrawList* dl = GetWindowDrawList();

    ImVec2 a = ImVec2(winPos.x + 16,            winPos.y + 80);
    ImVec2 b = ImVec2(winPos.x + sidebarW + 16, winPos.y + winH - 20);
    dl->AddRectFilled(a, b, IM_COL32(14, 22, 38, 255), 14.0f);
    dl->AddRect(a, b, IM_COL32(50, 65, 90, 200), 14.0f, 0, 1.0f);

    // ===== TAMBAH: Aksen hijau di atas sidebar =====
    dl->AddRectFilled(ImVec2(winPos.x + 16, winPos.y + 78), 
                      ImVec2(winPos.x + sidebarW + 16, winPos.y + 81), 
                      COL_GOLD, 4.0f);

    SetCursorPos(ImVec2(22, 96));
    BeginGroup();

    struct Tab { const char* en; const char* ar; const char* icon; };
    Tab tabs[] = {
        { "General",   "ﺕﺍﺩﺍﺪﻋﻻﺍ",          "0" },
        { "Auto Play", "ﻲﺋﺎﻘﻠﺘﻟ", "1" }, 
        { "Table",     "ﺔﻟﻭﺎﻄﻟﺍ",        "2" },
        { "Account",   "ﺏﺎﺴﺤﻟﺍ",         "3" },
        { "Notify",    "ﺕﺍﺭﺎﻌﺷﻹﺍ",      "4" }, 
        { "About",     "ﻝﻮﺣ",            "5" },
    };
    int n = (int)(sizeof(tabs)/sizeof(tabs[0]));
    for (int i = 0; i < n; i++) {
        SetCursorPosX(22);
        if (GoldSidebarButton(L(tabs[i].en, tabs[i].ar), tabs[i].icon, g_menu.currentTab == i, sidebarW))
            g_menu.currentTab = i;
        Dummy(ImVec2(0, 6));
    }
    EndGroup();
}

static void DrawContentArea(float sidebarW, float winW, float winH, ImVec2 winPos){
    bool need_save = false;
    ImDrawList* dl = GetWindowDrawList();

    ImVec2 a = ImVec2(winPos.x + sidebarW + 32, winPos.y + 80);
    ImVec2 b = ImVec2(winPos.x + winW - 16,     winPos.y + winH - 20);
    dl->AddRectFilled(a, b, IM_COL32(14, 22, 38, 255), 14.0f);
    dl->AddRect(a, b, IM_COL32(50, 65, 90, 200), 14.0f, 0, 1.0f);

    GameSpeed::Draw();
    
    const char* titlesEn[] = { "General","Auto Play","Table","Account","Notifications","About" };
    const char* titlesAr[] = { "ﺕﺍﺩﺍﺪﻋﻻﺍ","ﻲﺋﺎﻘﻠﺘﻟﺍ","ﺔﻟﻭﺎﻄﻟﺍ","ﺏﺎﺴﺤﻟﺍ","ﺕﺍﺭﺎﻌﺷﻹﺍ","ﻝﻮﺣ" };
    int idx = g_menu.currentTab;
    dl->AddText(ImVec2(a.x + 22, a.y + 18), COL_GOLD_BRIGHT, L(titlesEn[idx], titlesAr[idx]));
    dl->AddLine(ImVec2(a.x + 22, a.y + 50), ImVec2(b.x - 22, a.y + 50), IM_COL32(55,70,95,180), 1.0f);
    // ── EXP Key di pojok kanan atas (sebelah kanan judul) ──
const char* expText = (g_ExpTime.empty() || g_ExpTime == "N/A" || g_ExpTime == "Lifetime") 
                      ? "EXP : Lifetime" 
                      : "EXP : %s";

// Hitung posisi teks agar ada di sebelah kanan garis
ImVec2 textSize = ImGui::CalcTextSize(expText, nullptr, true);
float textX = b.x - 22 - textSize.x - 10; // 10px padding dari kanan
float textY = a.y + 18; // sejajar dengan judul

dl->AddText(ImVec2(textX, textY), IM_COL32(255, 0, 0, 255), expText);

    SetCursorScreenPos(ImVec2(a.x + 16, a.y + 62));
    PushStyleColor(ImGuiCol_ChildBg, ImVec4(0,0,0,0));
    PushStyleVar(ImGuiStyleVar_ScrollbarSize, 6.0f);
    BeginChild(O("##Content"), ImVec2(b.x - a.x - 32, b.y - a.y - 80), false);

    switch (idx) {
        case 0: { 

           Dummy(ImVec2(0,4));

            need_save |= GoldToggle(L("Draw Prediction Lines","ﻁﻮﻄﺧ ﻢﺳﺭ"),
                                    L("",""),
                                    &persistent_bool[O("bESP_DrawPredictionLine")]);
            Dummy(ImVec2(0,8));
            need_save |= GoldToggle(L("Draw Pockets","ﺏﻮﻴﺠﻟﺍ ﻢﺳﺭ"),
                                    L("",""),
                                    &persistent_bool[O("bESP_DrawPockets")]);
            Dummy(ImVec2(0,8));
            need_save |= GoldToggle(L("Draw Shot State","ﺐﻳﻮﺼﺘﻟﺍ ﺔﻟﺎﺣ ﻢﺳﺭ"),
                                    L("",""),
                                    &persistent_bool[O("bESP_DrawPocketsShotState")]);
            Dummy(ImVec2(0,8));
            
            need_save |= GoldSliderFloat("Line Thickness", "", &persistent_float["fLineThick"], 0.5f, 8.0f, "%.1f px");
            Dummy(ImVec2(0,8));
            break;
        }
        case 1: {
            Dummy(ImVec2(0,4));
            need_save |= GoldToggle("Enable Auto Play", "", &persistent_bool[O("bAutoPlay")]);
            Dummy(ImVec2(0,8));
            need_save |= GoldToggle("Enable Auto Aim", "", &persistent_bool[O("bAutoAim")]);
            Dummy(ImVec2(0,8));
            break;
        }
        case 2: { 
            Dummy(ImVec2(0,4));
            need_save |= GoldToggle(O("Enable Auto Queue"), O(""), &persistent_bool[("bAutoQueue")]);
            Dummy(ImVec2(0, 20));
            
            TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Mode"));
            Dummy(ImVec2(0, 8));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 12));
            PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
            PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.16f, 0.16f, 0.20f, 1.0f));
            SetNextItemWidth(GetContentRegionAvail().x);
         //   need_save |= GoldCombo("##mode", &persistent_int["iAutoQueue_Mode"], "Last Selected\0Smart\0");
            need_save |= GoldCombo("Mode", "Queue selection mode", &persistent_int["iAutoQueue_Mode"], "Last Selected\0Smart\0");
            PopStyleColor(2);
            PopStyleVar(2);
            
            if (persistent_int["iAutoQueue_Mode"] == 1) {
                Dummy(ImVec2(0, 15));
                TextColored(ImVec4(0.75f, 0.75f, 0.8f, 1.0f), O("Bet Percent"));
                Dummy(ImVec2(0, 8));
                PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                PushStyleVar(ImGuiStyleVar_GrabRounding, 10.0f);
                PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.15f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.3f, 0.6f, 0.95f, 1.0f));
                PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
                SetNextItemWidth(GetContentRegionAvail().x);
                need_save |= SliderInt("##betpercent", &persistent_int["iAutoQueue_BetPercent"], 1, 100, "%d%%");
                PopStyleColor(3);
                PopStyleVar(2);
            }
            
            Dummy(ImVec2(0, 25));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("You will be auto queued to"));
            TextColored(ImVec4(0.5f, 0.5f, 0.55f, 1.0f), O("the last game mode you played"));
            break;
        }
        case 3: { 
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.18f, 0.80f, 0.44f, 1.0f), "%s", L("Mod Information","تفصيل ما يقولونه"));
            Dummy(ImVec2(0,14));
            TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", L("Version: ","DewaaPrtamaa: "));
            SameLine();
            TextColored(ImVec4(0.95f,0.95f,1.0f,1.0f), "%s", "8 Ball Pool 56.22.0");
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", L("Premium: ","Rafisla: "));
            SameLine();
            TextColored(ImVec4(0.95f,0.95f,1.0f,1.0f), "%s", "No");
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", L("Dev: ","ﻡﺍﺮﺠﻴﻠﻴﺗ: "));
            SameLine();
            TextColored(ImVec4(0.18f, 0.80f, 0.44f, 1.0f), "%s", "@xabi666");
            Dummy(ImVec2(0,20));
            PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f,0.18f,0.18f,1.0f));
            PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.70f,0.22f,0.22f,1.0f));
            PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            if (Button(L("Logout","ﺝﻭﺮﺨﻟﺍ ﻞﻴﺠﺴﺗ"), ImVec2(GetContentRegionAvail().x, 44))) {
                persistent_string["key"] = "";
                logged_in = false;
            }
            PopStyleVar(); PopStyleColor(2);
            break;
        }
        case 4: {
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", L("No new notifications","ﺓﺪﻳﺪﺟ ﺕﺍﺭﺎﻌﺷﺇ ﺪﺟﻮﺗ ﻻ"));
            break;
        }
        case 5: {
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.18f, 0.80f, 0.44f, 1.0f), "%s", "CM ENGINE  v1.0");
            Dummy(ImVec2(0,10));
            TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", L("Premium 8 Ball Pool Assistant","https://t.me/cmengine ﺎﻫﻮﻌﺑﺎﺗ ﻲﻠﺘﻟﺍ ﺓﺎﻨﻗ"));
            Dummy(ImVec2(0,20));
            TextColored(ImVec4(0.55f,0.60f,0.70f,1.0f), "%s", L("Contact @xabi666 on Telegram","@xabi666 ﻡﺍﺮﺠﻠﺗ ﻞﺻﺍﻮﺗ"));
            break;
        }
    }

    if (need_save) save_persistence();
    EndChild();
    PopStyleVar();
    PopStyleColor();
}


INLINE void DrawMenu(ImGuiIO& io) {
    if (g_menu.hideForCapture) return; 
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
        if (is_segv_handler_active()) {
            jump_buffer_active = 1;
            if (!sigsetjmp(jump_buffer, 1)) DrawESP(GetBackgroundDrawList());
            jump_buffer_active = 0;
        }

        if (g_menu.isOpen) g_menu.menuAlpha += (1.0f - g_menu.menuAlpha) * io.DeltaTime * 12.0f;
        else               g_menu.menuAlpha  = 0.0f;

        if (g_menu.menuAlpha > 0.01f) {
            float winW = 1000.0f;
            float winH = 620.0f;

            

            SetNextWindowSize(ImVec2(winW, winH), ImGuiCond_Always);
            SetNextWindowPos(ImVec2(Width / 2.0f, Height / 2.0f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

            PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.035f, 0.055f, 0.10f, g_menu.menuAlpha));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
            PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            PushStyleVar(ImGuiStyleVar_Alpha, g_menu.menuAlpha);

            ImGuiWindowFlags wf = ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse;
            if (Begin(O("##QPMenu"), &g_menu.isOpen, wf)) {
                ImDrawList* dl = GetWindowDrawList();
                ImVec2 wp = GetWindowPos();

                
                DrawOrnateFrame(dl, wp, ImVec2(wp.x + winW, wp.y + winH));

                DrawTitleFrame(dl, wp, winW);

                float sidebarW = 250.0f;
                DrawSidebar(sidebarW, winH, wp);
                DrawContentArea(sidebarW, winW, winH, wp);
            }
            End();

            PopStyleVar(4);
            PopStyleColor();
        }
    }
}

static void DrawFloatingButton(ImGuiIO& io) {
   // if (g_menu.isOpen) return;

    // Simple floating button — clean circle design
    float btnR    = 38.0f;   // radius
    float winSize = btnR * 2.0f + 8.0f;
    const float rightMargin = 24.0f;

    // Initialise shared position to bottom-right (first frame only)
    if (g_sideBtnsX <= 0.0f)
        g_sideBtnsX = io.DisplaySize.x - rightMargin;
    if (g_sideBtnsY <= 0.0f)
        g_sideBtnsY = io.DisplaySize.y - 150.0f;

    // Floating button sits 100px ABOVE the toggle button, centred on same X anchor
    float toggleWidth = 78.f + (GetStyle().WindowPadding.x * 2.0f);
    float posX = ImClamp(g_sideBtnsX - toggleWidth + (toggleWidth - winSize) * 0.5f,
                         0.0f, io.DisplaySize.x - winSize);
    float posY = g_sideBtnsY - 100.0f;

    SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    SetNextWindowSize(ImVec2(winSize, winSize), ImGuiCond_Always);

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0, 0, 0, 0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    if (Begin(O("##FloatBtn"), nullptr,
              ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {

        ImDrawList* dl = GetWindowDrawList();
        ImVec2 center  = ImVec2(posX + winSize * 0.5f, posY + winSize * 0.5f);

        InvisibleButton(O("##FloatBtnHit"), ImVec2(winSize, winSize));

        // Drag: move BOTH X and Y freely anywhere on screen
        if (IsItemActive() && IsMouseDragging(ImGuiMouseButton_Left)) {
            g_sideBtnsX += io.MouseDelta.x;
            g_sideBtnsY += io.MouseDelta.y;
            // Clamp: keep buttons fully visible
            g_sideBtnsX = ImClamp(g_sideBtnsX, toggleWidth, io.DisplaySize.x - 4.0f);
            g_sideBtnsY = ImClamp(g_sideBtnsY, 140.0f,      io.DisplaySize.y - 80.0f);
        }

        // Tap opens menu — use total drag distance, not just Y
        ImVec2 dd = ImGui::GetMouseDragDelta(0);
        bool wasTap = (dd.x * dd.x + dd.y * dd.y) < 16.0f;  // <4px total movement
        if (IsItemHovered() && IsMouseReleased(0) && wasTap) {
            g_menu.isOpen = !g_menu.isOpen;
        }

        bool hov = IsItemHovered();

        // Outer glow ring
        dl->AddCircle(center, btnR + 5.0f, IM_COL32(200, 30, 30, hov ? 120 : 60), 0, 2.0f);
        // Main filled circle — dark
        dl->AddCircleFilled(center, btnR, hov ? IM_COL32(38, 38, 48, 245) : IM_COL32(22, 22, 30, 230));
        // Red accent border
        dl->AddCircle(center, btnR, IM_COL32(200, 30, 30, hov ? 255 : 180), 0, 2.5f);

        // LYN4XP logo inside floating button
        static GLuint s_lyn4xp_float_tex = LoadTextureFromMemory(logo_png, logo_png_len);
        if (s_lyn4xp_float_tex) {
            float imgSz = btnR * 0.95f;
            ImVec2 iMin(center.x - imgSz, center.y - imgSz);
            ImVec2 iMax(center.x + imgSz, center.y + imgSz);
            dl->AddImageRounded((void*)(intptr_t)s_lyn4xp_float_tex, iMin, iMax,
                ImVec2(0,0), ImVec2(1,1), IM_COL32(255,255,255,230), btnR * 0.60f);
        }
    }
    End();
    PopStyleVar(2);
    PopStyleColor();
}


static void DrawCaptureButton(ImGuiIO& io){
    static ImVec2 pos = ImVec2(80, 260);
    static bool dragging = false;
    float R = 30.0f, S = R * 2.0f;

    SetNextWindowPos(pos, ImGuiCond_Always);
    SetNextWindowSize(ImVec2(S + 10, S + 10), ImGuiCond_Always);
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0,0,0,0));
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);


    if (Begin(O("##QPCapBtn"), nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings)) {
        ImDrawList* dl = GetWindowDrawList();
        ImVec2 c = ImVec2(pos.x + R + 2, pos.y + R + 2);
        SetCursorPos(ImVec2(0,0));
        InvisibleButton(O("##QPCapHit"), ImVec2(S, S));

        ImU32 base = g_menu.hideForCapture ? IM_COL32(20, 30, 50, 80) : IM_COL32(20, 30, 50, 220);
        ImU32 ring = g_menu.hideForCapture ? IM_COL32(212, 175, 75, 120) : COL_GOLD_BRIGHT;
        dl->AddCircleFilled(c, R,     base, 40);
        dl->AddCircle      (c, R,     ring, 40, 2.0f);


        ImU32 ic = g_menu.hideForCapture ? IM_COL32(255,255,255,160) : COL_GOLD_BRIGHT;
        dl->AddRect      (ImVec2(c.x - 14, c.y - 9), ImVec2(c.x + 14, c.y + 11), ic, 3.0f, 0, 2.0f);
        dl->AddRectFilled(ImVec2(c.x - 5,  c.y - 14), ImVec2(c.x + 5,  c.y - 9),  ic, 1.0f);
        dl->AddCircle    (c, 6.0f, ic, 24, 1.8f);

        if (IsItemActive() && IsMouseDragging(0)) {
            dragging = true;
            pos.x += io.MouseDelta.x;
            pos.y += io.MouseDelta.y;
            pos.x = ImClamp(pos.x, 0.0f, (float)Width  - S);
            pos.y = ImClamp(pos.y, 0.0f, (float)Height - S);
        }
        if (IsItemHovered() && IsMouseReleased(0) && !dragging) {
            g_menu.hideForCapture = !g_menu.hideForCapture;
        }
        if (!IsItemActive()) dragging = false;
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
    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.03f, 0.05f, 0.09f, 0.96f));
    Begin(O("##Overlay"), nullptr, ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoInputs|ImGuiWindowFlags_NoSavedSettings);
    PopStyleColor();

    float cardW = 760, cardH = 620;
    SetNextWindowSize(ImVec2(cardW, cardH), ImGuiCond_Always);
    SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.10f, 0.18f, 1.0f));
    PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    Begin(O("##LoginCard"), nullptr, ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = GetWindowDrawList();
    ImVec2 wp = GetWindowPos();
    DrawOrnateFrame(dl, wp, ImVec2(wp.x + cardW, wp.y + cardH));


    DrawGradientRect(dl, ImVec2(wp.x + 20, wp.y + 20), ImVec2(wp.x + cardW - 20, wp.y + 90), COL_GOLD_DEEP, COL_GOLD_BRIGHT, true);
    SetWindowFontScale(1.7f);
    ImVec2 ts = CalcTextSize("CM ENGINE");
    DrawBoldText(dl, ImVec2(wp.x + (cardW - ts.x) * 0.5f, wp.y + 38), IM_COL32(15,22,36,255), "CM ENGINE");
    SetWindowFontScale(1.0f);
    ImVec2 ss = CalcTextSize("");
    dl->AddText(ImVec2(wp.x + (cardW - ss.x) * 0.5f, wp.y + 66), IM_COL32(30,40,55,255), "");

    SetCursorPosY(110);

    Dummy(ImVec2(0, 20));

    if (!ERROR_MESSAGE.empty()) {
        SetCursorPosX(30);
        PushTextWrapPos(cardW - 30);
        TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", ERROR_MESSAGE.c_str());
        PopTextWrapPos();
        Dummy(ImVec2(0, 8));
    }

    if (is_logging_in) {
        SetCursorPosY(240);
        static float ang = 0.0f;
        ang += io.DeltaTime * 5.0f;
        ImVec2 cc = ImVec2(wp.x + cardW * 0.5f, wp.y + 280);
        for (int i = 0; i < 12; i++) {
            float a = ang + i * PI * 2.0f / 12.0f;
            float al = (12 - i) / 12.0f;
            dl->AddCircleFilled(ImVec2(cc.x + cosf(a)*30, cc.y + sinf(a)*30), 5.0f,
                                IM_COL32(245, 210, 110, (int)(al * 255)));
        }
        const char* l = L("Authenticating...", "ﻖﻘﺤﺘﻟﺍ ﻱﺭﺎﺟ...");
        ImVec2 ls = CalcTextSize(l);
        SetCursorPosX((cardW - ls.x) * 0.5f);
        SetCursorPosY(340);
        TextColored(ImVec4(0.7f,0.75f,0.85f,1.0f), "%s", l);
    } else {
        SetCursorPosY(210);
        const char* hint = L("Paste your license key then tap LOGIN", "ﻝﻮﺧﺩ ﻂﻐﺿﺍ ﻢﺛ ﺺﻴﺧﺮﺘﻟﺍ ﺡﺎﺘﻔﻣ ﻖﺼﻟﺍ");
        ImVec2 hs = CalcTextSize(hint);
        SetCursorPosX((cardW - hs.x) * 0.5f);
        TextColored(ImVec4(0.62f,0.66f,0.75f,1.0f), "%s", hint);

        Dummy(ImVec2(0, 30));

        bool AutoLogin = first_time && !persistent_string["key"].empty();

        SetCursorPosX(30);
        PushStyleColor(ImGuiCol_Button,        (ImVec4)ImColor(COL_GOLD_DEEP));
        PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.62f, 0.24f, 1.0f));
        PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.55f, 0.42f, 0.16f, 1.0f));
        PushStyleColor(ImGuiCol_Text,          ImVec4(1, 1, 1, 1));
        PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);

        if (AutoLogin || Button(L("LOGIN  (paste key from clipboard)", "ﺡﺎﺘﻔﻤﻟﺍ ﻖﺼﻟﺍ"),
                                ImVec2(cardW - 60, 56))) {
    if (DEBUG_BYPASS_LOGIN) {
        // Debug bypass: open menu immediately
        logged_in = true;
        g_menu.isOpen = true;
    } else {
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
}
        PopStyleVar();
        PopStyleColor(4);

        Dummy(ImVec2(0, 20));
        const char* help = L("To get a key, contact Telegram @xabi666",
                             " ﻡﺍﺮﺠﻠﺗ ﺮﺒﻋ ﺎﻨﻌﻣ ﻞﺻﺍﻮﺗ ﺡﺎﺘﻔﻣ ﻰﻠﻋ ﻝﻮﺼﺤﻠﻟ @xabi666");
        ImVec2 hps = CalcTextSize(help);
        SetCursorPosX((cardW - hps.x) * 0.5f);
        TextColored(ImVec4(0.18f, 0.80f, 0.44f, 1.0f), "%s", help);
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
    
    if (persistent_float["fLineThick"] < 0.5f) {
    persistent_float["fLineThick"] = 2.0f;
    }

    static string INI_PATH = O("/data/user_de/0/") + PACKAGE_NAME + O("/no_backup/.ini");
    io.IniFilename = persistent_bool["bImguiAutoSave"] ? INI_PATH.c_str() : nullptr;
    io.ConfigWindowsMoveFromTitleBarOnly = persistent_bool["bMoveOnlyWithTitleBar"];

    ImFontConfig font_cfg;
    font_cfg.SizePixels = persistent_float["fFontScale"];
    if (font_cfg.SizePixels < 8.0f) font_cfg.SizePixels = 22.0f;
    io.Fonts->AddFontDefault(&font_cfg);

    // تعريب كامل بس اعكس الكلمات
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


DEFINES(EGLBoolean, Draw, EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH,  &Width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &Height);
    if (Width <= 0 || Height <= 0) return _Draw(dpy, surface);
    screenCenter = Vector2(Width / 2, Height / 2);
    if (!bImguiSetup) SetupImgui();

    ImGuiIO& io = ImGui::GetIO();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplAndroid_NewFrame(Width, Height);
    ImGui::NewFrame();

    if (!is_segv_handler_active()) setup_global_segv_handler();

    if (IsExpired()) {
        DrawExpired(io);
    } else
    if ((!g_Token.empty() && !g_Auth.empty() && g_Token == g_Auth) || DEBUG_BYPASS_LOGIN) {
   //     DrawCaptureButton(io);
        DrawFloatingButton(io);
        DrawMenu(io);
        DrawShotApprovalPrompt(io);
        //DrawLiveStatusOverlay(io);
        SetNextWindowPos(ImVec2(Width * 0.5f, Height - 60.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
                Begin(O("##PoweredBy"), nullptr,
                      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                      ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize |
                      ImGuiWindowFlags_NoInputs);
                TextColored(ImColor(0, 255, 0, 255), O("tg : @Cmengine"));
                End();



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
