#include "tc_common_resp.hpp"
#include "common.hpp"
#include <memory>


TCPtResp* TCPtResp::ForkREQ(std::string title, Flits::REQ<config>::opcode_t opcode) noexcept
{
    if (!title.empty())
        std::cout << "[ >> ] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    forked->isSNP = false;

    forked->req.TgtID() = 2; // to #2 HN-F
    forked->req.SrcID() = 4; // from #4 RN-F
    forked->req.TxnID() = 2;
    forked->req.Opcode() = opcode;
    forked->req.Size() = Sizes::B64;
    forked->req.AllowRetry() = 1;
    forked->req.Addr() = 0;
    forked->req.ExpCompAck() = 1;

    Xact::XactDenialEnum denial
        = forked->rnJoint.NextTXREQ(nullptr, 0, forked->topo, forked->req, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        std::cout << "Unexpected denial on Fork REQ: " << denial->name << "\n";
        forked->envError = true;
    }

    return forked;
}

TCPtResp* TCPtResp::ForkSNP(std::string title, Flits::SNP<config>::opcode_t opcode, bool retToSrc, bool doNotGoToSD) noexcept
{
    if (!title.empty())
        std::cout << "[ >> ] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    forked->isSNP = true;

    forked->snp.SrcID() = 2; // from #2 HN-F
    forked->snp.FwdNID() = 10; // to #10 RN-F
    forked->snp.FwdTxnID() = 0;
    forked->snp.TxnID() = 2;
    forked->snp.Opcode() = opcode;
    forked->snp.Addr() = 0;
    forked->snp.RetToSrc() = retToSrc ? 1 : 0;
    forked->snp.DoNotGoToSD() = doNotGoToSD ? 1 : 0;

    Xact::XactDenialEnum denial
        = forked->rnJoint.NextRXSNP(nullptr, 0, forked->topo, forked->snp, 4, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        std::cout << "Unexpected denial on Fork SNP: " << denial->name << "\n";
        forked->envError = true;
    }

    return forked;
}

Flits::RSP<config> TCPtResp::GenFlitRXRSP() const noexcept
{
    Flits::RSP<config> rxrsp = { 0 };
    rxrsp.TgtID() = 4; // to #4 HN-F
    rxrsp.SrcID() = 2; // from #2 HN-F
    rxrsp.TxnID() = 2;
    rxrsp.RespErr() = RespErrs::OK;
    return rxrsp;
}

Flits::DAT<config> TCPtResp::GenFlitRXDAT() const noexcept
{
    Flits::DAT<config> rxdat = { 0 };
    rxdat.TgtID() = 4; // to #4 RN-F
    rxdat.SrcID() = 2; // from #2 HN-F
    rxdat.TxnID() = 2;
    rxdat.HomeNID() = 2;
    rxdat.RespErr() = RespErrs::OK;
    rxdat.BE() = 0xFFFFFFFFU;
    return rxdat;
}

Flits::RSP<config> TCPtResp::GenFlitTXRSP() const noexcept
{
    Flits::RSP<config> txrsp = { 0 };
    txrsp.TgtID() = 2; // to #2 HN-F
    txrsp.SrcID() = 4; // from #4 RN-F
    txrsp.TxnID() = 2;
    txrsp.RespErr() = RespErrs::OK;
    return txrsp;
}

Flits::DAT<config> TCPtResp::GenFlitTXDAT() const noexcept
{
    Flits::DAT<config> txdat = { 0 };
    txdat.TgtID() = 2; // to #2 HN-F
    txdat.SrcID() = 4; // from #4 RN-F
    txdat.TxnID() = 2;
    txdat.RespErr() = RespErrs::OK;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenDCTFlitTXDAT() const noexcept
{
    Flits::DAT<config> txdat = { 0 }; 
    txdat.TgtID() = 10; // to #10 RN-F
    txdat.SrcID() = 4; // from #4 RN-F
    txdat.TxnID() = 0;
    txdat.DBID() = 2;
    txdat.HomeNID() = 2; // #2 HN-F
    txdat.RespErr() = RespErrs::OK;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenCompData() const noexcept
{
    Flits::DAT<config> rxdat = GenFlitRXDAT();
    rxdat.Opcode() = Opcodes::DAT::CompData;
    return rxdat;
}

Flits::RSP<config> TCPtResp::GenComp() const noexcept
{
    Flits::RSP<config> rxrsp = GenFlitRXRSP();
    rxrsp.Opcode() = Opcodes::RSP::Comp;
    rxrsp.DBID() = 0;
    return rxrsp;
}

Flits::DAT<config> TCPtResp::GenDataSepResp() const noexcept
{
    Flits::DAT<config> rxdat = GenFlitRXDAT();
    rxdat.Opcode() = Opcodes::DAT::DataSepResp;
    return rxdat;
}

Flits::RSP<config> TCPtResp::GenCompStashDone() const noexcept
{
    Flits::RSP<config> rxrsp = GenFlitRXRSP();
    rxrsp.Opcode() = Opcodes::RSP::CompStashDone;
    return rxrsp;
}

Flits::RSP<config> TCPtResp::GenCompPersist() const noexcept
{
    Flits::RSP<config> rxrsp = GenFlitRXRSP();
    rxrsp.Opcode() = Opcodes::RSP::CompPersist;
    return rxrsp;
}

Flits::RSP<config> TCPtResp::GenCompDBIDResp() const noexcept
{
    Flits::RSP<config> rxrsp = GenFlitRXRSP();
    rxrsp.Opcode() = Opcodes::RSP::CompDBIDResp;
    rxrsp.DBID() = 0;
    return rxrsp;
}

Flits::RSP<config> TCPtResp::GenDBIDResp() const noexcept
{
    Flits::RSP<config> rxrsp = GenFlitRXRSP();
    rxrsp.Opcode() = Opcodes::RSP::DBIDResp;
    rxrsp.DBID() = 0;
    return rxrsp;
}

Flits::DAT<config> TCPtResp::GenCopyBackWrData() const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.TxnID() = 0;
    txdat.Opcode() = Opcodes::DAT::CopyBackWrData;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenNonCopyBackWrData() const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.TxnID() = 0;
    txdat.Opcode() = Opcodes::DAT::NonCopyBackWrData;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenNCBWrDataCompAck() const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.TxnID() = 0;
    txdat.Opcode() = Opcodes::DAT::NCBWrDataCompAck;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenWriteDataCancel() const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.TxnID() = 0;
    txdat.Opcode() = Opcodes::DAT::WriteDataCancel;
    return txdat;
}

Flits::RSP<config> TCPtResp::GenFlitSnpResp(Resp resp) const noexcept
{
    Flits::RSP<config> txrsp = GenFlitTXRSP();
    txrsp.Opcode() = Opcodes::RSP::SnpResp;
    txrsp.Resp() = resp;
    return txrsp;
}

Flits::DAT<config> TCPtResp::GenFlitSnpRespData(Resp resp) const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.Opcode() = Opcodes::DAT::SnpRespData;
    txdat.Resp() = resp;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenFlitSnpRespDataPtl(Resp resp) const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.Opcode() = Opcodes::DAT::SnpRespDataPtl;
    txdat.Resp() = resp;
    return txdat;
}

