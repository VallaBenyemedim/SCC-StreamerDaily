#include "MonoBridge.hpp"

#include "AppState.hpp"
#include "Logger.hpp"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string_view>

namespace MonoBridge {
    namespace {
        struct MonoDomain;
        struct MonoAssembly;
        struct MonoImage;
        struct MonoClass;
        struct MonoClassField;
        struct MonoVTable;
        struct MonoObject;
        struct MonoThread;

        using AssemblyCallback = void(*)(MonoAssembly*, void*);
        struct Api {
            MonoDomain* (*GetRootDomain)() = nullptr;
            MonoThread* (*ThreadCurrent)() = nullptr;
            MonoThread* (*ThreadAttach)(MonoDomain*) = nullptr;
            void (*AssemblyForeach)(AssemblyCallback, void*) = nullptr;
            MonoImage* (*AssemblyGetImage)(MonoAssembly*) = nullptr;
            const char* (*ImageGetName)(MonoImage*) = nullptr;
            MonoClass* (*ClassFromName)(MonoImage*, const char*, const char*) = nullptr;
            MonoClassField* (*ClassGetFieldFromName)(MonoClass*, const char*) = nullptr;
            MonoVTable* (*ClassVTable)(MonoDomain*, MonoClass*) = nullptr;
            void (*FieldStaticGetValue)(MonoVTable*, MonoClassField*, void*) = nullptr;
            void (*FieldGetValue)(MonoObject*, MonoClassField*, void*) = nullptr;
            void (*FieldSetValue)(MonoObject*, MonoClassField*, void*) = nullptr;
        } Mono;

        HMODULE MonoModule = nullptr;
        MonoDomain* RootDomain = nullptr;
        MonoImage* GameImage = nullptr;
        MonoClass* OpClass = nullptr;
        MonoClass* PlayerClass = nullptr;
        MonoClass* CharacterClass = nullptr;
        MonoClassField* OpSingletonField = nullptr;
        MonoClassField* OpPlayerField = nullptr;
        MonoClassField* PlayerDataField = nullptr;
        MonoClassField* WalkSpeedField = nullptr;
        MonoClassField* MoneyField = nullptr;
        MonoClassField* HealthField = nullptr;
        MonoClassField* EnergyField = nullptr;
        MonoClassField* NeedField = nullptr;
        MonoClassField* HappinessField = nullptr;
        MonoClassField* FollowersField = nullptr;
        MonoClassField* SubscribersField = nullptr;
        MonoClassField* SkillPointsField = nullptr;
        MonoClassField* ExperienceField = nullptr;
        MonoClassField* LevelField = nullptr;
        MonoClassField* ExtraViewersField = nullptr;

        std::mutex BridgeMutex;
        bool MetadataReady = false;
        bool WasPlayerReady = false;
        bool WorkerAttached = false;
        ULONGLONG LastTick = 0;
        MonoObject* TrackedPlayer = nullptr;
        float BaseWalkSpeed = 0.0f;
        float LastAppliedSpeed = 0.0f;
        bool SpeedWasApplied = false;

        template <typename T>
        bool Bind(T& destination, const char* name) {
            destination = reinterpret_cast<T>(GetProcAddress(MonoModule, name));
            if (!destination) Logger::Write("Mono export eksik: %s", name);
            return destination != nullptr;
        }

        void SetStatus(bool monoReady, bool playerReady, std::string_view status) {
            std::scoped_lock lock(AppState::ModelMutex);
            AppState::Live.MonoReady = monoReady;
            AppState::Live.PlayerReady = playerReady;
            AppState::Live.Status.assign(status);
        }

        bool AttachThread() {
            if (WorkerAttached) return true;
            Logger::Write("Mono attach: basladi, root=%p", RootDomain);
            if (!RootDomain) {
                RootDomain = Mono.GetRootDomain();
                Logger::Write("Mono attach: root alindi %p", RootDomain);
            }
            if (!RootDomain) return false;
            auto* current = Mono.ThreadAttach(RootDomain);
            Logger::Write("Mono attach: sonuc=%p", current);
            WorkerAttached = current != nullptr;
            return WorkerAttached;
        }

