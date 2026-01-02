//#pragma once

#include "includes.hpp"


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B__TOPOLOGY)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB__TOPOLOGY))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B__TOPOLOGY
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB__TOPOLOGY
#endif


/*
namespace CHI {
*/
    namespace Xact {

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
            inline constexpr XactScopeEnumBack  Requester   ("Requester",   0);
            inline constexpr XactScopeEnumBack  Home        ("Home",        1);
            inline constexpr XactScopeEnumBack  Subordinate ("Subordinate", 2);
        }

        class NodeTypeEnumBack {
        public:
            const char*         name;
            const XactScopeEnum scope;
            const int           value;

        public:
            inline constexpr NodeTypeEnumBack(const char* name, const XactScopeEnum scope, const int value) noexcept
            : name(name), scope(scope), value(value) { }

        public:
            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const NodeTypeEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const NodeTypeEnumBack& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const NodeTypeEnumBack& obj) const noexcept
            { return !(*this == obj); }

            inline constexpr bool IsRequester() const noexcept
            { return scope == XactScope::Requester; }

            inline constexpr bool IsHome() const noexcept
            { return scope == XactScope::Home; }

            inline constexpr bool IsSubordinate() const noexcept
            { return scope == XactScope::Subordinate; }
        };

        using NodeTypeEnum = const NodeTypeEnumBack*;

        namespace NodeType {
            inline constexpr NodeTypeEnumBack   RN_F    ("RN-F", XactScope::Requester   , 0);
            inline constexpr NodeTypeEnumBack   RN_D    ("RN-D", XactScope::Requester   , 1);
            inline constexpr NodeTypeEnumBack   RN_I    ("RN-I", XactScope::Requester   , 2);
            inline constexpr NodeTypeEnumBack   HN_F    ("HN-F", XactScope::Home        , 3);
            inline constexpr NodeTypeEnumBack   HN_I    ("HN-I", XactScope::Home        , 4);
            inline constexpr NodeTypeEnumBack   SN_F    ("SN-F", XactScope::Subordinate , 5);
            inline constexpr NodeTypeEnumBack   SN_I    ("SN-I", XactScope::Subordinate , 6);
            inline constexpr NodeTypeEnumBack   MN      ("MN",   XactScope::Home        , 7);
        }

        class Topology {
        public:
            class Node {
            public:
                NodeTypeEnum    type;
                std::string     name;
                bool            valid;

            public:
                Node() noexcept;
                Node(NodeTypeEnum type, const std::string& name) noexcept;

            public:
                bool            IsValid() const noexcept;
                operator        bool() const noexcept;
            };

        protected:
            std::unordered_map<uint16_t, Node>  nodes;

        public:
            void            SetNode(uint16_t id, NodeTypeEnum type, const std::string& name = "unspecified") noexcept;
            bool            HasNode(uint16_t id) const noexcept;
            Node            GetNode(uint16_t id) const noexcept;
            bool            RemoveNode(uint16_t id) noexcept;

        public:
            bool            IsRequester(uint16_t id) const noexcept;
            bool            IsHome(uint16_t id) const noexcept;
            bool            IsSubordinate(uint16_t id) const noexcept;
        };
    }
/*
}
*/


// Implementation of: class Topology::Node
namespace /*CHI::*/Xact {
    /*
    NodeTypeEnum    type;
    std::string     name;
    bool            valid;
    */

    inline Topology::Node::Node() noexcept
        : type      (NodeType::RN_F)
        , name      ("invalid")
        , valid     (false)
    { }

    inline Topology::Node::Node(NodeTypeEnum type, const std::string& name) noexcept
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
namespace /*CHI::*/Xact {
    /*
    std::unordered_map<uint16_t, Node>  nodes;
    */

    inline void Topology::SetNode(uint16_t id, NodeTypeEnum type, const std::string& name) noexcept
    {
        nodes[id] = Node(type, name);
    }

    inline bool Topology::HasNode(uint16_t id) const noexcept
    {
        return nodes.contains(id);
    }

    inline Topology::Node Topology::GetNode(uint16_t id) const noexcept
    {
        auto iter = nodes.find(id);

        if (iter == nodes.end())
            return Node();

        return iter->second;
    }

    inline bool Topology::RemoveNode(uint16_t id) noexcept
    {
        return nodes.erase(id) != 0;
    }

    inline bool Topology::IsRequester(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsRequester();
    }

    inline bool Topology::IsHome(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsHome();
    }

    inline bool Topology::IsSubordinate(uint16_t id) const noexcept
    {
        Node node = GetNode(id);
        return node.valid && node.type->IsSubordinate();
    }
}


#endif
