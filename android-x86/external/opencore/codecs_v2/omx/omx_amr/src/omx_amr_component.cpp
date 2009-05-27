/* ------------------------------------------------------------------
 * Copyright (C) 2008 PacketVideo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * -------------------------------------------------------------------
 */
#include "pv_omxdefs.h"
#include "oscl_types.h"
#include <string.h>
#include "omx_amr_component.h"

extern OMX_U32 g_ComponentIndex; // this is determined outside the component

#if PROXY_INTERFACE

#include "omx_proxy_interface.h"
extern ProxyApplication_OMX* pProxyTerm[];

#endif

#define OMX_HALFRANGE_THRESHOLD 0x7FFFFFFF
/**** The duration of one output AMR frame (in ms) is fixed and equal to 20ms - needed for timestamp updates ****/
/**** Note that AMR sampling rate is always 8khz, so a frame of 20 ms always corresponds to 160 (16-bit) samples = 320 bytes */
#define OMX_AMR_DEC_FRAME_INTERVAL 20

// This function is called by OMX_GetHandle and it creates an instance of the amr component AO
OMX_ERRORTYPE AmrOmxComponentFactory(OMX_OUT OMX_HANDLETYPE* pHandle, OMX_IN  OMX_PTR pAppData)
{
    OpenmaxAmrAO* pOpenmaxAOType;
    OMX_ERRORTYPE Status;

    // move InitAmrOmxComponentFields content to actual constructor

    pOpenmaxAOType = (OpenmaxAmrAO*) OSCL_NEW(OpenmaxAmrAO, ());

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorInsufficientResources;
    }

    //Call the construct component to initialize OMX types
    Status = pOpenmaxAOType->ConstructComponent(pAppData);

    *pHandle = pOpenmaxAOType->GetOmxHandle();

    return Status;
    ///////////////////////////////////////////////////////////////////////////////////////
}

// This function is called by OMX_FreeHandle when component AO needs to be destroyed
OMX_ERRORTYPE AmrOmxComponentDestructor(OMX_IN OMX_HANDLETYPE pHandle)
{
    // get pointer to component AO
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)pHandle)->pComponentPrivate;

    // clean up decoder, OMX component stuff
    pOpenmaxAOType->DestroyComponent();

    // destroy the AO class
    OSCL_DELETE(pOpenmaxAOType);

    return OMX_ErrorNone;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OMX_ERRORTYPE OpenmaxAmrAO::ConstructComponent(OMX_PTR pAppData)
{
    AmrComponentPortType* pInPort, *pOutPort;
    OMX_U32 ii;

    iNumPorts = 2;
    iOmxComponent.nSize = sizeof(OMX_COMPONENTTYPE);
    iOmxComponent.pComponentPrivate = (OMX_PTR) this;  // pComponentPrivate points to THIS component AO class
    iOmxComponent.pApplicationPrivate = pAppData; // init the App data


#if PROXY_INTERFACE
    iPVCapabilityFlags.iIsOMXComponentMultiThreaded = OMX_TRUE;

    iOmxComponent.SendCommand = WrapperSendCommand;
    iOmxComponent.GetParameter = WrapperGetParameter;
    iOmxComponent.SetParameter = WrapperSetParameter;
    iOmxComponent.GetConfig = WrapperGetConfig;
    iOmxComponent.SetConfig = WrapperSetConfig;
    iOmxComponent.GetExtensionIndex = WrapperGetExtensionIndex;
    iOmxComponent.GetState = WrapperGetState;
    iOmxComponent.UseBuffer = WrapperUseBuffer;
    iOmxComponent.AllocateBuffer = WrapperAllocateBuffer;
    iOmxComponent.FreeBuffer = WrapperFreeBuffer;
    iOmxComponent.EmptyThisBuffer = WrapperEmptyThisBuffer;
    iOmxComponent.FillThisBuffer = WrapperFillThisBuffer;

#else
    iPVCapabilityFlags.iIsOMXComponentMultiThreaded = OMX_FALSE;

    iOmxComponent.SendCommand = OpenmaxAmrAO::BaseComponentSendCommand;
    iOmxComponent.GetParameter = OpenmaxAmrAO::BaseComponentGetParameter;
    iOmxComponent.SetParameter = OpenmaxAmrAO::BaseComponentSetParameter;
    iOmxComponent.GetConfig = OpenmaxAmrAO::BaseComponentGetConfig;
    iOmxComponent.SetConfig = OpenmaxAmrAO::BaseComponentSetConfig;
    iOmxComponent.GetExtensionIndex = OpenmaxAmrAO::BaseComponentGetExtensionIndex;
    iOmxComponent.GetState = OpenmaxAmrAO::BaseComponentGetState;
    iOmxComponent.UseBuffer = OpenmaxAmrAO::BaseComponentUseBuffer;
    iOmxComponent.AllocateBuffer = OpenmaxAmrAO::BaseComponentAllocateBuffer;
    iOmxComponent.FreeBuffer = OpenmaxAmrAO::BaseComponentFreeBuffer;
    iOmxComponent.EmptyThisBuffer = OpenmaxAmrAO::BaseComponentEmptyThisBuffer;
    iOmxComponent.FillThisBuffer = OpenmaxAmrAO::BaseComponentFillThisBuffer;
#endif

    iOmxComponent.SetCallbacks = OpenmaxAmrAO::BaseComponentSetCallbacks;
    iOmxComponent.nVersion.s.nVersionMajor = SPECVERSIONMAJOR;
    iOmxComponent.nVersion.s.nVersionMinor = SPECVERSIONMINOR;
    iOmxComponent.nVersion.s.nRevision = SPECREVISION;
    iOmxComponent.nVersion.s.nStep = SPECSTEP;

    // PV capability
    iPVCapabilityFlags.iOMXComponentSupportsExternalInputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsExternalOutputBufferAlloc = OMX_TRUE;
    iPVCapabilityFlags.iOMXComponentSupportsMovableInputBuffers = OMX_TRUE;

    if (ipAppPriv)
    {
        oscl_free(ipAppPriv);
        ipAppPriv = NULL;
    }

    ipAppPriv = (AmrPrivateType*) oscl_malloc(sizeof(AmrPrivateType));
    if (NULL == ipAppPriv)
    {
        return OMX_ErrorInsufficientResources;
    }

    if (iNumPorts)
    {
        if (ipPorts)
        {
            oscl_free(ipPorts);
            ipPorts = NULL;
        }

        ipPorts = (AmrComponentPortType**) oscl_calloc(iNumPorts, sizeof(AmrComponentPortType*));
        if (!ipPorts)
        {
            return OMX_ErrorInsufficientResources;
        }

        for (ii = 0; ii < iNumPorts; ii++)
        {
            ipPorts[ii] = (AmrComponentPortType*) oscl_calloc(1, sizeof(AmrComponentPortType));
            if (!ipPorts[ii])
            {
                return OMX_ErrorInsufficientResources;
            }

            ipPorts[ii]->TransientState = OMX_StateMax;
            SetHeader(&ipPorts[ii]->PortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            ipPorts[ii]->PortParam.nPortIndex = ii;

            /** Allocate and initialize buffer Queue */
            ipPorts[ii]->pBufferQueue = (QueueType*) oscl_malloc(sizeof(QueueType));

            if (NULL == ipPorts[ii]->pBufferQueue)
            {
                return OMX_ErrorInsufficientResources;
            }

            QueueInit(ipPorts[ii]->pBufferQueue);

            ipPorts[ii]->LoadedToIdleFlag = OMX_FALSE;
            ipPorts[ii]->IdleToLoadedFlag = OMX_FALSE;

        }

        AmrComponentSetPortFlushFlag(iNumPorts, -1, OMX_FALSE);
        AmrComponentSetNumBufferFlush(iNumPorts, -1, OMX_FALSE);
    }

    /** Domain specific section for the ports. */
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.eDomain = OMX_PortDomainAudio;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.audio.cMIMEType = "audio/mpeg";
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.audio.pNativeRender = 0;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.format.audio.eEncoding = OMX_AUDIO_CodingAMR;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.eDir = OMX_DirInput;
    //Set to a default value, will change later during setparameter call
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferCountActual = NUMBER_INPUT_BUFFER_AMR;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferCountMin = 1;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize = INPUT_BUFFER_SIZE_AMR;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.bEnabled = OMX_TRUE;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.bPopulated = OMX_FALSE;


    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.eDomain = OMX_PortDomainAudio;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.audio.cMIMEType = "raw";
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.audio.pNativeRender = 0;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.audio.bFlagErrorConcealment = OMX_FALSE;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.format.audio.eEncoding = OMX_AUDIO_CodingPCM;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.eDir = OMX_DirOutput;
    //Set to a default value, will change later during setparameter call
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferCountActual = NUMBER_OUTPUT_BUFFER_AMR;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferCountMin = 1;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.nBufferSize = OUTPUT_BUFFER_SIZE_AMR * 6;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.bEnabled = OMX_TRUE;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->PortParam.bPopulated = OMX_FALSE;

    //Default values for AMR audio param port
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.nChannels = 1;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.nBitRate = 0;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.eAMRBandMode = OMX_AUDIO_AMRBandModeNB0;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.eAMRDTXMode = OMX_AUDIO_AMRDTXModeOnVAD1;
    ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.eAMRFrameFormat = OMX_AUDIO_AMRFrameFormatConformance;

    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.nChannels = 1;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.eNumData = OMX_NumericalDataSigned;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.bInterleaved = OMX_TRUE;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.nBitPerSample = 16;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.nSamplingRate = 8000;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.ePCMMode = OMX_AUDIO_PCMModeLinear;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.eChannelMapping[0] = OMX_AUDIO_ChannelLF;
    ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.eChannelMapping[1] = OMX_AUDIO_ChannelRF;

    iPortTypesParam.nPorts = 2;
    iPortTypesParam.nStartPortNumber = 0;

    pInPort = (AmrComponentPortType*) ipPorts[OMX_PORT_INPUTPORT_INDEX];
    pOutPort = (AmrComponentPortType*) ipPorts[OMX_PORT_OUTPUTPORT_INDEX];

    SetHeader(&pInPort->AudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pInPort->AudioParam.nPortIndex = 0;
    pInPort->AudioParam.nIndex = 0;
    pInPort->AudioParam.eEncoding = OMX_AUDIO_CodingAMR;

    SetHeader(&pOutPort->AudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
    pOutPort->AudioParam.nPortIndex = 1;
    pOutPort->AudioParam.nIndex = 0;
    pOutPort->AudioParam.eEncoding = OMX_AUDIO_CodingPCM;

    iCodecReady = OMX_FALSE;
    ipCallbacks = NULL;
    iCallbackData = NULL;
    iState = OMX_StateLoaded;
    ipTempInputBuffer = NULL;
    iTempInputBufferFilledLength = 0;
    iNumInputBuffer = 0;
    iPartialFrameAssembly = OMX_FALSE;
    iEndofStream = OMX_FALSE;
    iIsInputBufferEnded = OMX_TRUE;
    iNewOutBufRequired = OMX_TRUE;
    iOutputFrameLength = OUTPUT_BUFFER_SIZE_AMR;
    iRepositionFlag = OMX_FALSE;
    iInputBufferRemainingBytes = 0;

    /* Initialize the asynchronous command Queue */
    if (ipCoreDescriptor)
    {
        oscl_free(ipCoreDescriptor);
        ipCoreDescriptor = NULL;
    }

    ipCoreDescriptor = (CoreDescriptorType*) oscl_malloc(sizeof(CoreDescriptorType));
    if (NULL == ipCoreDescriptor)
    {
        return OMX_ErrorInsufficientResources;
    }

    ipCoreDescriptor->pMessageQueue = NULL;
    ipCoreDescriptor->pMessageQueue = (QueueType*) oscl_malloc(sizeof(QueueType));
    if (NULL == ipCoreDescriptor->pMessageQueue)
    {
        return OMX_ErrorInsufficientResources;
    }

    QueueInit(ipCoreDescriptor->pMessageQueue);

    /** Default parameters setting */
    iIsInit = OMX_FALSE;
    iGroupPriority = 0;
    iGroupID = 0;
    ipMark = NULL;

    SetHeader(&iPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));

    iOutBufferCount = 0;
    iStateTransitionFlag = OMX_FALSE;
    iFlushPortFlag = OMX_FALSE;
    iEndOfFrameFlag = OMX_FALSE;
    iFirstFragment = OMX_FALSE;

    //Will be used in case of partial frame assembly
    ipInputCurrBuffer = NULL;
    ipAppPriv->AmrHandle = &iOmxComponent;

    if (ipAmrDec)
    {
        OSCL_DELETE(ipAmrDec);
        ipAmrDec = NULL;
    }

    ipAmrDec = OSCL_NEW(OmxAmrDecoder, ());
    if (NULL == ipAmrDec)
    {
        return OMX_ErrorInsufficientResources;
    }

    //Commented memset here as some default values are set in the constructor
    //oscl_memset(ipAmrDec, 0, sizeof (OmxAmrDecoder));

#if PROXY_INTERFACE

    pProxyTerm[g_ComponentIndex]->ComponentSendCommand = BaseComponentSendCommand;
    pProxyTerm[g_ComponentIndex]->ComponentGetParameter = BaseComponentGetParameter;
    pProxyTerm[g_ComponentIndex]->ComponentSetParameter = BaseComponentSetParameter;
    pProxyTerm[g_ComponentIndex]->ComponentGetConfig = BaseComponentGetConfig;
    pProxyTerm[g_ComponentIndex]->ComponentSetConfig = BaseComponentSetConfig;
    pProxyTerm[g_ComponentIndex]->ComponentGetExtensionIndex = BaseComponentGetExtensionIndex;
    pProxyTerm[g_ComponentIndex]->ComponentGetState = BaseComponentGetState;
    pProxyTerm[g_ComponentIndex]->ComponentUseBuffer = BaseComponentUseBuffer;
    pProxyTerm[g_ComponentIndex]->ComponentAllocateBuffer = BaseComponentAllocateBuffer;
    pProxyTerm[g_ComponentIndex]->ComponentFreeBuffer = BaseComponentFreeBuffer;
    pProxyTerm[g_ComponentIndex]->ComponentEmptyThisBuffer = BaseComponentEmptyThisBuffer;
    pProxyTerm[g_ComponentIndex]->ComponentFillThisBuffer = BaseComponentFillThisBuffer;

#endif
    return OMX_ErrorNone;
}


/*********************
 *
 * Component verfication routines
 *
 **********************/

void OpenmaxAmrAO::SetHeader(OMX_PTR aHeader, OMX_U32 aSize)
{
    OMX_VERSIONTYPE* pVersion = (OMX_VERSIONTYPE*)((OMX_STRING) aHeader + sizeof(OMX_U32));
    *((OMX_U32*) aHeader) = aSize;

    pVersion->s.nVersionMajor = SPECVERSIONMAJOR;
    pVersion->s.nVersionMinor = SPECVERSIONMINOR;
    pVersion->s.nRevision = SPECREVISION;
    pVersion->s.nStep = SPECSTEP;
}


OMX_ERRORTYPE OpenmaxAmrAO::CheckHeader(OMX_PTR aHeader, OMX_U32 aSize)
{
    OMX_VERSIONTYPE* pVersion = (OMX_VERSIONTYPE*)((OMX_STRING) aHeader + sizeof(OMX_U32));

    if (NULL == aHeader)
    {
        return OMX_ErrorBadParameter;
    }

    if (*((OMX_U32*) aHeader) != aSize)
    {
        return OMX_ErrorBadParameter;
    }

    if (pVersion->s.nVersionMajor != SPECVERSIONMAJOR ||
            pVersion->s.nVersionMinor != SPECVERSIONMINOR ||
            pVersion->s.nRevision != SPECREVISION ||
            pVersion->s.nStep != SPECSTEP)
    {
        return OMX_ErrorVersionMismatch;
    }

    return OMX_ErrorNone;
}


/**
 * This function verify component state and structure header
 */
OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentParameterSanityCheck(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_PTR pStructure,
    OMX_IN  size_t size)
{

    if (iState != OMX_StateLoaded &&
            iState != OMX_StateWaitForResources)
    {
        return OMX_ErrorIncorrectStateOperation;
    }

    if (nPortIndex >= iNumPorts)
    {
        return OMX_ErrorBadPortIndex;
    }

    return CheckHeader(pStructure, size);
}

/**
 * Set/Reset Port Flush Flag
 */
void OpenmaxAmrAO::AmrComponentSetPortFlushFlag(OMX_S32 NumPorts, OMX_S32 index, OMX_BOOL value)
{
    OMX_S32 ii;

    if (-1 == index)
    {
        for (ii = 0; ii < NumPorts; ii++)
        {
            ipPorts[ii]->IsPortFlushed = value;
        }
    }
    else
    {
        ipPorts[index]->IsPortFlushed = value;
    }

}

/**
 * Set Number of Buffer Flushed with the value Specified
 */
void OpenmaxAmrAO::AmrComponentSetNumBufferFlush(OMX_S32 NumPorts, OMX_S32 index, OMX_S32 value)
{
    OMX_S32 ii;

    if (-1 == index)
    { // For all ComponentPort
        for (ii = 0; ii < NumPorts; ii++)
        {
            ipPorts[ii]->NumBufferFlushed = value;
        }
    }
    else
    {
        ipPorts[index]->NumBufferFlushed = value;
    }
}


/** This function assembles multiple input buffers into
	* one frame with the marker flag OMX_BUFFERFLAG_ENDOFFRAME set
	*/

OMX_BOOL OpenmaxAmrAO::AmrComponentAssemblePartialFrames(OMX_BUFFERHEADERTYPE* aInputBuffer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentAssemblePartialFrames IN"));

    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;

    AmrComponentPortType* pInPort = ipPorts[OMX_PORT_INPUTPORT_INDEX];


    ipAmrInputBuffer = aInputBuffer;

    if (!iPartialFrameAssembly)
    {
        if (iNumInputBuffer > 0)
        {
            if (ipAmrInputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME)
            {
                iInputCurrLength = ipAmrInputBuffer->nFilledLen;
                ipFrameDecodeBuffer = ipAmrInputBuffer->pBuffer + ipAmrInputBuffer->nOffset;
                //capture the timestamp to be send to the corresponding output buffer
                iFrameTimestamp = ipAmrInputBuffer->nTimeStamp;
            }
            else
            {
                iInputCurrLength = 0;
                iPartialFrameAssembly = OMX_TRUE;
                iFirstFragment = OMX_TRUE;
                iFrameTimestamp = ipAmrInputBuffer->nTimeStamp;
                ipFrameDecodeBuffer = ipInputCurrBuffer;
            }

        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentAssemblePartialFrames ERROR"));
            return OMX_FALSE;
        }

    }

    //Assembling of partial frame will be done based on OMX_BUFFERFLAG_ENDOFFRAME flag marked
    if (iPartialFrameAssembly)
    {
        while (iNumInputBuffer > 0)
        {
            if (OMX_FALSE == iFirstFragment)
            {
                /* If the timestamp of curr fragment doesn't match with previous,
                 * discard the previous fragments & start reconstructing from new
                 */
                if (iFrameTimestamp != ipAmrInputBuffer->nTimeStamp)
                {
                    iInputCurrLength = 0;
                    iPartialFrameAssembly = OMX_TRUE;
                    iFirstFragment = OMX_TRUE;
                    iFrameTimestamp = ipAmrInputBuffer->nTimeStamp;
                    ipFrameDecodeBuffer = ipInputCurrBuffer;
                }
            }

            if ((ipAmrInputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME) != 0)
            {
                break;
            }

            iInputCurrLength += ipAmrInputBuffer->nFilledLen;
            oscl_memcpy(ipFrameDecodeBuffer, (ipAmrInputBuffer->pBuffer + ipAmrInputBuffer->nOffset), ipAmrInputBuffer->nFilledLen); // copy buffer data
            ipFrameDecodeBuffer += ipAmrInputBuffer->nFilledLen; // move the ptr

            ipAmrInputBuffer->nFilledLen = 0;

            AmrComponentReturnInputBuffer(ipAmrInputBuffer, pInPort);

            iFirstFragment = OMX_FALSE;

            if (iNumInputBuffer > 0)
            {
                ipAmrInputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pInputQueue);
                if (ipAmrInputBuffer->nFlags & OMX_BUFFERFLAG_EOS)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentAssemblePartialFrames EndOfStream arrived"));
                    iEndofStream = OMX_TRUE;
                }
            }
        }

        // if we broke out of the while loop because of lack of buffers, then return and wait for more input buffers
        if (0 == iNumInputBuffer)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentAssemblePartialFrames OUT"));
            return OMX_FALSE;
        }
        else
        {
            // we have found the buffer that is the last piece of the frame.
            // Copy the buffer, but do not release it yet (this will be done after decoding for consistency)

            iInputCurrLength += ipAmrInputBuffer->nFilledLen;
            oscl_memcpy(ipFrameDecodeBuffer, (ipAmrInputBuffer->pBuffer + ipAmrInputBuffer->nOffset), ipAmrInputBuffer->nFilledLen); // copy buffer data
            ipFrameDecodeBuffer += ipAmrInputBuffer->nFilledLen; // move the ptr

            ipFrameDecodeBuffer = ipInputCurrBuffer; // reset the pointer back to beginning of assembly buffer
            iPartialFrameAssembly = OMX_FALSE; // we have finished with assembling the frame, so this is not needed any more
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentAssemblePartialFrames OUT"));
    return OMX_TRUE;
}


/** This is the central function for buffers processing and decoding.
	* It is called through the Run() of active object when the component is in executing state
	* and is signalled each time a new buffer is available on the given ports
	* This function will process the input buffers & return output buffers
	*/

void OpenmaxAmrAO::AmrComponentBufferMgmtFunction()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentBufferMgmtFunction IN"));

    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;

    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    AmrComponentPortType* pInPort = (AmrComponentPortType*) ipPorts[OMX_PORT_INPUTPORT_INDEX];

    OMX_BOOL PartialFrameReturn;

    /* Don't dequeue any further buffer after endofstream buffer has been dequeued
     * till we send the callback and reset the flag back to false
     */
    if (OMX_FALSE == iEndofStream)
    {
        //More than one frame can't be dequeued in case of outbut blocked
        if ((OMX_TRUE == iIsInputBufferEnded) && (GetQueueNumElem(pInputQueue) > 0))
        {
            ipAmrInputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pInputQueue);

            if (ipAmrInputBuffer->nFlags & OMX_BUFFERFLAG_EOS)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentBufferMgmtFunction EndOfStream arrived"));
                iEndofStream = OMX_TRUE;
            }

            if (ipAmrInputBuffer->nFilledLen != 0)
            {
                if (0 == iFrameCount)
                {
                    //Set the marker flag (iEndOfFrameFlag) if first frame has the EndOfFrame flag marked.
                    if (ipAmrInputBuffer->nFlags & OMX_BUFFERFLAG_ENDOFFRAME)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentBufferMgmtFunction EndOfFrame flag present"));
                        iEndOfFrameFlag = OMX_TRUE;
                    }
                }

                /* This condition will be true if OMX_BUFFERFLAG_ENDOFFRAME flag is
                 *  not marked in all the input buffers
                 */
                if (!iEndOfFrameFlag)
                {
                    AmrBufferMgmtWithoutMarker(ipAmrInputBuffer);

                }
                //If OMX_BUFFERFLAG_ENDOFFRAME flag is marked, come here
                else
                {
                    PartialFrameReturn = AmrComponentAssemblePartialFrames(ipAmrInputBuffer);
                    if (OMX_FALSE == PartialFrameReturn)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentBufferMgmtFunction OUT"));
                        return;
                    }
                    iIsInputBufferEnded = OMX_FALSE;

                    ipTargetComponent = (OMX_COMPONENTTYPE*) ipAmrInputBuffer->hMarkTargetComponent;

                    iTargetMarkData = ipAmrInputBuffer->pMarkData;
                    if (ipTargetComponent == (OMX_COMPONENTTYPE*) pHandle)
                    {
                        (*(ipCallbacks->EventHandler))
                        (pHandle,
                         iCallbackData,
                         OMX_EventMark,
                         1,
                         0,
                         ipAmrInputBuffer->pMarkData);
                    }

                    //Do not check for silence insertion if the clip is repositioned
                    if (OMX_FALSE == iRepositionFlag)
                    {
                        CheckForSilenceInsertion();
                    }


                    /* Set the current timestamp equal to input buffer timestamp in case of
                     * a) All input frames
                     * b) First frame after repositioning */
                    if (OMX_FALSE == iSilenceInsertionInProgress || OMX_TRUE == iRepositionFlag)
                    {
                        iCurrentTimestamp = iFrameTimestamp;

                        //Reset the flag back to false, once timestamp has been updated from input frame
                        if (OMX_TRUE == iRepositionFlag)
                        {
                            iRepositionFlag = OMX_FALSE;
                        }
                    }
                }

            }	//end braces for if (ipAmrInputBuffer->nFilledLen != 0)
            else
            {
                AmrComponentReturnInputBuffer(ipAmrInputBuffer, pInPort);
            }

        }	//end braces for if ((OMX_TRUE == iIsInputBufferEnded) && (GetQueueNumElem(pInputQueue) > 0))
    }	//if (OMX_FALSE == iEndofStream)

    if (!iEndOfFrameFlag)
    {
        AmrDecodeWithoutMarker();
    }
    else
    {
        AmrDecodeWithMarker(ipAmrInputBuffer);
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentBufferMgmtFunction OUT"));
    return;
}


