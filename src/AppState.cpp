#include "AppState.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace AppState {
    std::mutex ModelMutex;
    Settings Options;
    Snapshot Live;
    PendingActions Pending;
    std::atomic<bool> MenuOpen{true};
    std::atomic<bool> UnloadRequested{false};

    namespace {
        float SafeFloat(float value, float fallback, float minimum, float maximum) {
            if (!std::isfinite(value)) return fallback;
            return std::clamp(value, minimum, maximum);
        }

        int SaturatingAdd(int left, int right) {
            const auto sum = static_cast<long long>(left) + right;
            return static_cast<int>(std::clamp(sum,
                static_cast<long long>(std::numeric_limits<int>::min()),
                static_cast<long long>(std::numeric_limits<int>::max())));
        }
    }

    void Normalize(Settings& settings) {
        settings.MoneyTarget = SafeFloat(settings.MoneyTarget, 100000.0f, 0.0f, 1000000000.0f);
        settings.HealthTarget = SafeFloat(settings.HealthTarget, 1.0f, 0.0f, 1.0f);
        settings.EnergyTarget = SafeFloat(settings.EnergyTarget, 1.0f, 0.0f, 1.0f);
        settings.NeedTarget = SafeFloat(settings.NeedTarget, 1.0f, 0.0f, 1.0f);
        settings.HappinessTarget = SafeFloat(settings.HappinessTarget, 1.0f, 0.0f, 1.0f);
        settings.SkillPointsTarget = std::clamp(settings.SkillPointsTarget, 0, 1000000);
        settings.MinimumExtraViewers = std::clamp(settings.MinimumExtraViewers, 0, 10000000);
        settings.SpeedMultiplier = SafeFloat(settings.SpeedMultiplier, 1.5f, 0.5f, 5.0f);
        settings.AccentTheme = std::clamp(settings.AccentTheme, 0, 2);
        settings.StartPage = std::clamp(settings.StartPage, 0, 5);
    }

    void Queue(const PendingActions& actions) {
        std::scoped_lock lock(ModelMutex);
        Pending.AddMoney = std::clamp(Pending.AddMoney + actions.AddMoney, -1000000000.0f, 1000000000.0f);
        Pending.AddFollowers = SaturatingAdd(Pending.AddFollowers, actions.AddFollowers);
        Pending.AddSubscribers = SaturatingAdd(Pending.AddSubscribers, actions.AddSubscribers);
        Pending.AddSkillPoints = SaturatingAdd(Pending.AddSkillPoints, actions.AddSkillPoints);
        Pending.AddExperience = SaturatingAdd(Pending.AddExperience, actions.AddExperience);
    }
}

