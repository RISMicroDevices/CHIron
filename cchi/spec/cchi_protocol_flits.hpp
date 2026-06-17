#pragma once

//#ifndef __CCHI__CCHI_PROTOCOL_FLITS
//#define __CCHI__CCHI_PROTOCOL_FLITS

#include <concepts>                             // IWYU pragma: keep
#include <variant>

#include "../basic/cchi_components.hpp"
#include "../basic/cchi_parameters.hpp"

#include "../../common/nonstdint.hpp"           // IWYU pragma: export


namespace CCHI {

    namespace FlitConfigurationConstraints {

        template<size_t TxnIDWidth>
        concept TxnID               = CCHI::CheckTxnIDWidth(TxnIDWidth);

        template<size_t DBIDWidth>
        concept DBID                = CCHI::CheckDBIDWidth(DBIDWidth);

        template<size_t UpstreamNodeIDWidth>
        concept UpstreamNodeID      = CCHI::CheckUpstreamNodeIDWidth(UpstreamNodeIDWidth);

        template<size_t DownstreamNodeIDWidth>
        concept DownstreamNodeID    = CCHI::CheckDownstreamNodeIDWidth(DownstreamNodeIDWidth);

        template<size_t WayIndexWidth>
        concept WayIndex            = CCHI::CheckWayIndexWidth(WayIndexWidth);
    }


    /*
    General Flit Configuration for all channels
    */
    template<ComponentTypeEnum ComponentType    = ComponentType::TYPE_1,
             size_t TxnIDWidth                  = 7,
             size_t DBIDWidth                   = 7,
             size_t UpstreamNodeIDWidth         = 5,
             size_t DownstreamNodeIDWidth       = 5,
             size_t WayIndexWidth               = 4,
             bool   UWPersistEnable             = true,
             bool   UWPredictEnable             = true>
    requires FlitConfigurationConstraints::TxnID<TxnIDWidth>
          && FlitConfigurationConstraints::DBID<DBIDWidth>
          && FlitConfigurationConstraints::UpstreamNodeID<UpstreamNodeIDWidth>
          && FlitConfigurationConstraints::DownstreamNodeID<DownstreamNodeIDWidth>
          && FlitConfigurationConstraints::WayIndex<WayIndexWidth>
    struct FlitConfiguration {
        static constexpr ComponentTypeEnum componentType        = ComponentType;
        //
        static constexpr size_t     opcodeEVTWidth              = ComponentType->opcodeWidthEVT;
        static constexpr size_t     opcodeREQWidth              = ComponentType->opcodeWidthREQ;
        static constexpr size_t     opcodeSNPWidth              = ComponentType->opcodeWidthSNP;
        static constexpr size_t     opcodeDnRSPWidth            = ComponentType->opcodeWidthDnRSP;
        static constexpr size_t     opcodeUpRSPWidth            = ComponentType->opcodeWidthUpRSP;
        static constexpr size_t     opcodeDnDATWidth            = ComponentType->opcodeWidthDnDAT;
        static constexpr size_t     opcodeUpDATWidth            = ComponentType->opcodeWidthUpDAT;
        static constexpr bool       hasEVT                      = ComponentType->hasEVT;
        static constexpr bool       hasREQ                      = ComponentType->hasREQ;
        static constexpr bool       hasSNP                      = ComponentType->hasSNP;
        static constexpr bool       hasDnRSP                    = ComponentType->hasDnRSP;
        static constexpr bool       hasUpRSP                    = ComponentType->hasUpRSP;
        static constexpr bool       hasDnDAT                    = ComponentType->hasDnDAT;
        static constexpr bool       hasUpDAT                    = ComponentType->hasUpDAT;
        //
        static constexpr size_t     txnIdWidth                  = TxnIDWidth;
        static constexpr size_t     dbIdWidth                   = DBIDWidth;
        static constexpr size_t     upstreamNodeIdWidth         = UpstreamNodeIDWidth;
        static constexpr size_t     downstreamNodeIdWidth       = DownstreamNodeIDWidth;
        static constexpr size_t     wayIndexWidth               = WayIndexWidth;
        static constexpr bool       upstreamWayPersistEnable    = UWPersistEnable;
        static constexpr bool       upstreamWayPredictEnable    = UWPredictEnable;
    };


