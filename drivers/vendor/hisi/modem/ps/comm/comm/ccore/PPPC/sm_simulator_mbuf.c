



/*****************************************************************************
  1 头文件包含
*****************************************************************************/
#include "ppp_inc.h"

#include "PsLib.h"
#include "PsCommonDef.h"
#include "sm_simulator_mbuf.h"



#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


/*****************************************************************************
    协议栈打印打点方式下的.C文件宏定义
*****************************************************************************/
#define         THIS_FILE_ID            PS_FILE_ID_PPPC_MBUF_C


/*****************************************************************************
  2 全局变量定义
*****************************************************************************/

SM_PGPADAPT_PACKET_INFO_S g_astPgpAdaptPktInfo[SM_PGPADAPT_MBUF_CORR_PKTID_MAX_NUM];

SM_VOID sm_debug_printAll
(
    SM_UCHAR* szFormat,
    ...
)
{
    return;
}

SM_ULONG    g_ulCurMbufIndex = 0;  /* 缓存的Mbuf当前索引 */

SM_PGPADAPT_MBUF_INFO_S g_stPgpAdaptMbufInfo = {0};

SM_ULONG    g_ulPktIdIndex = 0;    /* 存储Mbuf和packet id的对应关系的索引 */

extern SM_PGPADAPT_PACKET_INFO_S g_astPgpAdaptPktInfo[];

#define PMBUF_RESV_HEAD_SIZE            (256)
#define PMBUF_DATABLOCK_SIZE(len)       (len + PMBUF_RESV_HEAD_SIZE)

/*****************************************************************************
  3 函数实现
*****************************************************************************/
/*lint -save -e958 */

ULONG PMBUF_Destroy(PMBUF_S * pstMbuf)
{
    MBUF_DATABLOCKDESCRIPTOR_S         *pstNextDataBlock    = VOS_NULL_PTR;
    MBUF_DATABLOCKDESCRIPTOR_S         *pstTempDataBlock    = VOS_NULL_PTR;


    if (VOS_NULL_PTR == pstMbuf)
    {
        return VOS_ERR;
    }

    /* 获取下一块DataBlockScriptor指针 */
    pstNextDataBlock    = pstMbuf->stDataBlockDescriptor.pstNextDataBlockDescriptor;

    /* 不需要判断pstNextDataBlock是否为VOS_NULL_PTR */
    PMBUF_DataBlockDestroy(pstNextDataBlock);
    pstNextDataBlock = VOS_NULL_PTR;

    /* 清除当前DataBlock的数据指针及MBUF内存 */
    if (VOS_NULL_PTR != pstMbuf->stDataBlockDescriptor.pucDataBlock)
    {
        PPP_Free(pstMbuf->stDataBlockDescriptor.pucDataBlock);
        pstMbuf->stDataBlockDescriptor.pucDataBlock = VOS_NULL_PTR;
        pstMbuf->stDataBlockDescriptor.pucData= VOS_NULL_PTR;
    }
    PPP_Free(pstMbuf);
    pstMbuf = VOS_NULL_PTR;

    return VOS_OK;
}
PMBUF_S *PMBUF_GetPMbuf(ULONG ulSubMod, ULONG ulMBufLen)
{
    MBUF_DATABLOCKDESCRIPTOR_S         *pstNextDataBlock    = VOS_NULL_PTR;
    MBUF_DATABLOCKDESCRIPTOR_S         *pstCurrDataBlock    = VOS_NULL_PTR;
    PMBUF_S                            *pstMbuf             = VOS_NULL_PTR;
    UCHAR                              *pucTempdata         = VOS_NULL_PTR;
    ULONG                               ulActualLen         = 0;


    /* 长度为0,无需申请MBUF */
    if (0 == ulMBufLen)
    {
        PPPC_WARNING_LOG("MBufLen is 0, No Need to Alloc Memory!\r\n");
        return VOS_NULL_PTR;
    }

    /* 申请MBUF头 */
    pstMbuf = PPP_Malloc(sizeof(PMBUF_S));
    if (VOS_NULL == pstMbuf)
    {
        PPPC_WARNING_LOG("Malloc PMBUF Fail!\r\n");
        return VOS_NULL_PTR;
    }
    PS_MEM_SET(pstMbuf, 0x0, sizeof(PMBUF_S));

    pstCurrDataBlock            = &(pstMbuf->stDataBlockDescriptor);
    while ((ulMBufLen > 0) && (VOS_NULL_PTR != pstCurrDataBlock))
    {
        if (ulMBufLen > PMBUF_MAX_AVAILABLE_MEM_SIZE)
        {
            ulActualLen         = PMBUF_MAX_AVAILABLE_MEM_SIZE;
            ulMBufLen          -= PMBUF_MAX_AVAILABLE_MEM_SIZE;

            /* 数据长度大于一次可申请的上限,必然还有下一块 */
            pstNextDataBlock    = (MBUF_DATABLOCKDESCRIPTOR_S *)PPP_Malloc(
                                    sizeof(MBUF_DATABLOCKDESCRIPTOR_S));
            if (VOS_NULL_PTR == pstNextDataBlock)
            {
                PPPC_ERROR_LOG("Malloc DATABLOCKSCRIPTOR Fail!\r\n");
                PMBUF_Destroy(pstMbuf);
                return VOS_NULL_PTR;
            }
            PS_MEM_SET(pstNextDataBlock, 0, sizeof(MBUF_DATABLOCKDESCRIPTOR_S));
        }
        else
        {
            ulActualLen = ulMBufLen;
            ulMBufLen   = 0;

            /* 数据长度小于等于一次可申请的上限,必然没有下一块,置为空 */
            pstNextDataBlock = VOS_NULL_PTR;
        }
        pstCurrDataBlock->pstNextDataBlockDescriptor   = pstNextDataBlock;
        (pstMbuf->ulDataBlockNumber)++;

        /* 申请存放报文的内存 */
        pucTempdata = PPP_Malloc(PMBUF_DATABLOCK_SIZE(ulActualLen));
        if (VOS_NULL == pucTempdata)
        {
            PPPC_WARNING_LOG("Malloc Data Block Fail!\r\n");
            PMBUF_Destroy(pstMbuf);
            return VOS_NULL_PTR;
        }

        PS_MEM_SET(pucTempdata, 0x0, PMBUF_DATABLOCK_SIZE(ulActualLen));
        pstCurrDataBlock->pucDataBlock       = pucTempdata;
        pstCurrDataBlock->ulDataBlockLength  = PMBUF_DATABLOCK_SIZE(ulActualLen);
        pstCurrDataBlock->pucData            = pucTempdata + PMBUF_RESV_HEAD_SIZE;
        pstCurrDataBlock->ulDataLength       = 0;
        pstCurrDataBlock                     = pstNextDataBlock;
    }

    return pstMbuf;
}
ULONG PMBUF_DataBlockDestroy(MBUF_DATABLOCKDESCRIPTOR_S *pstHeadDataBlock)
{
    MBUF_DATABLOCKDESCRIPTOR_S         *pstNextDataBlock    = VOS_NULL_PTR;
    MBUF_DATABLOCKDESCRIPTOR_S         *pstCurrDataBlock    = VOS_NULL_PTR;


    pstCurrDataBlock    = pstHeadDataBlock;

    while (VOS_NULL_PTR != pstCurrDataBlock)
    {
        /* 存储下一块DataBlock指针 */
        pstNextDataBlock    = pstCurrDataBlock->pstNextDataBlockDescriptor;

        /* 释放数据内存 */
        if (VOS_NULL_PTR != pstCurrDataBlock->pucDataBlock)
        {
            /* 清除当前数据块的Data指针 */
            PPP_Free(pstCurrDataBlock->pucDataBlock);
            pstCurrDataBlock->pucDataBlock  = VOS_NULL_PTR;
            pstCurrDataBlock->pucData       = VOS_NULL_PTR;
        }
        pstCurrDataBlock->ulDataBlockLength = 0;
        pstCurrDataBlock->ulDataLength  = 0;
        pstCurrDataBlock->ulType        = 0;

        /* 最后将当前数据块也清除, 释放DataBlock */
        PPP_Free(pstCurrDataBlock);

        pstCurrDataBlock    = pstNextDataBlock;
    }

    return VOS_OK;
}


