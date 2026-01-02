//#pragma once

//#ifndef __CHI__CHI_XACT_GLOBAL
//#define __CHI__CHI_XACT_GLOBAL

#ifndef CHI_XACT_GLOBAL__STANDALONE
#   include "chi_xact_global_header.hpp"            // IWYU pragma: keep
#   include "chi_xact_base_header.hpp"              // IWYU pragma: keep
#   include "chi_xact_base.hpp"                     // IWYU pragma: keep
#   include "chi_xact_checkers_field_header.hpp"    // IWYU pragma: keep
#   include "chi_xact_checkers_field.hpp"           // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_BASE_B__GLOBAL)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_BASE_EB__GLOBAL))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_BASE_B__GLOBAL
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_BASE_EB__GLOBAL
#endif


/*
namespace CHI {
*/
    namespace Xact {

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class GlobalCheckFieldMapping {
        public:
            bool enable = true;
            struct {
                bool                                        enable  = true;
                RequestFieldMappingChecker<config, conn>    checker;
            } REQ;
            struct {
                bool                                        enable  = true;
                SnoopFieldMappingChecker<config, conn>      checker;
            } SNP;
            struct {
                bool                                        enable  = true;
                ResponseFieldMappingChecker<config, conn>   checker;
            } RSP;
            struct {
                bool                                        enable  = true;
                DataFieldMappingChecker<config, conn>       checker;
            } DAT;

        public:
            XactDenialEnum  Check(const Flits::REQ<config, conn>&) const noexcept;
            XactDenialEnum  Check(const Flits::SNP<config, conn>&) const noexcept;
            XactDenialEnum  Check(const Flits::RSP<config, conn>&) const noexcept;
            XactDenialEnum  Check(const Flits::DAT<config, conn>&) const noexcept;
        };

        class GlobalSAMScope {
        public:
            /*
            Whether enabling SAM-scope-interferenced Node ID checkings.
            The Node ID consistency between flits through out a transaction was never checked with this disabled.
            */
            bool enable = true;

            /*
            Default SAM Scope when not specified by Node ID.
            Requester Nodes and Home Nodes that were not listed/configured in this container would refer to
            this value.
            */
            SAMScopeEnum defaultScope = SAMScope::AfterSAM;

        protected:
            std::unordered_map<uint16_t, SAMScopeEnum> scopes;
            
        public:
            void            Set(uint16_t nodeId, SAMScopeEnum scope) noexcept;
            SAMScopeEnum    Get(uint16_t nodeId) const noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn    = CHI::Connection<>>
        class Global {
        public:
            /*
            Enable Controls and Checkers of Flit Field Checking.
            */
            std::shared_ptr<GlobalCheckFieldMapping<config, conn>>
                CHECK_FIELD_MAPPING;

            /*
            (SAM, System Address Map)
            SAM Scopes of Nodes, indicating the transaction flits were observed either before or after SAM.
            Node ID inferencings and checkings are interferenced by SAM Scopes.
            */
            std::shared_ptr<GlobalSAMScope>
                SAM_SCOPE;
        };
    }
/*
}
*/


// Implementation of: class GlobalCheckFieldMapping
namespace /*CHI::*/Xact {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum GlobalCheckFieldMapping<config, conn>::Check(const Flits::REQ<config, conn>& reqFlit) const noexcept
    {
        if (!enable || !REQ.enable)
            return XactDenial::ACCEPTED;

        return REQ.checker.Check(reqFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum GlobalCheckFieldMapping<config, conn>::Check(const Flits::SNP<config, conn>& snpFlit) const noexcept
    {
        if (!enable || !SNP.enable)
            return XactDenial::ACCEPTED;

        return SNP.checker.Check(snpFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum GlobalCheckFieldMapping<config, conn>::Check(const Flits::RSP<config, conn>& rspFlit) const noexcept
    {
        if (!enable || !RSP.enable)
            return XactDenial::ACCEPTED;

        return RSP.checker.Check(rspFlit);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum GlobalCheckFieldMapping<config, conn>::Check(const Flits::DAT<config, conn>& datFlit) const noexcept
    {
        if (!enable || !DAT.enable)
            return XactDenial::ACCEPTED;

        return DAT.checker.Check(datFlit);
    }
}


// Implementation of: class GlobalSAMScope
namespace /*CHI::*/Xact {

    inline void GlobalSAMScope::Set(uint16_t nodeId, SAMScopeEnum scope) noexcept
    {
        scopes[nodeId] = scope;
    }

    inline SAMScopeEnum GlobalSAMScope::Get(uint16_t nodeId) const noexcept
    {
        auto iter = scopes.find(nodeId);
        
        if (iter == scopes.end())
            return defaultScope;
        else
            return iter->second;
    }
}


#endif

//#endif // __CHI__CHI_XACT_GLOBAL
