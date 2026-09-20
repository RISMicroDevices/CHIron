#pragma once

#include <unordered_map>
#include <cstdint>
#include <any>
#include <optional>
#include <functional>
#include <string>
#include <vector>

#include "simpleini/SimpleIni.h"


namespace Cohestra {

    class ConfigurationEntryBackBase {
    public:
        const char* name;
        const char* key;
        const char* section;

    protected:
        inline constexpr ConfigurationEntryBackBase(const char* name, const char* key, const char* section) noexcept
        : name(name), key(key), section(section) { }

    public:
        inline constexpr operator const ConfigurationEntryBackBase*() const noexcept
        { return this; }
    };

    template<class T>
    class ConfigurationEntryBack : public ConfigurationEntryBackBase {
    public:
        using type = T;
        using deserializer_t = std::function<std::optional<T>(std::string&)>;
        using serializer_t = std::function<std::string(const T&)>;

    public:
        const T defaultValue;

        const deserializer_t deserializer;
        const serializer_t serializer;

    public:
        inline constexpr ConfigurationEntryBack(const char* name, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, name, nullptr), defaultValue(), deserializer(deserializer), serializer(serializer) { }

        inline constexpr ConfigurationEntryBack(const char* name, const char* key, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, key, nullptr), defaultValue(), deserializer(deserializer), serializer(serializer) { }

        inline constexpr ConfigurationEntryBack(const char* name, const char* key, const char* section, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, key, section), defaultValue(), deserializer(deserializer), serializer(serializer) { }

        inline constexpr ConfigurationEntryBack(const char* name, const T& defaultValue, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, name, nullptr), defaultValue(defaultValue), deserializer(deserializer), serializer(serializer) { }

        inline constexpr ConfigurationEntryBack(const char* name, const char* key, const T& defaultValue, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, key, nullptr), defaultValue(defaultValue), deserializer(deserializer), serializer(serializer) { }

        inline constexpr ConfigurationEntryBack(const char* name, const char* key, const char* section, const T& defaultValue, const deserializer_t& deserializer, const serializer_t& serializer) noexcept
        : ConfigurationEntryBackBase(name, key, section), defaultValue(defaultValue), deserializer(deserializer), serializer(serializer) { }

    public:
        inline operator const ConfigurationEntryBack<T>*() const noexcept
        { return this; }
    };

    using ConfigurationEntryBase = const ConfigurationEntryBackBase*;

    template<class T>
    using ConfigurationEntry = const ConfigurationEntryBack<T>*;

    std::optional<std::string> DeserializeString(std::string& value) noexcept;
    std::string SerializeString(const std::string& value) noexcept;

    std::optional<bool> DeserializeBool(std::string& value) noexcept;
    std::string SerializeBool(const bool& value) noexcept;

    std::optional<int32_t> DeserializeInt32(std::string& value) noexcept;
    std::string SerializeInt32(const int32_t& value) noexcept;

    std::optional<uint32_t> DeserializeUInt32(std::string& value) noexcept;
    std::string SerializeUInt32(const uint32_t& value) noexcept;

    std::optional<int64_t> DeserializeInt64(std::string& value) noexcept;
    std::string SerializeInt64(const int64_t& value) noexcept;

    std::optional<uint64_t> DeserializeUInt64(std::string& value) noexcept;
    std::string SerializeUInt64(const uint64_t& value) noexcept;

    std::string SerializeUInt32Vector(const std::vector<uint32_t>& vec) noexcept;
    std::optional<std::vector<uint32_t>> DeserializeUInt32Vector(std::string& value) noexcept;

    std::string SerializeUInt64Vector(const std::vector<uint64_t>& vec) noexcept;
    std::optional<std::vector<uint64_t>> DeserializeUInt64Vector(std::string& value) noexcept;

    std::string SerializeStringWordVector(const std::vector<std::string>& vec) noexcept;
    std::optional<std::vector<std::string>> DeserializeStringWordVector(std::string& value) noexcept;


    class Configuration {
    private:
        std::unordered_map<uintptr_t, std::any> _map;

    public:
        template<class T>
        inline void Set(ConfigurationEntry<T> entry, const T& value) noexcept
        {
            _map[reinterpret_cast<uintptr_t>(entry)] = value;
        }

        template<class T>
        inline std::optional<T> Get(ConfigurationEntry<T> entry) const noexcept
        {
            auto iter = _map.find(reinterpret_cast<uintptr_t>(entry));
            if (iter != _map.end())
                return { std::any_cast<T>(iter->second) };
            else
                return std::nullopt;
        }

