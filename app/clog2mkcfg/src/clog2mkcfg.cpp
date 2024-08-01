#include <getopt.h>

#include <iostream>
#include <fstream>
#include <vector>

#include "../../../clog/clog_t.hpp"
#include "../../../clog/clog_t_util.hpp"

//
std::ostream*   output;

//
static std::string ToString(CLog::Issue issue) noexcept
{
    switch (issue)
    {
        case CLog::Issue::B:    return "B";
        case CLog::Issue::Eb:   return "E.b";
        default:                return "";
    }
}

//
static void print_version() noexcept
{
    std::cerr << std::endl;
    std::cerr << "                CHIron Toolset" << std::endl;
    std::cerr << std::endl;
    std::cerr << "           xsdb2clog - " << __DATE__ << std::endl;
    std::cerr << std::endl;
    std::cerr << "  clog2mkcfg: CLog to CHIron Makefile configuration extractor" << std::endl;
    std::cerr << std::endl;
}

static void print_help() noexcept
{
    std::cout << "Usage: clog2mkcfg [OPTION...]" << std::endl;
    std::cout << std::endl;
    std::cout << "  -h, --help              print help info" << std::endl;
    std::cout << "  -v, --version           print version info" << std::endl;
    std::cout << "  -o, --output=FILE       specify makefile output. by default, the makefile output" << std::endl;
    std::cout << "                          is written to the standard console output" << std::endl;
    std::cout << "  -T, --clogT             *default* specify output format as CLog.T" << std::endl;
    std::cout << "  -B, --clogB             [WIP] specify output format as CLog.B" << std::endl;
    std::cout << "  -Z, --clogBz            [WIP] specify output format as CLog.Bz" << std::endl;
    std::cout << "  -f, --param-file=FILE   specify CLog input files for CHI parameters" << std::endl;
    std::cout << "  -s, --seg, --segment    enable segment mode for CLog input files for CHI parameters" << std::endl;
    std::cout << std::endl;
}