void OpenmaxAmrAO::AmrBufferMgmtWithoutMarker(OMX_BUFFERHEADERTYPE* pAmrInputBuffer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrBufferMgmtWithoutMarker IN"));
    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;
    AmrComponentPortType*	pInPort = (AmrComponentPortType*) ipPorts[OMX_PORT_INPUTPORT_INDEX];
    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;

    OMX_U32 TempInputBufferSize = (2 * sizeof(uint8) * (ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize));

    /* Assembling of partial frame will be done based on max input buf size
     * If Flushport flag is true, that means its not a partial frame
     * but an unconsumed frame, process it independently
     * Same is true for endofstream condition, process the buffer independently
     */
    if ((pAmrInputBuffer->nFilledLen < pAmrInputBuffer->nAllocLen)
            && (iEndofStream != OMX_TRUE) && (OMX_FALSE == iFlushPortFlag))
    {
        if (!iPartialFrameAssembly)
        {
            iInputCurrLength = 0;
            ipFrameDecodeBuffer = ipInputCurrBuffer;
        }

        while (iNumInputBuffer > 0)
        {
            oscl_memcpy(ipFrameDecodeBuffer, (pAmrInputBuffer->pBuffer + pAmrInputBuffer->nOffset), pAmrInputBuffer->nFilledLen);
            ipFrameDecodeBuffer += pAmrInputBuffer->nFilledLen; // move the ptr

            iFrameTimestamp = pAmrInputBuffer->nTimeStamp;

            if (((iInputCurrLength += pAmrInputBuffer->nFilledLen) >= pAmrInputBuffer->nAllocLen)
                    || OMX_TRUE == iEndofStream)
            {
                break;
            }

            //Set the filled len to zero to indiacte buffer is fully consumed
            pAmrInputBuffer->nFilledLen = 0;
            AmrComponentReturnInputBuffer(pAmrInputBuffer, pInPort);

            if (iNumInputBuffer > 0)
            {
                pAmrInputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pInputQueue);
                if (pAmrInputBuffer->nFlags & OMX_BUFFERFLAG_EOS)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrBufferMgmtWithoutMarker EndOfStream arrived"));
                    iEndofStream = OMX_TRUE;
                }
            }
        }

        if (((iInputCurrLength < pAmrInputBuffer->nAllocLen)) && OMX_TRUE != iEndofStream)
        {
            iPartialFrameAssembly = OMX_TRUE;
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrBufferMgmtWithoutMarker OUT"));
            return;
        }
        else
        {
            ipFrameDecodeBuffer = ipInputCurrBuffer;
            /* Used in timestamp calculation
             * Since we coyp one buffer in advance, so let the first buffer finish up
             * before applying 2nd buffers timestamp into the output timestamp */
            if (iInputBufferRemainingBytes <= 0)
            {
                if (0 == iFrameCount)
                {
                    iPreviousFrameLength = pAmrInputBuffer->nFilledLen;
                    iCurrentTimestamp = iFrameTimestamp;
                }

                iInputBufferRemainingBytes += iPreviousFrameLength;
            }
            iPreviousFrameLength = pAmrInputBuffer->nFilledLen;

            iPartialFrameAssembly = OMX_FALSE;
        }
    }
    else
    {
        if (iNumInputBuffer > 0)
        {
            iInputCurrLength = pAmrInputBuffer->nFilledLen;
            ipFrameDecodeBuffer = pAmrInputBuffer->pBuffer + pAmrInputBuffer->nOffset;
            iFrameTimestamp = pAmrInputBuffer->nTimeStamp;

            //Used in timestamp calculation
            if (iInputBufferRemainingBytes <= 0)
            {
                if (0 == iFrameCount)
                {
                    iPreviousFrameLength = pAmrInputBuffer->nFilledLen;
                    iCurrentTimestamp = iFrameTimestamp;
                }

                iInputBufferRemainingBytes += iPreviousFrameLength;
            }
            iPreviousFrameLength = pAmrInputBuffer->nFilledLen;
        }
        else
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrBufferMgmtWithoutMarker OUT"));
            return; // nothing to decode
        }
    }

    if (iTempInputBufferFilledLength < (TempInputBufferSize >> 1))
    {
        oscl_memmove(ipTempInputBuffer, &ipTempInputBuffer[iTempConsumedLength], iTempInputBufferFilledLength);
        iIsInputBufferEnded = OMX_TRUE;
        iTempConsumedLength = 0;
    }

    if ((iTempInputBufferFilledLength + iTempConsumedLength
            + iInputCurrLength) <= TempInputBufferSize)
    {
        oscl_memcpy(&ipTempInputBuffer[iTempInputBufferFilledLength + iTempConsumedLength], ipFrameDecodeBuffer, iInputCurrLength);
        iTempInputBufferFilledLength += iInputCurrLength;

        if (iTempInputBufferFilledLength + (TempInputBufferSize >> 1) <= TempInputBufferSize)
        {
            iNewInBufferRequired = OMX_TRUE;
        }
        else
        {
            iNewInBufferRequired = OMX_FALSE;
        }

        ipTargetComponent = (OMX_COMPONENTTYPE*) pAmrInputBuffer->hMarkTargetComponent;

        iTargetMarkData = pAmrInputBuffer->pMarkData;
        if (ipTargetComponent == (OMX_COMPONENTTYPE*) pHandle)
        {
            (*(ipCallbacks->EventHandler))
            (pHandle,
             iCallbackData,
             OMX_EventMark,
             1,
             0,
             pAmrInputBuffer->pMarkData);
        }
        pAmrInputBuffer->nFilledLen = 0;
        AmrComponentReturnInputBuffer(pAmrInputBuffer, pInPort);

    }

    if (iTempInputBufferFilledLength >= (TempInputBufferSize >> 1))
    {
        iIsInputBufferEnded = OMX_FALSE;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrBufferMgmtWithoutMarker OUT"));
    return;

}

