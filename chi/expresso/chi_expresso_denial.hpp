//#pragma once

//#ifndef __CHI_CHI_EXPRESSO_DENIAL
//#define __CHI_CHI_EXPRESSO_DENIAL

#ifndef CHI_EXPRESSO_DENIAL__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_expresso_denial_header.hpp"    // IWYU pragma: keep
#   include "chi_expresso_xaction.hpp"          // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_EXPRESSO_DENIAL_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_EXPRESSO_DENIAL_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_EXPRESSO_DENIAL_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_EXPRESSO_DENIAL_EB
#endif


/*
namespace CHI {
*/
    namespace Expresso::Denial {

        class SourceEnumBack {
        public:
            const char* name;
            const int   ordinal;

        public:
            inline constexpr SourceEnumBack(const char* name, const int ordinal) noexcept
            : name(name), ordinal(ordinal) { }

        public:
            inline constexpr operator int() const noexcept
            { return ordinal; }

            inline constexpr operator const SourceEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const SourceEnumBack& obj) const noexcept
            { return ordinal == obj.ordinal; }
            
            inline constexpr bool operator!=(const SourceEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using SourceEnum = const SourceEnumBack*;

        namespace Source {
            inline constexpr SourceEnumBack ENVIRONMENT     ("ENVIRONMENT"  , 0); // Framework environment & outer components
            inline constexpr SourceEnumBack XACTION         ("XACTION"      , 1); // Transaction-level
            inline constexpr SourceEnumBack JOINT           ("JOINT"        , 2); // Interface-level or Node-level
        };

        class ObjectTypeEnumBack {
        public:
            const char* name;
            const int   ordinal;

        public:
            inline constexpr ObjectTypeEnumBack(const char* name, const int ordinal) noexcept
            : name(name), ordinal(ordinal) { }

        public:
            inline constexpr operator int() const noexcept
            { return ordinal; }

            inline constexpr operator const ObjectTypeEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const ObjectTypeEnumBack& obj) const noexcept
            { return ordinal == obj.ordinal; }

            inline constexpr bool operator!=(const ObjectTypeEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using ObjectTypeEnum = const ObjectTypeEnumBack*;

        namespace ObjectType {
            inline constexpr ObjectTypeEnumBack EMPTY                   ("EMPTY"                    , 0);
            inline constexpr ObjectTypeEnumBack FIRED_FLIT_REQUEST      ("FIRED_FLIT_REQUEST"       , 1);
            inline constexpr ObjectTypeEnumBack FIRED_FLIT_RESPONSE     ("FIRED_FLIT_RESPONSE"      , 2);
            inline constexpr ObjectTypeEnumBack XACTION                 ("XACTION"                  , 3);
            inline constexpr ObjectTypeEnumBack FIELD_CONVENTION        ("FIELD_CONVENTION"         , 4);
            inline constexpr ObjectTypeEnumBack CACHE_STATE_MAP         ("CACHE_STATE_MAP"          , 5);
        };

        template<FlitConfigurationConcept config>
        class Object {
        public:
            ObjectTypeEnum type;
            union {
                std::shared_ptr<Xact::FiredRequestFlit<config>>     firedFlitRequest;
                std::shared_ptr<Xact::FiredResponseFlit<config>>    firedFlitResponse;
                std::shared_ptr<Xact::Xaction<config>>              xaction;
                // TODO: FIELD_CONVENTION
                // TODO: CACHE_STATE_MAP
            } obj;

        public:
            inline Object() noexcept;
            inline Object(const std::shared_ptr<Xact::FiredRequestFlit<config>>& firedFlitRequest) noexcept;
            inline Object(const std::shared_ptr<Xact::FiredResponseFlit<config>>& firedFlitResponse) noexcept;
            inline Object(const std::shared_ptr<Xact::Xaction<config>>& xaction) noexcept;
            inline ~Object() noexcept;
        };

        template<FlitConfigurationConcept config>
        using Objects = std::vector<Object<config>>;

        template<FlitConfigurationConcept config>
        using ObjectKeys = std::function<std::vector<Flit::Key>(const Object<config>&)>;

        //
        template<FlitConfigurationConcept config>
        class ExplanationBack {
        public:
            using FunctionTitleMessage = std::function<std::string(
                const Xact::Global<config>&         glbl,
                SourceEnum                          source
            )>;

            using FunctionFurtherMessage = std::function<std::string(
                const Xact::Global<config>&         glbl,
                SourceEnum                          source,
                const Flit::Printer*                flitPrinter,
                const Objects<config>&              subjects,
                const Objects<config>&              complements
            )>;

        public:
            const ExplanationBack* const    prev;
            const char*                     name;
            const int                       value;

        public: 
            const FunctionTitleMessage      GetTitleMessage;
            const FunctionFurtherMessage    GetFurtherMessage;
            const ObjectKeys<config>        GetSubjectKeys;
            const ObjectKeys<config>        GetComplementKeys;

        public:
            inline constexpr ExplanationBack(const char*                            name,
                                             const int                              value,
                                             const FunctionTitleMessage             funcGetTitleMessage,
                                             const FunctionFurtherMessage           funcGetFurtherMessage,
                                             const ObjectKeys<config>               funcGetSubjectKeys,
                                             const ObjectKeys<config>               funcGetComplementKeys,
                                             const ExplanationBack<config>*         prev = nullptr) noexcept;

        public:
            inline constexpr operator const ExplanationBack<config>*() const noexcept
            { return this; }
        };

        template<FlitConfigurationConcept config>
        using Explanation = const ExplanationBack<config>*;


        template<FlitConfigurationConcept config>
        class Explained {
        public:
            Xact::XactDenialEnum        denial;
            Explanation<config>         explanation;

            std::string                 titleMessage;
            std::string                 furtherMessage;
            SourceEnum                  source;

            Objects<config>             subjects;
            ObjectKeys<config>          subjectKeys;

            Objects<config>             complements;
            ObjectKeys<config>          complementKeys;
        };

        class Printer {
            // TODO
        };

        template<FlitConfigurationConcept config>
        class Explainer {
        public:
            

        public:
            // TODO: DENIED_NESTING_SNP

            /*
            DENIED_CHANNEL_TXREQ
            - Title Message: "Denied channel TXREQ"
            - Further Message: "Unexpected flit on channel TXREQ: {Print({FiredRequestFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_TXREQ = (
                "DENIED_CHANNEL_TXREQ",
                Xact::XactDenial::DENIED_CHANNEL_TXREQ,
                [](auto, SourceEnum source) -> std::string { return "Denied channel TXREQ"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel TXREQ: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsREQ()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.req);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_RXREQ
            - Title Message: "Denied channel RXREQ"
            - Further Message: "Unexpected flit on channel RXREQ: {Print({FiredResponseFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_RXREQ = (
                "DENIED_CHANNEL_RXREQ",
                Xact::XactDenial::DENIED_CHANNEL_RXREQ,
                [](auto, SourceEnum source) -> std::string { return "Denied channel RXREQ"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel RXREQ: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsREQ()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.req);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_TXSNP
            - Title Message: "Denied channel TXSNP"
            - Further Message: "Unexpected flit on channel TXSNP: {Print({FiredRequestFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_TXSNP = (
                "DENIED_CHANNEL_TXSNP",
                Xact::XactDenial::DENIED_CHANNEL_TXSNP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel TXSNP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel TXSNP: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsSNP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.snp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_RXSNP
            - Title Message: "Denied channel RXSNP"
            - Further Message: "Unexpected flit on channel RXSNP: {Print({FiredRequestFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_RXSNP = (
                "DENIED_CHANNEL_RXSNP",
                Xact::XactDenial::DENIED_CHANNEL_RXSNP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel RXSNP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel RXSNP: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsSNP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.snp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_TXRSP
            - Title Message: "Denied channel TXRSP"
            - Further Message: "Unexpected flit on channel TXRSP: {Print({FiredResponseFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_TXRSP = (
                "DENIED_CHANNEL_TXRSP",
                Xact::XactDenial::DENIED_CHANNEL_TXRSP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel TXRSP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel TXRSP: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->IsRSP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.rsp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_RXRSP
            - Title Message: "Denied channel RXRSP"
            - Further Message: "Unexpected flit on channel RXRSP: {Print({FiredResponseFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_RXRSP = (
                "DENIED_CHANNEL_RXRSP",
                Xact::XactDenial::DENIED_CHANNEL_RXRSP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel RXRSP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel RXRSP: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->IsRSP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.rsp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_TXDAT
            - Title Message: "Denied channel TXDAT"
            - Further Message: "Unexpected flit on channel TXDAT: {Print({FiredResponseFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_TXDAT = (
                "DENIED_CHANNEL_TXDAT",
                Xact::XactDenial::DENIED_CHANNEL_TXDAT,
                [](auto, SourceEnum source) -> std::string { return "Denied channel TXDAT"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel TXDAT: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.dat);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_RXDAT
            - Title Message: "Denied channel RXDAT"
            - Further Message: "Unexpected flit on channel RXDAT: {Print({FiredResponseFlit})}"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_RXDAT = (
                "DENIED_CHANNEL_RXDAT",
                Xact::XactDenial::DENIED_CHANNEL_RXDAT,
                [](auto, SourceEnum source) -> std::string { return "Denied channel RXDAT"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on channel RXDAT: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.dat);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_NOT_REQ
            - Title Message: "Denied channel not REQ"'
            - Further Message: "Unexpected flit on non-REQ channel: {Print({FiredRequestFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_NOT_REQ = (
                "DENIED_CHANNEL_NOT_REQ",
                Xact::XactDenial::DENIED_CHANNEL_NOT_REQ,
                [](auto, SourceEnum source) -> std::string { return "Denied channel not REQ"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on non-REQ channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsREQ()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.req);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_NOT_SNP
            - Title Message: "Denied channel not SNP"'
            - Further Message: "Unexpected flit on non-SNP channel: {Print({FiredRequestFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_NOT_SNP = {
                "DENIED_CHANNEL_NOT_SNP",
                Xact::XactDenial::DENIED_CHANNEL_NOT_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel not SNP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on non-SNP channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsSNP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.snp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            };

            /*
            DENIED_CHANNEL_NOT_RSP
            - Title Message: "Denied channel not RSP"'
            - Further Message: "Unexpected flit on non-RSP channel: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_NOT_RSP = {
                "DENIED_CHANNEL_NOT_RSP",
                Xact::XactDenial::DENIED_CHANNEL_NOT_RSP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel not RSP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on non-RSP channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->IsRSP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.rsp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            };

            /*
            DENIED_CHANNEL_NOT_DAT
            - Title Message: "Denied channel not DAT"'
            - Further Message: "Unexpected flit on non-DAT channel: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_NOT_DAT = {
                "DENIED_CHANNEL_NOT_DAT",
                Xact::XactDenial::DENIED_CHANNEL_NOT_DAT,
                [](auto, SourceEnum source) -> std::string { return "Denied channel not DAT"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on non-DAT channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->IsDAT()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.dat);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            };

            /*
            DENIED_CHANNEL_REQ
            - Title Message: "Denied channel REQ"
            - Further Message: "Unexpected flit on REQ channel: {Print({FiredRequestFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_REQ = (
                "DENIED_CHANNEL_REQ",
                Xact::XactDenial::DENIED_CHANNEL_REQ,
                [](auto, SourceEnum source) -> std::string { return "Denied channel REQ"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on REQ channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsREQ()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.req);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_SNP
            - Title Message: "Denied channel SNP"
            - Further Message: "Unexpected flit on SNP channel: {Print({FiredRequestFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_SNP = (
                "DENIED_CHANNEL_SNP",
                Xact::XactDenial::DENIED_CHANNEL_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel SNP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on SNP channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->IsSNP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.snp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_RSP
            - Title Message: "Denied channel RSP"
            - Further Message: "Unexpected flit on RSP channel: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_RSP = (
                "DENIED_CHANNEL_RSP",
                Xact::XactDenial::DENIED_CHANNEL_RSP,
                [](auto, SourceEnum source) -> std::string { return "Denied channel RSP"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on RSP channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->IsRSP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.rsp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_CHANNEL_DAT
            - Title Message: "Denied channel DAT"
            - Further Message: "Unexpected flit on DAT channel: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_CHANNEL_DAT = (
                "DENIED_CHANNEL_DAT",
                Xact::XactDenial::DENIED_CHANNEL_DAT,
                [](auto, SourceEnum source) -> std::string { return "Denied channel DAT"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected flit on DAT channel: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.dat);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_COMPLETED_RSP
            - Title Message: "Denied RSP on completed transaction"
            - Further Message: "Unexpected RSP flit after transaction completed: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_COMPLETED_RSP = (
                "DENIED_COMPLETED_RSP",
                Xact::XactDenial::DENIED_COMPLETED_RSP,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP on completed transaction"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected RSP flit after transaction completed: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.rsp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_COMPLETED_DAT
            - Title Message: "Denied DAT on completed transaction"
            - Further Message: "Unexpected DAT flit after transaction completed: {Print({FiredResponseFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_COMPLETED_DAT = (
                "DENIED_COMPLETED_DAT",
                Xact::XactDenial::DENIED_COMPLETED_DAT,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT on completed transaction"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected DAT flit after transaction completed: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitResponse->flit.dat);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_COMPLETED_SNP
            - Title Message: "Denied SNP on completed transaction"
            - Further Message: "Unexpected SNP flit after transaction completed: {Print({FiredRequestFlit})}"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            */
            inline static constexpr ExplanationBack<config> DENIED_COMPLETED_SNP = (
                "DENIED_COMPLETED_SNP",
                Xact::XactDenial::DENIED_COMPLETED_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied SNP on completed transaction"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "Unexpected SNP flit after transaction completed: ";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            flitPrinter->PrintFlit(oss, subject.obj.firedFlitRequest->flit.snp);
                            break;
                        }
                    }
                    return oss.str();
                },
                [](auto) -> std::vector<Flit::Key> { return {}; },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_REQ_TXNID_IN_USE
            - Title Message: "Denied TxnID of REQ already in use"
            - Further Message: "TxnID {REQ.TxnID} of REQ flit is already in use by existing transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredRequestFlit, Xaction]
            - Subject Key: 1. REQ.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_TXNID_IN_USE = (
                "DENIED_REQ_TXNID_IN_USE",
                Xact::XactDenial::DENIED_REQ_TXNID_IN_USE,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID of REQ already in use"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "TxnID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            uint64_t txnId = subject.obj.firedFlitRequest->flit.req.TxnID();
                            oss << std::format(" {:#x}", txnId);
                            break;
                        }
                    }
                    oss << " of REQ flit is already in use by existing transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::TxnID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_SNP_TXNID_IN_USE
            - Title Message: "Denied TxnID of SNP already in use"
            - Further Message: "TxnID {SNP.TxnID} of SNP flit is already in use by existing transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredRequestFlit, Xaction]
            - Subject Key: 1. SNP.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_TXNID_IN_USE = (
                "DENIED_SNP_TXNID_IN_USE",
                Xact::XactDenial::DENIED_SNP_TXNID_IN_USE,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID of SNP already in use"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "TxnID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            uint64_t txnId = subject.obj.firedFlitRequest->flit.snp.TxnID();
                            oss << std::format(" {:#x}", txnId);
                            break;
                        }
                    }
                    oss << " of SNP flit is already in use by existing transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TxnID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_RSP_TXNID_NOT_EXIST
            - Title Message: "Denied TxnID of RSP does not exist"
            - Further Message: "TxnID {RSP.TxnID} of RSP flit does not match any transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            - Subject Key: 1. RSP.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_TXNID_NOT_EXIST = (
                "DENIED_RSP_TXNID_NOT_EXIST",
                Xact::XactDenial::DENIED_RSP_TXNID_NOT_EXIST,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID of RSP does not exist"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "TxnID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t txnId = subject.obj.firedFlitResponse->flit.rsp.TxnID();
                            oss << std::format(" {:#x}", txnId);
                            break;
                        }
                    }
                    oss << " of RSP flit does not match any transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_DAT_TXNID_NOT_EXIST
            - Title Message: "Denied TxnID of DAT does not exist"
            - Further Message: "TxnID {DAT.TxnID} of DAT flit does not match any transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_NOT_EXIST = (
                "DENIED_DAT_TXNID_NOT_EXIST",
                Xact::XactDenial::DENIED_DAT_TXNID_NOT_EXIST,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID of DAT does not exist"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "TxnID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t txnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            oss << std::format(" {:#x}", txnId);
                            break;
                        }
                    }
                    oss << " of DAT flit does not match any transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_RSP_DBID_IN_USE
            - Title Message: "Denied DBID of RSP already in use"
            - Further Message: "DBID {RSP.DBID} of RSP flit occupied by existing transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit, Xaction]
            - Subject Key: 1. RSP.DBID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_DBID_IN_USE = (
                "DENIED_RSP_DBID_IN_USE",
                Xact::XactDenial::DENIED_RSP_DBID_IN_USE,
                [](auto, SourceEnum source) -> std::string { return "Denied DBID of RSP already in use"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "DBID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t dbId = subject.obj.firedFlitResponse->flit.rsp.DBID();
                            oss << std::format(" {:#x}", dbId);
                            break;
                        }
                    }
                    oss << " of RSP flit occupied by existing transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::DBID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_DAT_DBID_IN_USE
            - Title Message: "Denied DBID of DAT already in use"
            - Further Message: "DBID {DAT.DBID} of DAT flit occupied by existing transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit, Xaction]
            - Subject Key: 1. DAT.DBID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_DBID_IN_USE = (
                "DENIED_DAT_DBID_IN_USE",
                Xact::XactDenial::DENIED_DAT_DBID_IN_USE,
                [](auto, SourceEnum source) -> std::string { return "Denied DBID of DAT already in use"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "DBID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t dbId = subject.obj.firedFlitResponse->flit.dat.DBID();
                            oss << std::format(" {:#x}", dbId);
                            break;
                        }
                    }
                    oss << " of DAT flit occupied by existing transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::DBID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_RSP_DBID_NOT_EXIST
            - Title Message: "Denied DBID of RSP does not exist"
            - Further Message: "DBID {RSP.DBID} of RSP flit does not match any transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            - Subject Key: 1. RSP.DBID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_DBID_NOT_EXIST = (
                "DENIED_RSP_DBID_NOT_EXIST",
                Xact::XactDenial::DENIED_RSP_DBID_NOT_EXIST,
                [](auto, SourceEnum source) -> std::string { return "Denied DBID of RSP does not exist"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "DBID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t dbId = subject.obj.firedFlitResponse->flit.rsp.DBID();
                            oss << std::format(" {:#x}", dbId);
                            break;
                        }
                    }
                    oss << " of RSP flit does not match any transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::DBID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            /*
            DENIED_DAT_DBID_NOT_EXIST
            - Title Message: "Denied DBID of DAT does not exist"
            - Further Message: "DBID {DAT.DBID} of DAT flit does not match any transaction"
            - Source: [Joint]
            - Subject: [Joint, FiredResponseFlit]
            - Subject Key: 1. DAT.DBID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_DBID_NOT_EXIST = (
                "DENIED_DAT_DBID_NOT_EXIST",
                Xact::XactDenial::DENIED_DAT_DBID_NOT_EXIST,
                [](auto, SourceEnum source) -> std::string { return "Denied DBID of DAT does not exist"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, const Flit::Printer* flitPrinter, const Objects<config>& subjects, auto) -> std::string {
                    std::ostringstream oss;
                    oss << "DBID";
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t dbId = subject.obj.firedFlitResponse->flit.dat.DBID();
                            oss << std::format(" {:#x}", dbId);
                            break;
                        }
                    }
                    oss << " of DAT flit does not match any transaction";
                    return oss.str();
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::DBID };
                },
                [](auto) -> std::vector<Flit::Key> { return {}; }
            );

            // TODO

            /*
            DENIED_REQ_OPCODE
            - Title Message: "Denied REQ opcode"
            - Further Message: "Unexpected opcode: {REQ.Opcode} (Decode({REQ.Opcode}))"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            - Subject Key: 1. REQ.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_OPCODE = (
                "DENIED_REQ_OPCODE",
                Xact::XactDenial::DENIED_REQ_OPCODE,
                [](auto, SourceEnum source) -> std::string { return "Denied REQ opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            return std::format("Unexpected opcode: {:#x} ({})",
                                uint64_t(subject.obj.firedFlitRequest->flit.req.Opcode()),
                                Opcodes::REQ::Decoder<Flits::REQ<config>>::INSTANCE.Decode(subject.obj.firedFlitRequest->flit.req.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_OPCODE
            - Title Message: "Denied RSP opcode"
            - Further Message: "Unexpected opcode: {RSP.Opcode} (Decode({RSP.Opcode}))"
            - Source: [Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            - Subject Key: 1. RSP.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_OPCODE = (
                "DENIED_RSP_OPCODE",
                Xact::XactDenial::DENIED_RSP_OPCODE,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            return std::format("Unexpected opcode: {:#x} ({})",
                                uint64_t(subject.obj.firedFlitResponse->flit.rsp.Opcode()),
                                Opcodes::RSP::Decoder<Flits::RSP<config>>::INSTANCE.Decode(subject.obj.firedFlitResponse->flit.rsp.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );



        // TODO
        };
    }
/*
}
*/