Flits::RSP<config> TCPtResp::GenFlitSnpRespFwded(Resp resp) const noexcept
{
    Flits::RSP<config> txrsp = GenFlitTXRSP();
    txrsp.Opcode() = Opcodes::RSP::SnpRespFwded;
    txrsp.Resp() = resp;
    return txrsp;
}

Flits::DAT<config> TCPtResp::GenFlitSnpRespDataFwded(Resp resp) const noexcept
{
    Flits::DAT<config> txdat = GenFlitTXDAT();
    txdat.Opcode() = Opcodes::DAT::SnpRespDataFwded;
    txdat.Resp() = resp;
    return txdat;
}

Flits::DAT<config> TCPtResp::GenDCTFlitCompData(Resp resp) const noexcept
{
    Flits::DAT<config> txdat = GenDCTFlitTXDAT();
    txdat.Opcode() = Opcodes::DAT::CompData;
    txdat.Resp() = resp;
    return txdat;
}

TCPtResp* TCPtResp::TestAndForkInitial(std::string title, Xact::CacheState state) noexcept
{
    if (!title.empty())
        std::cout << "[ ---] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    forked->rnStates.SetSilentEviction(false);
    forked->rnStates.SetSilentSharing(false);
    forked->rnStates.SetSilentStore(false);
    forked->rnStates.SetSilentInvalidation(false);
    
    if (forked->isSNP)
        forked->rnStates.Set(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, state);
    else
        forked->rnStates.Set(req.Addr(), state);

    //
    Xact::XactDenialEnum denial;
    
    if (forked->isSNP)
        denial = forked->rnStates.NextRXSNP(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, snp);
    else
        denial = forked->rnStates.NextTXREQ(req.Addr(), req);


    if (denial == Xact::XactDenial::ACCEPTED)
        ;
    else if (denial == Xact::XactDenial::DENIED_STATE_INITIAL)
    {
        std::cout << "\nThis Fork Initial was disabled due to previous fail." << "\n";
        forked->disabled = true;
    }
    else
    {
        std::cout << "\nUnexpected denial on Test And Fork Initial: " << denial->name << "\n";
        forked->envError = true;
    }

    return forked;
}

TCPtResp* TCPtResp::TestAndForkTransfer(std::string title, Xact::CacheState initial, Xact::CacheState intermediate) noexcept
{
    if (!title.empty())
        std::cout << "[ ---] " << title;

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    forked->rnStates.SetSilentEviction(false);
    forked->rnStates.SetSilentSharing(false);
    forked->rnStates.SetSilentStore(false);
    forked->rnStates.SetSilentInvalidation(false);
    
    if (isSNP)
        forked->rnStates.Set(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, initial);
    else
        forked->rnStates.Set(req.Addr(), initial);

    //
    Xact::XactDenialEnum denial;
    
    if (isSNP)
        denial = forked->rnStates.NextTXREQ(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, req);
    else
        denial = forked->rnStates.NextTXREQ(req.Addr(), req);

    if (denial == Xact::XactDenial::ACCEPTED)
        ;
    else if (denial == Xact::XactDenial::DENIED_STATE_INITIAL)
    {
        std::cout << "\nThis Fork Transfer was disabled due to previous fail." << "\n";
        forked->disabled = true;
    }
    else
    {
        std::cout << "\nUnexpected denial on Test And Fork Transfer: " << denial->name << "\n";
        forked->envError = true;
    }

    //
    if (totalCount)
        (*totalCount)++;

    if (isSNP)
        denial = forked->rnStates.Transfer(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, intermediate, forked->xaction.get());
    else
        denial = forked->rnStates.Transfer(req.Addr(), intermediate, forked->xaction.get());

    if (denial == Xact::XactDenial::ACCEPTED)
    {
        std::cout << STATE_PASS << "\n";
    }
    else
    {
        std::cout << STATE_FAIL << "\n";

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL, " ", title).ToString());
    }

    return forked;
}