    /*
    General Flit Configuration concept for all channels
    */
    template<class T>
    concept FlitConfigurationConcept = requires {
        { T::opcodeEVTWidth              }   -> std::convertible_to<size_t>;
        { T::opcodeREQWidth              }   -> std::convertible_to<size_t>;
        { T::opcodeSNPWidth              }   -> std::convertible_to<size_t>;
        { T::opcodeDnRSPWidth            }   -> std::convertible_to<size_t>;
        { T::opcodeUpRSPWidth            }   -> std::convertible_to<size_t>;
        { T::opcodeDnDATWidth            }   -> std::convertible_to<size_t>;
        { T::opcodeUpDATWidth            }   -> std::convertible_to<size_t>;
        { T::hasEVT                      }   -> std::convertible_to<bool>;
        { T::hasREQ                      }   -> std::convertible_to<bool>;
        { T::hasSNP                      }   -> std::convertible_to<bool>;
        { T::hasDnRSP                    }   -> std::convertible_to<bool>;
        { T::hasUpRSP                    }   -> std::convertible_to<bool>;
        { T::hasDnDAT                    }   -> std::convertible_to<bool>;
        { T::hasUpDAT                    }   -> std::convertible_to<bool>;
        //
        { T::txnIdWidth                 }   -> std::convertible_to<size_t>;
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
        { T::wayIndexWidth              }   -> std::convertible_to<size_t>;
        { T::upstreamWayPersistEnable   }   -> std::convertible_to<bool>;
        { T::upstreamWayPredictEnable   }   -> std::convertible_to<bool>;
    };

    /*
    EVT Flit Configuration concept
    */
    template<class T>
    concept EVTFlitConfigurationConcept = requires {
        requires HasEVT<T::componentType>;
        { T::opcodeEVTWidth             }   -> std::convertible_to<size_t>;
        //
        { T::txnIdWidth                 }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
        { T::wayIndexWidth              }   -> std::convertible_to<size_t>;
        { T::upstreamWayPersistEnable   }   -> std::convertible_to<bool>;
    };

    /*
    REQ Flit Configuration concept
    */
    template<class T>
    concept REQFlitConfigurationConcept = requires {
        requires HasREQ<T::componentType>;
        { T::opcodeREQWidth             }   -> std::convertible_to<size_t>;
        //
        { T::txnIdWidth                 }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
        { T::wayIndexWidth              }   -> std::convertible_to<size_t>;
        { T::upstreamWayPersistEnable   }   -> std::convertible_to<bool>;
        { T::upstreamWayPredictEnable   }   -> std::convertible_to<bool>;
    };

    /*
    SNP Flit Configuration concept
    */
    template<class T>
    concept SNPFlitConfigurationConcept = requires {
        requires HasSNP<T::componentType>;
        { T::opcodeSNPWidth             }   -> std::convertible_to<size_t>;
        //
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
    };

    /*
    DnRSP Flit Configuration concept
    */
    template<class T>
    concept DnRSPFlitConfigurationConcept = requires {
        requires HasDnRSP<T::componentType>;
        { T::opcodeDnRSPWidth           }   -> std::convertible_to<size_t>;
        //
        { T::txnIdWidth                 }   -> std::convertible_to<size_t>;
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
        { T::wayIndexWidth              }   -> std::convertible_to<size_t>;
        { T::upstreamWayPersistEnable   }   -> std::convertible_to<bool>;
    };

    /*
    UpRSP Flit Configuration concept
    */
    template<class T>
    concept UpRSPFlitConfigurationConcept = requires {
        requires HasUpRSP<T::componentType>;
        { T::opcodeUpRSPWidth           }   -> std::convertible_to<size_t>;
        //
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
    };