// Implementation of: class Object
namespace /*CHI::*/Expresso::Denial {

    template<FlitConfigurationConcept config>
    inline Object<config>::Object() noexcept
        : type  (ObjectType::EMPTY)
    { }

    template<FlitConfigurationConcept config>
    inline Object<config>::Object(const std::shared_ptr<Xact::FiredRequestFlit<config>>& firedFlitRequest) noexcept
        : type  (ObjectType::FIRED_FLIT_REQUEST)
    { 
        obj.firedFlitRequest = firedFlitRequest;
    }

    template<FlitConfigurationConcept config>
    inline Object<config>::Object(const std::shared_ptr<Xact::FiredResponseFlit<config>>& firedFlitResponse) noexcept
        : type  (ObjectType::FIRED_FLIT_RESPONSE)
    {
        obj.firedFlitResponse = firedFlitResponse;
    }

    template<FlitConfigurationConcept config>
    inline Object<config>::Object(const std::shared_ptr<Xact::Xaction<config>>& xaction) noexcept
        : type  (ObjectType::XACTION)
    {
        obj.xaction = xaction;
    }

    // TODO: FIELD_CONVENTION

    // TODO: CACHE_STATE_MAP

    template<FlitConfigurationConcept config>
    inline Object<config>::~Object() noexcept
    {
        switch (type->ordinal)
        {
            case ObjectType::FIRED_FLIT_REQUEST:
                obj.firedFlitRequest.~shared_ptr<>();
                break;

            case ObjectType::FIRED_FLIT_RESPONSE:
                obj.firedFlitResponse.~shared_ptr<>();
                break;

            case ObjectType::XACTION:
                obj.xaction.~shared_ptr<>();
                break;

            default:
                break;
        }
    }
}


