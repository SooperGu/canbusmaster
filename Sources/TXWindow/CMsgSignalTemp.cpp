/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief This file contain definition of all function of
 * @author Amarnath Shastry
 * @copyright Copyright (c) 2011, Robert Bosch Engineering and Business Solutions. All rights reserved.
 *
 * This file contain definition of all function of
 */

#include "TxWindow_stdafx.h"     // Standard header include file
#include "Include/BaseDefs.h"
#include "CMsgSignalTemp.h"      // Class defintion file

static CHAR s_acTraceStr[1024] = {""};

/** Helper function to calculate how many bytes the signal consumes */
UINT static nGetNoOfBytesToRead(UINT nBitNum, UINT nSigLen)
{
    ASSERT(nSigLen > 0);
    UINT nBytesToRead = 1; //Consider first byte

    INT nRemainingLength = nSigLen - (defBITS_IN_BYTE - nBitNum);

    if (nRemainingLength > 0)
    {
        // Add te number of bytes totally it consumes.
        nBytesToRead += (INT)(nRemainingLength / defBITS_IN_BYTE);

        // Check for extra bits which traverse to the next byte.
        INT nTotalBitsConsidered = ((nBytesToRead - 1) * defBITS_IN_BYTE) +
                                   (defBITS_IN_BYTE - nBitNum);
        if ((UINT)nTotalBitsConsidered < nSigLen)
        {
            nBytesToRead++;
        }
    }

    return nBytesToRead;
}

BOOL CMsgSignal::bValidateSignal(UINT nDLC, UINT nByteNum, UINT nBitNum,
                                 UINT nLength, EFORMAT_DATA bDataFormat)
{
    BOOL bValid = TRUE;
    UINT nBytesToRead = nGetNoOfBytesToRead(nBitNum, nLength);
    bValid = (bDataFormat == DATA_FORMAT_INTEL)?
             (INT)(nByteNum + nBytesToRead - 1) <= nDLC :
             (INT)(nByteNum - nBytesToRead) >= 0;
    return bValid;
}

BOOL CMsgSignal::bCalcBitMaskForSig(BYTE* pbyMaskByte, UINT unArrayLen,
                                    UINT nByteNum, UINT nBitNum, UINT nLength,
                                    EFORMAT_DATA bDataFormat)
{
    BOOL bValid = TRUE;
    //Reset the Byte array
    memset(pbyMaskByte, 0, sizeof(BYTE) * unArrayLen);

    // Calculate how many bytes the signal occupies
    UINT nBytesToRead = nGetNoOfBytesToRead(nBitNum, nLength);

    UINT CurrBitNum = nBitNum;
    /* If the Byte order is the motorola Bit mask has to be updated in
    the reverse order */
    INT nByteOrder = (bDataFormat == DATA_FORMAT_INTEL) ? 1: -1;
    bValid = bValidateSignal(unArrayLen, nByteNum, nBitNum, nLength, bDataFormat);

    if (bValid == TRUE)
    {
        UINT nBitsRead = 0;

        for (UINT i = 0; i < nBytesToRead; i++)
        {
            BYTE byMsgByteVal = 0xFF; //Mask
            if (CurrBitNum != 0)
            {
                byMsgByteVal <<= CurrBitNum; //Set the bits high after the bit number
            }
            // Check for the extra bits at the end and set them low.
            INT ExtraBits = (defBITS_IN_BYTE - CurrBitNum) - (nLength - nBitsRead);
            BYTE nMask = (ExtraBits > 0) ? 0xFF >> (BYTE)ExtraBits : 0xFF;
            byMsgByteVal &= nMask;

            //Calculate the bits read for each time
            nBitsRead += min (defBITS_IN_BYTE - CurrBitNum, nLength - nBitsRead);
            /*Reset the current bit num since bit number always starts from
            the zero after the first byte */
            CurrBitNum = 0;
            //Update the mask in the corresponding byte of the array
            pbyMaskByte[(nByteNum - 1) + (i * nByteOrder)] = byMsgByteVal;
        }
    }

    return bValid;
}