MBUF_DATABLOCKDESCRIPTOR_S *PMBUF_GetNextDataBlock
(
    ULONG                               ulSubMod,
    ULONG                               ulDataBlockLen,
    ULONG                              *pulBlockNum
)
{
    MBUF_DATABLOCKDESCRIPTOR_S         *pstNextDataBlock    = VOS_NULL_PTR;
    MBUF_DATABLOCKDESCRIPTOR_S         *pstHeadDataBlock    = VOS_NULL_PTR;
    MBUF_DATABLOCKDESCRIPTOR_S         *pstCurrDataBlock    = VOS_NULL_PTR;
    UCHAR                              *pucTempdata         = VOS_NULL_PTR;
    ULONG                               ulActualLen         = 0;


    /* 块数初始化为0 */
    *pulBlockNum    = 0;

    /* 长度为0,无需申请MBUF */
    if (0 == ulDataBlockLen)
    {
        PPPC_WARNING_LOG("ulDataBlockLen is 0, No Need to Alloc Memory!\r\n");
        return VOS_NULL_PTR;
    }


    /* 数据长度大于一次可申请的上限,必然还有下一块 */
    pstHeadDataBlock    = (MBUF_DATABLOCKDESCRIPTOR_S *)PPP_Malloc(
                            sizeof(MBUF_DATABLOCKDESCRIPTOR_S));
    if (VOS_NULL_PTR == pstHeadDataBlock)
    {
        PPPC_ERROR_LOG("Malloc DATABLOCKSCRIPTOR Fail!\r\n");
        return VOS_NULL_PTR;
    }
    PS_MEM_SET(pstHeadDataBlock, 0, sizeof(MBUF_DATABLOCKDESCRIPTOR_S));

    pstCurrDataBlock    = pstHeadDataBlock;

    while ((ulDataBlockLen > 0) && (VOS_NULL_PTR != pstCurrDataBlock))
    {
        if (ulDataBlockLen > PMBUF_MAX_AVAILABLE_MEM_SIZE)
        {
            ulActualLen         = PMBUF_MAX_AVAILABLE_MEM_SIZE;
            ulDataBlockLen     -= PMBUF_MAX_AVAILABLE_MEM_SIZE;

            /* 数据长度大于一次可申请的上限,必然还有下一块 */
            pstNextDataBlock    = (MBUF_DATABLOCKDESCRIPTOR_S *)PPP_Malloc(
                                    sizeof(MBUF_DATABLOCKDESCRIPTOR_S));
            if (VOS_NULL_PTR == pstNextDataBlock)
            {
                PPPC_ERROR_LOG("Malloc DATABLOCKSCRIPTOR Fail!\r\n");

                /* 块数初始化为0 */
                *pulBlockNum    = 0;

                PMBUF_DataBlockDestroy(pstHeadDataBlock);
                return VOS_NULL_PTR;
            }
            PS_MEM_SET(pstNextDataBlock, 0, sizeof(MBUF_DATABLOCKDESCRIPTOR_S));
        }
        else
        {
            ulActualLen         = ulDataBlockLen;
            ulDataBlockLen      = 0;

            /* 数据长度小于等于一次可申请的上限,必然没有下一块,置为空 */
            pstNextDataBlock    = VOS_NULL_PTR;
        }
        pstCurrDataBlock->pstNextDataBlockDescriptor   = pstNextDataBlock;
        (*pulBlockNum)++;

        /* 申请存放报文的内存 */
        pucTempdata = PPP_Malloc(PMBUF_DATABLOCK_SIZE(ulActualLen));
        if (VOS_NULL == pucTempdata)
        {
            PPPC_WARNING_LOG("Malloc Data Block Fail!\r\n");

            /* 块数初始化为0 */
            *pulBlockNum    = 0;

            PMBUF_DataBlockDestroy(pstHeadDataBlock);
            return VOS_NULL_PTR;
        }

        PS_MEM_SET(pucTempdata, 0x0, PMBUF_DATABLOCK_SIZE(ulActualLen));
        pstCurrDataBlock->pucDataBlock       = pucTempdata;
        pstCurrDataBlock->ulDataBlockLength  = PMBUF_DATABLOCK_SIZE(ulActualLen);
        pstCurrDataBlock->pucData            = pucTempdata + PMBUF_RESV_HEAD_SIZE;
        pstCurrDataBlock->ulDataLength       = 0;
        pstCurrDataBlock                     = pstNextDataBlock;
    }

    return pstHeadDataBlock;
}


PF_VOID MBUF_UPDATE_BY_LEN(PMBUF_S* pstMBuf,PF_LONG lLen)
{
    pstMBuf->stDataBlockDescriptor.pucData -= lLen;
    pstMBuf->stDataBlockDescriptor.ulDataLength += lLen;
    pstMBuf->ulTotalDataLength += lLen;

    return;
}

PF_VOID MBUF_UPDATE_BY_LEN_TAIL(PMBUF_S* pstMBuf,PF_LONG lLen)
{
    pstMBuf->stDataBlockDescriptor.ulDataLength += lLen;
    pstMBuf->ulTotalDataLength += lLen;

    return;
}

PF_VOID MBUF_UPDATE_LENGTH(PMBUF_S* pstMBuf,PF_LONG lLen)
{
    pstMBuf->stDataBlockDescriptor.ulDataLength = lLen;
    pstMBuf->ulTotalDataLength = lLen;

    return;
}


VOID PMBUF_ReleasePMbuf(PMBUF_S *pMbuf)
{
    /* 不支持Mbuf链释放 */
    if ((VOS_NULL_PTR == pMbuf) || (pMbuf->pstNextMBuf != VOS_NULL_PTR))
    {
        return;
    }
    sm_pgpAdapt_releaseMbuf(pMbuf);
    return;
}

SM_MBUF_S* PMBUF_DBDescToMbuf(SM_MBUF_DATABLOCKDESCRIPTOR_S *pstDataBlockDescriptor)
{
    SM_MBUF_S *pMbuf = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S *pstCurDataBlockDescriptor;
    ULONG ulBlockNum = 1;
    ULONG ulLen;

    if (pstDataBlockDescriptor == VOS_NULL_PTR)
    {
        return VOS_NULL_PTR;
    }

    ulLen = pstDataBlockDescriptor->ulDataLength;
    pstCurDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
    while (pstCurDataBlockDescriptor != VOS_NULL_PTR)
    {
        ulBlockNum++;
        ulLen += pstCurDataBlockDescriptor->ulDataLength;
        pstCurDataBlockDescriptor = pstCurDataBlockDescriptor->pstNextDataBlockDescriptor;
    }

    pMbuf = PMBUF_GET_PMBUF_FROM_DBD(pstDataBlockDescriptor);
    pMbuf->ulDataBlockNumber = ulBlockNum;
    pMbuf->ulTotalDataLength = ulLen;

    /* 是否做Mbuf检查??? */

    return pMbuf;
}


/*****************************************************************************
 函 数 名  : PMBUF_PullUp
 功能描述  : 保证指定的MBUF中(单个MBUF)有ulLength的数据，
             否则就从下一个DATA中提上来，并且释放数据为空的MBUF数据块
             注意，此函数处理的MBUF使用DBD串的链，并且ulLength不得大于2368字节
 输入参数  : UCHAR * pstMBuf       指向MBUF指针
             ULONG ulLength,       要保证的数据长度
             ULONG ulModuleID      使用者的模块ID
 输出参数  : 无
 返 回 值  : ULONG
 调用函数  : PMBUF_TAILING_SPACE()
             PMBUF_MIN()
 被调函数  :

 修改历史      :
  1.日    期   : 2007年6月14日
    作    者   : l60020798
    修改内容   : 新生成函数

*****************************************************************************/
ULONG PMBUF_PullUp(SM_MBUF_S * pstMBuf, ULONG ulLength, ULONG ulModuleID)
{
    SM_MBUF_S *pstDelMBuf = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstCurdbd = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstNextdbd = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstTmpdbd = VOS_NULL_PTR;
    ULONG ulTotalLength = 0;  /* 需要补充的长度 */
    ULONG ulPullUpLength = 0; /* 本次补充的长度 */

    /* 入参检查 */
    if (VOS_NULL_PTR == pstMBuf)
    {
        return PMBUF_FAIL;
    }
    /* 如果MBUF中的数据长度已经大于等于要求的长度则返回成功 */
    if (pstMBuf->stDataBlockDescriptor.ulDataLength >= ulLength)
    {
        return PMBUF_OK;
    }

    /* 如果总的数据长度都不够要求的长度的话，则返回错误 */
    if (pstMBuf->ulTotalDataLength < ulLength)
    {
        PPPC_WARNING_LOG2("The total data len is not enough, ulTotalDataLength:%d, ulLength:%d",
            pstMBuf->ulTotalDataLength, ulLength);
        return PMBUF_FAIL;
    }

    /* 如果第一个MBUF尾部没有足够的空间的话，则返回错误 */
    ulTotalLength = ulLength - pstMBuf->stDataBlockDescriptor.ulDataLength;
    if ( ulTotalLength > PMBUF_TAILING_SPACE(pstMBuf) )
    {
        PPPC_WARNING_LOG2("The space is not enough, ulTotalLength:%d, space:%d",
            ulTotalLength, PMBUF_TAILING_SPACE(pstMBuf));
        return PMBUF_FAIL;
    }

    pstCurdbd = &(pstMBuf->stDataBlockDescriptor);
    pstNextdbd = pstMBuf->stDataBlockDescriptor.pstNextDataBlockDescriptor;
    while (ulTotalLength > 0 && pstNextdbd != VOS_NULL_PTR)
    {
        ulPullUpLength = PMBUF_MIN (pstNextdbd->ulDataLength, ulTotalLength);

        VOS_MemCpy((UCHAR*)(pstCurdbd->pucData + pstCurdbd->ulDataLength),
               (UCHAR*)pstNextdbd->pucData, (ULONG)ulPullUpLength);

        ulTotalLength -= ulPullUpLength;
        pstCurdbd->ulDataLength += ulPullUpLength;

        pstNextdbd->ulDataLength -= ulPullUpLength;
        pstNextdbd->pucData += ulPullUpLength;

        if ( 0 == pstNextdbd->ulDataLength )
        {
            pstTmpdbd = pstNextdbd->pstNextDataBlockDescriptor;
            pstCurdbd->pstNextDataBlockDescriptor = pstTmpdbd;
#ifdef __VXWORKS_PLATFORM__
            pstDelMBuf = (PMBUF_S *)PMBUF_DTOM((UCHAR*) pstNextdbd->pucDataBlock);
#else
            pstNextdbd->pstNextDataBlockDescriptor = VOS_NULL_PTR;
            pstDelMBuf = PMBUF_DBDescToMbuf(pstNextdbd);
#endif
            sm_pgpAdapt_releaseMbuf(pstDelMBuf);
            pstMBuf->ulDataBlockNumber --;
            if ( 0 == ulTotalLength )
            {
                break;
            }
            pstNextdbd = pstTmpdbd;
            continue;
        }
        if ( 0 == ulTotalLength )
        {
            break;
        }
    }
    return PMBUF_OK;
}

