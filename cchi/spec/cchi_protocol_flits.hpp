#pragma once

#ifndef __CCHI__CCHI_PROTOCOL_FLITS
#define __CCHI__CCHI_PROTOCOL_FLITS

#include <bit>
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

        template<size_t DataWidth>
        concept Data                = CCHI::CheckDataWidth(DataWidth);
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
             size_t DataWidth                   = 256,
             bool   UWPersistEnable             = true,
             bool   UWPredictEnable             = true>
    requires FlitConfigurationConstraints::TxnID<TxnIDWidth>
          && FlitConfigurationConstraints::DBID<DBIDWidth>
          && FlitConfigurationConstraints::UpstreamNodeID<UpstreamNodeIDWidth>
          && FlitConfigurationConstraints::DownstreamNodeID<DownstreamNodeIDWidth>
          && FlitConfigurationConstraints::WayIndex<WayIndexWidth>
          && FlitConfigurationConstraints::Data<DataWidth>
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
        static constexpr size_t     dataIdWidth                 = std::bit_width(512 / DataWidth - 1);
        static constexpr size_t     dataWidth                   = DataWidth;
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
        { T::dataWidth                  }   -> std::convertible_to<size_t>;
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
        { T::dataIdWidth                }   -> std::convertible_to<size_t>;
        { T::dataWidth                  }   -> std::convertible_to<size_t>;
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
        { T::dataIdWidth                }   -> std::convertible_to<size_t>;
        { T::dataWidth                  }   -> std::convertible_to<size_t>;
    };


    namespace Flits {

        //
        template<FlitConfigurationConcept config = FlitConfiguration<>>
        using txnid_t = uint_fit_t<config::txnIdWidth>;

        template<FlitConfigurationConcept config = FlitConfiguration<>>
        using dbid_t = uint_fit_t<config::dbIdWidth>;

        template<FlitConfigurationConcept config = FlitConfiguration<>>
        using up_nodeid_t = uint_fit_t<config::upstreamNodeIdWidth>;

        template<FlitConfigurationConcept config = FlitConfiguration<>>
        using dn_nodeid_t = uint_fit_t<config::downstreamNodeIdWidth>;
        
        //
        template<EVTFlitConfigurationConcept config = FlitConfiguration<>>
        class EVT {
        public:
            /*
            TxnID: <TxnID_Width> bits
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
            static constexpr size_t TXNID_WIDTH = config::txnIdWidth;
            using txnid_t = CCHI::Flits::txnid_t<config>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = up_nodeid_t<config>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = dn_nodeid_t<config>;

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

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline EVT<configDst> ConvertEVT(const EVT<configSrc>& src) noexcept
        {
            EVT<configDst> dst;

            dst.TxnID      = static_cast<typename EVT<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename EVT<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename EVT<configDst>::tgtid_t>(src.TgtID);
            dst.Opcode     = static_cast<typename EVT<configDst>::opcode_t>(src.Opcode);
            dst.Addr       = static_cast<typename EVT<configDst>::addr_t>(src.Addr);
            dst.NS         = static_cast<typename EVT<configDst>::ns_t>(src.NS);
            dst.MemAttr    = static_cast<typename EVT<configDst>::memattr_t>(src.MemAttr);
            dst.WayValid   = static_cast<typename EVT<configDst>::wayvalid_t>(src.WayValid);
            dst.Way        = static_cast<typename EVT<configDst>::way_t>(src.Way);
            dst.TraceTag   = static_cast<typename EVT<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertEVT(EVT<configDst>& dst, const EVT<configSrc>& src) noexcept
        {
            dst = ConvertEVT<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsEVTConversionSafe()
        {
            return (EVT<configDst>::TXNID_WIDTH      >= EVT<configSrc>::TXNID_WIDTH)
                && (EVT<configDst>::SRCID_WIDTH      >= EVT<configSrc>::SRCID_WIDTH)
                && (EVT<configDst>::TGTID_WIDTH      >= EVT<configSrc>::TGTID_WIDTH)
                && (EVT<configDst>::OPCODE_WIDTH     >= EVT<configSrc>::OPCODE_WIDTH)
                && (EVT<configDst>::ADDR_WIDTH       >= EVT<configSrc>::ADDR_WIDTH)
                && (EVT<configDst>::WAY_WIDTH        >= EVT<configSrc>::WAY_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsEVTConversionSafe([[maybe_unused]] const EVT<configSrc>& src)
        {
            return IsEVTConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsEVTConversionSafe([[maybe_unused]] const EVT<configSrc>& src, [[maybe_unused]] const EVT<configDst>& dst)
        {
            return IsEVTConversionSafe<configDst, configSrc>();
        }

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
            using txnid_t = CCHI::Flits::txnid_t<config>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = up_nodeid_t<config>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = dn_nodeid_t<config>;

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
            static constexpr size_t SSIZE_WIDTH = 3;
            using ssize_t = uint_fit_t<SSIZE_WIDTH>;

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
            ExpCompData: 1 bit
            Expecting CompData. Indicates whether the source node expects response with data.
            */
            static constexpr size_t EXPCOMPDATA_WIDTH = 1;
            using expcompdata_t = uint_fit_t<EXPCOMPDATA_WIDTH>;

            /*
            ExpCompStash: 1 bit
            Expecting CompStash. Indicates whether the source node expects CompStash on stash completion.
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
                                          + SSIZE_WIDTH         + ADDR_WIDTH        + NS_WIDTH              + ORDER_WIDTH
                                          + MEMATTR_WIDTH       + EXCL_WIDTH        + EXPCOMPDATA_WIDTH   /*+ EXPCOMPSTASH_WIDTH*/
                                          + WAYVALID_WIDTH      + WAY_WIDTH         + TRACETAG_WIDTH;

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
            excl_t                  Excl;
            union {
            expcompdata_t           ExpCompData;
            expcompstash_t          ExpCompStash;
            };
            wayvalid_t              WayValid;
            way_t                   Way;
            tracetag_t              TraceTag;
        };

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline REQ<configDst> ConvertREQ(const REQ<configSrc>& srcFlit) noexcept
        {
            REQ<configDst> dstFlit;

            dstFlit.TxnID = static_cast<typename REQ<configDst>::txnid_t>(srcFlit.TxnID);
            dstFlit.SrcID = static_cast<typename REQ<configDst>::srcid_t>(srcFlit.SrcID);
            dstFlit.TgtID = static_cast<typename REQ<configDst>::tgtid_t>(srcFlit.TgtID);
            dstFlit.Opcode = static_cast<typename REQ<configDst>::opcode_t>(srcFlit.Opcode);
            dstFlit.Size = static_cast<typename REQ<configDst>::ssize_t>(srcFlit.Size);
            dstFlit.Addr = static_cast<typename REQ<configDst>::addr_t>(srcFlit.Addr);
            dstFlit.NS = static_cast<typename REQ<configDst>::ns_t>(srcFlit.NS);
            dstFlit.Order = static_cast<typename REQ<configDst>::order_t>(srcFlit.Order);
            dstFlit.MemAttr = static_cast<typename REQ<configDst>::memattr_t>(srcFlit.MemAttr);
            dstFlit.Excl = static_cast<typename REQ<configDst>::excl_t>(srcFlit.Excl);
            dstFlit.ExpCompData = static_cast<typename REQ<configDst>::expcompdata_t>(srcFlit.ExpCompData);
            dstFlit.WayValid = static_cast<typename REQ<configDst>::wayvalid_t>(srcFlit.WayValid);
            dstFlit.Way = static_cast<typename REQ<configDst>::way_t>(srcFlit.Way);
            dstFlit.TraceTag = static_cast<typename REQ<configDst>::tracetag_t>(srcFlit.TraceTag);

            return dstFlit;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertREQ(REQ<configDst>& dstFlit, const REQ<configSrc>& srcFlit) noexcept
        {
            dstFlit = ConvertREQ<configDst, configSrc>(srcFlit);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsREQConversionSafe()
        {
            return (REQ<configDst>::TXNID_WIDTH      >= REQ<configSrc>::TXNID_WIDTH)
                && (REQ<configDst>::SRCID_WIDTH      >= REQ<configSrc>::SRCID_WIDTH)
                && (REQ<configDst>::TGTID_WIDTH      >= REQ<configSrc>::TGTID_WIDTH)
                && (REQ<configDst>::OPCODE_WIDTH     >= REQ<configSrc>::OPCODE_WIDTH)
                && (REQ<configDst>::ADDR_WIDTH       >= REQ<configSrc>::ADDR_WIDTH)
                && (REQ<configDst>::WAY_WIDTH        >= REQ<configSrc>::WAY_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsREQConversionSafe([[maybe_unused]] const REQ<configSrc>& srcFlit)
        {
            return IsREQConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsxREQConversionSafe([[maybe_unused]] const REQ<configSrc>& srcFlit, [[maybe_unused]] const REQ<configDst>& dstFlit)
        {
            return IsREQConversionSafe<configDst, configSrc>();
        }

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
            { T::SSIZE_WIDTH                    } -> std::convertible_to<size_t>;

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

            // ExpCompData
            typename T::expcompdata_t;
            { T::EXPCOMPDATA_WIDTH              } -> std::convertible_to<size_t>;

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
            using txnid_t = dbid_t<config>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = dn_nodeid_t<config>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = up_nodeid_t<config>;

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

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline SNP<configDst> ConvertSNP(const SNP<configSrc>& src) noexcept
        {
            SNP<configDst> dst;

            dst.TxnID      = static_cast<typename SNP<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename SNP<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename SNP<configDst>::tgtid_t>(src.TgtID);
            dst.Opcode     = static_cast<typename SNP<configDst>::opcode_t>(src.Opcode);
            dst.Addr       = static_cast<typename SNP<configDst>::addr_t>(src.Addr);
            dst.NS         = static_cast<typename SNP<configDst>::ns_t>(src.NS);
            dst.TraceTag   = static_cast<typename SNP<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertSNP(SNP<configDst>& dst, const SNP<configSrc>& src) noexcept
        {
            dst = ConvertSNP<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsSNPConversionSafe()
        {
            return (SNP<configDst>::TXNID_WIDTH      >= SNP<configSrc>::TXNID_WIDTH)
                && (SNP<configDst>::SRCID_WIDTH      >= SNP<configSrc>::SRCID_WIDTH)
                && (SNP<configDst>::TGTID_WIDTH      >= SNP<configSrc>::TGTID_WIDTH)
                && (SNP<configDst>::OPCODE_WIDTH     >= SNP<configSrc>::OPCODE_WIDTH)
                && (SNP<configDst>::ADDR_WIDTH       >= SNP<configSrc>::ADDR_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsSNPConversionSafe([[maybe_unused]] const SNP<configSrc>& src)
        {
            return IsSNPConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsSNPConversionSafe([[maybe_unused]] const SNP<configSrc>& src, [[maybe_unused]] const SNP<configDst>& dst)
        {
            return IsSNPConversionSafe<configDst, configSrc>();
        }

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
            using txnid_t = CCHI::Flits::txnid_t<config>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = dn_nodeid_t<config>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = up_nodeid_t<config>;

            /*
            DBID: <DBID_Width> bits
            DBID. A transaction has a unique DBID per target node.
            */
            static constexpr size_t DBID_WIDTH = config::dbIdWidth;
            using dbid_t = CCHI::Flits::dbid_t<config>;

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

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline DnRSP<configDst> ConvertDnRSP(const DnRSP<configSrc>& src) noexcept
        {
            DnRSP<configDst> dst;

            dst.TxnID      = static_cast<typename DnRSP<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename DnRSP<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename DnRSP<configDst>::tgtid_t>(src.TgtID);
            dst.DBID       = static_cast<typename DnRSP<configDst>::dbid_t>(src.DBID);
            dst.Opcode     = static_cast<typename DnRSP<configDst>::opcode_t>(src.Opcode);
            dst.RespErr    = static_cast<typename DnRSP<configDst>::resperr_t>(src.RespErr);
            dst.Resp       = static_cast<typename DnRSP<configDst>::resp_t>(src.Resp);
            dst.CBusy      = static_cast<typename DnRSP<configDst>::cbusy_t>(src.CBusy);
            dst.WayValid   = static_cast<typename DnRSP<configDst>::wayvalid_t>(src.WayValid);
            dst.Way        = static_cast<typename DnRSP<configDst>::way_t>(src.Way);
            dst.TraceTag   = static_cast<typename DnRSP<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertDnRSP(DnRSP<configDst>& dst, const DnRSP<configSrc>& src) noexcept
        {
            dst = ConvertDnRSP<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnRSPConversionSafe()
        {
            return (DnRSP<configDst>::TXNID_WIDTH      >= DnRSP<configSrc>::TXNID_WIDTH)
                && (DnRSP<configDst>::SRCID_WIDTH      >= DnRSP<configSrc>::SRCID_WIDTH)
                && (DnRSP<configDst>::TGTID_WIDTH      >= DnRSP<configSrc>::TGTID_WIDTH)
                && (DnRSP<configDst>::DBID_WIDTH       >= DnRSP<configSrc>::DBID_WIDTH)
                && (DnRSP<configDst>::OPCODE_WIDTH     >= DnRSP<configSrc>::OPCODE_WIDTH)
                && (DnRSP<configDst>::WAY_WIDTH        >= DnRSP<configSrc>::WAY_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnRSPConversionSafe([[maybe_unused]] const DnRSP<configSrc>& src)
        {
            return IsDnRSPConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnRSPConversionSafe([[maybe_unused]] const DnRSP<configSrc>& src, [[maybe_unused]] const DnRSP<configDst>& dst)
        {
            return IsDnRSPConversionSafe<configDst, configSrc>();
        }

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
            using txnid_t = dbid_t<config>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = up_nodeid_t<config>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = dn_nodeid_t<config>;

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

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline UpRSP<configDst> ConvertUpRSP(const UpRSP<configSrc>& src) noexcept
        {
            UpRSP<configDst> dst;

            dst.TxnID      = static_cast<typename UpRSP<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename UpRSP<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename UpRSP<configDst>::tgtid_t>(src.TgtID);
            dst.Opcode     = static_cast<typename UpRSP<configDst>::opcode_t>(src.Opcode);
            dst.RespErr    = static_cast<typename UpRSP<configDst>::resperr_t>(src.RespErr);
            dst.Resp       = static_cast<typename UpRSP<configDst>::resp_t>(src.Resp);
            dst.TraceTag   = static_cast<typename UpRSP<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertUpRSP(UpRSP<configDst>& dst, const UpRSP<configSrc>& src) noexcept
        {
            dst = ConvertUpRSP<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpRSPConversionSafe()
        {
            return (UpRSP<configDst>::TXNID_WIDTH      >= UpRSP<configSrc>::TXNID_WIDTH)
                && (UpRSP<configDst>::SRCID_WIDTH      >= UpRSP<configSrc>::SRCID_WIDTH)
                && (UpRSP<configDst>::TGTID_WIDTH      >= UpRSP<configSrc>::TGTID_WIDTH)
                && (UpRSP<configDst>::OPCODE_WIDTH     >= UpRSP<configSrc>::OPCODE_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpRSPConversionSafe([[maybe_unused]] const UpRSP<configSrc>& src)
        {
            return IsUpRSPConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpRSPConversionSafe([[maybe_unused]] const UpRSP<configSrc>& src, [[maybe_unused]] const UpRSP<configDst>& dst)
        {
            return IsUpRSPConversionSafe<configDst, configSrc>();
        }

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
            using txnid_t = CCHI::Flits::txnid_t<config>;

            /*
            SrcID: <DownstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::downstreamNodeIdWidth;
            using srcid_t = dn_nodeid_t<config>;

            /*
            TgtID: <UpstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::upstreamNodeIdWidth;
            using tgtid_t = up_nodeid_t<config>;

            /*
            DBID: <DBID_Width> bits
            DBID. A transaction has a unique DBID per target node.
            */
            static constexpr size_t DBID_WIDTH = config::dbIdWidth;
            using dbid_t = CCHI::Flits::dbid_t<config>;

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
            DataID: log2(512 / <Data_Width>) bits
            Data ID. The ID of the data payload in the transaction.
            */
            static constexpr size_t DATAID_WIDTH = config::dataIdWidth;

            static constexpr bool hasDataID = DATAID_WIDTH > 0;
            using dataid_t = uint_fit_t<DATAID_WIDTH, std::monostate>;

            /*
            Data: <Data_Width> bits
            Data. The data payload of the transaction.
            */
            static constexpr size_t DATA_WIDTH = config::dataWidth;
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
                                          + CBUSY_WIDTH         + WAYVALID_WIDTH    + WAY_WIDTH         + DATAID_WIDTH
                                          + DATA_WIDTH          + TRACETAG_WIDTH;

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
            dataid_t                DataID;
            data_t                  Data;
            tracetag_t              TraceTag;
        };
        
        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline DnDAT<configDst> ConvertDnDAT(const DnDAT<configSrc>& src) noexcept
        {
            DnDAT<configDst> dst;

            dst.TxnID      = static_cast<typename DnDAT<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename DnDAT<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename DnDAT<configDst>::tgtid_t>(src.TgtID);
            dst.DBID       = static_cast<typename DnDAT<configDst>::dbid_t>(src.DBID);
            dst.Opcode     = static_cast<typename DnDAT<configDst>::opcode_t>(src.Opcode);
            dst.RespErr    = static_cast<typename DnDAT<configDst>::resperr_t>(src.RespErr);
            dst.Resp       = static_cast<typename DnDAT<configDst>::resp_t>(src.Resp);
            dst.DataSource = static_cast<typename DnDAT<configDst>::datasource_t>(src.DataSource);
            dst.CBusy      = static_cast<typename DnDAT<configDst>::cbusy_t>(src.CBusy);
            dst.WayValid   = static_cast<typename DnDAT<configDst>::wayvalid_t>(src.WayValid);
            dst.Way        = static_cast<typename DnDAT<configDst>::way_t>(src.Way);
            dst.DataID     = static_cast<typename DnDAT<configDst>::dataid_t>(src.DataID);

            for (size_t i = 0; i < sizeof(dst.Data) / sizeof(dst.Data[0]); ++i)
                dst.Data[i] = src.Data[i];

            dst.TraceTag   = static_cast<typename DnDAT<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertDnDAT(DnDAT<configDst>& dst, const DnDAT<configSrc>& src) noexcept
        {
            dst = ConvertDnDAT<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnDATConversionSafe()
        {
            return (DnDAT<configDst>::TXNID_WIDTH      >= DnDAT<configSrc>::TXNID_WIDTH)
                && (DnDAT<configDst>::SRCID_WIDTH      >= DnDAT<configSrc>::SRCID_WIDTH)
                && (DnDAT<configDst>::TGTID_WIDTH      >= DnDAT<configSrc>::TGTID_WIDTH)
                && (DnDAT<configDst>::DBID_WIDTH       >= DnDAT<configSrc>::DBID_WIDTH)
                && (DnDAT<configDst>::OPCODE_WIDTH     >= DnDAT<configSrc>::OPCODE_WIDTH)
                && (DnDAT<configDst>::WAY_WIDTH        >= DnDAT<configSrc>::WAY_WIDTH)
                && (DnDAT<configDst>::DATAID_WIDTH     == DnDAT<configSrc>::DATAID_WIDTH)
                && (DnDAT<configDst>::DATA_WIDTH       == DnDAT<configSrc>::DATA_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnDATConversionSafe([[maybe_unused]] const DnDAT<configSrc>& src)
        {
            return IsDnDATConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsDnDATConversionSafe([[maybe_unused]] const DnDAT<configSrc>& src, [[maybe_unused]] const DnDAT<configDst>& dst)
        {
            return IsDnDATConversionSafe<configDst, configSrc>();
        }

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

            // DataID
            typename T::dataid_t;
            { T::DATAID_WIDTH                   } -> std::convertible_to<size_t>;

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
            using txnid_t = dbid_t<config>;

            /*
            SrcID: <UpstreamNodeID_Width> bits
            Source ID. The ID of the source node that initiates the transaction.
            */
            static constexpr size_t SRCID_WIDTH = config::upstreamNodeIdWidth;
            using srcid_t = up_nodeid_t<config>;

            /*
            TgtID: <DownstreamNodeID_Width> bits
            Target ID. The ID of the target node that is the destination of the transaction.
            */
            static constexpr size_t TGTID_WIDTH = config::downstreamNodeIdWidth;
            using tgtid_t = dn_nodeid_t<config>;

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
            DataID: log2(512 / <Data_Width>) bits
            Data ID. The ID of the data payload in the transaction.
            */
            static constexpr size_t DATAID_WIDTH = config::dataIdWidth;

            static constexpr bool hasDataID = DATAID_WIDTH > 0;
            using dataid_t = uint_fit_t<DATAID_WIDTH, std::monostate>;

            /*
            Data: <Data_Width> bits
            Data. The data payload of the transaction.
            */
            static constexpr size_t DATA_WIDTH = config::dataWidth;
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
            dataid_t                DataID;
            data_t                  Data;
            be_t                    BE;
            tracetag_t              TraceTag;
        };

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline UpDAT<configDst> ConvertUpDAT(const UpDAT<configSrc>& src) noexcept
        {
            UpDAT<configDst> dst;

            dst.TxnID      = static_cast<typename UpDAT<configDst>::txnid_t>(src.TxnID);
            dst.SrcID      = static_cast<typename UpDAT<configDst>::srcid_t>(src.SrcID);
            dst.TgtID      = static_cast<typename UpDAT<configDst>::tgtid_t>(src.TgtID);
            dst.Opcode     = static_cast<typename UpDAT<configDst>::opcode_t>(src.Opcode);
            dst.RespErr    = static_cast<typename UpDAT<configDst>::resperr_t>(src.RespErr);
            dst.Resp       = static_cast<typename UpDAT<configDst>::resp_t>(src.Resp);
            dst.DataID     = static_cast<typename UpDAT<configDst>::dataid_t>(src.DataID);

            for (size_t i = 0; i < sizeof(dst.Data) / sizeof(dst.Data[0]); ++i)
                dst.Data[i] = src.Data[i];

            dst.BE         = static_cast<typename UpDAT<configDst>::be_t>(src.BE);
            dst.TraceTag   = static_cast<typename UpDAT<configDst>::tracetag_t>(src.TraceTag);

            return dst;
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline void ConvertUpDAT(UpDAT<configDst>& dst, const UpDAT<configSrc>& src) noexcept
        {
            dst = ConvertUpDAT<configDst, configSrc>(src);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpDATConversionSafe()
        {
            return (UpDAT<configDst>::TXNID_WIDTH      >= UpDAT<configSrc>::TXNID_WIDTH)
                && (UpDAT<configDst>::SRCID_WIDTH      >= UpDAT<configSrc>::SRCID_WIDTH)
                && (UpDAT<configDst>::TGTID_WIDTH      >= UpDAT<configSrc>::TGTID_WIDTH)
                && (UpDAT<configDst>::OPCODE_WIDTH     >= UpDAT<configSrc>::OPCODE_WIDTH)
                && (UpDAT<configDst>::DATAID_WIDTH     == UpDAT<configSrc>::DATAID_WIDTH)
                && (UpDAT<configDst>::DATA_WIDTH       == UpDAT<configSrc>::DATA_WIDTH);
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpDATConversionSafe([[maybe_unused]] const UpDAT<configSrc>& src)
        {
            return IsUpDATConversionSafe<configDst, configSrc>();
        }

        template<FlitConfigurationConcept configDst, FlitConfigurationConcept configSrc>
        inline constexpr bool IsUpDATConversionSafe([[maybe_unused]] const UpDAT<configSrc>& src, [[maybe_unused]] const UpDAT<configDst>& dst)
        {
            return IsUpDATConversionSafe<configDst, configSrc>();
        }

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

            // DataID
            typename T::dataid_t;
            { T::DATAID_WIDTH                   } -> std::convertible_to<size_t>;

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


#endif // __CCHI__CCHI_PROTOCOL_FLITS
