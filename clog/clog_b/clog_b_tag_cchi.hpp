#pragma once

#ifndef __CHI__CLOG_B_TAG_CCHI
#define __CHI__CLOG_B_TAG_CCHI

#include "clog_b_tag.hpp"
#include "../clog_cchi.hpp"
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
    namespace Encodings {
        static constexpr tagtype_t CCHI_PARAMETERS  = 0x02;
        static constexpr tagtype_t CCHI_TOPOS       = 0x03;
        static constexpr tagtype_t CCHI_RECORDS     = 0x11;
    }

    /* CLog.B Tag of CCHI Parameters */
    /*
    *   Binary format:
    *       0 -->   componentType         : 1B
    *               txnIdWidth            : 1B
    *               dbIdWidth             : 1B
    *               upstreamNodeIdWidth   : 1B
    *               downstreamNodeIdWidth : 1B
    *               wayIndexWidth         : 1B
    *               dataWidth             : 2B
    *               tagAliasWidth         : 1B
    *               uwPersistEnable       : 1B
    *               uwPredictEnable       : 1B
    *               reserved              : 1B
    */
    class TagCCHIParameters : public Tag {
    public:
        CCHI::Parameters    parameters;

    public:
        TagCCHIParameters() noexcept;
        virtual ~TagCCHIParameters() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };

    /* CLog.B Tag of CCHI Topologies */
    /*
    *   Binary format:
    *       0 -->   count           : 2B
    *               nodes[count] => {
    *                   nodeType        : 1B
    *                   nodeId          : 2B
    *               }
    */
    class TagCCHITopologies : public Tag {
    public:
        struct Node {
            CCHI::NodeType  type;
            uint16_t        id;
        };

    public:
        std::vector<Node>   nodes;

    public:
        TagCCHITopologies() noexcept;
        virtual ~TagCCHITopologies() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };

    /* CLog.B Tag of CCHI Records */
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
    class TagCCHIRecords : public Tag {
    public:
        struct Head {
            uint32_t    length;
            uint32_t    compressed; // set to non-zero value to enable compression on serialization
            uint64_t    timeBase;
        };

        struct Record {
            uint32_t                    timeShift;
            uint16_t                    nodeId;
            CCHI::Channel               channel;
            uint8_t                     flitLength;
            std::shared_ptr<uint32_t[]> flit;
        };

    public:
        Head                head        = { 0, 0, 0 };
        std::vector<Record> records;

    public:
        TagCCHIRecords() noexcept;
        virtual ~TagCCHIRecords() noexcept;

        virtual std::string     GetName() const noexcept override;
        virtual std::string     GetCanonicalName() const noexcept override;

        virtual bool            Deserialize(std::istream& is, std::string& errorMessage) noexcept override;
        virtual void            Serialize(std::ostream& os) const noexcept override;
    };
}


// Implementation of: class TagCCHIParameters
namespace CLog::CLogB {
    /*
    CCHI::Parameters    parameters;
    */

    inline TagCCHIParameters::TagCCHIParameters() noexcept
        : Tag   (Encodings::CCHI_PARAMETERS)
    { }

    inline TagCCHIParameters::~TagCCHIParameters() noexcept
    { }

    inline std::string TagCCHIParameters::GetName() const noexcept
    {
        return "CCHI_PARAMETERS";
    }

    inline std::string TagCCHIParameters::GetCanonicalName() const noexcept
    {
        return "CCHIParameters";
    }

    inline bool TagCCHIParameters::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        char b;

