/*
* Value Definitions.
*/
`define CLOG_ISSUE_B                    0
`define CLOG_ISSUE_E                    3

`define CLOG_NODE_TYPE_RN_F             1
`define CLOG_NODE_TYPE_RN_D             2
`define CLOG_NODE_TYPE_RN_I             3
`define CLOG_NODE_TYPE_HN_F             5
`define CLOG_NODE_TYPE_HN_I             7
`define CLOG_NODE_TYPE_SN_F             9
`define CLOG_NODE_TYPE_SN_I             11
`define CLOG_NODE_TYPE_MN               12

`define CLOG_CHANNEL_TXREQ              0
`define CLOG_CHANNEL_TXRSP              1
`define CLOG_CHANNEL_TXDAT              2
`define CLOG_CHANNEL_TXSNP              3
`define CLOG_CHANNEL_RXREQ              4
`define CLOG_CHANNEL_RXRSP              5
`define CLOG_CHANNEL_RXDAT              6
`define CLOG_CHANNEL_RXSNP              7


/*
* File handle operations.
*/
import "DPI-C" function chandle CLogT_OpenFile (
    input   string              path,
    output  int                 status
);

import "DPI-C" function void CLogT_CloseFile (
    input   chandle             handle
);

/*
* CLog.T operations.
*/
import "DPI-C" function void CLogT_WriteSegmentParamBegin (
    input   chandle             handle
);

import "DPI-C" function void CLogT_WriteSegmentParamEnd (
    input   chandle             handle
);

import "DPI-C" function void CLogT_WriteSegmentTopoBegin (
    input   chandle             handle
);

import "DPI-C" function void CLogT_WriteSegmentTopoEnd (
    input   chandle             handle
);

import "DPI-C" function void CLogT_WriteIssue (
    input   chandle             handle,
    input   int                 issue
);

import "DPI-C" function void CLogT_WriteWidthNodeID (
    input   chandle             handle,
    input   int                 nodeIdWidth
);

import "DPI-C" function void CLogT_WriteWidthAddr (
    input   chandle             handle,
    input   int                 addrWidth
);

import "DPI-C" function void CLogT_WriteWidthReqRSVDC (
    input   chandle             handle,
    input   int                 reqRsvdcWidth
);

import "DPI-C" function void CLogT_WriteWidthDatRSVDC (
    input   chandle             handle,
    input   int                 datRsvdcWidth
);

import "DPI-C" function void CLogT_WriteWidthData (
    input   chandle             handle,
    input   int                 dataWidth
);

import "DPI-C" function void CLogT_WriteEnableDataCheck (
    input   chandle             handle,
    input   int                 dataCheckPresent
);

import "DPI-C" function void CLogT_WriteEnablePoison (
    input   chandle             handle,
    input   int                 poisonPresent
);

import "DPI-C" function void CLogT_WriteEnableMPAM (
    input   chandle             handle,
    input   int                 mpamPresent
);

import "DPI-C" function void CLogT_WriteTopo (
    input   chandle             handle,
    input   int                 nodeId,
    input   int                 nodeType
);

import "DPI-C" function void CLogT_WriteLog (
    input   chandle             handle,
    input   longint             cycle,
    input   int                 nodeId,
    input   int                 channel,
    input   bit [511:0]         flit,
    input   int                 flitLength
);
