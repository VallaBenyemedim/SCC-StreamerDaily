#include "Config.hpp"

#include "AppState.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <unordered_map>

namespace Config {
    namespace {
        std::filesystem::path ConfigPath;

        std::string Trim(std::string value) {
            const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
            value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
            return value;
        }

        bool ParseBool(const std::string& value, bool fallback) {
            if (value == "1" || value == "true" || value == "on") return true;
            if (value == "0" || value == "false" || value == "off") return false;
            return fallback;
        }

        int ParseInt(const std::string& value, int fallback) {
            int parsed{};
            const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            return result.ec == std::errc{} && result.ptr == value.data() + value.size() ? parsed : fallback;
        }

        float ParseFloat(const std::string& value, float fallback) {
            char* end = nullptr;
            const auto parsed = std::strtof(value.c_str(), &end);
            return end == value.c_str() + value.size() ? parsed : fallback;
        }
    }

    void Initialize(HMODULE module) {
        wchar_t buffer[32768]{};
        const auto length = GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
        std::filesystem::path modulePath(length ? std::wstring(buffer, length) : L"SCC-StreamerDaily.dll");
        ConfigPath = modulePath.parent_path() / L"config" / L"client.ini";
    }

    bool Load(std::string* error) {
        try {
            AppState::Settings loaded;
            if (!std::filesystem::exists(ConfigPath)) {
                std::scoped_lock lock(AppState::ModelMutex);
                AppState::Options = loaded;
                return true;
            }

            std::ifstream input(ConfigPath);
            if (!input) throw std::runtime_error("ayar dosyasi acilamadi");
            std::unordered_map<std::string, std::string> values;
            std::string line;
            while (std::getline(input, line)) {
                line = Trim(line);
                if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[') continue;
                const auto separator = line.find('=');
                if (separator == std::string::npos) continue;
                values[Trim(line.substr(0, separator))] = Trim(line.substr(separator + 1));
            }
            const auto get = [&](const char* key) -> const std::string* {
                const auto found = values.find(key);
                return found == values.end() ? nullptr : &found->second;
            };
            if (auto value = get("lockMoney")) loaded.LockMoney = ParseBool(*value, loaded.LockMoney);
            if (auto value = get("moneyTarget")) loaded.MoneyTarget = ParseFloat(*value, loaded.MoneyTarget);
            if (auto value = get("lockHealth")) loaded.LockHealth = ParseBool(*value, loaded.LockHealth);
            if (auto value = get("healthTarget")) loaded.HealthTarget = ParseFloat(*value, loaded.HealthTarget);
            if (auto value = get("lockEnergy")) loaded.LockEnergy = ParseBool(*value, loaded.LockEnergy);
            if (auto value = get("energyTarget")) loaded.EnergyTarget = ParseFloat(*value, loaded.EnergyTarget);
            if (auto value = get("lockNeed")) loaded.LockNeed = ParseBool(*value, loaded.LockNeed);
            if (auto value = get("needTarget")) loaded.NeedTarget = ParseFloat(*value, loaded.NeedTarget);
            if (auto value = get("lockHappiness")) loaded.LockHappiness = ParseBool(*value, loaded.LockHappiness);
            if (auto value = get("happinessTarget")) loaded.HappinessTarget = ParseFloat(*value, loaded.HappinessTarget);
            if (auto value = get("lockSkillPoints")) loaded.LockSkillPoints = ParseBool(*value, loaded.LockSkillPoints);
            if (auto value = get("skillPointsTarget")) loaded.SkillPointsTarget = ParseInt(*value, loaded.SkillPointsTarget);
            if (auto value = get("streamBoost")) loaded.StreamBoost = ParseBool(*value, loaded.StreamBoost);
            if (auto value = get("minimumExtraViewers")) loaded.MinimumExtraViewers = ParseInt(*value, loaded.MinimumExtraViewers);
            if (auto value = get("speedEnabled")) loaded.SpeedEnabled = ParseBool(*value, loaded.SpeedEnabled);
            if (auto value = get("speedMultiplier")) loaded.SpeedMultiplier = ParseFloat(*value, loaded.SpeedMultiplier);
            if (auto value = get("accentTheme")) loaded.AccentTheme = ParseInt(*value, loaded.AccentTheme);
            if (auto value = get("reducedMotion")) loaded.ReducedMotion = ParseBool(*value, loaded.ReducedMotion);
            if (auto value = get("startPage")) loaded.StartPage = ParseInt(*value, loaded.StartPage);
            AppState::Normalize(loaded);
            {
                std::scoped_lock lock(AppState::ModelMutex);
                AppState::Options = loaded;
            }
            Logger::Write("Ayarlar yuklendi");
            return true;
        } catch (const std::exception& exception) {
            if (error) *error = exception.what();
            Logger::Write("Ayar yukleme hatasi: %s", exception.what());
            return false;
        }
    }

    bool Save(std::string* error) {
        try {
            AppState::Settings settings;
            {
                std::scoped_lock lock(AppState::ModelMutex);
                settings = AppState::Options;
            }
            AppState::Normalize(settings);
            std::filesystem::create_directories(ConfigPath.parent_path());
            const auto temporary = ConfigPath.wstring() + L".tmp";
            std::ofstream output(std::filesystem::path(temporary), std::ios::trunc);
            if (!output) throw std::runtime_error("gecici ayar dosyasi acilamadi");
            output << std::boolalpha << std::setprecision(9);
            output << "[SCCStreamerDaily]\n";
            output << "lockMoney=" << settings.LockMoney << '\n';
            output << "moneyTarget=" << settings.MoneyTarget << '\n';
            output << "lockHealth=" << settings.LockHealth << '\n';
            output << "healthTarget=" << settings.HealthTarget << '\n';
            output << "lockEnergy=" << settings.LockEnergy << '\n';
            output << "energyTarget=" << settings.EnergyTarget << '\n';
            output << "lockNeed=" << settings.LockNeed << '\n';
            output << "needTarget=" << settings.NeedTarget << '\n';
            output << "lockHappiness=" << settings.LockHappiness << '\n';
            output << "happinessTarget=" << settings.HappinessTarget << '\n';
            output << "lockSkillPoints=" << settings.LockSkillPoints << '\n';
            output << "skillPointsTarget=" << settings.SkillPointsTarget << '\n';
            output << "streamBoost=" << settings.StreamBoost << '\n';
            output << "minimumExtraViewers=" << settings.MinimumExtraViewers << '\n';
            output << "speedEnabled=" << settings.SpeedEnabled << '\n';
            output << "speedMultiplier=" << settings.SpeedMultiplier << '\n';
            output << "accentTheme=" << settings.AccentTheme << '\n';
            output << "reducedMotion=" << settings.ReducedMotion << '\n';
            output << "startPage=" << settings.StartPage << '\n';
            output.flush();
            if (!output) throw std::runtime_error("ayar dosyasi yazilamadi");
            output.close();
            if (!MoveFileExW(temporary.c_str(), ConfigPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                DeleteFileW(temporary.c_str());
                throw std::runtime_error("ayar dosyasi degistirilemedi");
            }
            Logger::Write("Ayarlar kaydedildi");
            return true;
        } catch (const std::exception& exception) {
            if (error) *error = exception.what();
            Logger::Write("Ayar kaydetme hatasi: %s", exception.what());
            return false;
        }
    }

    std::wstring Path() { return ConfigPath.wstring(); }
}