TCPtResp* TCPtResp::TestAndLeafTransfer(std::string title, Xact::CacheState initial, Xact::CacheState intermediate, bool accept) noexcept
{
    if (!title.empty())
        std::cout << "[ ---] " << title;

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    forked->rnStates.SetSilentEviction(false);
    forked->rnStates.SetSilentSharing(false);
    forked->rnStates.SetSilentStore(false);
    forked->rnStates.SetSilentInvalidation(false);

    if (isSNP)
        forked->rnStates.Set(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, initial);
    else
        forked->rnStates.Set(req.Addr(), initial);

    //
    Xact::XactDenialEnum denial;
    
    if (isSNP)
        denial = forked->rnStates.NextRXSNP(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, snp);
    else
        denial = forked->rnStates.NextTXREQ(req.Addr(), req);

    if (denial == Xact::XactDenial::ACCEPTED)
        ;
    else if (denial == Xact::XactDenial::DENIED_STATE_INITIAL)
    {
        std::cout << "\nThis Leaf Transfer was disabled due to previous fail." << "\n";
        forked->disabled = true;
    }
    else
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL, " ", title).ToString());

        std::cout << "Unexpected denial on Test And Leaf Transfer: " << denial->name << "\n";
        forked->envError = true;
    }

    //
    if (totalCount)
        (*totalCount)++;

    if (isSNP)
        denial = forked->rnStates.Transfer(Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3, intermediate, forked->xaction.get());
    else
        denial = forked->rnStates.Transfer(req.Addr(), intermediate, forked->xaction.get());

    if ((denial == Xact::XactDenial::ACCEPTED) == accept)
    {
        std::cout << STATE_PASS << "\n";
    }
    else
    {
        std::cout << STATE_FAIL << "\n";

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL, " ", title).ToString());
    }

    delete forked;

    return this;
}


TCPtResp* TCPtResp::ForkCompData(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxdat = { GenCompData() };

    return forked;
}

TCPtResp* TCPtResp::ForkDataSepResp(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxdat = { GenDataSepResp() };

    return forked;
}

TCPtResp* TCPtResp::ForkComp(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxrsp = { GenComp() };

    return forked;
}

TCPtResp* TCPtResp::ForkCompStashDone(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxrsp = { GenCompStashDone() };

    return forked;
}

TCPtResp* TCPtResp::ForkCompPersist(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxrsp = { GenCompPersist() };

    return forked;
}

TCPtResp* TCPtResp::ForkCompDBIDResp(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->rxrsp = { GenCompDBIDResp() };

    return forked;
}

TCPtResp* TCPtResp::ForkInstantCompDBIDResp(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
    {
        Flits::RSP<config> rxrsp = GenCompDBIDResp();

        Xact::XactDenialEnum denial 
            = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, rxrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant CompDBIDResp: " << denial->name << "\n";

            forked->envError = true;
        }
    }

    return forked;
}

TCPtResp* TCPtResp::ForkInstantComp(std::string title, Resp resp) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
    {
        Flits::RSP<config> rxrsp = GenComp();

        Xact::XactDenialEnum denial 
            = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, rxrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant Comp: " << denial->name << "\n";

            forked->envError = true;
        }
    }

    return forked;
}

TCPtResp* TCPtResp::ForkInstantCompAndDBIDResp(std::string title, Resp resp) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
    {
        // Comp
        Flits::RSP<config> rxrsp = GenComp();

        Xact::XactDenialEnum denial 
            = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, rxrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant Comp And DBIDResp (Comp): " << denial->name << "\n";

            forked->envError = true;
        }

        // DBIDResp
        rxrsp = GenDBIDResp();

        denial = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, rxrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant Comp And DBIDResp (DBIDResp): " << denial->name << "\n";

            forked->envError = true;
        }
    }

    return forked;
}

TCPtResp* TCPtResp::ForkInstantDBIDResp(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
    {
        Flits::RSP<config> rxrsp = GenDBIDResp();

        Xact::XactDenialEnum denial 
            = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, rxrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant DBIDResp: " << denial->name << "\n";

            forked->envError = true;
        }
    }

    return forked;
}

TCPtResp* TCPtResp::ForkCopyBackWrData(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenCopyBackWrData() };

    return forked;
}

TCPtResp* TCPtResp::ForkNonCopyBackWrData(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenNonCopyBackWrData() };

    return forked;
}

TCPtResp* TCPtResp::ForkNCBWrDataCompAck(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenNCBWrDataCompAck() };

    return forked;
}

TCPtResp* TCPtResp::ForkWriteDataCancel(std::string title) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";
    
    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenWriteDataCancel() };

    return forked;
}