    /*
    DnDAT Flit Configuration concept
    */
    template<class T>
    concept DnDATFlitConfigurationConcept = requires {
        requires HasDnDAT<T::componentType>;
        { T::opcodeDnDATWidth           }   -> std::convertible_to<size_t>;
        //
        { T::txnIdWidth                 }   -> std::convertible_to<size_t>;
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
        { T::wayIndexWidth              }   -> std::convertible_to<size_t>;
        { T::upstreamWayPersistEnable   }   -> std::convertible_to<bool>;
    };

    /*
    UpDAT Flit Configuration concept
    */
    template<class T>
    concept UpDATFlitConfigurationConcept = requires {
        requires HasUpDAT<T::componentType>;
        { T::opcodeUpDATWidth           }   -> std::convertible_to<size_t>;
        //
        { T::dbIdWidth                  }   -> std::convertible_to<size_t>;
        { T::upstreamNodeIdWidth        }   -> std::convertible_to<size_t>;
        { T::downstreamNodeIdWidth      }   -> std::convertible_to<size_t>;
    };


    namespace Flits {
        
        //
        template<EVTFlitConfigurationConcept config = FlitConfiguration<>>
        class EVT {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::txnIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            Opcode: "opcodeEVTWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeEVTWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            Addr: 48 bits
            Address. The memory address associated with the transaction.
            */
            static constexpr size_t ADDR_WIDTH = 48;
            using addr_t = uint_fit_t<ADDR_WIDTH>;

            /*
            NS: 1 bit
            Non-secure. Indicates whether the transaction is Non-secure or Secure.
            */
            static constexpr size_t NS_WIDTH = 1;
            using ns_t = uint_fit_t<NS_WIDTH>;

            /*
            MemAttr: 1 bit
            Memory Attributes. Indicating allocate attribute for the transaction.
            */
            static constexpr size_t MEMATTR_WIDTH = 1;
            using memattr_t = uint_fit_t<MEMATTR_WIDTH>;

            /*
            WayValid: N/A or 1 bit, depending on <UWPersist_Enable>
            Upstream Way Valid. Indicates whether the way index of the address was persisted by upstream. 
                                See <UWPersist_Enable>.
            */
            static constexpr size_t WAYVALID_WIDTH = config::upstreamWayPersistEnable ? 1 : 0;

            static constexpr bool hasWayValid = WAYVALID_WIDTH > 0;
            using wayvalid_t = uint_fit_t<WAYVALID_WIDTH, std::monostate>;

            /*
            Way: N/A or <WayIndex_Width> bits, depending on <UWPredict_Enable>
            Upstream Way Index. The way index of the address provided by upstream. 
                                See <UWPredict_Enable>.
            */
            static constexpr size_t WAY_WIDTH = config::upstreamWayPredictEnable ? config::wayIndexWidth : 0;

            static constexpr bool hasWay = WAY_WIDTH > 0;
            using way_t = uint_fit_t<WAY_WIDTH, std::monostate>;

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            static constexpr size_t WIDTH = TXNID_WIDTH     + SRCID_WIDTH       + TGTID_WIDTH       + OPCODE_WIDTH  
                                          + ADDR_WIDTH      + NS_WIDTH          + MEMATTR_WIDTH     + WAYVALID_WIDTH
                                          + WAY_WIDTH       + TRACETAG_WIDTH;

        // Flit fields
        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            opcode_t                Opcode;
            addr_t                  Addr;
            ns_t                    NS;
            memattr_t               MemAttr;
            wayvalid_t              WayValid;
            way_t                   Way;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept EVTFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // Addr
            typename T::addr_t;
            { T::ADDR_WIDTH                     } -> std::convertible_to<size_t>;

            // NS
            typename T::ns_t;
            { T::NS_WIDTH                       } -> std::convertible_to<size_t>;

            // MemAttr
            typename T::memattr_t;
            { T::MEMATTR_WIDTH                  } -> std::convertible_to<size_t>;

            // WayValid
            typename T::wayvalid_t;
            { T::WAYVALID_WIDTH                 } -> std::convertible_to<size_t>;

