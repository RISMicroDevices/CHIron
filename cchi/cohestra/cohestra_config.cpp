#include "cohestra_config.hpp"


namespace Cohestra {
    
    std::optional<std::string> DeserializeString(std::string& value) noexcept
    {
        return { value };
    }

    std::string SerializeString(const std::string& value) noexcept
    {
        return value;
    }

    std::optional<bool> DeserializeBool(std::string& value) noexcept
    {
        if (value == "true" || value == "1")
            return { true };
        else if (value == "false" || value == "0")
            return { false };
        else
            return std::nullopt;
    }

    std::string SerializeBool(const bool& value) noexcept
    {
        return value ? "1" : "0";
    }

    std::optional<int32_t> DeserializeInt32(std::string& value) noexcept
    {
        try {
            return { std::stoi(value) };
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string SerializeInt32(const int32_t& value) noexcept
    {
        return std::to_string(value);
    }

    std::optional<uint32_t> DeserializeUInt32(std::string& value) noexcept
    {
        try {
            return { std::stoul(value) };
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string SerializeUInt32(const uint32_t& value) noexcept
    {
        return std::to_string(value);
    }

    std::optional<int64_t> DeserializeInt64(std::string& value) noexcept
    {
        try {
            return { std::stoll(value) };
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string SerializeInt64(const int64_t& value) noexcept
    {
        return std::to_string(value);
    }

    std::optional<uint64_t> DeserializeUInt64(std::string& value) noexcept
    {
        try {
            return { std::stoull(value) };
        } catch (...) {
            return std::nullopt;
        }
    }

    std::string SerializeUInt64(const uint64_t& value) noexcept
    {
        return std::to_string(value);
    }

    std::string SerializeUInt32Vector(const std::vector<uint32_t>& vec) noexcept
    {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            result += std::to_string(vec[i]);
            if (i < vec.size() - 1)
                result += ",";
        }
        return result;
    }

    std::optional<std::vector<uint32_t>> DeserializeUInt32Vector(std::string& value) noexcept
    {
        std::vector<uint32_t> result;
        size_t start = 0;
        size_t end = value.find(',');
        while (end != std::string::npos)
        {
            std::string token = value.substr(start, end - start);
            try {
                result.push_back(std::stoul(token));
            } catch (...) {
                return std::nullopt;
            }
            start = end + 1;
            end = value.find(',', start);
        }
        // Last token
        std::string token = value.substr(start);
        try {
            result.push_back(std::stoul(token));
        } catch (...) {
            return std::nullopt;
        }
        return { result };
    }

    std::string SerializeUInt64Vector(const std::vector<uint64_t>& vec) noexcept
    {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            result += std::to_string(vec[i]);
            if (i < vec.size() - 1)
                result += ",";
        }
        return result;
    }

    std::optional<std::vector<uint64_t>> DeserializeUInt64Vector(std::string& value) noexcept
    {
        std::vector<uint64_t> result;
        size_t start = 0;
        size_t end = value.find(',');
        while (end != std::string::npos)
        {
            std::string token = value.substr(start, end - start);
            try {
                result.push_back(std::stoull(token));
            } catch (...) {
                return std::nullopt;
            }
            start = end + 1;
            end = value.find(',', start);
        }
        // Last token
        std::string token = value.substr(start);
        try {
            result.push_back(std::stoull(token));
        } catch (...) {
            return std::nullopt;
        }
        return { result };
    }

    std::string SerializeStringWordVector(const std::vector<std::string>& vec) noexcept
    {
        std::string result;
        for (size_t i = 0; i < vec.size(); ++i)
        {
            result += vec[i];
            if (i < vec.size() - 1)
                result += ",";
        }
        return result;
    }

    std::optional<std::vector<std::string>> DeserializeStringWordVector(std::string& value) noexcept
    {
        std::vector<std::string> result;
        size_t start = 0;
        size_t end = value.find(',');
        while (end != std::string::npos)
        {
            std::string token = value.substr(start, end - start);
            result.push_back(token);
            start = end + 1;
            end = value.find(',', start);
        }
        // Last token
        std::string token = value.substr(start);
        result.push_back(token);
        return { result };
    }
}
