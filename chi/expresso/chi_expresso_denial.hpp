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
            - Source: [RNCacheStateMap, Joint]
            - Subject: [RNCacheStateMap/Joint, FiredResponseFlit]
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
            - Source: [RNCacheStateMap, Joint, Xaction]
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

            /*
            DENIED_DAT_OPCODE
            - Title Message: "Denied DAT opcode"
            - Further Message: "Unexpected opcode: {DAT.Opcode} (Decode({DAT.Opcode}))"
            - Source: [RNCacheStateMap, Xaction]
            - Subject: [Xaction, FiredResponseFlit]
            - Subject Key: 1. DAT.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_OPCODE = (
                "DENIED_DAT_OPCODE",
                Xact::XactDenial::DENIED_DAT_OPCODE,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            return std::format("Unexpected opcode: {:#x} ({})",
                                uint64_t(subject.obj.firedFlitResponse->flit.dat.Opcode()),
                                Opcodes::DAT::Decoder<Flits::DAT<config>>::INSTANCE.Decode(subject.obj.firedFlitResponse->flit.dat.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_SNP_OPCODE
            - Title Message: "Denied SNP opcode"
            - Further Message: "Unexpected opcode: {SNP.Opcode} (Decode({SNP.Opcode}))"
            - Source: [Xaction]
            - Subject: [Xaction, FiredRequestFlit]
            - Subject Key: 1. SNP.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_OPCODE = (
                "DENIED_SNP_OPCODE",
                Xact::XactDenial::DENIED_SNP_OPCODE,
                [](auto, SourceEnum source) -> std::string { return "Denied SNP opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            return std::format("Unexpected opcode: {:#x} ({})",
                                uint64_t(subject.obj.firedFlitRequest->flit.snp.Opcode()),
                                Opcodes::SNP::Decoder<Flits::SNP<config>>::INSTANCE.Decode(subject.obj.firedFlitRequest->flit.snp.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_REQ_OPCODE_NOT_SUPPORTED
            - Title Message: "Denied unsupported REQ opcode by component"
            - Further Message: "Opcode {REQ.Opcode} (Decode({REQ.Opcode})) decoded but not supported by component"
            - Source: [Xaction, Joint, RNCacheStateMap]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. REQ.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_OPCODE_NOT_SUPPORTED = (
                "DENIED_REQ_OPCODE_NOT_SUPPORTED",
                Xact::XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED,
                [](auto, SourceEnum source) -> std::string { return "Denied unsupported REQ opcode by component"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            return std::format("REQ Opcode {:#x} ({}) decoded but not supported by component",
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
            DENIED_RSP_OPCODE_NOT_SUPPORTED
            - Title Message: "Denied unsupported RSP opcode by component"
            - Further Message: "Opcode {RSP.Opcode} (Decode({RSP.Opcode})) decoded but not supported by component"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_OPCODE_NOT_SUPPORTED = (
                "DENIED_RSP_OPCODE_NOT_SUPPORTED",
                Xact::XactDenial::DENIED_RSP_OPCODE_NOT_SUPPORTED,
                [](auto, SourceEnum source) -> std::string { return "Denied unsupported RSP opcode by component"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            return std::format("RSP Opcode {:#x} ({}) decoded but not supported by component",
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

            /*
            DENIED_DAT_OPCODE_NOT_SUPPORTED
            - Title Message: "Denied unsupported DAT opcode by component"
            - Further Message: "Opcode {DAT.Opcode} (Decode({DAT.Opcode})) decoded but not supported by component"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_OPCODE_NOT_SUPPORTED = (
                "DENIED_DAT_OPCODE_NOT_SUPPORTED",
                Xact::XactDenial::DENIED_DAT_OPCODE_NOT_SUPPORTED,
                [](auto, SourceEnum source) -> std::string { return "Denied unsupported DAT opcode by component"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            return std::format("DAT Opcode {:#x} ({}) decoded but not supported by component",
                                uint64_t(subject.obj.firedFlitResponse->flit.dat.Opcode()),
                                Opcodes::DAT::Decoder<Flits::DAT<config>>::INSTANCE.Decode(subject.obj.firedFlitResponse->flit.dat.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_SNP_OPCODE_NOT_SUPPORTED
            - Title Message: "Denied unsupported SNP opcode by component"
            - Further Message: "Opcode {SNP.Opcode} (Decode({SNP.Opcode})) decoded but not supported by component"
            - Source: [Xaction, Joint, RNCacheStateMap]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_OPCODE_NOT_SUPPORTED = (
                "DENIED_SNP_OPCODE_NOT_SUPPORTED",
                Xact::XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED,
                [](auto, SourceEnum source) -> std::string { return "Denied unsupported SNP opcode by component"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            return std::format("SNP Opcode {:#x} ({}) decoded but not supported by component",
                                uint64_t(subject.obj.firedFlitRequest->flit.snp.Opcode()),
                                Opcodes::SNP::Decoder<Flits::SNP<config>>::INSTANCE.Decode(subject.obj.firedFlitRequest->flit.snp.Opcode()).GetName("<unknown>"));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_REQ_OPCODE_NOT_DECODED
            - Title Message: "Denied unrecognized REQ opcode"
            - Further Message: "REQ Opcode {REQ.Opcode} could not be decoded"
            - Source: [Joint, RNCacheStateMap]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. REQ.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_OPCODE_NOT_DECODED = (
                "DENIED_REQ_OPCODE_NOT_DECODED",
                Xact::XactDenial::DENIED_REQ_OPCODE_NOT_DECODED,
                [](auto, SourceEnum source) -> std::string { return "Denied unrecognized REQ opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            return std::format("REQ Opcode {:#x} could not be decoded",
                                uint64_t(subject.obj.firedFlitRequest->flit.req.Opcode()));
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
            DENIED_SNP_OPCODE_NOT_DECODED
            - Title Message: "Denied unrecognized SNP opcode"
            - Further Message: "SNP Opcode {SNP.Opcode} could not be decoded"
            - Source: [Joint, RNCacheStateMap]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.Opcode
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_OPCODE_NOT_DECODED = (
                "DENIED_SNP_OPCODE_NOT_DECODED",
                Xact::XactDenial::DENIED_SNP_OPCODE_NOT_DECODED,
                [](auto, SourceEnum source) -> std::string { return "Denied unrecognized SNP opcode"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            return std::format("SNP Opcode {:#x} could not be decoded",
                                uint64_t(subject.obj.firedFlitRequest->flit.snp.Opcode()));
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::Opcode };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_REQ_NOT_TO_HN
            - Title Message: "Denied REQ flit target node, expecting HN"
            - Further Message: "Unexpected target node: {REQ.TgtID} ({TOPOLOGY[REQ.TgtID]}), expecting HN"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. REQ.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_NOT_TO_HN = (
                "DENIED_REQ_NOT_TO_HN",
                Xact::XactDenial::DENIED_REQ_NOT_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied REQ flit target node, expecting HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            uint64_t tgtId = subject.obj.firedFlitRequest->flit.req.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting HN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_REQ_NOT_FROM_RN_TO_HN
            - Title Message: "Denied REQ flit source node or target node, expecting RN to HN"
            - Further Message: "Unexpected source node: {REQ.SrcID} ({TOPOLOGY[REQ.SrcID]}), or target node: {REQ.TgtID} ({TOPOLOGY[REQ.TgtID]}), expecting from RN to HN"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. REQ.SrcID
                           2. REQ.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_NOT_FROM_RN_TO_HN = (
                "DENIED_REQ_NOT_FROM_RN_TO_HN",
                Xact::XactDenial::DENIED_REQ_NOT_FROM_RN_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied REQ flit source node or target node, expecting RN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            srcId = subject.obj.firedFlitRequest->flit.req.SrcID();
                            tgtId = subject.obj.firedFlitRequest->flit.req.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from RN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::SrcID, Flit::Keys::REQ::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_REQ_NOT_FROM_HN_TO_SN
            - Title Message: "Denied REQ flit source node or target node, expecting HN to SN"
            - Further Message: "Unexpected source node: {REQ.SrcID} ({TOPOLOGY[REQ.SrcID]}), or target node: {REQ.TgtID} ({TOPOLOGY[REQ.TgtID]}), expecting from HN to SN"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. REQ.SrcID
                           2. REQ.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_REQ_NOT_FROM_HN_TO_SN = (
                "DENIED_REQ_NOT_FROM_HN_TO_SN",
                Xact::XactDenial::DENIED_REQ_NOT_FROM_HN_TO_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied REQ flit source node or target node, expecting HN to SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsREQ()) {
                            srcId = subject.obj.firedFlitRequest->flit.req.SrcID();
                            tgtId = subject.obj.firedFlitRequest->flit.req.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from HN to SN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::SrcID, Flit::Keys::REQ::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_TO_RN
            - Title Message: "Denied RSP flit target node, expecting RN"
            - Further Message: "Unexpected target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_TO_RN = (
                "DENIED_RSP_NOT_TO_RN",
                Xact::XactDenial::DENIED_RSP_NOT_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit target node, expecting RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting RN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_FROM_HN_TO_RN
            - Title Message: "Denied RSP flit source node or target node, expecting HN to RN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from HN to RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_HN_TO_RN = (
                "DENIED_RSP_NOT_FROM_HN_TO_RN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_HN_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node or target node, expecting HN to RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from HN to RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_TO_HN
            - Title Message: "Denied RSP flit target node, expecting HN"
            - Further Message: "Unexpected target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_TO_HN = (
                "DENIED_RSP_NOT_TO_HN",
                Xact::XactDenial::DENIED_RSP_NOT_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit target node, expecting HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting HN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_FROM_RN_TO_HN
            - Title Message: "Denied RSP flit source node or target node, expecting RN to HN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from RN to HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_RN_TO_HN = (
                "DENIED_RSP_NOT_FROM_RN_TO_HN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_RN_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node or target node, expecting RN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from RN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_FROM_SN
            - Title Message: "Denied RSP flit source node, expecting SN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), expecting SN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_SN = (
                "DENIED_RSP_NOT_FROM_SN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node, expecting SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            return std::format("Unexpected source node: {:#x} ({}), expecting SN",
                                srcId,
                                glbl.TOPOLOGY.GetNode(srcId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );
            
            /*
            DENIED_RSP_NOT_FROM_SN_TO_HN
            - Title Message: "Denied RSP flit source node or target node, expecting SN to HN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from SN to HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_SN_TO_HN = (
                "DENIED_RSP_NOT_FROM_SN_TO_HN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node or target node, expecting SN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from SN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_FROM_SN_TO_RN
            - Title Message: "Denied RSP flit source node or target node, expecting SN to RN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from SN to RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_SN_TO_RN = (
                "DENIED_RSP_NOT_FROM_SN_TO_RN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_SN_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node or target node, expecting SN to RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from SN to RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN
            - Title Message: "Denied RSP flit source node or target node, expecting SN to HN or RN"
            - Further Message: "Unexpected source node: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from SN to HN or RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN = (
                "DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN",
                Xact::XactDenial::DENIED_RSP_NOT_FROM_SN_TO_HN_OR_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit source node or target node, expecting SN to HN or RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from SN to HN or RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_NOT_TO_SN
            - Title Message: "Denied RSP flit target node, expecting SN"
            - Further Message: "Unexpected target node: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting SN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_NOT_TO_SN = (
                "DENIED_RSP_NOT_TO_SN",
                Xact::XactDenial::DENIED_RSP_NOT_TO_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied RSP flit target node, expecting SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting SN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_TO_RN
            - Title Message: "Denied DAT flit target node, expecting RN"
            - Further Message: "Unexpected target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_TO_RN = (
                "DENIED_DAT_NOT_TO_RN",
                Xact::XactDenial::DENIED_DAT_NOT_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit target node, expecting RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting RN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_FROM_HN_TO_RN
            - Title Message: "Denied DAT flit source node or target node, expecting HN to RN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), or target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting from HN to RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
                           2. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_HN_TO_RN = (
                "DENIED_DAT_NOT_FROM_HN_TO_RN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_HN_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node or target node, expecting HN to RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from HN to RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID, Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_FROM_HN_TO_SN
            - Title Message: "Denied DAT flit source node or target node, expecting HN to SN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), or target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting from HN to SN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
                           2. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_HN_TO_SN = (
                "DENIED_DAT_NOT_FROM_HN_TO_SN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_HN_TO_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node or target node, expecting HN to SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from HN to SN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID, Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_TO_HN
            - Title Message: "Denied DAT flit target node, expecting HN"
            - Further Message: "Unexpected target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_TO_HN = (
                "DENIED_DAT_NOT_TO_HN",
                Xact::XactDenial::DENIED_DAT_NOT_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit target node, expecting HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting HN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_FROM_RN_TO_HN
            - Title Message: "Denied DAT flit source node or target node, expecting RN to HN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), or target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting from RN to HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
                           2. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_RN_TO_HN = (
                "DENIED_DAT_NOT_FROM_RN_TO_HN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_RN_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node or target node, expecting RN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from RN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID, Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_TO_SN
            - Title Message: "Denied DAT flit target node, expecting SN"
            - Further Message: "Unexpected target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting SN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_TO_SN = (
                "DENIED_DAT_NOT_TO_SN",
                Xact::XactDenial::DENIED_DAT_NOT_TO_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit target node, expecting SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting SN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );
            
            /*
            DENIED_DAT_NOT_FROM_SN
            - Title Message: "Denied DAT flit source node, expecting SN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), expecting SN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_SN = (
                "DENIED_DAT_NOT_FROM_SN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_SN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node, expecting SN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            return std::format("Unexpected source node: {:#x} ({}), expecting SN",
                                srcId,
                                glbl.TOPOLOGY.GetNode(srcId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_FROM_SN_TO_HN
            - Title Message: "Denied DAT flit source node or target node, expecting SN to HN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), or target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting from SN to HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
                           2. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_SN_TO_HN = (
                "DENIED_DAT_NOT_FROM_SN_TO_HN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_SN_TO_HN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node or target node, expecting SN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from SN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID, Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_TO_HN_OR_RN
            - Title Message: "Denied DAT flit target node, expecting HN or RN"
            - Further Message: "Unexpected target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting HN or RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_TO_HN_OR_RN = (
                "DENIED_DAT_NOT_TO_HN_OR_RN",
                Xact::XactDenial::DENIED_DAT_NOT_TO_HN_OR_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit target node, expecting HN or RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            uint64_t tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting HN or RN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_DAT_NOT_FROM_RN_TO_RN
            - Title Message: "Denied DAT flit source node or target node, expecting RN to RN"
            - Further Message: "Unexpected source node: {DAT.SrcID} ({TOPOLOGY[DAT.SrcID]}), or target node: {DAT.TgtID} ({TOPOLOGY[DAT.TgtID]}), expecting from RN to RN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.SrcID
                           2. DAT.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_NOT_FROM_RN_TO_RN = (
                "DENIED_DAT_NOT_FROM_RN_TO_RN",
                Xact::XactDenial::DENIED_DAT_NOT_FROM_RN_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied DAT flit source node or target node, expecting RN to RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            srcId = subject.obj.firedFlitResponse->flit.dat.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.dat.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from RN to RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::SrcID, Flit::Keys::DAT::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_SNP_NOT_TO_RN (Not utilized, snoop flits do not contain TgtID, reserved for future use)
            - Title Message: "Denied SNP flit target node, expecting RN"
            - Further Message: "Unexpected target node: {SNP.TgtID} ({TOPOLOGY[SNP.TgtID]}), expecting RN"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.TgtID
            *//*
            inline static constexpr ExplanationBack<config> DENIED_SNP_NOT_TO_RN = (
                "DENIED_SNP_NOT_TO_RN",
                Xact::XactDenial::DENIED_SNP_NOT_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied SNP flit target node, expecting RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            uint64_t tgtId = subject.obj.firedFlitRequest->flit.snp.TgtID();
                            return std::format("Unexpected target node: {:#x} ({}), expecting RN",
                                tgtId,
                                glbl.TOPOLOGY.GetNode(tgtId).type.name);
                        }
                    }
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );
            */

            /*
            DENIED_SNP_NOT_FROM_HN_TO_RN
            - Title Message: "Denied SNP flit source node or target node, expecting HN to RN"
            - Further Message: "Unexpected source node: {SNP.SrcID} ({TOPOLOGY[SNP.SrcID]}), or target node: {SNP.TgtID} ({TOPOLOGY[SNP.TgtID]}), expecting from HN to RN"
            - Source: [Joint, Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.SrcID
                           2. SNP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_NOT_FROM_HN_TO_RN = (
                "DENIED_SNP_NOT_FROM_HN_TO_RN",
                Xact::XactDenial::DENIED_SNP_NOT_FROM_HN_TO_RN,
                [](auto, SourceEnum source) -> std::string { return "Denied SNP flit source node or target node, expecting HN to RN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            srcId = subject.obj.firedFlitRequest->flit.snp.SrcID();
                            tgtId = subject.obj.firedFlitRequest->nodeId;
                            break;
                        }
                    }
                    return std::format("Unexpected source node: {:#x} ({}), or target node: {:#x} ({}), expecting from HN to RN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::SrcID, Flit::Keys::SNP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_RSP_RETRYACK_ROUTE
            - Title Message: "Denied RetryAck flit route, expecting HN to RN or SN to HN"
            - Further Message: "Unexpected source: {RSP.SrcID} ({TOPOLOGY[RSP.SrcID]}), or target: {RSP.TgtID} ({TOPOLOGY[RSP.TgtID]}), expecting from HN to RN or SN to HN"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.SrcID
                           2. RSP.TgtID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_RETRYACK_ROUTE = (
                "DENIED_RSP_RETRYACK_ROUTE",
                Xact::XactDenial::DENIED_RSP_RETRYACK_ROUTE,
                [](auto, SourceEnum source) -> std::string { return "Denied RetryAck flit route, expecting HN to RN or SN to HN"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, tgtId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP() && subject.obj.firedFlitResponse->flit.rsp.IsRetryAck()) {
                            srcId = subject.obj.firedFlitResponse->flit.rsp.SrcID();
                            tgtId = subject.obj.firedFlitResponse->flit.rsp.TgtID();
                            break;
                        }
                    }
                    return std::format("Unexpected source: {:#x} ({}), or target: {:#x} ({}), expecting from HN to RN or SN to HN",
                        srcId,
                        glbl.TOPOLOGY.GetNode(srcId).type.name,
                        tgtId,
                        glbl.TOPOLOGY.GetNode(tgtId).type.name);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::SrcID, Flit::Keys::RSP::TgtID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return {};
                }
            );

            /*
            DENIED_SRCID_MISMATCH
            - Title Message: "Denied SrcID, not identical to previous flit"
            - Further Message: "Unexpected SrcID: {SNP.SrcID}, not identical to previous SrcID: {complementary SNP.SrcID}"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.SrcID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.SrcID
            */
            inline static constexpr ExplanationBack<config> DENIED_SRCID_MISMATCH = (
                "DENIED_SRCID_MISMATCH",
                Xact::XactDenial::DENIED_SRCID_MISMATCH,
                [](auto, SourceEnum source) -> std::string { return "Denied SrcID, not identical to previous flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t srcId = 0, complementSrcId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            srcId = subject.obj.firedFlitRequest->flit.snp.SrcID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            complementSrcId = complement.obj.firedFlitRequest->flit.snp.SrcID();
                            break;
                        }
                    }
                    return std::format("Unexpected SrcID: {:#x}, not identical to previous flit SrcID: {:#x}",
                        srcId,
                        complementSrcId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::SrcID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::SrcID };
                }
            );

            /*
            DENIED_PGROUPID_MISMATCH
            - Title Message: "Denied PGroupID, not identical to previous flit"
            - Further Message: "Unexpected PGroupID: {RSP.PGroupID}, not identical to previous PGroupID: {complementary REQ.PGroupID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.PGroupID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. REQ.PGroupID
            */
            inline static constexpr ExplanationBack<config> DENIED_PGROUPID_MISMATCH = (
                "DENIED_PGROUPID_MISMATCH",
                Xact::XactDenial::DENIED_PGROUPID_MISMATCH,
                [](auto, SourceEnum source) -> std::string { return "Denied PGroupID, not identical to previous flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t pGroupId = 0, complementPGroupId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            pGroupId = subject.obj.firedFlitResponse->flit.rsp.PGroupID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsREQ()) {
                            complementPGroupId = complement.obj.firedFlitRequest->flit.req.PGroupID();
                            break;
                        }
                    }
                    return std::format("Unexpected PGroupID: {:#x}, not identical to previous flit PGroupID: {:#x}",
                        pGroupId,
                        complementPGroupId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::PGroupID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::PGroupID };
                }
            );

            /*
            DENIED_RSP_TXNID_MISMATCHING_REQ
            - Title Message: "Denied TxnID in RSP flit, not identical to REQ flit"
            - Further Message: "Unexpected RSP TxnID: {RSP.TxnID}, expecting: {REQ.TxnID} from REQ flit"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. REQ.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_TXNID_MISMATCHING_REQ = (
                "DENIED_RSP_TXNID_MISMATCHING_REQ",
                Xact::XactDenial::DENIED_RSP_TXNID_MISMATCHING_REQ,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in RSP flit, not identical to REQ flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t rspTxnId = 0, reqTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            rspTxnId = subject.obj.firedFlitResponse->flit.rsp.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsREQ()) {
                            reqTxnId = complement.obj.firedFlitRequest->flit.req.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected RSP TxnID: {:#x}, expecting: {:#x} from REQ flit",
                        rspTxnId,
                        reqTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::TxnID };
                }
            );

            /*
            DENIED_DAT_TXNID_MISMATCHING_REQ
            - Title Message: "Denied TxnID in DAT flit, not identical to REQ flit"
            - Further Message: "Unexpected DAT TxnID: {DAT.TxnID}, expecting: {REQ.TxnID} from REQ flit"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. REQ.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_MISMATCHING_REQ = (
                "DENIED_DAT_TXNID_MISMATCHING_REQ",
                Xact::XactDenial::DENIED_DAT_TXNID_MISMATCHING_REQ,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in DAT flit, not identical to REQ flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datTxnId = 0, reqTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datTxnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsREQ()) {
                            reqTxnId = complement.obj.firedFlitRequest->flit.req.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected DAT TxnID: {:#x}, expecting: {:#x} from REQ flit",
                        datTxnId,
                        reqTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::TxnID };
                }
            );

            /*
            DENIED_RSP_TXNID_MISMATCHING_DBID
            - Title Message: "Denied TxnID in RSP flit, not identical to DBID sourced from RSP or DAT flit"
            - Further Message: "Unexpected RSP TxnID: {RSP.TxnID}, not identical to DBID-sourced TxnID: {complementary RSP.TxnID/DAT.TxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TxnID
            - Complement: [FiredResponseFlit]
            - Complement Key: 1. RSP.TxnID
                              2. DAT.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_TXNID_MISMATCHING_DBID = (
                "DENIED_RSP_TXNID_MISMATCHING_DBID",
                Xact::XactDenial::DENIED_RSP_TXNID_MISMATCHING_DBID,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in RSP flit, not identical to DBID sourced from RSP or DAT flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t rspTxnId = 0, dbidSourcedTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            rspTxnId = subject.obj.firedFlitResponse->flit.rsp.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_RESPONSE && complement.obj.firedFlitResponse->flit.IsRSP()) {
                            dbidSourcedTxnId = complement.obj.firedFlitResponse->flit.rsp.TxnID();
                            break;
                        }
                        else if (complement.type == ObjectType::FIRED_FLIT_RESPONSE && complement.obj.firedFlitResponse->flit.IsDAT()) {
                            dbidSourcedTxnId = complement.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected RSP TxnID: {:#x}, not identical to DBID-sourced TxnID: {:#x}",
                        rspTxnId,
                        dbidSourcedTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID, Flit::Keys::DAT::TxnID };
                }
            );

            /*
            DENIED_DAT_TXNID_MISMATCHING_DBID
            - Title Message: "Denied TxnID in DAT flit, not identical to DBID sourced from RSP or DAT flit"
            - Further Message: "Unexpected DAT TxnID: {DAT.TxnID}, not identical to DBID-sourced TxnID: {complementary RSP.TxnID/DAT.TxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            - Complement: [FiredResponseFlit]
            - Complement Key: 1. RSP.TxnID
                              2. DAT.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_MISMATCHING_DBID = (
                "DENIED_DAT_TXNID_MISMATCHING_DBID",
                Xact::XactDenial::DENIED_DAT_TXNID_MISMATCHING_DBID,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in DAT flit, not identical to DBID sourced from RSP or DAT flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datTxnId = 0, dbidSourcedTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datTxnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_RESPONSE && complement.obj.firedFlitResponse->flit.IsRSP()) {
                            dbidSourcedTxnId = complement.obj.firedFlitResponse->flit.rsp.TxnID();
                            break;
                        }
                        else if (complement.type == ObjectType::FIRED_FLIT_RESPONSE && complement.obj.firedFlitResponse->flit.IsDAT()) {
                            dbidSourcedTxnId = complement.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected DAT TxnID: {:#x}, not identical to DBID-sourced TxnID: {:#x}",
                        datTxnId,
                        dbidSourcedTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID, Flit::Keys::DAT::TxnID };
                }
            );

            /*
            DENIED_RSP_TXNID_MISMATCHING_SNP
            - Title Message: "Denied TxnID in RSP flit, not identical to SNP flit"
            - Further Message: "Unexpected RSP TxnID: {RSP.TxnID}, not identical to SNP flit TxnID: {complementary SNP.TxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. RSP.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_RSP_TXNID_MISMATCHING_SNP = (
                "DENIED_RSP_TXNID_MISMATCHING_SNP",
                Xact::XactDenial::DENIED_RSP_TXNID_MISMATCHING_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in RSP flit, not identical to SNP flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t rspTxnId = 0, snpTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsRSP()) {
                            rspTxnId = subject.obj.firedFlitResponse->flit.rsp.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            snpTxnId = complement.obj.firedFlitRequest->flit.snp.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected RSP TxnID: {:#x}, not identical to SNP flit TxnID: {:#x}",
                        rspTxnId,
                        snpTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::RSP::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TxnID };
                }
            );

            /*
            DENIED_DAT_TXNID_MISMATCHING_SNP
            - Title Message: "Denied TxnID in DAT flit, not identical to SNP flit"
            - Further Message: "Unexpected DAT TxnID: {DAT.TxnID}, not identical to SNP flit TxnID: {complementary SNP.TxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_MISMATCHING_SNP = (
                "DENIED_DAT_TXNID_MISMATCHING_SNP",
                Xact::XactDenial::DENIED_DAT_TXNID_MISMATCHING_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in DAT flit, not identical to SNP flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datTxnId = 0, snpTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datTxnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            snpTxnId = complement.obj.firedFlitRequest->flit.snp.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected DAT TxnID: {:#x}, not identical to SNP flit TxnID: {:#x}",
                        datTxnId,
                        snpTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TxnID };
                }
            );

            /*
            DENIED_SNP_TXNID_MISMATCHING_SNP
            - Title Message: "Denied TxnID in SNP flit, not identical to previous SNP flit"
            - Further Message: "Unexpected SNP TxnID: {SNP.TxnID}, not identical to previous SNP flit TxnID: {complementary SNP.TxnID}"
            - Source: [Xaction]
            - Subject: [FiredRequestFlit]
            - Subject Key: 1. SNP.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.TxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_SNP_TXNID_MISMATCHING_SNP = (
                "DENIED_SNP_TXNID_MISMATCHING_SNP",
                Xact::XactDenial::DENIED_SNP_TXNID_MISMATCHING_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in SNP flit, not identical to previous SNP flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t snpTxnId = 0, complementSnpTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_REQUEST && subject.obj.firedFlitRequest->flit.IsSNP()) {
                            snpTxnId = subject.obj.firedFlitRequest->flit.snp.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            complementSnpTxnId = complement.obj.firedFlitRequest->flit.snp.TxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected SNP TxnID: {:#x}, not identical to previous SNP flit TxnID: {:#x}",
                        snpTxnId,
                        complementSnpTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::TxnID };
                }
            );

            /*
            DENIED_DAT_TXNID_MISMATCHING_DMT
            - Title Message: "Denied TxnID in DAT flit under DMT, not identical to DMT ReturnTxnID"
            - Further Message: "Unexpected DMT DAT TxnID: {DAT.TxnID}, not identical to DMT REQ ReturnTxnID: {complementary REQ.ReturnTxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. REQ.ReturnTxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_MISMATCHING_DMT = (
                "DENIED_DAT_TXNID_MISMATCHING_DMT",
                Xact::XactDenial::DENIED_DAT_TXNID_MISMATCHING_DMT,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in DAT flit under DMT, not identical to DMT ReturnTxnID-sourced TxnID"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datTxnId = 0, dmtReturnTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datTxnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsREQ()) {
                            dmtReturnTxnId = complement.obj.firedFlitRequest->flit.req.ReturnTxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected DMT DAT TxnID: {:#x}, not identical to DMT REQ ReturnTxnID: {:#x}",
                        datTxnId,
                        dmtReturnTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::REQ::ReturnTxnID };
                }
            );

            /*
            DENIED_DAT_TXNID_MISMATCHING_DCT
            - Title Message: "Denied TxnID in DAT flit under DCT, not identical to DCT FwdTxnID"
            - Further Message: "Unexpected DCT DAT TxnID: {DAT.TxnID}, not identical to DCT SNP FwdTxnID: {complementary SNP.FwdTxnID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.TxnID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.FwdTxnID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_TXNID_MISMATCHING_DCT = (
                "DENIED_DAT_TXNID_MISMATCHING_DCT",
                Xact::XactDenial::DENIED_DAT_TXNID_MISMATCHING_DCT,
                [](auto, SourceEnum source) -> std::string { return "Denied TxnID in DAT flit under DCT, not identical to DCT FwdTxnID-sourced TxnID"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datTxnId = 0, dctFwdTxnId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datTxnId = subject.obj.firedFlitResponse->flit.dat.TxnID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            dctFwdTxnId = complement.obj.firedFlitRequest->flit.snp.FwdTxnID();
                            break;
                        }
                    }
                    return std::format("Unexpected DCT DAT TxnID: {:#x}, not identical to DCT SNP FwdTxnID: {:#x}",
                        datTxnId,
                        dctFwdTxnId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::TxnID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::FwdTxnID };
                }
            );

            /*
            DENIED_DAT_HOMENID_MISMATCHING_SNP
            - Title Message: "Denied HomeNID in DCT DAT flit, not identical to SNP flit"
            - Further Message: "Unexpected DCT DAT HomeNID: {DAT.HomeNID}, not identical to SNP flit SrcID: {complementary SNP.SrcID}"
            - Source: [Xaction]
            - Subject: [FiredResponseFlit]
            - Subject Key: 1. DAT.HomeNID
            - Complement: [FiredRequestFlit]
            - Complement Key: 1. SNP.SrcID
            */
            inline static constexpr ExplanationBack<config> DENIED_DAT_HOMENID_MISMATCHING_SNP = (
                "DENIED_DAT_HOMENID_MISMATCHING_SNP",
                Xact::XactDenial::DENIED_DAT_HOMENID_MISMATCHING_SNP,
                [](auto, SourceEnum source) -> std::string { return "Denied HomeNID in DCT DAT flit, not identical to SNP flit"; },
                [](const Xact::Global<config>& glbl, SourceEnum source, auto, const Objects<config>& subjects, const Objects<config>& complements) -> std::string {
                    uint64_t datHomeNid = 0, snpSrcId = 0;
                    for (const Object<config>& subject : subjects) {
                        if (subject.type == ObjectType::FIRED_FLIT_RESPONSE && subject.obj.firedFlitResponse->flit.IsDAT()) {
                            datHomeNid = subject.obj.firedFlitResponse->flit.dat.HomeNID();
                            break;
                        }
                    }
                    for (const Object<config>& complement : complements) {
                        if (complement.type == ObjectType::FIRED_FLIT_REQUEST && complement.obj.firedFlitRequest->flit.IsSNP()) {
                            snpSrcId = complement.obj.firedFlitRequest->flit.snp.SrcID();
                            break;
                        }
                    }
                    return std::format("Unexpected DCT DAT HomeNID: {:#x}, not identical to SNP flit SrcID: {:#x}",
                        datHomeNid,
                        snpSrcId);
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::DAT::HomeNID };
                },
                [](const Object<config>& obj) -> std::vector<Flit::Key> {
                    return { Flit::Keys::SNP::SrcID };
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