        void FindAssembly(MonoAssembly* assembly, void* userData) {
            auto** result = static_cast<MonoImage**>(userData);
            if (*result) return;
            auto* image = Mono.AssemblyGetImage(assembly);
            const auto* name = image ? Mono.ImageGetName(image) : nullptr;
            if (name && (std::strcmp(name, "Assembly-CSharp") == 0 || std::strcmp(name, "Assembly-CSharp.dll") == 0)) {
                *result = image;
            }
        }

        MonoClassField* Field(MonoClass* klass, const char* name) {
            auto* result = klass ? Mono.ClassGetFieldFromName(klass, name) : nullptr;
            if (!result) Logger::Write("Alan bulunamadi: %s", name);
            return result;
        }

        bool ResolveMetadata() {
            if (MetadataReady) return true;
            Logger::Write("Assembly-CSharp aranıyor");
            GameImage = nullptr;
            Mono.AssemblyForeach(FindAssembly, &GameImage);
            if (!GameImage) {
                SetStatus(true, false, "Assembly-CSharp bekleniyor");
                return false;
            }
            Logger::Write("Assembly-CSharp bulundu");
            OpClass = Mono.ClassFromName(GameImage, "", "OP");
            PlayerClass = Mono.ClassFromName(GameImage, "", "Oyuncu");
            CharacterClass = Mono.ClassFromName(GameImage, "", "KarakterS");
            if (!OpClass || !PlayerClass || !CharacterClass) {
                SetStatus(true, false, "Oyun siniflari uyumsuz");
                return false;
            }
            OpSingletonField = Field(OpClass, "bu");
            OpPlayerField = Field(OpClass, "OyuncuChar");
            PlayerDataField = Field(PlayerClass, "veri");
            WalkSpeedField = Field(PlayerClass, "YurumeHizi");
            MoneyField = Field(CharacterClass, "Para");
            HealthField = Field(CharacterClass, "Saglik");
            EnergyField = Field(CharacterClass, "EnerjiIht");
            NeedField = Field(CharacterClass, "IhtiyacIht");
            HappinessField = Field(CharacterClass, "MutlulukIht");
            FollowersField = Field(CharacterClass, "ToplamTakipci");
            SubscribersField = Field(CharacterClass, "ToplamAbone");
            SkillPointsField = Field(CharacterClass, "YetenekPuani");
            ExperienceField = Field(CharacterClass, "Tecrube");
            LevelField = Field(CharacterClass, "Seviye");
            ExtraViewersField = Field(CharacterClass, "EkstraIzleyen");
            MetadataReady = OpSingletonField && OpPlayerField && PlayerDataField && WalkSpeedField && MoneyField &&
                HealthField && EnergyField && NeedField && HappinessField && FollowersField && SubscribersField &&
                SkillPointsField && ExperienceField && LevelField && ExtraViewersField;
            if (MetadataReady) Logger::Write("Mono bridge ready: OP -> Oyuncu -> KarakterS");
            return MetadataReady;
        }

        bool Objects(MonoObject*& op, MonoObject*& player, MonoObject*& data) {
            op = nullptr;
            player = nullptr;
            data = nullptr;
            auto* table = Mono.ClassVTable(RootDomain, OpClass);
            if (!table) return false;
            Mono.FieldStaticGetValue(table, OpSingletonField, &op);
            if (!op) return false;
            Mono.FieldGetValue(op, OpPlayerField, &player);
            if (!player) return false;
            Mono.FieldGetValue(player, PlayerDataField, &data);
            return data != nullptr;
        }

        template <typename T>
        T Read(MonoObject* object, MonoClassField* field) {
            T value{};
            Mono.FieldGetValue(object, field, &value);
            return value;
        }

        template <typename T>
        void Write(MonoObject* object, MonoClassField* field, T value) {
            Mono.FieldSetValue(object, field, &value);
        }