TCPtResp* TCPtResp::ForkSnpResp(std::string title, Resp resp) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txrsp = { GenFlitSnpResp(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkSnpRespData(std::string title, Resp resp) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenFlitSnpRespData(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkSnpRespDataPtl(std::string title, Resp resp) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << "\n";

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenFlitSnpRespDataPtl(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkSnpRespFwded(std::string title, Resp resp, bool ext) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << (ext ? "" : "\n");

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txrsp = { GenFlitSnpRespFwded(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkSnpRespDataFwded(std::string title, Resp resp, bool ext) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << (ext ? "" : "\n");

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenFlitSnpRespDataFwded(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkDCTCompData(std::string title, Resp resp, bool ext) noexcept
{
    if (!title.empty())
        std::cout << "[  --] " << title << (ext ? "" : "\n");

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!disabled)
        forked->txdat = { GenDCTFlitCompData(resp) };

    return forked;
}

TCPtResp* TCPtResp::ForkInstantSnpRespFwded(std::string title, Resp resp, Xact::CacheState state) noexcept
{
    TCPtResp* forked = ForkSnpRespFwded(title, resp, true);

    if (totalCount)
        (*totalCount)++;

    if (!disabled)
    {
        auto addr = Flits::REQ<config>::addr_t::value_type(forked->snp.Addr()) << 3;

        //
        Xact::XactDenialEnum denial
            = forked->rnJoint.NextTXRSP(nullptr, 1, forked->topo, *forked->txrsp, &forked->xaction);
        
        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant SnpRespFwded: " << denial->name << "\n";

            forked->envError = true;

            return forked;
        }

        denial = forked->rnStates.NextTXRSP(addr, *forked->xaction, *forked->txrsp);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << STATE_FAIL_0 << "\n";

            std::cout << "ForkInstantSnpRespFwded: denied by state map: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

            return forked;
        }

        if (forked->rnStates.Get(addr) != state)
        {
            std::cout << STATE_FAIL_1 << "\n";

            std::cout << "ForkInstantSnpRespFwded: state mismatch: "
                << "{" << forked->rnStates.Get(addr).ToString() << "}"
                << ", expected {" << state.ToString() << "}" << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

            return forked;
        }

        //
        std::cout << STATE_PASS << "\n";
    }
    else
    {
        std::cout << STATE_DISABLED << "\n";
    }

    return forked;
}

TCPtResp* TCPtResp::ForkInstantSnpRespDataFwded(std::string title, Resp resp, Xact::CacheState state) noexcept
{
    TCPtResp* forked = ForkSnpRespDataFwded(title, resp, true);

    if (totalCount)
        (*totalCount)++;

    if (!disabled)
    {
        Xact::XactDenialEnum denial;
        auto addr = Flits::REQ<config>::addr_t::value_type(forked->snp.Addr()) << 3;

        // Repeat 0
        forked->txdat->DataID() = 0;
        denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, *forked->txdat, &forked->xaction);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant SnpRespDataFwded (repeat 0): " << denial->name << "\n";

            forked->envError = true;

            return forked;
        }

        denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << STATE_FAIL_0 << "\n";

            std::cout << "ForkInstantSnpRespDataFwded (repeat 0): denied by state map: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

            return forked;
        }

        if (forked->rnStates.Get(addr) != state)
        {
            std::cout << STATE_FAIL_1 << "\n";

            std::cout << "ForkInstantSnpRespDataFwded (repeat 0): state mismatch: "
                << "{" << forked->rnStates.Get(addr).ToString() << "}"
                << ", expected {" << state.ToString() << "}" << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

            return forked;
        }

        // Repeat 1
        forked->txdat->DataID() = 2;
        denial = forked->rnJoint.NextTXDAT(nullptr, 2, forked->topo, *forked->txdat, &forked->xaction);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork Instant SnpRespDataFwded (repeat 1): " << denial->name << "\n";

            forked->envError = true;

            return forked;
        }

        denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << STATE_FAIL_2 << "\n";

            std::cout << "ForkInstantSnpRespDataFwded (repeat 1): denied by state map: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_2, " ", title).ToString());

            return forked;
        }

        if (forked->rnStates.Get(addr) != state)
        {
            std::cout << STATE_FAIL_3 << "\n";

            std::cout << "ForkInstantSnpRespDataFwded (repeat 1): state mismatch: "
                << "{" << forked->rnStates.Get(addr).ToString() << "}"
                << ", expected {" << state.ToString() << "}" << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_3, " ", title).ToString());

            return forked;
        }

        //
        std::cout << STATE_PASS << "\n";
    }
    else
    {
        std::cout << STATE_DISABLED << "\n";
    }

    return forked;
}

TCPtResp* TCPtResp::ForkInstantDCTCompData(std::string title, Resp resp, Xact::CacheState state, bool checkState) noexcept
{
    TCPtResp* forked = ForkDCTCompData(title, resp, true);

    if (totalCount)
        (*totalCount)++;

    if (!disabled)
    {
        Xact::XactDenialEnum denial;
        auto addr = Flits::REQ<config>::addr_t::value_type(forked->snp.Addr()) << 3;

        // Repeat 0
        forked->txdat->DataID() = 0;
        denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, *forked->txdat, &forked->xaction);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork DCT CompData (repeat 0): " << denial->name << "\n";

            forked->envError = true;

            return forked;
        }

        denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << STATE_FAIL_0 << "\n";

            std::cout << "ForkInstantDCTCompData (repeat 0): denied by state map: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

            return forked;
        }

        if (checkState && forked->rnStates.Get(addr) != state)
        {
            std::cout << STATE_FAIL_1 << "\n";

            std::cout << "ForkInstantDCTCompData (repeat 0): state mismatch: "
                << "{" << forked->rnStates.Get(addr).ToString() << "}"
                << ", expected {" << state.ToString() << "}" << std::endl;
            
            if (errCountFail)
                (*errCountFail)++;
            
            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

            return forked;
        }

        // Repeat 1
        forked->txdat->DataID() = 2;
        denial = forked->rnJoint.NextTXDAT(nullptr, 2, forked->topo, *forked->txdat, &forked->xaction);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << "Unexpected denial on Fork DCT CompData (repeat 1): " << denial->name << "\n";

            forked->envError = true;

            return forked;
        }

        denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);

        if (denial != Xact::XactDenial::ACCEPTED)
        {
            std::cout << STATE_FAIL_2 << "\n";

            std::cout << "ForkDCTCompData (repeat 1): denied by state map: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_2, "", title).ToString());

            return forked;
        }

        if (checkState && forked->rnStates.Get(addr) != state)
        {
            std::cout << STATE_FAIL_3 << "\n";

            std::cout << "ForkDCTCompData (repeat 1): state mismatch: "
                << "{" << forked->rnStates.Get(addr).ToString() << "}"
                << ", expected {" << state.ToString() << "}" << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_3, " ", title).ToString());

            return forked;
        }

        //
        std::cout << STATE_PASS << "\n";
    }
    else
    {
        std::cout << STATE_DISABLED << std::endl;
    }

    return forked;
}

