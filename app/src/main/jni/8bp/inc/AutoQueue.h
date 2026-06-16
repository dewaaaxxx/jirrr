#pragma once

struct MatchInfo {
    bool set = false;
    string Tier;
    int32_t arg10 = 0, arg11 = 0;
};

static MatchInfo lastMatchInfo;

DEFINE(void*, StartMatch, ptr arg1, int64_t arg2, string arg3, int64_t arg4, int32_t arg5, int32_t arg6, int64_t arg7, int32_t arg8, char arg9, int32_t arg10, int32_t arg11) {
    LOGI("StartMatch (hook)");

    LOGI("Tier %s", arg3.c_str());
    LOGI("arg4 %p, arg5 %d, arg6 %d, arg7 %p, arg8 %d, arg9 %d, arg10 %d, arg11 %d", (void*)arg4, arg5, arg6, (void*)arg7, arg8, (int)arg9, arg10, arg11);
    
    lastMatchInfo.set = true;
    lastMatchInfo.Tier = arg3;
    lastMatchInfo.arg10 = arg10;
    lastMatchInfo.arg11 = arg11;

    return _StartMatch(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
}

std::map<string, int64_t> modeBets = {
    {"M1", 50},
    {"M2", 100},
    {"M3", 500}, // Call Pocket on 8 Ball
    {"M4", 2500},
    {"M5", 10000}, // Call Pocket on All Shots
    {"M6", 50000},
    {"M7", 100000},
    {"M8", 250000},
    {"M9", 500000},
    {"M10", 1000000}, // Call Pocket on 8 Ball
    {"M11", 2500000}, // Call Pocket on All Shots
    {"M12", 4000000}, // Call Pocket on 8 Ball
    {"M13", 5000000}, // Call Pocket on All Shots
    {"M14", 10000000}, // Call Pocket on All Shots
    {"M15", 15000000}, // Call Pocket on All Shots
    {"M16", 25000000}, // Call Pocket on 8 Ball
    {"M17", 100000000}, // Call Pocket on All Shots + Cushion shot on 8 Ball
};

void StartLastMatch() {
    LOGI("StartLastMatch");

    // Respect the same toggle used in the menu (bAutoQueue)
    if (!persistent_bool[O("bAutoQueue")]) {
        LOGI("AutoQueue disabled, not starting match");
        return;
    }

    if (persistent_int["iAutoQueue_Mode"] == 0) {
        // MODE 0: LAST SELECTED MATCH
        if (lastMatchInfo.set) {
            LOGI("Starting last match: %s", lastMatchInfo.Tier.c_str());
            _StartMatch(sharedMenuManager.instance, 0, lastMatchInfo.Tier, 0, 0, 0, 0, 0, 0, lastMatchInfo.arg10, lastMatchInfo.arg11);
        } else {
            LOGI("AutoQueue Mode 0: No previous match found, not starting");
        }
    } else if (persistent_int["iAutoQueue_Mode"] == 1) {
        // MODE 1: SMART MODE
        auto coins = sharedUserInfo.coins();
        auto maxBet = coins * persistent_int["iAutoQueue_BetPercent"] / 100;
        
        string selectedMode = "M1";
        int64_t selectedBet = 50;
        
        for (const auto& [mode, bet] : modeBets) {
            if (maxBet >= bet) {
                selectedMode = mode;
                selectedBet = bet;
            }
        }
        
        LOGI("Selected mode %s with bet %lld (coins %lld)", selectedMode.c_str(), (long long)selectedBet, (long long)coins);
        _StartMatch(sharedMenuManager.instance, 0, selectedMode, 0, 0, 0, 0, 0, 0, 0x7100000001, 0xffffffff);
    }
}

DEFINE(int64_t, popMenuState, int64_t arg1, int64_t arg2, int32_t arg3, int64_t arg4) {
    LOGI("popMenuState arg1 %p, arg2 %p, arg3 %d, arg4 %p", (void*)arg1, (void*)arg2, arg3, (void*)arg4);
    
    // Hook: If AutoQueue is enabled, start the match after menu closes
    // Use the same key the menu toggles (bAutoQueue)
    if (persistent_bool[O("bAutoQueue")]) {
        LOGI("AutoQueue: Menu closing, starting queued match");
        StartLastMatch();
    }
    
    return _popMenuState(arg1, arg2, arg3, arg4);
}

void PopMenuState(int stateId) {
    LOGI("PopMenuState %d", stateId);
    auto _popMenuState = M(int64_t, libmain + 0x3051f00, int64_t, int64_t, int32_t, int64_t); // MenuManager::popMenuState:withScene:
    _popMenuState(sharedMenuManager.instance, 0, stateId, sharedMenuManager.instance);
}