int main(int argc, char* argv[])
{
    //
    std::string                 outputFile;

    std::vector<std::string>    clogParamFiles;
    bool                        clogParamFilesSegmentMode = false;

    // input parameters
    const struct option long_options[] = {
        { "help"            , 0, NULL, 'h' },
        { "version"         , 0, NULL, 'v' },
        { "output"          , 1, NULL, 'o' },
        { "clogT"           , 0, NULL, 'T' },
        { "clogB"           , 0, NULL, 'B' },
        { "clogBz"          , 0, NULL, 'Z' },
        { "segment"         , 0, NULL, 's' },
        { "seg"             , 0, NULL, 's' },
        { "param-file"      , 1, NULL, 'f' },
        { 0                 , 0, NULL,  0  }
    };

    int long_index = 0;
    int o;
    while ((o = getopt_long(argc, argv, "hvo:TBZsf:", long_options, &long_index)) != -1)
    {
        switch (o)
        {
            case 0:
                switch (long_index)
                {
                    default:
                        print_help();
                        return 1;
                }

            case 'h':
                print_help();
                return 0;

            case 'v':
                print_version();
                return 0;

            case 'o':
                outputFile = optarg;
                break;

            case 'T':
                break;

            case 'B':
                std::cerr << "$ERROR: format CLog.B not supported currently, hold tight on future versions!" << std::endl;
                return 1;

            case 'Z':
                std::cerr << "$ERROR: format CLog.Bz not supported currently, hold tight on future versions!" << std::endl;
                return 1;

            case 's':
                clogParamFilesSegmentMode = true;
                break;

            case 'f':
                clogParamFiles.push_back(std::string(optarg));
                break;

            default:
                print_help();
                return 1;
        }
    }

    //
    print_version();

    /* Extract parameters from param-file(s) */
    CLog::CLogT::Parser<>           parser;

    CLog::Parameters                params;
    CLog::CLogT::ParametersSerDes<> paramsSerDes;

    parser.SetStopOnSegmentEnd(true);
    parser.SetSegmentMode(clogParamFilesSegmentMode);

    paramsSerDes.SetParametersReference(&params);
    paramsSerDes.RegisterAsDeserializer(parser);
    paramsSerDes.ApplySegmentToken(parser);
    paramsSerDes.ApplySegmentMask(parser);

    for (auto& clogParamFile : clogParamFiles)
    {
        if (clogParamFile.empty())
            continue;

        std::ifstream ifs(clogParamFile);

        if (!ifs)
        {
            std::cerr << "%ERROR: cannot open input param file: " << clogParamFile << std::endl;
            return 3;
        }

        std::cerr << "%INFO: reading params from input param-file: " << clogParamFile;

        if (!parser.Parse(ifs))
        {
            std::cerr << "%ERROR: CLog.T parsing error on file: " << clogParamFile 
                << ", after " << parser.GetExecutionCounter() << " tokens."  << std::endl;
            return -1;
        }

        std::cerr << ": " << parser.GetExecutionCounter() << std::endl;
        parser.SetExecutionCounter(0);
    }

    /* Specify output direction */
    std::ofstream foutput;
    if (outputFile.empty())
    {
        output = &std::cout;
    }
    else
    {
        foutput.open(outputFile.c_str());

        if (!foutput)
        {
            std::cerr << "%ERROR: cannot open output file: " << outputFile << std::endl;
            return 3;
        }

        output = &foutput;

        std::cerr << "%INFO: ready to write into output file: " << outputFile << std::endl;
    }

    //
    foutput << "#" << std::endl;
    foutput << "# The CHI Issue." << std::endl;
    foutput << "#   Permitted value: \"B\", \"E.b\"" << std::endl;
    foutput << "#" << std::endl;
    foutput << "ISSUE ?= " << ToString(params.GetIssue()) << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The width of Node ID." << std::endl;
    foutput << "# 	Permitted value: 7 to 11" << std::endl;
    foutput << "#" << std::endl;
    foutput << "NODEID_WIDTH ?= " << params.GetNodeIdWidth() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The width of address of REQ flit, which also effects the SNP flit." << std::endl;
    foutput << "#	Permitted value: 44 to 52" << std::endl;
    foutput << "#" << std::endl;
    foutput << "REQ_ADDR_WIDTH ?= " << params.GetReqAddrWidth() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The width of RSVDC field of REQ flit." << std::endl;
    foutput << "# 	Permitted value: 0 or 4, 8, 12, 16, 24, 32" << std::endl;
    foutput << "#" << std::endl;
    foutput << "REQ_RSVDC_WIDTH ?= " << params.GetReqRSVDCWidth() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The width of RSVDC field of DAT flit." << std::endl;
    foutput << "#	Permitted value: 0 or 4, 8, 12, 16, 24, 32" << std::endl;
    foutput << "#" << std::endl;
    foutput << "DAT_RSVDC_WIDTH ?= " << params.GetDatRSVDCWidth() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The width of Data field of DAT flit." << std::endl;
    foutput << "#	Permitted value: 128, 256, 512" << std::endl;
    foutput << "#" << std::endl;
    foutput << "DATA_WIDTH ?= " << params.GetDataWidth() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The presence of DataCheck field of DAT flit." << std::endl;
    foutput << "#	Permitted value: true, false, 0, 1" << std::endl;
    foutput << "#" << std::endl;
    foutput << "DATACHECK_PRESENT ?= " << params.IsDataCheckPresent() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The presence of Poison field of DAT flit." << std::endl;
    foutput << "#	Permitted value: true, false, 0, 1" << std::endl;
    foutput << "#" << std::endl;
    foutput << "POISON_PRESENT ?= " << params.IsPoisonPresent() << std::endl;
    foutput << std::endl;

    foutput << "#" << std::endl;
    foutput << "# The presence of MPAM field." << std::endl;
    foutput << "# *Only applicable to Issue E.b*" << std::endl;
    foutput << "#	Permitted value: true, false, 0, 1" << std::endl;
    foutput << "#" << std::endl;
    foutput << "MPAM_PRESENT ?= " << params.IsMPAMPresent() << std::endl;
    foutput << std::endl;

    //
    std::cerr << "%INFO: operation done" << std::endl;

    return 0;
}
