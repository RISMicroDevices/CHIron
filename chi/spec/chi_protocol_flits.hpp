//#pragma once

//#ifndef __CHI__CHI_PROTOCOL_FLITS
//#define __CHI__CHI_PROTOCOL_FLITS

#ifndef CHI_PROTOCOL_FLITS__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_protocol_flits_header.hpp"
#   include <concepts>                                  // IWYU pragma: keep
#endif


/*
namespace CHI {
*/

    namespace FlitConfigurationConstraints {

        template<unsigned int NodeIDWidth>
        concept NodeID  = CHI::CheckNodeIdWidth(NodeIDWidth);

        template<unsigned int ReqAddrWidth>
        concept ReqAddr = CHI::CheckReqAddrWidth(ReqAddrWidth);

        template<unsigned int RSVDCWidth>
        concept RSVDC   = CHI::CheckRSVDCWidth(RSVDCWidth);

        template<unsigned int DataWidth>
        concept Data    = CHI::CheckDataWidth(DataWidth);
    }


    /*
    General Flit Configuration for all channels
    */
    template<   unsigned int NodeIDWidth           = 7
             ,  unsigned int ReqAddrWidth          = 48
             ,  unsigned int ReqRSVDCWidth         = 4
             ,  unsigned int DatRSVDCWidth         = 4
             ,  unsigned int DataWidth             = 256
             ,  bool         DataCheckPresent      = false
             ,  bool         PoisonPresent         = false
#ifdef CHI_ISSUE_EB_ENABLE
             ,  bool         MPAMPresent           = false