            // Way
            typename T::way_t;
            { T::WAY_WIDTH                      } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<REQFlitConfigurationConcept config = FlitConfiguration<>>
        class REQ {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::txnIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            Opcode: "opcodeREQWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeREQWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            Size: 3 bits
            Size. The size of the transaction in terms of number of bytes.
            */
            static constexpr ssize_t SIZE_WIDTH = 3;
            using ssize_t = uint_fit_t<SIZE_WIDTH>;

            /*
            Addr: 48 bits
            Address. The memory address associated with the transaction.
            */
            static constexpr size_t ADDR_WIDTH = 48;
            using addr_t = uint_fit_t<ADDR_WIDTH>;

            /*
            NS: 1 bit
            Non-secure. Indicates whether the transaction is Non-secure or Secure.
            */
            static constexpr size_t NS_WIDTH = 1;
            using ns_t = uint_fit_t<NS_WIDTH>;

            /*
            Order: 2 bits
            Order. Indicates the ordering requirement of the transaction.
            */
            static constexpr size_t ORDER_WIDTH = 2;
            using order_t = uint_fit_t<ORDER_WIDTH>;

            /*
            MemAttr: 4 bits
            Memory Attributes. Indicating allocate and cache attributes for the transaction.
            */
            static constexpr size_t MEMATTR_WIDTH = 4;
            using memattr_t = uint_fit_t<MEMATTR_WIDTH>;

            /*
            Excl: 1 bit
            Exclusive. Indicates whether the transaction is an exclusive access.
            */
            static constexpr size_t EXCL_WIDTH = 1;
            using excl_t = uint_fit_t<EXCL_WIDTH>;

            /*
            ExpCompStash: 1 bit
            Expected Completion Stash. Indicates whether the source node expects CompStash on stash completion.
            */
            static constexpr size_t EXPCOMPSTASH_WIDTH = 1;
            using expcompstash_t = uint_fit_t<EXPCOMPSTASH_WIDTH>;

            /*
            WayValid: N/A or 1 bit, depending on <UWPersist_Enable> and <UWPredict_Enable>
            Upstream Way Valid. Indicates whether the way index of the address was persisted or predicted by upstream. 
                                See <UWPersist_Enable> and <UWPredict_Enable>.
            */
            static constexpr size_t WAYVALID_WIDTH = (config::upstreamWayPersistEnable || config::upstreamWayPredictEnable) ? 1 : 0;
            using wayvalid_t = uint_fit_t<WAYVALID_WIDTH, std::monostate>;

            /*
            Way: N/A or <WayIndex_Width> bits, depending on <UWPersist_Enable> and <UWPredict_Enable>
            Upstream Way Index. The way index of the address provided by upstream. 
                                See <UWPersist_Enable> and <UWPredict_Enable>.
            */
            static constexpr size_t WAY_WIDTH = (config::upstreamWayPersistEnable || config::upstreamWayPredictEnable) ? config::wayIndexWidth : 0;
            using way_t = uint_fit_t<WAY_WIDTH, std::monostate>;

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            static constexpr size_t WIDTH = TXNID_WIDTH         + SRCID_WIDTH       + TGTID_WIDTH           + OPCODE_WIDTH  
                                          + SIZE_WIDTH          + ADDR_WIDTH        + NS_WIDTH              + ORDER_WIDTH
                                          + MEMATTR_WIDTH       + EXCL_WIDTH      /*+ EXPCOMPSTASH_WIDTH*/  + WAYVALID_WIDTH
                                          + WAY_WIDTH           + TRACETAG_WIDTH;

        // Flit fields
        // *NOTICE: Some fields are overlapped.
        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            opcode_t                Opcode;
            ssize_t                 Size;
            addr_t                  Addr;
            ns_t                    NS;
            order_t                 Order;
            memattr_t               MemAttr;
            union {
            excl_t                  Excl;
            expcompstash_t          ExpCompStash;
            };
            wayvalid_t              WayValid;
            way_t                   Way;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept REQFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // Size
            typename T::ssize_t;
            { T::SIZE_WIDTH                     } -> std::convertible_to<size_t>;

            // Addr
            typename T::addr_t;
            { T::ADDR_WIDTH                     } -> std::convertible_to<size_t>;

            // NS
            typename T::ns_t;
            { T::NS_WIDTH                       } -> std::convertible_to<size_t>;