SM_MBUF_DATABLOCKDESCRIPTOR_S* sm_plt_pmbuf_getTailDBD(SM_MBUF_S * pstMBuf)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstCurDataBlockDescriptor = VOS_NULL_PTR;
    if ( VOS_NULL_PTR == pstMBuf )
    {
        return VOS_NULL_PTR;
    }

    for (pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);
         pstDataBlockDescriptor != VOS_NULL_PTR;
         pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        pstCurDataBlockDescriptor = pstDataBlockDescriptor;
    }
    return pstCurDataBlockDescriptor;
}

SM_MBUF_S * sm_plt_mbufReferenceCopy(SM_MBUF_S * pstMBuf, ULONG ulOffset, ULONG ulLength, ULONG ulModuleID)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstNextDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstTailDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_S* pstNewMbuf = VOS_NULL_PTR;
    SM_MBUF_S* pstHeadMbuf = VOS_NULL_PTR;
    ULONG ulCopyLength = 0;
    VOS_INT32 iHead = 0;/* 如果传入的MBUF已经扩展了头，则数据区长度有可能大于规定的数据区长度，这个值为校正值 */

    /* 入参检查 */
    if ( VOS_NULL_PTR == pstMBuf )
    {
        return VOS_NULL_PTR;
    }
    if(ulOffset > pstMBuf->ulTotalDataLength)
    {
        return VOS_NULL_PTR;
    }
    if(ulLength > pstMBuf->ulTotalDataLength - ulOffset)
    {
        return VOS_NULL_PTR;
    }

    /* 通过偏移量找到从哪个MBUF开始拷贝 */
    for(pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);
        ulOffset >= pstDataBlockDescriptor->ulDataLength;
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        ulOffset -= pstDataBlockDescriptor->ulDataLength;
        if (VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
        {
            break;
        }
    }
    /* 先处理新的MBUF链的第一个结点 */
    pstHeadMbuf = sm_pgpAdapt_getMbuf(ulModuleID);
    if ( VOS_NULL_PTR == pstHeadMbuf)
    {
        return VOS_NULL_PTR;
    }

    /* 拷贝用户信息 */
    (VOID)VOS_MemCpy((VOID*)(&pstHeadMbuf->stUserTagData),
                       (VOID*)(&pstMBuf->stUserTagData),
                       sizeof(pstMBuf->stUserTagData));
    ulCopyLength = PMBUF_MIN(ulLength, pstDataBlockDescriptor->ulDataLength - ulOffset);
    iHead = (VOS_INT32)(pstDataBlockDescriptor->pucData - (pstDataBlockDescriptor->pucDataBlock + 512));
    /* 拷贝数据 */
    (VOID)VOS_MemCpy(pstHeadMbuf->stDataBlockDescriptor.pucData + iHead,
                       pstDataBlockDescriptor->pucData + ulOffset ,
                       ulCopyLength);
    pstHeadMbuf->stDataBlockDescriptor.pucData += iHead;
    pstHeadMbuf->stDataBlockDescriptor.ulDataLength = ulCopyLength;
    pstHeadMbuf->ulTotalDataLength = ulLength;
    ulOffset = 0;
    ulLength = ulLength - ulCopyLength;
    if ( 0 == ulLength )
    {
        return pstHeadMbuf;
    }
    /* 如果一个数据块就可以搞定的话，直接返回新的MBUF */
    if ( VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        return pstHeadMbuf;
    }
    pstNextDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
    while ( (pstNextDataBlockDescriptor != VOS_NULL_PTR)&& (ulLength > 0))
    {
        pstNewMbuf = sm_pgpAdapt_getMbuf(ulModuleID);
        if ( VOS_NULL_PTR == pstNewMbuf)
        {
            sm_pgpAdapt_releaseMbuf(pstMBuf);
            return VOS_NULL_PTR;
        }
        pstHeadMbuf->ulDataBlockNumber ++;
        ulCopyLength = PMBUF_MIN(ulLength, pstNextDataBlockDescriptor->ulDataLength);

        iHead = (VOS_INT32)(pstNextDataBlockDescriptor->pucData - (pstNextDataBlockDescriptor->pucDataBlock + 512));
        (VOID)VOS_MemCpy(pstNewMbuf->stDataBlockDescriptor.pucData + iHead,
                           pstNextDataBlockDescriptor->pucData ,
                           ulCopyLength);
        pstNewMbuf->stDataBlockDescriptor.pucData += iHead;
        pstNewMbuf->stDataBlockDescriptor.ulDataLength = ulCopyLength;
        ulLength = ulLength - ulCopyLength;

        /* 串链 */
        pstTailDataBlockDescriptor = sm_plt_pmbuf_getTailDBD(pstHeadMbuf);
        if ( VOS_NULL_PTR == pstTailDataBlockDescriptor)
        {
            sm_pgpAdapt_releaseMbuf(pstMBuf);
            sm_pgpAdapt_releaseMbuf(pstNewMbuf);
            return VOS_NULL_PTR;
        }
        pstTailDataBlockDescriptor->pstNextDataBlockDescriptor = &(pstNewMbuf->stDataBlockDescriptor);
        if (0 == ulLength)
        {
            break;
        }
        /* 取下一个MBUF */
        pstNextDataBlockDescriptor = pstNextDataBlockDescriptor->pstNextDataBlockDescriptor;
    }
    return pstHeadMbuf;
}


/*****************************************************************************
 函 数 名  : PMBUF_CutHeadInMultiDataBlock
 功能描述  : 从头部开始在多个数据块中切除数据,如果本MBUF数据块长度为0，并不释放
 输入参数  : PMBUF_S * pstMBuf
             ULONG ulLength
 输出参数  : 无
 返 回 值  : VOID
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2007年7月19日
    作    者   : l60020798
    修改内容   : 老函数，没有修改，加了个函数头而已

*****************************************************************************/
VOID PMBUF_CutHeadInMultiDataBlock(PMBUF_S * pstMBuf, ULONG ulLength)
{
    ULONG ulMyLength = 0;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;

    /*pstMBuf has been checked by caller*/
    /*the following code can process the situation that ulLength > pstMBuf->ulTotalDataLength*/
    if (VOS_NULL_PTR == pstMBuf)
    {
        return;
    }

    ulMyLength = (ulLength);
    pstDataBlockDescriptor = &((pstMBuf)->stDataBlockDescriptor);
    while(pstDataBlockDescriptor != VOS_NULL_PTR)
    {
        if(pstDataBlockDescriptor->ulDataLength <= ulMyLength)
        {
            ulMyLength -= pstDataBlockDescriptor->ulDataLength;
            pstDataBlockDescriptor->pucData += pstDataBlockDescriptor->ulDataLength;
            pstDataBlockDescriptor->ulDataLength = 0;

            /*next*/
            pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        }
        else
        {
            pstDataBlockDescriptor->ulDataLength -= ulMyLength;
            pstDataBlockDescriptor->pucData += ulMyLength;
            ulMyLength = 0;
            break;
        }
    }
    /*whether terminate from break or the loop, the following is OK.*/
    pstMBuf->ulTotalDataLength -= ((ulLength) - ulMyLength);
    return ;
}



/*****************************************************************************
 函 数 名  : PMBUF_CopyDataFromPMBufToBuffer
 功能描述  : 将MBUF中的数据拷贝到BUFFER中
 输入参数  : PMBUF_S * pstMBuf
             ULONG ulOffset
             ULONG ulLength
             UCHAR * pucBuffer
 输出参数  : 无
 返 回 值  : ULONG
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2007年7月19日
    作    者   : l60020798
    修改内容   : 老函数，没有修改，加了函数头而已

*****************************************************************************/
ULONG PMBUF_CopyDataFromPMBufToBuffer(SM_MBUF_S * pstMBuf, ULONG ulOffset, ULONG ulLength, UCHAR * pucBuffer)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor;
    ULONG ulCopyLength;

    if(pstMBuf == VOS_NULL_PTR)
    {
        return PMBUF_FAIL;
    }
    if(pucBuffer == VOS_NULL_PTR)
    {
        return PMBUF_FAIL;
    }
    if(ulOffset > pstMBuf->ulTotalDataLength)
    {
        return PMBUF_FAIL;
    }
    if(ulLength > pstMBuf->ulTotalDataLength - ulOffset)
    {
        return PMBUF_FAIL;
    }
    if(ulLength == 0)
    {
        return PMBUF_OK;
    }
    for(pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);\
        ulOffset >= pstDataBlockDescriptor->ulDataLength;\
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        ulOffset -= pstDataBlockDescriptor->ulDataLength;
        if (VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
        {
           return PMBUF_FAIL;
        }
    }
    while(ulLength > 0 )
    {
        ulCopyLength = PMBUF_MIN(pstDataBlockDescriptor->ulDataLength - ulOffset, ulLength);
        (VOID)VOS_MemCpy(pucBuffer, pstDataBlockDescriptor->pucData + ulOffset, ulCopyLength);

        ulLength -= ulCopyLength;
        pucBuffer += ulCopyLength;
        ulOffset = 0;
        if ( 0 == ulLength )
        {
            break;
        }
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        if (VOS_NULL_PTR == pstDataBlockDescriptor)
        {
            return PMBUF_FAIL;
        }
    }
    return PMBUF_OK;
}



