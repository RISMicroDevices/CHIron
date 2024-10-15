#pragma once

#ifndef __CHI__CLOG_B_TAG
#define __CHI__CLOG_B_TAG

#include "../clog.hpp"
#include "../../common/utility.hpp"

#include <vector>
#include <istream>
#include <ostream>
#include <sstream>
#include <memory>
#include <bit>

#include <zlib.h>

namespace CLog::CLogB {

    //
    using tagtype_t = uint8_t;

    /* CLog.B tag type encodings */
    namespace Encodings {
        static constexpr tagtype_t CHI_PARAMETERS   = 0x00;
        static constexpr tagtype_t CHI_TOPOS        = 0x01;
        static constexpr tagtype_t CHI_RECORDS      = 0x10;
    }

    /* CLog.B Tag base class */
    class Tag {
    public:
        tagtype_t   type;

    public:
        Tag(tagtype_t type) noexcept;
        virtual ~Tag() noexcept;

        virtual std::string     GetName() const noexcept = 0;
        virtual std::string     GetCanonicalName() const noexcept = 0;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept = 0;
        virtual void            Serialize(std::ostream& os) const noexcept = 0;
    };


    /* CLog.B Tag of CHI Parameters */
    /*  
    *   Binary format:
    *       0 -->   issue           : 1B
    *               nodeIdWidth     : 1B
    *               reqAddrWidth    : 1B
    *               reqRsvdcWidth   : 1B
    *               datRsvdcWidth   : 1B
    *               dataWidth       : 2B
    *               dataCheckPresent: 1B
    *               poisonPresent   : 1B
    *               mpamPresent     : 1B
    */
    class TagCHIParameters : public Tag {
    public:
        Parameters  parameters;

    public:
        TagCHIParameters() noexcept;
        virtual ~TagCHIParameters() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };


    /* CLog.B Tag of CHI Topologies */
    /*
    *   Binary format:
    *       0 -->   count           : 2B
    *               nodes[count] => {
    *                   nodeType        : 1B
    *                   nodeId          : 2B
    *               }
    */
    class TagCHITopologies : public Tag {
    public:
        struct Node {
            NodeType    type;
            uint16_t    id;
        };

    public:
        std::vector<Node>   nodes;

    public:
        TagCHITopologies() noexcept;
        virtual ~TagCHITopologies() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };


    /* CLog.B Tag of CHI Records */
    /*
    * Binary format:
    *   0 -->   count       : 4B
    *           timeBase    : 8B
    *           length      : 4B
    *           compressed  : 4B
    *           records[count] => {
    *               timeShift   : 4B
    *               nodeId      : 2B
    *               channel     : 1B
    *               flitLength  : 1B
    *               flit[flitLength]: (flitLength)B
    *           }
    */
    class TagCHIRecords : public Tag {
    public:
        struct Head {
            uint32_t    length;
            uint32_t    compressed; // set to non-zero value to enable compression on serialization
            uint64_t    timeBase;
        };

        struct Record {
            uint32_t                    timeShift;
            uint16_t                    nodeId;
            Channel                     channel;
            uint8_t                     flitLength;
            std::shared_ptr<uint32_t[]> flit;
        };

    public:
        Head                head        = { 0, 0, 0 };
        std::vector<Record> records;

    public:
        TagCHIRecords() noexcept;
        virtual ~TagCHIRecords() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };



    // details
    namespace details {

        inline uint16_t __bswap16_to_little(uint16_t val)
        {
            if constexpr (std::endian::native == std::endian::big)
                return __builtin_bswap16(val);
            else
                return val;
        }

        inline uint32_t __bswap32_to_little(uint32_t val)
        {
            if constexpr (std::endian::native == std::endian::big)
                return __builtin_bswap32(val);
            else
                return val;
        }

        inline uint64_t __bswap64_to_little(uint64_t val)
        {
            if constexpr (std::endian::native == std::endian::big)
                return __builtin_bswap64(val);
            else
                return val;
        }
    }
}


// Implementation of: class Tag
namespace CLog::CLogB {
    /*
    tagtype_t   type;
    */

    inline Tag::Tag(tagtype_t type) noexcept
        : type  (type)
    { }

    inline Tag::~Tag() noexcept
    { }
}


// Implementation of: class TagCHIParameters
namespace CLog::CLogB {
    /*
    Parameters  parameters;
    */

    inline TagCHIParameters::TagCHIParameters() noexcept
        : Tag   (Encodings::CHI_PARAMETERS)
    { }

    inline TagCHIParameters::~TagCHIParameters() noexcept
    { }

    inline std::string TagCHIParameters::GetName() const noexcept
    {
        return "CHI_PARAMETERS";
    }

    inline std::string TagCHIParameters::GetCanonicalName() const noexcept
    {
        return "CHIParameters";
    }