void OpenmaxAmrAO::AmrDecodeWithoutMarker()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker IN"));

    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    QueueType* pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;
    AmrComponentPortType*	pOutPort = ipPorts[OMX_PORT_OUTPUTPORT_INDEX];
    OMX_COMPONENTTYPE  *pHandle = &iOmxComponent;

    OMX_U8*					pOutBuffer;
    OMX_U32					OutputLength;
    OMX_U8*					pTempInBuffer;
    OMX_U32					TempInLength;
    OMX_BOOL				ResizeNeeded = OMX_FALSE;
    OMX_BOOL				DecodeReturn = OMX_FALSE;

    OMX_U32 TempInputBufferSize = (2 * sizeof(uint8) * (ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize));


    if ((!iIsInputBufferEnded) || iEndofStream)
    {
        //Check whether prev output bufer has been released or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not
            if (0 == (GetQueueNumElem(pOutputQueue)))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker OUT output buffer unavailable"));
                //Store the mark data for output buffer, as it will be overwritten next time
                if (NULL != ipTargetComponent)
                {
                    ipTempTargetComponent = ipTargetComponent;
                    iTempTargetMarkData = iTargetMarkData;
                    iMarkPropagate = OMX_TRUE;
                }
                return;
            }

            ipAmrOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
            ipAmrOutputBuffer->nFilledLen = 0;
            iNewOutBufRequired = OMX_FALSE;

            //Set the current timestamp to the output buffer timestamp
            ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
        }

        /* Code for the marking buffer. Takes care of the OMX_CommandMarkBuffer
         * command and hMarkTargetComponent as given by the specifications
         */
        if (ipMark != NULL)
        {
            ipAmrOutputBuffer->hMarkTargetComponent = ipMark->hMarkTargetComponent;
            ipAmrOutputBuffer->pMarkData = ipMark->pMarkData;
            ipMark = NULL;
        }

        if ((OMX_TRUE == iMarkPropagate) && (ipTempTargetComponent != ipTargetComponent))
        {
            ipAmrOutputBuffer->hMarkTargetComponent = ipTempTargetComponent;
            ipAmrOutputBuffer->pMarkData = iTempTargetMarkData;
            ipTempTargetComponent = NULL;
            iMarkPropagate = OMX_FALSE;
        }
        else if (ipTargetComponent != NULL)
        {
            ipAmrOutputBuffer->hMarkTargetComponent = ipTargetComponent;
            ipAmrOutputBuffer->pMarkData = iTargetMarkData;
            ipTargetComponent = NULL;
            iMarkPropagate = OMX_FALSE;

        }
        //Mark buffer code ends here

        if (iTempInputBufferFilledLength > 0)
        {
            pOutBuffer = &ipAmrOutputBuffer->pBuffer[ipAmrOutputBuffer->nFilledLen];
            OutputLength = 0;

            pTempInBuffer = ipTempInputBuffer + iTempConsumedLength;
            TempInLength = iTempInputBufferFilledLength;

            //Output buffer is passed as a short pointer
            DecodeReturn = ipAmrDec->AmrDecodeFrame((OMX_S16*) pOutBuffer,
                                                    (OMX_U32*) & OutputLength,
                                                    &(pTempInBuffer),
                                                    &TempInLength,
                                                    &iFrameCount,
                                                    &ResizeNeeded);

            ipAmrOutputBuffer->nFilledLen += OutputLength;

            //offset not required in our case, set it to zero
            ipAmrOutputBuffer->nOffset = 0;

            //If decoder returned error, report it to the client via a callback
            if (OMX_FALSE == DecodeReturn && OMX_FALSE == iEndofStream)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker ErrorStreamCorrupt callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventError,
                 OMX_ErrorStreamCorrupt,
                 0,
                 NULL);
            }

            if (ResizeNeeded == OMX_TRUE)
            {
                if (0 != OutputLength)
                {
                    iOutputFrameLength = OutputLength;
                    //For the first buffer, assign the input buffer timestamp into it
                    ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
                }
                // send port settings changed event
                OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*) ipAppPriv->AmrHandle;

                iResizePending = OMX_TRUE;

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventPortSettingsChanged, //The command was completed
                 OMX_PORT_OUTPUTPORT_INDEX,
                 0,
                 NULL);

            }

            if (OutputLength > 0)
            {
                iCurrentTimestamp += OMX_AMR_DEC_FRAME_INTERVAL;
            }

            iTempConsumedLength += (iTempInputBufferFilledLength - TempInLength);
            iInputBufferRemainingBytes -= (iTempInputBufferFilledLength - TempInLength);

            iTempInputBufferFilledLength = TempInLength;

            if (iInputBufferRemainingBytes <= 0)
            {
                iCurrentTimestamp = iFrameTimestamp;
            }

            //Do not decode if big buffer is less than half the size
            if (TempInLength < (TempInputBufferSize >> 1))
            {
                iIsInputBufferEnded = OMX_TRUE;
                iNewInBufferRequired = OMX_TRUE;
            }
        }


        /* If EOS flag has come from the client & there are no more
         * input buffers to decode, send the callback to the client
         */
        if (OMX_TRUE == iEndofStream)
        {
            if ((0 == iTempInputBufferFilledLength) || (OMX_FALSE == DecodeReturn))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker EOS callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventBufferFlag,
                 1,
                 OMX_BUFFERFLAG_EOS,
                 NULL);

                iNewInBufferRequired = OMX_TRUE;
                iEndofStream = OMX_FALSE;

                ipAmrOutputBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker OUT"));

                return;
            }
        }

        //Send the output buffer back after decode
        if (((ipAmrOutputBuffer->nAllocLen - ipAmrOutputBuffer->nFilledLen) < (iOutputFrameLength))
                || (OMX_TRUE == ResizeNeeded))
        {
            AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);
        }

        /* If there is some more processing left with current buffers, re-schedule the AO
         * Do not go for more than one round of processing at a time.
         * This may block the AO longer than required.
         */
        if ((TempInLength != 0 || GetQueueNumElem(pInputQueue) > 0)	&& (GetQueueNumElem(pOutputQueue) > 0) && (ResizeNeeded == OMX_FALSE))
        {
            RunIfNotReady();
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker OUT"));
    return;
}


void OpenmaxAmrAO::AmrDecodeWithMarker(OMX_BUFFERHEADERTYPE* pAmrInputBuffer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithMarker IN"));

    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    QueueType* pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;

    AmrComponentPortType*	pInPort = ipPorts[OMX_PORT_INPUTPORT_INDEX];
    AmrComponentPortType*	pOutPort = ipPorts[OMX_PORT_OUTPUTPORT_INDEX];

    OMX_U8*					pOutBuffer;
    OMX_U32					OutputLength;
    OMX_BOOL				DecodeReturn = OMX_FALSE;
    OMX_COMPONENTTYPE*		pHandle = &iOmxComponent;
    OMX_BOOL				ResizeNeeded = OMX_FALSE;

    if ((!iIsInputBufferEnded) || (iEndofStream))
    {
        if (OMX_TRUE == iSilenceInsertionInProgress)
        {
            DoSilenceInsertion();
            //If the flag is still true, come back to this routine again
            if (OMX_TRUE == iSilenceInsertionInProgress)
            {
                return;
            }
        }

        //Check whether prev output bufer has been released or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not
            if (0 == (GetQueueNumElem(pOutputQueue)))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithoutMarker OUT output buffer unavailable"));
                return;
            }

            ipAmrOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
            ipAmrOutputBuffer->nFilledLen = 0;
            iNewOutBufRequired = OMX_FALSE;

            //Set the current timestamp to the output buffer timestamp
            ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
        }

        /* Code for the marking buffer. Takes care of the OMX_CommandMarkBuffer
         * command and hMarkTargetComponent as given by the specifications
         */
        if (ipMark != NULL)
        {
            ipAmrOutputBuffer->hMarkTargetComponent = ipMark->hMarkTargetComponent;
            ipAmrOutputBuffer->pMarkData = ipMark->pMarkData;
            ipMark = NULL;
        }

        if (ipTargetComponent != NULL)
        {
            ipAmrOutputBuffer->hMarkTargetComponent = ipTargetComponent;
            ipAmrOutputBuffer->pMarkData = iTargetMarkData;
            ipTargetComponent = NULL;

        }
        //Mark buffer code ends here

        if (iInputCurrLength > 0)
        {
            pOutBuffer = &ipAmrOutputBuffer->pBuffer[ipAmrOutputBuffer->nFilledLen];
            OutputLength = 0;

            //Output buffer is passed as a short pointer
            DecodeReturn = ipAmrDec->AmrDecodeFrame((OMX_S16*)pOutBuffer,
                                                    (OMX_U32*) & OutputLength,
                                                    &(ipFrameDecodeBuffer),
                                                    &(iInputCurrLength),
                                                    &iFrameCount,
                                                    &ResizeNeeded);

            //If decoder returned error, report it to the client via a callback
            if ((OMX_FALSE == DecodeReturn) && (OMX_FALSE == iEndofStream))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithMarker ErrorStreamCorrupt callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventError,
                 OMX_ErrorStreamCorrupt,
                 0,
                 NULL);
            }

            if (ResizeNeeded == OMX_TRUE)
            {
                if (0 != OutputLength)
                {
                    iOutputFrameLength = OutputLength;

                    //Set the current timestamp to the output buffer timestamp for the first output frame
                    //Later it will be done at the time of dequeue
                    //(reason here is that at the time of dequeue, output buffer timestamp was equal to timestamp of config input buffer, not the first input buffer)
                    ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
                }

                // send port settings changed event
                OMX_COMPONENTTYPE* pHandle = (OMX_COMPONENTTYPE*) ipAppPriv->AmrHandle;

                iResizePending = OMX_TRUE;

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventPortSettingsChanged, //The command was completed
                 OMX_PORT_OUTPUTPORT_INDEX,
                 0,
                 NULL);

            }

            ipAmrOutputBuffer->nFilledLen += OutputLength;
            if (OutputLength > 0)
            {
                iCurrentTimestamp += OMX_AMR_DEC_FRAME_INTERVAL;
            }
            //offset not required in our case, set it to zero
            ipAmrOutputBuffer->nOffset = 0;


            /* Return the input buffer it has been consumed fully or decoder returned error*/
            if ((iInputCurrLength == 0) || (OMX_FALSE == DecodeReturn))
            {
                pAmrInputBuffer->nFilledLen = 0;
                AmrComponentReturnInputBuffer(pAmrInputBuffer, pInPort);
                iNewInBufferRequired = OMX_TRUE;
                iIsInputBufferEnded = OMX_TRUE;
            }
            else
            {
                iNewInBufferRequired = OMX_FALSE;
                iIsInputBufferEnded = OMX_FALSE;
            }
        }


        /* If EOS flag has come from the client & there are no more
         * input buffers to decode, send the callback to the client
         */
        if (OMX_TRUE == iEndofStream)
        {
            if ((0 == iInputCurrLength) || (OMX_FALSE == DecodeReturn))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithMarker EOS callback send"));

                (*(ipCallbacks->EventHandler))
                (pHandle,
                 iCallbackData,
                 OMX_EventBufferFlag,
                 1,
                 OMX_BUFFERFLAG_EOS,
                 NULL);

                iNewInBufferRequired = OMX_TRUE;
                //Mark this flag false once the callback has been send back
                iEndofStream = OMX_FALSE;

                ipAmrOutputBuffer->nFlags |= OMX_BUFFERFLAG_EOS;
                AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);

                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithMarker OUT"));
                return;
            }

        }

        //Send the output buffer back when it has become full
        if (((ipAmrOutputBuffer->nAllocLen - ipAmrOutputBuffer->nFilledLen) < (iOutputFrameLength))
                || (OMX_TRUE == ResizeNeeded))
        {
            AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);
        }

        /* If there is some more processing left with current buffers, re-schedule the AO
         * Do not go for more than one round of processing at a time.
         * This may block the AO longer than required.
         */
        if ((iInputCurrLength != 0 || GetQueueNumElem(pInputQueue) > 0)
                && (GetQueueNumElem(pOutputQueue) > 0) && (ResizeNeeded == OMX_FALSE))
        {
            RunIfNotReady();
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrDecodeWithMarker OUT"));
    return;
}

