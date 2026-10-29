#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

namespace AppState {
    struct Settings {
        bool LockMoney = false;
        float MoneyTarget = 100000.0f;
        bool LockHealth = false;
        float HealthTarget = 1.0f;
        bool LockEnergy = false;
        float EnergyTarget = 1.0f;
        bool LockNeed = false;
        float NeedTarget = 1.0f;
        bool LockHappiness = false;
        float HappinessTarget = 1.0f;
        bool LockSkillPoints = false;
        int SkillPointsTarget = 99;
        bool StreamBoost = false;
        int MinimumExtraViewers = 1000;
        bool SpeedEnabled = false;
        float SpeedMultiplier = 1.50f;
        int AccentTheme = 0;
        bool ReducedMotion = false;
        int StartPage = 0;
    };

    struct Snapshot {
        bool MonoReady = false;
        bool PlayerReady = false;
        float Money = 0.0f;
        float Health = 0.0f;
        float Energy = 0.0f;
        float Need = 0.0f;
        float Happiness = 0.0f;
        float WalkSpeed = 0.0f;
        int Followers = 0;
        int Subscribers = 0;
        int SkillPoints = 0;
        int Experience = 0;
        int Level = 0;
        int ExtraViewers = 0;
        std::string Status = "Mono bekleniyor";
    };

    struct PendingActions {
        float AddMoney = 0.0f;
        int AddFollowers = 0;
        int AddSubscribers = 0;
        int AddSkillPoints = 0;
        int AddExperience = 0;
    };

    extern std::mutex ModelMutex;
    extern Settings Options;
    extern Snapshot Live;
    extern PendingActions Pending;
    extern std::atomic<bool> MenuOpen;
    extern std::atomic<bool> UnloadRequested;

    void Normalize(Settings& settings);
    void Queue(const PendingActions& actions);
}

