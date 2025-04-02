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
* CLog.B file handle operations.
*/
import "DPI-C" function chandle CLogB_OpenFile (
    input   string              path,
    output  int                 status
);

import "DPI-C" function void CLogB_CloseFile (
    input   chandle             handle
);

import "DPI-C" function void CLogB_ShareHandle (
    input   string              id,
    input   chandle             handle
);

import "DPI-C" function int CLogB_UnshareHandle (
    input   string              id
);

import "DPI-C" function chandle CLogB_GetSharedHandle (
    input   string              id
);


/*
* CLog.B parameters write operations.
*/
import "DPI-C" function void CLogB_WriteParameters (
    input   chandle             handle,
    input   int                 issue,
    input   int                 nodeIdWidth,
    input   int                 addrWidth,
    input   int                 reqRsvdcWidth,
    input   int                 datRsvdcWidth,
    input   int                 dataWidth,
    input   int                 dataCheckPresent,
    input   int                 poisonPresent,
    input   int                 mpamPresent
);

import "DPI-C" function void CLogB_SharedWriteParameters (
    input   string              id,
    input   int                 issue,
    input   int                 nodeIdWidth,
    input   int                 addrWidth,
    input   int                 reqRsvdcWidth,
    input   int                 datRsvdcWidth,
    input   int                 dataWidth,
    input   int                 dataCheckPresent,
    input   int                 poisonPresent,
    input   int                 mpamPresent
);


/*
* CLog.B topologies write operations.
*/
import "DPI-C" function void CLogB_WriteTopo (
    input   chandle             handle,
    input   int                 nodeId,
    input   int                 nodeType
);

import "DPI-C" function void CLogB_WriteTopoEnd (
    input   chandle             handle
);

import "DPI-C" function void CLogB_SharedWriteTopo (
    input   string              id,
    input   int                 nodeId,
    input   int                 nodeType
);

import "DPI-C" function void CLogB_SharedWriteTopoEnd (
    input   string              id
);


/*
* CLog.B log record write operations.
*/
import "DPI-C" function void CLogB_WriteRecord (
    input   chandle             handle,
    input   longint             cycletime,
    input   int                 nodeId,
    input   int                 channel,
    input   bit [511:0]         flit,
    input   int                 flitLength
);

import "DPI-C" function void CLogB_SharedWriteRecord (
    input   string              id,
    input   longint             cycleTime,
    input   int                 nodeId,
    input   int                 channel,
    input   bit [511:0]         flit,
    input   int                 flitLength
);