        int AddClamped(int current, int delta) {
            const auto value = static_cast<long long>(current) + delta;
            return static_cast<int>(std::clamp(value, 0LL, static_cast<long long>(std::numeric_limits<int>::max())));
        }

        void RestoreSpeed(MonoObject* player) {
            if (!player || player != TrackedPlayer || !SpeedWasApplied || !std::isfinite(BaseWalkSpeed)) return;
            const auto current = Read<float>(player, WalkSpeedField);
            if (std::isfinite(current) && std::abs(current - LastAppliedSpeed) < 0.05f) {
                Write(player, WalkSpeedField, BaseWalkSpeed);
                Logger::Write("Hareket hizi geri yuklendi: %.3f", BaseWalkSpeed);
            }
            SpeedWasApplied = false;
        }
    }

    bool Initialize() {
        std::scoped_lock lock(BridgeMutex);
        MonoModule = GetModuleHandleW(L"mono-2.0-bdwgc.dll");
        if (!MonoModule) {
            SetStatus(false, false, "Mono modulu bulunamadi");
            return false;
        }
        bool ok = true;
        ok &= Bind(Mono.GetRootDomain, "mono_get_root_domain");
        ok &= Bind(Mono.ThreadCurrent, "mono_thread_current");
        ok &= Bind(Mono.ThreadAttach, "mono_thread_attach");
        ok &= Bind(Mono.AssemblyForeach, "mono_assembly_foreach");
        ok &= Bind(Mono.AssemblyGetImage, "mono_assembly_get_image");
        ok &= Bind(Mono.ImageGetName, "mono_image_get_name");
        ok &= Bind(Mono.ClassFromName, "mono_class_from_name");
        ok &= Bind(Mono.ClassGetFieldFromName, "mono_class_get_field_from_name");
        ok &= Bind(Mono.ClassVTable, "mono_class_vtable");
        ok &= Bind(Mono.FieldStaticGetValue, "mono_field_static_get_value");
        ok &= Bind(Mono.FieldGetValue, "mono_field_get_value");
        ok &= Bind(Mono.FieldSetValue, "mono_field_set_value");
        RootDomain = ok ? Mono.GetRootDomain() : nullptr;
        if (!ok || !RootDomain) {
            SetStatus(false, false, "Mono API uyumsuz");
            return false;
        }
        SetStatus(true, false, "Oyun sahnesi bekleniyor");
        Logger::Write("Mono API baglandi");
        return true;
    }