void OpenmaxAmrAO::AmrComponentReturnInputBuffer(OMX_BUFFERHEADERTYPE* pAmrInputBuffer, AmrComponentPortType *pPort)
{
    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;

    if (iNumInputBuffer)
    {
        iNumInputBuffer--;
    }

    //Callback for releasing the input buffer
    (*(ipCallbacks->EmptyBufferDone))
    (pHandle, iCallbackData, pAmrInputBuffer);

    pAmrInputBuffer = NULL;

}

/**
 * Returns Output Buffer back to the IL client
 */
void OpenmaxAmrAO::AmrComponentReturnOutputBuffer(OMX_BUFFERHEADERTYPE* pAmrOutputBuffer,
        AmrComponentPortType *pPort)
{
    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;

    //Callback for sending back the output buffer
    (*(ipCallbacks->FillBufferDone))
    (pHandle, iCallbackData, pAmrOutputBuffer);

    if (iOutBufferCount)
    {
        iOutBufferCount--;
    }

    pPort->NumBufferFlushed++;
    iNewOutBufRequired = OMX_TRUE;
}

/** The panic function that exits from the application.
 */
OMX_S32 OpenmaxAmrAO::AmrComponentPanic()
{
    OSCL_ASSERT(false);
    OsclError::Panic("PVERROR", OsclErrGeneral);
    return 0;
}


/** Flushes all the buffers under processing by the given port.
	* This function is called due to a state change of the component, typically
	* @param Component the component which owns the port to be flushed
	* @param PortIndex the ID of the port to be flushed
	*/

OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentFlushPort(OMX_S32 PortIndex)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentFlushPort IN"));

    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;

    QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;
    QueueType* pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;

    OMX_BUFFERHEADERTYPE* pOutputBuff;
    OMX_BUFFERHEADERTYPE* pInputBuff;

    if (OMX_PORT_INPUTPORT_INDEX == PortIndex || OMX_PORT_ALLPORT_INDEX == PortIndex)
    {
        while ((GetQueueNumElem(pInputQueue) > 0))
        {
            pInputBuff = (OMX_BUFFERHEADERTYPE*) DeQueue(pInputQueue);
            (*(ipCallbacks->EmptyBufferDone))
            (pHandle, iCallbackData, pInputBuff);
            iNumInputBuffer--;
        }
        // if a buffer was previously dequeued, it wasnt freed in above loop. return it now
        if ((iNumInputBuffer > 0) && (OMX_FALSE == iIsInputBufferEnded) && ipAmrInputBuffer)
        {
            ipAmrInputBuffer->nFilledLen = 0;
            (*(ipCallbacks->EmptyBufferDone))
            (pHandle, iCallbackData, ipAmrInputBuffer);
            iNumInputBuffer--;
            iIsInputBufferEnded = OMX_TRUE;
            iInputCurrLength = 0;
        }
    }

    if (OMX_PORT_OUTPUTPORT_INDEX == PortIndex || OMX_PORT_ALLPORT_INDEX == PortIndex)
    {
        if ((OMX_FALSE == iNewOutBufRequired) && (iOutBufferCount > 0))
        {
            if (ipAmrOutputBuffer)
            {
                (*(ipCallbacks->FillBufferDone))
                (pHandle, iCallbackData, ipAmrOutputBuffer);
                iOutBufferCount--;
                iNewOutBufRequired = OMX_TRUE;
            }
        }


        while ((GetQueueNumElem(pOutputQueue) > 0))
        {
            pOutputBuff = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
            pOutputBuff->nFilledLen = 0;
            (*(ipCallbacks->FillBufferDone))
            (pHandle, iCallbackData, pOutputBuff);
            iOutBufferCount--;
        }

    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentFlushPort OUT"));
    return OMX_ErrorNone;
}


/** This function is called by the omx core when the component
	* is disposed by the IL client with a call to FreeHandle().
	* \param Component, the component to be disposed
	*/

OMX_ERRORTYPE OpenmaxAmrAO::DestroyComponent()
{
    OMX_U32 ii;

    if (iIsInit != OMX_FALSE)
    {
        AmrComponentDeInit();
    }

    /*Deinitialize and free ports semaphores and Queue*/
    for (ii = 0; ii < iNumPorts; ii++)
    {
        if (ipPorts[ii]->pBufferQueue != NULL)
        {
            QueueDeinit(ipPorts[ii]->pBufferQueue);
            oscl_free(ipPorts[ii]->pBufferQueue);
            ipPorts[ii]->pBufferQueue = NULL;
        }
        /*Free port*/
        if (ipPorts[ii] != NULL)
        {
            oscl_free(ipPorts[ii]);
            ipPorts[ii] = NULL;
        }
    }

    if (ipPorts)
    {
        oscl_free(ipPorts);
        ipPorts = NULL;
    }

    iState = OMX_StateLoaded;

    if (ipInputCurrBuffer)
    {
        oscl_free(ipInputCurrBuffer);
        ipInputCurrBuffer = NULL;
    }

    if (ipTempInputBuffer)
    {
        oscl_free(ipTempInputBuffer);
        ipTempInputBuffer = NULL;
    }

    if (ipAmrDec)
    {
        OSCL_DELETE(ipAmrDec);
        ipAmrDec = NULL;
    }

    if (ipCoreDescriptor != NULL)
    {

        if (ipCoreDescriptor->pMessageQueue != NULL)
        {
            /* De-initialize the asynchronous command queue */
            QueueDeinit(ipCoreDescriptor->pMessageQueue);
            oscl_free(ipCoreDescriptor->pMessageQueue);
            ipCoreDescriptor->pMessageQueue = NULL;
        }

        oscl_free(ipCoreDescriptor);
        ipCoreDescriptor = NULL;
    }

    if (ipAppPriv)
    {
        ipAppPriv->AmrHandle = NULL;

        oscl_free(ipAppPriv);
        ipAppPriv = NULL;
    }

    return OMX_ErrorNone;
}


/**
 * Disable Single Port
 */
void OpenmaxAmrAO::AmrComponentDisableSinglePort(OMX_U32 PortIndex)
{
    ipPorts[PortIndex]->PortParam.bEnabled = OMX_FALSE;

    if (PORT_IS_POPULATED(ipPorts[PortIndex]) && OMX_TRUE == iIsInit)
    {
        if (OMX_FALSE == ipPorts[PortIndex]->IdleToLoadedFlag)
        {
            iStateTransitionFlag = OMX_TRUE;
            return;
        }
        else
        {
            ipPorts[PortIndex]->PortParam.bPopulated = OMX_FALSE;
        }
    }

    ipPorts[PortIndex]->NumBufferFlushed = 0;
}


/** Disables the specified port. This function is called due to a request by the IL client
	* @param Component the component which owns the port to be disabled
	* @param PortIndex the ID of the port to be disabled
	*/
OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentDisablePort(OMX_S32 PortIndex)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDisablePort IN"));
    OMX_U32 ii;

    if (-1 == PortIndex)
    {
        for (ii = 0; ii < iNumPorts; ii++)
        {
            ipPorts[ii]->IsPortFlushed = OMX_TRUE;
        }

        /*Flush all ports*/
        AmrComponentFlushPort(PortIndex);

        for (ii = 0; ii < iNumPorts; ii++)
        {
            ipPorts[ii]->IsPortFlushed = OMX_FALSE;
        }
    }
    else
    {
        /*Flush the port specified*/
        ipPorts[PortIndex]->IsPortFlushed = OMX_TRUE;
        AmrComponentFlushPort(PortIndex);
        ipPorts[PortIndex]->IsPortFlushed = OMX_FALSE;
    }

    /*Disable ports*/
    if (PortIndex != -1)
    {
        AmrComponentDisableSinglePort(PortIndex);
    }
    else
    {
        for (ii = 0; ii < iNumPorts; ii++)
        {
            AmrComponentDisableSinglePort(ii);
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDisablePort OUT"));

    return OMX_ErrorNone;
}

/**
 * Enable Single Port
 */
void OpenmaxAmrAO::AmrComponentEnableSinglePort(OMX_U32 PortIndex)
{
    ipPorts[PortIndex]->PortParam.bEnabled = OMX_TRUE;

    if (!PORT_IS_POPULATED(ipPorts[PortIndex]) && OMX_TRUE == iIsInit)
    {
        if (OMX_FALSE == ipPorts[PortIndex]->LoadedToIdleFlag)
        {
            iStateTransitionFlag = OMX_TRUE;
            return;
        }
        else
        {
            ipPorts[PortIndex]->PortParam.bPopulated = OMX_TRUE;
        }
    }
}

/** Enables the specified port. This function is called due to a request by the IL client
	* @param Component the component which owns the port to be enabled
	* @param PortIndex the ID of the port to be enabled
	*/
OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentEnablePort(OMX_S32 PortIndex)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentEnablePort IN"));

    OMX_U32 ii;

    /*Enable port/s*/
    if (PortIndex != -1)
    {
        AmrComponentEnableSinglePort(PortIndex);
    }
    else
    {
        for (ii = 0; ii < iNumPorts; ii++)
        {
            AmrComponentEnableSinglePort(ii);
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentEnablePort OUT"));
    return OMX_ErrorNone;
}

//Not implemented & supported in case of base profile components

void OpenmaxAmrAO::AmrComponentGetRolesOfComponent(OMX_STRING* aRoleString)
{
    *aRoleString = "audio_decoder.amr";
}



OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentTunnelRequest(
    OMX_IN  OMX_HANDLETYPE hComp,
    OMX_IN  OMX_U32 nPort,
    OMX_IN  OMX_HANDLETYPE hTunneledComp,
    OMX_IN  OMX_U32 nTunneledPort,
    OMX_INOUT  OMX_TUNNELSETUPTYPE* pTunnelSetup)
{
    return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentGetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_INOUT OMX_PTR pComponentConfigStructure)
{
    return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentSetConfig(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nIndex,
    OMX_IN  OMX_PTR pComponentConfigStructure)
{
    return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentGetExtensionIndex(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_STRING cParameterName,
    OMX_OUT OMX_INDEXTYPE* pIndexType)
{
    return OMX_ErrorNotImplemented;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentGetState(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_OUT OMX_STATETYPE* pState)
{
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;

    pOpenmaxAOType->GetState(pState);

    return OMX_ErrorNone;
}

void OpenmaxAmrAO::GetState(OMX_OUT OMX_STATETYPE* pState)
{
    *pState = iState;
}



//Active object constructor
OpenmaxAmrAO::OpenmaxAmrAO() :
        OsclActiveObject(OsclActiveObject::EPriorityNominal, "OMXAmrDec")
{

    iLogger = PVLogger::GetLoggerObject("PVMFOMXAudioDecNode");
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : constructed"));

    //Flag to call BufferMgmtFunction in the Run() when the component state is executing
    iBufferExecuteFlag = OMX_FALSE;
    ipAppPriv = NULL;

    ipCallbacks = NULL;
    iCallbackData = NULL;
    iState = OMX_StateLoaded;


    ipCoreDescriptor = NULL;
    iNumInputBuffer = 0;

    ipFrameDecodeBuffer = NULL;
    iPartialFrameAssembly = OMX_FALSE;
    iIsInputBufferEnded = OMX_TRUE;
    iEndofStream = OMX_FALSE;
    ipTempInputBuffer = NULL;


    ipTargetComponent = NULL;
    iTargetMarkData = NULL;
    iNewOutBufRequired = OMX_TRUE;

    iTempConsumedLength = 0;
    iOutBufferCount = 0;
    iCodecReady = OMX_FALSE;
    ipInputCurrBuffer = NULL;
    iInputCurrLength = 0;
    iFrameCount = 0;
    iStateTransitionFlag = OMX_FALSE;
    iFlushPortFlag = OMX_FALSE;
    iEndOfFrameFlag = OMX_FALSE;
    ipAmrInputBuffer = NULL;
    ipAmrOutputBuffer = NULL;


    iFirstFragment = OMX_FALSE;
    iResizePending = OMX_FALSE;

    iFrameTimestamp = 0;
    iSilenceInsertionInProgress = OMX_FALSE;

    iNumPorts = 0;
    ipPorts = NULL;

    //Indicate whether component has been already initialized */
    iIsInit = OMX_FALSE;

    iGroupPriority = 0;
    iGroupID = 0;

    ipMark = NULL;

    ipAmrDec = NULL;

    if (!IsAdded())
    {
        AddToScheduler();
    }
}


//Active object destructor
OpenmaxAmrAO::~OpenmaxAmrAO()
{
    if (IsAdded())
    {
        RemoveFromScheduler();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : destructed"));
}


/** The Initialization function
 */
OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentInit IN"));

    OMX_BOOL Status;

    if (OMX_TRUE == iIsInit)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentInit error incorrect operation"));
        return OMX_ErrorIncorrectStateOperation;
    }
    iIsInit = OMX_TRUE;

    //amr lib init
    if (!iCodecReady)
    {
        Status = ipAmrDec->AmrDecInit(ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.eAMRFrameFormat, ipPorts[OMX_PORT_INPUTPORT_INDEX]->AudioAmrParam.eAMRBandMode);
        iCodecReady = OMX_TRUE;
    }

    iInputCurrLength = 0;
    //Used in dynamic port reconfiguration
    iFrameCount = 0;

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentInit OUT"));

    if (OMX_TRUE == Status)
    {
        return OMX_ErrorNone;
    }
    else
    {
        return OMX_ErrorInvalidComponent;
    }
}

/** This function is called upon a transition to the idle or invalid state.
 *  Also it is called by the AmrComponentDestructor() function
 */
OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentDeInit()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDeInit IN"));

    iIsInit = OMX_FALSE;

    if (iCodecReady)
    {
        ipAmrDec->AmrDecDeinit();
        iCodecReady = OMX_FALSE;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDeInit OUT"));

    return OMX_ErrorNone;

}

OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentGetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR ComponentParameterStructure)
{

    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->GetParameter(hComponent, nParamIndex, ComponentParameterStructure);
    return Status;

}

OMX_ERRORTYPE OpenmaxAmrAO::GetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_INOUT OMX_PTR ComponentParameterStructure)

{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter IN"));

    OMX_PRIORITYMGMTTYPE* pPrioMgmt;
    OMX_PARAM_BUFFERSUPPLIERTYPE* pBufSupply;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef;
    OMX_PORT_PARAM_TYPE* pPortDomains;
    OMX_U32 PortIndex;

    OMX_AUDIO_PARAM_PORTFORMATTYPE* pAudioPortFormat;
    OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
    OMX_AUDIO_PARAM_AMRTYPE* pAudioAmr;

    AmrComponentPortType* pComponentPort;

    if (NULL == ComponentParameterStructure)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error bad parameter"));
        return OMX_ErrorBadParameter;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamPriorityMgmt:
        {
            pPrioMgmt = (OMX_PRIORITYMGMTTYPE*) ComponentParameterStructure;
            SetHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE));
            pPrioMgmt->nGroupPriority = iGroupPriority;
            pPrioMgmt->nGroupID = iGroupID;
        }
        break;

        case OMX_IndexParamAudioInit:
        {
            SetHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
            oscl_memcpy(ComponentParameterStructure, &iPortTypesParam, sizeof(OMX_PORT_PARAM_TYPE));
        }
        break;


        //Following 3 cases have a single common piece of code to be executed
        case OMX_IndexParamVideoInit:
        case OMX_IndexParamImageInit:
        case OMX_IndexParamOtherInit:
        {
            pPortDomains = (OMX_PORT_PARAM_TYPE*) ComponentParameterStructure;
            SetHeader(pPortDomains, sizeof(OMX_PORT_PARAM_TYPE));
            pPortDomains->nPorts = 0;
            pPortDomains->nStartPortNumber = 0;
        }
        break;

        case OMX_IndexParamAudioPortFormat:
        {
            pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*) ComponentParameterStructure;
            //Added to pass parameter test
            if (pAudioPortFormat->nIndex > ipPorts[pAudioPortFormat->nPortIndex]->AudioParam.nIndex)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error index out of range"));
                return OMX_ErrorNoMore;
            }
            SetHeader(pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            if (pAudioPortFormat->nPortIndex <= 1)
            {
                pComponentPort = (AmrComponentPortType*) ipPorts[pAudioPortFormat->nPortIndex];
                oscl_memcpy(pAudioPortFormat, &pComponentPort->AudioParam, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamAudioPcm:
        {
            pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*) ComponentParameterStructure;
            if (pAudioPcmMode->nPortIndex > 1)
            {
                return OMX_ErrorBadPortIndex;
            }
            PortIndex = pAudioPcmMode->nPortIndex;
            oscl_memcpy(pAudioPcmMode, &ipPorts[PortIndex]->AudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            SetHeader(pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        break;

        case OMX_IndexParamAudioAmr:
        {
            pAudioAmr = (OMX_AUDIO_PARAM_AMRTYPE*) ComponentParameterStructure;
            if (pAudioAmr->nPortIndex != 0)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
            PortIndex = pAudioAmr->nPortIndex;
            oscl_memcpy(pAudioAmr, &ipPorts[PortIndex]->AudioAmrParam, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
            SetHeader(pAudioAmr, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
        }
        break;

        case OMX_IndexParamPortDefinition:
        {
            pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
            PortIndex = pPortDef->nPortIndex;
            if (PortIndex >= iNumPorts)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
            oscl_memcpy(pPortDef, &ipPorts[PortIndex]->PortParam, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
        }
        break;

        case OMX_IndexParamCompBufferSupplier:
        {
            pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*) ComponentParameterStructure;
            PortIndex = pBufSupply->nPortIndex;
            if (PortIndex >= iNumPorts)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
            SetHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));

            if (OMX_DirInput == ipPorts[PortIndex]->PortParam.eDir)
            {
                pBufSupply->eBufferSupplier = OMX_BufferSupplyUnspecified;
            }
            else
            {
                SetHeader(pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
                pBufSupply->eBufferSupplier = OMX_BufferSupplyUnspecified;
            }
        }
        break;

        case(OMX_INDEXTYPE) PV_OMX_COMPONENT_CAPABILITY_TYPE_INDEX:
        {
            PV_OMXComponentCapabilityFlagsType *pCap_flags = (PV_OMXComponentCapabilityFlagsType *) ComponentParameterStructure;
            if (NULL == pCap_flags)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error pCap_flags NULL"));
                return OMX_ErrorBadParameter;
            }
            oscl_memcpy(pCap_flags, &iPVCapabilityFlags, sizeof(iPVCapabilityFlags));

        }
        break;

        default:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter error Unsupported Index"));
            return OMX_ErrorUnsupportedIndex;
        }
        break;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : GetParameter OUT"));

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentSetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_IN  OMX_PTR ComponentParameterStructure)
{

    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->SetParameter(hComponent, nParamIndex, ComponentParameterStructure);

    return Status;
}


OMX_ERRORTYPE OpenmaxAmrAO::SetParameter(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_INDEXTYPE nParamIndex,
    OMX_IN  OMX_PTR ComponentParameterStructure)

{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter IN"));

    OMX_ERRORTYPE ErrorType = OMX_ErrorNone;
    OMX_PRIORITYMGMTTYPE* pPrioMgmt;
    OMX_AUDIO_PARAM_PORTFORMATTYPE* pAudioPortFormat;
    OMX_AUDIO_PARAM_PCMMODETYPE* pAudioPcmMode;
    OMX_AUDIO_PARAM_AMRTYPE* pAudioAmr;
    OMX_PARAM_BUFFERSUPPLIERTYPE* pBufSupply;
    OMX_PARAM_PORTDEFINITIONTYPE* pPortDef ;
    OMX_U32 PortIndex;
    OMX_PARAM_COMPONENTROLETYPE* pCompRole;

    AmrComponentPortType* pComponentPort;

    if (NULL == ComponentParameterStructure)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error bad parameter"));
        return OMX_ErrorBadParameter;
    }

    switch (nParamIndex)
    {
        case OMX_IndexParamAudioInit:
        {
            /*Check Structure Header*/
            CheckHeader(ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
            if (ErrorType != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error audio init failed"));
                return ErrorType;
            }
            oscl_memcpy(&iPortTypesParam, ComponentParameterStructure, sizeof(OMX_PORT_PARAM_TYPE));
        }
        break;

        case OMX_IndexParamAudioPortFormat:
        {
            pAudioPortFormat = (OMX_AUDIO_PARAM_PORTFORMATTYPE*) ComponentParameterStructure;
            PortIndex = pAudioPortFormat->nPortIndex;
            /*Check Structure Header and verify component state*/
            ErrorType = BaseComponentParameterSanityCheck(hComponent, PortIndex, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            if (ErrorType != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error parameter sanity check error"));
                return ErrorType;
            }
            if (PortIndex <= 1)
            {
                pComponentPort = (AmrComponentPortType*) ipPorts[PortIndex];
                oscl_memcpy(&pComponentPort->AudioParam, pAudioPortFormat, sizeof(OMX_AUDIO_PARAM_PORTFORMATTYPE));
            }
            else
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
        }
        break;

        case OMX_IndexParamAudioPcm:
        {
            pAudioPcmMode = (OMX_AUDIO_PARAM_PCMMODETYPE*) ComponentParameterStructure;
            PortIndex = pAudioPcmMode->nPortIndex;
            /*Check Structure Header and verify component State*/
            ErrorType = BaseComponentParameterSanityCheck(hComponent, PortIndex, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
            oscl_memcpy(&ipPorts[PortIndex]->AudioPcmMode, pAudioPcmMode, sizeof(OMX_AUDIO_PARAM_PCMMODETYPE));
        }
        break;

        case OMX_IndexParamAudioAmr:
        {
            pAudioAmr = (OMX_AUDIO_PARAM_AMRTYPE*) ComponentParameterStructure;
            PortIndex = pAudioAmr->nPortIndex;
            /*Check Structure Header and verify component state*/
            ErrorType = BaseComponentParameterSanityCheck(hComponent, PortIndex, pAudioAmr, sizeof(OMX_AUDIO_PARAM_AMRTYPE));
            if (ErrorType != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error parameter sanity check error"));
                return ErrorType;
            }
            oscl_memcpy(&ipPorts[PortIndex]->AudioAmrParam, pAudioAmr, sizeof(OMX_AUDIO_PARAM_AMRTYPE));

            //If the band mode turns out to be WB, set the sampling freq to 16KHz
            if ((pAudioAmr->eAMRBandMode >= OMX_AUDIO_AMRBandModeWB0) &&
                    (pAudioAmr->eAMRBandMode <= OMX_AUDIO_AMRBandModeWB8))
            {
                ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->AudioPcmMode.nSamplingRate = 16000;
            }
        }
        break;

        case OMX_IndexParamPriorityMgmt:
        {
            if (iState != OMX_StateLoaded && iState != OMX_StateWaitForResources)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error incorrect state error"));
                return OMX_ErrorIncorrectStateOperation;
            }
            pPrioMgmt = (OMX_PRIORITYMGMTTYPE*) ComponentParameterStructure;
            if ((ErrorType = CheckHeader(pPrioMgmt, sizeof(OMX_PRIORITYMGMTTYPE))) != OMX_ErrorNone)
            {
                break;
            }
            iGroupPriority = pPrioMgmt->nGroupPriority;
            iGroupID = pPrioMgmt->nGroupID;
        }
        break;

        case OMX_IndexParamPortDefinition:
        {
            pPortDef  = (OMX_PARAM_PORTDEFINITIONTYPE*) ComponentParameterStructure;
            PortIndex = pPortDef->nPortIndex;

            ErrorType = BaseComponentParameterSanityCheck(hComponent, PortIndex, pPortDef, sizeof(OMX_PARAM_PORTDEFINITIONTYPE));
            if (ErrorType != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error parameter sanity check error"));
                return ErrorType;
            }

            ipPorts[PortIndex]->PortParam.nBufferCountActual = pPortDef->nBufferCountActual;
            ipPorts[PortIndex]->PortParam.nBufferSize = pPortDef->nBufferSize;
        }
        break;

        case OMX_IndexParamCompBufferSupplier:
        {
            pBufSupply = (OMX_PARAM_BUFFERSUPPLIERTYPE*) ComponentParameterStructure;
            PortIndex = pBufSupply->nPortIndex;

            ErrorType = BaseComponentParameterSanityCheck(hComponent, PortIndex, pBufSupply, sizeof(OMX_PARAM_BUFFERSUPPLIERTYPE));
            if (OMX_ErrorIncorrectStateOperation == ErrorType)
            {
                if (PORT_IS_ENABLED(ipPorts[pBufSupply->nPortIndex]))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error incorrect state error"));
                    return OMX_ErrorIncorrectStateOperation;
                }
            }
            else if (ErrorType != OMX_ErrorNone)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error parameter sanity check error"));
                return ErrorType;
            }

            if (pBufSupply->eBufferSupplier == OMX_BufferSupplyUnspecified)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter OUT"));
                return OMX_ErrorNone;
            }

            ErrorType = OMX_ErrorNone;
        }
        break;

        case OMX_IndexParamStandardComponentRole:
        {
            pCompRole = (OMX_PARAM_COMPONENTROLETYPE*) ComponentParameterStructure;
            if ((ErrorType = CheckHeader(pCompRole, sizeof(OMX_PARAM_COMPONENTROLETYPE))) != OMX_ErrorNone)
            {
                break;
            }
            strcpy((OMX_STRING)iComponentRole, (OMX_STRING)pCompRole->cRole);
        }
        break;


        default:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter error bad parameter"));
            return OMX_ErrorBadParameter;
        }
        break;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetParameter OUT"));
    return ErrorType;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentUseBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->UseBuffer(hComponent, ppBufferHdr, nPortIndex, pAppPrivate, nSizeBytes, pBuffer);

    return Status;
}


OMX_ERRORTYPE OpenmaxAmrAO::UseBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** ppBufferHdr,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes,
    OMX_IN OMX_U8* pBuffer)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer IN"));
    AmrComponentPortType* pBaseComponentPort;
    OMX_U32 ii;

    if (nPortIndex >= iNumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer error bad port index"));
        return OMX_ErrorBadPortIndex;
    }

    pBaseComponentPort = ipPorts[nPortIndex];

    if (pBaseComponentPort->TransientState != OMX_StateIdle)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer error incorrect state"));
        return OMX_ErrorIncorrectStateTransition;
    }

    if (NULL == pBaseComponentPort->pBuffer)
    {
        pBaseComponentPort->pBuffer = (OMX_BUFFERHEADERTYPE**) oscl_calloc(pBaseComponentPort->PortParam.nBufferCountActual, sizeof(OMX_BUFFERHEADERTYPE*));
        pBaseComponentPort->BufferState = (OMX_U32*) oscl_calloc(pBaseComponentPort->PortParam.nBufferCountActual, sizeof(OMX_U32));
    }

    for (ii = 0; ii < pBaseComponentPort->PortParam.nBufferCountActual; ii++)
    {
        if (!(pBaseComponentPort->BufferState[ii] & BUFFER_ALLOCATED) &&
                !(pBaseComponentPort->BufferState[ii] & BUFFER_ASSIGNED))
        {
            pBaseComponentPort->pBuffer[ii] = (OMX_BUFFERHEADERTYPE*) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if (NULL == pBaseComponentPort->pBuffer[ii])
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }
            SetHeader(pBaseComponentPort->pBuffer[ii], sizeof(OMX_BUFFERHEADERTYPE));
            pBaseComponentPort->pBuffer[ii]->pBuffer = pBuffer;
            pBaseComponentPort->pBuffer[ii]->nAllocLen = nSizeBytes;
            pBaseComponentPort->pBuffer[ii]->nFilledLen = 0;
            pBaseComponentPort->pBuffer[ii]->nOffset = 0;
            pBaseComponentPort->pBuffer[ii]->nFlags = 0;
            pBaseComponentPort->pBuffer[ii]->pPlatformPrivate = pBaseComponentPort;
            pBaseComponentPort->pBuffer[ii]->pAppPrivate = pAppPrivate;
            pBaseComponentPort->pBuffer[ii]->nTickCount = 0;
            pBaseComponentPort->pBuffer[ii]->nTimeStamp = 0;
            *ppBufferHdr = pBaseComponentPort->pBuffer[ii];
            if (OMX_DirInput == pBaseComponentPort->PortParam.eDir)
            {
                pBaseComponentPort->pBuffer[ii]->nInputPortIndex = nPortIndex;
                pBaseComponentPort->pBuffer[ii]->nOutputPortIndex = iNumPorts; // here is assigned a non-valid port index
            }
            else
            {
                pBaseComponentPort->pBuffer[ii]->nOutputPortIndex = nPortIndex;
                pBaseComponentPort->pBuffer[ii]->nInputPortIndex = iNumPorts; // here is assigned a non-valid port index
            }
            pBaseComponentPort->BufferState[ii] |= BUFFER_ASSIGNED;
            pBaseComponentPort->BufferState[ii] |= HEADER_ALLOCATED;
            pBaseComponentPort->NumAssignedBuffers++;
            if (pBaseComponentPort->PortParam.nBufferCountActual == pBaseComponentPort->NumAssignedBuffers)
            {
                pBaseComponentPort->PortParam.bPopulated = OMX_TRUE;
                if (OMX_TRUE == iStateTransitionFlag)
                {
                    //Reschedule the AO for a state change (Loaded->Idle) if its pending on buffer allocation
                    RunIfNotReady();
                    //Set the corresponding flags
                    pBaseComponentPort->LoadedToIdleFlag = OMX_TRUE;
                    pBaseComponentPort->IdleToLoadedFlag = OMX_FALSE;
                    iStateTransitionFlag = OMX_FALSE;
                }
            }
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer OUT"));
            return OMX_ErrorNone;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : UseBuffer OUT"));
    return OMX_ErrorNone;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentAllocateBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->AllocateBuffer(hComponent, pBuffer, nPortIndex, pAppPrivate, nSizeBytes);

    return Status;
}