/*****************************************************************************
 函 数 名  : PMBUF_CopyDataFromBufferToPMBuf
 功能描述  : 将BUF数据拷贝到MBUF指定位置,最长拷贝16K数据
             并且新的MBUF链是使用DBD串起来的MBUF链
 输入参数  : PMBUF_S * pstMBuf   指向MBUF指针
             ULONG ulOffset     MBUF中的偏移
             ULONG ulLength     数据长度
             UCHAR * pucBuffer  BUF
             ULONG ulModuleID
 输出参数  : 无
 返 回 值  : 成功返回MBUF_OK;失败返回MBUF_FAIL;
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2007年7月4日
    作    者   : l60020798
    修改内容   : 新生成函数

*****************************************************************************/
ULONG PMBUF_CopyDataFromBufferToPMBuf(SM_MBUF_S * pstMBuf, ULONG ulOffset,
                              ULONG ulLength, UCHAR * pucBuffer, ULONG ulModuleID)
{
    ULONG                               ulCopyLength                = 0;
    ULONG                               ulTotalLength               = 0;
    ULONG                               ulDataLength                = 0;
    ULONG                               ulBlockNum                  = 0;
    ULONG                               ulSurplusDBDNum             = 0;
    SM_MBUF_DATABLOCKDESCRIPTOR_S      *pstDataBlockDescriptor      = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S      *pstTemDataBlockDescriptor   = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S      *pstLastDataBlockDescriptor  = VOS_NULL_PTR;


    /* 入参检查 */
    if( 0 == ulLength )
    {
        return PMBUF_OK;
    }
    if(( VOS_NULL_PTR == pstMBuf )||( VOS_NULL_PTR == pucBuffer ))
    {
        return PMBUF_FAIL;
    }
    if(ulOffset > pstMBuf->ulTotalDataLength)
    {
        return PMBUF_FAIL;
    }

    /* 不支持以MBUF头指针相链接的MBUF */
    if ( VOS_NULL_PTR != pstMBuf->pstNextMBuf )
    {
        return PMBUF_FAIL;
    }

    /*buff超长，只处理16k字节，后面的数据将被丢弃 */
    if ( ulLength > 16*1024 )
    {
        ulLength = 16*1024;
    }
    ulTotalLength = ulLength + ulOffset;

    /* 得到首个MBUF的数据块指针 */
    pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);

    /* 通过偏移量找到第一个要拷贝的MBUF,也就是最后一个非空的DataBlock */
    while (VOS_NULL_PTR != pstDataBlockDescriptor)
    {
        if (ulOffset > pstDataBlockDescriptor->ulDataLength)
        {
            if ( VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor )
            {
                /* ulOffset比ulDataLength大下一个DataBlock却已经空为异常场景,
                   已经不可能组成正常报文,直接返回异常退出 */
                return PMBUF_FAIL;
            }

            ulOffset -= pstMBuf->stDataBlockDescriptor.ulDataLength;
            pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        }
        else if (ulOffset == pstDataBlockDescriptor->ulDataLength)
        {
            if (ulOffset == 0)
            {
                pstLastDataBlockDescriptor  = pstDataBlockDescriptor;
                break;
            }

            ulOffset -= pstMBuf->stDataBlockDescriptor.ulDataLength;

            /* ulOffset与ulDataLength相同,则下一块为空时申请新DataBlock */
            if (VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
            {
                ulDataLength
                    = PS_MIN(ulLength, PMBUF_MAX_AVAILABLE_MEM_SIZE);
                pstLastDataBlockDescriptor
                    = (SM_MBUF_DATABLOCKDESCRIPTOR_S *)PMBUF_GetNextDataBlock(
                        ulModuleID, ulDataLength, &ulBlockNum);
                if (VOS_NULL_PTR == pstLastDataBlockDescriptor)
                {
                    return PMBUF_FAIL;
                }
                pstDataBlockDescriptor->pstNextDataBlockDescriptor = pstLastDataBlockDescriptor;

                pstMBuf->ulDataBlockNumber += ulBlockNum;
            }
            else
            {
                pstLastDataBlockDescriptor  = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
            }

            break;
        }
        else
        {
            /* offset=0或当前DataBlock还能存放数据时从此处退出 */
            pstLastDataBlockDescriptor  = pstDataBlockDescriptor;
            break;
        }
    }


    /* 将数据拷贝到MBUF中 */
    while (ulLength > 0)
    {
        ULONG ulUsableLength = 0;
        ulUsableLength = (ULONG)(pstLastDataBlockDescriptor->pucDataBlock
                        + pstLastDataBlockDescriptor->ulDataBlockLength
                        - pstLastDataBlockDescriptor->pucData
                        - ulOffset);

        ulCopyLength = PMBUF_MIN(ulUsableLength, ulLength);

        (VOID)VOS_MemCpy((UCHAR*)(pstLastDataBlockDescriptor->pucData) + ulOffset,
                          pucBuffer, ulCopyLength);

        ulLength    -= ulCopyLength;
        pucBuffer   += ulCopyLength;

        /* 重新设置数据长度,用户认为的数据长度应该是拷贝的长度+偏移量预留的长度 */
        pstLastDataBlockDescriptor->ulDataLength = ulCopyLength + ulOffset;
        ulOffset = 0;

        /* 如果本次拷贝已经完成了全部的数据拷贝，则退出循环 */
        if (0 == ulLength)
        {
            break;
        }
        /* 如果数据没有拷贝完，并且后续的MBUF链为空的话，则新申请MBUF */
        if (VOS_NULL_PTR == pstLastDataBlockDescriptor->pstNextDataBlockDescriptor)
        {
            ulDataLength
                = PS_MIN(ulLength, PMBUF_MAX_AVAILABLE_MEM_SIZE);
            pstTemDataBlockDescriptor
                = (SM_MBUF_DATABLOCKDESCRIPTOR_S *)PMBUF_GetNextDataBlock(
                    ulModuleID, ulDataLength, &ulBlockNum);
            if (VOS_NULL_PTR == pstTemDataBlockDescriptor)
            {
                return PMBUF_FAIL;
            }
            pstLastDataBlockDescriptor->pstNextDataBlockDescriptor
                = pstTemDataBlockDescriptor;
            pstMBuf->ulDataBlockNumber += ulBlockNum;
        }

        pstLastDataBlockDescriptor
            = pstLastDataBlockDescriptor->pstNextDataBlockDescriptor;
    }

    pstTemDataBlockDescriptor = pstLastDataBlockDescriptor->pstNextDataBlockDescriptor;
    while( VOS_NULL_PTR != pstTemDataBlockDescriptor )
    {
        ulSurplusDBDNum++;
        pstTemDataBlockDescriptor = pstTemDataBlockDescriptor->pstNextDataBlockDescriptor;
    }
    /* 如果MBUF ulLength以后还有DBD链 则将随后的链删除 */
    if( VOS_NULL_PTR != pstLastDataBlockDescriptor->pstNextDataBlockDescriptor )
    {
        PMBUF_DataBlockDestroy(pstLastDataBlockDescriptor->pstNextDataBlockDescriptor);
    }

    pstMBuf->ulDataBlockNumber = pstMBuf->ulDataBlockNumber - ulSurplusDBDNum;
    pstLastDataBlockDescriptor->pstNextDataBlockDescriptor = VOS_NULL_PTR;

    /* 设置数据总长度 */
    pstMBuf->ulTotalDataLength = ulTotalLength;

    return PMBUF_OK;
}

ULONG PMBUF_CutTail(SM_MBUF_S * pstMBuf, ULONG ulLength)
{
    ULONG ulSurplusLength = 0;
    ULONG ulCutDataBlockNumber = 0;
    SM_MBUF_S* pstRelMbuf = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstNextDataBlockDescriptor = VOS_NULL_PTR;

    if ( VOS_NULL_PTR == pstMBuf )
    {
        return PMBUF_FAIL;
    }

    if (0 == ulLength)
    {
        return PMBUF_OK;
    }

    if (ulLength > pstMBuf->ulTotalDataLength )
    {
        return PMBUF_FAIL;
    }
    /* 修改MBUF数据区的长度为剩余长度 */
    ulSurplusLength = pstMBuf->ulTotalDataLength - ulLength;
    pstMBuf->ulTotalDataLength = ulSurplusLength;

    /*ulSurplusLength 将作为偏移量，用来找到从哪个MBUF开始把后面的数据截取掉 */
    pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);
    while ( ulSurplusLength > pstDataBlockDescriptor->ulDataLength )
    {
        ulSurplusLength -= pstDataBlockDescriptor->ulDataLength;
        if (VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
        {
            return PMBUF_FAIL;
        }

        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
    }
    pstDataBlockDescriptor->ulDataLength = ulSurplusLength;

    pstNextDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
    pstDataBlockDescriptor->pstNextDataBlockDescriptor = VOS_NULL_PTR;/* 破坏原来的DBD链 */
    pstDataBlockDescriptor = pstNextDataBlockDescriptor;
    /* 开始释放后面的MBUF */
    while ( pstDataBlockDescriptor != VOS_NULL_PTR )
    {
        pstNextDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;

        pstDataBlockDescriptor->pstNextDataBlockDescriptor = VOS_NULL_PTR;
        pstRelMbuf = PMBUF_DBDescToMbuf(pstDataBlockDescriptor);

        sm_pgpAdapt_releaseMbuf(pstRelMbuf);
        ulCutDataBlockNumber ++;
        pstDataBlockDescriptor = pstNextDataBlockDescriptor;
    }
    /* 设置MBUF数据块的个数 */
    pstMBuf->ulDataBlockNumber -= ulCutDataBlockNumber;
    return PMBUF_OK;

}

