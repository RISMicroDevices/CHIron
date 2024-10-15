#pragma once

#ifndef __CHI__CLOG_B
#define __CHI__CLOG_B

#include <memory>
#include <string>
#include <istream>
#include <ostream>
#include <sstream>
#include <functional>

#include "clog_b_tag.hpp"

namespace CLog::CLogB {

    /* CLog.B Tag Reader */
    class Reader {
    public:
        virtual ~Reader() noexcept;

    public:
        std::shared_ptr<Tag>    Next(std::istream& is, std::string& errorMessage) noexcept;

    public:
        virtual bool    OnTagCHIParameters(std::shared_ptr<TagCHIParameters> tag, std::string& errorMessage) noexcept = 0;
        virtual bool    OnTagCHITopologies(std::shared_ptr<TagCHITopologies> tag, std::string& errorMessage) noexcept = 0;
        virtual bool    OnTagCHIRecords(std::shared_ptr<TagCHIRecords> tag, std::string& errorMessage) noexcept = 0;
    };

    /* CLog.B Tag Reader with Callbacks */
    class ReaderWithCallback : public Reader {
    public:
        ReaderWithCallback() noexcept;
        virtual ~ReaderWithCallback() noexcept;

    public:
        using CallbackOnTagCHIParameters =
            std::function<bool(std::shared_ptr<TagCHIParameters>, std::string&)>;

        using CallbackOnTagCHITopologies =
            std::function<bool(std::shared_ptr<TagCHITopologies>, std::string&)>;

        using CallbackOnTagCHIRecords =
            std::function<bool(std::shared_ptr<TagCHIRecords>, std::string&)>;

    public:
        CallbackOnTagCHIParameters  callbackOnTagCHIParameters;
        CallbackOnTagCHITopologies  callbackOnTagCHITopologies;
        CallbackOnTagCHIRecords     callbackOnTagCHIRecords;

    public:
        virtual bool    OnTagCHIParameters(std::shared_ptr<TagCHIParameters> tag, std::string& errorMessage) noexcept override;
        virtual bool    OnTagCHITopologies(std::shared_ptr<TagCHITopologies> tag, std::string& errorMessage) noexcept override;
        virtual bool    OnTagCHIRecords(std::shared_ptr<TagCHIRecords> tag, std::string& errorMessage) noexcept override;
    };

    /* CLog.B Tag Writer */
    class Writer {
    public:
        void            Next(std::ostream& os, const Tag* tag) const noexcept;
    };
}


// Implementation of: class Reader
namespace CLog::CLogB {
    /*
    */

    inline Reader::~Reader() noexcept
    { }

    inline std::shared_ptr<Tag> Reader::Next(std::istream& is, std::string& errorMessage) noexcept
    {
        // read tag type
        tagtype_t type;

        if (!is.read(reinterpret_cast<char*>(&type), 1))
        {
            errorMessage = "Reader: type: unexpected EOF";
            return nullptr;
        }

        // read length
        size_t length;

        if (!is.read(reinterpret_cast<char*>(&length), 4))
        {
            errorMessage = "Reader: length: unexpected EOF";
            return nullptr;
        }

        length = details::__bswap64_to_little(length);

        //
        if (type == Encodings::CHI_PARAMETERS)
        {
            std::shared_ptr<TagCHIParameters> tag(new TagCHIParameters);

            if (!tag->Deserialize(is, errorMessage))
                return nullptr;

            return tag;
        }
        else if (type == Encodings::CHI_TOPOS)
        {
            std::shared_ptr<TagCHITopologies> tag(new TagCHITopologies);

            if (!tag->Deserialize(is, errorMessage))
                return nullptr;

            return tag;
        }
        else if (type == Encodings::CHI_RECORDS)
        {
            std::shared_ptr<TagCHIRecords> tag(new TagCHIRecords);

            if (!tag->Deserialize(is, errorMessage))
                return nullptr;

            return tag;
        }
        else
        {
            errorMessage = StringAppender("Reader: ")
                .Append("unrecognized tag type: ", uint64_t(type))
                .ToString();
            return nullptr;
        }
    }
}


// Implementation of: class ReaderWithCallback
namespace CLog::CLogB {
    /*
    CallbackOnTagCHIParameters  callbackOnTagCHIParameters;
    CallbackOnTagCHITopologies  callbackOnTagCHITopologies;
    CallbackOnTagCHIRecords     callbackOnTagCHIRecords;
    */

    inline ReaderWithCallback::ReaderWithCallback() noexcept
        : callbackOnTagCHIParameters    ([](auto, auto) { return true; })
        , callbackOnTagCHITopologies    ([](auto, auto) { return true; })
        , callbackOnTagCHIRecords       ([](auto, auto) { return true; })
    { }

    inline ReaderWithCallback::~ReaderWithCallback() noexcept
    { }

    inline bool ReaderWithCallback::OnTagCHIParameters(std::shared_ptr<TagCHIParameters> tag, std::string& errorMessage) noexcept
    {
        return callbackOnTagCHIParameters(tag, errorMessage);
    }

    inline bool ReaderWithCallback::OnTagCHITopologies(std::shared_ptr<TagCHITopologies> tag, std::string& errorMessage) noexcept
    {
        return callbackOnTagCHITopologies(tag, errorMessage);
    }

    inline bool ReaderWithCallback::OnTagCHIRecords(std::shared_ptr<TagCHIRecords> tag, std::string& errorMessage) noexcept
    {
        return callbackOnTagCHIRecords(tag, errorMessage);
    }
}


// Implementation of: class Writer
namespace CLog::CLogB {
    /*
    */

    inline void Writer::Next(std::ostream& os, const Tag* tag) const noexcept
    {
        std::ostringstream oss(std::ios_base::out | std::ios_base::binary);

        // write tag type
        os.write(reinterpret_cast<const char*>(&(tag->type)), 1);

        // serialize tag
        tag->Serialize(oss);

        // write tag length
        size_t length = details::__bswap64_to_little(oss.tellp());
        os.write(reinterpret_cast<const char*>(&length), 8);

        // write tag
        os.write(reinterpret_cast<const char*>(oss.str().c_str()), length);
    }
}


#endif // __CHI__CLOG_B