TCPtResp* TCPtResp::LeafSnpRespFwded(std::string title, Resp resp, Xact::CacheState state, bool accept) noexcept
{
    //
    if (totalCount)
        (*totalCount)++;

    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(4).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    if (disabled)
    {
        std::cout << STATE_DISABLED << "\n";

        return this;
    }

    //
    if (envError)
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;
        
        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        return this;
    }

    //
    auto addr = Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3;

    Flits::RSP<config> txrsp = GenFlitSnpRespFwded(resp);

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    Xact::XactDenialEnum denial;

    //
    denial = forked->rnJoint.NextTXRSP(nullptr, 1, forked->topo, txrsp, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        forked->envError = true;

        std::cout << STATE_ENVERROR << "\n";
        std::cout << "Unexpected denial on Leaf SnpRespFwded: " << denial->name << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        delete forked;
        return this;
    }

    denial = forked->rnStates.NextTXRSP(addr, *forked->xaction, txrsp);

    if ((denial == Xact::XactDenial::ACCEPTED) != accept)
    {
        std::cout << STATE_FAIL_0 << "\n";

        std::cout << "LeafSnpRespFwded: denied by state map: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;
        
        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

        delete forked;
        return this;
    }

    if (!accept)
    {
        std::cout << STATE_PASS << std::endl;

        delete forked;
        return this;
    }

    if (forked->rnStates.Get(addr) != state)
    {
        std::cout << STATE_FAIL_1 << "\n";

        std::cout << "LeafSnpRespFwded: state mismatch: "
            << "{" << forked->rnStates.Get(addr).ToString() << "}"
            << ", expected {" << state.ToString() << "}" << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

        delete forked;
        return this;
    }

    std::cout << STATE_PASS << std::endl;

    delete forked;
    return this;
}

TCPtResp* TCPtResp::LeafSnpRespDataFwded(std::string title, Resp resp, Xact::CacheState state, bool accept) noexcept
{
    //
    if (totalCount)
        (*totalCount)++;

    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(4).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    if (disabled)
    {
        std::cout << STATE_DISABLED << "\n";

        return this;
    }

    //
    if (envError)
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;
        
        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        return this;
    }

    //
    auto addr = Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3;

    Flits::DAT<config> txdat = GenFlitSnpRespDataFwded(resp);

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    Xact::XactDenialEnum denial;

    // Repeat 0
    txdat.DataID() = 0;
    denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, txdat, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        forked->envError = true;

        std::cout << STATE_ENVERROR << "\n";
        std::cout << "Unexpected denial on LeafSnpRespDataFwded (repeat = 0): " << denial->name << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        delete forked;
        return this;
    }

    denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, txdat);

    if ((denial == Xact::XactDenial::ACCEPTED) != accept)
    {
        std::cout << STATE_FAIL_0 << "\n";

        std::cout << "LeafSnpRespDataFwded (repeat = 0): denied by state map: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

        delete forked;
        return this;
    }

    if (!accept)
    {
        std::cout << STATE_PASS << std::endl;

        delete forked;
        return this;
    }

    if (forked->rnStates.Get(addr) != state)
    {
        std::cout << STATE_FAIL_1 << "\n";

        std::cout << "LeafSnpRespDataFwded (repeat = 0): state mismatch: "
            << "{" << forked->rnStates.Get(addr).ToString() << "}"
            << ", expected {" << state.ToString() << "}" << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

        delete forked;
        return this;
    }

    // Repeat 1
    txdat.DataID() = 2;
    denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, txdat, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        forked->envError = true;

        std::cout << STATE_ENVERROR << "\n";
        std::cout << "Unexpected denial on LeafSnpRespDataFwded (repeat = 1): " << denial->name << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        delete forked;
        return this;
    }

    denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, txdat);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        std::cout << STATE_FAIL_2 << "\n";

        std::cout << "LeafSnpRespDataFwded (repeat = 1): denied by state map: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_2, " ", title).ToString());

        delete forked;
        return this;
    }

    if (forked->rnStates.Get(addr) != state)
    {
        std::cout << STATE_FAIL_3 << "\n";

        std::cout << "LeafSnpRespDataFwded (repeat = 1): state mismatch: "
            << "{" << forked->rnStates.Get(addr).ToString() << "}"
            << ", expected {" << state.ToString() << "}" << std::endl;

        if (errCountFail)
            (*errCountFail)++;
        
        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_3, " ", title).ToString());

        delete forked;
        return this;
    }

    std::cout << STATE_PASS << std::endl;

    delete forked;
    return this;
}