SM_ULONG sm_plt_concatenateMbuf(SM_MBUF_S * pstDestinationMBuf, SM_MBUF_S * pstSourceMBuf, SM_ULONG ulModuleID)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S *pstDataBlockDescriptor = VOS_NULL_PTR;

    /* 入参检测 */
    if ( (VOS_NULL_PTR == pstDestinationMBuf) || (VOS_NULL_PTR == pstSourceMBuf) )
    {
        return PMBUF_FAIL;
    }

    if (pstDestinationMBuf == pstSourceMBuf)
    {
        return PMBUF_FAIL;
    }

    for ( pstDataBlockDescriptor = &(pstDestinationMBuf->stDataBlockDescriptor);\
        pstDataBlockDescriptor->pstNextDataBlockDescriptor != VOS_NULL_PTR;\
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor )
    {
        /* 找到最后一个MBUF */
    }
    pstDataBlockDescriptor->pstNextDataBlockDescriptor = &pstSourceMBuf->stDataBlockDescriptor;
    pstDestinationMBuf->ulTotalDataLength += pstSourceMBuf->ulTotalDataLength;
    pstDestinationMBuf->ulDataBlockNumber += pstSourceMBuf->ulDataBlockNumber;
    pstSourceMBuf->ulTotalDataLength = 0;
    pstSourceMBuf->ulDataBlockNumber = 1;
    return PMBUF_OK;
}


SM_MBUF_DATABLOCKDESCRIPTOR_S* sm_plt_getTailDBDMbuf(SM_MBUF_S * pstMBuf)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstCurDataBlockDescriptor = VOS_NULL_PTR;
    if ( VOS_NULL_PTR == pstMBuf )
    {
        return VOS_NULL_PTR;
    }

    for (pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);
         pstDataBlockDescriptor != VOS_NULL_PTR;
         pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        pstCurDataBlockDescriptor = pstDataBlockDescriptor;
    }
    return pstCurDataBlockDescriptor;
}


ULONG sm_plt_neatConcatenateMbuf(SM_MBUF_S * pstDestinationMBuf, SM_MBUF_S * pstSourceMBuf,
                                 SM_MBUF_DATABLOCKDESCRIPTOR_S ** ppstLastDataBlockDescriptor,
                                 SM_ULONG ulModuleID)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstNextDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_S * pstRealMBUF = VOS_NULL_PTR;
    SM_ULONG ulRemainingSpace = 0;

    /* 入参检查 */
    if ((VOS_NULL_PTR == pstDestinationMBuf) || (VOS_NULL_PTR == pstSourceMBuf) || (VOS_NULL_PTR == ppstLastDataBlockDescriptor) )
    {
        return PMBUF_FAIL;
    }

    if (pstDestinationMBuf == pstSourceMBuf)
    {
        return PMBUF_FAIL;
    }

    /* 寻找目的MBUF的最后一个DBD */
    for (pstDataBlockDescriptor = &(pstDestinationMBuf->stDataBlockDescriptor);
         pstDataBlockDescriptor->pstNextDataBlockDescriptor != VOS_NULL_PTR;
         pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
        /* do noting */
    }


    pstDataBlockDescriptor->pstNextDataBlockDescriptor = &(pstSourceMBuf->stDataBlockDescriptor);
    pstDestinationMBuf->ulTotalDataLength += pstSourceMBuf->ulTotalDataLength;
    pstDestinationMBuf->ulDataBlockNumber += pstSourceMBuf->ulDataBlockNumber;
    pstSourceMBuf->ulTotalDataLength = 0;
    pstSourceMBuf->ulDataBlockNumber = 1;

    while(pstDataBlockDescriptor->pstNextDataBlockDescriptor != VOS_NULL_PTR)
    {
        pstNextDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        /* 计算尾部剩余空间 */
        ulRemainingSpace = (ULONG)(pstDataBlockDescriptor->pucDataBlock + pstDataBlockDescriptor->ulDataBlockLength
                           - (pstDataBlockDescriptor->pucData + pstDataBlockDescriptor->ulDataLength));
        if((pstDataBlockDescriptor->ulType == pstNextDataBlockDescriptor->ulType)
           && (ulRemainingSpace >= pstNextDataBlockDescriptor->ulDataLength)
           && (pstNextDataBlockDescriptor->ulDataLength < MBUF_TOO_SMALL_LENGTH))
        {
            (VOID)VOS_MemCpy(pstDataBlockDescriptor->pucData + pstDataBlockDescriptor->ulDataLength,
                               pstNextDataBlockDescriptor->pucData,
                               pstNextDataBlockDescriptor->ulDataLength);

            pstDataBlockDescriptor->ulDataLength += pstNextDataBlockDescriptor->ulDataLength;

            pstDataBlockDescriptor->pstNextDataBlockDescriptor = pstNextDataBlockDescriptor->pstNextDataBlockDescriptor;

            pstNextDataBlockDescriptor->pstNextDataBlockDescriptor = VOS_NULL_PTR;
            /* 找到数据区域为NULL的MBUF头 */
            pstRealMBUF = PMBUF_DBDescToMbuf(pstNextDataBlockDescriptor);
            /* 释放掉MBUF*/
            sm_pgpAdapt_releaseMbuf(pstRealMBUF);
            pstDestinationMBuf->ulDataBlockNumber --;
        }
        else
        {
            /* 取下一个MBUF的数据 */
            pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        }
    }
    /* for the last one */
    * ppstLastDataBlockDescriptor = sm_plt_getTailDBDMbuf(pstDestinationMBuf);
    return PMBUF_OK;
}

SM_MBUF_S* sm_plt_createForControlPacket(SM_ULONG ulReserveHeadSpace, SM_ULONG ulLength,
                                         SM_ULONG ulType, SM_ULONG ulModuleID)
{
    SM_MBUF_S* pstMBuf = VOS_NULL_PTR;

    /* 预留的空间+数据长度 > MBUF数据区最大长度 */
    if( (ulReserveHeadSpace > PMBUF_DATA_MAXLEN)
        || (ulLength > PMBUF_DATA_MAXLEN)
        || (ulReserveHeadSpace + ulLength > PMBUF_DATA_MAXLEN) )
    {
        return VOS_NULL_PTR;
    }

    /* 申请一个MBUF */
    pstMBuf = sm_pgpAdapt_getMbuf(ulModuleID);
    if( VOS_NULL_PTR == pstMBuf )
    {
        return VOS_NULL_PTR;
    }

    /* 设置MBUF相关信息 */
    pstMBuf->stDataBlockDescriptor.ulType = ulType;
    pstMBuf->stDataBlockDescriptor.pstNextDataBlockDescriptor = VOS_NULL_PTR;
    pstMBuf->stDataBlockDescriptor.pucData = pstMBuf->stDataBlockDescriptor.pucData + ulReserveHeadSpace;
    pstMBuf->stDataBlockDescriptor.ulDataLength = 0;

    pstMBuf->ulTotalDataLength = pstMBuf->stDataBlockDescriptor.ulDataLength;
    pstMBuf->ulDataBlockNumber = 1;

    /* 保证申请的mbuf是一个单独的结点 */
    pstMBuf->pstNextMBuf = VOS_NULL_PTR;

    return pstMBuf;
}

SM_MBUF_DATABLOCKDESCRIPTOR_S * sm_plt_CreateDBDescriptorAndDB(SM_ULONG ulDataBlockLength,
                                                               SM_ULONG ulType,
                                                               SM_ULONG ulModuleID)
{
    SM_MBUF_S* pstMbuf = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S* pstMbufDataDescriptor = VOS_NULL_PTR;

    /* 入参检查 */
    if ( ulDataBlockLength > PMBUF_DATA_SIZE)
    {
        return VOS_NULL_PTR;
    }

    /* 先申请一个MBUF */
    pstMbuf = sm_pgpAdapt_getMbuf(ulModuleID);
    if ( VOS_NULL_PTR == pstMbuf )
    {
        return VOS_NULL_PTR;
    }

    /* 设置相关信息 */
    pstMbufDataDescriptor = (&pstMbuf->stDataBlockDescriptor);
    pstMbufDataDescriptor->pstNextDataBlockDescriptor = VOS_NULL_PTR;
    pstMbufDataDescriptor->ulType = ulType;

    return pstMbufDataDescriptor;
}

