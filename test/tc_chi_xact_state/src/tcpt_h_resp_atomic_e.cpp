#include "tc_common_resp.hpp"           // IWYU pragma: keep
#include "tcpt_h_resp_atomic.hpp"       // IWYU pragma: keep

#ifdef CHI_ISSUE_E

void TCPtHRespAtomic(
    size_t*                     totalCount,
    size_t*                     errCountFail,
    size_t*                     errCountEnvError,
    std::vector<std::string>*   errList,
    const Xact::Topology&       topo) noexcept
{
    TCPtResp* tcptResp = (new TCPtResp())
        ->TotalCount(totalCount)
        ->ErrCountFail(errCountFail)
        ->ErrCountEnvError(errCountEnvError)
        ->ErrList(errList)
        ->Topology(topo)
        ->Begin();

    tcptResp
        ->ForkREQ("# TCPt H.1. Response of AtomicStore::ADD", Opcodes::REQ::AtomicStore::ADD)

            ->TestAndForkInitial("## H.1.1. I   -> AtomicStore::ADD", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.1.1.1. I   -> AtomicStore::ADD -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.1.1.1.1. I   -> AtomicStore::ADD -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.1.1.2. I   -> AtomicStore::ADD -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.1.1.3. I   -> AtomicStore::ADD -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.1.1.4. I   -> AtomicStore::ADD -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.1.1.5. I   -> AtomicStore::ADD -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.1.1.5.1. I   -> AtomicStore::ADD -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.2. Response of AtomicStore::CLR", Opcodes::REQ::AtomicStore::CLR)

            ->TestAndForkInitial("## H.2.1. I   -> AtomicStore::CLR", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.2.1.1. I   -> AtomicStore::CLR -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.2.1.1.1. I   -> AtomicStore::CLR -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.2.1.2. I   -> AtomicStore::CLR -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.2.1.3. I   -> AtomicStore::CLR -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.2.1.4. I   -> AtomicStore::CLR -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.2.1.5. I   -> AtomicStore::CLR -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.2.1.5.1. I   -> AtomicStore::CLR -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.3. Response of AtomicStore::EOR", Opcodes::REQ::AtomicStore::EOR)

            ->TestAndForkInitial("## H.3.1. I   -> AtomicStore::EOR", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.3.1.1. I   -> AtomicStore::EOR -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.3.1.1.1. I   -> AtomicStore::EOR -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.3.1.2. I   -> AtomicStore::EOR -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.3.1.3. I   -> AtomicStore::EOR -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.3.1.4. I   -> AtomicStore::EOR -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.3.1.5. I   -> AtomicStore::EOR -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.3.1.5.1. I   -> AtomicStore::EOR -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.4. Response of AtomicStore::SET", Opcodes::REQ::AtomicStore::SET)

            ->TestAndForkInitial("## H.4.1. I   -> AtomicStore::SET", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.4.1.1. I   -> AtomicStore::SET -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.4.1.1.1. I   -> AtomicStore::SET -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.4.1.2. I   -> AtomicStore::SET -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.4.1.3. I   -> AtomicStore::SET -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.4.1.4. I   -> AtomicStore::SET -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.4.1.5. I   -> AtomicStore::SET -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.4.1.5.1. I   -> AtomicStore::SET -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.5. Response of AtomicStore::SMAX", Opcodes::REQ::AtomicStore::SMAX)

            ->TestAndForkInitial("## H.5.1. I   -> AtomicStore::SMAX", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.5.1.1. I   -> AtomicStore::SMAX -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.5.1.1.1. I   -> AtomicStore::SMAX -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.5.1.2. I   -> AtomicStore::SMAX -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.5.1.3. I   -> AtomicStore::SMAX -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.5.1.4. I   -> AtomicStore::SMAX -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.5.1.5. I   -> AtomicStore::SMAX -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.5.1.5.1. I   -> AtomicStore::SMAX -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.6. Response of AtomicStore::SMIN", Opcodes::REQ::AtomicStore::SMIN)

            ->TestAndForkInitial("## H.6.1. I   -> AtomicStore::SMIN", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.6.1.1. I   -> AtomicStore::SMIN -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.6.1.1.1. I   -> AtomicStore::SMIN -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.6.1.2. I   -> AtomicStore::SMIN -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.6.1.3. I   -> AtomicStore::SMIN -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.6.1.4. I   -> AtomicStore::SMIN -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.6.1.5. I   -> AtomicStore::SMIN -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.6.1.5.1. I   -> AtomicStore::SMIN -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.7. Response of AtomicStore::UMAX", Opcodes::REQ::AtomicStore::UMAX)

            ->TestAndForkInitial("## H.7.1. I   -> AtomicStore::UMAX", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.7.1.1. I   -> AtomicStore::UMAX -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.7.1.1.1. I   -> AtomicStore::UMAX -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.7.1.2. I   -> AtomicStore::UMAX -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.7.1.3. I   -> AtomicStore::UMAX -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.7.1.4. I   -> AtomicStore::UMAX -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.7.1.5. I   -> AtomicStore::UMAX -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.7.1.5.1. I   -> AtomicStore::UMAX -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.8. Response of AtomicStore::UMIN", Opcodes::REQ::AtomicStore::UMIN)

            ->TestAndForkInitial("## H.8.1. I   -> AtomicStore::UMIN", Xact::CacheStates::I)
                ->ForkInstantCompAndDBIDResp("### H.8.1.1. I   -> AtomicStore::UMIN -> Comp_I", Resps::Comp_I)
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.8.1.1.1. I   -> AtomicStore::UMIN -> Comp_I -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
                ->ForkComp("")
                    ->Leaf("### H.8.1.2. I   -> AtomicStore::UMIN -> Comp_UC    ", Resps::Comp_UC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.8.1.3. I   -> AtomicStore::UMIN -> Comp_SC    ", Resps::Comp_SC     , Xact::CacheStates::None, false)
                    ->Leaf("### H.8.1.4. I   -> AtomicStore::UMIN -> Comp_UD_PD ", Resps::Comp_UD_PD  , Xact::CacheStates::None, false)
                ->Rest()
                ->ForkInstantCompDBIDResp("### H.8.1.5. I   -> AtomicStore::UMIN -> CompDBIDResp ")
                    ->ForkNonCopyBackWrData("")
                        ->Leaf("#### H.8.1.5.1. I   -> AtomicStore::UMIN -> CompDBIDResp -> NonCopyBackWrData -> I ", Resps::NonCopyBackWrData, Xact::CacheStates::I, true)
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.9. Response of AtomicLoad::ADD", Opcodes::REQ::AtomicLoad::ADD)

            ->TestAndForkInitial("## H.9.1. I   -> AtomicLoad::ADD", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.9.1.1. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.9.1.1.1. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.9.1.1.2. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.9.1.1.3. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.9.1.1.4. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.9.1.1.5. I   -> AtomicLoad::ADD -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.10. Response of AtomicLoad::CLR", Opcodes::REQ::AtomicLoad::CLR)

            ->TestAndForkInitial("## H.10.1. I   -> AtomicLoad::CLR", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.10.1.1. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.10.1.1.1. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.10.1.1.2. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.10.1.1.3. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.10.1.1.4. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.10.1.1.5. I   -> AtomicLoad::CLR -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.11. Response of AtomicLoad::EOR", Opcodes::REQ::AtomicLoad::EOR)

            ->TestAndForkInitial("## H.11.1. I   -> AtomicLoad::EOR", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.11.1.1. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.11.1.1.1. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.11.1.1.2. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.11.1.1.3. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.11.1.1.4. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.11.1.1.5. I   -> AtomicLoad::EOR -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.12. Response of AtomicLoad::SET", Opcodes::REQ::AtomicLoad::SET)

            ->TestAndForkInitial("## H.12.1. I   -> AtomicLoad::SET", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.12.1.1. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.12.1.1.1. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.12.1.1.2. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.12.1.1.3. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.12.1.1.4. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.12.1.1.5. I   -> AtomicLoad::SET -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.13. Response of AtomicLoad::SMAX", Opcodes::REQ::AtomicLoad::SMAX)

            ->TestAndForkInitial("## H.13.1. I   -> AtomicLoad::SMAX", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.13.1.1. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.13.1.1.1. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.13.1.1.2. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.13.1.1.3. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.13.1.1.4. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.13.1.1.5. I   -> AtomicLoad::SMAX -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.14. Response of AtomicLoad::SMIN", Opcodes::REQ::AtomicLoad::SMIN)

            ->TestAndForkInitial("## H.14.1. I   -> AtomicLoad::SMIN", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.14.1.1. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.14.1.1.1. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.14.1.1.2. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.14.1.1.3. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.14.1.1.4. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.14.1.1.5. I   -> AtomicLoad::SMIN -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.15. Response of AtomicLoad::UMAX", Opcodes::REQ::AtomicLoad::UMAX)

            ->TestAndForkInitial("## H.15.1. I   -> AtomicLoad::UMAX", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.15.1.1. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.15.1.1.1. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.15.1.1.2. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.15.1.1.3. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.15.1.1.4. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.15.1.1.5. I   -> AtomicLoad::UMAX -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.16. Response of AtomicLoad::UMIN", Opcodes::REQ::AtomicLoad::UMIN)

            ->TestAndForkInitial("## H.16.1. I   -> AtomicLoad::UMIN", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.16.1.1. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.16.1.1.1. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.16.1.1.2. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.16.1.1.3. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.16.1.1.4. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.16.1.1.5. I   -> AtomicLoad::UMIN -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.17. Response of AtomicSwap", Opcodes::REQ::AtomicSwap)

            ->TestAndForkInitial("## H.17.1. I   -> AtomicSwap", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.17.1.1. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.17.1.1.1. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.17.1.1.2. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.17.1.1.3. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.17.1.1.4. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.17.1.1.5. I   -> AtomicSwap -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp
        ->ForkREQ("# TCPt H.18. Response of AtomicCompare", Opcodes::REQ::AtomicCompare)

            ->TestAndForkInitial("## H.18.1. I   -> AtomicCompare", Xact::CacheStates::I)
                ->ForkInstantDBIDResp("")
                    ->ForkNonCopyBackWrData("")
                        ->ForkCompData("### H.18.1.1. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData")
                            ->Leaf("#### H.18.1.1.1. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData_I -> I ", Resps::CompData_I      , Xact::CacheStates::I      , true )
                            ->Leaf("#### H.18.1.1.2. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData_UC     ", Resps::CompData_UC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.18.1.1.3. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData_SC     ", Resps::CompData_SC     , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.18.1.1.4. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData_UD_PD  ", Resps::CompData_UD_PD  , Xact::CacheStates::None   , false)
                            ->Leaf("#### H.18.1.1.5. I   -> AtomicCompare -> DBIDResp -> NonCopyBackWrData -> CompData_SD_PD  ", Resps::CompData_SD_PD  , Xact::CacheStates::None   , false)
                        ->Rest()
                    ->Rest()
                ->Rest()
            ->Rest()

        ->End();

    tcptResp->End();
    tcptResp = nullptr;
}

#endif