            // Order
            typename T::order_t;
            { T::ORDER_WIDTH                    } -> std::convertible_to<size_t>;

            // MemAttr
            typename T::memattr_t;
            { T::MEMATTR_WIDTH                  } -> std::convertible_to<size_t>;

            // Excl
            typename T::excl_t;
            { T::EXCL_WIDTH                     } -> std::convertible_to<size_t>;

            // ExpCompStash
            typename T::expcompstash_t;
            { T::EXPCOMPSTASH_WIDTH             } -> std::convertible_to<size_t>;

            // WayValid
            typename T::wayvalid_t;
            { T::WAYVALID_WIDTH                 } -> std::convertible_to<size_t>;

            // Way
            typename T::way_t;
            { T::WAY_WIDTH                      } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<SNPFlitConfigurationConcept config = FlitConfiguration<>>
        class SNP {
        public:
            /*
            TxnID: <DBID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::dbIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            Opcode: "opcodeSNPWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeSNPWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            Addr: 45 bits
            Address. The memory address associated with the transaction. 
                     Note that bit [2:0] are not included as they are always 0 in cacheline granularity.
            */
            static constexpr size_t ADDR_WIDTH = 45;
            using addr_t = uint_fit_t<ADDR_WIDTH>;

            /*
            NS: 1 bit
            Non-secure. Indicates whether the transaction is Non-secure or Secure.
            */
            static constexpr size_t NS_WIDTH = 1;
            using ns_t = uint_fit_t<NS_WIDTH>;

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            static constexpr size_t WIDTH = TXNID_WIDTH     + SRCID_WIDTH       + TGTID_WIDTH       + OPCODE_WIDTH  
                                          + ADDR_WIDTH      + NS_WIDTH          + TRACETAG_WIDTH;

        // Flit fields
        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            opcode_t                Opcode;
            addr_t                  Addr;
            ns_t                    NS;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept SNPFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // Addr
            typename T::addr_t;
            { T::ADDR_WIDTH                     } -> std::convertible_to<size_t>;

            // NS
            typename T::ns_t;
            { T::NS_WIDTH                       } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<DnRSPFlitConfigurationConcept config = FlitConfiguration<>>
        class DnRSP {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::txnIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            DBID: <DBID_Width> bits
            DBID. A transaction has a unique DBID per target node.
            */
            static constexpr size_t DBID_WIDTH = config::dbIdWidth;
            using dbid_t = uint_fit_t<DBID_WIDTH>;

            /*
            Opcode: "opcodeDnRSPWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeDnRSPWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            RespErr: 2 bits
            Response Error. Indicates the error status of the transaction.
            */
            static constexpr size_t RESPERR_WIDTH = 2;
            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response. Indicates the response state of the transaction.
            */
            static constexpr size_t RESP_WIDTH = 3;
            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            CBusy: 3 bits
            Cache Busy. Indicates the busy status of the target node.
            */
            static constexpr size_t CBUSY_WIDTH = 3;
            using cbusy_t = uint_fit_t<CBUSY_WIDTH>;

            /*
            WayValid: N/A or 1 bit, depending on <UWPersist_Enable>
            Upstream Way Valid. Indicates whether the way index of the address was persisted by upstream. 
                                See <UWPersist_Enable>.
            */
            static constexpr size_t WAYVALID_WIDTH = config::upstreamWayPersistEnable ? 1 : 0;

            static constexpr bool hasWayValid = WAYVALID_WIDTH > 0;
            using wayvalid_t = uint_fit_t<WAYVALID_WIDTH, std::monostate>;

            /*
            Way: N/A or <WayIndex_Width> bits, depending on <UWPredict_Enable>
            Upstream Way Index. The way index of the address provided by upstream. 
                                See <UWPredict_Enable>.
            */
            static constexpr size_t WAY_WIDTH = config::upstreamWayPredictEnable ? config::wayIndexWidth : 0;