SM_UCHAR *sm_plt_AppendMemorySpace(SM_MBUF_S * pstMBuf, SM_ULONG ulLength, SM_ULONG ulModuleID)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;
    UCHAR * pucAppending = VOS_NULL_PTR;
    SM_ULONG ulType = 0;

    if (VOS_NULL_PTR == pstMBuf)
    {
        return VOS_NULL_PTR;
    }
    if(ulLength > PMBUF_DATA_MAXLEN)
    {
        return VOS_NULL_PTR;
    }

    ulType = pstMBuf->stDataBlockDescriptor.ulType;

    /* 找到最后一个DBD */
    for(pstDataBlockDescriptor = &pstMBuf->stDataBlockDescriptor;\
        pstDataBlockDescriptor->pstNextDataBlockDescriptor != VOS_NULL_PTR;\
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor)
    {
    }
    if(ulLength == 0)
    {
        return (pstDataBlockDescriptor->pucData + pstDataBlockDescriptor->ulDataLength);
    }

    /* 如果尾部空间不足的话则新申请一个数据区域为空的MBUF供用户使用 */
    if((SM_ULONG)(pstDataBlockDescriptor->pucDataBlock + pstDataBlockDescriptor->ulDataBlockLength \
        - (pstDataBlockDescriptor->pucData + pstDataBlockDescriptor->ulDataLength) ) < ulLength)
    {
        pstDataBlockDescriptor->pstNextDataBlockDescriptor
            = sm_plt_CreateDBDescriptorAndDB(PMBUF_DATA_MAXLEN + PMBUF_TRANSMIT_RESERVED_SIZE,
                                             ulType, ulModuleID);
        if( pstDataBlockDescriptor->pstNextDataBlockDescriptor == VOS_NULL_PTR)
        {
            return VOS_NULL_PTR;
        }
        pstMBuf->ulDataBlockNumber ++;
        /* 使pstDataBlockDescriptor 为最后一个数据块的指针 */
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;

        pstDataBlockDescriptor->ulDataLength = ulLength;
        pucAppending = pstDataBlockDescriptor->pucData;
        pstMBuf->ulTotalDataLength += ulLength;
        return pucAppending;
    }
    else
    {
        pucAppending = pstDataBlockDescriptor->pucData + pstDataBlockDescriptor->ulDataLength;
        pstDataBlockDescriptor->ulDataLength += ulLength;
        pstMBuf->ulTotalDataLength += ulLength;
    }


    return (pucAppending);
}

SM_ULONG sm_simulatort_releasembufandDatablack(SM_MBUF_S *pPFData )
{
    SM_MBUF_S *pstMBuf = SM_NULL_PTR;
    if ( pPFData == SM_NULL_PTR )
    {
        return SM_RET_ERR;
    }
    if ( pPFData->ulDataBlockNumber > 1 )
    {
        PMBUF_S *pstMBuf = VOS_NULL_PTR;
        PMBUF_DATABLOCKDESCRIPTOR_S *pstDataBlockDescriptor = VOS_NULL_PTR;
        SM_UCHAR *pucData = VOS_NULL_PTR;
        SM_ULONG ulTotalDataLength = 0;
        SM_ULONG ulDataLength = 0;

        pstDataBlockDescriptor = &pPFData->stDataBlockDescriptor;
        while ( SM_NULL_PTR != pstDataBlockDescriptor)
        {
            pstMBuf = (PMBUF_S *)(pstDataBlockDescriptor->pucDataBlock - sizeof(PMBUF_S));
            pstDataBlockDescriptor =  pstDataBlockDescriptor->pstNextDataBlockDescriptor;
            pstMBuf->stDataBlockDescriptor.pstNextDataBlockDescriptor = VOS_NULL_PTR;
            sm_pgpAdapt_releaseMbuf(pstMBuf);
        }
    }
    else
    {
        sm_pgpAdapt_releaseMbuf(pPFData);
    }
    return SM_RET_OK;
}
SM_ULONG sm_simulatort_pmbuf_cutpartforwin32(SM_MBUF_S * pstMBuf, SM_ULONG ulOffset, SM_ULONG ulLength)
{
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptor = VOS_NULL_PTR;
    SM_MBUF_DATABLOCKDESCRIPTOR_S * pstDataBlockDescriptorFirstCut = VOS_NULL_PTR;
    SM_ULONG ulCutLength = 0;
    SM_ULONG ulOriginalLength = 0;

    if(pstMBuf == VOS_NULL_PTR)
    {
        return SM_RET_ERR;
    }
    if(ulOffset > pstMBuf->ulTotalDataLength)
    {
        return SM_RET_ERR;
    }
    if(ulLength > pstMBuf->ulTotalDataLength - ulOffset)
    {
        return SM_RET_ERR;
    }

    if(ulLength == 0)/*ulOffset == ulTotalDataLength*/
    {
        return SM_RET_OK;
    }

    ulOriginalLength = ulLength;
    pstDataBlockDescriptor = &(pstMBuf->stDataBlockDescriptor);
    /*find the DB*/
    while( ulOffset >= pstDataBlockDescriptor->ulDataLength)
    {
            ulOffset -= pstDataBlockDescriptor->ulDataLength;
            if (VOS_NULL_PTR == pstDataBlockDescriptor->pstNextDataBlockDescriptor)
            {
                return SM_RET_ERR;
            }
             pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
    }
    /*ulOffset < pstDataBlockDescriptor->ulDataLength*/
    pstDataBlockDescriptorFirstCut = pstDataBlockDescriptor;
    while(ulLength > 0)
    {
        ulCutLength = PMBUF_MIN(pstDataBlockDescriptor->ulDataLength - ulOffset, ulLength);
        ulLength -= ulCutLength;
        if(ulLength == 0)
        {
            /*cut end*/
            if(pstDataBlockDescriptor == pstDataBlockDescriptorFirstCut && ulOffset != 0)
            {
                /*move*/
                (VOID)VOS_MemCpy(pstDataBlockDescriptor->pucData + ulOffset,
                                     pstDataBlockDescriptor->pucData + ulOffset + ulCutLength,
                                     pstDataBlockDescriptor->ulDataLength - (ulOffset + ulCutLength));

                pstMBuf->ulTotalDataLength -= ulOriginalLength;
                pstDataBlockDescriptor->ulDataLength -= ulCutLength;
                return SM_RET_OK;
            }/*move*/
            else
            {
                /*need not move*/
                pstDataBlockDescriptor->ulDataLength -= ulCutLength;
                pstDataBlockDescriptor->pucData += ulCutLength;
                pstMBuf->ulTotalDataLength -= ulOriginalLength;
                return SM_RET_OK;
            }
        }

        /*cut next*/
        pstDataBlockDescriptor->ulDataLength -= ulCutLength;
        ulOffset = 0;
        pstDataBlockDescriptor = pstDataBlockDescriptor->pstNextDataBlockDescriptor;
        if (VOS_NULL_PTR == pstDataBlockDescriptor)
        {
            return SM_RET_ERR;
        }
    }/*while, never exit from here */
    /*never come here*/
    pstMBuf->ulTotalDataLength -= ulOriginalLength;
    return SM_RET_OK;
}





SM_VOID sm_pgpAdapt_setMbufMsgCode
(
    PMBUF_S* pstMbuf,
    SM_ULONG ulMsgCode
)
{
    SM_ULONG *pulMsgCode = 0;

    pulMsgCode = ((SM_ULONG *)(pstMbuf->stDataBlockDescriptor.pucData)) - 1;

    *pulMsgCode = ulMsgCode;

}


SM_ULONG sm_pgpAdapt_getMbufMsgCode
(
    PMBUF_S* pstMbuf
)
{
    SM_ULONG *pulMsgCode = 0;

    pulMsgCode = (SM_ULONG *)(pstMbuf->stDataBlockDescriptor.pucData) - 1;

    return *pulMsgCode;
}

/*****************************************************************************
 函 数 名  : _pgpAdapt_clearLinkInMbufAndPktid
 功能描述  : 清除Mbuf和pkt Id的对应关系
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : sm_pgpAdapt_releaseMbuf

 修改历史      :
  1.日    期   : 2010年12月22日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID _pgpAdapt_clearLinkInMbufAndPktid
(
    IN      SM_ULONG    ulPacketId
)
{
    SM_PGPADAPT_PACKET_INFO_S* pstPgpAdaptPktInfo = SM_NULL_PTR;
    SM_ULONG ulPacketIndex = 0;

    /* 因为数组下标从0开始，Packet Id从1开始，故要减1*/
    ulPacketIndex = ulPacketId - 1;

    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    pstPgpAdaptPktInfo[ulPacketIndex].pstMbuf = SM_NULL_PTR;
    pstPgpAdaptPktInfo[ulPacketIndex].ulPacketId = 0;

    return;
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_findFreeMbuf
 功能描述  : 在全局变量中查找非使用态的Mbuf
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : sm_pgpAdapt_getPktIdByMbuf

 修改历史      :
  1.日    期   : 2010年12月18日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_ULONG sm_pgpAdapt_findFreeMbuf
