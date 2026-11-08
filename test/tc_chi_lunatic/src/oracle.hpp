#pragma once

#include "common.hpp"
#include "../../../chi_eb/xact/chi_eb_joint.hpp"

#include <source_location>
#include <stdexcept>
#include <string>

namespace LunaticTest {

    template<typename config>
    class RNOracle {
        CHI::Eb::Xact::Global<config>   global;
        CHI::Eb::Xact::RNFJoint<config> joint;
        CHI::Lunatic::NodeId            home;
        CHI::Lunatic::NodeId            requester;

        static void Accept(CHI::Eb::Xact::XactDenialEnum denial,
                           std::source_location location = std::source_location::current())
        {
            if (denial != CHI::Eb::Xact::XactDenial::ACCEPTED)
                throw std::runtime_error(std::string("CHIron RN denial in ") + location.function_name() + ": " + denial->name);
        }

    public:
        RNOracle(CHI::Lunatic::NodeId home, CHI::Lunatic::NodeId requester,
                 CHI::Eb::Xact::NodeTypeEnum role = CHI::Eb::Xact::NodeType::RN_F)
            : home(home), requester(requester)
        {
            global.TOPOLOGY.SetNode(home, CHI::Eb::Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(requester, role);
        }

        void RegisterRequester(CHI::Lunatic::NodeId requester)
        {
            global.TOPOLOGY.SetNode(requester, CHI::Eb::Xact::NodeType::RN_F);
        }

        void RegisterSubordinate(CHI::Lunatic::NodeId subordinate)
        {
            global.TOPOLOGY.SetNode(subordinate, CHI::Eb::Xact::NodeType::SN_F);
        }

        void Request(uint64_t time, const CHI::Eb::Flits::REQ<config>& flit)
        {
            Accept(joint.NextTXREQ(global, time, flit));
        }

        void Response(uint64_t time, const CHI::Eb::Flits::RSP<config>& flit)
        {
            Accept(joint.NextRXRSP(global, time, flit));
        }

        void Data(uint64_t time, const CHI::Eb::Flits::DAT<config>& flit)
        {
            Accept(joint.NextRXDAT(global, time, flit));
        }

        void CompAck(uint64_t time, CHI::Lunatic::DataBufferId dbid)
        {
            CHI::Eb::Flits::RSP<config> flit{};
            flit.Opcode() = CHI::Eb::Opcodes::RSP::CompAck;
            flit.SrcID() = requester;
            flit.TgtID() = home;
            flit.TxnID() = dbid;
            Accept(joint.NextTXRSP(global, time, flit));
        }

        void Snoop(uint64_t time, const CHI::Eb::Flits::SNP<config>& flit)
        {
            Accept(joint.NextRXSNP(global, time, flit, requester));
        }

        void SnoopResponse(uint64_t time, const CHI::Eb::Flits::RSP<config>& flit)
        {
            Accept(joint.NextTXRSP(global, time, flit));
        }

        void SnoopData(uint64_t time, const CHI::Eb::Flits::DAT<config>& flit)
        {
            Accept(joint.NextTXDAT(global, time, flit));
        }

        void WriteData(uint64_t time, const CHI::Eb::Flits::DAT<config>& flit)
        {
            Accept(joint.NextTXDAT(global, time, flit));
        }

        bool HasInflight() const noexcept { return joint.HasInflight(); }
    };

    template<typename config>
    class SNOracle {
        CHI::Eb::Xact::Global<config>   global;
        CHI::Eb::Xact::SNFJoint<config> joint;

        static void Accept(CHI::Eb::Xact::XactDenialEnum denial)
        {
            if (denial != CHI::Eb::Xact::XactDenial::ACCEPTED)
                throw std::runtime_error(std::string("CHIron SN denial: ") + denial->name);
        }

    public:
        SNOracle(CHI::Lunatic::NodeId home, CHI::Lunatic::NodeId subordinate)
        {
            global.TOPOLOGY.SetNode(home, CHI::Eb::Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(subordinate, CHI::Eb::Xact::NodeType::SN_F);
        }

        void RegisterRequester(CHI::Lunatic::NodeId requester)
        {
            global.TOPOLOGY.SetNode(requester, CHI::Eb::Xact::NodeType::RN_F);
        }

        void Request(uint64_t time, const CHI::Eb::Flits::REQ<config>& flit)
        {
            Accept(joint.NextRXREQ(global, time, flit));
        }

        void Response(uint64_t time, const CHI::Eb::Flits::RSP<config>& flit)
        {
            Accept(joint.NextTXRSP(global, time, flit));
        }

        void Data(uint64_t time, const CHI::Eb::Flits::DAT<config>& flit)
        {
            const auto denial = joint.NextTXDAT(global, time, flit);
            if (denial != CHI::Eb::Xact::XactDenial::ACCEPTED)
                throw std::runtime_error(std::string("CHIron SN TXDAT denial: ") + denial->name +
                    " src=" + std::to_string(uint16_t(flit.SrcID())) +
                    " tgt=" + std::to_string(uint16_t(flit.TgtID())) +
                    " txn=" + std::to_string(uint16_t(flit.TxnID())) +
                    " dbid=" + std::to_string(uint16_t(flit.DBID())) +
                    " home=" + std::to_string(uint16_t(flit.HomeNID())));
        }

        void WriteData(uint64_t time, const CHI::Eb::Flits::DAT<config>& flit)
        {
            const auto denial = joint.NextRXDAT(global, time, flit);
            if (denial != CHI::Eb::Xact::XactDenial::ACCEPTED)
                throw std::runtime_error(std::string("CHIron SN RXDAT denial: ") + denial->name +
                    " src=" + std::to_string(uint16_t(flit.SrcID())) +
                    " tgt=" + std::to_string(uint16_t(flit.TgtID())) +
                    " txn=" + std::to_string(uint16_t(flit.TxnID())) +
                    " dbid=" + std::to_string(uint16_t(flit.DBID())) +
                    " home=" + std::to_string(uint16_t(flit.HomeNID())));
        }

        bool HasInflight() const noexcept { return joint.HasInflight(); }
    };

}