OMX_ERRORTYPE OpenmaxAmrAO::AllocateBuffer(
    OMX_IN OMX_HANDLETYPE hComponent,
    OMX_INOUT OMX_BUFFERHEADERTYPE** pBuffer,
    OMX_IN OMX_U32 nPortIndex,
    OMX_IN OMX_PTR pAppPrivate,
    OMX_IN OMX_U32 nSizeBytes)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer IN"));

    AmrComponentPortType* pBaseComponentPort;
    OMX_U32 ii;

    if (nPortIndex >= iNumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer error bad port index"));
        return OMX_ErrorBadPortIndex;
    }

    pBaseComponentPort = ipPorts[nPortIndex];

    if (pBaseComponentPort->TransientState != OMX_StateIdle)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer error incorrect state"));
        return OMX_ErrorIncorrectStateTransition;
    }

    if (NULL == pBaseComponentPort->pBuffer)
    {
        pBaseComponentPort->pBuffer = (OMX_BUFFERHEADERTYPE**) oscl_calloc(pBaseComponentPort->PortParam.nBufferCountActual, sizeof(OMX_BUFFERHEADERTYPE*));
        pBaseComponentPort->BufferState = (OMX_U32*) oscl_calloc(pBaseComponentPort->PortParam.nBufferCountActual, sizeof(OMX_U32));
    }

    for (ii = 0; ii < pBaseComponentPort->PortParam.nBufferCountActual; ii++)
    {
        if (!(pBaseComponentPort->BufferState[ii] & BUFFER_ALLOCATED) &&
                !(pBaseComponentPort->BufferState[ii] & BUFFER_ASSIGNED))
        {
            pBaseComponentPort->pBuffer[ii] = (OMX_BUFFERHEADERTYPE*) oscl_malloc(sizeof(OMX_BUFFERHEADERTYPE));
            if (NULL == pBaseComponentPort->pBuffer[ii])
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }
            SetHeader(pBaseComponentPort->pBuffer[ii], sizeof(OMX_BUFFERHEADERTYPE));
            /* allocate the buffer */
            pBaseComponentPort->pBuffer[ii]->pBuffer = (OMX_BYTE) oscl_malloc(nSizeBytes);
            if (NULL == pBaseComponentPort->pBuffer[ii]->pBuffer)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }
            pBaseComponentPort->pBuffer[ii]->nAllocLen = nSizeBytes;
            pBaseComponentPort->pBuffer[ii]->nFlags = 0;
            pBaseComponentPort->pBuffer[ii]->pPlatformPrivate = pBaseComponentPort;
            pBaseComponentPort->pBuffer[ii]->pAppPrivate = pAppPrivate;
            *pBuffer = pBaseComponentPort->pBuffer[ii];
            pBaseComponentPort->BufferState[ii] |= BUFFER_ALLOCATED;
            pBaseComponentPort->BufferState[ii] |= HEADER_ALLOCATED;

            if (OMX_DirInput == pBaseComponentPort->PortParam.eDir)
            {
                pBaseComponentPort->pBuffer[ii]->nInputPortIndex = nPortIndex;
                // here is assigned a non-valid port index
                pBaseComponentPort->pBuffer[ii]->nOutputPortIndex = iNumPorts;
            }
            else
            {
                // here is assigned a non-valid port index
                pBaseComponentPort->pBuffer[ii]->nInputPortIndex = iNumPorts;
                pBaseComponentPort->pBuffer[ii]->nOutputPortIndex = nPortIndex;
            }

            pBaseComponentPort->NumAssignedBuffers++;

            if (pBaseComponentPort->PortParam.nBufferCountActual == pBaseComponentPort->NumAssignedBuffers)
            {
                pBaseComponentPort->PortParam.bPopulated = OMX_TRUE;

                if (OMX_TRUE == iStateTransitionFlag)
                {
                    //Reschedule the AO for a state change (Loaded->Idle) if its pending on buffer allocation
                    RunIfNotReady();
                    //Set the corresponding flags
                    pBaseComponentPort->LoadedToIdleFlag = OMX_TRUE;
                    pBaseComponentPort->IdleToLoadedFlag = OMX_FALSE;
                    iStateTransitionFlag = OMX_FALSE;
                }
            }

            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer OUT"));
            return OMX_ErrorNone;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AllocateBuffer OUT"));
    return OMX_ErrorInsufficientResources;
}

OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentFreeBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->FreeBuffer(hComponent, nPortIndex, pBuffer);

    return Status;
}


OMX_ERRORTYPE OpenmaxAmrAO::FreeBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_U32 nPortIndex,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FreeBuffer IN"));

    AmrComponentPortType* pBaseComponentPort;

    OMX_U32 ii;
    OMX_BOOL FoundBuffer;

    if (nPortIndex >= iNumPorts)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FreeBuffer error bad port index"));
        return OMX_ErrorBadPortIndex;
    }

    pBaseComponentPort = ipPorts[nPortIndex];

    if (pBaseComponentPort->TransientState != OMX_StateLoaded
            && pBaseComponentPort->TransientState != OMX_StateInvalid)
    {

        (*(ipCallbacks->EventHandler))
        (hComponent,
         iCallbackData,
         OMX_EventError, /* The command was completed */
         OMX_ErrorPortUnpopulated, /* The commands was a OMX_CommandStateSet */
         nPortIndex, /* The State has been changed in message->MessageParam2 */
         NULL);
    }

    for (ii = 0; ii < pBaseComponentPort->PortParam.nBufferCountActual; ii++)
    {
        if ((pBaseComponentPort->BufferState[ii] & BUFFER_ALLOCATED) &&
                (pBaseComponentPort->pBuffer[ii]->pBuffer == pBuffer->pBuffer))
        {

            pBaseComponentPort->NumAssignedBuffers--;
            oscl_free(pBuffer->pBuffer);
            pBuffer->pBuffer = NULL;

            if (pBaseComponentPort->BufferState[ii] & HEADER_ALLOCATED)
            {
                oscl_free(pBuffer);
                pBuffer = NULL;
            }
            pBaseComponentPort->BufferState[ii] = BUFFER_FREE;
            break;
        }
        else if ((pBaseComponentPort->BufferState[ii] & BUFFER_ASSIGNED) &&
                 (pBaseComponentPort->pBuffer[ii] == pBuffer))
        {

            pBaseComponentPort->NumAssignedBuffers--;

            if (pBaseComponentPort->BufferState[ii] & HEADER_ALLOCATED)
            {
                oscl_free(pBuffer);
                pBuffer = NULL;
            }

            pBaseComponentPort->BufferState[ii] = BUFFER_FREE;
            break;
        }
    }

    FoundBuffer = OMX_FALSE;

    for (ii = 0; ii < pBaseComponentPort->PortParam.nBufferCountActual; ii++)
    {
        if (pBaseComponentPort->BufferState[ii] != BUFFER_FREE)
        {
            FoundBuffer = OMX_TRUE;
            break;
        }
    }
    if (!FoundBuffer)
    {
        pBaseComponentPort->PortParam.bPopulated = OMX_FALSE;
        if (OMX_TRUE == iStateTransitionFlag)
        {
            //Reschedule the AO for a state change (Idle->Loaded) if its pending on buffer de-allocation
            RunIfNotReady();
            //Set the corresponding flags
            pBaseComponentPort->IdleToLoadedFlag = OMX_TRUE;
            pBaseComponentPort->LoadedToIdleFlag = OMX_FALSE;
            iStateTransitionFlag = OMX_FALSE;
            //Reset the decoding flags while freeing buffers
            if (OMX_PORT_INPUTPORT_INDEX == nPortIndex)
            {
                iIsInputBufferEnded = OMX_TRUE;
                iTempInputBufferFilledLength = 0;
                iTempConsumedLength = 0;
            }
            else if (OMX_PORT_OUTPUTPORT_INDEX == nPortIndex)
            {
                iNewOutBufRequired = OMX_TRUE;
            }
        }

        if (NULL != pBaseComponentPort->pBuffer)
        {
            oscl_free(pBaseComponentPort->pBuffer);
            pBaseComponentPort->pBuffer = NULL;
            oscl_free(pBaseComponentPort->BufferState);
            pBaseComponentPort->BufferState = NULL;
        }
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FreeBuffer OUT"));
    return OMX_ErrorNone;
}


/** Set Callbacks. It stores in the component private structure the pointers to the user application callbacs
	* @param hComponent the handle of the component
	* @param ipCallbacks the OpenMAX standard structure that holds the callback pointers
	* @param pAppData a pointer to a private structure, not covered by OpenMAX standard, in needed
    */

OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentSetCallbacks(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN  OMX_PTR pAppData)
{
    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->SetCallbacks(hComponent, pCallbacks, pAppData);

    return Status;
}

OMX_ERRORTYPE OpenmaxAmrAO::SetCallbacks(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_CALLBACKTYPE* pCallbacks,
    OMX_IN  OMX_PTR pAppData)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SetCallbacks"));
    ipCallbacks = pCallbacks;
    iCallbackData = pAppData;

    return OMX_ErrorNone;
}

OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentSendCommand(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_COMMANDTYPE Cmd,
    OMX_IN  OMX_U32 nParam,
    OMX_IN  OMX_PTR pCmdData)
{

    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->SendCommand(hComponent, Cmd, nParam, pCmdData);

    return Status;
}

OMX_ERRORTYPE OpenmaxAmrAO::SendCommand(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_COMMANDTYPE Cmd,
    OMX_IN  OMX_S32 nParam,
    OMX_IN  OMX_PTR pCmdData)
{

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand IN"));

    OMX_U32 ii;
    OMX_ERRORTYPE ErrMsgHandler = OMX_ErrorNone;
    QueueType* pMessageQueue;
    CoreMessage* Message;

    pMessageQueue = ipCoreDescriptor->pMessageQueue;

    if (OMX_StateInvalid == iState)
    {
        ErrMsgHandler = OMX_ErrorInvalidState;
    }

    switch (Cmd)
    {
        case OMX_CommandStateSet:
        {
            Message = (CoreMessage*) oscl_malloc(sizeof(CoreMessage));

            if (NULL == Message)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }

            Message->pComponent = (OMX_COMPONENTTYPE *) hComponent;
            Message->MessageType = SENDCOMMAND_MSG_TYPE;
            Message->MessageParam1 = OMX_CommandStateSet;
            Message->MessageParam2 = nParam;
            Message->pCmdData = pCmdData;

            if ((OMX_StateIdle == nParam) && (OMX_StateLoaded == iState))
            {
                ErrMsgHandler = AmrComponentInit();

                if (ErrMsgHandler != OMX_ErrorNone)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error component init"));
                    return OMX_ErrorInsufficientResources;
                }
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    ipPorts[ii]->TransientState = OMX_StateIdle;
                }
            }
            else if ((OMX_StateLoaded == nParam) && (OMX_StateIdle == iState))
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (PORT_IS_ENABLED(ipPorts[ii]))
                    {
                        ipPorts[ii]->TransientState = OMX_StateLoaded;
                    }
                }
            }
            else if (OMX_StateInvalid == nParam)
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (PORT_IS_ENABLED(ipPorts[ii]))
                    {
                        ipPorts[ii]->TransientState = OMX_StateInvalid;
                    }
                }
            }
            else if (((OMX_StateIdle == nParam) || (OMX_StatePause == nParam))
                     && (OMX_StateExecuting == iState))
            {
                iBufferExecuteFlag = OMX_FALSE;
            }

        }
        break;

        case OMX_CommandFlush:
        {
            Message = (CoreMessage*) oscl_malloc(sizeof(CoreMessage));

            if (NULL == Message)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }

            Message->pComponent = (OMX_COMPONENTTYPE *) hComponent;
            Message->MessageType = SENDCOMMAND_MSG_TYPE;
            Message->MessageParam1 = OMX_CommandFlush;
            Message->MessageParam2 = nParam;
            Message->pCmdData = pCmdData;

            if ((iState != OMX_StateExecuting) && (iState != OMX_StatePause))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                break;

            }
            if ((nParam != -1) && ((OMX_U32) nParam >= iNumPorts))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error bad port index"));
                return OMX_ErrorBadPortIndex;
            }
            //Assume that reposition command has come
            iRepositionFlag = OMX_TRUE;
            //Reset the silence insertion logic also
            iSilenceInsertionInProgress = OMX_FALSE;
            // reset decoder
            if (ipAmrDec)
            {
                ipAmrDec->ResetDecoder();
            }

            AmrComponentSetPortFlushFlag(iNumPorts, nParam, OMX_TRUE);
            AmrComponentSetNumBufferFlush(iNumPorts, -1, 0);
        }
        break;

        case OMX_CommandPortDisable:
        {
            if ((nParam != -1) && ((OMX_U32) nParam >= iNumPorts))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error bad port index"));
                return OMX_ErrorBadPortIndex;
            }

            iResizePending = OMX_FALSE;

            if (-1 == nParam)
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (!PORT_IS_ENABLED(ipPorts[ii]))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                        ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                        break;
                    }
                    else
                    {
                        ipPorts[ii]->TransientState = OMX_StateLoaded;
                    }
                }
            }
            else
            {
                if (!PORT_IS_ENABLED(ipPorts[nParam]))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                    ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                    break;
                }
                else
                {
                    ipPorts[nParam]->TransientState = OMX_StateLoaded;
                }
            }

            Message = (CoreMessage*) oscl_malloc(sizeof(CoreMessage));
            if (NULL == Message)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }

            Message->pComponent = (OMX_COMPONENTTYPE *) hComponent;
            if (OMX_ErrorNone == ErrMsgHandler)
            {
                Message->MessageType = SENDCOMMAND_MSG_TYPE;
                Message->MessageParam2 = nParam;
            }
            else
            {
                Message->MessageType = ERROR_MSG_TYPE;
                Message->MessageParam2 = ErrMsgHandler;
            }
            Message->MessageParam1 = OMX_CommandPortDisable;
            Message->pCmdData = pCmdData;
        }
        break;


        case OMX_CommandPortEnable:
        {
            if ((nParam != -1) && ((OMX_U32) nParam >= iNumPorts))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error bad port index"));
                return OMX_ErrorBadPortIndex;
            }

            if (-1 == nParam)
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (PORT_IS_ENABLED(ipPorts[ii]))
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                        ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                        break;
                    }
                    else
                    {
                        ipPorts[ii]->TransientState = OMX_StateIdle;
                    }
                }
            }
            else
            {
                if (PORT_IS_ENABLED(ipPorts[nParam]))
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                    ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                    break;
                }
                else
                {
                    ipPorts[nParam]->TransientState = OMX_StateIdle;
                }
            }

            Message = (CoreMessage*) oscl_malloc(sizeof(CoreMessage));
            if (NULL == Message)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }

            Message->pComponent = (OMX_COMPONENTTYPE *) hComponent;
            if (OMX_ErrorNone == ErrMsgHandler)
            {
                Message->MessageType = SENDCOMMAND_MSG_TYPE;
            }
            else
            {
                Message->MessageType = ERROR_MSG_TYPE;
            }

            Message->MessageParam1 = OMX_CommandPortEnable;
            Message->MessageParam2 = nParam;
            Message->pCmdData = pCmdData;
        }
        break;


        case OMX_CommandMarkBuffer:
        {
            if ((iState != OMX_StateExecuting) && (iState != OMX_StatePause))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error incorrect state"));
                ErrMsgHandler = OMX_ErrorIncorrectStateOperation;
                break;
            }

            if ((nParam != -1) && ((OMX_U32) nParam >= iNumPorts))
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error bad port index"));
                return OMX_ErrorBadPortIndex;
            }

            Message = (CoreMessage*) oscl_malloc(sizeof(CoreMessage));
            if (NULL == Message)
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error insufficient resources"));
                return OMX_ErrorInsufficientResources;
            }
            Message->pComponent = (OMX_COMPONENTTYPE *) hComponent;
            Message->MessageType = SENDCOMMAND_MSG_TYPE;
            Message->MessageParam1 = OMX_CommandMarkBuffer;
            Message->MessageParam2 = nParam;
            Message->pCmdData = pCmdData;
        }
        break;


        default:
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand error unsupported index"));
            ErrMsgHandler = OMX_ErrorUnsupportedIndex;
        }
        break;
    }

    Queue(pMessageQueue, Message);
    RunIfNotReady();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : SendCommand OUT"));
    return ErrMsgHandler;
}