(
    OUT     MBUF_S**     ppstMbuf
)
{
    SM_PGPADAPT_MBUF_INFO_S*  pstPgpAdaptMbufInfo = SM_NULL_PTR;
    MBUF_S*  pstMbuf = SM_NULL_PTR;
    SM_ULONG ulPacketId = 0;
    SM_ULONG ulIndex = 0;
    SM_ULONG ulIndexMax = 0;
    SM_ULONG ulFindFlag = SM_FALSE;
    SM_ULONG ulCount = 0;


    pstPgpAdaptMbufInfo = &(g_stPgpAdaptMbufInfo);

    /* 循环使用，认为缓存不会超过500个mbuf*/
    if (g_ulCurMbufIndex >= SM_PGPADAPT_MBUF_MAX_NUM)
    {
        g_ulCurMbufIndex = 0;
    }

    /* 先在当前索引的后面查找 */
    ulIndexMax = SM_PGPADAPT_MBUF_MAX_NUM;
    for (ulIndex = g_ulCurMbufIndex; ulIndex < SM_PGPADAPT_MBUF_MAX_NUM;)
    {
        if(ulCount >= SM_PGPADAPT_MBUF_MAX_NUM)
        {
            break;
        }


        if (SM_FALSE == pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulIndex].ulUsed)
        {
            ulPacketId = ulIndex;
            ulFindFlag = SM_TRUE;
            break;
        }

        ulIndex++;
        ulCount++;

        /* 当前索引后面没找到的时候再从头查找 */
        if (ulIndex >= SM_PGPADAPT_MBUF_MAX_NUM)
        {
            ulIndex = 0;
            ulIndexMax = g_ulCurMbufIndex;
        }


    }

    if (SM_TRUE == ulFindFlag)
    {
        pstMbuf = &(pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulPacketId].stMbuf);
        *ppstMbuf = pstMbuf;

        /* 设置Mbuf状态为使用态 */
        pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulPacketId].ulUsed = SM_TRUE;

        pstPgpAdaptMbufInfo->ulMbufMallocNum++;
        g_ulCurMbufIndex = ulPacketId;

        return SM_RET_OK;
    }

    return SM_RET_ERR;
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_getMbufDataBlock
 功能描述  : 为Mbuf申请pucDataBlock，申请个数增加
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : sm_pgpAdapt_getMbuf

 修改历史      :
  1.日    期   : 2010年12月20日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID sm_pgpAdapt_getMbufDataBlock
(
    IN      MBUF_S*                         pstMbuf,
    OUT      PMBUF_DATABLOCKDESCRIPTOR_S*   pstDataBlockDescriptor,
    IN      SM_ULONG                        ulMID
)
{
    SM_PGPADAPT_MBUF_INFO_S*  pstPgpAdaptMbufInfo = SM_NULL_PTR;
    SM_UCHAR*   pucDataBlock = SM_NULL_PTR;

    pstPgpAdaptMbufInfo = &(g_stPgpAdaptMbufInfo);



    pucDataBlock = (SM_UCHAR*)(pstMbuf + 1);

    VOS_MemSet(pucDataBlock, 0, SM_PGPADAPT_DATABLOCK_MAX_LENGTH);

    pstDataBlockDescriptor->pucDataBlock = pucDataBlock;
    pstDataBlockDescriptor->pucData = pucDataBlock + SM_PGP_MBUF_DATABLOCK_OFFSET;
    pstDataBlockDescriptor->ulDataBlockLength = SM_PGP_MBUF_DATABLOCK_MAX_LEN;
    pstDataBlockDescriptor->pstNextDataBlockDescriptor = SM_NULL_PTR;
    pstDataBlockDescriptor->ulDataLength = 0;

    pstPgpAdaptMbufInfo->astPgpAdaptMbuf[g_ulCurMbufIndex].ulDataBlockMallocNum++;
    pstPgpAdaptMbufInfo->astPgpAdaptMbuf[g_ulCurMbufIndex].ulMID = ulMID; /* 记录模块号 */

    return;
}
MBUF_S* sm_pgpAdapt_DTOM(UCHAR *pData)
{
    return (MBUF_S*)(pData - sizeof(MBUF_S));
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_getMbuf
 功能描述  : 申请Mbuf，申请个数增加，并建立Mbuf和pkt Id的对应关系
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : sm_mbuf_getMbuf

 修改历史      :
  1.日    期   : 2010年12月20日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
MBUF_S* sm_pgpAdapt_getMbuf
(
    IN      SM_ULONG ulMID
)
{
    SM_PGPADAPT_PACKET_INFO_S* pstPgpAdaptPktInfo = SM_NULL_PTR;
    MBUF_S*     pstMbuf = SM_NULL_PTR;
    SM_ULONG ulMbufIndex = 0;
    SM_ULONG ulRet = SM_RET_OK;

    /* 在全局变量中查找非使用态的Mbuf, 若查找不到则直接返回 */
    ulRet = sm_pgpAdapt_findFreeMbuf(&pstMbuf);
    if (SM_RET_OK != ulRet)
    {
        return SM_NULL_PTR;
    }

    PS_MEM_SET(pstMbuf, 0, sizeof(MBUF_S));

    /* 建立Mbuf和pkt Id的对应关系 */
    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);
    pstPgpAdaptPktInfo[g_ulPktIdIndex].pstMbuf = pstMbuf;
    pstPgpAdaptPktInfo[g_ulPktIdIndex].ulPacketId = g_ulPktIdIndex + 1; /* 保证与数据包中的packet id一致,从1开始 */
    g_ulPktIdIndex++;

    /* 申请DataBlock*/
    sm_pgpAdapt_getMbufDataBlock(pstMbuf, &(pstMbuf->stDataBlockDescriptor), ulMID);
    pstMbuf->ulDataBlockNumber = 1;

    g_ulCurMbufIndex++;

    return pstMbuf;
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_getMbuf
 功能描述  : 释放Mbuf，释放个数增加，并删除对应pkt Id与Mbuf的对应关系
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : sm_mbuf_releaseMbuf

 修改历史      :
  1.日    期   : 2010年12月20日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_ULONG g_ulCurMbufReleaseIndex = 0;
SM_VOID sm_pgpAdapt_releaseMbuf
(
    IN  MBUF_S*     pstMBuf
)
{
    SM_PGPADAPT_MBUF_INFO_S*        pstPgpAdaptMbufInfo    = SM_NULL_PTR;
    SM_PGPADAPT_PACKET_INFO_S*      pstPgpAdaptPktInfo     = SM_NULL_PTR;
    PMBUF_DATABLOCKDESCRIPTOR_S*    pstDataBlockDescriptor = SM_NULL_PTR;
    SM_ULONG ulindex = 0;
    SM_ULONG ulPacketId = 0;

    pstPgpAdaptMbufInfo = &(g_stPgpAdaptMbufInfo);
    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    for (ulindex = 0; ulindex < SM_PGPADAPT_MBUF_MAX_NUM; ulindex++)
    {
        /* 在全局变量中查找与pstMBuf相同的Mbuf*/
        if (pstMBuf == &(pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulindex].stMbuf))
        {
            /* 设置Mbuf状态为非使用态 */
            pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulindex].ulUsed = SM_FALSE;
            pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulindex].ulDataBlockFreeNum++;

            break;
        }
    }

    for (ulindex = 0; ulindex < g_ulPktIdIndex; ulindex++)
    {
        /* 在全局变量中查找与pstMBuf相同的Mbuf*/
        if (pstMBuf == pstPgpAdaptPktInfo[ulindex].pstMbuf)
        {
            ulPacketId =  pstPgpAdaptPktInfo[ulindex].ulPacketId;

            /* 删除对应pkt Id与Mbuf的对应关系 */
            _pgpAdapt_clearLinkInMbufAndPktid(ulPacketId);

            break;
        }
    }

    pstPgpAdaptMbufInfo->ulMbufFreeNum++;
    g_ulCurMbufReleaseIndex ++;
    return;
}



SM_VOID sm_pgpAdapt_releaseMbufLink
(
    IN  MBUF_S* pstMBuf
)
{
    MBUF_S* pstTmp = pstMBuf;

    do
    {
        sm_pgpAdapt_releaseMbuf(pstTmp);
        /* 需要释放下一个 datablock */
        pstTmp = PMBUF_DBDescToMbuf(pstTmp->stDataBlockDescriptor.pstNextDataBlockDescriptor);
    } while ( SM_NULL_PTR != pstTmp );

    return;
}