// Implementation of: Printer
namespace /*CHI::*/Expresso::Denial {

}


// Implementation of: class Explainer
namespace /*CHI::*/Expresso::Denial {


}



/*
DENIED_OPCODE ** DEPRECATED **
- Title Message: "Denied opcode"
- Further Message: "Unexpected opcode: {REQ.Opcode/SNP.Opcode/RSP.Opcode/DAT.Opcode} ({Decode({REQ.Opcode/SNP.Opcode/RSP.Opcode/DAT.Opcode})})"
- Source: [Xaction, Joint]
- Subject: [FiredRequestFlit, FiredResponseFlit]
- Subject Key: 1. REQ.Opcode/SNP.Opcode/RSP.Opcode/DAT.Opcode
*/

/*
DENIED_REQ_OPCODE
- Title Message: "Denied REQ opcode"
- Further Message: "Unexpected opcode: {REQ.Opcode} (Decode({REQ.Opcode}))"
- Source: [Xaction, Joint]
- Subject: [FiredRequestFlit]
- Subject Key: 1. REQ.Opcode
*/

// ...

/*
DENIED_REQ_OPCODE_NOT_SUPPORTED
- Title Message: "Denied unsupported REQ opcode by component"
- Further Message: "Unsupported opcode: {REQ.Opcode} (Decode({REQ.Opcode}))"
- Source: [Xaction, Joint]
- Subject: [FiredRequestFlit]
- Subject Key: 1. REQ.Opcode
*/

