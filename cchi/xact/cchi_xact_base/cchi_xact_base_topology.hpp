#pragma once

#ifndef __CCHI__CCHI_XACT_BASE__TOPOLOGY
#define __CCHI__CCHI_XACT_BASE__TOPOLOGY

#include <cstdint>
#include <string>
#include <unordered_map>

#include "../../basic/cchi_components.hpp"


namespace CCHI::Xact {

    class XactScopeEnumBack {
    public:
        const char* name;
        const int   value;

    public:
        inline constexpr XactScopeEnumBack(const char* name, const int value) noexcept
        : name(name), value(value) { }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const XactScopeEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const XactScopeEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const XactScopeEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using XactScopeEnum = const XactScopeEnumBack*;

    namespace XactScope {
        inline constexpr XactScopeEnumBack  Downstream  ("Downstream",  0);
        inline constexpr XactScopeEnumBack  Upstream    ("Upstream",    1);
    }

    class ChannelTypeEnumBack {
    public:
        const char* name;
        const int   value;

    public:
        inline constexpr ChannelTypeEnumBack(const char* name, const int value) noexcept
        : name(name), value(value) { }

    public:
        inline constexpr operator int() const noexcept
        { return value; }

        inline constexpr operator const ChannelTypeEnumBack*() const noexcept
        { return this; }

        inline constexpr bool operator==(const ChannelTypeEnumBack& obj) const noexcept
        { return value == obj.value; }

        inline constexpr bool operator!=(const ChannelTypeEnumBack& obj) const noexcept
        { return !(*this == obj); }
    };

    using ChannelTypeEnum = const ChannelTypeEnumBack*;

    namespace ChannelType {
        inline constexpr ChannelTypeEnumBack    EVT     ("EVT", 0);
        inline constexpr ChannelTypeEnumBack    SNP     ("SNP", 1);
        inline constexpr ChannelTypeEnumBack    REQ     ("REQ", 2);
        inline constexpr ChannelTypeEnumBack    DnRSP   ("DnRSP", 3);
        inline constexpr ChannelTypeEnumBack    UpRSP   ("UpRSP", 4);
        inline constexpr ChannelTypeEnumBack    DnDAT   ("DnDAT", 5);
        inline constexpr ChannelTypeEnumBack    UpDAT   ("UpDAT", 6);
    }

    class Topology {
    public:
        class Node {
        public:
            ComponentTypeEnum   type;
            std::string         name;
            bool                valid;

        public:
            Node() noexcept;
            Node(ComponentTypeEnum type, const std::string& name) noexcept;

        public:
            bool IsValid() const noexcept;
            operator bool() const noexcept;
        };
    
    protected:
        std::unordered_map<uint16_t, Node>  nodes;

    public:
        void            SetNode(uint16_t id, ComponentTypeEnum type, const std::string& name) noexcept;
        bool            HasNode(uint16_t id) const noexcept;
        const Node&     GetNode(uint16_t id) const noexcept;
        bool            RemoveNode(uint16_t id) noexcept;

    public:
        bool            IsHome(uint16_t id) const noexcept;
        bool            IsType1(uint16_t id) const noexcept;
        bool            IsType2(uint16_t id) const noexcept;
        bool            IsType3(uint16_t id) const noexcept;
        bool            IsType4(uint16_t id) const noexcept;
        bool            IsType5(uint16_t id) const noexcept;
    };
}


// Implementation of: class Topology::Node
namespace CCHI::Xact {

    inline Topology::Node::Node() noexcept
        : type      (ComponentType::Unknown)
        , name      ("")
        , valid     (false)
    { }

    inline Topology::Node::Node(ComponentTypeEnum type, const std::string& name) noexcept
        : type      (type)
        , name      (name)
        , valid     (true)
    { }

    inline bool Topology::Node::IsValid() const noexcept
    {
        return valid;
    }

    inline Topology::Node::operator bool() const noexcept
    {
        return valid;
    }
}


// Implementation of: class Topology
namespace CCHI::Xact {

    inline void Topology::SetNode(uint16_t id, ComponentTypeEnum type, const std::string& name) noexcept
    {
        nodes[id] = Node(type, name);
    }

    inline bool Topology::HasNode(uint16_t id) const noexcept
    {
        return nodes.contains(id);
    }

    inline const Topology::Node& Topology::GetNode(uint16_t id) const noexcept
    {
        static Node invalidNode;

        auto iter = nodes.find(id);

        if (iter == nodes.end())
            return invalidNode;

        return iter->second;
    }

    inline bool Topology::RemoveNode(uint16_t id) noexcept
    {
        return nodes.erase(id) != 0;
    }

    inline bool Topology::IsHome(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::HOME;
    }

    inline bool Topology::IsType1(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::TYPE_1;
    }

    inline bool Topology::IsType2(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::TYPE_2;
    }

    inline bool Topology::IsType3(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::TYPE_3;
    }

    inline bool Topology::IsType4(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::TYPE_4;
    }

    inline bool Topology::IsType5(uint16_t id) const noexcept
    {
        const Node& node = GetNode(id);
        return node.valid && node.type == ComponentType::TYPE_5;
    }
}


#endif // __CCHI__CCHI_XACT_BASE__TOPOLOGY
