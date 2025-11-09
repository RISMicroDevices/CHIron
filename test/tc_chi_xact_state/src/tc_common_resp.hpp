#pragma once

#include "tc_common.hpp"            // IWYU pragma: keep

#include <optional>


class TCPtResp : public TCPt<TCPtResp> {
private:
    Flits::REQ<config>                      req         = { 0 };
    Flits::SNP<config>                      snp         = { 0 };
    bool                                    isSNP;
    std::shared_ptr<Xact::Xaction<config>>  xaction;
    Xact::RNFJoint<config>                  rnJoint;
    Xact::RNCacheStateMap<config>           rnStates;

private:
    Xact::CacheState                    state;
    std::optional<Flits::RSP<config>>   rxrsp = std::nullopt;
    std::optional<Flits::DAT<config>>   rxdat = std::nullopt;
    std::optional<Flits::RSP<config>>   txrsp = std::nullopt;
    std::optional<Flits::DAT<config>>   txdat = std::nullopt;

protected:
    bool    disabled = false;

public:
    Flits::RSP<config>      GenFlitRXRSP() const noexcept;
    Flits::DAT<config>      GenFlitRXDAT() const noexcept;
    Flits::RSP<config>      GenFlitTXRSP() const noexcept;
    Flits::DAT<config>      GenFlitTXDAT() const noexcept;
    Flits::DAT<config>      GenDCTFlitTXDAT() const noexcept;

    Flits::DAT<config>      GenCompData() const noexcept;
    Flits::RSP<config>      GenComp() const noexcept;
    Flits::DAT<config>      GenDataSepResp() const noexcept;

    Flits::RSP<config>      GenCompStashDone() const noexcept;
    Flits::RSP<config>      GenCompPersist() const noexcept;
    Flits::RSP<config>      GenCompDBIDResp() const noexcept;
    Flits::RSP<config>      GenDBIDResp() const noexcept;

    Flits::DAT<config>      GenCopyBackWrData() const noexcept;
    Flits::DAT<config>      GenNonCopyBackWrData() const noexcept;
    Flits::DAT<config>      GenNCBWrDataCompAck() const noexcept;
    Flits::DAT<config>      GenWriteDataCancel() const noexcept;

    Flits::RSP<config>      GenFlitSnpResp(Resp::type resp) const noexcept;
    Flits::DAT<config>      GenFlitSnpRespData(Resp::type resp) const noexcept;
    Flits::DAT<config>      GenFlitSnpRespDataPtl(Resp::type resp) const noexcept;
    Flits::RSP<config>      GenFlitSnpRespFwded(Resp::type resp) const noexcept;
    Flits::DAT<config>      GenFlitSnpRespDataFwded(Resp::type resp) const noexcept;
    Flits::DAT<config>      GenDCTFlitCompData(Resp::type resp) const noexcept;

public:
    virtual ~TCPtResp() noexcept = default;

    TCPtResp* ForkREQ(std::string title, Flits::REQ<config>::opcode_t opcode) noexcept;
    TCPtResp* ForkSNP(std::string title, Flits::SNP<config>::opcode_t opcode, bool retToSrc = false, bool doNotGoToSD = false) noexcept;

    TCPtResp* TestAndForkInitial(std::string title, Xact::CacheState state) noexcept;
    TCPtResp* TestAndForkTransfer(std::string title, Xact::CacheState initial, Xact::CacheState intermediate) noexcept;
    TCPtResp* TestAndLeafTransfer(std::string title, Xact::CacheState initial, Xact::CacheState intermediate, bool accept = false) noexcept;

    TCPtResp* ForkCompData(std::string title) noexcept;
    TCPtResp* ForkDataSepResp(std::string title) noexcept;
    TCPtResp* ForkComp(std::string title) noexcept;
    TCPtResp* ForkCompStashDone(std::string title) noexcept;
    TCPtResp* ForkCompPersist(std::string title) noexcept;
    TCPtResp* ForkCompDBIDResp(std::string title) noexcept;

    TCPtResp* ForkInstantCompDBIDResp(std::string title) noexcept;
    TCPtResp* ForkInstantComp(std::string title, Resp::type resp) noexcept;
    TCPtResp* ForkInstantCompAndDBIDResp(std::string title, Resp::type resp) noexcept;
    TCPtResp* ForkInstantDBIDResp(std::string title) noexcept;

    TCPtResp* ForkCopyBackWrData(std::string title) noexcept;
    TCPtResp* ForkNonCopyBackWrData(std::string title) noexcept;
    TCPtResp* ForkNCBWrDataCompAck(std::string title) noexcept;
    TCPtResp* ForkWriteDataCancel(std::string title) noexcept;