// ...

/*
DENIED_REQ_NOT_TO_HN
- Title Message: "Denied REQ flit target, expecting HN"
- Further Message: "Unexpected target: {REQ.TgtID} ({TOPOLOGY[REQ.TgtID]})"
- Source: [Xaction]
- Subject: [FiredRequestFlit]
- Subject Key: 1. REQ.TgtID
*/

// ...

/*
DENIED_RSP_RETRYACK_ROUTE
- Title Message: "Denied RetryAck flit route, expecting HN to RN or SN to HN"
- Further Message: "Unexpected source: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]})"
- Source: [Xaction]
- Subject: [FiredResponseFlit]
- Subject Key: 1. RSP.SrcID
               2. RSP.TgtID
*/

// ...

/*
DENIED_RSP_TXNID_MISMATCHING_REQ
- Title Message: "Denied TxnID in RSP flit, not identical to REQ"
- Further Message: "Unexpected RSP TxnID: {RSP.TxnID}, expecting: {REQ.TxnID} from REQ"
- Source: [Xaction]
- Subject: [FiredResponseFlit]
- Subject Key: 1. RSP.TxnID
- Complement: [FiredRequestFlit]
- Complement Key: 1. REQ.TxnID
*/

// ...

/*
DENIED_RSP_DBID_MISMATCH
- Title Message: "Denied DBID in RSP flit, not identical between flits"
- Further Message: "Unexpected non-identical RSP DBID: {RSP.DBID}, with: {RSP.DBID/DAT.DBID} from DBID source"
- Source: [Xaction]
- Subject: [FiredResponseFlit]
- Subject Key: 1. RSP.DBID
- Complement: [FiredRequestFlit]
- Complement Key: 1. RSP.DBID/DAT.DBID
*/