    inline bool TagCHIParameters::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        char b;

        // read Issue
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: Issue: unexpected EOF";
            return false;
        }

        parameters.SetIssue(Issue(b));

        switch (parameters.GetIssue())
        {
            case Issue::B:
            case Issue::Eb:
                break;

            default:
                errorMessage = StringAppender("Tag CHIParameters: unknown Issue value: ")
                    .Append(uint64_t(b))
                    .ToString();
                return false;
        }

        // read NodeIdWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: NodeIdWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetNodeIdWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CHIParameters: invalid NodeIdWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read reqAddrWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: ReqAddrWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetReqAddrWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CHIParameters: invalid ReqAddrWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read reqRsvdcWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: ReqRSVDCWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetReqRSVDCWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CHIParameters: invalid ReqRSVDCWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read datRsvdcWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: DatRSVDCWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetDatRSVDCWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CHIParameters: invalid DatRSVDCWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read dataWidth
        uint16_t dataWidth;

        if (!is.read(reinterpret_cast<char*>(&dataWidth), sizeof(dataWidth)))
        {
            errorMessage = "Tag CHIParameters: DataWidth: unexpected EOF";
            return false;
        }

        dataWidth = details::__bswap16_to_little(dataWidth);

        if (!parameters.SetDataWidth(size_t(dataWidth)))
        {
            errorMessage = StringAppender("Tag CHIParameters: invalid DataWidth value: ")
                .Append(uint64_t(dataWidth))
                .ToString();
            return false;
        }

        // read dataCheckPresent
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: DataCheckPresent: unexpected EOF";
            return false;
        }

        parameters.SetDataCheckPresent(bool(b));

        // read poisonPresent
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: PoisonPresent: unexpected EOF";
            return false;
        }

        parameters.SetPoisonPresent(bool(b));

        // read mpamPresent
        if (!is.get(b))
        {
            errorMessage = "Tag CHIParameters: MPAMPresent: unexpected EOF";
            return false;
        }

        parameters.SetMPAMPresent(bool(b));

        return true;
    }

    inline void TagCHIParameters::Serialize(std::ostream& os) const noexcept
    {
        uint16_t v;

        // write Issue
        v = uint8_t(parameters.GetIssue());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write NodeIdWidth
        v = uint8_t(parameters.GetNodeIdWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write ReqAddrWidth
        v = uint8_t(parameters.GetReqAddrWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write ReqRSVDCWidth
        v = uint8_t(parameters.GetReqRSVDCWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write DatRSVDCWidth
        v = uint8_t(parameters.GetDatRSVDCWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write DataWidth
        v = details::__bswap16_to_little(uint16_t(parameters.GetDataWidth()));
        os.write(reinterpret_cast<char*>(&v), 2);

        // write DataCheckPresent
        v = uint8_t(parameters.IsDataCheckPresent());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write PoisonPresent
        v = uint8_t(parameters.IsPoisonPresent());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write MPAMPresent
        v = uint8_t(parameters.IsMPAMPresent());
        os.write(reinterpret_cast<char*>(&v), 1);
    }
}


// Implementation of: class TagCHITopologies
namespace CLog::CLogB {
    /*
    std::vector<Node>   nodes;
    */

    inline TagCHITopologies::TagCHITopologies() noexcept
        : Tag   (Encodings::CHI_TOPOS)
    { }

    inline TagCHITopologies::~TagCHITopologies() noexcept
    { }

    inline std::string TagCHITopologies::GetName() const noexcept
    {
        return "CHI_TOPOS";
    }

    inline std::string TagCHITopologies::GetCanonicalName() const noexcept
    {
        return "CHITopologies";
    }

    inline bool TagCHITopologies::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        // read count
        uint16_t count;

        if (!is.read(reinterpret_cast<char*>(&count), sizeof(count)))
        {
            errorMessage = "Tag CHITopologies: count: unexpected EOF";
            return false;
        }

        count = details::__bswap16_to_little(count);

        // read nodes
        nodes.clear();

        for (uint16_t i = 0; i < count; i++)
        {
            TagCHITopologies::Node node;

            // read node type
            uint8_t nodeType;

            if (!is.read(reinterpret_cast<char*>(&nodeType), sizeof(nodeType)))
            {
                errorMessage = StringAppender("Tag CHITopologies: ")
                    .Append("node(", uint64_t(i), "): ")
                    .Append("type: unexpected EOF")
                    .ToString();
                return false;
            }

            switch (node.type = NodeType(nodeType))
            {
                case NodeType::RN_F:
                case NodeType::RN_D:
                case NodeType::RN_I:
                case NodeType::HN_F:
                case NodeType::HN_I:
                case NodeType::SN_F:
                case NodeType::SN_I:
                case NodeType::MN:
                    break;

                default:
                    errorMessage = StringAppender("Tag CHITopologies: ")
                        .Append("node(", uint64_t(i), "): ")
                        .Append("invalid type value: ", uint64_t(nodeType))
                        .ToString();
                    return false;
            }

            // read node id
            uint16_t nodeId;

            if (!is.read(reinterpret_cast<char*>(&nodeId), sizeof(nodeId)))
            {
                errorMessage = StringAppender("Tag CHITopologies: ")
                    .Append("node(", uint64_t(i), "): ")
                    .Append("id: unexpected EOF")
                    .ToString();
                return false;
            }

            nodeId = details::__bswap16_to_little(nodeId);

            node.id = nodeId;

            //
            nodes.push_back(node);
        }

        return true;
    }

    inline void TagCHITopologies::Serialize(std::ostream& os) const noexcept
    {
        uint16_t v;

        // write count
        v = details::__bswap16_to_little(uint16_t(nodes.size()));
        os.write(reinterpret_cast<char*>(&v), 2);

        // write nodes
        for (uint16_t i = 0; i < nodes.size(); i++)
        {
            // write node type
            v = uint8_t(nodes[i].type);
            os.write(reinterpret_cast<char*>(&v), 1);

            // write node id
            v = details::__bswap16_to_little(uint16_t(nodes[i].id));
            os.write(reinterpret_cast<char*>(&v), 2);
        }
    }
}


// Implementation of: class TagCHIRecords
namespace CLog::CLogB {
    /*
    Head                head;
    std::vector<Record> records;
    */

    inline TagCHIRecords::TagCHIRecords() noexcept
        : Tag   (Encodings::CHI_RECORDS)
    { }

    inline TagCHIRecords::~TagCHIRecords() noexcept
    { }

    inline std::string TagCHIRecords::GetName() const noexcept
    {
        return "CHI_RECORDS";
    }

    inline std::string TagCHIRecords::GetCanonicalName() const noexcept
    {
        return "CHIRecords";
    }

    inline bool TagCHIRecords::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        // read count
        uint32_t count;

        if (!is.read(reinterpret_cast<char*>(&count), sizeof(count)))
        {
            errorMessage = "Tag CHIRecords: count: unexpected EOF";
            return false;
        }

        count = details::__bswap32_to_little(count);

        // read timeBase
        if (!is.read(reinterpret_cast<char*>(&head.timeBase), sizeof(head.timeBase)))
        {
            errorMessage = "Tag CHIRecords: timeBase: unexpected EOF";
            return false;
        }

        head.timeBase = details::__bswap64_to_little(head.timeBase);

        // read length
        if (!is.read(reinterpret_cast<char*>(&head.length), sizeof(head.length)))
        {
            errorMessage = "Tag CHIRecords: length: unexpected EOF";
            return false;
        }

        head.length = details::__bswap32_to_little(head.length);

        // read compressed
        if (!is.read(reinterpret_cast<char*>(&head.compressed), sizeof(head.compressed)))
        {
            errorMessage = "Tag CHIRecords: compressed: unexpected EOF";
            return false;
        }

        head.compressed = details::__bswap32_to_little(head.compressed);

        // read records
        records.reserve(count);

        uint8_t* flitData = new uint8_t[head.length];

        if (head.compressed)
        {
            uint8_t* flitDataCompressed = new uint8_t[head.compressed];

            if (!is.read(reinterpret_cast<char*>(flitDataCompressed), head.compressed))
            {
                errorMessage = "Tag CHIRecords: records(compressed): unexpected EOF";
                return false;
            }

            unsigned long flitLengthUncompressed = head.length;
            int zerr = uncompress(flitData, &flitLengthUncompressed, flitDataCompressed, head.compressed);

            if (zerr != Z_OK)
            {
                if (zerr == Z_BUF_ERROR)
                {
                    errorMessage = "Tag CHIRecords: records(compressed): uncompression overflow";
                    return false;
                }

                errorMessage = "Tag CHIRecords: records(compressed): uncompression exception";
                return false;
            }

            if (flitLengthUncompressed != head.length)
            {
                errorMessage = "Tag CHIRecords: records(compressed): data shrunk or corruption";
                return false;
            }

            delete[] flitDataCompressed;
        }
        else
        {
            if (!is.read(reinterpret_cast<char*>(flitData), head.length))
            {
                errorMessage = "Tag CHIRecords: records: unexpected EOF";
                return false;
            }
        }

        std::istringstream ibs(std::string(reinterpret_cast<char*>(flitData), head.length),
            std::ios::binary);

        for (uint32_t i = 0; i < count; i++)
        {
            Record record;

            // read timeShift
            if (!ibs.read(reinterpret_cast<char*>(&record.timeShift), sizeof(record.timeShift)))
            {
                errorMessage = StringAppender("Tag CHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("timeShift: unexpected EOS")
                    .ToString();
                return false;
            }

            record.timeShift = details::__bswap32_to_little(record.timeShift);

            // read nodeId
            if (!ibs.read(reinterpret_cast<char*>(&record.nodeId), sizeof(record.nodeId)))
            {
                errorMessage = StringAppender("Tag CHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("nodeId: unexpected EOS")
                    .ToString();
                return false;
            }

            record.nodeId = details::__bswap16_to_little(record.nodeId);

            // read channel
            uint8_t channel;
            if (!ibs.read(reinterpret_cast<char*>(&channel), 1))
            {
                errorMessage = StringAppender("Tag CHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("channel: unexpected EOS")
                    .ToString();
                return false;
            }

            switch(record.channel = Channel(channel))
            {
                case Channel::TXREQ:
                case Channel::TXRSP:
                case Channel::TXDAT:
                case Channel::TXSNP:
                case Channel::RXREQ:
                case Channel::RXRSP:
                case Channel::RXDAT:
                case Channel::RXSNP:
                    break;

                default:
                    errorMessage = StringAppender("Tag CHIRecords: ")
                        .Append("record(", i, "): ")
                        .Append("channel: invalid value: ", uint64_t(channel))
                        .ToString();
                    return false;
            }

            // read flitLength
            if (!ibs.read(reinterpret_cast<char*>(&record.flitLength), sizeof(record.flitLength)))
            {
                errorMessage = StringAppender("Tag CHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("flitLength: unexpected EOS")
                    .ToString();
                return false;
            }

            // read flit

            // record.flit = std::make_shared<uint32_t[]>((head.length + 3) >> 2);
            // * for legacy c++ header support:
            record.flit = std::shared_ptr<uint32_t[]>(new uint32_t[(head.length + 3) >> 2]);

            if (!ibs.read(reinterpret_cast<char*>(record.flit.get()), head.length))
            {
                errorMessage = StringAppender("Tag CHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("flit: unexpected EOS")
                    .ToString();
                return false;
            }

            // *NOTICE: all flits are stored in Little-Endian
            if (std::endian::native == std::endian::big)
                for (size_t i = 0; i < ((head.length + 3) >> 2); i++)
                    record.flit.get()[i] = __builtin_bswap32(record.flit.get()[i]);

            //
            records.push_back(record);
        }

        delete[] flitData;

        return true;
    }

    inline void TagCHIRecords::Serialize(std::ostream& os) const noexcept
    {
        uint64_t v;

        // generate record block
        std::string recordBlock;
        {
            std::ostringstream obs(std::ios::binary);

            for (auto& record : records)
            {
                // write timeShift
                v = details::__bswap32_to_little(record.timeShift);
                obs.write(reinterpret_cast<char*>(&v), 4);

                // write nodeId
                v = details::__bswap16_to_little(record.nodeId);
                obs.write(reinterpret_cast<char*>(&v), 2);

                // write channel
                v = uint8_t(record.channel);
                obs.write(reinterpret_cast<char*>(&v), 1);

                // write flitLength
                v = uint8_t(record.flitLength);
                obs.write(reinterpret_cast<char*>(&v), 1);

                // write flit
                obs.write(reinterpret_cast<char*>(record.flit.get()), record.flitLength);
            }

            recordBlock = obs.str();
        }

        // write count
        v = details::__bswap32_to_little(uint32_t(records.size()));
        os.write(reinterpret_cast<char*>(&v), 4);

        // write timeBase
        v = details::__bswap64_to_little(uint64_t(head.timeBase));
        os.write(reinterpret_cast<char*>(&v), 8);

        // write length
        v = details::__bswap32_to_little(uint32_t(recordBlock.size()));
        os.write(reinterpret_cast<char*>(&v), 4);

        // write and flit compression
        if (head.compressed)
        {
            unsigned long compressedLength = recordBlock.size();
            uint8_t* recordBlockCompressed = new uint8_t[recordBlock.size()];

            int zerr = compress(recordBlockCompressed, &compressedLength, 
                reinterpret_cast<const unsigned char*>(recordBlock.c_str()), recordBlock.size());

            if (zerr != Z_OK)
            {
                // drop back to uncompressed

                // write compressed
                v = 0;
                os.write(reinterpret_cast<char*>(&v), 4);

                // write records
                os.write(reinterpret_cast<const char*>(recordBlock.c_str()), recordBlock.size());
            }
            else
            {
                // write compressed
                v = details::__bswap32_to_little(uint32_t(compressedLength));
                os.write(reinterpret_cast<char*>(&v), 4);

                // write records
                os.write(reinterpret_cast<char*>(recordBlockCompressed), compressedLength);

                delete[] recordBlockCompressed;
            }
        }
    }
}


#endif // __CHI__CLOG_B_TAG