#endif
            >
    requires FlitConfigurationConstraints::NodeID<NodeIDWidth>
          && FlitConfigurationConstraints::ReqAddr<ReqAddrWidth>
          && FlitConfigurationConstraints::RSVDC<ReqRSVDCWidth>
          && FlitConfigurationConstraints::RSVDC<DatRSVDCWidth>
          && FlitConfigurationConstraints::Data<DataWidth>
    struct FlitConfiguration {
        static constexpr unsigned int   nodeIdWidth             = NodeIDWidth;
        static constexpr unsigned int   reqAddrWidth            = ReqAddrWidth;
        static constexpr unsigned int   snpAddrWidth            = ReqAddrWidth - 3;
#ifdef CHI_ISSUE_EB_ENABLE
        static constexpr unsigned int   tagWidth                = DataWidth / 32;
        static constexpr unsigned int   tagUpdateWidth          = DataWidth / 128;
#endif
        static constexpr unsigned int   reqRsvdcWidth           = ReqRSVDCWidth;
        static constexpr unsigned int   datRsvdcWidth           = DatRSVDCWidth;
        static constexpr unsigned int   dataWidth               = DataWidth;
        static constexpr unsigned int   byteEnableWidth         = DataWidth / 8;
        static constexpr unsigned int   dataCheckWidth          = DataCheckPresent ? (DataWidth / 8) : 0;
        static constexpr unsigned int   poisonWidth             = PoisonPresent ? (DataWidth / 64) : 0;
#ifdef CHI_ISSUE_EB_ENABLE
        static constexpr unsigned int   mpamWidth               = MPAMPresent ? 11 : 0;
#endif
    };


    /*
    General Flit Configuration concept for all channels
    */
    template<class T>
    concept FlitConfigurationConcept = requires {
        { T::nodeIdWidth        }   -> std::convertible_to<unsigned int>;
        { T::reqAddrWidth       }   -> std::convertible_to<unsigned int>;
        { T::snpAddrWidth       }   -> std::convertible_to<unsigned int>;
#ifdef CHI_ISSUE_EB_ENABLE
        { T::tagWidth           }   -> std::convertible_to<unsigned int>;
        { T::tagUpdateWidth     }   -> std::convertible_to<unsigned int>;
#endif
        { T::reqRsvdcWidth      }   -> std::convertible_to<unsigned int>;
        { T::datRsvdcWidth      }   -> std::convertible_to<unsigned int>;
        { T::dataWidth          }   -> std::convertible_to<unsigned int>;
        { T::byteEnableWidth    }   -> std::convertible_to<unsigned int>;
        { T::dataCheckWidth     }   -> std::convertible_to<unsigned int>;
        { T::poisonWidth        }   -> std::convertible_to<unsigned int>;
#ifdef CHI_ISSUE_EB_ENABLE
        { T::mpamWidth          }   -> std::convertible_to<unsigned int>;
#endif
    };

    /*
    REQ Flit Configuration concept
    */
    template<class T>
    concept REQFlitConfigurationConcept = requires {
        { T::nodeIdWidth        }   -> std::convertible_to<unsigned int>;
        { T::reqAddrWidth       }   -> std::convertible_to<unsigned int>;
        { T::reqRsvdcWidth      }   -> std::convertible_to<unsigned int>;
#ifdef CHI_ISSUE_EB_ENABLE
        { T::mpamWidth          }   -> std::convertible_to<unsigned int>;
#endif
    };

    /*
    DAT Flit Configuration concept
    */
    template<class T>
    concept DATFlitConfigurationConcept = requires {
        { T::nodeIdWidth        }   -> std::convertible_to<unsigned int>;
        { T::dataWidth          }   -> std::convertible_to<unsigned int>;
#ifdef CHI_ISSUE_EB_ENABLE
        { T::tagWidth           }   -> std::convertible_to<unsigned int>;
        { T::tagUpdateWidth     }   -> std::convertible_to<unsigned int>;
#endif
        { T::datRsvdcWidth      }   -> std::convertible_to<unsigned int>;
        { T::byteEnableWidth    }   -> std::convertible_to<unsigned int>;
        { T::dataCheckWidth     }   -> std::convertible_to<unsigned int>;
        { T::poisonWidth        }   -> std::convertible_to<unsigned int>;
    };

    /*
    RSP Flit Configuration concept
    */
    template<class T>
    concept RSPFlitConfigurationConcept = requires {
        { T::nodeIdWidth        }   -> std::convertible_to<unsigned int>;
    };

    /*
    SNP Flit Configuration concept
    */
    template<class T>
    concept SNPFlitConfigurationConcept = requires {
        { T::nodeIdWidth        }   -> std::convertible_to<unsigned int>;
        { T::snpAddrWidth       }   -> std::convertible_to<unsigned int>;
#ifdef CHI_ISSUE_EB_ENABLE
        { T::mpamWidth          }   -> std::convertible_to<unsigned int>;
#endif
    };


    namespace Flits {

        //
        using FlitRange = std::pair<size_t, size_t>;

        //
        template<REQFlitConfigurationConcept    config      = FlitConfiguration<>,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class REQ {
        public:
            /*
            QoS: 4 bits
            Quality of Service priority. Specifies 1 of 16 possible priority levels for the 
            transaction with ascending values of QoS indicating higher priority levels.
            */
            static constexpr size_t     QOS_WIDTH   = 4;
            static constexpr size_t     QOS_LSB     = 0;
            static constexpr size_t     QOS_MSB     = 3;
            static constexpr FlitRange  QOS_RANGE   = { QOS_LSB, QOS_MSB };

            using qos_t = uint_fit_t<QOS_WIDTH>;

            /*
            TgtID: <NodeIDWidth> bits
            Target ID. The node ID of the port on the component to which the packet is 
            targeted.
            */
            static constexpr size_t     TGTID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     TGTID_LSB   = QOS_MSB + 1;
            static constexpr size_t     TGTID_MSB   = QOS_MSB + TGTID_WIDTH;
            static constexpr FlitRange  TGTID_RANGE = { TGTID_LSB, TGTID_MSB };

            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            SrcID: <NodeIDWidth> bits
            Source ID. The node ID of the port on the component from which the packet 
            was sent.
            */
            static constexpr size_t     SRCID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     SRCID_LSB   = TGTID_MSB + 1;
            static constexpr size_t     SRCID_MSB   = TGTID_MSB + SRCID_WIDTH;
            static constexpr FlitRange  SRCID_RANGE = { SRCID_LSB, SRCID_MSB };

            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TxnID: 
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TXNID_WIDTH = 12;
#endif
            static constexpr size_t     TXNID_LSB   = SRCID_MSB + 1;
            static constexpr size_t     TXNID_MSB   = SRCID_MSB + TXNID_WIDTH;
            static constexpr FlitRange  TXNID_RANGE = { SRCID_LSB, SRCID_MSB };

            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            ReturnNID: <NodeIDWidth> bits
            Return Node ID. The node ID that the response with Data is to be sent to.
            */
            static constexpr size_t     RETURNNID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     RETURNNID_LSB   = TXNID_MSB + 1;
            static constexpr size_t     RETURNNID_MSB   = TXNID_MSB + RETURNNID_WIDTH;
            static constexpr FlitRange  RETURNNID_RANGE = { RETURNNID_LSB, RETURNNID_MSB };

            using returnnid_t = uint_strict_t<RETURNNID_WIDTH>;

            /*
            StashNID: <NodeIDWidth> bits
            Stash Node ID. The node ID of the Stash target.
            */
            static constexpr size_t     STASHNID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     STASHNID_LSB   = TXNID_MSB + 1;
            static constexpr size_t     STASHNID_MSB   = TXNID_MSB + STASHNID_WIDTH;
            static constexpr FlitRange  STASHNID_RANGE = { STASHNID_LSB, STASHNID_MSB };

            using stashnid_t = uint_strict_t<STASHNID_WIDTH>;

            /*
            SLCRepHint:
                * N/A    for CHI Issue B
                * 7 bits for CHI Issue E.b
            System Level Cache Replacement Hint. Forwards cache replacement hints 
            from the Requesters to the caches in the interconnect.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     SLCREPHINT_WIDTH = 7;
            static constexpr size_t     SLCREPHINT_LSB   = TXNID_MSB + 1;
            static constexpr size_t     SLCREPHINT_MSB   = TXNID_MSB + SLCREPHINT_WIDTH;
            static constexpr FlitRange  SLCREPHINT_RANGE = { SLCREPHINT_LSB, SLCREPHINT_MSB };

            using slcrephint_t = uintfitn_strict_t<RETURNNID_WIDTH, SLCREPHINT_WIDTH>;
#endif

            /*
            StashNIDValid: 1 bit
            Stash Node ID Valid. Indicates that the StashNID field has a valid Stash target 
            value.
            */
            static constexpr size_t     STASHNIDVALID_WIDTH = 1;
            static constexpr size_t     STASHNIDVALID_LSB   = RETURNNID_MSB + 1;
            static constexpr size_t     STASHNIDVALID_MSB   = RETURNNID_MSB + STASHNIDVALID_WIDTH;
            static constexpr FlitRange  STASHNIDVALID_RANGE = { STASHNIDVALID_LSB, STASHNIDVALID_MSB };

            using stashnidvalid_t = uint_strict_t<STASHNIDVALID_WIDTH>;
            
            /*
            Endian: 1 bit
            Endianness. Indicates the endianness of data in the data packet for Atomic 
            transactions.
            */
            static constexpr size_t     ENDIAN_WIDTH = 1;
            static constexpr size_t     ENDIAN_LSB   = RETURNNID_MSB + 1;
            static constexpr size_t     ENDIAN_MSB   = RETURNNID_MSB + ENDIAN_WIDTH;
            static constexpr FlitRange  ENDIAN_RANGE = { ENDIAN_LSB, ENDIAN_MSB };

            using endian_t = uint_strict_t<ENDIAN_WIDTH>;

            /*
            Deep:
                * N/A   for CHI Issue B
                * 1 bit for CHI Issue E.b
            Deep persistence. Indicates that the Persist response must not be sent until all 
            earlier writes are written to the final destination.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     DEEP_WIDTH = 1;
            static constexpr size_t     DEEP_LSB   = RETURNNID_MSB + 1;
            static constexpr size_t     DEEP_MSB   = RETURNNID_MSB + DEEP_WIDTH;
            static constexpr FlitRange  DEEP_RANGE = { DEEP_LSB, DEEP_MSB };

            using deep_t = uint_strict_t<DEEP_WIDTH>;
#endif

            /*
            ReturnTxnID: 
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Return Transaction ID. The unique transaction ID that conveys the value of 
            TxnID in the data response from the Slave.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     RETURNTXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     RETURNTXNID_WIDTH = 12;
#endif
            static constexpr size_t     RETURNTXNID_LSB   = STASHNIDVALID_MSB + 1;
            static constexpr size_t     RETURNTXNID_MSB   = STASHNIDVALID_MSB + RETURNTXNID_WIDTH;
            static constexpr FlitRange  RETURNTXNID_RANGE = { RETURNTXNID_LSB, RETURNTXNID_MSB };

            using returntxnid_t = uint_strict_t<RETURNTXNID_WIDTH>;

            /*
            StashLPID: 5 bits
            Stash Logical Processor ID. The ID of the logical processor at the Stash target. 
            */
            static constexpr size_t     STASHLPID_WIDTH = 5;
            static constexpr size_t     STASHLPID_LSB   = STASHNIDVALID_MSB + 1;
            static constexpr size_t     STASHLPID_MSB   = STASHNIDVALID_MSB + STASHLPID_WIDTH;
            static constexpr FlitRange  STASHLPID_RANGE = { STASHLPID_LSB, STASHLPID_MSB };

            using stashlpid_t = uintfitn_strict_t<RETURNTXNID_WIDTH, STASHLPID_WIDTH>;

            /*
            StashLPIDValid: 1 bit
            Stash Logical Processor ID Valid. Indicates that the StashLPID field value 
            must be considered as the Stash target.
            */
            static constexpr size_t     STASHLPIDVALID_WIDTH = 1;
            static constexpr size_t     STASHLPIDVALID_LSB   = STASHLPID_MSB + 1;
            static constexpr size_t     STASHLPIDVALID_MSB   = STASHLPID_MSB + STASHLPIDVALID_WIDTH;
            static constexpr FlitRange  STASHLPIDVALID_RANGE = { STASHLPIDVALID_LSB, STASHLPIDVALID_MSB };

            using stashlpidvalid_t = uintfitn_strict_t<RETURNTXNID_WIDTH, STASHLPIDVALID_WIDTH, STASHLPID_WIDTH>;

            /*
            Opcode:
                * 6 bits for CHI Issue B
                * 7 bits for CHI Issue E.b
            Request opcode. Specifies the transaction type and is the primary field that 
            determines the transaction structure.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 6;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 7;
#endif
            static constexpr size_t     OPCODE_LSB   = RETURNTXNID_MSB + 1;
            static constexpr size_t     OPCODE_MSB   = RETURNTXNID_MSB + OPCODE_WIDTH;
            static constexpr FlitRange  OPCODE_RANGE = { OPCODE_LSB, OPCODE_MSB };

            using opcode_t = uint_fit_t<OPCODE_WIDTH>;

            /*
            Size: 3 bits
            Data size. Specifies the size of the data associated with the transaction. This 
            determines the number of data packets within the transaction.
            */
            static constexpr size_t     SSIZE_WIDTH = 3;
            static constexpr size_t     SSIZE_LSB   = OPCODE_MSB + 1;
            static constexpr size_t     SSIZE_MSB   = OPCODE_MSB + SSIZE_WIDTH;
            static constexpr FlitRange  SSIZE_RANGE = { SSIZE_LSB, SSIZE_MSB };

            using ssize_t = uint_fit_t<SSIZE_WIDTH>;

            /*
            Addr: <ReqAddrWidth> bits
            Address. The address of the memory location being accessed for read and 
            write requests.
            */
            static constexpr size_t     ADDR_WIDTH = config::reqAddrWidth;
            static constexpr size_t     ADDR_LSB   = SSIZE_MSB + 1;
            static constexpr size_t     ADDR_MSB   = SSIZE_MSB + ADDR_WIDTH;
            static constexpr FlitRange  ADDR_RANGE = { ADDR_LSB, ADDR_MSB };

            using addr_t = uint_fit_t<ADDR_WIDTH>;

            /*
            NS: 1 bit
            Non-secure. Determines if the transaction is Non-secure or Secure.
            */
            static constexpr size_t     NS_WIDTH = 1;
            static constexpr size_t     NS_LSB   = ADDR_MSB + 1;
            static constexpr size_t     NS_MSB   = ADDR_MSB + NS_WIDTH;
            static constexpr FlitRange  NS_RANGE = { NS_LSB, NS_MSB };

            using ns_t = uint_fit_t<NS_WIDTH>;

            /*
            LikelyShared: 1 bit
            Likely Shared. Provides an allocation hint for downstream caches.
            */
            static constexpr size_t     LIKELYSHARED_WIDTH = 1;
            static constexpr size_t     LIKELYSHARED_LSB   = NS_MSB + 1;
            static constexpr size_t     LIKELYSHARED_MSB   = NS_MSB + LIKELYSHARED_WIDTH;
            static constexpr FlitRange  LIKELYSHARED_RANGE = { LIKELYSHARED_LSB, LIKELYSHARED_MSB };
        
            using likelyshared_t = uint_fit_t<LIKELYSHARED_WIDTH>;

            /*
            AllowRetry: 1 bit
            Allow Retry. Determines if the target is permitted to give a Retry response.
            */
            static constexpr size_t     ALLOWRETRY_WIDTH = 1;
            static constexpr size_t     ALLOWRETRY_LSB   = LIKELYSHARED_MSB + 1;
            static constexpr size_t     ALLOWRETRY_MSB   = LIKELYSHARED_MSB + ALLOWRETRY_WIDTH;
            static constexpr FlitRange  ALLOWRETRY_RANGE = { ALLOWRETRY_LSB, ALLOWRETRY_MSB };

            using allowretry_t = uint_fit_t<ALLOWRETRY_WIDTH>;

            /*
            Order: 2 bits
            Order requirement. Determines the ordering requirement for this request with 
            respect to other transactions from the same agent. See Ordering on page 2-63.
            */
            static constexpr size_t     ORDER_WIDTH = 2;
            static constexpr size_t     ORDER_LSB   = ALLOWRETRY_MSB + 1;
            static constexpr size_t     ORDER_MSB   = ALLOWRETRY_MSB + ORDER_WIDTH;
            static constexpr FlitRange  ORDER_RANGE = { ORDER_LSB, ORDER_MSB };

            using order_t = uint_fit_t<ORDER_WIDTH>;

            /*
            PCrdType: 4 bits
            Protocol Credit Type. Indicates the type of Protocol Credit being used by a 
            request that has the AllowRetry field deasserted.
            */
            static constexpr size_t     PCRDTYPE_WIDTH = 4;
            static constexpr size_t     PCRDTYPE_LSB   = ORDER_MSB + 1;
            static constexpr size_t     PCRDTYPE_MSB   = ORDER_MSB + PCRDTYPE_WIDTH;
            static constexpr FlitRange  PCRDTYPE_RANGE = { PCRDTYPE_LSB, PCRDTYPE_MSB };

            using pcrdtype_t = uint_fit_t<PCRDTYPE_WIDTH>;

            /*
            MemAttr: 4 bits
            Memory attribute. Determines the memory attributes associated with the 
            transaction.
            */
            static constexpr size_t     MEMATTR_WIDTH = 4;
            static constexpr size_t     MEMATTR_LSB   = PCRDTYPE_MSB + 1;
            static constexpr size_t     MEMATTR_MSB   = PCRDTYPE_MSB + MEMATTR_WIDTH;
            static constexpr FlitRange  MEMATTR_RANGE = { MEMATTR_LSB, MEMATTR_MSB };

            using memattr_t = uint_fit_t<MEMATTR_WIDTH>;

            /*
            SnpAttr: 1 bit
            Snoop attribute. Specifies the snoop attributes associated with the transaction.
            */
            static constexpr size_t     SNPATTR_WIDTH = 1;
            static constexpr size_t     SNPATTR_LSB   = MEMATTR_MSB + 1;
            static constexpr size_t     SNPATTR_MSB   = MEMATTR_MSB + SNPATTR_WIDTH;
            static constexpr FlitRange  SNPATTR_RANGE = { SNPATTR_LSB, SNPATTR_MSB };

            using snpattr_t = uint_strict_t<SNPATTR_WIDTH>;

            /*
            DoDWT:
                * N/A   for CHI Issue B
                * 1 bit for CHI Issue E.b
            Do Direct Write Transfer. Supports Direct Write-data Transfer and the 
            handling of Combined Writes.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     DODWT_WIDTH = 1;
            static constexpr size_t     DODWT_LSB   = MEMATTR_MSB + 1;
            static constexpr size_t     DODWT_MSB   = MEMATTR_MSB + DODWT_WIDTH;
            static constexpr FlitRange  DODWT_RANGE = { DODWT_LSB, DODWT_MSB };

            using dodwt_t = uint_strict_t<DODWT_WIDTH>;
#endif

            /*
            LPID: 5 bits
            Logical Processor ID. Used in conjunction with the SrcID field to uniquely 
            identify the logical processor that generated the request.
            */
            static constexpr size_t     LPID_WIDTH = 5;
            static constexpr size_t     LPID_LSB   = SNPATTR_MSB + 1;
            static constexpr size_t     LPID_MSB   = SNPATTR_MSB + LPID_WIDTH;
            static constexpr FlitRange  LPID_RANGE = { LPID_LSB, LPID_MSB };

#ifdef CHI_ISSUE_B_ENABLE
            using lpid_t = uint_fit_t<LPID_WIDTH>;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            using lpid_t = uintfitn_strict_t<8 /*TAGGROUPID_WIDTH*/, LPID_WIDTH>;
#endif

            /*
            PGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            Persistence Group ID. Indicates the set of CleanSharedPersistSep transactions 
            to which the request applies.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     PGROUPID_WIDTH = 8;
            static constexpr size_t     PGROUPID_LSB   = SNPATTR_MSB + 1;
            static constexpr size_t     PGROUPID_MSB   = SNPATTR_MSB + PGROUPID_WIDTH;
            static constexpr FlitRange  PRGOUPID_RANGE = { PGROUPID_LSB, PGROUPID_MSB };

            using pgroupid_t = uint_strict_t<PGROUPID_WIDTH>;
#endif

            /*
            StashGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            Stash Group ID. Indicates the set of StashOnceSep transactions to which the 
            request applies.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     STASHGROUPID_WIDTH = 8;
            static constexpr size_t     STASHGROUPID_LSB   = SNPATTR_MSB + 1;
            static constexpr size_t     STASHGROUPID_MSB   = SNPATTR_MSB + STASHGROUPID_WIDTH;
            static constexpr FlitRange  STASHGROUPID_RANGE = { STASHGROUPID_LSB, STASHGROUPID_MSB };

            using stashgroupid_t = uint_strict_t<STASHGROUPID_WIDTH>;
#endif

            /*
            TagGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            TagGroupID. Precise contents are IMPLEMENTATION DEFINED. Typically 
            expected to contain Exception level, TTBR value, and CPU identifier.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAGGROUPID_WIDTH = 8;
            static constexpr size_t     TAGGROUPID_LSB   = SNPATTR_MSB + 1;
            static constexpr size_t     TAGGROUPID_MSB   = SNPATTR_MSB + TAGGROUPID_WIDTH;
            static constexpr FlitRange  TAGGROUPID_RANGE = { TAGGROUPID_LSB, TAGGROUPID_MSB };

            using taggroupid_t = uint_strict_t<TAGGROUPID_WIDTH>;
#endif

            /*
            Excl: 1 bit
            Exclusive access. Indicates that the corresponding transaction is an Exclusive 
            access transaction.
            */
            static constexpr size_t     EXCL_WIDTH = 1;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     EXCL_LSB   = LPID_MSB + 1;
            static constexpr size_t     EXCL_MSB   = LPID_MSB + EXCL_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     EXCL_LSB   = TAGGROUPID_MSB + 1;
            static constexpr size_t     EXCL_MSB   = TAGGROUPID_MSB + EXCL_WIDTH;
#endif
            static constexpr FlitRange  EXCL_RANGE = { EXCL_LSB, EXCL_MSB };

            using excl_t = uint_strict_t<EXCL_WIDTH>;
            
            /*
            SnoopMe: 1 bit
            Snoop Me. Indicates that Home must determine whether to send a snoop to the 
            Requester.
            */
            static constexpr size_t     SNOOPME_WIDTH = 1;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     SNOOPME_LSB   = LPID_MSB + 1;
            static constexpr size_t     SNOOPME_MSB   = LPID_MSB + SNOOPME_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     SNOOPME_LSB   = TAGGROUPID_MSB + 1;
            static constexpr size_t     SNOOPME_MSB   = TAGGROUPID_MSB + SNOOPME_WIDTH;
#endif
            static constexpr FlitRange  SNOOPME_RAGNE = { SNOOPME_LSB, SNOOPME_MSB };

            using snoopme_t = uint_strict_t<SNOOPME_WIDTH>;

            /*
            ExpCompAck: 1 bit
            Expect CompAck. Indicates that the transaction will include a completion 
            acknowledge message.
            */
            static constexpr size_t     EXPCOMPACK_WIDTH = 1;
            static constexpr size_t     EXPCOMPACK_LSB   = EXCL_MSB + 1;
            static constexpr size_t     EXPCOMPACK_MSB   = EXCL_MSB + EXPCOMPACK_WIDTH;
            static constexpr FlitRange  EXPCOMPACK_RANGE = { EXPCOMPACK_LSB, EXPCOMPACK_MSB };

            using expcompack_t = uint_fit_t<EXPCOMPACK_WIDTH>;

            /*
            TagOp:
                * N/A    for CHI Issue B
                * 2 bits for CHI Issue E.b
            Tag Operation. Indicates the operation to be performed on the tags present in 
            the corresponding DAT channel.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAGOP_WIDTH = 2;
            static constexpr size_t     TAGOP_LSB   = EXPCOMPACK_MSB + 1;
            static constexpr size_t     TAGOP_MSB   = EXPCOMPACK_MSB + TAGOP_WIDTH;
            static constexpr FlitRange  TAGOP_RANGE = { TAGOP_LSB, TAGOP_MSB };

            using tagop_t = uint_fit_t<TAGOP_WIDTH>;
#endif

            /*
            TraceTag: 1 bit
            Trace Tag. Provides additional support for the debugging, tracing, and 
            performance measurement of systems. See Chapter 11 Data Source and Trace 
            Tag.
            */
            static constexpr size_t     TRACETAG_WIDTH = 1;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TRACETAG_LSB   = EXPCOMPACK_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = EXPCOMPACK_MSB + TRACETAG_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TRACETAG_LSB   = TAGOP_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = TAGOP_MSB + TRACETAG_WIDTH;
#endif
            static constexpr FlitRange  TRACETAG_RANGE = { TRACETAG_LSB, TRACETAG_MSB };

            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

            /*
            MPAM:
                * N/A         for CHI Issue B
                * <mpamWidth> for CHI Issue E.b
            Memory System Performance Resource Partitioning and Monitoring. 
            Efficiently utilizes the memory resources among users and monitors their use.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     MPAM_WIDTH = config::mpamWidth;
            static constexpr size_t     MPAM_LSB   = TRACETAG_MSB + 1;
            static constexpr size_t     MPAM_MSB   = TRACETAG_MSB + MPAM_WIDTH;
            static constexpr FlitRange  MPAM_RANGE = { MPAM_LSB, MPAM_MSB };

            static constexpr bool   hasMPAM = config::mpamWidth > 0;
            // *NOTICE: Use 'if constexpr (hasMPAM)' to check if 'MPAM' is available.
            using mpam_t = uint_fit_t<MPAM_WIDTH, std::monostate>;
#endif

            /*
            RSVDC: <reqRsvdcWidth> bits
            User-defined.
            */
            static constexpr size_t     RSVDC_WIDTH = config::reqRsvdcWidth;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     RSVDC_LSB   = TRACETAG_MSB + 1;
            static constexpr size_t     RSVDC_MSB   = TRACETAG_MSB + RSVDC_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     RSVDC_LSB   = MPAM_MSB + 1;
            static constexpr size_t     RSVDC_MSB   = MPAM_MSB + RSVDC_WIDTH;
#endif
            static constexpr FlitRange  RSVDC_RANGE = { RSVDC_LSB, RSVDC_MSB };

            static constexpr bool   hasRSVDC = config::reqRsvdcWidth > 0;
            // *NOTICE: Use 'if constexpr (hasRSVDC)' to check if 'RSVDC' is available.
            using rsvdc_t = uint_fit_t<RSVDC_WIDTH, std::monostate>;

        public:
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + RETURNNID_WIDTH + STASHNIDVALID_WIDTH                   + RETURNTXNID_WIDTH
                                              + OPCODE_WIDTH    + SSIZE_WIDTH       + ADDR_WIDTH        + NS_WIDTH
                                              + LIKELYSHARED_WIDTH                  + ALLOWRETRY_WIDTH  + ORDER_WIDTH
                                              + PCRDTYPE_WIDTH  + MEMATTR_WIDTH     + SNPATTR_WIDTH     + LPID_WIDTH
                                              + EXCL_WIDTH      + EXPCOMPACK_WIDTH  + TRACETAG_WIDTH    + RSVDC_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + RETURNNID_WIDTH + STASHNIDVALID_WIDTH                   + RETURNTXNID_WIDTH
                                              + OPCODE_WIDTH    + SSIZE_WIDTH       + ADDR_WIDTH        + NS_WIDTH
                                              + LIKELYSHARED_WIDTH                  + ALLOWRETRY_WIDTH  + ORDER_WIDTH
                                              + PCRDTYPE_WIDTH  + MEMATTR_WIDTH     + SNPATTR_WIDTH     + TAGGROUPID_WIDTH
                                              + EXCL_WIDTH      + EXPCOMPACK_WIDTH  + TAGOP_WIDTH       + TRACETAG_WIDTH
                                              + MPAM_WIDTH      + RSVDC_WIDTH;
#endif
            static constexpr size_t     LSB   = 0;
            static constexpr size_t     MSB   = WIDTH - 1;
            static constexpr FlitRange  RANGE = { LSB, MSB };

        // Request flit fields
        // *NOTICE: Some fields are overlapped in Flit Implementation.
        public:
            as_pointer_if_t<conn::connectedIO, qos_t>               _QoS;
            as_pointer_if_t<conn::connectedIO, tgtid_t>             _TgtID;
            as_pointer_if_t<conn::connectedIO, srcid_t>             _SrcID;
            as_pointer_if_t<conn::connectedIO, txnid_t>             _TxnID;
            union {
            as_pointer_if_t<conn::connectedIO, returnnid_t>         _ReturnNID;
            as_pointer_if_t<conn::connectedIO, stashnid_t>          _StashNID;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, slcrephint_t>        _SLCRepHint;
#endif
            };
            union {
            as_pointer_if_t<conn::connectedIO, stashnidvalid_t>     _StashNIDValid;
            as_pointer_if_t<conn::connectedIO, endian_t>            _Endian;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, deep_t>              _Deep;
#endif
            };
            union {
            as_pointer_if_t<conn::connectedIO, returntxnid_t>       _ReturnTxnID;
            as_pointer_if_t<conn::connectedIO, stashlpidvalid_t>    _StashLPIDValid;
            as_pointer_if_t<conn::connectedIO, stashlpid_t>         _StashLPID;
            };
            as_pointer_if_t<conn::connectedIO, opcode_t>            _Opcode;
            as_pointer_if_t<conn::connectedIO, ssize_t>             _Size;
            as_pointer_if_t<conn::connectedIO, addr_t>              _Addr;
            as_pointer_if_t<conn::connectedIO, ns_t>                _NS;
            as_pointer_if_t<conn::connectedIO, likelyshared_t>      _LikelyShared;
            as_pointer_if_t<conn::connectedIO, allowretry_t>        _AllowRetry;
            as_pointer_if_t<conn::connectedIO, order_t>             _Order;
            as_pointer_if_t<conn::connectedIO, pcrdtype_t>          _PCrdType;
            as_pointer_if_t<conn::connectedIO, memattr_t>           _MemAttr;
            union {
            as_pointer_if_t<conn::connectedIO, snpattr_t>           _SnpAttr;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, dodwt_t>             _DoDWT;
#endif
            };
            union {
            as_pointer_if_t<conn::connectedIO, lpid_t>              _LPID;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, pgroupid_t>          _PGroupID;
            as_pointer_if_t<conn::connectedIO, stashgroupid_t>      _StashGroupID;
            as_pointer_if_t<conn::connectedIO, taggroupid_t>        _TagGroupID;
#endif
            };
            union {
            as_pointer_if_t<conn::connectedIO, excl_t>              _Excl;
            as_pointer_if_t<conn::connectedIO, snoopme_t>           _SnoopMe;
            };
            as_pointer_if_t<conn::connectedIO, expcompack_t>        _ExpCompAck;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, tagop_t>             _TagOp;
#endif
            as_pointer_if_t<conn::connectedIO, tracetag_t>          _TraceTag;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, mpam_t>              _MPAM;
#endif
            as_pointer_if_t<conn::connectedIO, rsvdc_t>             _RSVDC;

        // field decay helper functions
        public:
            inline constexpr        qos_t&              QoS             ()          noexcept { return CHI::decay<qos_t>(_QoS); }
            inline constexpr        tgtid_t&            TgtID           ()          noexcept { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr        srcid_t&            SrcID           ()          noexcept { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr        txnid_t&            TxnID           ()          noexcept { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr        returnnid_t&        ReturnNID       ()          noexcept { return CHI::decay<returnnid_t>(_ReturnNID); }
            inline constexpr        stashnid_t&         StashNID        ()          noexcept { return CHI::decay<stashnid_t>(_StashNID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        slcrephint_t&       SLCRepHint      ()          noexcept { return CHI::decay<slcrephint_t>(_SLCRepHint); }
#endif
            inline constexpr        stashnidvalid_t&    StashNIDValid   ()          noexcept { return CHI::decay<stashnidvalid_t>(_StashNIDValid); }
            inline constexpr        endian_t&           Endian          ()          noexcept { return CHI::decay<endian_t>(_Endian); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        deep_t&             Deep            ()          noexcept { return CHI::decay<deep_t>(_Deep); }
#endif
            inline constexpr        returntxnid_t&      ReturnTxnID     ()          noexcept { return CHI::decay<returntxnid_t>(_ReturnTxnID); }
            inline constexpr        stashlpidvalid_t&   StashLPIDValid  ()          noexcept { return CHI::decay<stashlpidvalid_t>(_StashLPIDValid); }
            inline constexpr        stashlpid_t&        StashLPID       ()          noexcept { return CHI::decay<stashlpid_t>(_StashLPID); }
            inline constexpr        opcode_t&           Opcode          ()          noexcept { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr        ssize_t&            Size            ()          noexcept { return CHI::decay<ssize_t>(_Size); }
            inline constexpr        addr_t&             Addr            ()          noexcept { return CHI::decay<addr_t>(_Addr); }
            inline constexpr        ns_t&               NS              ()          noexcept { return CHI::decay<ns_t>(_NS); }
            inline constexpr        likelyshared_t&     LikelyShared    ()          noexcept { return CHI::decay<likelyshared_t>(_LikelyShared); }
            inline constexpr        allowretry_t&       AllowRetry      ()          noexcept { return CHI::decay<allowretry_t>(_AllowRetry); }
            inline constexpr        order_t&            Order           ()          noexcept { return CHI::decay<order_t>(_Order); }
            inline constexpr        pcrdtype_t&         PCrdType        ()          noexcept { return CHI::decay<pcrdtype_t>(_PCrdType); }
            inline constexpr        memattr_t&          MemAttr         ()          noexcept { return CHI::decay<memattr_t>(_MemAttr); }
            inline constexpr        snpattr_t&          SnpAttr         ()          noexcept { return CHI::decay<snpattr_t>(_SnpAttr); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        dodwt_t&            DoDWT           ()          noexcept { return CHI::decay<dodwt_t>(_DoDWT); }
#endif
            inline constexpr        lpid_t&             LPID            ()          noexcept { return CHI::decay<lpid_t>(_LPID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        pgroupid_t&         PGroupID        ()          noexcept { return CHI::decay<pgroupid_t>(_PGroupID); }
            inline constexpr        stashgroupid_t&     StashGroupID    ()          noexcept { return CHI::decay<stashgroupid_t>(_StashGroupID); }
            inline constexpr        taggroupid_t&       TagGroupID      ()          noexcept { return CHI::decay<taggroupid_t>(_TagGroupID); }
#endif
            inline constexpr        excl_t&             Excl            ()          noexcept { return CHI::decay<excl_t>(_Excl); }
            inline constexpr        snoopme_t&          SnoopMe         ()          noexcept { return CHI::decay<snoopme_t>(_SnoopMe); }
            inline constexpr        expcompack_t&       ExpCompAck      ()          noexcept { return CHI::decay<expcompack_t>(_ExpCompAck); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        tagop_t&            TagOp           ()          noexcept { return CHI::decay<tagop_t>(_TagOp); }
#endif
            inline constexpr        tracetag_t&         TraceTag        ()          noexcept { return CHI::decay<tracetag_t>(_TraceTag); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        mpam_t&             MPAM            ()          noexcept { return CHI::decay<mpam_t>(_MPAM); }
#endif
            inline constexpr        rsvdc_t&            RSVDC           ()          noexcept { return CHI::decay<rsvdc_t>(_RSVDC); }

        // field decay helper functions (const)
        public:
            inline constexpr const  qos_t&              QoS             () const    noexcept { return CHI::decay<qos_t>(_QoS); }
            inline constexpr const  tgtid_t&            TgtID           () const    noexcept { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr const  srcid_t&            SrcID           () const    noexcept { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr const  txnid_t&            TxnID           () const    noexcept { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr const  returnnid_t&        ReturnNID       () const    noexcept { return CHI::decay<returnnid_t>(_ReturnNID); }
            inline constexpr const  stashnid_t&         StashNID        () const    noexcept { return CHI::decay<stashnid_t>(_StashNID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  slcrephint_t&       SLCRepHint      () const    noexcept { return CHI::decay<slcrephint_t>(_SLCRepHint); }
#endif
            inline constexpr const  stashnidvalid_t&    StashNIDValid   () const    noexcept { return CHI::decay<stashnidvalid_t>(_StashNIDValid); }
            inline constexpr const  endian_t&           Endian          () const    noexcept { return CHI::decay<endian_t>(_Endian); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  deep_t&             Deep            () const    noexcept { return CHI::decay<deep_t>(_Deep); }
#endif
            inline constexpr const  returntxnid_t&      ReturnTxnID     () const    noexcept { return CHI::decay<returntxnid_t>(_ReturnTxnID); }
            inline constexpr const  stashlpidvalid_t&   StashLPIDValid  () const    noexcept { return CHI::decay<stashlpidvalid_t>(_StashLPIDValid); }
            inline constexpr const  stashlpid_t&        StashLPID       () const    noexcept { return CHI::decay<stashlpid_t>(_StashLPID); }
            inline constexpr const  opcode_t&           Opcode          () const    noexcept { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr const  ssize_t&            Size            () const    noexcept { return CHI::decay<ssize_t>(_Size); }
            inline constexpr const  addr_t&             Addr            () const    noexcept { return CHI::decay<addr_t>(_Addr); }
            inline constexpr const  ns_t&               NS              () const    noexcept { return CHI::decay<ns_t>(_NS); }
            inline constexpr const  likelyshared_t&     LikelyShared    () const    noexcept { return CHI::decay<likelyshared_t>(_LikelyShared); }
            inline constexpr const  allowretry_t&       AllowRetry      () const    noexcept { return CHI::decay<allowretry_t>(_AllowRetry); }
            inline constexpr const  order_t&            Order           () const    noexcept { return CHI::decay<order_t>(_Order); }
            inline constexpr const  pcrdtype_t&         PCrdType        () const    noexcept { return CHI::decay<pcrdtype_t>(_PCrdType); }
            inline constexpr const  memattr_t&          MemAttr         () const    noexcept { return CHI::decay<memattr_t>(_MemAttr); }
            inline constexpr const  snpattr_t&          SnpAttr         () const    noexcept { return CHI::decay<snpattr_t>(_SnpAttr); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  dodwt_t&            DoDWT           () const    noexcept { return CHI::decay<dodwt_t>(_DoDWT); }
#endif
            inline constexpr const  lpid_t&             LPID            () const    noexcept { return CHI::decay<lpid_t>(_LPID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  pgroupid_t&         PGroupID        () const    noexcept { return CHI::decay<pgroupid_t>(_PGroupID); }
            inline constexpr const  stashgroupid_t&     StashGroupID    () const    noexcept { return CHI::decay<stashgroupid_t>(_StashGroupID); }
            inline constexpr const  taggroupid_t&       TagGroupID      () const    noexcept { return CHI::decay<taggroupid_t>(_TagGroupID); }
#endif
            inline constexpr const  excl_t&             Excl            () const    noexcept { return CHI::decay<excl_t>(_Excl); }
            inline constexpr const  snoopme_t&          SnoopMe         () const    noexcept { return CHI::decay<snoopme_t>(_SnoopMe); }
            inline constexpr const  expcompack_t&       ExpCompAck      () const    noexcept { return CHI::decay<expcompack_t>(_ExpCompAck); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  tagop_t&            TagOp           () const    noexcept { return CHI::decay<tagop_t>(_TagOp); }
#endif
            inline constexpr const  tracetag_t&         TraceTag        () const    noexcept { return CHI::decay<tracetag_t>(_TraceTag); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  mpam_t&             MPAM            () const    noexcept { return CHI::decay<mpam_t>(_MPAM); }
#endif
            inline constexpr const  rsvdc_t&            RSVDC           () const    noexcept { return CHI::decay<rsvdc_t>(_RSVDC); }
        };

        //
        template<class T>
        concept REQFlitFormatConcept = requires {

            // QoS
            typename T::qos_t;
            { T::QOS_WIDTH              } -> std::convertible_to<size_t>;
            { T::QOS_LSB                } -> std::convertible_to<size_t>;
            { T::QOS_MSB                } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TGTID_LSB              } -> std::convertible_to<size_t>;
            { T::TGTID_MSB              } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH            } -> std::convertible_to<size_t>;
            { T::SRCID_LSB              } -> std::convertible_to<size_t>;
            { T::SRCID_MSB              } -> std::convertible_to<size_t>;

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TXNID_LSB              } -> std::convertible_to<size_t>;
            { T::TXNID_MSB              } -> std::convertible_to<size_t>;

            // ReturnNID
            typename T::returnnid_t;
            { T::RETURNNID_WIDTH        } -> std::convertible_to<size_t>;
            { T::RETURNNID_LSB          } -> std::convertible_to<size_t>;
            { T::RETURNNID_MSB          } -> std::convertible_to<size_t>;

            // StashNID
            typename T::stashnid_t;
            { T::STASHNID_WIDTH         } -> std::convertible_to<size_t>;
            { T::STASHNID_LSB           } -> std::convertible_to<size_t>;
            { T::STASHNID_MSB           } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // SLCRepHint
            typename T::slcrephint_t;
            { T::SLCREPHINT_WIDTH       } -> std::convertible_to<size_t>;
            { T::SLCREPHINT_LSB         } -> std::convertible_to<size_t>;
            { T::SLCREPHINT_MSB         } -> std::convertible_to<size_t>;
#endif

            // StashNIDValid
            typename T::stashnidvalid_t;
            { T::STASHNIDVALID_WIDTH    } -> std::convertible_to<size_t>;
            { T::STASHNIDVALID_LSB      } -> std::convertible_to<size_t>;
            { T::STASHNIDVALID_MSB      } -> std::convertible_to<size_t>;

            // Endian
            typename T::endian_t;
            { T::ENDIAN_WIDTH           } -> std::convertible_to<size_t>;
            { T::ENDIAN_LSB             } -> std::convertible_to<size_t>;
            { T::ENDIAN_MSB             } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // Deep
            typename T::deep_t;
            { T::DEEP_WIDTH             } -> std::convertible_to<size_t>;
            { T::DEEP_LSB               } -> std::convertible_to<size_t>;
            { T::DEEP_MSB               } -> std::convertible_to<size_t>;
#endif

            // ReturnTxnID
            typename T::returntxnid_t;
            { T::RETURNTXNID_WIDTH      } -> std::convertible_to<size_t>;
            { T::RETURNTXNID_LSB        } -> std::convertible_to<size_t>;
            { T::RETURNTXNID_MSB        } -> std::convertible_to<size_t>;

            // StashLPID
            typename T::stashlpid_t;
            { T::STASHLPID_WIDTH        } -> std::convertible_to<size_t>;
            { T::STASHLPID_LSB          } -> std::convertible_to<size_t>;
            { T::STASHLPID_MSB          } -> std::convertible_to<size_t>;

            // StashLPIDValid
            typename T::stashlpidvalid_t;
            { T::STASHLPIDVALID_WIDTH   } -> std::convertible_to<size_t>;
            { T::STASHLPIDVALID_LSB     } -> std::convertible_to<size_t>;
            { T::STASHLPIDVALID_MSB     } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
            { T::OPCODE_LSB             } -> std::convertible_to<size_t>;
            { T::OPCODE_MSB             } -> std::convertible_to<size_t>;

            // Size
            typename T::ssize_t;
            { T::SSIZE_WIDTH            } -> std::convertible_to<size_t>;
            { T::SSIZE_LSB              } -> std::convertible_to<size_t>;
            { T::SSIZE_MSB              } -> std::convertible_to<size_t>;

            // Addr
            typename T::addr_t;
            { T::ADDR_WIDTH             } -> std::convertible_to<size_t>;
            { T::ADDR_LSB               } -> std::convertible_to<size_t>;
            { T::ADDR_MSB               } -> std::convertible_to<size_t>;

            // NS
            typename T::ns_t;
            { T::NS_WIDTH               } -> std::convertible_to<size_t>;
            { T::NS_LSB                 } -> std::convertible_to<size_t>;
            { T::NS_MSB                 } -> std::convertible_to<size_t>;

            // LikelyShared
            typename T::likelyshared_t;
            { T::LIKELYSHARED_WIDTH     } -> std::convertible_to<size_t>;
            { T::LIKELYSHARED_LSB       } -> std::convertible_to<size_t>;
            { T::LIKELYSHARED_MSB       } -> std::convertible_to<size_t>;

            // AllowRetry
            typename T::allowretry_t;
            { T::ALLOWRETRY_WIDTH       } -> std::convertible_to<size_t>;
            { T::ALLOWRETRY_LSB         } -> std::convertible_to<size_t>;
            { T::ALLOWRETRY_MSB         } -> std::convertible_to<size_t>;

            // Order
            typename T::order_t;
            { T::ORDER_WIDTH            } -> std::convertible_to<size_t>;
            { T::ORDER_LSB              } -> std::convertible_to<size_t>;
            { T::ORDER_MSB              } -> std::convertible_to<size_t>;

            // PCrdType
            typename T::pcrdtype_t;
            { T::PCRDTYPE_WIDTH         } -> std::convertible_to<size_t>;
            { T::PCRDTYPE_LSB           } -> std::convertible_to<size_t>;
            { T::PCRDTYPE_MSB           } -> std::convertible_to<size_t>;

            // MemAttr
            typename T::memattr_t;
            { T::MEMATTR_WIDTH          } -> std::convertible_to<size_t>;
            { T::MEMATTR_LSB            } -> std::convertible_to<size_t>;
            { T::MEMATTR_MSB            } -> std::convertible_to<size_t>;

            // SnpAttr
            typename T::snpattr_t;
            { T::SNPATTR_WIDTH          } -> std::convertible_to<size_t>;
            { T::SNPATTR_LSB            } -> std::convertible_to<size_t>;
            { T::SNPATTR_MSB            } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // DoDWT
            typename T::dodwt_t;
            { T::DODWT_WIDTH            } -> std::convertible_to<size_t>;
            { T::DODWT_LSB              } -> std::convertible_to<size_t>;
            { T::DODWT_MSB              } -> std::convertible_to<size_t>;
#endif

            // LPID
            typename T::lpid_t;
            { T::LPID_WIDTH             } -> std::convertible_to<size_t>;
            { T::LPID_LSB               } -> std::convertible_to<size_t>;
            { T::LPID_MSB               } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // PGroupID
            typename T::pgroupid_t;
            { T::PGROUPID_WIDTH         } -> std::convertible_to<size_t>;
            { T::PGROUPID_LSB           } -> std::convertible_to<size_t>;
            { T::PGROUPID_MSB           } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // StashGroupID
            typename T::stashgroupid_t;
            { T::STASHGROUPID_WIDTH     } -> std::convertible_to<size_t>;
            { T::STASHGROUPID_LSB       } -> std::convertible_to<size_t>;
            { T::STASHGROUPID_MSB       } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // TagGroupID
            typename T::taggroupid_t;
            { T::TAGGROUPID_WIDTH       } -> std::convertible_to<size_t>;
            { T::TAGGROUPID_LSB         } -> std::convertible_to<size_t>;
            { T::TAGGROUPID_MSB         } -> std::convertible_to<size_t>;
#endif

            // Excl
            typename T::excl_t;
            { T::EXCL_WIDTH             } -> std::convertible_to<size_t>;
            { T::EXCL_LSB               } -> std::convertible_to<size_t>;
            { T::EXCL_MSB               } -> std::convertible_to<size_t>;

            // SnoopMe
            typename T::snoppme_t;
            { T::SNOOPME_WIDTH          } -> std::convertible_to<size_t>;
            { T::SNOOPME_LSB            } -> std::convertible_to<size_t>;
            { T::SNOOPME_MSB            } -> std::convertible_to<size_t>;

            // ExpCompAck
            typename T::expcompack_t;
            { T::EXPCOMPACK_WIDTH       } -> std::convertible_to<size_t>;
            { T::EXPCOMPACK_LSB         } -> std::convertible_to<size_t>;
            { T::EXPCOMPACK_MSB         } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            typename T::tagop_t;
            { T::TAGOP_WIDTH            } -> std::convertible_to<size_t>;
            { T::TAGOP_LSB              } -> std::convertible_to<size_t>;
            { T::TAGOP_MSB              } -> std::convertible_to<size_t>;
#endif

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH         } -> std::convertible_to<size_t>;
            { T::TRACETAG_LSB           } -> std::convertible_to<size_t>;
            { T::TRACETAG_MSB           } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // MPAM
            typename T::mpam_t;
            { T::MPAM_WIDTH             } -> std::convertible_to<size_t>;
            { T::MPAM_LSB               } -> std::convertible_to<size_t>;
            { T::MPAM_MSB               } -> std::convertible_to<size_t>;
            { T::hasMPAM                } -> std::convertible_to<bool>;
#endif

            // RSVDC
            typename T::rsvdc_t;
            { T::RSVDC_WIDTH            } -> std::convertible_to<size_t>;
            { T::RSVDC_LSB              } -> std::convertible_to<size_t>;
            { T::RSVDC_MSB              } -> std::convertible_to<size_t>;
            { T::hasRSVDC               } -> std::convertible_to<bool>;

            //
            { T::WIDTH                  } -> std::convertible_to<size_t>;
            { T::LSB                    } -> std::convertible_to<size_t>;
            { T::MSB                    } -> std::convertible_to<size_t>;
        };



        //
        template<DATFlitConfigurationConcept    config      = FlitConfiguration<>,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class DAT {
        public:
            /*
            QoS: 4 bits
            Quality of Service priority. Specifies 1 of 16 possible priority levels for the 
            transaction with ascending values of QoS indicating higher priority levels.
            */
            static constexpr size_t     QOS_WIDTH = 4;
            static constexpr size_t     QOS_LSB   = 0;
            static constexpr size_t     QOS_MSB   = 3;
            static constexpr FlitRange  QOS_RANGE = { QOS_LSB, QOS_MSB };

            using qos_t = uint_fit_t<QOS_WIDTH>;

            /*
            TgtID: <NodeIDWidth> bits
            Target ID. The node ID of the port on the component to which the packet is 
            targeted.
            */
            static constexpr size_t     TGTID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     TGTID_LSB   = QOS_MSB + 1;
            static constexpr size_t     TGTID_MSB   = QOS_MSB + TGTID_WIDTH;
            static constexpr FlitRange  TGTID_RANGE = { TGTID_LSB, TGTID_MSB };

            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            SrcID: <NodeIDWidth> bits
            Source ID. The node ID of the port on the component from which the packet 
            was sent.
            */
            static constexpr size_t     SRCID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     SRCID_LSB   = TGTID_MSB + 1;
            static constexpr size_t     SRCID_MSB   = TGTID_MSB + SRCID_WIDTH;
            static constexpr FlitRange  SRCID_RANGE = { SRCID_LSB, SRCID_MSB };

            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TxnID: 
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TXNID_WIDTH = 12;
#endif
            static constexpr size_t     TXNID_LSB   = SRCID_MSB + 1;
            static constexpr size_t     TXNID_MSB   = SRCID_MSB + TXNID_WIDTH;
            static constexpr FlitRange  TXNID_RANGE = { SRCID_LSB, SRCID_MSB };

            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            HomeNID: <NodeIDWidth> bits
            Home Node ID. The Node ID of the target of the CompAck response to be 
            sent from the Requester.
            */
            static constexpr size_t     HOMENID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     HOMENID_LSB   = TXNID_MSB + 1;
            static constexpr size_t     HOMENID_MSB   = TXNID_MSB + HOMENID_WIDTH;
            static constexpr FlitRange  HOMENID_RANGE = { HOMENID_LSB, HOMENID_MSB };

            using homenid_t = uint_fit_t<HOMENID_WIDTH>;

            /*
            Opcode:
                * 3 bits for CHI Issue B
                * 4 bits for CHI Issue E.b
            Data opcode. Indicates, for example, if the data packet is related to a Read 
            transaction, a Write transaction, or a Snoop transaction. 
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 3;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 4;
#endif
            static constexpr size_t     OPCODE_LSB   = HOMENID_MSB + 1;
            static constexpr size_t     OPCODE_MSB   = HOMENID_MSB + OPCODE_WIDTH;
            static constexpr FlitRange  OPCODE_RANGE = { OPCODE_LSB, OPCODE_MSB };

            using opcode_t = uint_fit_t<OPCODE_WIDTH>;

            /*
            RespErr: 2 bits
            Response Error status. Indicates the error status associated with a data 
            transfer.
            */
            static constexpr size_t     RESPERR_WIDTH = 2;
            static constexpr size_t     RESPERR_LSB   = OPCODE_MSB + 1;
            static constexpr size_t     RESPERR_MSB   = OPCODE_MSB + RESPERR_WIDTH;
            static constexpr FlitRange  RESPERR_RANGE = { RESPERR_LSB, RESPERR_MSB };

            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response status. Indicates the cache line state associated with a data transfer. 
            */
            static constexpr size_t     RESP_WIDTH = 3;
            static constexpr size_t     RESP_LSB   = RESPERR_MSB + 1;
            static constexpr size_t     RESP_MSB   = RESPERR_MSB + RESP_WIDTH;
            static constexpr FlitRange  RESP_RANGE = { RESP_LSB, RESP_MSB };

            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            FwdState: 3 bits
            Forward State. Indicates the cache line state associated with a data transfer 
            to the Requester from the receiver of the snoop.
            */
            static constexpr size_t     FWDSTATE_WIDTH = 3;
            static constexpr size_t     FWDSTATE_LSB   = RESP_MSB + 1;
            static constexpr size_t     FWDSTATE_MSB   = RESP_MSB + FWDSTATE_WIDTH;
            static constexpr FlitRange  FWDSTATE_RANGE = { FWDSTATE_LSB, FWDSTATE_MSB };

#ifdef CHI_ISSUE_B_ENABLE
            using fwdstate_t = uint_strict_t<FWDSTATE_WIDTH>;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            using fwdstate_t = uintfitn_strict_t<4 /*DATASOURCE_WIDTH*/, FWDSTATE_WIDTH>;
#endif

            /*
            DataPull: 3 bits
            Data Pull. Indicates the inclusion of an implied Read request in the Data 
            response.
            */
            static constexpr size_t     DATAPULL_WIDTH = 3;
            static constexpr size_t     DATAPULL_LSB   = RESP_MSB + 1;
            static constexpr size_t     DATAPULL_MSB   = RESP_MSB + DATAPULL_WIDTH;
            static constexpr FlitRange  DATAPULL_RANGE = { DATAPULL_LSB, DATAPULL_MSB };
            
#ifdef CHI_ISSUE_B_ENABLE
            using datapull_t = uint_strict_t<DATAPULL_WIDTH>;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            using datapull_t = uint_strict_t<4 /*DATASOURCE_WIDTH*/, DATAPULL_WIDTH>;
#endif

            /*
            DataSource:
                * 3 bits for CHI Issue B
                * 4 bits for CHI Issue E.b
            Data Source. The value indicates the source of the data in a read Data 
            response.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     DATASOURCE_WIDTH = 3;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     DATASOURCE_WIDTH = 4;
#endif
            static constexpr size_t     DATASOURCE_LSB   = RESP_MSB + 1;
            static constexpr size_t     DATASOURCE_MSB   = RESP_MSB + DATASOURCE_WIDTH;
            static constexpr FlitRange  DATASOURCE_RANGE = { DATASOURCE_LSB, DATASOURCE_MSB };

            using datasource_t = uint_strict_t<DATASOURCE_WIDTH>;

            /*
            CBusy:
                * N/A    for CHI Issue B
                * 3 bits for CHI Issue E.b
            Completer Busy. Indicates the current level of activity at the Completer.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     CBUSY_WIDTH = 3;
            static constexpr size_t     CBUSY_LSB   = DATASOURCE_MSB + 1;
            static constexpr size_t     CBUSY_MSB   = DATASOURCE_MSB + CBUSY_WIDTH;
            static constexpr FlitRange  CBUSY_RANGE = { CBUSY_LSB, CBUSY_MSB };

            using cbusy_t = uint_fit_t<CBUSY_WIDTH>;
#endif

            /*
            DBID:
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Data Buffer ID. The ID provided to be used as the TxnID in the response to 
            this message.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     DBID_WIDTH = 8;
            static constexpr size_t     DBID_LSB   = DATASOURCE_MSB + 1;
            static constexpr size_t     DBID_MSB   = DATASOURCE_MSB + DBID_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     DBID_WIDTH = 12;
            static constexpr size_t     DBID_LSB   = CBUSY_MSB + 1;
            static constexpr size_t     DBID_MSB   = CBUSY_MSB + DBID_WIDTH;
#endif
            static constexpr FlitRange  DBID_RANGE = { DBID_LSB, DBID_MSB };

            using dbid_t = uint_fit_t<DBID_WIDTH>;

            /*
            CCID: 2 bits
            Critical Chunk Identifier. Replicates the address offset of the original 
            transaction request.
            */
            static constexpr size_t     CCID_WIDTH = 2;
            static constexpr size_t     CCID_LSB   = DBID_MSB + 1;
            static constexpr size_t     CCID_MSB   = DBID_MSB + CCID_WIDTH;
            static constexpr FlitRange  CCID_RANGE = { CCID_LSB, CCID_MSB };

            using ccid_t = uint_fit_t<CCID_WIDTH>;

            /*
            DataID: 2 bits
            Data Identifier. Provides the address offset of the data provided in the packet. 
            */
            static constexpr size_t     DATAID_WIDTH = 2;
            static constexpr size_t     DATAID_LSB   = CCID_MSB + 1;
            static constexpr size_t     DATAID_MSB   = CCID_MSB + DATAID_WIDTH;
            static constexpr FlitRange  DATAID_RANGE = { DATAID_LSB, DATAID_MSB };

            using dataid_t = uint_fit_t<DATAID_WIDTH>;

            /*
            TagOp:
                * N/A    for CHI Issue B
                * 2 bits for CHI Issue E.b
            Tag Operation.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAGOP_WIDTH = 2;
            static constexpr size_t     TAGOP_LSB   = DATAID_MSB + 1;
            static constexpr size_t     TAGOP_MSB   = DATAID_MSB + TAGOP_WIDTH;
            static constexpr FlitRange  TAGOP_RANGE = { TAGOP_LSB, TAGOP_MSB };

            using tagop_t = uint_fit_t<TAGOP_WIDTH>;
#endif

            /*
            Tag:
                * N/A             for CHI Issue B
                * <tagWidth> bits for CHI Issue E.b
            Memory Tag. Provides sets of 4-bit tags, each associated with an aligned 
            16-byte of data. 
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAG_WIDTH = config::tagWidth;
            static constexpr size_t     TAG_LSB   = TAGOP_MSB + 1;
            static constexpr size_t     TAG_MSB   = TAGOP_MSB + TAG_WIDTH;
            static constexpr FlitRange  TAG_RANGE = { TAG_LSB, TAG_MSB };

            using tag_t = uint_fit_t<TAG_WIDTH>;
#endif

            /*
            TU:
                * N/A                   for CHI Issue B
                * <tagUpdateWidth> bits for CHI Issue E.b
            Tag Update. Indicates which of the Allocation Tags must be updated.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TU_WIDTH = config::tagUpdateWidth;
            static constexpr size_t     TU_LSB   = TAG_MSB + 1;
            static constexpr size_t     TU_MSB   = TAG_MSB + TU_WIDTH;
            static constexpr FlitRange  TU_RANGE = { TU_LSB, TU_MSB };

            using tu_t = uint_fit_t<TU_WIDTH>;
#endif

            /*
            TraceTag: 1 bit
            Trace Tag. Provides additional support for the debugging, tracing, and 
            performance measurement of systems.
            */
            static constexpr size_t     TRACETAG_WIDTH = 1;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TRACETAG_LSB   = DATAID_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = DATAID_MSB + TRACETAG_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TRACETAG_LSB   = TU_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = TU_MSB + TRACETAG_WIDTH;
#endif
            static constexpr FlitRange  TRACETAG_RANGE = { TRACETAG_LSB, TRACETAG_MSB };

            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

            /*
            RSVDC: <datRsvdcWidth> bits
            User-defined.
            */
            static constexpr size_t     RSVDC_WIDTH = config::datRsvdcWidth;
            static constexpr size_t     RSVDC_LSB   = TRACETAG_MSB + 1;
            static constexpr size_t     RSVDC_MSB   = TRACETAG_MSB + RSVDC_WIDTH;
            static constexpr FlitRange  RSVDC_RANGE = { RSVDC_LSB, RSVDC_MSB };

            static constexpr bool   hasRSVDC = config::datRsvdcWidth > 0;
            // *NOTICE: Use 'if constexpr (hasRSVDC)' to check if 'RSVDC' is available.
            using rsvdc_t = uint_fit_t<RSVDC_WIDTH, std::monostate>;

            /*
            BE: <byteEnableWidth> bits
            Byte Enable. For a data write, or data provided in response to a snoop, 
            indicates which bytes are valid.
            */
            static constexpr size_t     BE_WIDTH = config::byteEnableWidth;
            static constexpr size_t     BE_LSB   = RSVDC_MSB + 1;
            static constexpr size_t     BE_MSB   = RSVDC_MSB + BE_WIDTH;
            static constexpr FlitRange  BE_RANGE = { BE_LSB, BE_MSB };

            using be_t = uint_fit_t<BE_WIDTH>;

            /*
            Data: <dataWidth> bits
            Data payload.
            */
            static constexpr size_t     DATA_WIDTH = config::dataWidth;
            static constexpr size_t     DATA_LSB   = BE_MSB + 1;
            static constexpr size_t     DATA_MSB   = BE_MSB + DATA_WIDTH;
            static constexpr FlitRange  DATA_RANGE = { DATA_LSB, DATA_MSB };

            using data_t = uint64_t[config::dataWidth / 64];

            /*
            DataCheck: <dataCheckWidth> bits
            Data Check. Detects data errors in the DAT packet.
            */
            static constexpr size_t     DATACHECK_WIDTH = config::dataCheckWidth;
            static constexpr size_t     DATACHECK_LSB   = DATA_MSB + 1;
            static constexpr size_t     DATACHECK_MSB   = DATA_MSB + DATACHECK_WIDTH;
            static constexpr FlitRange  DATACHECK_RANGE = { DATACHECK_LSB, DATACHECK_MSB };

            static constexpr bool   hasDataCheck = config::dataCheckWidth > 0;
            // *NOTICE: Use 'if constexpr (hasDataCheck)' to check if 'DataCheck' is available.
            using datacheck_t = uint_fit_t<DATACHECK_WIDTH, std::monostate>;

            /*
            Poison: <poisonWidth> bits
            Poison. Indicates that a set of data bytes has previously been corrupted.
            */
            static constexpr size_t     POISON_WIDTH = config::poisonWidth;
            static constexpr size_t     POISON_LSB   = DATACHECK_MSB + 1;
            static constexpr size_t     POISON_MSB   = DATACHECK_MSB + POISON_WIDTH;
            static constexpr FlitRange  POISON_RANGE = { POISON_LSB, POISON_MSB };

            static constexpr bool   hasPoison = config::poisonWidth > 0;
            // *NOTICE: Use 'if constexpr (hasPoison)' to check if 'Poison' is available.
            using poison_t = uint_fit_t<POISON_WIDTH, std::monostate>;

        public:
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + HOMENID_WIDTH   + OPCODE_WIDTH      + RESPERR_WIDTH     + RESP_WIDTH
                                              + DATASOURCE_WIDTH                    + DBID_WIDTH        + CCID_WIDTH
                                              + DATAID_WIDTH    + TRACETAG_WIDTH    + RSVDC_WIDTH       + BE_WIDTH
                                              + DATA_WIDTH      + DATACHECK_WIDTH   + POISON_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + HOMENID_WIDTH   + OPCODE_WIDTH      + RESPERR_WIDTH     + RESP_WIDTH
                                              + DATASOURCE_WIDTH                    + CBUSY_WIDTH       + DBID_WIDTH
                                              + CCID_WIDTH      + DATAID_WIDTH      + TAGOP_WIDTH       + TAG_WIDTH
                                              + TU_WIDTH        + TRACETAG_WIDTH    + RSVDC_WIDTH       + BE_WIDTH
                                              + DATA_WIDTH      + DATACHECK_WIDTH   + POISON_WIDTH;
#endif
            static constexpr size_t     LSB   = 0;
            static constexpr size_t     MSB   = WIDTH - 1;
            static constexpr FlitRange  RANGE = { LSB, MSB };

        // Data flit fields
        // *NOTICE: Some fields are overlapped in Flit Implementation.
        public:
            as_pointer_if_t<conn::connectedIO, qos_t>               _QoS;
            as_pointer_if_t<conn::connectedIO, tgtid_t>             _TgtID;
            as_pointer_if_t<conn::connectedIO, srcid_t>             _SrcID;
            as_pointer_if_t<conn::connectedIO, txnid_t>             _TxnID;
            as_pointer_if_t<conn::connectedIO, homenid_t>           _HomeNID;
            as_pointer_if_t<conn::connectedIO, opcode_t>            _Opcode;
            as_pointer_if_t<conn::connectedIO, resperr_t>           _RespErr;
            as_pointer_if_t<conn::connectedIO, resp_t>              _Resp;
            union {
            as_pointer_if_t<conn::connectedIO, fwdstate_t>          _FwdState;
            as_pointer_if_t<conn::connectedIO, datapull_t>          _DataPull;
            as_pointer_if_t<conn::connectedIO, datasource_t>        _DataSource;
            };
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, cbusy_t>             _CBusy;
#endif
            as_pointer_if_t<conn::connectedIO, dbid_t>              _DBID;
            as_pointer_if_t<conn::connectedIO, ccid_t>              _CCID;
            as_pointer_if_t<conn::connectedIO, dataid_t>            _DataID;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, tagop_t>             _TagOp;
            as_pointer_if_t<conn::connectedIO, tag_t>               _Tag;
            as_pointer_if_t<conn::connectedIO, tu_t>                _TU;
#endif
            as_pointer_if_t<conn::connectedIO, tracetag_t>          _TraceTag;
            as_pointer_if_t<conn::connectedIO, rsvdc_t>             _RSVDC;
            as_pointer_if_t<conn::connectedIO, be_t>                _BE;
            as_pointer_if_t<conn::connectedIO, data_t>              _Data;
            as_pointer_if_t<conn::connectedIO, datacheck_t>         _DataCheck;
            as_pointer_if_t<conn::connectedIO, poison_t>            _Poison;

        // field decay helper functions
        public:
            inline constexpr        qos_t&              QoS             ()          noexcept    { return CHI::decay<qos_t>(_QoS); }
            inline constexpr        tgtid_t&            TgtID           ()          noexcept    { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr        srcid_t&            SrcID           ()          noexcept    { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr        txnid_t&            TxnID           ()          noexcept    { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr        homenid_t&          HomeNID         ()          noexcept    { return CHI::decay<homenid_t>(_HomeNID); }
            inline constexpr        opcode_t&           Opcode          ()          noexcept    { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr        resperr_t&          RespErr         ()          noexcept    { return CHI::decay<resperr_t>(_RespErr); }
            inline constexpr        resp_t&             Resp            ()          noexcept    { return CHI::decay<resp_t>(_Resp); }
            inline constexpr        fwdstate_t&         FwdState        ()          noexcept    { return CHI::decay<fwdstate_t>(_FwdState); }
            inline constexpr        datapull_t&         DataPull        ()          noexcept    { return CHI::decay<datapull_t>(_DataPull); }
            inline constexpr        datasource_t&       DataSource      ()          noexcept    { return CHI::decay<datasource_t>(_DataSource); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        cbusy_t&            CBusy           ()          noexcept    { return CHI::decay<cbusy_t>(_CBusy); }
#endif
            inline constexpr        dbid_t&             DBID            ()          noexcept    { return CHI::decay<dbid_t>(_DBID); }
            inline constexpr        ccid_t&             CCID            ()          noexcept    { return CHI::decay<ccid_t>(_CCID); }
            inline constexpr        dataid_t&           DataID          ()          noexcept    { return CHI::decay<dataid_t>(_DataID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        tagop_t&            TagOp           ()          noexcept    { return CHI::decay<tagop_t>(_TagOp); }
            inline constexpr        tag_t&              Tag             ()          noexcept    { return CHI::decay<tag_t>(_Tag); }
            inline constexpr        tu_t&               TU              ()          noexcept    { return CHI::decay<tu_t>(_TU); }
#endif
            inline constexpr        tracetag_t&         TraceTag        ()          noexcept    { return CHI::decay<tracetag_t>(_TraceTag); }
            inline constexpr        rsvdc_t&            RSVDC           ()          noexcept    { return CHI::decay<rsvdc_t>(_RSVDC); }
            inline constexpr        be_t&               BE              ()          noexcept    { return CHI::decay<be_t>(_BE); }
            inline constexpr        data_t&             Data            ()          noexcept    { return CHI::decay<data_t>(_Data); }
            inline constexpr        datacheck_t&        DataCheck       ()          noexcept    { return CHI::decay<datacheck_t>(_DataCheck); }
            inline constexpr        poison_t&           Poison          ()          noexcept    { return CHI::decay<poison_t>(_Poison); }

        // field decay helper functions
        public:
            inline constexpr const  qos_t&              QoS             () const    noexcept    { return CHI::decay<qos_t>(_QoS); }
            inline constexpr const  tgtid_t&            TgtID           () const    noexcept    { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr const  srcid_t&            SrcID           () const    noexcept    { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr const  txnid_t&            TxnID           () const    noexcept    { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr const  homenid_t&          HomeNID         () const    noexcept    { return CHI::decay<homenid_t>(_HomeNID); }
            inline constexpr const  opcode_t&           Opcode          () const    noexcept    { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr const  resperr_t&          RespErr         () const    noexcept    { return CHI::decay<resperr_t>(_RespErr); }
            inline constexpr const  resp_t&             Resp            () const    noexcept    { return CHI::decay<resp_t>(_Resp); }
            inline constexpr const  fwdstate_t&         FwdState        () const    noexcept    { return CHI::decay<fwdstate_t>(_FwdState); }
            inline constexpr const  datapull_t&         DataPull        () const    noexcept    { return CHI::decay<datapull_t>(_DataPull); }
            inline constexpr const  datasource_t&       DataSource      () const    noexcept    { return CHI::decay<datasource_t>(_DataSource); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  cbusy_t&            CBusy           () const    noexcept    { return CHI::decay<cbusy_t>(_CBusy); }
#endif
            inline constexpr const  dbid_t&             DBID            () const    noexcept    { return CHI::decay<dbid_t>(_DBID); }
            inline constexpr const  ccid_t&             CCID            () const    noexcept    { return CHI::decay<ccid_t>(_CCID); }
            inline constexpr const  dataid_t&           DataID          () const    noexcept    { return CHI::decay<dataid_t>(_DataID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  tagop_t&            TagOp           () const    noexcept    { return CHI::decay<tagop_t>(_TagOp); }
            inline constexpr const  tag_t&              Tag             () const    noexcept    { return CHI::decay<tag_t>(_Tag); }
            inline constexpr const  tu_t&               TU              () const    noexcept    { return CHI::decay<tu_t>(_TU); }
#endif
            inline constexpr const  tracetag_t&         TraceTag        () const    noexcept    { return CHI::decay<tracetag_t>(_TraceTag); }
            inline constexpr const  rsvdc_t&            RSVDC           () const    noexcept    { return CHI::decay<rsvdc_t>(_RSVDC); }
            inline constexpr const  be_t&               BE              () const    noexcept    { return CHI::decay<be_t>(_BE); }
            inline constexpr const  data_t&             Data            () const    noexcept    { return CHI::decay<data_t>(_Data); }
            inline constexpr const  datacheck_t&        DataCheck       () const    noexcept    { return CHI::decay<datacheck_t>(_DataCheck); }
            inline constexpr const  poison_t&           Poison          () const    noexcept    { return CHI::decay<poison_t>(_Poison); }
        };

        //
        template<class T>
        concept DATFlitFormatConcept = requires {

            // QoS
            typename T::qos_t;
            { T::QOS_WIDTH              } -> std::convertible_to<size_t>;
            { T::QOS_LSB                } -> std::convertible_to<size_t>;
            { T::QOS_MSB                } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TGTID_LSB              } -> std::convertible_to<size_t>;
            { T::TGTID_MSB              } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH            } -> std::convertible_to<size_t>;
            { T::SRCID_LSB              } -> std::convertible_to<size_t>;
            { T::SRCID_MSB              } -> std::convertible_to<size_t>;

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TXNID_LSB              } -> std::convertible_to<size_t>;
            { T::TXNID_MSB              } -> std::convertible_to<size_t>;

            // HomeNID
            typename T::homenid_t;
            { T::HOMENID_WIDTH          } -> std::convertible_to<size_t>;
            { T::HOMENID_LSB            } -> std::convertible_to<size_t>;
            { T::HOMENID_MSB            } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
            { T::OPCODE_LSB             } -> std::convertible_to<size_t>;
            { T::OPCODE_MSB             } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH          } -> std::convertible_to<size_t>;
            { T::RESPERR_LSB            } -> std::convertible_to<size_t>;
            { T::RESPERR_MSB            } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH             } -> std::convertible_to<size_t>;
            { T::RESP_LSB               } -> std::convertible_to<size_t>;
            { T::RESP_MSB               } -> std::convertible_to<size_t>;

            // FwdState
            typename T::fwdstate_t;
            { T::FWDSTATE_WIDTH         } -> std::convertible_to<size_t>;
            { T::FWDSTATE_LSB           } -> std::convertible_to<size_t>;
            { T::FWDSTATE_MSB           } -> std::convertible_to<size_t>;

            // DataPull
            typename T::datapull_t;
            { T::DATAPULL_WIDTH         } -> std::convertible_to<size_t>;
            { T::DATAPULL_LSB           } -> std::convertible_to<size_t>;
            { T::DATAPULL_MSB           } -> std::convertible_to<size_t>;

            // DataSource
            typename T::datasource_t;
            { T::DATASOURCE_WIDTH       } -> std::convertible_to<size_t>;
            { T::DATASOURCE_LSB         } -> std::convertible_to<size_t>;
            { T::DATASOURCE_MSB         } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // CBusy
            typename T::cbusy_t;
            { T::CBUSY_WIDTH            } -> std::convertible_to<size_t>;
            { T::CBUSY_LSB              } -> std::convertible_to<size_t>;
            { T::CBUSY_MSB              } -> std::convertible_to<size_t>;
#endif

            // DBID
            typename T::dbid_t;
            { T::DBID_WIDTH             } -> std::convertible_to<size_t>;
            { T::DBID_LSB               } -> std::convertible_to<size_t>;
            { T::DBID_MSB               } -> std::convertible_to<size_t>;

            // CCID
            typename T::ccid_t;
            { T::CCID_WIDTH             } -> std::convertible_to<size_t>;
            { T::CCID_LSB               } -> std::convertible_to<size_t>;
            { T::CCID_MSB               } -> std::convertible_to<size_t>;

            // DataID
            typename T::dataid_t;
            { T::DATAID_WIDTH           } -> std::convertible_to<size_t>;
            { T::DATAID_LSB             } -> std::convertible_to<size_t>;
            { T::DATAID_MSB             } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            typename T::tagop_t;
            { T::TAGOP_WIDTH            } -> std::convertible_to<size_t>;
            { T::TAGOP_LSB              } -> std::convertible_to<size_t>;
            { T::TAGOP_MSB              } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // Tag
            typename T::tag_t;
            { T::TAG_WIDTH              } -> std::convertible_to<size_t>;
            { T::TAG_LSB                } -> std::convertible_to<size_t>;
            { T::TAG_MSB                } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // TU
            typename T::tu_t;
            { T::TU_WIDTH               } -> std::convertible_to<size_t>;
            { T::TU_LSB                 } -> std::convertible_to<size_t>;
            { T::TU_MSB                 } -> std::convertible_to<size_t>;
#endif

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH         } -> std::convertible_to<size_t>;
            { T::TRACETAG_LSB           } -> std::convertible_to<size_t>;
            { T::TRACETAG_MSB           } -> std::convertible_to<size_t>;

            // RSVDC
            typename T::rsvdc_t;
            { T::RSVDC_WIDTH            } -> std::convertible_to<size_t>;
            { T::RSVDC_LSB              } -> std::convertible_to<size_t>;
            { T::RSVDC_MSB              } -> std::convertible_to<size_t>;
            { T::hasRSVDC               } -> std::convertible_to<bool>;

            // BE
            typename T::be_t;
            { T::BE_WIDTH               } -> std::convertible_to<size_t>;
            { T::BE_LSB                 } -> std::convertible_to<size_t>;
            { T::BE_MSB                 } -> std::convertible_to<size_t>;

            // Data
            typename T::data_t;
            { T::DATA_WIDTH             } -> std::convertible_to<size_t>;
            { T::DATA_LSB               } -> std::convertible_to<size_t>;
            { T::DATA_MSB               } -> std::convertible_to<size_t>;

            // DataCheck
            typename T::datacheck_t;
            { T::DATACHECK_WIDTH        } -> std::convertible_to<size_t>;
            { T::DATACHECK_LSB          } -> std::convertible_to<size_t>;
            { T::DATACHECK_MSB          } -> std::convertible_to<size_t>;
            { T::hasDataCheck           } -> std::convertible_to<bool>;

            // Poison
            typename T::poison_t;
            { T::POISON_WIDTH           } -> std::convertible_to<size_t>;
            { T::POISON_LSB             } -> std::convertible_to<size_t>;
            { T::POISON_MSB             } -> std::convertible_to<size_t>;
            { T::hasPoison              } -> std::convertible_to<bool>;

            //
            { T::WIDTH                  } -> std::convertible_to<size_t>;
            { T::LSB                    } -> std::convertible_to<size_t>;
            { T::MSB                    } -> std::convertible_to<size_t>;
        };



        //
        template<RSPFlitConfigurationConcept    config      = FlitConfiguration<>,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class RSP {
        public:
            /*
            QoS: 4 bits
            Quality of Service priority. Specifies 1 of 16 possible priority levels for the 
            transaction with ascending values of QoS indicating higher priority levels.
            */
            static constexpr size_t     QOS_WIDTH = 4;
            static constexpr size_t     QOS_LSB   = 0;
            static constexpr size_t     QOS_MSB   = 3;
            static constexpr FlitRange  QOS_RANGE = { QOS_LSB, QOS_MSB };

            using qos_t = uint_fit_t<QOS_WIDTH>;

            /*
            TgtID: <NodeIDWidth> bits
            Target ID. The node ID of the port on the component to which the packet is 
            targeted.
            */
            static constexpr size_t     TGTID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     TGTID_LSB   = QOS_MSB + 1;
            static constexpr size_t     TGTID_MSB   = QOS_MSB + TGTID_WIDTH;
            static constexpr FlitRange  TGTID_RANGE = { TGTID_LSB, TGTID_MSB };

            using tgtid_t = uint_fit_t<TGTID_WIDTH>;

            /*
            SrcID: <NodeIDWidth> bits
            Source ID. The node ID of the port on the component from which the packet 
            was sent.
            */
            static constexpr size_t     SRCID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     SRCID_LSB   = TGTID_MSB + 1;
            static constexpr size_t     SRCID_MSB   = TGTID_MSB + SRCID_WIDTH;
            static constexpr FlitRange  SRCID_RANGE = { SRCID_LSB, SRCID_MSB };

            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TxnID: 
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TXNID_WIDTH = 12;
#endif
            static constexpr size_t     TXNID_LSB   = SRCID_MSB + 1;
            static constexpr size_t     TXNID_MSB   = SRCID_MSB + TXNID_WIDTH;
            static constexpr FlitRange  TXNID_RANGE = { SRCID_LSB, SRCID_MSB };

            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            Opcode:
                * 4 bits for CHI Issue B
                * 5 bits for CHI Issue E.b
            Response opcode. Specifies the response type.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 4;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     OPCODE_WIDTH = 5;
#endif
            static constexpr size_t     OPCODE_LSB   = TXNID_MSB + 1;
            static constexpr size_t     OPCODE_MSB   = TXNID_MSB + OPCODE_WIDTH;
            static constexpr FlitRange  OPCODE_RANGE = { OPCODE_LSB, OPCODE_MSB };

            using opcode_t = uint_fit_t<OPCODE_WIDTH>;

            /*
            RespErr: 2 bits
            Response Error status. Indicates the error status associated with a data 
            transfer.
            */
            static constexpr size_t     RESPERR_WIDTH = 2;
            static constexpr size_t     RESPERR_LSB   = OPCODE_MSB + 1;
            static constexpr size_t     RESPERR_MSB   = OPCODE_MSB + RESPERR_WIDTH;
            static constexpr FlitRange  RESPERR_RANGE = { RESPERR_LSB, RESPERR_MSB };

            using resperr_t = uint_fit_t<RESPERR_WIDTH>;

            /*
            Resp: 3 bits
            Response status. Indicates the cache line state associated with a data transfer. 
            */
            static constexpr size_t     RESP_WIDTH = 3;
            static constexpr size_t     RESP_LSB   = RESPERR_MSB + 1;
            static constexpr size_t     RESP_MSB   = RESPERR_MSB + RESP_WIDTH;
            static constexpr FlitRange  RESP_RANGE = { RESP_LSB, RESP_MSB };

            using resp_t = uint_fit_t<RESP_WIDTH>;

            /*
            FwdState: 3 bits
            Forward State. Indicates the cache line state associated with a data transfer 
            to the Requester from the receiver of the snoop.
            */
            static constexpr size_t     FWDSTATE_WIDTH = 3;
            static constexpr size_t     FWDSTATE_LSB   = RESP_MSB + 1;
            static constexpr size_t     FWDSTATE_MSB   = RESP_MSB + FWDSTATE_WIDTH;
            static constexpr FlitRange  FWDSTATE_RANGE = { FWDSTATE_LSB, FWDSTATE_MSB };

            using fwdstate_t = uint_strict_t<FWDSTATE_WIDTH>;

            /*
            DataPull: 3 bits
            Data Pull. Indicates the inclusion of an implied Read request in the Data 
            response.
            */
            static constexpr size_t     DATAPULL_WIDTH = 3;
            static constexpr size_t     DATAPULL_LSB   = RESP_MSB + 1;
            static constexpr size_t     DATAPULL_MSB   = RESP_MSB + DATAPULL_WIDTH;
            static constexpr FlitRange  DATAPULL_RANGE = { DATAPULL_LSB, DATAPULL_MSB };

            using datapull_t = uint_strict_t<DATAPULL_WIDTH>;

            /*
            CBusy:
                * N/A    for CHI Issue B
                * 3 bits for CHI Issue E.b
            Completer Busy. Indicates the current level of activity at the Completer.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     CBUSY_WIDTH = 3;
            static constexpr size_t     CBUSY_LSB   = FWDSTATE_MSB + 1;
            static constexpr size_t     CBUSY_MSB   = FWDSTATE_MSB + CBUSY_WIDTH;
            static constexpr FlitRange  CBUSY_RANGE = { CBUSY_LSB, CBUSY_MSB };

            using cbusy_t = uint_fit_t<CBUSY_WIDTH>;
#endif

            /*
            DBID:
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Data Buffer ID. The ID provided to be used as the TxnID in the response to 
            this message.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     DBID_WIDTH = 8;
            static constexpr size_t     DBID_LSB   = FWDSTATE_MSB + 1;
            static constexpr size_t     DBID_MSB   = FWDSTATE_MSB + DBID_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     DBID_WIDTH = 12;
            static constexpr size_t     DBID_LSB   = CBUSY_MSB + 1;
            static constexpr size_t     DBID_MSB   = CBUSY_MSB + DBID_WIDTH;
#endif
            static constexpr FlitRange  DBID_RANGE = { DBID_LSB, DBID_MSB };

#ifdef CHI_ISSUE_B_ENABLE
            using dbid_t = uint_fit_t<DBID_WIDTH>;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            using dbid_t = uint_strict_t<DBID_WIDTH>;
#endif

            /*
            PGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            Persistence Group ID. Indicates the set of CleanSharedPersistSep transactions 
            to which the request applies.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     PGROUPID_WIDTH = 8;
            static constexpr size_t     PGROUPID_LSB   = CBUSY_MSB + 1;
            static constexpr size_t     PGROUPID_MSB   = CBUSY_MSB + PGROUPID_WIDTH;
            static constexpr FlitRange  PGROUPID_RANGE = { PGROUPID_LSB, PGROUPID_MSB };

            using pgroupid_t = uintfitn_strict_t<DBID_WIDTH, PGROUPID_WIDTH>;
#endif

            /*
            StashGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            Stash Group ID. Indicates the set of StashOnceSep transactions to which the 
            request applies.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     STASHGROUPID_WIDTH = 8;
            static constexpr size_t     STASHGROUPID_LSB   = CBUSY_MSB + 1;
            static constexpr size_t     STASHGROUPID_MSB   = CBUSY_MSB + STASHGROUPID_WIDTH;
            static constexpr FlitRange  STASHGROUPID_RANGE = { STASHGROUPID_LSB, STASHGROUPID_MSB };

            using stashgroupid_t = uintfitn_strict_t<DBID_WIDTH, STASHGROUPID_WIDTH>;
#endif

            /*
            TagGroupID:
                * N/A    for CHI Issue B
                * 8 bits for CHI Issue E.b
            TagGroupID. Precise contents are IMPLEMENTATION DEFINED. Typically 
            expected to contain Exception level, TTBR value, and CPU identifier.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAGGROUPID_WIDTH = 8;
            static constexpr size_t     TAGGROUPID_LSB   = CBUSY_MSB + 1;
            static constexpr size_t     TAGGROUPID_MSB   = CBUSY_MSB + TAGGROUPID_WIDTH;
            static constexpr FlitRange  TAGGROUPID_RANGE = { TAGGROUPID_LSB, TAGGROUPID_MSB };

            using taggroupid_t = uintfitn_strict_t<DBID_WIDTH, TAGGROUPID_WIDTH>;
#endif

            /*
            PCrdType: 4 bits
            Protocol Credit Type.
            */
            static constexpr size_t     PCRDTYPE_WIDTH = 4;
            static constexpr size_t     PCRDTYPE_LSB   = DBID_MSB + 1;
            static constexpr size_t     PCRDTYPE_MSB   = DBID_MSB + PCRDTYPE_WIDTH;
            static constexpr FlitRange  PCRDTYPE_RANGE = { PCRDTYPE_LSB, PCRDTYPE_MSB };

            using pcrdtype_t = uint_fit_t<PCRDTYPE_WIDTH>;

            /*
            TagOp:
                * N/A    for CHI Issue B
                * 2 bits for CHI Issue E.b
            Tag Operation.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TAGOP_WIDTH = 2;
            static constexpr size_t     TAGOP_LSB   = PCRDTYPE_MSB + 1;
            static constexpr size_t     TAGOP_MSB   = PCRDTYPE_MSB + TAGOP_WIDTH;
            static constexpr FlitRange  TAGOP_RANGE = { TAGOP_LSB, TAGOP_MSB };

            using tagop_t = uint_fit_t<TAGOP_WIDTH>;
#endif

            /*
            TraceTag: 1 bit
            Trace Tag. Provides additional support for the debugging, tracing, and 
            performance measurement of systems.
            */
            static constexpr size_t     TRACETAG_WIDTH = 1;
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TRACETAG_LSB   = PCRDTYPE_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = PCRDTYPE_MSB + TRACETAG_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TRACETAG_LSB   = TAGOP_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = TAGOP_MSB + TRACETAG_WIDTH;
#endif
            static constexpr FlitRange  TRACETAG_RANGE = { TRACETAG_LSB, TRACETAG_MSB };

            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

        public:
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + OPCODE_WIDTH    + RESPERR_WIDTH     + RESP_WIDTH        + FWDSTATE_WIDTH
                                              + DBID_WIDTH      + PCRDTYPE_WIDTH    + TRACETAG_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + TGTID_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH
                                              + OPCODE_WIDTH    + RESPERR_WIDTH     + RESP_WIDTH        + FWDSTATE_WIDTH
                                              + CBUSY_WIDTH     + DBID_WIDTH        + PCRDTYPE_WIDTH    + TAGOP_WIDTH
                                              + TRACETAG_WIDTH;
#endif
            static constexpr size_t     LSB   = 0;
            static constexpr size_t     MSB   = WIDTH - 1;
            static constexpr FlitRange  RANGE = { LSB, MSB };

        // Response flit fields
        // *NOTICE: Some fields are overlapped in Flit Implementation.
        public:
            as_pointer_if_t<conn::connectedIO, qos_t>               _QoS;
            as_pointer_if_t<conn::connectedIO, tgtid_t>             _TgtID;
            as_pointer_if_t<conn::connectedIO, srcid_t>             _SrcID;
            as_pointer_if_t<conn::connectedIO, txnid_t>             _TxnID;
            as_pointer_if_t<conn::connectedIO, opcode_t>            _Opcode;
            as_pointer_if_t<conn::connectedIO, resperr_t>           _RespErr;
            as_pointer_if_t<conn::connectedIO, resp_t>              _Resp;
            union {
            as_pointer_if_t<conn::connectedIO, fwdstate_t>          _FwdState;
            as_pointer_if_t<conn::connectedIO, datapull_t>          _DataPull;
            };
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, cbusy_t>             _CBusy;
#endif
            union {
            as_pointer_if_t<conn::connectedIO, dbid_t>              _DBID;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, pgroupid_t>          _PGroupID;
            as_pointer_if_t<conn::connectedIO, stashgroupid_t>      _StashGroupID;
            as_pointer_if_t<conn::connectedIO, taggroupid_t>        _TagGroupID;
#endif
            };
            as_pointer_if_t<conn::connectedIO, pcrdtype_t>          _PCrdType;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, tagop_t>             _TagOp;
#endif
            as_pointer_if_t<conn::connectedIO, tracetag_t>          _TraceTag;

        // field decay helper functions
        public:
            inline constexpr        qos_t&              QoS             ()          noexcept    { return CHI::decay<qos_t>(_QoS); }
            inline constexpr        tgtid_t&            TgtID           ()          noexcept    { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr        srcid_t&            SrcID           ()          noexcept    { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr        txnid_t&            TxnID           ()          noexcept    { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr        opcode_t&           Opcode          ()          noexcept    { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr        resperr_t&          RespErr         ()          noexcept    { return CHI::decay<resperr_t>(_RespErr); }
            inline constexpr        resp_t&             Resp            ()          noexcept    { return CHI::decay<resp_t>(_Resp); }
            inline constexpr        fwdstate_t&         FwdState        ()          noexcept    { return CHI::decay<fwdstate_t>(_FwdState); }
            inline constexpr        datapull_t&         DataPull        ()          noexcept    { return CHI::decay<datapull_t>(_DataPull); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        cbusy_t&            CBusy           ()          noexcept    { return CHI::decay<cbusy_t>(_CBusy); }
#endif
            inline constexpr        dbid_t&             DBID            ()          noexcept    { return CHI::decay<dbid_t>(_DBID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        pgroupid_t&         PGroupID        ()          noexcept    { return CHI::decay<pgroupid_t>(_PGroupID); }
            inline constexpr        stashgroupid_t&     StashGroupID    ()          noexcept    { return CHI::decay<stashgroupid_t>(_StashGroupID); }
            inline constexpr        taggroupid_t&       TagGroupID      ()          noexcept    { return CHI::decay<taggroupid_t>(_TagGroupID); }
#endif
            inline constexpr        pcrdtype_t&         PCrdType        ()          noexcept    { return CHI::decay<pcrdtype_t>(_PCrdType); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        tagop_t&            TagOp           ()          noexcept    { return CHI::decay<tagop_t>(_TagOp); }
#endif
            inline constexpr        tracetag_t&         TraceTag        ()          noexcept    { return CHI::decay<tracetag_t>(_TraceTag); }
        
        // field decay helper functions
        public:
            inline constexpr const  qos_t&              QoS             () const    noexcept    { return CHI::decay<qos_t>(_QoS); }
            inline constexpr const  tgtid_t&            TgtID           () const    noexcept    { return CHI::decay<tgtid_t>(_TgtID); }
            inline constexpr const  srcid_t&            SrcID           () const    noexcept    { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr const  txnid_t&            TxnID           () const    noexcept    { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr const  opcode_t&           Opcode          () const    noexcept    { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr const  resperr_t&          RespErr         () const    noexcept    { return CHI::decay<resperr_t>(_RespErr); }
            inline constexpr const  resp_t&             Resp            () const    noexcept    { return CHI::decay<resp_t>(_Resp); }
            inline constexpr const  fwdstate_t&         FwdState        () const    noexcept    { return CHI::decay<fwdstate_t>(_FwdState); }
            inline constexpr const  datapull_t&         DataPull        () const    noexcept    { return CHI::decay<datapull_t>(_DataPull); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  cbusy_t&            CBusy           () const    noexcept    { return CHI::decay<cbusy_t>(_CBusy); }
#endif
            inline constexpr const  dbid_t&             DBID            () const    noexcept    { return CHI::decay<dbid_t>(_DBID); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  pgroupid_t&         PGroupID        () const    noexcept    { return CHI::decay<pgroupid_t>(_PGroupID); }
            inline constexpr const  stashgroupid_t&     StashGroupID    () const    noexcept    { return CHI::decay<stashgroupid_t>(_StashGroupID); }
            inline constexpr const  taggroupid_t&       TagGroupID      () const    noexcept    { return CHI::decay<taggroupid_t>(_TagGroupID); }
#endif
            inline constexpr const  pcrdtype_t&         PCrdType        () const    noexcept    { return CHI::decay<pcrdtype_t>(_PCrdType); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  tagop_t&            TagOp           () const    noexcept    { return CHI::decay<tagop_t>(_TagOp); }
#endif
            inline constexpr const  tracetag_t&         TraceTag        () const    noexcept    { return CHI::decay<tracetag_t>(_TraceTag); }
        };

        //
        template<class T>
        concept RSPFlitFormatConcept = requires {

            // QoS
            typename T::qos_t;
            { T::QOS_WIDTH              } -> std::convertible_to<size_t>;
            { T::QOS_LSB                } -> std::convertible_to<size_t>;
            { T::QOS_MSB                } -> std::convertible_to<size_t>;

            // TgtID
            typename T::tgtid_t;
            { T::TGTID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TGTID_LSB              } -> std::convertible_to<size_t>;
            { T::TGTID_MSB              } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH            } -> std::convertible_to<size_t>;
            { T::SRCID_LSB              } -> std::convertible_to<size_t>;
            { T::SRCID_MSB              } -> std::convertible_to<size_t>;

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TXNID_LSB              } -> std::convertible_to<size_t>;
            { T::TXNID_MSB              } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
            { T::OPCODE_LSB             } -> std::convertible_to<size_t>;
            { T::OPCODE_MSB             } -> std::convertible_to<size_t>;

            // RespErr
            typename T::resperr_t;
            { T::RESPERR_WIDTH          } -> std::convertible_to<size_t>;
            { T::RESPERR_LSB            } -> std::convertible_to<size_t>;
            { T::RESPERR_MSB            } -> std::convertible_to<size_t>;

            // Resp
            typename T::resp_t;
            { T::RESP_WIDTH             } -> std::convertible_to<size_t>;
            { T::RESP_LSB               } -> std::convertible_to<size_t>;
            { T::RESP_MSB               } -> std::convertible_to<size_t>;

            // FwdState
            typename T::fwdstate_t;
            { T::FWDSTATE_WIDTH         } -> std::convertible_to<size_t>;
            { T::FWDSTATE_LSB           } -> std::convertible_to<size_t>;
            { T::FWDSTATE_MSB           } -> std::convertible_to<size_t>;

            // DataPull
            typename T::datapull_t;
            { T::DATAPULL_WIDTH         } -> std::convertible_to<size_t>;
            { T::DATAPULL_LSB           } -> std::convertible_to<size_t>;
            { T::DATAPULL_MSB           } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // CBusy
            typename T::cbusy_t;
            { T::CBUSY_WIDTH            } -> std::convertible_to<size_t>;
            { T::CBUSY_LSB              } -> std::convertible_to<size_t>;
            { T::CBUSY_MSB              } -> std::convertible_to<size_t>;
#endif

            // DBID
            typename T::cbusy_t;
            { T::DBID_WIDTH             } -> std::convertible_to<size_t>;
            { T::DBID_LSB               } -> std::convertible_to<size_t>;
            { T::DBID_MSB               } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // PGroupID
            typename T::pgroupid_t;
            { T::PGROUPID_WIDTH         } -> std::convertible_to<size_t>;
            { T::PGROUPID_LSB           } -> std::convertible_to<size_t>;
            { T::PGROUPID_MSB           } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // StashGroupID
            typename T::stashgroupid_t;
            { T::STASHGROUPID_WIDTH     } -> std::convertible_to<size_t>;
            { T::STASHGROUPID_LSB       } -> std::convertible_to<size_t>;
            { T::STASHGROUPID_MSB       } -> std::convertible_to<size_t>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // TagGroupID
            typename T::taggroupid_t;
            { T::TAGGROUPID_WIDTH       } -> std::convertible_to<size_t>;
            { T::TAGGROUPID_LSB         } -> std::convertible_to<size_t>;
            { T::TAGGROUPID_MSB         } -> std::convertible_to<size_t>;
#endif

            // PCrdType
            typename T::pcrdtype_t;
            { T::PCRDTYPE_WIDTH         } -> std::convertible_to<size_t>;
            { T::PCRDTYPE_LSB           } -> std::convertible_to<size_t>;
            { T::PCRDTYPE_MSB           } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            typename T::tagop_t;
            { T::TAGOP_WIDTH            } -> std::convertible_to<size_t>;
            { T::TAGOP_LSB              } -> std::convertible_to<size_t>;
            { T::TAGOP_MSB              } -> std::convertible_to<size_t>;
#endif

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH         } -> std::convertible_to<size_t>;
            { T::TRACETAG_LSB           } -> std::convertible_to<size_t>;
            { T::TRACETAG_MSB           } -> std::convertible_to<size_t>;

            //
            { T::WIDTH                  } -> std::convertible_to<size_t>;
            { T::LSB                    } -> std::convertible_to<size_t>;
            { T::MSB                    } -> std::convertible_to<size_t>;
        };



        //
        template<SNPFlitConfigurationConcept    config      = FlitConfiguration<>,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class SNP {
        public:
            /*
            QoS: 4 bits
            Quality of Service priority. Specifies 1 of 16 possible priority levels for the 
            transaction with ascending values of QoS indicating higher priority levels.
            */
            static constexpr size_t     QOS_WIDTH = 4;
            static constexpr size_t     QOS_LSB   = 0;
            static constexpr size_t     QOS_MSB   = 3;
            static constexpr FlitRange  QOS_RANGE = { QOS_LSB, QOS_MSB };

            using qos_t = uint_fit_t<QOS_WIDTH>;

            /*
            SrcID: <NodeIDWidth> bits
            Source ID. The node ID of the port on the component from which the packet 
            was sent.
            */
            static constexpr size_t     SRCID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     SRCID_LSB   = QOS_MSB + 1;
            static constexpr size_t     SRCID_MSB   = QOS_MSB + SRCID_WIDTH;
            static constexpr FlitRange  SRCID_RANGE = { SRCID_LSB, SRCID_MSB };

            using srcid_t = uint_fit_t<SRCID_WIDTH>;

            /*
            TxnID: 
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Transaction ID. A transaction has a unique transaction ID per source node.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     TXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     TXNID_WIDTH = 12;
#endif
            static constexpr size_t     TXNID_LSB   = SRCID_MSB + 1;
            static constexpr size_t     TXNID_MSB   = SRCID_MSB + TXNID_WIDTH;
            static constexpr FlitRange  TXNID_RANGE = { TXNID_LSB, TXNID_MSB };

            using txnid_t = uint_fit_t<TXNID_WIDTH>;

            /*
            FwdNID: <NodeIDWidth> bits
            Forward Node ID. The node ID of the original Requester.
            */
            static constexpr size_t     FWDNID_WIDTH = config::nodeIdWidth;
            static constexpr size_t     FWDNID_LSB   = TXNID_MSB + 1;
            static constexpr size_t     FWDNID_MSB   = TXNID_MSB + FWDNID_WIDTH;
            static constexpr FlitRange  FWDNID_RANGE = { FWDNID_LSB, FWDNID_MSB };

            using fwdnid_t = uint_fit_t<FWDNID_WIDTH>;

            /*
            FwdTxnID:
                *  8 bits for CHI Issue B
                * 12 bits for CHI Issue E.b
            Forward Transaction ID. The transaction ID used in the Request by the original 
            Requester.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     FWDTXNID_WIDTH = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     FWDTXNID_WIDTH = 12;
#endif
            static constexpr size_t     FWDTXNID_LSB   = FWDNID_MSB + 1;
            static constexpr size_t     FWDTXNID_MSB   = FWDNID_MSB + FWDTXNID_WIDTH;
            static constexpr FlitRange  FWDTXNID_RANGE = { FWDTXNID_LSB, FWDTXNID_MSB };

            using fwdtxnid_t = uint_strict_t<FWDTXNID_WIDTH>;

            /*
            StashLPID: 5 bits
            Stash Logical Processor ID.
            */
            static constexpr size_t     STASHLPID_WIDTH = 5;
            static constexpr size_t     STASHLPID_LSB   = FWDNID_MSB + 1;
            static constexpr size_t     STASHLPID_MSB   = FWDNID_MSB + STASHLPID_WIDTH;
            static constexpr FlitRange  STASHLPID_RANGE = { STASHLPID_LSB, STASHLPID_MSB };

            using stashlpid_t = uintfitn_strict_t<FWDTXNID_WIDTH, STASHLPID_WIDTH>;

            /*
            StashLPIDValid: 1 bit
            Stash Logical Processor ID Valid.
            */
            static constexpr size_t     STASHLPIDVALID_WIDTH = 1;
            static constexpr size_t     STASHLPIDVALID_LSB   = STASHLPID_MSB + 1;
            static constexpr size_t     STASHLPIDVALID_MSB   = STASHLPID_MSB + STASHLPIDVALID_WIDTH;
            static constexpr FlitRange  STASHLPIDVALID_RANGE = { STASHLPIDVALID_LSB, STASHLPIDVALID_MSB };

            using stashlpidvalid_t = uintfitn_strict_t<FWDTXNID_WIDTH, STASHLPIDVALID_WIDTH, STASHLPID_WIDTH>;

            /*
            VMIDExt: 8 bits
            Virtual Machine ID Extension.
            */
            static constexpr size_t     VMIDEXT_WIDTH = 8;
            static constexpr size_t     VMIDEXT_LSB   = FWDNID_MSB + 1;
            static constexpr size_t     VMIDEXT_MSB   = FWDNID_MSB + VMIDEXT_WIDTH;
            static constexpr FlitRange  VMIDEXT_RANGE = { VMIDEXT_LSB, VMIDEXT_MSB };

            using vmidext_t = uintfitn_strict_t<FWDTXNID_WIDTH, VMIDEXT_WIDTH>;

            /*
            Opcode: 5 bits
            Snoop opcode.
            */
            static constexpr size_t     OPCODE_WIDTH = 5;
            static constexpr size_t     OPCODE_LSB   = FWDTXNID_MSB + 1;
            static constexpr size_t     OPCODE_MSB   = FWDTXNID_MSB + OPCODE_WIDTH;
            static constexpr FlitRange  OPCODE_RANGE = { OPCODE_LSB, OPCODE_MSB };

            using opcode_t = uint_fit_t<OPCODE_WIDTH>;

            /*
            Addr: <snpAddrWidth> bits
            Address. The address of the memory location being accessed for Snoop requests.
            */
            static constexpr size_t     ADDR_WIDTH = config::snpAddrWidth;
            static constexpr size_t     ADDR_LSB   = OPCODE_MSB + 1;
            static constexpr size_t     ADDR_MSB   = OPCODE_MSB + ADDR_WIDTH;
            static constexpr FlitRange  ADDR_RANGE = { ADDR_LSB, ADDR_MSB };

            using addr_t = uint_fit_t<ADDR_WIDTH>;

            /*
            NS: 1 bit
            Non-secure. Determines if the transaction is Non-secure or Secure.
            */
            static constexpr size_t     NS_WIDTH = 1;
            static constexpr size_t     NS_LSB   = ADDR_MSB + 1;
            static constexpr size_t     NS_MSB   = ADDR_MSB + NS_WIDTH;
            static constexpr FlitRange  NS_RANGE = { NS_LSB, NS_MSB };

            using ns_t = uint_fit_t<NS_WIDTH>;

            /*
            DoNotGoToSD: 1 bit
            Do Not Go To SD state. Controls Snoopee use of SD state.
            */
            static constexpr size_t     DONOTGOTOSD_WIDTH = 1;
            static constexpr size_t     DONOTGOTOSD_LSB   = NS_MSB + 1;
            static constexpr size_t     DONOTGOTOSD_MSB   = NS_MSB + DONOTGOTOSD_WIDTH;
            static constexpr FlitRange  DONOTGOTOSD_RANGE = { DONOTGOTOSD_LSB, DONOTGOTOSD_MSB };

#ifdef CHI_ISSUE_B_ENABLE
            using donotgotosd_t = uint_strict_t<DONOTGOTOSD_WIDTH>;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            using donotgotosd_t = uint_fit_t<DONOTGOTOSD_WIDTH>;
#endif

            /*
            DoNotDataPull:
                * 1 bit for CHI Issue B
                * N/A   for CHI Issue E.b
            Do Not Data Pull. Instructs the Snoopee that it is not permitted to use the Data Pull 
            feature associated with Stash requests.
            */
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     DONOTDATAPULL_WIDTH = 1;
            static constexpr size_t     DONOTDATAPULL_LSB   = NS_MSB + 1;
            static constexpr size_t     DONOTDATAPULL_MSB   = NS_MSB + DONOTDATAPULL_WIDTH;
            static constexpr FlitRange  DONOTDATAPULL_RANGE = { DONOTDATAPULL_LSB, DONOTDATAPULL_MSB };

            using donotdatapull_t = uint_strict_t<DONOTDATAPULL_WIDTH>;
#endif

            /*
            RetToSrc: 1 bit
            Return to Source. Instructs the receiver of the snoop to return data with the Snoop 
            response.
            */
            static constexpr size_t     RETTOSRC_WIDTH = 1;
            static constexpr size_t     RETTOSRC_LSB   = DONOTGOTOSD_MSB + 1;
            static constexpr size_t     RETTOSRC_MSB   = DONOTGOTOSD_MSB + RETTOSRC_WIDTH;
            static constexpr FlitRange  RETTOSRC_RANGE = { RETTOSRC_LSB, RETTOSRC_MSB };

            using rettosrc_t = uint_fit_t<RETTOSRC_WIDTH>;

            /*
            TraceTag: 1 bit
            Trace Tag.
            */
            static constexpr size_t     TRACETAG_WIDTH = 1;
            static constexpr size_t     TRACETAG_LSB   = RETTOSRC_MSB + 1;
            static constexpr size_t     TRACETAG_MSB   = RETTOSRC_MSB + TRACETAG_WIDTH;
            static constexpr FlitRange  TRACETAG_RANGE = { TRACETAG_LSB, TRACETAG_MSB };

            using tracetag_t = uint_fit_t<TRACETAG_WIDTH>;

            /*
            MPAM:
                * N/A         for CHI Issue B
                * <mpamWidth> for CHI Issue E.b
            Memory System Performance Resource Partitioning and Monitoring. 
            Efficiently utilizes the memory resources among users and monitors their use.
            */
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     MPAM_WIDTH = config::mpamWidth;
            static constexpr size_t     MPAM_LSB   = TRACETAG_MSB + 1;
            static constexpr size_t     MPAM_MSB   = TRACETAG_MSB + MPAM_WIDTH;
            static constexpr FlitRange  MPAM_RANGE = { MPAM_LSB, MPAM_MSB };

            static constexpr bool   hasMPAM = config::mpamWidth > 0;
            // *NOTICE: Use 'if constexpr (hasMPAM)' to check if 'MPAM' is available.
            using mpam_t = uint_fit_t<MPAM_WIDTH, std::monostate>;
#endif

        public:
#ifdef CHI_ISSUE_B_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH       + FWDNID_WIDTH
                                              + FWDTXNID_WIDTH  + OPCODE_WIDTH      + ADDR_WIDTH        + NS_WIDTH
                                              + DONOTGOTOSD_WIDTH                   + RETTOSRC_WIDTH    + TRACETAG_WIDTH;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            static constexpr size_t     WIDTH = QOS_WIDTH       + SRCID_WIDTH       + TXNID_WIDTH       + FWDNID_WIDTH
                                              + FWDTXNID_WIDTH  + OPCODE_WIDTH      + ADDR_WIDTH        + NS_WIDTH
                                              + DONOTGOTOSD_WIDTH                   + RETTOSRC_WIDTH    + TRACETAG_WIDTH
                                              + MPAM_WIDTH;
#endif
            static constexpr size_t     LSB   = 0;
            static constexpr size_t     MSB   = WIDTH - 1;
            static constexpr FlitRange  RANGE = { LSB, MSB };

        // Request flit fields
        // *NOTICE: Some fields are overlapped in Flit Implementation.
        public:
            as_pointer_if_t<conn::connectedIO, qos_t>               _QoS;
            as_pointer_if_t<conn::connectedIO, srcid_t>             _SrcID;
            as_pointer_if_t<conn::connectedIO, txnid_t>             _TxnID;
            as_pointer_if_t<conn::connectedIO, fwdnid_t>            _FwdNID;
            union {
            as_pointer_if_t<conn::connectedIO, fwdtxnid_t>          _FwdTxnID;
            as_pointer_if_t<conn::connectedIO, stashlpid_t>         _StashLPID;
            as_pointer_if_t<conn::connectedIO, stashlpidvalid_t>    _StashLPIDValid;
            as_pointer_if_t<conn::connectedIO, vmidext_t>           _VMIDExt;
            };
            as_pointer_if_t<conn::connectedIO, opcode_t>            _Opcode;
            as_pointer_if_t<conn::connectedIO, addr_t>              _Addr;
            as_pointer_if_t<conn::connectedIO, ns_t>                _NS;
            union {
            as_pointer_if_t<conn::connectedIO, donotgotosd_t>       _DoNotGoToSD;
#ifdef CHI_ISSUE_B_ENABLE
            as_pointer_if_t<conn::connectedIO, donotdatapull_t>     _DoNotDataPull;
#endif
            };
            as_pointer_if_t<conn::connectedIO, rettosrc_t>          _RetToSrc;
            as_pointer_if_t<conn::connectedIO, tracetag_t>          _TraceTag;
#ifdef CHI_ISSUE_EB_ENABLE
            as_pointer_if_t<conn::connectedIO, mpam_t>              _MPAM;
#endif

        // field decay helper functions
        public:
            inline constexpr        qos_t&              QoS             ()          noexcept { return CHI::decay<qos_t>(_QoS); }
            inline constexpr        srcid_t&            SrcID           ()          noexcept { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr        txnid_t&            TxnID           ()          noexcept { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr        fwdnid_t&           FwdNID          ()          noexcept { return CHI::decay<fwdnid_t>(_FwdNID); }
            inline constexpr        fwdtxnid_t&         FwdTxnID        ()          noexcept { return CHI::decay<fwdtxnid_t>(_FwdTxnID); }
            inline constexpr        stashlpid_t&        StashLPID       ()          noexcept { return CHI::decay<stashlpid_t>(_StashLPID); }
            inline constexpr        stashlpidvalid_t&   StashLPIDValid  ()          noexcept { return CHI::decay<stashlpidvalid_t>(_StashLPIDValid); }
            inline constexpr        vmidext_t&          VMIDExt         ()          noexcept { return CHI::decay<vmidext_t>(_VMIDExt); }
            inline constexpr        opcode_t&           Opcode          ()          noexcept { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr        addr_t&             Addr            ()          noexcept { return CHI::decay<addr_t>(_Addr); }
            inline constexpr        ns_t&               NS              ()          noexcept { return CHI::decay<ns_t>(_NS); }
            inline constexpr        donotgotosd_t&      DoNotGoToSD     ()          noexcept { return CHI::decay<donotgotosd_t>(_DoNotGoToSD); }
#ifdef CHI_ISSUE_B_ENABLE
            inline constexpr        donotdatapull_t&    DoNotDataPull   ()          noexcept { return CHI::decay<donotdatapull_t>(_DoNotDataPull); }
#endif
            inline constexpr        rettosrc_t&         RetToSrc        ()          noexcept { return CHI::decay<rettosrc_t>(_RetToSrc); }
            inline constexpr        tracetag_t&         TraceTag        ()          noexcept { return CHI::decay<tracetag_t>(_TraceTag); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr        mpam_t&             MPAM            ()          noexcept { return CHI::decay<mpam_t>(_MPAM); }
#endif
        
        // field decay helper functions (const)
        public:
            inline constexpr const  qos_t&              QoS             () const    noexcept { return CHI::decay<qos_t>(_QoS); }
            inline constexpr const  srcid_t&            SrcID           () const    noexcept { return CHI::decay<srcid_t>(_SrcID); }
            inline constexpr const  txnid_t&            TxnID           () const    noexcept { return CHI::decay<txnid_t>(_TxnID); }
            inline constexpr const  fwdnid_t&           FwdNID          () const    noexcept { return CHI::decay<fwdnid_t>(_FwdNID); }
            inline constexpr const  fwdtxnid_t&         FwdTxnID        () const    noexcept { return CHI::decay<fwdtxnid_t>(_FwdTxnID); }
            inline constexpr const  stashlpid_t&        StashLPID       () const    noexcept { return CHI::decay<stashlpid_t>(_StashLPID); }
            inline constexpr const  stashlpidvalid_t&   StashLPIDValid  () const    noexcept { return CHI::decay<stashlpidvalid_t>(_StashLPIDValid); }
            inline constexpr const  vmidext_t&          VMIDExt         () const    noexcept { return CHI::decay<vmidext_t>(_VMIDExt); }
            inline constexpr const  opcode_t&           Opcode          () const    noexcept { return CHI::decay<opcode_t>(_Opcode); }
            inline constexpr const  addr_t&             Addr            () const    noexcept { return CHI::decay<addr_t>(_Addr); }
            inline constexpr const  ns_t&               NS              () const    noexcept { return CHI::decay<ns_t>(_NS); }
            inline constexpr const  donotgotosd_t&      DoNotGoToSD     () const    noexcept { return CHI::decay<donotgotosd_t>(_DoNotGoToSD); }
#ifdef CHI_ISSUE_B_ENABLE
            inline constexpr const  donotdatapull_t&    DoNotDataPull   () const    noexcept { return CHI::decay<donotdatapull_t>(_DoNotDataPull); }
#endif
            inline constexpr const  rettosrc_t&         RetToSrc        () const    noexcept { return CHI::decay<rettosrc_t>(_RetToSrc); }
            inline constexpr const  tracetag_t&         TraceTag        () const    noexcept { return CHI::decay<tracetag_t>(_TraceTag); }
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr const  mpam_t&             MPAM            () const    noexcept { return CHI::decay<mpam_t>(_MPAM); }
#endif
        };

        //
        template<class T>
        concept SNPFlitFormatConcept = requires {
            
            // QoS
            typename T::qos_t;
            { T::QOS_WIDTH              } -> std::convertible_to<size_t>;
            { T::QOS_LSB                } -> std::convertible_to<size_t>;
            { T::QOS_MSB                } -> std::convertible_to<size_t>;

            // SrcID
            typename T::srcid_t;
            { T::SRCID_WIDTH            } -> std::convertible_to<size_t>;
            { T::SRCID_LSB              } -> std::convertible_to<size_t>;
            { T::SRCID_MSB              } -> std::convertible_to<size_t>;

            // TxnID
            typename T::txnid_t;
            { T::TXNID_WIDTH            } -> std::convertible_to<size_t>;
            { T::TXNID_LSB              } -> std::convertible_to<size_t>;
            { T::TXNID_MSB              } -> std::convertible_to<size_t>;

            // FwdNID
            typename T::fwdnid_t;
            { T::FWDNID_WIDTH           } -> std::convertible_to<size_t>;
            { T::FWDNID_LSB             } -> std::convertible_to<size_t>;
            { T::FWDNID_MSB             } -> std::convertible_to<size_t>;

            // FwdTxnID:
            typename T::fwdtxnid_t;
            { T::FWDTXNID_WIDTH         } -> std::convertible_to<size_t>;
            { T::FWDTXNID_LSB           } -> std::convertible_to<size_t>;
            { T::FWDTXNID_MSB           } -> std::convertible_to<size_t>;

            // StashLPID
            typename T::stashlpid_t;
            { T::STASHLPID_WIDTH        } -> std::convertible_to<size_t>;
            { T::STASHLPID_LSB          } -> std::convertible_to<size_t>;
            { T::STASHLPID_MSB          } -> std::convertible_to<size_t>;

            // StashLPIDValid
            typename T::stashlpidvalid_t;
            { T::STASHLPIDVALID_WIDTH   } -> std::convertible_to<size_t>;
            { T::STASHLPIDVALID_LSB     } -> std::convertible_to<size_t>;
            { T::STASHLPIDVALID_MSB     } -> std::convertible_to<size_t>;

            // VMIDExt
            typename T::vmidext_t;
            { T::VMIDEXT_WIDTH          } -> std::convertible_to<size_t>;
            { T::VMIDEXT_LSB            } -> std::convertible_to<size_t>;
            { T::VMIDEXT_MSB            } -> std::convertible_to<size_t>;

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
            { T::OPCODE_LSB             } -> std::convertible_to<size_t>;
            { T::OPCODE_MSB             } -> std::convertible_to<size_t>;

            // Addr
            typename T::addr_t;
            { T::ADDR_WIDTH             } -> std::convertible_to<size_t>;
            { T::ADDR_LSB               } -> std::convertible_to<size_t>;
            { T::ADDR_MSB               } -> std::convertible_to<size_t>;

            // NS
            typename T::ns_t;
            { T::NS_WIDTH               } -> std::convertible_to<size_t>;
            { T::NS_LSB                 } -> std::convertible_to<size_t>;
            { T::NS_MSB                 } -> std::convertible_to<size_t>;

            // DoNotGoToSD
            typename T::donotgotosd_t;
            { T::DONOTGOTOSD_WIDTH      } -> std::convertible_to<size_t>;
            { T::DONOTGOTOSD_LSB        } -> std::convertible_to<size_t>;
            { T::DONOTGOTOSD_MSB        } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_B_ENABLE
            // DoNotDataPull
            typename T::donotdatapull_t;
            { T::DONOTDATAPULL_WIDTH    } -> std::convertible_to<size_t>;
            { T::DONOTDATAPULL_LSB      } -> std::convertible_to<size_t>;
            { T::DONOTDATAPULL_MSB      } -> std::convertible_to<size_t>;
#endif

            // RetToSrc
            typename T::rettosrc_t;
            { T::RETTOSRC_WIDTH         } -> std::convertible_to<size_t>;
            { T::RETTOSRC_LSB           } -> std::convertible_to<size_t>;
            { T::RETTOSRC_MSB           } -> std::convertible_to<size_t>;

            // TraceTag
            typename T::tracetag_t;
            { T::TRACETAG_WIDTH         } -> std::convertible_to<size_t>;
            { T::TRACETAG_LSB           } -> std::convertible_to<size_t>;
            { T::TRACETAG_MSB           } -> std::convertible_to<size_t>;

#ifdef CHI_ISSUE_EB_ENABLE
            // MPAM
            typename T::mpam_t;
            { T::MPAM_WIDTH             } -> std::convertible_to<size_t>;
            { T::MPAM_LSB               } -> std::convertible_to<size_t>;
            { T::MPAM_MSB               } -> std::convertible_to<size_t>;
            { T::hasMPAM                } -> std::convertible_to<bool>;
#endif

            //
            { T::WIDTH                  } -> std::convertible_to<size_t>;
            { T::LSB                    } -> std::convertible_to<size_t>;
            { T::MSB                    } -> std::convertible_to<size_t>;
        };



        //
        template<class T>
        concept FlitOpcodeFormatConcept = requires {

            // Opcode
            typename T::opcode_t;
            { T::OPCODE_WIDTH           } -> std::convertible_to<size_t>;
            { T::OPCODE_LSB             } -> std::convertible_to<size_t>;
            { T::OPCODE_MSB             } -> std::convertible_to<size_t>;
        };
    }

/*
}
*/