/** This is called by the OMX core in its message processing
 * thread context upon a component request. A request is made
 * by the component when some asynchronous services are needed:
 * 1) A SendCommand() is to be processed
 * 2) An error needs to be notified
 * \param Message, the message that has been passed to core
 */

OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentMessageHandler(CoreMessage* Message)
{

    OMX_COMPONENTTYPE* pHandle = &iOmxComponent;
    OMX_U32 ii;
    OMX_ERRORTYPE ErrorType = OMX_ErrorNone;


    /** Dealing with a SendCommand call.
     * -MessageParam1 contains the command to execute
     * -MessageParam2 contains the parameter of the command
     *  (destination state in case of a state change command).
     */

    OMX_STATETYPE orig_state = iState;
    if (SENDCOMMAND_MSG_TYPE == Message->MessageType)
    {
        switch (Message->MessageParam1)
        {
            case OMX_CommandStateSet:
            {
                /* Do the actual state change */
                ErrorType = AmrComponentDoStateSet(Message->MessageParam2);

                if (OMX_TRUE == iStateTransitionFlag)
                {
                    return OMX_ErrorNone;
                }

                //Do not send the callback now till the State gets changed
                if (ErrorType != OMX_ErrorNone)
                {
                    (*(ipCallbacks->EventHandler))
                    (pHandle,
                     iCallbackData,
                     OMX_EventError, /* The command was completed */
                     ErrorType, /* The commands was a OMX_CommandStateSet */
                     0, /* The iState has been changed in Message->MessageParam2 */
                     NULL);
                }
                else
                {
                    /* And run the callback */
                    (*(ipCallbacks->EventHandler))
                    (pHandle,
                     iCallbackData,
                     OMX_EventCmdComplete, /* The command was completed */
                     OMX_CommandStateSet, /* The commands was a OMX_CommandStateSet */
                     Message->MessageParam2, /* The iState has been changed in Message->MessageParam2 */
                     NULL);
                }
            }
            break;

            case OMX_CommandFlush:
            {
                /*Flush ports*/
                ErrorType = AmrComponentFlushPort(Message->MessageParam2);

                AmrComponentSetNumBufferFlush(iNumPorts, -1, 0);

                if (ErrorType != OMX_ErrorNone)
                {
                    (*(ipCallbacks->EventHandler))
                    (pHandle,
                     iCallbackData,
                     OMX_EventError, /* The command was completed */
                     ErrorType, /* The commands was a OMX_CommandStateSet */
                     0, /* The iState has been changed in Message->MessageParam2 */
                     NULL);
                }
                else
                {
                    if (-1 == Message->MessageParam2)
                    { /*Flush all port*/
                        for (ii = 0; ii < iNumPorts; ii++)
                        {
                            (*(ipCallbacks->EventHandler))
                            (pHandle,
                             iCallbackData,
                             OMX_EventCmdComplete, /* The command was completed */
                             OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
                             ii, /* The iState has been changed in Message->MessageParam2 */
                             NULL);
                        }
                    }
                    else
                    {/*Flush input/output port*/
                        (*(ipCallbacks->EventHandler))
                        (pHandle,
                         iCallbackData,
                         OMX_EventCmdComplete, /* The command was completed */
                         OMX_CommandFlush, /* The commands was a OMX_CommandStateSet */
                         Message->MessageParam2, /* The iState has been changed in Message->MessageParam2 */
                         NULL);
                    }
                }
                AmrComponentSetPortFlushFlag(iNumPorts, -1, OMX_FALSE);
            }
            break;

            case OMX_CommandPortDisable:
            {
                /** This condition is added to pass the tests, it is not significant for the environment */
                ErrorType = AmrComponentDisablePort(Message->MessageParam2);
                if (OMX_TRUE == iStateTransitionFlag)
                {
                    return OMX_ErrorNone;
                }

                if (ErrorType != OMX_ErrorNone)
                {
                    (*(ipCallbacks->EventHandler))
                    (pHandle,
                     iCallbackData,
                     OMX_EventError, /* The command was completed */
                     ErrorType, /* The commands was a OMX_CommandStateSet */
                     0, /* The iState has been changed in Message->MessageParam2 */
                     NULL);
                }
                else
                {
                    if (-1 == Message->MessageParam2)
                    { /*Disable all ports*/
                        for (ii = 0; ii < iNumPorts; ii++)
                        {
                            (*(ipCallbacks->EventHandler))
                            (pHandle,
                             iCallbackData,
                             OMX_EventCmdComplete, /* The command was completed */
                             OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
                             ii, /* The iState has been changed in Message->MessageParam2 */
                             NULL);
                        }
                    }
                    else
                    {
                        (*(ipCallbacks->EventHandler))
                        (pHandle,
                         iCallbackData,
                         OMX_EventCmdComplete, /* The command was completed */
                         OMX_CommandPortDisable, /* The commands was a OMX_CommandStateSet */
                         Message->MessageParam2, /* The iState has been changed in Message->MessageParam2 */
                         NULL);
                    }
                }
            }
            break;

            case OMX_CommandPortEnable:
            {
                ErrorType = AmrComponentEnablePort(Message->MessageParam2);
                if (OMX_TRUE == iStateTransitionFlag)
                {
                    return OMX_ErrorNone;
                }

                if (ErrorType != OMX_ErrorNone)
                {
                    (*(ipCallbacks->EventHandler))
                    (pHandle,
                     iCallbackData,
                     OMX_EventError, /* The command was completed */
                     ErrorType, /* The commands was a OMX_CommandStateSet */
                     0, /* The State has been changed in Message->MessageParam2 */
                     NULL);
                }
                else
                {
                    if (Message->MessageParam2 != -1)
                    {
                        (*(ipCallbacks->EventHandler))
                        (pHandle,
                         iCallbackData,
                         OMX_EventCmdComplete, /* The command was completed */
                         OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
                         Message->MessageParam2, /* The State has been changed in Message->MessageParam2 */
                         NULL);
                    }
                    else
                    {
                        for (ii = 0; ii < iNumPorts; ii++)
                        {
                            (*(ipCallbacks->EventHandler))
                            (pHandle,
                             iCallbackData,
                             OMX_EventCmdComplete, /* The command was completed */
                             OMX_CommandPortEnable, /* The commands was a OMX_CommandStateSet */
                             ii, /* The State has been changed in Message->MessageParam2 */
                             NULL);
                        }
                    }
                }
            }
            break;

            case OMX_CommandMarkBuffer:
            {
                ipMark = (OMX_MARKTYPE *)Message->pCmdData;
            }
            break;

            default:
            {

            }
            break;
        }
        /* Dealing with an asynchronous error condition
         */
    }

    if (orig_state != OMX_StateInvalid)
    {
        ErrorType = OMX_ErrorNone;
    }

    return ErrorType;
}

/** Changes the state of a component taking proper actions depending on
 * the transiotion requested
 * \param Component, the component which state is to be changed
 * \param aDestinationState the requested target state.
 */

OMX_ERRORTYPE OpenmaxAmrAO::AmrComponentDoStateSet(OMX_U32 aDestinationState)
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE,
                    (0, "OpenmaxAmrAO : AmrComponentDoStateSet IN : iState (%i) aDestinationState (%i)", iState, aDestinationState));

    OMX_ERRORTYPE ErrorType = OMX_ErrorNone;
    OMX_U32 ii;

    if (OMX_StateLoaded == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }

            case OMX_StateWaitForResources:
            {
                iState = OMX_StateLoaded;
            }
            break;

            case OMX_StateLoaded:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error same state"));
                return OMX_ErrorSameState;
            }
            break;

            case OMX_StateIdle:
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (PORT_IS_ENABLED(ipPorts[ii]) &&
                            PORT_IS_POPULATED(ipPorts[ii]))
                    {
                        if (OMX_FALSE == ipPorts[ii]->IdleToLoadedFlag)
                        {
                            iStateTransitionFlag = OMX_TRUE;
                        }

                        else
                        {
                            ipPorts[ii]->PortParam.bPopulated = OMX_FALSE;
                            ipPorts[ii]->TransientState = OMX_StateMax;
                        }
                    }
                }
                if (OMX_TRUE == iStateTransitionFlag)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet Waiting port to be de-populated"));
                    return OMX_ErrorNone;
                }

                iState = OMX_StateLoaded;

                iNumInputBuffer = 0;
                iOutBufferCount = 0;
                iPartialFrameAssembly = OMX_FALSE;
                iEndofStream = OMX_FALSE;
                iIsInputBufferEnded = OMX_TRUE;
                iNewOutBufRequired = OMX_TRUE;
                iFirstFragment = OMX_FALSE;
                ipAmrDec->iAmrInitFlag = 0;

                AmrComponentDeInit();
            }
            break;

            default:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error incorrect state"));
                return OMX_ErrorIncorrectStateTransition;
            }
        }
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
        return OMX_ErrorNone;
    }

    if (OMX_StateWaitForResources == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }
            break;

            case OMX_StateLoaded:
            {
                iState = OMX_StateWaitForResources;
            }
            break;

            case OMX_StateWaitForResources:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error same state"));
                return OMX_ErrorSameState;
            }
            break;

            default:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error incorrect state"));
                return OMX_ErrorIncorrectStateTransition;
            }
        }
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
        return OMX_ErrorNone;
    }

    if (OMX_StateIdle == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }
            break;

            case OMX_StateWaitForResources:
            {
                iState = OMX_StateIdle;
            }
            break;

            case OMX_StateLoaded:
            {
                for (ii = 0; ii < iNumPorts; ii++)
                {
                    if (PORT_IS_ENABLED(ipPorts[ii]) &&
                            !PORT_IS_POPULATED(ipPorts[ii]))
                    {
                        if (OMX_FALSE == ipPorts[ii]->LoadedToIdleFlag)
                        {
                            iStateTransitionFlag = OMX_TRUE;
                        }
                        else
                        {
                            ipPorts[ii]->PortParam.bPopulated = OMX_TRUE;
                            ipPorts[ii]->TransientState = OMX_StateMax;
                        }
                    }
                }
                if (OMX_TRUE == iStateTransitionFlag)
                {
                    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet Waiting port to be populated"));
                    return OMX_ErrorNone;
                }

                iState = OMX_StateIdle;

                //Used in case of partial frame assembly
                if (!ipInputCurrBuffer)
                {
                    //Keep the size of temp buffer double to be on safer side
                    ipInputCurrBuffer = (OMX_U8*) oscl_malloc(2 * sizeof(uint8) * (ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize));
                    if (NULL == ipInputCurrBuffer)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error insufficient resources"));
                        return OMX_ErrorInsufficientResources;
                    }
                }

                if (!ipTempInputBuffer)
                {
                    ipTempInputBuffer = (OMX_U8*) oscl_malloc(2 * sizeof(uint8) * ipPorts[OMX_PORT_INPUTPORT_INDEX]->PortParam.nBufferSize);
                    if (NULL == ipTempInputBuffer)
                    {
                        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error insufficient resources"));
                        return OMX_ErrorInsufficientResources;
                    }
                }

                iTempInputBufferFilledLength = 0;
                iTempConsumedLength = 0;
                iInputBufferRemainingBytes = 0;
            }
            break;

            case OMX_StateIdle:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error same state"));
                return OMX_ErrorSameState;
            }
            break;

            //Both the below cases have same body
            case OMX_StateExecuting:
            case OMX_StatePause:
            {
                AmrComponentSetNumBufferFlush(iNumPorts, -1, 0);
                AmrComponentSetPortFlushFlag(iNumPorts, -1, OMX_TRUE);

                AmrComponentPortType* pInPort = (AmrComponentPortType*) ipPorts[OMX_PORT_INPUTPORT_INDEX];

                //Return all the buffers if still occupied
                QueueType* pInputQueue = ipPorts[OMX_PORT_INPUTPORT_INDEX]->pBufferQueue;

                while ((iNumInputBuffer > 0) && (GetQueueNumElem(pInputQueue) > 0))
                {
                    AmrComponentFlushPort(OMX_PORT_INPUTPORT_INDEX);
                }
                // if a buffer was previously dequeued, it wasnt freed in above loop. return it now
                if (iNumInputBuffer > 0)
                {
                    ipAmrInputBuffer->nFilledLen = 0;
                    AmrComponentReturnInputBuffer(ipAmrInputBuffer, pInPort);
                    iIsInputBufferEnded = OMX_TRUE;
                }

                //Mark these flags as true
                iIsInputBufferEnded = OMX_TRUE;
                iEndofStream = OMX_FALSE;

                //Reset the decoder's input buffers/flags
                iTempInputBufferFilledLength = 0;
                iTempConsumedLength = 0;
                iInputBufferRemainingBytes = 0;

                //Assume for this state transition that reposition command has come
                iRepositionFlag = OMX_TRUE;
                //Reset the silence insertion logic also
                iSilenceInsertionInProgress = OMX_FALSE;
                // reset decoder
                if (ipAmrDec)
                {
                    ipAmrDec->ResetDecoder();
                }

                while (iOutBufferCount > 0)
                {
                    AmrComponentFlushPort(OMX_PORT_OUTPUTPORT_INDEX);
                }

                AmrComponentSetPortFlushFlag(iNumPorts, -1, OMX_FALSE);
                AmrComponentSetNumBufferFlush(iNumPorts, -1, 0);

                iState = OMX_StateIdle;
            }
            break;

            default:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error incorrect state"));
                return OMX_ErrorIncorrectStateTransition;
            }
            break;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
        return ErrorType;
    }

    if (OMX_StatePause == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }
            break;

            case OMX_StatePause:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error same state"));
                return OMX_ErrorSameState;
            }
            break;

            //Falling through to the next case
            case OMX_StateExecuting:
            case OMX_StateIdle:
            {
                iState = OMX_StatePause;
            }
            break;

            default:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error incorrect state"));
                return OMX_ErrorIncorrectStateTransition;
            }
            break;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
        return OMX_ErrorNone;
    }

    if (OMX_StateExecuting == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }
            break;

            case OMX_StateIdle:
            {
                iState = OMX_StateExecuting;
            }
            break;

            case OMX_StatePause:
            {
                iState = OMX_StateExecuting;
                /* A trigger to start the processing of buffers when component
                 * transitions to executing from pause, as it is already
                 * holding the required buffers
                 */
                RunIfNotReady();
            }
            break;

            case OMX_StateExecuting:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error same state"));
                return OMX_ErrorSameState;
            }
            break;

            default:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error incorrect state"));
                return OMX_ErrorIncorrectStateTransition;
            }
            break;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
        return OMX_ErrorNone;
    }

    if (OMX_StateInvalid == aDestinationState)
    {
        switch (iState)
        {
            case OMX_StateInvalid:
            {
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
                return OMX_ErrorInvalidState;
            }
            break;

            default:
            {
                iState = OMX_StateInvalid;
                if (iIsInit != OMX_FALSE)
                {
                    AmrComponentDeInit();
                }
            }
            break;
        }

        if (iIsInit != OMX_FALSE)
        {
            AmrComponentDeInit();
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet error invalid state"));
        return OMX_ErrorInvalidState;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : AmrComponentDoStateSet OUT"));
    return OMX_ErrorNone;
}



OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentEmptyThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{

    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE *)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->EmptyThisBuffer(hComponent, pBuffer);

    return Status;

}