SM_ULONG sm_pgpAdapt_copyMbuf()
{
    return SM_RET_OK;
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_getPktIdByMbuf
 功能描述  : 根据Mnuf查找对应的packet ID，以用于报文内容的检查
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : _pfAdapt_sendPktProc，sm_pfAdapt_recvPacketProc

 修改历史      :
  1.日    期   : 2010年12月20日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID sm_pgpAdapt_getPktIdByMbuf
(
    IN      MBUF_S*     pPFData,
    OUT     SM_ULONG*   pulPktId
)
{
    SM_PGPADAPT_PACKET_INFO_S*  pstPgpAdaptPktInfo = SM_NULL_PTR;
    SM_ULONG ulIndex = 0;
    SM_ULONG ulPacketId = 0;

    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    for (ulIndex = 0; ulIndex < g_ulPktIdIndex; ulIndex++)
    {
        /* 在全局变量中查找与pPFData相同的Mbuf*/
        if ( pPFData == pstPgpAdaptPktInfo[ulIndex].pstMbuf)
        {
            ulPacketId =  pstPgpAdaptPktInfo[ulIndex].ulPacketId;

            *pulPktId = ulPacketId;
            return;
        }
    }

    return;
}

/*****************************************************************************
 函 数 名  : sm_pgpAdapt_getMbufByPktId
 功能描述  : 根据packet ID查找对应的Mbuf
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  :

 修改历史      :
  1.日    期   : 2010年12月20日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID sm_pgpAdapt_getMbufByPktId
(
    IN       SM_ULONG     ulPktId,
    OUT      MBUF_S**     ppPFData
)
{
    SM_PGPADAPT_PACKET_INFO_S*  pstPgpAdaptPktInfo = SM_NULL_PTR;
    SM_ULONG ulIndex = 0;

    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    for (ulIndex = 0; ulIndex < g_ulPktIdIndex; ulIndex++)
    {
        /* 在全局变量中查找与pPFData相同的Mbuf*/
        if (ulPktId ==  pstPgpAdaptPktInfo[ulIndex].ulPacketId)
        {
            *ppPFData = pstPgpAdaptPktInfo[ulIndex].pstMbuf;

            return;
        }
    }

    return;
}
SM_VOID sm_pgpAdapt_getMbufByPktId_headen
(
    IN       SM_ULONG     ulPktId,
    OUT      MBUF_S**     ppPFData
)
{
    SM_PGPADAPT_PACKET_INFO_S*  pstPgpAdaptPktInfo = SM_NULL_PTR;
    SM_ULONG ulIndex = 0;

    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    for (ulIndex = 0; ulIndex < g_ulPktIdIndex; ulIndex++)
    {
        /* 在全局变量中查找与pPFData相同的Mbuf*/
        if(SM_NULL_PTR != pstPgpAdaptPktInfo[ulIndex].pstMbuf)
        {
            if(ulPktId ==  pstPgpAdaptPktInfo[ulIndex].pstMbuf->ulResv[1])
            {
                *ppPFData = pstPgpAdaptPktInfo[ulIndex].pstMbuf;
                return;
            }
        }
    }

    return;
}


/*****************************************************************************
 函 数 名  : _pgpAdapt_checkMbufInfo
 功能描述  : 检查Mbuf的信息
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : _pgpAdapt_checkResByUser

 修改历史      :
  1.日    期   : 2010年12月22日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_BOOL _pgpAdapt_checkMbufInfo
(
    OUT  SM_PGPADAPT_MBUF_CHECK_RESULT_S* pstResult
)
{
    SM_PGPADAPT_MBUF_INFO_S*    pstPgpAdaptMbufInfo = SM_NULL_PTR;
    SM_PGPADAPT_PACKET_INFO_S*  pstPgpAdaptPktInfo    = SM_NULL_PTR;
    MBUF_S* pPFData = SM_NULL_PTR;
    SM_ULONG ulIndex = 0;

    pstPgpAdaptMbufInfo = &(g_stPgpAdaptMbufInfo);
    pstPgpAdaptPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);
    pstResult->ulMbufMallocNum =  pstPgpAdaptMbufInfo->ulMbufMallocNum;
    pstResult->ulMbufFreeNum =  pstPgpAdaptMbufInfo->ulMbufFreeNum;
    /* 检查申请的Mbuf数和释放的Mbuf数是否相同 */
    if (pstPgpAdaptMbufInfo->ulMbufMallocNum != pstPgpAdaptMbufInfo->ulMbufFreeNum)
    {
        pstResult->bIfMbufMallocEqualFree = SM_FALSE;
    }

    /* 检查每个Mbuf的状态是否为非使用态 */
    for (ulIndex = 0; ulIndex < SM_PGPADAPT_MBUF_MAX_NUM; ulIndex++)
    {
        if (SM_TRUE == pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulIndex].ulUsed)
        {
            pstResult->ulMbufUsedNum++;
        }

        /* 检查每个Mbuf的datablock申请个数和释放个数是否相同 */
        if (pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulIndex].ulDataBlockMallocNum
            != pstPgpAdaptMbufInfo->astPgpAdaptMbuf[ulIndex].ulDataBlockFreeNum)
        {
            pstResult->ulMbufErrNum++;
        }

    }

    /* 检查每个Mbuf和Packet Id的对应关系是否都清除 */
    for (ulIndex = 0; ulIndex < g_ulPktIdIndex; ulIndex++)
    {
        if ((SM_NULL_PTR != pstPgpAdaptPktInfo[ulIndex].pstMbuf)
            || (0 != pstPgpAdaptPktInfo[ulIndex].ulPacketId))
        {
            pstResult->ulMbufExistRelationNum++;
        }
    }

    if ((SM_TRUE != pstResult->bIfMbufMallocEqualFree)
        || (0 != pstResult->ulMbufUsedNum)
        || (0 != pstResult->ulMbufErrNum)
        || (0 != pstResult->ulMbufExistRelationNum))
    {
        return SM_FALSE;
    }

    return SM_TRUE;
}

/*****************************************************************************
 函 数 名  : _pgpAdapt_reportMbufFailureInfo
 功能描述  : 打印PGP 的resoure检查结果
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : _pgpAdapt_checkResByUser

 修改历史      :
  1.日    期   : 2010年12月22日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID _pgpAdapt_reportMbufFailureInfo
(
    IN      SM_PGPADAPT_MBUF_CHECK_RESULT_S* pstResult,
    IN      SM_VOID* pFile
)
{

    sm_debug_printAll(
                     "\r\n");
    sm_debug_printAll(
                     "\r\n -----------------------------------------------------------------");
    sm_debug_printAll(
                     "\r\n |                         MBUF CHECK ERROR                      |");
    if (SM_FALSE == pstResult->bIfMbufMallocEqualFree)
    {
        sm_debug_printAll(
                     "\r\n |             Mbuf Malloc Num                   = % 4u,         |"
                     "\r\n |             Mbuf Free Num                     = % 4u,         |",
                    pstResult->ulMbufMallocNum,
                    pstResult->ulMbufFreeNum);
    }

    sm_debug_printAll(
                     "\r\n |             Mbuf Used Num                     = % 4u,         |"
                     "\r\n |             Mbuf Err Num                      = % 4u,         |"
                     "\r\n |             Relation Num (Mbuf With PktId)    = % 4u,         |",
                     pstResult->ulMbufUsedNum,
                     pstResult->ulMbufErrNum,
                     pstResult->ulMbufExistRelationNum);

    sm_debug_printAll(
                     "\r\n -----------------------------------------------------------------");

    return;
}

/*****************************************************************************
 函 数 名  : _pgpAdapt_clearResInfo
 功能描述  : 清除使用状态
 输入参数  :
 输出参数  :
 返 回 值  :
 调用函数  :
 被调函数  : _pgpAdapt_checkResByUser

 修改历史      :
  1.日    期   : 2010年12月22日
    作    者   : jinhua
    修改内容   : 新生成函数
*****************************************************************************/
SM_VOID _pgpAdapt_clearMbufInfo
(
    IN      SM_PGPADAPT_MBUF_CHECK_RESULT_S*  pstMbufCheckResult
)
{
    SM_PGPADAPT_MBUF_INFO_S*   pstPgpAdaptMbufInfo = SM_NULL_PTR;
    SM_PGPADAPT_MBUF_S*   pstPgpAdaptMbuf = SM_NULL_PTR;
    SM_PGPADAPT_PACKET_INFO_S* pstPktInfo = SM_NULL_PTR;
    SM_ULONG ulIndex = 0;

    pstPgpAdaptMbufInfo = &(g_stPgpAdaptMbufInfo);
    pstPgpAdaptMbuf = (SM_PGPADAPT_MBUF_S*)&(pstPgpAdaptMbufInfo->astPgpAdaptMbuf);

    /* 清除Mbuf存储信息 */
    g_ulPktIdIndex = 0;
    g_ulCurMbufIndex = 0;
    g_ulCurMbufReleaseIndex = 0;
    pstPgpAdaptMbufInfo->ulMbufMallocNum = 0;
    pstPgpAdaptMbufInfo->ulMbufFreeNum   = 0;

    for (ulIndex = 0; ulIndex < SM_PGPADAPT_MBUF_MAX_NUM; ulIndex++)
    {
        pstPgpAdaptMbuf[ulIndex].ulUsed               = SM_FALSE;
        pstPgpAdaptMbuf[ulIndex].ulDataBlockMallocNum = 0;
        pstPgpAdaptMbuf[ulIndex].ulDataBlockFreeNum   = 0;
    }

    /* 清除Mbuf检查信息 */
    pstMbufCheckResult->bIfMbufMallocEqualFree = SM_TRUE;
    pstMbufCheckResult->ulMbufErrNum = 0;
    pstMbufCheckResult->ulMbufExistRelationNum = 0;
    pstMbufCheckResult->ulMbufUsedNum = 0;

    pstPktInfo = (SM_PGPADAPT_PACKET_INFO_S*)&(g_astPgpAdaptPktInfo);

    for (ulIndex = 0; ulIndex < SM_PGPADAPT_MBUF_MAX_NUM; ulIndex++)
    {
        pstPktInfo[ulIndex].pstMbuf = SM_NULL_PTR;
        pstPktInfo[ulIndex].ulPacketId = 0;
    }


    return;
}



/*lint -restore */


#ifdef __cplusplus
    #if __cplusplus
        }
    #endif
#endif