    void Tick() {
        const auto now = GetTickCount64();
        if (now - LastTick < 33) return;
        LastTick = now;
        std::scoped_lock bridgeLock(BridgeMutex);
        if (!MonoModule) return;
        if (!AttachThread()) { SetStatus(false, false, "Mono thread baglantisi basarisiz"); return; }
        if (!ResolveMetadata()) return;

        MonoObject* op = nullptr;
        MonoObject* player = nullptr;
        MonoObject* data = nullptr;
        if (!Objects(op, player, data)) {
            if (WasPlayerReady) Logger::Write("Oyuncu verisi sahne degisimi nedeniyle bekleniyor");
            WasPlayerReady = false;
            TrackedPlayer = nullptr;
            SpeedWasApplied = false;
            SetStatus(true, false, "Kayitli oyun sahnesi bekleniyor");
            return;
        }

        AppState::Settings settings;
        AppState::PendingActions actions;
        {
            std::scoped_lock lock(AppState::ModelMutex);
            settings = AppState::Options;
            actions = AppState::Pending;
            AppState::Pending = {};
        }
        AppState::Normalize(settings);

        if (player != TrackedPlayer) {
            TrackedPlayer = player;
            BaseWalkSpeed = Read<float>(player, WalkSpeedField);
            LastAppliedSpeed = BaseWalkSpeed;
            SpeedWasApplied = false;
            Logger::Write("Oyuncu bulundu; temel yurume hizi %.3f", BaseWalkSpeed);
        }

        auto money = Read<float>(data, MoneyField);
        auto followers = Read<int>(data, FollowersField);
        auto subscribers = Read<int>(data, SubscribersField);
        auto skillPoints = Read<int>(data, SkillPointsField);
        auto experience = Read<int>(data, ExperienceField);
        if (actions.AddMoney != 0.0f) Write(data, MoneyField, std::clamp(money + actions.AddMoney, 0.0f, 1000000000.0f));
        if (actions.AddFollowers) Write(data, FollowersField, AddClamped(followers, actions.AddFollowers));
        if (actions.AddSubscribers) Write(data, SubscribersField, AddClamped(subscribers, actions.AddSubscribers));
        if (actions.AddSkillPoints) Write(data, SkillPointsField, AddClamped(skillPoints, actions.AddSkillPoints));
        if (actions.AddExperience) Write(data, ExperienceField, AddClamped(experience, actions.AddExperience));

        if (settings.LockMoney) Write(data, MoneyField, settings.MoneyTarget);
        if (settings.LockHealth) Write(data, HealthField, settings.HealthTarget);
        if (settings.LockEnergy) Write(data, EnergyField, settings.EnergyTarget);
        if (settings.LockNeed) Write(data, NeedField, settings.NeedTarget);
        if (settings.LockHappiness) Write(data, HappinessField, settings.HappinessTarget);
        if (settings.LockSkillPoints) Write(data, SkillPointsField, settings.SkillPointsTarget);
        if (settings.StreamBoost) {
            const auto current = Read<int>(data, ExtraViewersField);
            if (current < settings.MinimumExtraViewers) Write(data, ExtraViewersField, settings.MinimumExtraViewers);
        }

        if (settings.SpeedEnabled && std::isfinite(BaseWalkSpeed) && BaseWalkSpeed > 0.01f) {
            const auto desired = BaseWalkSpeed * settings.SpeedMultiplier;
            Write(player, WalkSpeedField, desired);
            LastAppliedSpeed = desired;
            if (!SpeedWasApplied) Logger::Write("Hareket hizi uygulandi: %.3f -> %.3f", BaseWalkSpeed, desired);
            SpeedWasApplied = true;
        } else if (SpeedWasApplied) {
            RestoreSpeed(player);
        }

        AppState::Snapshot snapshot;
        snapshot.MonoReady = true;
        snapshot.PlayerReady = true;
        snapshot.Status = "Oyun verisi bagli";
        snapshot.Money = Read<float>(data, MoneyField);
        snapshot.Health = Read<float>(data, HealthField);
        snapshot.Energy = Read<float>(data, EnergyField);
        snapshot.Need = Read<float>(data, NeedField);
        snapshot.Happiness = Read<float>(data, HappinessField);
        snapshot.WalkSpeed = Read<float>(player, WalkSpeedField);
        snapshot.Followers = Read<int>(data, FollowersField);
        snapshot.Subscribers = Read<int>(data, SubscribersField);
        snapshot.SkillPoints = Read<int>(data, SkillPointsField);
        snapshot.Experience = Read<int>(data, ExperienceField);
        snapshot.Level = Read<int>(data, LevelField);
        snapshot.ExtraViewers = Read<int>(data, ExtraViewersField);
        {
            std::scoped_lock lock(AppState::ModelMutex);
            AppState::Live = std::move(snapshot);
        }
        if (!WasPlayerReady) {
            Logger::Write("Canli oyuncu verisi dogrulandi: para=%.1f saglik=%.1f enerji=%.1f ihtiyac=%.1f mutluluk=%.1f takipci=%d abone=%d seviye=%d",
                snapshot.Money, snapshot.Health, snapshot.Energy, snapshot.Need, snapshot.Happiness,
                snapshot.Followers, snapshot.Subscribers, snapshot.Level);
        }
        WasPlayerReady = true;
    }

    void Shutdown() {
        std::scoped_lock bridgeLock(BridgeMutex);
        if (MonoModule && RootDomain && AttachThread() && MetadataReady) {
            MonoObject* op = nullptr;
            MonoObject* player = nullptr;
            MonoObject* data = nullptr;
            if (Objects(op, player, data)) RestoreSpeed(player);
        }
        SetStatus(false, false, "Kaldirildi");
    }
}