            static constexpr bool hasWay = WAY_WIDTH > 0;
            using way_t = uint_fit_t<WAY_WIDTH, std::monostate>;
            
            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            static constexpr size_t WIDTH = TXNID_WIDTH         + SRCID_WIDTH       + TGTID_WIDTH       + DBID_WIDTH
                                          + OPCODE_WIDTH        + RESPERR_WIDTH     + RESP_WIDTH        + CBUSY_WIDTH
                                          + WAYVALID_WIDTH      + WAY_WIDTH         + TRACETAG_WIDTH;

        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            dbid_t                  DBID;
            opcode_t                Opcode;
            resperr_t               RespErr;
            resp_t                  Resp;
            cbusy_t                 CBusy;
            wayvalid_t              WayValid;
            way_t                   Way;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept DnRSPFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // DBID
            typename T::dbid_t;
            { T::DBID_WIDTH                     } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH                  } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH                     } -> std::convertible_to<size_t>;

            // CBusy
            typename T::cbusy_t;
            { T::CBUSY_WIDTH                    } -> std::convertible_to<size_t>;

            // WayValid
            typename T::wayvalid_t;
            { T::WAYVALID_WIDTH                 } -> std::convertible_to<size_t>;

            // Way
            typename T::way_t;
            { T::WAY_WIDTH                      } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<UpRSPFlitConfigurationConcept config = FlitConfiguration<>>
        class UpRSP {
        public:
            /*
            TxnID: <DBID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per target node.
            */
            static constexpr size_t TXNID_WIDTH = config::dbIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            Opcode: "opcodeUpRSPWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeUpRSPWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            RespErr: 2 bits
            Response Error. Indicates the error status of the transaction.
            */
            static constexpr size_t RESPERR_WIDTH = 2;
            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response. Indicates the response state of the transaction.
            */
            static constexpr size_t RESP_WIDTH = 3;
            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        // Flit fields
        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            opcode_t                Opcode;
            resperr_t               RespErr;
            resp_t                  Resp;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept UpRSPFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH                  } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH                     } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<DnDATFlitConfigurationConcept config = FlitConfiguration<>>
        class DnDAT {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::txnIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            DBID: <DBID_Width> bits
            DBID. A transaction has a unique DBID per target node.
            */
            static constexpr size_t DBID_WIDTH = config::dbIdWidth;
            using dbid_t = uint_fit_t<DBID_WIDTH>;

            /*
            Opcode: "opcodeDnDATWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeDnDATWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            RespErr: 2 bits
            Response Error. Indicates the error status of the transaction.
            */
            static constexpr size_t RESPERR_WIDTH = 2;
            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response. Indicates the response state of the transaction.
            */
            static constexpr size_t RESP_WIDTH = 3;
            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            DataSource: 5 bits
            Data Source. Indicates the source of the data in the transaction in SoC.
            */
            static constexpr size_t DATASOURCE_WIDTH = 5;
            using datasource_t = uint_fit_t<DATASOURCE_WIDTH>;

            /*
            CBusy: 3 bits
            Cache Busy. Indicates the busy status of the target node.
            */
            static constexpr size_t CBUSY_WIDTH = 3;
            using cbusy_t = uint_fit_t<CBUSY_WIDTH>;

            /*
            WayValid: N/A or 1 bit, depending on <UWPersist_Enable>
            Upstream Way Valid. Indicates whether the way index of the address was persisted by upstream. 
                                See <UWPersist_Enable>.
            */
            static constexpr size_t WAYVALID_WIDTH = config::upstreamWayPersistEnable ? 1 : 0;

            static constexpr bool hasWayValid = WAYVALID_WIDTH > 0;
            using wayvalid_t = uint_fit_t<WAYVALID_WIDTH, std::monostate>;

            /*
            Way: N/A or <WayIndex_Width> bits, depending on <UWPredict_Enable>
            Upstream Way Index. The way index of the address provided by upstream. 
                                See <UWPredict_Enable>.
            */
            static constexpr size_t WAY_WIDTH = config::upstreamWayPredictEnable ? config::wayIndexWidth : 0;

            static constexpr bool hasWay = WAY_WIDTH > 0;
            using way_t = uint_fit_t<WAY_WIDTH, std::monostate>;