        template<class T>
        inline T GetOrDefault(ConfigurationEntry<T> entry) const noexcept
        {
            auto iter = _map.find(reinterpret_cast<uintptr_t>(entry));
            if (iter != _map.end())
                return std::any_cast<T>(iter->second);
            else
                return entry->defaultValue;
        }

        template<class T>
        inline bool Has(ConfigurationEntry<T> entry) const noexcept
        {
            return _map.find(reinterpret_cast<uintptr_t>(entry)) != _map.end();
        }
    };

    class StringLoaderBase {
    public:
        virtual ~StringLoaderBase() noexcept = default;

    public:
        virtual std::optional<std::string> GetRawString(
            ConfigurationEntryBase          entry,
            const std::string&              defaultSection = "global") const noexcept = 0;
    };

    class INIConfigurationLoader : public StringLoaderBase {
    protected:
        std::string filePath;
        CSimpleIniA ini;

    public:
        inline INIConfigurationLoader(const std::string& filePath) noexcept
            : filePath  (filePath)
            , ini       ()
        { }

    public:
        inline bool LoadFile() noexcept
        {
            SI_Error rc = ini.LoadFile(filePath.c_str());
            ini.SetUnicode();
            return rc == SI_OK;
        }

        template<class T>
        inline std::optional<T> Load(
            Configuration&          config,
            ConfigurationEntry<T>   entry,
            const std::string&      defaultSection = "global") const noexcept
        {
            const char* rawValue = ini.GetValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key);

            if (rawValue == nullptr)
                return std::nullopt;

            std::string value(rawValue);
            std::optional<T> result = entry->deserializer(value);

            if (result)
                config.Set(entry, *result);

            return result;
        }

        inline std::optional<std::string> GetRawString(
            ConfigurationEntryBase          entry,
            const std::string&              defaultSection = "global") const noexcept override
        {
            const char* rawValue = ini.GetValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key);

            if (rawValue == nullptr)
                return std::nullopt;

            return { std::string(rawValue) };
        }

        inline std::optional<bool> LoadBool(
            Configuration&              config,
            ConfigurationEntry<bool>    entry,
            const std::string&          defaultSection = "global") const noexcept
        {
            if (!ini.KeyExists(entry->section ? entry->section : defaultSection.c_str(), entry->key))
                return std::nullopt;

            bool value = ini.GetBoolValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key);
            
            config.Set(entry, value);

            return value;
        }

        inline std::optional<long> LoadLong(
            Configuration&              config,
            ConfigurationEntry<long>    entry,
            const std::string&          defaultSection = "global") const noexcept
        {
            if (!ini.KeyExists(entry->section ? entry->section : defaultSection.c_str(), entry->key))
                return std::nullopt;

            long value = ini.GetLongValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key);
            
            config.Set(entry, value);

            return value;
        }

        inline std::optional<double> LoadDouble(
            Configuration&                config,
            ConfigurationEntry<double>    entry,
            const std::string&            defaultSection = "global") const noexcept
        {
            if (!ini.KeyExists(entry->section ? entry->section : defaultSection.c_str(), entry->key))
                return std::nullopt;

            double value = ini.GetDoubleValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key);
            
            config.Set(entry, value);

            return value;
        }

        inline std::string LoadOrDefaultRawString(
            Configuration&                  config,
            ConfigurationEntry<std::string> entry,
            const std::string&              defaultSection = "global") const noexcept
        {
            std::string value = ini.GetValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key,
                entry->defaultValue.c_str());

            config.Set(entry, value);

            return value;
        }

        inline bool LoadOrDefaultBool(
            Configuration&              config,
            ConfigurationEntry<bool>    entry,
            const std::string&          defaultSection = "global") const noexcept
        {
            bool value = ini.GetBoolValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key,
                entry->defaultValue);

            config.Set(entry, value);

            return value;
        }

        inline long LoadOrDefaultLong(
            Configuration&              config,
            ConfigurationEntry<long>    entry,
            const std::string&          defaultSection = "global") const noexcept
        {
            long value = ini.GetLongValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key,
                entry->defaultValue);

            config.Set(entry, value);

            return value;
        }

        inline double LoadOrDefaultDouble(
            Configuration&                config,
            ConfigurationEntry<double>    entry,
            const std::string&            defaultSection = "global") const noexcept
        {
            double value = ini.GetDoubleValue(
                entry->section ? entry->section : defaultSection.c_str(),
                entry->key,
                entry->defaultValue);

            config.Set(entry, value);

            return value;
        }
    };
}