        // read ComponentType
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: ComponentType: unexpected EOF";
            return false;
        }

        parameters.SetComponentType(CCHI::NodeType(b));

        switch (parameters.GetComponentType())
        {
            case CCHI::NodeType::Type1:
            case CCHI::NodeType::Type2:
            case CCHI::NodeType::Type3:
            case CCHI::NodeType::Type4:
            case CCHI::NodeType::Type5:
            case CCHI::NodeType::Home:
                break;

            default:
                errorMessage = StringAppender("Tag CCHIParameters: unknown ComponentType value: ")
                    .Append(uint64_t(b))
                    .ToString();
                return false;
        }

        // read TxnIdWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: TxnIdWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetTxnIdWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid TxnIdWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read DBIdWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: DBIdWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetDBIdWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid DBIdWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read UpstreamNodeIdWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: UpstreamNodeIdWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetUpstreamNodeIdWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid UpstreamNodeIdWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read DownstreamNodeIdWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: DownstreamNodeIdWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetDownstreamNodeIdWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid DownstreamNodeIdWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read WayIndexWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: WayIndexWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetWayIndexWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid WayIndexWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read DataWidth
        uint16_t dataWidth;

        if (!is.read(reinterpret_cast<char*>(&dataWidth), sizeof(dataWidth)))
        {
            errorMessage = "Tag CCHIParameters: DataWidth: unexpected EOF";
            return false;
        }

        dataWidth = details::__bswap16_to_little(dataWidth);

        if (!parameters.SetDataWidth(size_t(dataWidth)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid DataWidth value: ")
                .Append(uint64_t(dataWidth))
                .ToString();
            return false;
        }

        // read TagAliasWidth
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: TagAliasWidth: unexpected EOF";
            return false;
        }

        if (!parameters.SetTagAliasWidth(size_t(b)))
        {
            errorMessage = StringAppender("Tag CCHIParameters: invalid TagAliasWidth value: ")
                .Append(uint64_t(b))
                .ToString();
            return false;
        }

        // read UWPersistEnable
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: UWPersistEnable: unexpected EOF";
            return false;
        }

        parameters.SetUWPersistEnable(bool(b));

        // read UWPredictEnable
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: UWPredictEnable: unexpected EOF";
            return false;
        }

        parameters.SetUWPredictEnable(bool(b));

        // read reserved
        if (!is.get(b))
        {
            errorMessage = "Tag CCHIParameters: reserved: unexpected EOF";
            return false;
        }

        return true;
    }

    inline void TagCCHIParameters::Serialize(std::ostream& os) const noexcept
    {
        uint16_t v;

        // write ComponentType
        v = uint8_t(parameters.GetComponentType());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write TxnIdWidth
        v = uint8_t(parameters.GetTxnIdWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write DBIdWidth
        v = uint8_t(parameters.GetDBIdWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write UpstreamNodeIdWidth
        v = uint8_t(parameters.GetUpstreamNodeIdWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write DownstreamNodeIdWidth
        v = uint8_t(parameters.GetDownstreamNodeIdWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write WayIndexWidth
        v = uint8_t(parameters.GetWayIndexWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write DataWidth
        v = details::__bswap16_to_little(uint16_t(parameters.GetDataWidth()));
        os.write(reinterpret_cast<char*>(&v), 2);

        // write TagAliasWidth
        v = uint8_t(parameters.GetTagAliasWidth());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write UWPersistEnable
        v = uint8_t(parameters.IsUWPersistEnabled());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write UWPredictEnable
        v = uint8_t(parameters.IsUWPredictEnabled());
        os.write(reinterpret_cast<char*>(&v), 1);

        // write reserved
        v = 0;
        os.write(reinterpret_cast<char*>(&v), 1);
    }
}


// Implementation of: class TagCCHITopologies
namespace CLog::CLogB {
    /*
    std::vector<Node>   nodes;
    */

    inline TagCCHITopologies::TagCCHITopologies() noexcept
        : Tag   (Encodings::CCHI_TOPOS)
    { }

    inline TagCCHITopologies::~TagCCHITopologies() noexcept
    { }

    inline std::string TagCCHITopologies::GetName() const noexcept
    {
        return "CCHI_TOPOS";
    }

    inline std::string TagCCHITopologies::GetCanonicalName() const noexcept
    {
        return "CCHITopologies";
    }

    inline bool TagCCHITopologies::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        // read count
        uint16_t count;

        if (!is.read(reinterpret_cast<char*>(&count), sizeof(count)))
        {
            errorMessage = "Tag CCHITopologies: count: unexpected EOF";
            return false;
        }

        count = details::__bswap16_to_little(count);

        // read nodes
        nodes.clear();

        for (uint16_t i = 0; i < count; i++)
        {
            TagCCHITopologies::Node node;

            // read node type
            uint8_t nodeType;

            if (!is.read(reinterpret_cast<char*>(&nodeType), sizeof(nodeType)))
            {
                errorMessage = StringAppender("Tag CCHITopologies: ")
                    .Append("node(", uint64_t(i), "): ")
                    .Append("type: unexpected EOF")
                    .ToString();
                return false;
            }

            switch (node.type = CCHI::NodeType(nodeType))
            {
                case CCHI::NodeType::Type1:
                case CCHI::NodeType::Type2:
                case CCHI::NodeType::Type3:
                case CCHI::NodeType::Type4:
                case CCHI::NodeType::Type5:
                case CCHI::NodeType::Home:
                    break;

                default:
                    errorMessage = StringAppender("Tag CCHITopologies: ")
                        .Append("node(", uint64_t(i), "): ")
                        .Append("invalid type value: ", uint64_t(nodeType))
                        .ToString();
                    return false;
            }

            // read node id
            uint16_t nodeId;

            if (!is.read(reinterpret_cast<char*>(&nodeId), sizeof(nodeId)))
            {
                errorMessage = StringAppender("Tag CCHITopologies: ")
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

    inline void TagCCHITopologies::Serialize(std::ostream& os) const noexcept
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


// Implementation of: class TagCCHIRecords
namespace CLog::CLogB {
    /*
    Head                head;
    std::vector<Record> records;
    */

    inline TagCCHIRecords::TagCCHIRecords() noexcept
        : Tag   (Encodings::CCHI_RECORDS)
    { }

    inline TagCCHIRecords::~TagCCHIRecords() noexcept
    { }

    inline std::string TagCCHIRecords::GetName() const noexcept
    {
        return "CCHI_RECORDS";
    }

    inline std::string TagCCHIRecords::GetCanonicalName() const noexcept
    {
        return "CCHIRecords";
    }

    inline bool TagCCHIRecords::Deserialize(std::istream& is, std::string& errorMessage) noexcept
    {
        // read count
        uint32_t count;

        if (!is.read(reinterpret_cast<char*>(&count), sizeof(count)))
        {
            errorMessage = "Tag CCHIRecords: count: unexpected EOF";
            return false;
        }

        count = details::__bswap32_to_little(count);

        // read timeBase
        if (!is.read(reinterpret_cast<char*>(&head.timeBase), sizeof(head.timeBase)))
        {
            errorMessage = "Tag CCHIRecords: timeBase: unexpected EOF";
            return false;
        }

        head.timeBase = details::__bswap64_to_little(head.timeBase);

        // read length
        if (!is.read(reinterpret_cast<char*>(&head.length), sizeof(head.length)))
        {
            errorMessage = "Tag CCHIRecords: length: unexpected EOF";
            return false;
        }

        head.length = details::__bswap32_to_little(head.length);

        // read compressed
        if (!is.read(reinterpret_cast<char*>(&head.compressed), sizeof(head.compressed)))
        {
            errorMessage = "Tag CCHIRecords: compressed: unexpected EOF";
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
                errorMessage = "Tag CCHIRecords: records(compressed): unexpected EOF";
                return false;
            }

            unsigned long flitLengthUncompressed = head.length;
            int zerr = uncompress(flitData, &flitLengthUncompressed, flitDataCompressed, head.compressed);

            if (zerr != Z_OK)
            {
                if (zerr == Z_BUF_ERROR)
                {
                    errorMessage = "Tag CCHIRecords: records(compressed): uncompression overflow";
                    return false;
                }

                errorMessage = "Tag CCHIRecords: records(compressed): uncompression exception";
                return false;
            }

            if (flitLengthUncompressed != head.length)
            {
                errorMessage = "Tag CCHIRecords: records(compressed): data shrunk or corruption";
                return false;
            }

            delete[] flitDataCompressed;
        }
        else
        {
            if (!is.read(reinterpret_cast<char*>(flitData), head.length))
            {
                errorMessage = "Tag CCHIRecords: records: unexpected EOF";
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
                errorMessage = StringAppender("Tag CCHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("timeShift: unexpected EOS")
                    .ToString();
                return false;
            }

            record.timeShift = details::__bswap32_to_little(record.timeShift);

            // read nodeId
            if (!ibs.read(reinterpret_cast<char*>(&record.nodeId), sizeof(record.nodeId)))
            {
                errorMessage = StringAppender("Tag CCHIRecords: ")
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
                errorMessage = StringAppender("Tag CCHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("channel: unexpected EOS")
                    .ToString();
                return false;
            }

            switch(record.channel = CCHI::Channel(channel))
            {
                case CCHI::Channel::TXEVT:
                case CCHI::Channel::TXREQ:
                case CCHI::Channel::TXSNP:
                case CCHI::Channel::TXDNRSP:
                case CCHI::Channel::TXUPRSP:
                case CCHI::Channel::TXDNDAT:
                case CCHI::Channel::TXUPDAT:
                case CCHI::Channel::RXEVT:
                case CCHI::Channel::RXREQ:
                case CCHI::Channel::RXSNP:
                case CCHI::Channel::RXDNRSP:
                case CCHI::Channel::RXUPRSP:
                case CCHI::Channel::RXDNDAT:
                case CCHI::Channel::RXUPDAT:
                    break;

                default:
                    errorMessage = StringAppender("Tag CCHIRecords: ")
                        .Append("record(", i, "): ")
                        .Append("channel: invalid value: ", uint64_t(channel))
                        .ToString();
                    return false;
            }

            // read flitLength
            if (!ibs.read(reinterpret_cast<char*>(&record.flitLength), sizeof(record.flitLength)))
            {
                errorMessage = StringAppender("Tag CCHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("flitLength: unexpected EOS")
                    .ToString();
                return false;
            }

            // read flit

            // record.flit = std::make_shared<uint32_t[]>((record.flitLength + 3) >> 2);
            // * for legacy c++ header support:
            record.flit = std::shared_ptr<uint32_t[]>(new uint32_t[(record.flitLength + 3) >> 2]);

            if (!ibs.read(reinterpret_cast<char*>(record.flit.get()), record.flitLength))
            {
                errorMessage = StringAppender("Tag CCHIRecords: ")
                    .Append("record(", i, "): ")
                    .Append("flit: unexpected EOS")
                    .ToString();
                return false;
            }

            // *NOTICE: all flits are stored in Little-Endian
            if (std::endian::native == std::endian::big)
                for (size_t i = 0; i < size_t((record.flitLength + 3) >> 2); i++)
                    record.flit.get()[i] = __builtin_bswap32(record.flit.get()[i]);

            //
            records.push_back(record);
        }

        delete[] flitData;

        return true;
    }

    inline void TagCCHIRecords::Serialize(std::ostream& os) const noexcept
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
        else
        {
            // write compressed
            v = 0;
            os.write(reinterpret_cast<char*>(&v), 4);

            // write records
            os.write(reinterpret_cast<const char*>(recordBlock.c_str()), recordBlock.size());
        }
    }
}


#endif // __CHI__CLOG_B_TAG_CCHI