            /*
            Data: 256 bits
            Data. The data payload of the transaction.
            */
            static constexpr size_t DATA_WIDTH = 256;
            using data_t = uint64_t[DATA_WIDTH / 64];

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            static constexpr size_t WIDTH = TXNID_WIDTH         + SRCID_WIDTH       + TGTID_WIDTH       + DBID_WIDTH
                                          + OPCODE_WIDTH        + RESPERR_WIDTH     + RESP_WIDTH        + DATASOURCE_WIDTH
                                          + CBUSY_WIDTH         + WAYVALID_WIDTH    + WAY_WIDTH         + DATA_WIDTH
                                          + TRACETAG_WIDTH;

        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            dbid_t                  DBID;
            opcode_t                Opcode;
            resperr_t               RespErr;
            resp_t                  Resp;
            datasource_t            DataSource;
            cbusy_t                 CBusy;
            wayvalid_t              WayValid;
            way_t                   Way;
            data_t                  Data;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept DnDATFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // DBID
            typename T::dbid_t;
            { T::DBID_WIDTH                     } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH                  } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH                     } -> std::convertible_to<size_t>;

            // DataSource
            typename T::datasource_t;
            { T::DATASOURCE_WIDTH               } -> std::convertible_to<size_t>;

            // CBusy
            typename T::cbusy_t;
            { T::CBUSY_WIDTH                    } -> std::convertible_to<size_t>;

            // WayValid
            typename T::wayvalid_t;
            { T::WAYVALID_WIDTH                 } -> std::convertible_to<size_t>;

            // Way
            typename T::way_t;
            { T::WAY_WIDTH                      } -> std::convertible_to<size_t>;

            // Data
            typename T::data_t;
            { sizeof(typename T::data_t) * 8    } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<UpDATFlitConfigurationConcept config = FlitConfiguration<>>
        class UpDAT {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per target node.
            */
            static constexpr size_t TXNID_WIDTH = config::dbIdWidth;
            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            Opcode: "opcodeUpDATWidth" bits
            Opcode. The operation code that specifies the type of transaction.
            */
            static constexpr size_t OPCODE_WIDTH = config::opcodeUpDATWidth;

            static constexpr bool hasOpcode = OPCODE_WIDTH > 0;
            using opcode_t = uint_fit_t<OPCODE_WIDTH, std::monostate>;

            /*
            RespErr: 2 bits
            Response Error. Indicates the error status of the transaction.
            */
            static constexpr size_t RESPERR_WIDTH = 2;
            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response. Indicates the response state of the transaction.
            */
            static constexpr size_t RESP_WIDTH = 3;
            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            Data: 256 bits
            Data. The data payload of the transaction.
            */
            static constexpr size_t DATA_WIDTH = 256;
            using data_t = uint64_t[DATA_WIDTH / 64];

            /*
            BE: 32 bits
            Byte Enable. Indicates the valid bytes in the data payload.
            */
            static constexpr size_t BE_WIDTH = 32;
            using be_t = uint_fit_t<BE_WIDTH>;

            /*
            TraceTag: 1 bit
            Trace Tag. Indicates whether the transaction is tagged for tracing.
            */
            static constexpr size_t TRACETAG_WIDTH = 1;
            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
            txnid_t                 TxnID;
            srcid_t                 SrcID;
            tgtid_t                 TgtID;
            opcode_t                Opcode;
            resperr_t               RespErr;
            resp_t                  Resp;
            data_t                  Data;
            be_t                    BE;
            tracetag_t              TraceTag;
        };

        //
        template<class T>
        concept UpDATFlitFormatConcept = requires {

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH                    } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH                    } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH                    } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH                   } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH                  } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH                     } -> std::convertible_to<size_t>;

            // Data
            typename T::data_t;
            { sizeof(typename T::data_t) * 8    } -> std::convertible_to<size_t>;

            // BE
            typename T::be_t;
            { T::BE_WIDTH                       } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH                 } -> std::convertible_to<size_t>;
        };


        //
        template<class T>
        concept FlitOpcodeFormatConcept = requires {

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
        };
    }
}