    TCPtResp* ForkSnpResp(std::string title, Resp::type resp = Resp::SnpResp_I) noexcept;
    TCPtResp* ForkSnpRespData(std::string title, Resp::type resp = Resp::SnpRespData_I) noexcept;
    TCPtResp* ForkSnpRespDataPtl(std::string title, Resp::type resp = Resp::SnpRespData_I) noexcept;

    TCPtResp* ForkSnpRespFwded(std::string title, Resp::type resp = Resp::SnpResp_I_Fwded_I, bool ext = false) noexcept;
    TCPtResp* ForkSnpRespDataFwded(std::string title, Resp::type resp = Resp::SnpRespData_I_Fwded_SC, bool ext = false) noexcept;
    TCPtResp* ForkDCTCompData(std::string title, Resp::type resp = Resp::CompData_I, bool ext = false) noexcept;

    TCPtResp* ForkInstantSnpRespFwded(std::string title, Resp::type resp, Xact::CacheState state) noexcept;
    TCPtResp* ForkInstantSnpRespDataFwded(std::string title, Resp::type resp, Xact::CacheState state) noexcept;
    TCPtResp* ForkInstantDCTCompData(std::string title, Resp::type resp, Xact::CacheState state = Xact::CacheStates::None, bool checkState = false) noexcept;

    TCPtResp* LeafSnpRespFwded(std::string title, Resp::type resp, Xact::CacheState state, bool accept) noexcept;
    TCPtResp* LeafSnpRespDataFwded(std::string title, Resp::type resp, Xact::CacheState state, bool accept) noexcept;
    TCPtResp* LeafDCTCompData(std::string title, Resp::type resp, Xact::CacheState state, bool accept, bool doubleReject = false) noexcept;

    TCPtResp* Leaf(std::string title, Resp::type resp, Xact::CacheState state, bool accept, bool reserve = false, bool checkState = true) noexcept;
    TCPtResp* LeafRepeat(std::string title, Resp::type resp, Xact::CacheState state, bool accept, bool reserve = false, bool checkState = true) noexcept;
    TCPtResp* LeafFwd(std::string title, Resp::type resp, FwdState::type fwdState, Xact::CacheState state, bool accept, bool doubleReject = false, bool reserve = false, bool checkState = true) noexcept;
    TCPtResp* LeafFwdRepeat(std::string title, Resp::type resp, FwdState::type fwdState, Xact::CacheState state, bool accept, bool doubleReject = false, bool reserve = false, bool checkState = true) noexcept;

    TCPtResp* LeafNoCheck(std::string title, Resp::type resp, bool accept, bool reserve = false) noexcept;
    TCPtResp* LeafRepeatNoCheck(std::string title, Resp::type resp, bool accept, bool reserve = false) noexcept;

    TCPtResp* LeafReserve(std::string title, Resp::type resp, Xact::CacheState state, bool checkState = true) noexcept;
    TCPtResp* LeafRepeatReserve(std::string title, Resp::type resp, Xact::CacheState state, bool checkState = true) noexcept;
    TCPtResp* LeafFwdReserve(std::string title, Resp::type resp, FwdState::type fwdState, Xact::CacheState state, bool checkState = true) noexcept;
    TCPtResp* LeafFwdRepeatReserve(std::string title, Resp::type resp, FwdState::type fwdState, Xact::CacheState state, bool checkState = true) noexcept;
    
    TCPtResp* LeafReserveNoCheck(std::string title, Resp::type resp) noexcept;
    TCPtResp* LeafRepeatReserveNoCheck(std::string title, Resp::type resp) noexcept;

    TCPtResp* LeafAndForkSnpRespFwded(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::SnpResp_I_Fwded_I) noexcept;
    TCPtResp* LeafAndForkSnpRespDataFwded(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::SnpRespData_I_Fwded_SC) noexcept;
    TCPtResp* LeafAndForkDCTCompData(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::CompData_I) noexcept;

    TCPtResp* LeafRepeatAndForkSnpRespFwded(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::SnpResp_I_Fwded_I) noexcept;
    TCPtResp* LeafRepeatAndForkSnpRespDataFwded(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::SnpRespData_I_Fwded_SC) noexcept;
    TCPtResp* LeafRepeatAndForkDCTCompData(std::string title, Resp::type resp, Xact::CacheState state, Resp::type nextResp = Resp::CompData_I) noexcept;
};