void CMsgSignal::omStrListGetMessageNames(CStringList& omStrListMsgs)
{
    // Clear the list
    omStrListMsgs.RemoveAll();
    m_omDBMapCritSec.Lock();
    // Clear the list
    int nTotalMsgCount = m_omMsgDetailsIDMap.GetCount();
    POSITION pos = m_omMsgDetailsIDMap.GetStartPosition( );
    sMESSAGE* psTempMsgStruct;
    CString strMsgKey;
    for (int nMsgIndex = 0; nMsgIndex < nTotalMsgCount && pos != NULL; nMsgIndex++ )
    {
        m_omMsgDetailsIDMap.GetNextAssoc(pos,strMsgKey,psTempMsgStruct);
        omStrListMsgs.AddHead( psTempMsgStruct->m_omStrMessageName);
    }
    m_omDBMapCritSec.Unlock();
}
UINT CMsgSignal::unGetNumberOfMessages()
{
    return ((UINT)m_omMsgDetailsIDMap.GetCount());
}

void CMsgSignal::unListGetMessageIDs(UINT* omListId)
{
    m_omDBMapCritSec.Lock();
    // Clear the list
    int nTotalMsgCount = (INT)m_omMsgDetailsIDMap.GetCount();
    POSITION pos = m_omMsgDetailsIDMap.GetStartPosition( );
    sMESSAGE* psTempMsgStruct;
    CString strMsgKey;
    for (int unMsgIndex = 0; unMsgIndex < nTotalMsgCount && pos != NULL; unMsgIndex++ )
    {
        m_omMsgDetailsIDMap.GetNextAssoc(pos,strMsgKey,psTempMsgStruct);
        omListId[unMsgIndex] = psTempMsgStruct->m_unMessageCode;
    }
    m_omDBMapCritSec.Unlock();
}

CString CMsgSignal::omStrGetMessageNameFromMsgCodeInactive( UINT unMsgCode)
{
    CString strMsgName = "";

    // validate message code
    if (unMsgCode >= 0)
    {
        for ( UINT unMsgCount = 0; unMsgCount < m_unMessageCount; unMsgCount++ )
        {
            if ( m_psMessages[unMsgCount].m_unMessageCode
                    == unMsgCode )
            {
                strMsgName = m_psMessages[unMsgCount].m_omStrMessageName;

                break;
            }
        }
    }

    return (strMsgName);
}

CString CMsgSignal::omStrGetMessageNameFromMsgCode( UINT unMsgCode)
{
    CString strMsgName = "";

    if (unMsgCode >= 0)
    {
        sMESSAGE* psMsgStruct = nullptr;
        m_omMsgDetailsIDMap.Lookup(unMsgCode,psMsgStruct);

        if(psMsgStruct != nullptr)
        {
            strMsgName = psMsgStruct->m_omStrMessageName;
        }
    }

    return strMsgName;
}

CString CMsgSignal::omStrGetMessageLengthFromMsgCode( UINT unMsgCode)
{
    CString omstrMsgLength = "";

    if (unMsgCode >= 0)
    {
        sMESSAGE* psMsgStruct = nullptr;
        m_omMsgDetailsIDMap.Lookup(unMsgCode,psMsgStruct);

        if(psMsgStruct != nullptr)
        {
            UINT nMsgLength = 0;
            nMsgLength = psMsgStruct->m_unMessageLength;

            omstrMsgLength.Format("%d", nMsgLength);
        }
    }

    return omstrMsgLength;
}

sMESSAGE* CMsgSignal::psGetMessagePointer( UINT unMsgID)
{
    sMESSAGE* psMsgStruct = nullptr;
    m_omMsgDetailsIDMap.Lookup(unMsgID,psMsgStruct);
    return psMsgStruct;
}

/******************************************************************************
  Function Name    :  psGetMessagePointer
  Input(s)         :  CString strMsgName
  Output           :  sMESSAGE*
  Functionality    :  Returns associted message pointer if found, otherwise
                      NULL.
  Member of        :  CMsgSignal
  Friend of        :      -
  Author(s)        :  Amarnath Shastry
  Date Created     :  19.02.2002
  Modifications    :  derka
                      04.06.2014, pasted this function here
******************************************************************************/
sMESSAGE* CMsgSignal::psGetMessagePointer( CString strMsgName)
{
    m_omDBMapCritSec.Lock();
    POSITION pos = m_omMsgDetailsIDMap.GetStartPosition();
    sMESSAGE* psMsgStruct = NULL;
    CString strKey;
    while (pos != NULL )
    {
        m_omMsgDetailsIDMap.GetNextAssoc( pos, strKey, psMsgStruct );
        if((NULL != psMsgStruct) && (!(psMsgStruct->m_omStrMessageName.Compare(strMsgName))))
        {
            return psMsgStruct;
        }
    }
    m_omDBMapCritSec.Unlock();
    return NULL;
}