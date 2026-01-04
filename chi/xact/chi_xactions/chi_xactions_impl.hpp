//#pragma once

#include "chi_xactions_impl_AllocatingRead.hpp"
#include "chi_xactions_impl_NonAllocatingRead.hpp"
#include "chi_xactions_impl_ImmediateWrite.hpp"
#include "chi_xactions_impl_WriteZero.hpp"
#include "chi_xactions_impl_CopyBackWrite.hpp"
#include "chi_xactions_impl_Atomic.hpp"
#include "chi_xactions_impl_IndependentStash.hpp"
#include "chi_xactions_impl_Dataless.hpp"
#include "chi_xactions_impl_HomeRead.hpp"
#include "chi_xactions_impl_HomeWrite.hpp"
#include "chi_xactions_impl_HomeWriteZero.hpp"
#include "chi_xactions_impl_HomeDataless.hpp"
#include "chi_xactions_impl_HomeAtomic.hpp"
#include "chi_xactions_impl_HomeSnoop.hpp"
#include "chi_xactions_impl_ForwardSnoop.hpp"
#include "chi_xactions_impl_DVMSnoop.hpp"

// TODO: XactionCombinedImmediateWriteAndCMO
// TODO: XactionCombinedImmediateWriteAndPersistCMO
// TODO: XactionCombinedCopyBackWriteAndCMO
// TODO: XactionPrefetch
// TODO: XactionDVMOp        
// TODO: XactionHomeCombinedWriteAndCMO
