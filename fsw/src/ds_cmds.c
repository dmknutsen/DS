/************************************************************************
 * NASA Docket No. GSC-18,917-1, and identified as “CFS Data Storage
 * (DS) application version 2.6.1”
 *
 * Copyright (c) 2021 United States Government as represented by the
 * Administrator of the National Aeronautics and Space Administration.
 * All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ************************************************************************/

/**
 * @file
 *  CFS Data Storage (DS) command handler functions
 */

#include "cfe.h"

#include "ds_platform_cfg.h"
#include "ds_verify.h"

#include "ds_appdefs.h"
#include "ds_msgids.h"

#include "ds_msg.h"
#include "ds_app.h"
#include "ds_cmds.h"
#include "ds_file.h"
#include "ds_table.h"
#include "ds_events.h"
#include "ds_version.h"

#include <stdio.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* NOOP command                                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdNoop(const CFE_SB_Buffer_t *BufPtr)
{
    size_t ActualLength   = 0;
    size_t ExpectedLength = sizeof(DS_NoopCmd_t);

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_NOOP_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid NOOP command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else
    {
        /*
        ** Do nothing except display "aliveness" event...
        */
        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_NOOP_CMD_EID, CFE_EVS_EventType_INFORMATION, "NOOP command, Version %d.%d.%d.%d",
                          DS_MAJOR_VERSION, DS_MINOR_VERSION, DS_REVISION, DS_MISSION_REV);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Reset hk telemetry counters command                             */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdReset(const CFE_SB_Buffer_t *BufPtr)
{
    size_t ActualLength   = 0;
    size_t ExpectedLength = sizeof(DS_ResetCmd_t);

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_RESET_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid RESET command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else
    {
        /*
        ** Reset application command counters...
        */
        DS_AppData.CmdAcceptedCounter = 0;
        DS_AppData.CmdRejectedCounter = 0;

        /*
        ** Reset packet storage counters...
        */
        DS_AppData.DisabledPktCounter = 0;
        DS_AppData.IgnoredPktCounter  = 0;
        DS_AppData.FilteredPktCounter = 0;
        DS_AppData.PassedPktCounter   = 0;

        /*
        ** Reset file I/O counters...
        */
        DS_AppData.FileWriteCounter     = 0;
        DS_AppData.FileWriteErrCounter  = 0;
        DS_AppData.FileUpdateCounter    = 0;
        DS_AppData.FileUpdateErrCounter = 0;

        /*
        ** Reset configuration table counters...
        */
        DS_AppData.DestTblLoadCounter   = 0;
        DS_AppData.DestTblErrCounter    = 0;
        DS_AppData.FilterTblLoadCounter = 0;
        DS_AppData.FilterTblErrCounter  = 0;

        CFE_EVS_SendEvent(DS_RESET_CMD_EID, CFE_EVS_EventType_DEBUG, "Reset counters command");
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set application ena/dis state                                   */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetAppState(const CFE_SB_Buffer_t *BufPtr)
{
    DS_AppStateCmd_t *DS_AppStateCmd = (DS_AppStateCmd_t *)BufPtr;
    size_t            ActualLength   = 0;
    size_t            ExpectedLength = sizeof(DS_AppStateCmd_t);

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ENADIS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid APP STATE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyState(DS_AppStateCmd->EnableState) == false)
    {
        /*
        ** Invalid enable/disable state...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ENADIS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid APP STATE command arg: app state = %d", DS_AppStateCmd->EnableState);
    }
    else
    {
        /*
        ** Set new DS application enable/disable state...
        */
        DS_AppData.AppEnableState = DS_AppStateCmd->EnableState;

        /*
        ** Update the Critical Data Store (CDS)...
        */
        DS_TableUpdateCDS();

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_ENADIS_CMD_EID, CFE_EVS_EventType_DEBUG, "APP STATE command: state = %d",
                          DS_AppStateCmd->EnableState);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set packet filter file index                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetFilterFile(const CFE_SB_Buffer_t *BufPtr)
{
    DS_FilterFileCmd_t *DS_FilterFileCmd = (DS_FilterFileCmd_t *)BufPtr;
    size_t              ActualLength     = 0;
    size_t              ExpectedLength   = sizeof(DS_FilterFileCmd_t);
    DS_PacketEntry_t *  pPacketEntry     = NULL;
    DS_FilterParms_t *  pFilterParms     = NULL;
    int32               FilterTableIndex = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER FILE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (!CFE_SB_IsValidMsgId(DS_FilterFileCmd->MessageID))
    {
        /*
        ** Invalid packet messageID...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER FILE command arg: invalid messageID = 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_FilterFileCmd->MessageID));
    }
    else if (DS_FilterFileCmd->FilterParmsIndex >= DS_FILTERS_PER_PACKET)
    {
        /*
        ** Invalid packet filter parameters index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER FILE command arg: filter parameters index = %d",
                          DS_FilterFileCmd->FilterParmsIndex);
    }
    else if (DS_TableVerifyFileIndex(DS_FilterFileCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER FILE command arg: file table index = %d", DS_FilterFileCmd->FileTableIndex);
    }
    else if (DS_AppData.FilterTblPtr == (DS_FilterTable_t *)NULL)
    {
        /*
        ** Must have a valid packet filter table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER FILE command: packet filter table is not loaded");
    }
    else
    {
        /*
        ** Get the index of the filter table entry for this message ID...
        */
        FilterTableIndex = DS_TableFindMsgID(DS_FilterFileCmd->MessageID);

        if (FilterTableIndex == DS_INDEX_NONE)
        {
            /*
            ** Must not create - may only modify existing packet filter...
            */
            DS_AppData.CmdRejectedCounter++;

            CFE_EVS_SendEvent(DS_FILE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid FILTER FILE command: Message ID 0x%08lX is not in filter table",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterFileCmd->MessageID));
        }
        else
        {
            /*
            ** Set new packet filter value (file table index)...
            */
            pPacketEntry = &DS_AppData.FilterTblPtr->Packet[FilterTableIndex];
            pFilterParms = &pPacketEntry->Filter[DS_FilterFileCmd->FilterParmsIndex];

            pFilterParms->FileTableIndex = DS_FilterFileCmd->FileTableIndex;

            /*
            ** Notify cFE that we have modified the table data...
            */
            CFE_TBL_Modified(DS_AppData.FilterTblHandle);

            DS_AppData.CmdAcceptedCounter++;

            CFE_EVS_SendEvent(DS_FILE_CMD_EID, CFE_EVS_EventType_DEBUG,
                              "FILTER FILE command: MID = 0x%08lX, index = %d, filter = %d, file = %d",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterFileCmd->MessageID), (int)FilterTableIndex,
                              DS_FilterFileCmd->FilterParmsIndex, DS_FilterFileCmd->FileTableIndex);
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set pkt filter filename type                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetFilterType(const CFE_SB_Buffer_t *BufPtr)
{
    DS_FilterTypeCmd_t *DS_FilterTypeCmd = (DS_FilterTypeCmd_t *)BufPtr;
    size_t              ActualLength     = 0;
    size_t              ExpectedLength   = sizeof(DS_FilterTypeCmd_t);
    DS_PacketEntry_t *  pPacketEntry     = NULL;
    DS_FilterParms_t *  pFilterParms     = NULL;
    int32               FilterTableIndex = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER TYPE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (!CFE_SB_IsValidMsgId(DS_FilterTypeCmd->MessageID))
    {
        /*
        ** Invalid packet messageID...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER TYPE command arg: invalid messageID = 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_FilterTypeCmd->MessageID));
    }
    else if (DS_FilterTypeCmd->FilterParmsIndex >= DS_FILTERS_PER_PACKET)
    {
        /*
        ** Invalid packet filter parameters index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER TYPE command arg: filter parameters index = %d",
                          DS_FilterTypeCmd->FilterParmsIndex);
    }
    else if (DS_TableVerifyType(DS_FilterTypeCmd->FilterType) == false)
    {
        /*
        ** Invalid packet filter filename type...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER TYPE command arg: filter type = %d", DS_FilterTypeCmd->FilterType);
    }
    else if (DS_AppData.FilterTblPtr == (DS_FilterTable_t *)NULL)
    {
        /*
        ** Must have a valid packet filter table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER TYPE command: packet filter table is not loaded");
    }
    else
    {
        /*
        ** Get the index of the filter table entry for this message ID...
        */
        FilterTableIndex = DS_TableFindMsgID(DS_FilterTypeCmd->MessageID);

        if (FilterTableIndex == DS_INDEX_NONE)
        {
            /*
            ** Must not create - may only modify existing packet filter...
            */
            DS_AppData.CmdRejectedCounter++;

            CFE_EVS_SendEvent(DS_FTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid FILTER TYPE command: Message ID 0x%08lX is not in filter table",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterTypeCmd->MessageID));
        }
        else
        {
            /*
            ** Set new packet filter value (filter type)...
            */
            pPacketEntry = &DS_AppData.FilterTblPtr->Packet[FilterTableIndex];
            pFilterParms = &pPacketEntry->Filter[DS_FilterTypeCmd->FilterParmsIndex];

            pFilterParms->FilterType = DS_FilterTypeCmd->FilterType;

            /*
            ** Notify cFE that we have modified the table data...
            */
            CFE_TBL_Modified(DS_AppData.FilterTblHandle);

            DS_AppData.CmdAcceptedCounter++;

            CFE_EVS_SendEvent(DS_FTYPE_CMD_EID, CFE_EVS_EventType_DEBUG,
                              "FILTER TYPE command: MID = 0x%08lX, index = %d, filter = %d, type = %d",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterTypeCmd->MessageID), (int)FilterTableIndex,
                              DS_FilterTypeCmd->FilterParmsIndex, DS_FilterTypeCmd->FilterType);
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set packet filter parameters                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetFilterParms(const CFE_SB_Buffer_t *BufPtr)
{
    DS_FilterParmsCmd_t *DS_FilterParmsCmd = (DS_FilterParmsCmd_t *)BufPtr;
    size_t               ActualLength      = 0;
    size_t               ExpectedLength    = sizeof(DS_FilterParmsCmd_t);
    DS_PacketEntry_t *   pPacketEntry      = NULL;
    DS_FilterParms_t *   pFilterParms      = NULL;
    int32                FilterTableIndex  = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER PARMS command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (!CFE_SB_IsValidMsgId(DS_FilterParmsCmd->MessageID))
    {
        /*
        ** Invalid packet messageID...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER PARMS command arg: invalid messageID = 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_FilterParmsCmd->MessageID));
    }
    else if (DS_FilterParmsCmd->FilterParmsIndex >= DS_FILTERS_PER_PACKET)
    {
        /*
        ** Invalid packet filter parameters index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER PARMS command arg: filter parameters index = %d",
                          DS_FilterParmsCmd->FilterParmsIndex);
    }
    else if (DS_TableVerifyParms(DS_FilterParmsCmd->Algorithm_N, DS_FilterParmsCmd->Algorithm_X,
                                 DS_FilterParmsCmd->Algorithm_O) == false)
    {
        /*
        ** Invalid packet filter algorithm parameters...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER PARMS command arg: N = %d, X = %d, O = %d", DS_FilterParmsCmd->Algorithm_N,
                          DS_FilterParmsCmd->Algorithm_X, DS_FilterParmsCmd->Algorithm_O);
    }
    else if (DS_AppData.FilterTblPtr == (DS_FilterTable_t *)NULL)
    {
        /*
        ** Must have a valid packet filter table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid FILTER PARMS command: packet filter table is not loaded");
    }
    else
    {
        /*
        ** Get the index of the filter table entry for this message ID...
        */
        FilterTableIndex = DS_TableFindMsgID(DS_FilterParmsCmd->MessageID);

        if (FilterTableIndex == DS_INDEX_NONE)
        {
            /*
            ** Must not create - may only modify existing packet filter...
            */
            DS_AppData.CmdRejectedCounter++;

            CFE_EVS_SendEvent(DS_PARMS_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                              "Invalid FILTER PARMS command: Message ID 0x%08lX is not in filter table",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterParmsCmd->MessageID));
        }
        else
        {
            /*
            ** Set new packet filter values (algorithm)...
            */
            pPacketEntry = &DS_AppData.FilterTblPtr->Packet[FilterTableIndex];
            pFilterParms = &pPacketEntry->Filter[DS_FilterParmsCmd->FilterParmsIndex];

            pFilterParms->Algorithm_N = DS_FilterParmsCmd->Algorithm_N;
            pFilterParms->Algorithm_X = DS_FilterParmsCmd->Algorithm_X;
            pFilterParms->Algorithm_O = DS_FilterParmsCmd->Algorithm_O;

            /*
            ** Notify cFE that we have modified the table data...
            */
            CFE_TBL_Modified(DS_AppData.FilterTblHandle);

            DS_AppData.CmdAcceptedCounter++;

            CFE_EVS_SendEvent(DS_PARMS_CMD_EID, CFE_EVS_EventType_DEBUG,
                              "FILTER PARMS command: MID = 0x%08lX, index = %d, filter = %d, N = %d, X = %d, O = %d",
                              (unsigned long)CFE_SB_MsgIdToValue(DS_FilterParmsCmd->MessageID), (int)FilterTableIndex,
                              DS_FilterParmsCmd->FilterParmsIndex, pFilterParms->Algorithm_N, pFilterParms->Algorithm_X,
                              pFilterParms->Algorithm_O);
        }
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set destination filename type                                   */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestType(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestTypeCmd_t *  DS_DestTypeCmd = (DS_DestTypeCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestTypeCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_NTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST TYPE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestTypeCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_NTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST TYPE command arg: file table index = %d", DS_DestTypeCmd->FileTableIndex);
    }
    else if (DS_TableVerifyType(DS_DestTypeCmd->FileNameType) == false)
    {
        /*
        ** Invalid destination filename type...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_NTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST TYPE command arg: filename type = %d", DS_DestTypeCmd->FileNameType);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_NTYPE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST TYPE command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set new destination table filename type...
        */
        pDest               = &DS_AppData.DestFileTblPtr->File[DS_DestTypeCmd->FileTableIndex];
        pDest->FileNameType = DS_DestTypeCmd->FileNameType;

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_NTYPE_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST TYPE command: file table index = %d, filename type = %d",
                          DS_DestTypeCmd->FileTableIndex, DS_DestTypeCmd->FileNameType);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set dest file ena/dis state                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestState(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestStateCmd_t *DS_DestStateCmd = (DS_DestStateCmd_t *)BufPtr;
    size_t             ActualLength    = 0;
    size_t             ExpectedLength  = sizeof(DS_DestStateCmd_t);

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_STATE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST STATE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestStateCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_STATE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST STATE command arg: file table index = %d", DS_DestStateCmd->FileTableIndex);
    }
    else if (DS_TableVerifyState(DS_DestStateCmd->EnableState) == false)
    {
        /*
        ** Invalid destination file state...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_STATE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST STATE command arg: file state = %d", DS_DestStateCmd->EnableState);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_STATE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST STATE command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set new destination table file state in table and in current status...
        */
        DS_AppData.DestFileTblPtr->File[DS_DestStateCmd->FileTableIndex].EnableState = DS_DestStateCmd->EnableState;
        DS_AppData.FileStatus[DS_DestStateCmd->FileTableIndex].FileState             = DS_DestStateCmd->EnableState;

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_STATE_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST STATE command: file table index = %d, file state = %d", DS_DestStateCmd->FileTableIndex,
                          DS_DestStateCmd->EnableState);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set path portion of filename                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestPath(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestPathCmd_t *  DS_DestPathCmd = (DS_DestPathCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestPathCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PATH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST PATH command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestPathCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PATH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST PATH command arg: file table index = %d", (int)DS_DestPathCmd->FileTableIndex);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_PATH_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST PATH command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set path portion of destination table filename...
        */
        pDest = &DS_AppData.DestFileTblPtr->File[DS_DestPathCmd->FileTableIndex];
        strncpy(pDest->Pathname, DS_DestPathCmd->Pathname, sizeof(pDest->Pathname));

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_PATH_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST PATH command: file table index = %d, pathname = '%s'",
                          (int)DS_DestPathCmd->FileTableIndex, DS_DestPathCmd->Pathname);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set base portion of filename                                    */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestBase(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestBaseCmd_t *  DS_DestBaseCmd = (DS_DestBaseCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestBaseCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_BASE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST BASE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestBaseCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_BASE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST BASE command arg: file table index = %d", (int)DS_DestBaseCmd->FileTableIndex);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_BASE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST BASE command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set base portion of destination table filename...
        */
        pDest = &DS_AppData.DestFileTblPtr->File[DS_DestBaseCmd->FileTableIndex];
        strncpy(pDest->Basename, DS_DestBaseCmd->Basename, sizeof(pDest->Basename));

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_BASE_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST BASE command: file table index = %d, base filename = '%s'",
                          (int)DS_DestBaseCmd->FileTableIndex, DS_DestBaseCmd->Basename);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set extension portion of filename                               */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestExt(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestExtCmd_t *   DS_DestExtCmd  = (DS_DestExtCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestExtCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_EXT_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST EXT command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestExtCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_EXT_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST EXT command arg: file table index = %d", (int)DS_DestExtCmd->FileTableIndex);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_EXT_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST EXT command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set extension portion of destination table filename...
        */
        pDest = &DS_AppData.DestFileTblPtr->File[DS_DestExtCmd->FileTableIndex];
        strncpy(pDest->Extension, DS_DestExtCmd->Extension, sizeof(pDest->Extension));

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_EXT_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST EXT command: file table index = %d, extension = '%s'",
                          (int)DS_DestExtCmd->FileTableIndex, DS_DestExtCmd->Extension);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set maximum file size limit                                     */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestSize(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestSizeCmd_t *  DS_DestSizeCmd = (DS_DestSizeCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestSizeCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SIZE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST SIZE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestSizeCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SIZE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST SIZE command arg: file table index = %d", (int)DS_DestSizeCmd->FileTableIndex);
    }
    else if (DS_TableVerifySize(DS_DestSizeCmd->MaxFileSize) == false)
    {
        /*
        ** Invalid destination file size limit...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SIZE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST SIZE command arg: size limit = %d", (int)DS_DestSizeCmd->MaxFileSize);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SIZE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST SIZE command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set size limit for destination file...
        */
        pDest              = &DS_AppData.DestFileTblPtr->File[DS_DestSizeCmd->FileTableIndex];
        pDest->MaxFileSize = DS_DestSizeCmd->MaxFileSize;

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_SIZE_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST SIZE command: file table index = %d, size limit = %d",
                          (int)DS_DestSizeCmd->FileTableIndex, (int)DS_DestSizeCmd->MaxFileSize);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set maximum file age limit                                      */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestAge(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestAgeCmd_t *   DS_DestAgeCmd  = (DS_DestAgeCmd_t *)BufPtr;
    size_t              ActualLength   = 0;
    size_t              ExpectedLength = sizeof(DS_DestAgeCmd_t);
    DS_DestFileEntry_t *pDest          = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_AGE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST AGE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestAgeCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_AGE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST AGE command arg: file table index = %d", (int)DS_DestAgeCmd->FileTableIndex);
    }
    else if (DS_TableVerifyAge(DS_DestAgeCmd->MaxFileAge) == false)
    {
        /*
        ** Invalid destination file age limit...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_AGE_CMD_ERR_EID, CFE_EVS_EventType_ERROR, "Invalid DEST AGE command arg: age limit = %d",
                          (int)DS_DestAgeCmd->MaxFileAge);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_AGE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST AGE command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set age limit for destination file...
        */
        pDest             = &DS_AppData.DestFileTblPtr->File[DS_DestAgeCmd->FileTableIndex];
        pDest->MaxFileAge = DS_DestAgeCmd->MaxFileAge;

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_AGE_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST AGE command: file table index = %d, age limit = %d", (int)DS_DestAgeCmd->FileTableIndex,
                          (int)DS_DestAgeCmd->MaxFileAge);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Set seq cnt portion of filename                                 */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdSetDestCount(const CFE_SB_Buffer_t *BufPtr)
{
    DS_DestCountCmd_t * DS_DestCountCmd = (DS_DestCountCmd_t *)BufPtr;
    size_t              ActualLength    = 0;
    size_t              ExpectedLength  = sizeof(DS_DestCountCmd_t);
    DS_AppFileStatus_t *FileStatus      = NULL;
    DS_DestFileEntry_t *DestFile        = NULL;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SEQ_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST COUNT command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_DestCountCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SEQ_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST COUNT command arg: file table index = %d",
                          (int)DS_DestCountCmd->FileTableIndex);
    }
    else if (DS_TableVerifyCount(DS_DestCountCmd->SequenceCount) == false)
    {
        /*
        ** Invalid destination file sequence count...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SEQ_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST COUNT command arg: sequence count = %d", (int)DS_DestCountCmd->SequenceCount);
    }
    else if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
    {
        /*
        ** Must have a valid destination file table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_SEQ_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST COUNT command: destination file table is not loaded");
    }
    else
    {
        /*
        ** Set next sequence count for destination file...
        */
        DestFile   = &DS_AppData.DestFileTblPtr->File[DS_DestCountCmd->FileTableIndex];
        FileStatus = &DS_AppData.FileStatus[DS_DestCountCmd->FileTableIndex];

        /*
        ** Update both destination file table and current status...
        */
        DestFile->SequenceCount = DS_DestCountCmd->SequenceCount;
        FileStatus->FileCount   = DS_DestCountCmd->SequenceCount;

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.DestFileTblHandle);

        /*
        ** Update the Critical Data Store (CDS)...
        */
        DS_TableUpdateCDS();

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_SEQ_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "DEST COUNT command: file table index = %d, sequence count = %d",
                          (int)DS_DestCountCmd->FileTableIndex, (int)DS_DestCountCmd->SequenceCount);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Close destination file                                          */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdCloseFile(const CFE_SB_Buffer_t *BufPtr)
{
    DS_CloseFileCmd_t *DS_CloseFileCmd = (DS_CloseFileCmd_t *)BufPtr;
    size_t             ActualLength    = 0;
    size_t             ExpectedLength  = sizeof(DS_CloseFileCmd_t);

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_CLOSE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST CLOSE command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (DS_TableVerifyFileIndex(DS_CloseFileCmd->FileTableIndex) == false)
    {
        /*
        ** Invalid destination file table index...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_CLOSE_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST CLOSE command arg: file table index = %d",
                          (int)DS_CloseFileCmd->FileTableIndex);
    }
    else
    {
        /*
        ** Close destination file (if the file was open)...
        */
        if (OS_ObjectIdDefined(DS_AppData.FileStatus[DS_CloseFileCmd->FileTableIndex].FileHandle))
        {
            DS_FileUpdateHeader(DS_CloseFileCmd->FileTableIndex);
            DS_FileCloseDest(DS_CloseFileCmd->FileTableIndex);
        }

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_CLOSE_CMD_EID, CFE_EVS_EventType_DEBUG, "DEST CLOSE command: file table index = %d",
                          (int)DS_CloseFileCmd->FileTableIndex);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Close all open destination files                                */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdCloseAll(const CFE_SB_Buffer_t *BufPtr)
{
    size_t ActualLength   = 0;
    size_t ExpectedLength = sizeof(DS_CloseAllCmd_t);
    int32  i              = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_CLOSE_ALL_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid DEST CLOSE ALL command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else
    {
        /*
        ** Close all open destination files...
        */
        for (i = 0; i < DS_DEST_FILE_CNT; i++)
        {
            if (OS_ObjectIdDefined(DS_AppData.FileStatus[i].FileHandle))
            {
                DS_FileUpdateHeader(i);
                DS_FileCloseDest(i);
            }
        }

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_CLOSE_ALL_CMD_EID, CFE_EVS_EventType_DEBUG, "DEST CLOSE ALL command");
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Get file info packet                                            */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdGetFileInfo(const CFE_SB_Buffer_t *BufPtr)
{
    DS_FileInfoPkt_t DS_FileInfoPkt;
    size_t           ActualLength   = 0;
    size_t           ExpectedLength = sizeof(DS_GetFileInfoCmd_t);
    int32            i              = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_GET_FILE_INFO_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid GET FILE INFO command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else
    {
        /*
        ** Create and send a file info packet...
        */
        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_GET_FILE_INFO_CMD_EID, CFE_EVS_EventType_DEBUG, "GET FILE INFO command");

        /*
        ** Initialize file info telemetry packet...
        */
        CFE_MSG_Init(&DS_FileInfoPkt.TlmHeader.Msg, CFE_SB_ValueToMsgId(DS_DIAG_TLM_MID), sizeof(DS_FileInfoPkt_t));

        /*
        ** Process array of destination file info data...
        */
        for (i = 0; i < DS_DEST_FILE_CNT; i++)
        {
            /*
            ** Set file age and size...
            */
            DS_FileInfoPkt.FileInfo[i].FileAge  = DS_AppData.FileStatus[i].FileAge;
            DS_FileInfoPkt.FileInfo[i].FileSize = DS_AppData.FileStatus[i].FileSize;

            /*
            ** Set file growth rate (computed when process last HK request)...
            */
            DS_FileInfoPkt.FileInfo[i].FileRate = DS_AppData.FileStatus[i].FileRate;

            /*
            ** Set current filename sequence count...
            */
            DS_FileInfoPkt.FileInfo[i].SequenceCount = DS_AppData.FileStatus[i].FileCount;

            /*
            ** Set file enable/disable state...
            */
            if (DS_AppData.DestFileTblPtr == (DS_DestFileTable_t *)NULL)
            {
                DS_FileInfoPkt.FileInfo[i].EnableState = DS_DISABLED;
            }
            else
            {
                DS_FileInfoPkt.FileInfo[i].EnableState = DS_AppData.FileStatus[i].FileState;
            }

            /*
            ** Set file open/closed state...
            */
            if (!OS_ObjectIdDefined(DS_AppData.FileStatus[i].FileHandle))
            {
                DS_FileInfoPkt.FileInfo[i].OpenState = DS_CLOSED;
            }
            else
            {
                DS_FileInfoPkt.FileInfo[i].OpenState = DS_OPEN;

                /*
                ** Set current open filename...
                */
                strncpy(DS_FileInfoPkt.FileInfo[i].FileName, DS_AppData.FileStatus[i].FileName,
                        sizeof(DS_FileInfoPkt.FileInfo[i].FileName));
            }
        }

        /*
        ** Timestamp and send file info telemetry packet...
        */
        CFE_SB_TimeStampMsg(&DS_FileInfoPkt.TlmHeader.Msg);
        CFE_SB_TransmitMsg(&DS_FileInfoPkt.TlmHeader.Msg, true);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* Add message ID to packet filter table                           */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdAddMID(const CFE_SB_Buffer_t *BufPtr)
{
    DS_AddMidCmd_t *  DS_AddMidCmd     = (DS_AddMidCmd_t *)BufPtr;
    size_t            ActualLength     = 0;
    size_t            ExpectedLength   = sizeof(DS_AddMidCmd_t);
    DS_PacketEntry_t *pPacketEntry     = NULL;
    DS_FilterParms_t *pFilterParms     = NULL;
    int32             FilterTableIndex = 0;
    int32             HashTableIndex   = 0;
    int32             i                = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid ADD MID command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (!CFE_SB_IsValidMsgId(DS_AddMidCmd->MessageID))
    {
        /*
        ** Invalid packet message ID - can be anything but unused...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid ADD MID command arg: invalid MID = 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_AddMidCmd->MessageID));
    }
    else if (DS_AppData.FilterTblPtr == (DS_FilterTable_t *)NULL)
    {
        /*
        ** Must have a valid packet filter table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid ADD MID command: filter table is not loaded");
    }
    else if ((FilterTableIndex = DS_TableFindMsgID(DS_AddMidCmd->MessageID)) != DS_INDEX_NONE)
    {
        /*
        ** New message ID is already in packet filter table...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid ADD MID command: MID = 0x%08lX is already in filter table at index = %d",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_AddMidCmd->MessageID), (int)FilterTableIndex);
    }
    else if ((FilterTableIndex = DS_TableFindMsgID(CFE_SB_INVALID_MSG_ID)) == DS_INDEX_NONE)
    {
        /*
        ** Packet filter table has no unused entries...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid ADD MID command: filter table is full");
    }
    else
    {
        /*
        ** Initialize unused packet filter entry for new message ID...
        */
        pPacketEntry = &DS_AppData.FilterTblPtr->Packet[FilterTableIndex];

        pPacketEntry->MessageID = DS_AddMidCmd->MessageID;

        /* Add the message ID to the hash table as well */
        HashTableIndex = DS_TableAddMsgID(DS_AddMidCmd->MessageID, FilterTableIndex);

        for (i = 0; i < DS_FILTERS_PER_PACKET; i++)
        {
            pFilterParms = &pPacketEntry->Filter[i];

            pFilterParms->FileTableIndex = 0;
            pFilterParms->FilterType     = DS_BY_COUNT;

            pFilterParms->Algorithm_N = 0;
            pFilterParms->Algorithm_X = 0;
            pFilterParms->Algorithm_O = 0;
        }

        CFE_SB_SubscribeEx(DS_AddMidCmd->MessageID, DS_AppData.InputPipe, CFE_SB_DEFAULT_QOS, DS_PER_PACKET_PIPE_LIMIT);
        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.FilterTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_ADD_MID_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "ADD MID command: MID = 0x%08lX, filter index = %d, hash index = %d",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_AddMidCmd->MessageID), (int)FilterTableIndex,
                          (int)HashTableIndex);
    }
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                 */
/* DS_CmdRemoveMID() - remove message ID from packet filter table  */
/*                                                                 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void DS_CmdRemoveMID(const CFE_SB_Buffer_t *BufPtr)
{
    DS_RemoveMidCmd_t *DS_RemoveMidCmd  = (DS_RemoveMidCmd_t *)BufPtr;
    size_t             ActualLength     = 0;
    size_t             ExpectedLength   = sizeof(DS_RemoveMidCmd_t);
    DS_PacketEntry_t * pPacketEntry     = NULL;
    DS_FilterParms_t * pFilterParms     = NULL;
    int32              FilterTableIndex = 0;
    int32              HashTableIndex   = 0;
    int32              i                = 0;

    CFE_MSG_GetSize(&BufPtr->Msg, &ActualLength);
    FilterTableIndex = DS_TableFindMsgID(DS_RemoveMidCmd->MessageID);

    if (ExpectedLength != ActualLength)
    {
        /*
        ** Invalid command packet length...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_REMOVE_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid REMOVE MID command length: expected = %d, actual = %d", (int)ExpectedLength,
                          (int)ActualLength);
    }
    else if (!CFE_SB_IsValidMsgId(DS_RemoveMidCmd->MessageID))
    {
        /*
        ** Invalid packet message ID - can be anything but unused...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_REMOVE_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid REMOVE MID command arg: invalid MID = 0x%08lX",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_RemoveMidCmd->MessageID));
    }
    else if (DS_AppData.FilterTblPtr == (DS_FilterTable_t *)NULL)
    {
        /*
        ** Must have a valid packet filter table loaded...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_REMOVE_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid REMOVE MID command: filter table is not loaded");
    }
    else if (FilterTableIndex == DS_INDEX_NONE)
    {
        /*
        ** Message ID is not in packet filter table...
        */
        DS_AppData.CmdRejectedCounter++;

        CFE_EVS_SendEvent(DS_REMOVE_MID_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
                          "Invalid REMOVE MID command: MID = 0x%08lX is not in filter table",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_RemoveMidCmd->MessageID));
    }
    else
    {
        /* Convert MID into hash table index */
        HashTableIndex = DS_TableHashFunction(DS_RemoveMidCmd->MessageID);

        /*
        ** Reset used packet filter entry for used message ID...
        */
        pPacketEntry = &DS_AppData.FilterTblPtr->Packet[FilterTableIndex];

        pPacketEntry->MessageID = CFE_SB_INVALID_MSG_ID;

        /* Create new hash table as well */
        DS_TableCreateHash();

        for (i = 0; i < DS_FILTERS_PER_PACKET; i++)
        {
            pFilterParms = &pPacketEntry->Filter[i];

            pFilterParms->FileTableIndex = 0;
            pFilterParms->FilterType     = DS_BY_COUNT;

            pFilterParms->Algorithm_N = 0;
            pFilterParms->Algorithm_X = 0;
            pFilterParms->Algorithm_O = 0;
        }

        CFE_SB_Unsubscribe(DS_RemoveMidCmd->MessageID, DS_AppData.InputPipe);

        /*
        ** Notify cFE that we have modified the table data...
        */
        CFE_TBL_Modified(DS_AppData.FilterTblHandle);

        DS_AppData.CmdAcceptedCounter++;

        CFE_EVS_SendEvent(DS_REMOVE_MID_CMD_EID, CFE_EVS_EventType_DEBUG,
                          "REMOVE MID command: MID = 0x%08lX, filter index = %d, hash index = %d",
                          (unsigned long)CFE_SB_MsgIdToValue(DS_RemoveMidCmd->MessageID), (int)FilterTableIndex,
                          (int)HashTableIndex);
    }
}

/************************/
/*  End of File Comment */
/************************/