TCPtResp* TCPtResp::LeafDCTCompData(std::string title, Resp resp, Xact::CacheState state, bool accept, bool doubleReject) noexcept
{
    //
    if (totalCount)
        (*totalCount)++;

    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(4).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    if (disabled)
    {
        std::cout << STATE_DISABLED << "\n";

        return this;
    }

    //
    if (envError)
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;
        
        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        return this;
    }

    //
    auto addr = Flits::REQ<config>::addr_t::value_type(snp.Addr()) << 3;

    Flits::DAT<config> txdat = GenDCTFlitCompData(resp);

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    Xact::XactDenialEnum denial;

    // Repeat 0
    txdat.DataID() = 0;
    denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, txdat, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        if (!accept && doubleReject)
            ;
        else
        {
            forked->envError = true;

            std::cout << STATE_ENVERROR << "\n";
            std::cout << "Unexpected denial on LeafDCTCompData (repeat = 0): " << denial->name << "\n";

            if (errCountEnvError)
                (*errCountEnvError)++;

            if (errList)
                errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

            delete forked;
            return this;
        }
    }
    else if (!accept && doubleReject)
    {
        std::cout << STATE_FAIL_0 << "\n";
        std::cout << "LeafDCTCompData (repeat = 0, doubleReject = 1): denied by xaction: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

        delete forked;
        return this;
    }

    denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, txdat);

    if ((denial == Xact::XactDenial::ACCEPTED) != accept)
    {
        std::cout << STATE_FAIL_0 << "\n";

        std::cout << "LeafDCTCompData (repeat = 0): denied by state map: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());

        delete forked;
        return this;
    }

    if (!accept)
    {
        std::cout << STATE_PASS << std::endl;

        delete forked;
        return this;
    }

    if (forked->rnStates.Get(addr) != state)
    {
        std::cout << STATE_FAIL_1 << "\n";

        std::cout << "LeafDCTCompData (repeat = 0): state mismatch: "
            << "{" << forked->rnStates.Get(addr).ToString() << "}"
            << ", expected {" << state.ToString() << "}" << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());

        delete forked;
        return this;
    }

    // Repeat 1
    txdat.DataID() = 2;
    denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, txdat, &forked->xaction);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        forked->envError = true;

        std::cout << STATE_ENVERROR << "\n";
        std::cout << "Unexpected denial on LeafDCTCompData (repeat = 1): " << denial->name << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        delete forked;
        return this;
    }

    denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, txdat);

    if (denial != Xact::XactDenial::ACCEPTED)
    {
        std::cout << STATE_FAIL_2 << "\n";

        std::cout << "LeafDCTCompData (repeat = 1): denied by state map: " << denial->name << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_2, " ", title).ToString());

        delete forked;
        return this;
    }

    if (forked->rnStates.Get(addr) != state)
    {
        std::cout << STATE_FAIL_3 << "\n";

        std::cout << "LeafDCTCompData (repeat = 1): state mismatch: "
            << "{" << forked->rnStates.Get(addr).ToString() << "}"
            << ", expected {" << state.ToString() << "}" << std::endl;

        if (errCountFail)
            (*errCountFail)++;

        if (errList)
            errList->push_back(StringAppender(STATE_FAIL_3, " ", title).ToString());

        delete forked;
        return this;
    }

    std::cout << STATE_PASS << std::endl;

    delete forked;
    return this;
}

TCPtResp* TCPtResp::Leaf(std::string title, Resp resp, Xact::CacheState state, bool accept, bool reserve, bool checkState) noexcept
{
    return LeafFwd(title, resp, FwdStates::I, state, accept, false, reserve, checkState);
}

TCPtResp* TCPtResp::LeafFwd(std::string title, Resp resp, FwdState fwdState, Xact::CacheState state, bool accept, bool doubleReject, bool reserve, bool checkState) noexcept
{
    //
    if (totalCount)
        (*totalCount)++;

    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(4).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    if (disabled)
    {
        std::cout << STATE_DISABLED << "\n";

        return this;
    }

    //
    if (envError)
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        return this;
    }

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!envError)
    {
        std::string channel;
        Flits::REQ<config>::addr_t::value_type addr;

        if (forked->isSNP)
            addr = Flits::REQ<config>::addr_t::value_type(forked->snp.Addr()) << 3;
        else
            addr = forked->req.Addr();

        if (forked->rxdat)
        {
            channel = "RXDAT";
            forked->rxdat->Resp() = resp;
            forked->rxdat->FwdState() = fwdState;
        }
        else if (forked->rxrsp)
        {
            channel = "RXRSP";
            forked->rxrsp->Resp() = resp;
            forked->rxrsp->FwdState() = fwdState;
        }
        else if (forked->txdat)
        {
            channel = "TXDAT";
            forked->txdat->Resp() = resp;
            forked->txdat->FwdState() = fwdState;
        }
        else if (forked->txrsp)
        {
            channel = "TXRSP";
            forked->txrsp->Resp() = resp;
            forked->txrsp->FwdState() = fwdState;
        }

        Xact::XactDenialEnum denial;

        if (forked->rxdat)
            denial = forked->rnJoint.NextRXDAT(nullptr, 1, forked->topo, *forked->rxdat, &forked->xaction);
        else if (forked->rxrsp)
            denial = forked->rnJoint.NextRXRSP(nullptr, 1, forked->topo, *forked->rxrsp, &forked->xaction);
        else if (forked->txdat)
            denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, *forked->txdat, &forked->xaction);
        else if (forked->txrsp)
            denial = forked->rnJoint.NextTXRSP(nullptr, 1, forked->topo, *forked->txrsp, &forked->xaction);
        else
            denial = Xact::XactDenial::ACCEPTED;

        bool firstPass = false;
        if (denial != Xact::XactDenial::ACCEPTED)
        {
            if (!accept && doubleReject)
                firstPass = true;
            else
            {
                forked->envError = true;

                std::cout << STATE_ENVERROR << "\n";
                std::cout << "Unexpected denial on Leaf (" << channel << "): " << denial->name << "\n";

                if (errCountEnvError)
                    (*errCountEnvError)++;

                if (errList)
                    errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());
            }
        }
        else if (!accept && doubleReject)
        {
            std::cout << STATE_FAIL_0 << "\n";

            std::cout << "LeafFwd (doubleReject = 1): denied by xaction: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());
        }
        else
            firstPass = true;

        if (firstPass)
        {
            if (forked->rxdat)
                denial = forked->rnStates.NextRXDAT(addr, *forked->xaction, *forked->rxdat);
            else if (forked->rxrsp)
                denial = forked->rnStates.NextRXRSP(addr, *forked->xaction, *forked->rxrsp);
            else if (forked->txdat)
                denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);
            else if (forked->txrsp)
                denial = forked->rnStates.NextTXRSP(addr, *forked->xaction, *forked->txrsp);
            else
                denial = Xact::XactDenial::ACCEPTED;

            if ((denial == Xact::XactDenial::ACCEPTED) != accept)
            {
                std::cout << STATE_FAIL_0 << "\n";

                std::cout << "LeafFwd: denied by state map: " << denial->name << std::endl;

                if (errCountFail)
                    (*errCountFail)++;

                if (errList)
                    errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());
            }
            else
            {
                if (accept && checkState && !(forked->rnStates.Get(addr) == state))
                {
                    std::cout << STATE_FAIL_1 << "\n";

                    std::cout << "LeafFwd: state mismatch: "
                        << "{" << forked->rnStates.Get(addr).ToString() << "}"
                        << ", expected {" << state.ToString() << "}" << std::endl;

                    if (errCountFail)
                        (*errCountFail)++;

                    if (errList)
                        errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());
                }
                else
                {
                    std::cout << STATE_PASS << "\n";
                }
            }
        }
    }
    else
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());
    }

    if (!reserve)
    {
        delete forked;
        return this;
    }
    else
    {
        forked->txrsp = std::nullopt;
        forked->txdat = std::nullopt;
        forked->rxrsp = std::nullopt;
        forked->rxdat = std::nullopt;
        return forked;
    }
}