OMX_ERRORTYPE OpenmaxAmrAO::EmptyThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)

{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer IN"));
    //Do not queue buffers if component is in invalid state
    if (OMX_StateInvalid == iState)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error invalid state"));
        return OMX_ErrorInvalidState;
    }

    if ((OMX_StateIdle == iState) || (OMX_StatePause == iState) || (OMX_StateExecuting == iState))
    {
        OMX_U32 PortIndex;
        QueueType* pInputQueue;
        OMX_ERRORTYPE ErrorType = OMX_ErrorNone;

        PortIndex = pBuffer->nInputPortIndex;

        //Validate the port index & Queue the buffers available only at the input port
        if (PortIndex >= iNumPorts ||
                ipPorts[PortIndex]->PortParam.eDir != OMX_DirInput)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error bad port index"));
            return OMX_ErrorBadPortIndex;
        }

        //Port should be in enabled state before accepting buffers
        if (!PORT_IS_ENABLED(ipPorts[PortIndex]))
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error incorrect state"));
            return OMX_ErrorIncorrectStateOperation;
        }

        /* The number of buffers the component can queue at a time
         * depends upon the number of buffers allocated/assigned on the input port
         */
        if (iNumInputBuffer == (ipPorts[PortIndex]->NumAssignedBuffers))

        {
            RunIfNotReady();
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error incorrect state"));
            return OMX_ErrorIncorrectStateOperation;
        }

        //Finally after passing all the conditions, queue the buffer in Input queue
        pInputQueue = ipPorts[PortIndex]->pBufferQueue;

        if ((ErrorType = CheckHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error check header failed"));
            return ErrorType;
        }

        iNumInputBuffer++;
        Queue(pInputQueue, pBuffer);

        //Signal the AO about the incoming buffer
        RunIfNotReady();
    }
    else
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer error incorrect state"));
        //This macro is not accepted in any other state except the three mentioned above
        return OMX_ErrorIncorrectStateOperation;
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : EmptyThisBuffer OUT"));

    return OMX_ErrorNone;
}


OMX_ERRORTYPE OpenmaxAmrAO::BaseComponentFillThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)
{

    OpenmaxAmrAO* pOpenmaxAOType = (OpenmaxAmrAO*)((OMX_COMPONENTTYPE*)hComponent)->pComponentPrivate;
    OMX_ERRORTYPE Status;

    if (NULL == pOpenmaxAOType)
    {
        return OMX_ErrorBadParameter;
    }

    Status = pOpenmaxAOType->FillThisBuffer(hComponent, pBuffer);

    return Status;
}

OMX_ERRORTYPE OpenmaxAmrAO::FillThisBuffer(
    OMX_IN  OMX_HANDLETYPE hComponent,
    OMX_IN  OMX_BUFFERHEADERTYPE* pBuffer)

{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer IN"));

    OMX_U32 PortIndex;

    QueueType* pOutputQueue;
    OMX_ERRORTYPE ErrorType = OMX_ErrorNone;

    PortIndex = pBuffer->nOutputPortIndex;
    //Validate the port index & Queue the buffers available only at the output port
    if (PortIndex >= iNumPorts ||
            ipPorts[PortIndex]->PortParam.eDir != OMX_DirOutput)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer error bad port index"));
        return OMX_ErrorBadPortIndex;
    }

    pOutputQueue = ipPorts[PortIndex]->pBufferQueue;
    if (iState != OMX_StateExecuting &&
            iState != OMX_StatePause &&
            iState != OMX_StateIdle)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer error invalid state"));
        return OMX_ErrorInvalidState;
    }

    //Port should be in enabled state before accepting buffers
    if (!PORT_IS_ENABLED(ipPorts[PortIndex]))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer error incorrect state"));
        return OMX_ErrorIncorrectStateOperation;
    }

    if ((ErrorType = CheckHeader(pBuffer, sizeof(OMX_BUFFERHEADERTYPE))) != OMX_ErrorNone)
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer error check header failed"));
        return ErrorType;
    }

    //Queue the buffer in output queue
    Queue(pOutputQueue, pBuffer);
    iOutBufferCount++;

    //Signal the AO about the incoming buffer
    RunIfNotReady();

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : FillThisBuffer OUT"));

    return OMX_ErrorNone;
}



void OpenmaxAmrAO::Run()
{
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : Run IN"));


    CoreMessage* pCoreMessage;

    //Execute the commands from the message handler queue
    if ((GetQueueNumElem(ipCoreDescriptor->pMessageQueue) > 0))
    {
        pCoreMessage = (CoreMessage*) DeQueue(ipCoreDescriptor->pMessageQueue);

        if (OMX_CommandStateSet == pCoreMessage->MessageParam1)
        {
            if (OMX_StateExecuting == pCoreMessage->MessageParam2)
            {
                iBufferExecuteFlag = OMX_TRUE;
            }
            else
            {
                iBufferExecuteFlag = OMX_FALSE;
            }
        }

        AmrComponentMessageHandler(pCoreMessage);

        /* If some allocations/deallocations are required before the state transition
         * then queue the command again to be executed later on
         */
        if (OMX_TRUE == iStateTransitionFlag)
        {
            Queue(ipCoreDescriptor->pMessageQueue, pCoreMessage);
            // Don't reschedule. Wait for arriving buffers to do it
            //RunIfNotReady();
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : Run OUT"));
            return;
        }

        else
        {
            oscl_free(pCoreMessage);
            pCoreMessage = NULL;
        }
    }

    /* If the component is in executing state, call the Buffer management function.
     * Stop calling this function as soon as state transition request is received.
     */
    if ((OMX_TRUE == iBufferExecuteFlag) && (OMX_TRUE != iResizePending))
    {
        AmrComponentBufferMgmtFunction();
    }

    //Check for any more commands in the message handler queue & schedule them for later
    if ((GetQueueNumElem(ipCoreDescriptor->pMessageQueue) > 0))
    {
        RunIfNotReady();
    }

    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : Run OUT"));

    return;
}

//Check whether silence insertion is required here or not
void OpenmaxAmrAO::CheckForSilenceInsertion()
{
    OMX_TICKS TimestampGap;
    //Set the flag to false by default
    iSilenceInsertionInProgress = OMX_FALSE;

    TimestampGap = iFrameTimestamp - iCurrentTimestamp;

    if ((TimestampGap > OMX_HALFRANGE_THRESHOLD) || (TimestampGap < OMX_AMR_DEC_FRAME_INTERVAL && iFrameCount > 0))
    {
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAacAO : CheckForSilenceInsertion OUT - No need to insert silence"));
        return;
    }

    //Silence insertion needed, mark the flag to true
    if (iFrameCount > 0)
    {
        iSilenceInsertionInProgress = OMX_TRUE;
        //Determine the number of silence frames to insert
        iSilenceFramesNeeded = TimestampGap / OMX_AMR_DEC_FRAME_INTERVAL;
        iZeroFramesNeeded = 0;
        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : CheckForSilenceInsertion OUT - Silence Insertion required here"));
    }

    return;
}

//Perform the silence insertion
void OpenmaxAmrAO::DoSilenceInsertion()
{
    QueueType* pOutputQueue = ipPorts[OMX_PORT_OUTPUTPORT_INDEX]->pBufferQueue;
    AmrComponentPortType* pOutPort = ipPorts[OMX_PORT_OUTPUTPORT_INDEX];

    OMX_U8*	pOutBuffer;
    OMX_U32	OutputLength;
    OMX_BOOL DecodeReturn;


    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion IN"));

    while (iSilenceFramesNeeded > 0)
    {
        //Check whether prev output bufer has been consumed or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not
            if (0 == (GetQueueNumElem(pOutputQueue)))
            {
                //Resume Silence insertion next time when component will be called
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion OUT output buffer unavailable"));
                iSilenceInsertionInProgress = OMX_TRUE;
                return;
            }

            ipAmrOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
            ipAmrOutputBuffer->nFilledLen = 0;
            iNewOutBufRequired = OMX_FALSE;

            //Set the current timestamp to the output buffer timestamp
            ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
        }

        pOutBuffer = &ipAmrOutputBuffer->pBuffer[ipAmrOutputBuffer->nFilledLen];
        OutputLength = 0;

        //Decode the silence frame
        DecodeReturn = ipAmrDec->AmrDecodeSilenceFrame((OMX_S16*) pOutBuffer,
                       (OMX_U32*) & OutputLength);


        if (OMX_FALSE == DecodeReturn)
        {
            PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion - Decode error of silence generation, Insert zero frames instead"));
            iZeroFramesNeeded = iSilenceFramesNeeded;
            iSilenceFramesNeeded = 0;
            break;
        }

        //Output length for a buffer of OMX_U8* will be double as that of OMX_S16*
        ipAmrOutputBuffer->nFilledLen += OutputLength;
        //offset not required in our case, set it to zero
        ipAmrOutputBuffer->nOffset = 0;

        if (OutputLength > 0)
        {
            iCurrentTimestamp += OMX_AMR_DEC_FRAME_INTERVAL;
        }

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion - silence frame decoded"));

        //Send the output buffer back when it has become full
        if ((ipAmrOutputBuffer->nAllocLen - ipAmrOutputBuffer->nFilledLen) < iOutputFrameLength)
        {
            AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);
        }

        // Decrement the silence frame counter
        --iSilenceFramesNeeded;
    }

    // THE ZERO FRAME INSERTION IS PERFORMED ONLY IF SILENCE INSERTION FAILS
    while (iZeroFramesNeeded > 0)
    {
        //Check whether prev output bufer has been consumed or not
        if (OMX_TRUE == iNewOutBufRequired)
        {
            //Check whether a new output buffer is available or not
            if (0 == (GetQueueNumElem(pOutputQueue)))
            {
                //Resume Silence insertion next time when component will be called
                PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion OUT output buffer unavailable"));
                iSilenceInsertionInProgress = OMX_TRUE;
                return;
            }

            ipAmrOutputBuffer = (OMX_BUFFERHEADERTYPE*) DeQueue(pOutputQueue);
            ipAmrOutputBuffer->nFilledLen = 0;
            iNewOutBufRequired = OMX_FALSE;

            //Set the current timestamp to the output buffer timestamp
            ipAmrOutputBuffer->nTimeStamp = iCurrentTimestamp;
        }

        pOutBuffer = &ipAmrOutputBuffer->pBuffer[ipAmrOutputBuffer->nFilledLen];
        oscl_memset(pOutBuffer, 0, iOutputFrameLength);

        ipAmrOutputBuffer->nFilledLen += iOutputFrameLength;
        ipAmrOutputBuffer->nOffset = 0;
        iCurrentTimestamp += OMX_AMR_DEC_FRAME_INTERVAL;

        PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion - One frame of zeros inserted"));

        //Send the output buffer back when it has become full
        if ((ipAmrOutputBuffer->nAllocLen - ipAmrOutputBuffer->nFilledLen) < iOutputFrameLength)
        {
            AmrComponentReturnOutputBuffer(ipAmrOutputBuffer, pOutPort);
        }

        // Decrement the silence frame counter
        --iZeroFramesNeeded;
    }

    /* Completed Silence insertion successfully, now consider the input buffer already dequeued
     * for decoding & update the timestamps */

    iSilenceInsertionInProgress = OMX_FALSE;
    iCurrentTimestamp = iFrameTimestamp;
    PVLOGGER_LOGMSG(PVLOGMSG_INST_HLDBG, iLogger, PVLOGMSG_NOTICE, (0, "OpenmaxAmrAO : DoSilenceInsertion OUT - Done successfully"));

    return;
}
