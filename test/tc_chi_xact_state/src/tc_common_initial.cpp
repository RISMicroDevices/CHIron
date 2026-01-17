#include "tc_common_initial.hpp"
#include "common.hpp"


TCPtInitial* TCPtInitial::ForkREQ(std::string title, Flits::REQ<config>::opcode_t opcode) noexcept
{
    if (!title.empty())
        std::cout << "[  >> ] " << title << "\n";

    //
    TCPtInitial* forked = new TCPtInitial(*this);
    forked->prev = this;
    forked->req.TgtID() = 2; // to #2 HN-F
    forked->req.SrcID() = 4; // from #4 RN-F
    forked->req.TxnID() = 1;
    forked->req.Opcode() = opcode;
    forked->req.Size() = Sizes::B64;
    forked->req.AllowRetry() = 1;
    forked->req.Addr() = addr;

    Xact::XactDenialEnum denial 
        = forked->rnJoint.NextTXREQ(glbl, 0, forked->req, &forked->xaction);
    
    if (denial != Xact::XactDenial::ACCEPTED)
    {
        std::cout << "Unexpected denial on Fork REQ: " << denial->name << "\n";
        forked->envError = true;
    }

    return forked;
}

TCPtInitial* TCPtInitial::ForkSilentTransitions(
    std::string     title,
    bool            enableSilentEviction,
    bool            enableSilentSharing,
    bool            enableSilentStore,
    bool            enableSilentInvalidation
) noexcept
{
    if (!title.empty())
        std::cout << "[-----] " << title << "\n";

    //
    TCPtInitial* forked = new TCPtInitial(*this);
    forked->prev = this;

    forked->rnStates.SetSilentEviction(enableSilentEviction);
    forked->rnStates.SetSilentSharing(enableSilentSharing);
    forked->rnStates.SetSilentStore(enableSilentStore);
    forked->rnStates.SetSilentInvalidation(enableSilentInvalidation);

    return forked;
}

TCPtInitial* TCPtInitial::Leaf(std::string title, Xact::CacheState state, bool accept) noexcept
{
    if (!title.empty())
        std::cout 
            << StringAppender("[")
                .NextWidth(5).Fill(' ').Left().Append(totalCount ? *totalCount : 0)
                .Append("] ")
                .ToString() 
            << title;

    //
    TCPtInitial* forked = new TCPtInitial(*this);

    Xact::XactDenialEnum denial;
    if (!envError)
    {
        forked->rnStates.Set(addr, state);
        denial = forked->rnStates.NextTXREQ(addr, req);
            
        if ((denial == Xact::XactDenial::ACCEPTED) != accept)
        {
            std::cout << STATE_FAIL << "\n";

            if (errCountFail)
                (*errCountFail)++;

            if (errList)
                errList->push_back(StringAppender(STATE_FAIL, " ", title).ToString());
        }
        else
        {
            std::cout << STATE_PASS << "\n";
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

    delete forked;

    //
    if (totalCount)
        (*totalCount)++;

    return this;
}