TCPtResp* TCPtResp::LeafRepeat(std::string title, Resp resp, Xact::CacheState state, bool accept, bool reserve, bool checkState) noexcept
{
    return LeafFwdRepeat(title, resp, FwdStates::I, state , accept, false, reserve, checkState);
}

TCPtResp* TCPtResp::LeafFwdRepeat(std::string title, Resp resp, FwdState fwdState, Xact::CacheState state, bool accept, bool doubleReject, bool reserve, bool checkState) noexcept
{
    //
    if (totalCount)
        (*totalCount)++;

    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(4).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    if (disabled)
    {
        std::cout << STATE_DISABLED << "\n";

        return this;
    }

    //
    if (envError)
    {
        std::cout << STATE_ENVERROR << "\n";

        if (errCountEnvError)
            (*errCountEnvError)++;

        if (errList)
            errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());

        return this;
    }

    //
    TCPtResp* forked = new TCPtResp(*this);
    forked->prev = this;

    if (!envError)
    {
        Flits::REQ<config>::addr_t::value_type addr;

        if (forked->isSNP)
            addr = Flits::REQ<config>::addr_t::value_type(forked->snp.Addr()) << 3;
        else
            addr = forked->req.Addr();

        if (forked->rxdat)
        {
            forked->rxdat->Resp() = resp;
            forked->rxdat->FwdState() = fwdState;
        }
        else if (forked->txdat)
        {
            forked->txdat->Resp() = resp;
            forked->txdat->FwdState() = fwdState;
        }
        else
            assert(false && "only RXDAT & TXDAT could be repeated on leaf");

        Xact::XactDenialEnum denial;

        if (forked->rxdat)
        {
            forked->rxdat->DataID() = 0;
            denial = forked->rnJoint.NextRXDAT(nullptr, 1, forked->topo, *forked->rxdat, &forked->xaction);
        }
        else if (forked->txdat)
        {
            forked->txdat->DataID() = 0;
            denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, *forked->txdat, &forked->xaction);
        }
        else
            denial = Xact::XactDenial::ACCEPTED;

        bool firstPass = false;
        if (denial != Xact::XactDenial::ACCEPTED)
        {
            if (!accept && doubleReject)
                firstPass = true;
            else
            {
                forked->envError = true;

                std::cout << STATE_ENVERROR << "\n";
                std::cout << "Unexpected denial on Leaf Repeat (beat = 0): " << denial->name << "\n";

                if (errCountEnvError)
                    (*errCountEnvError)++;

                if (errList)
                    errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());
            }
        }
        else if (!accept && doubleReject)
        {
            std::cout << STATE_FAIL_0 << "\n";

            std::cout << "LeafFwdRepeat (doubleReject = 1): denied by xaction: " << denial->name << std::endl;

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());
        }
        else
            firstPass = true;

        if (firstPass)
        {
            if (forked->rxdat)
                denial = forked->rnStates.NextRXDAT(addr, *forked->xaction, *forked->rxdat);
            else if (forked->txdat)
                denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);
            else
                denial = Xact::XactDenial::ACCEPTED;

            if ((denial == Xact::XactDenial::ACCEPTED) != accept)
            {
                std::cout << STATE_FAIL_0 << "\n";

                std::cout << "LeafRepeat: denied by state map: " << denial->name << std::endl;

                if (errCountFail)
                    (*errCountFail)++;

                if (errList)
                    errList->push_back(StringAppender(STATE_FAIL_0, " ", title).ToString());
            }
            else if (accept)
            {
                if (checkState && forked->rnStates.Get(addr) != state)
                {
                    std::cout << STATE_FAIL_1 << "\n";

                    std::cout << "LeafRepeat: state mismatch: "
                        << "{" << forked->rnStates.Get(addr).ToString() << "}"
                        << ", expected {" << state.ToString() << "}" << std::endl;

                    if (errCountFail)
                        (*errCountFail)++;

                    if (errList)
                        errList->push_back(StringAppender(STATE_FAIL_1, " ", title).ToString());
                }
                else
                {
                    // repeat

                    if (forked->rxdat)
                    {
                        forked->rxdat->DataID() = 2;
                        denial = forked->rnJoint.NextRXDAT(nullptr, 1, forked->topo, *forked->rxdat, &forked->xaction);
                    }
                    else if (forked->txdat)
                    {
                        forked->txdat->DataID() = 2;
                        denial = forked->rnJoint.NextTXDAT(nullptr, 1, forked->topo, *forked->txdat, &forked->xaction);
                    }
                    else
                        denial = Xact::XactDenial::ACCEPTED;

                    if (denial == Xact::XactDenial::ACCEPTED)
                    {
                        if (forked->rxdat)
                            denial = forked->rnStates.NextRXDAT(addr, *forked->xaction, *forked->rxdat);
                        else if (forked->txdat)
                            denial = forked->rnStates.NextTXDAT(addr, *forked->xaction, *forked->txdat);
                        else
                            denial = Xact::XactDenial::ACCEPTED;

                        if (denial != Xact::XactDenial::ACCEPTED)
                        {
                            std::cout << STATE_FAIL_2 << "\n";

                            std::cout << "LeafRepeat: denied by state map: " << denial->name << std::endl;

                            if (errCountFail)
                                (*errCountFail)++;

                            if (errList)
                                errList->push_back(StringAppender(STATE_FAIL_2, " ", title).ToString());
                        }
                        else
                        {
                            if (checkState && forked->rnStates.Get(addr) != state)
                            {
                                std::cout << STATE_FAIL_3 << "\n";

                                std::cout << "LeafRepeat: state mismatch: "
                                    << "{" << forked->rnStates.Get(addr).ToString() << "}"
                                    << ", expected {" << state.ToString() << "}" << std::endl;

                                if (errCountFail)
                                    (*errCountFail)++;

                                if (errList)
                                    errList->push_back(StringAppender(STATE_FAIL_3, " ", title).ToString());
                            }
                            else
                            {
                                std::cout << STATE_PASS << "\n";
                            }
                        }
                    }
                    else
                    {
                        forked->envError = true;

                        std::cout << STATE_ENVERROR << "\n";
                        std::cout << "Unexpected denial on Leaf Repeat (beat = 1): " << denial->name << "\n";

                        if (errCountEnvError)
                            (*errCountEnvError)++;

                        if (errList)
                        errList->push_back(StringAppender(STATE_ENVERROR, " ", title).ToString());
                    }
                }
            }
            else
            {
                std::cout << STATE_PASS << "\n";
            }
        }
    }

    if (!reserve)
    {
        delete forked;
        return this;
    }
    else
    {
        forked->txrsp = std::nullopt;
        forked->txdat = std::nullopt;
        forked->rxrsp = std::nullopt;
        forked->rxdat = std::nullopt;
        return forked;
    }
}