// ...

/*
DENIED_PCRD_NO_RETRY
- Title Message: "Denied P-Credit consumption, delivered to a transaction without any received RetryAck"
- Further Message: "Unexpected P-Credit consumption by a transaction without any received RetryAck"
- Source: [Xaction]
- Subject: [Xaction], [FiredResponseFlit]
- Subject Key: 1. REQ.Opcode
               2. RSP.Opcode
- Complement: [Xaction]
- Complement Key: 1. REQ.Opcode
*/

// ...

/*
DENIED_RESPSEPDATA_AFTER_COMPDATA
- Title Message: "Denied RespSepData after CompData received"
- Further Message: "Unexpected RespSepData after CompData received"
- Source: [Xaction]
- Subject: [FiredResponseFlit]
- Subject Key: 1. RSP.Opcode
- Complement: [FiredResponseFlit]
- Complement Key: 1. DAT.Opcode
*/

// ...

/*
DENIED_SNPRESPFWDED_COMPDATA_FWDSTATE_MISMATCH
- Title Message: "Denied mismatched FwdState and Resp between SnpRespFwded and CompData on DCT"
- Further Message: "Unexpected mismatched FwdState: {RSP.FwdState} of SnpRespFwded and Resp: {DAT.Resp} of CompData "
- Source: [Xaction]
- Subject: [FiredResponseFlit], [FiredResponseFlit]
- Subject Key: 1. (RSP.Opcode == SnpRespFwded) RSP.FwdState
               2. (DAT.Opcode == CompData) DAT.Resp
*/

// ...


#endif //

//#endif