TCPtResp* TCPtResp::LeafNoCheck(std::string title, Resp resp, bool accept, bool reserve) noexcept
{
    return Leaf(title, resp, Xact::CacheStates::None, accept, reserve, false);
}

TCPtResp* TCPtResp::LeafRepeatNoCheck(std::string title, Resp resp, bool accept, bool reserve) noexcept
{
    return LeafRepeat(title, resp, Xact::CacheStates::None, accept, reserve, false);
}

TCPtResp* TCPtResp::LeafReserve(std::string title, Resp resp, Xact::CacheState state, bool checkState) noexcept
{
    return Leaf(title, resp, state, true, true, checkState);
}

TCPtResp* TCPtResp::LeafRepeatReserve(std::string title, Resp resp, Xact::CacheState state, bool checkState) noexcept
{
    return LeafRepeat(title, resp, state, true, true, checkState);
}

TCPtResp* TCPtResp::LeafFwdReserve(std::string title, Resp resp, FwdState fwdState, Xact::CacheState state, bool checkState) noexcept
{
    return LeafFwd(title, resp, fwdState, state, true, false, true, checkState);
}

TCPtResp* TCPtResp::LeafFwdRepeatReserve(std::string title, Resp resp, FwdState fwdState, Xact::CacheState state, bool checkState) noexcept
{
    return LeafFwdRepeat(title, resp, fwdState, state, true, true, checkState);
}

TCPtResp* TCPtResp::LeafReserveNoCheck(std::string title, Resp resp) noexcept
{
    return Leaf(title, resp, Xact::CacheStates::None, true, true, false);
}

TCPtResp* TCPtResp::LeafRepeatReserveNoCheck(std::string title, Resp resp) noexcept
{
    return LeafRepeat(title, resp, Xact::CacheStates::None, true, true, false);
}

TCPtResp* TCPtResp::LeafAndForkSnpRespFwded(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = Leaf(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txrsp = { GenFlitSnpRespFwded(nextResp) };

    return forked;
}

TCPtResp* TCPtResp::LeafAndForkSnpRespDataFwded(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = Leaf(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txdat = { GenFlitSnpRespDataFwded(resp) };

    return forked;
}

TCPtResp* TCPtResp::LeafAndForkDCTCompData(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = Leaf(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txdat = { GenDCTFlitCompData(resp) };

    return forked;
}

TCPtResp* TCPtResp::LeafRepeatAndForkSnpRespFwded(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = LeafRepeat(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txrsp = { GenFlitSnpRespFwded(resp) };

    return forked;
}

TCPtResp* TCPtResp::LeafRepeatAndForkSnpRespDataFwded(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = LeafRepeat(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txdat = { GenFlitSnpRespDataFwded(resp) };

    return forked;
}

TCPtResp* TCPtResp::LeafRepeatAndForkDCTCompData(std::string title, Resp resp, Xact::CacheState state, Resp nextResp) noexcept
{
    TCPtResp* forked = LeafRepeat(title, resp, state, true, true);

    forked->txrsp = std::nullopt;
    forked->txdat = std::nullopt;

    if (!forked->disabled)
        forked->txdat = { GenDCTFlitCompData(resp) };

    return forked;
}
