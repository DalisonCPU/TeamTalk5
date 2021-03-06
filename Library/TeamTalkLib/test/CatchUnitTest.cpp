/*
 * Copyright (c) 2005-2018, BearWare.dk
 *
 * Contact Information:
 *
 * Bjoern D. Rasmussen
 * Kirketoften 5
 * DK-8260 Viby J
 * Denmark
 * Email: contact@bearware.dk
 * Phone: +45 20 20 54 59
 * Web: http://www.bearware.dk
 *
 * This source code is part of the TeamTalk SDK owned by
 * BearWare.dk. Use of this file, or its compiled unit, requires a
 * TeamTalk SDK License Key issued by BearWare.dk.
 *
 * The TeamTalk SDK License Agreement along with its Terms and
 * Conditions are outlined in the file License.txt included with the
 * TeamTalk SDK distribution.
 *
 */

#include "catch.hpp"

#include <ace/ACE.h>
#include <ace/OS.h>

#include "TTUnitTest.h"

#include <myace/MyACE.h>
#include <teamtalk/server/ServerNode.h>

#include <map>
#include <map>
#include <iostream>
#include <future>

#if defined(ENABLE_OGG)
#include <codec/OggOutput.h>
#endif

#if defined(ENABLE_OPUS)
#include <codec/OpusEncoder.h>
#endif
#include <codec/WaveFile.h>

#if defined(ENABLE_FFMPEG3)
#include <avstream/FFMpeg3Streamer.h>
#endif

#if defined (ENABLE_PORTAUDIO)
#include <avstream/PortAudioWrapper.h>
#endif

#if defined(WIN32)
#include <ace/Init_ACE.h>
#include <assert.h>
#include <Mmsystem.h>
#include <propsys.h>
#include <atlbase.h>
#include <MMDeviceApi.h>
#include <avstream/DMOResampler.h>
#include <avstream/PortAudioWrapper.h>
#include <mfapi.h>

static class WinInit
{
public:
    WinInit()
    {
        int ret = ACE::init();
        assert(ret >= 0);
#if defined(ENABLE_MEDIAFOUNDATION)
        HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
        assert(SUCCEEDED(hr));
#endif
    }
    ~WinInit()
    {
        int ret = ACE::fini();
        assert(ret >= 0);
    }
} wininit;
#endif

TEST_CASE( "Init TT", "" ) {
    TTInstance* ttinst;
    REQUIRE( (ttinst = TT_InitTeamTalkPoll()) );
    REQUIRE( TT_CloseTeamTalk(ttinst) );
}

#if defined(ENABLE_OGG) && defined(ENABLE_SPEEX)
TEST_CASE( "Ogg Write", "" ) {
    SpeexEncFile spxfile;
    REQUIRE( spxfile.Open(ACE_TEXT("/foo.spx"), 1, DEFAULT_SPEEX_COMPLEXITY, 7, 32000, 48000, false) == false);
}
#endif

#if defined(ENABLE_OPUS)
TEST_CASE( "Record mux") {
    std::vector<ttinst> clients(2);
    for (size_t i=0;i<clients.size();++i)
    {
        REQUIRE((clients[i] = TT_InitTeamTalkPoll()));
        REQUIRE(InitSound(clients[i], SHARED_INPUT));
        REQUIRE(Connect(clients[i], ACE_TEXT("127.0.0.1"), 10333, 10333));
        REQUIRE(Login(clients[i], ACE_TEXT("MyNickname"), ACE_TEXT("guest"), ACE_TEXT("guest")));

        if (i == 0)
        {
            AudioCodec audiocodec = {};
            audiocodec.nCodec = OPUS_CODEC;
            audiocodec.opus.nApplication = OPUS_APPLICATION_VOIP;
            audiocodec.opus.nTxIntervalMSec = 240;
#if defined(OPUS_FRAMESIZE_120_MS)
            audiocodec.opus.nFrameSizeMSec = 120;
#else
            audiocodec.opus.nFrameSizeMSec = 40;
#endif
            audiocodec.opus.nBitRate = OPUS_MIN_BITRATE;
            audiocodec.opus.nChannels = 2;
            audiocodec.opus.nComplexity = 10;
            audiocodec.opus.nSampleRate= 48000;
            audiocodec.opus.bDTX = true;
            audiocodec.opus.bFEC = true;
            audiocodec.opus.bVBR = false;
            audiocodec.opus.bVBRConstraint = false;

            Channel chan = MakeChannel(clients[i], ACE_TEXT("foo"), TT_GetRootChannelID(clients[i]), audiocodec);
            REQUIRE(WaitForCmdSuccess(clients[i], TT_DoJoinChannel(clients[i], &chan)));
        }
        else
        {
            REQUIRE(WaitForCmdSuccess(clients[i], TT_DoJoinChannelByID(clients[i], TT_GetMyChannelID(clients[0]), ACE_TEXT(""))));
        }
    }

    Channel chan;
    REQUIRE(TT_GetChannel(clients[1], TT_GetMyChannelID(clients[1]), &chan));

    REQUIRE(TT_EnableVoiceTransmission(clients[0], true));
    WaitForEvent(clients[0], CLIENTEVENT_NONE, 100);
    REQUIRE(TT_EnableVoiceTransmission(clients[0], false));

    REQUIRE(TT_StartRecordingMuxedAudioFile(clients[1], &chan.audiocodec, ACE_TEXT("MyMuxFile.wav"), AFF_WAVE_FORMAT));

    REQUIRE(TT_DBG_SetSoundInputTone(clients[0], STREAMTYPE_VOICE, 500));
    REQUIRE(TT_EnableVoiceTransmission(clients[0], true));
    WaitForEvent(clients[0], CLIENTEVENT_NONE, 2500);
    REQUIRE(TT_EnableVoiceTransmission(clients[0], false));

    REQUIRE(TT_DBG_SetSoundInputTone(clients[1], STREAMTYPE_VOICE, 600));
    REQUIRE(TT_EnableVoiceTransmission(clients[1], true));
    WaitForEvent(clients[1], CLIENTEVENT_NONE, 2500);
    REQUIRE(TT_EnableVoiceTransmission(clients[1], false));

    WaitForEvent(clients[1], CLIENTEVENT_NONE, 10000);

    REQUIRE(TT_StopRecordingMuxedAudioFile(clients[1]));
}
#endif

#if defined(ENABLE_OPUS)
TEST_CASE( "Last voice packet" )
{
    std::vector<ttinst> clients;
    auto txclient = TT_InitTeamTalkPoll();
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(txclient);
    clients.push_back(rxclient);

    REQUIRE(InitSound(txclient, SHARED_INPUT));
    REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));

    REQUIRE(InitSound(rxclient, SHARED_INPUT));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));

    AudioCodec audiocodec = {};
    audiocodec.nCodec = OPUS_CODEC;
    audiocodec.opus.nApplication = OPUS_APPLICATION_VOIP;
    audiocodec.opus.nTxIntervalMSec = 240;
#if defined(OPUS_FRAMESIZE_120_MS)
    audiocodec.opus.nFrameSizeMSec = 120;
#else
    audiocodec.opus.nFrameSizeMSec = 60;
#endif
    audiocodec.opus.nBitRate = OPUS_MIN_BITRATE;
    audiocodec.opus.nChannels = 2;
    audiocodec.opus.nComplexity = 10;
    audiocodec.opus.nSampleRate= 48000;
    audiocodec.opus.bDTX = true;
    audiocodec.opus.bFEC = true;
    audiocodec.opus.bVBR = false;
    audiocodec.opus.bVBRConstraint = false;

    Channel chan = MakeChannel(txclient, ACE_TEXT("foo"), TT_GetRootChannelID(txclient), audiocodec);
    REQUIRE(WaitForCmdSuccess(txclient, TT_DoJoinChannel(txclient, &chan)));

    REQUIRE(WaitForCmdSuccess(rxclient, TT_DoJoinChannelByID(rxclient, TT_GetMyChannelID(txclient), ACE_TEXT(""))));

    REQUIRE(TT_DBG_SetSoundInputTone(txclient, STREAMTYPE_VOICE, 600));

    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, int(audiocodec.opus.nTxIntervalMSec * 5 + audiocodec.opus.nTxIntervalMSec * .5));
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));

    auto voicestop = [&](TTMessage msg)
    {
        if (msg.nClientEvent == CLIENTEVENT_USER_STATECHANGE &&
            msg.user.nUserID == TT_GetMyUserID(txclient) &&
            (msg.user.uUserState & USERSTATE_VOICE) == 0)
        {
            return true;
        }

        return false;
    };

    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_STATECHANGE, voicestop));
    //WaitForEvent(txclient, CLIENTEVENT_NONE, nullptr, audiocodec.opus.nTxIntervalMSec * 2);

    TTCHAR curdir[1024] = {};
    ACE_OS::getcwd(curdir, 1024);
    REQUIRE(TT_SetUserMediaStorageDir(rxclient, TT_GetMyUserID(txclient), curdir, ACE_TEXT(""), AFF_WAVE_FORMAT));

    REQUIRE(TT_DBG_SetSoundInputTone(txclient, STREAMTYPE_VOICE, 0));
    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 1000);
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));
}
#endif

TEST_CASE( "MuxedAudioToFile" )
{
    std::vector<ttinst> clients;
    auto txclient = TT_InitTeamTalkPoll();
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(txclient);
    clients.push_back(rxclient);

    REQUIRE(InitSound(txclient));
    REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(txclient));

    REQUIRE(InitSound(rxclient));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(rxclient));

    Channel chan;
    REQUIRE(TT_GetChannel(rxclient, TT_GetMyChannelID(rxclient), &chan));
    REQUIRE(TT_StartRecordingMuxedAudioFile(rxclient, &chan.audiocodec, ACE_TEXT("MyMuxFile.wav"), AFF_WAVE_FORMAT));

    REQUIRE(TT_DBG_SetSoundInputTone(txclient, STREAMTYPE_VOICE, 500));
    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));

    // This tone is not being stored in 'MyMuxFile.wav' because the
    // audio block will bypass the audio encoder.
    REQUIRE(TT_DBG_SetSoundInputTone(rxclient, STREAMTYPE_VOICE, 600));
    REQUIRE(TT_EnableVoiceTransmission(rxclient, true));
    WaitForEvent(rxclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(TT_EnableVoiceTransmission(rxclient, false));

    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));

    REQUIRE(TT_EnableVoiceTransmission(rxclient, true));
    WaitForEvent(rxclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(TT_EnableVoiceTransmission(rxclient, false));

    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(WaitForCmdSuccess(rxclient, TT_DoUnsubscribe(rxclient, TT_GetMyUserID(txclient), SUBSCRIBE_VOICE)));

    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);

    REQUIRE(WaitForCmdSuccess(rxclient, TT_DoSubscribe(rxclient, TT_GetMyUserID(txclient), SUBSCRIBE_VOICE)));

    REQUIRE(TT_EnableVoiceTransmission(rxclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);

    REQUIRE(TT_CloseSoundInputDevice(rxclient));
    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    WaitForEvent(txclient, CLIENTEVENT_NONE, 2000);
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));

    REQUIRE(TT_StopRecordingMuxedAudioFile(rxclient));
}

TEST_CASE( "MuxedAudioBlock" )
{
    std::vector<ttinst> clients;
    auto txclient = TT_InitTeamTalkPoll();
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(txclient);
    clients.push_back(rxclient);

    REQUIRE(InitSound(txclient));
    REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(txclient));

    REQUIRE(InitSound(rxclient));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(rxclient));

    REQUIRE(TT_EnableAudioBlockEvent(rxclient, TT_MUXED_USERID, STREAMTYPE_VOICE, TRUE));

    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_AUDIOBLOCK));
}

TEST_CASE( "MuxedAudioBlockUserEvent" )
{
    std::vector<ttinst> clients;
    auto txclient = TT_InitTeamTalkPoll();
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(txclient);
    clients.push_back(rxclient);

    REQUIRE(InitSound(txclient));
    REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(txclient));

    REQUIRE(InitSound(rxclient, DEFAULT, SOUNDDEVICEID_IGNORE, SOUNDDEVICEID_DEFAULT));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(rxclient));

    REQUIRE(TT_EnableAudioBlockEvent(rxclient, TT_MUXED_USERID, STREAMTYPE_VOICE, TRUE));
    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_AUDIOBLOCK));
    REQUIRE(TT_EnableVoiceTransmission(txclient, true));
    auto voicestart = [&](TTMessage msg)
    {
        if (msg.nClientEvent == CLIENTEVENT_USER_STATECHANGE &&
            msg.user.nUserID == TT_GetMyUserID(txclient) &&
            (msg.user.uUserState & USERSTATE_VOICE) == USERSTATE_VOICE)
        {
            return true;
        }

        return false;
    };
    auto voicestop = [&](TTMessage msg)
    {
        if (msg.nClientEvent == CLIENTEVENT_USER_STATECHANGE &&
            msg.user.nUserID == TT_GetMyUserID(txclient) &&
            (msg.user.uUserState & USERSTATE_VOICE) == USERSTATE_NONE)
        {
            return true;
        }

        return false;
    };

    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_STATECHANGE, voicestart));
    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_AUDIOBLOCK));
    REQUIRE(TT_EnableVoiceTransmission(txclient, false));
    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_STATECHANGE, voicestop));
    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_AUDIOBLOCK));
}

#if defined(ENABLE_OGG)
TEST_CASE( "Opus Read File" )
{
    std::vector<ttinst> clients;
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(rxclient);

    REQUIRE(InitSound(rxclient));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(rxclient));
    AudioCodec codec;
#if defined(ENABLE_OPUSTOOLS) && 0
    codec.nCodec = OPUS_CODEC;
    codec.opus = {};
    codec.opus.nApplication = OPUS_APPLICATION_AUDIO;
    codec.opus.nBitRate = 64000;
    codec.opus.nChannels = 2;
    codec.opus.nComplexity = 9;
    codec.opus.nFrameSizeMSec = 5;
    codec.opus.nSampleRate = 24000;
    codec.opus.nTxIntervalMSec = 400;
    Channel chan = MakeChannel(rxclient, ACE_TEXT("opustools"), TT_GetMyChannelID(rxclient), codec);
    REQUIRE(WaitForCmdSuccess(rxclient, TT_DoJoinChannel(rxclient, &chan)));
#else
    codec.nCodec = SPEEX_VBR_CODEC;
    codec.speex_vbr = {};
    codec.speex_vbr.nBandmode = 1;
    codec.speex_vbr.nBitRate = 16000;
    codec.speex_vbr.nMaxBitRate = 32000;
    codec.speex_vbr.nQuality = 5;
    codec.speex_vbr.nTxIntervalMSec = 400;
    Channel chan = MakeChannel(rxclient, ACE_TEXT("speex"), TT_GetMyChannelID(rxclient), codec);
    REQUIRE(WaitForCmdSuccess(rxclient, TT_DoJoinChannel(rxclient, &chan)));
#endif
    const TTCHAR FILENAME[] = ACE_TEXT("MyMuxFile.ogg");
    REQUIRE(TT_GetChannel(rxclient, TT_GetMyChannelID(rxclient), &chan));
    REQUIRE(TT_StartRecordingMuxedAudioFile(rxclient, &chan.audiocodec, FILENAME, AFF_CHANNELCODEC_FORMAT));

    WaitForEvent(rxclient, CLIENTEVENT_NONE, 2000);

    OggFile of;
    REQUIRE(of.Open(FILENAME));
    ogg_page op;
    int pages = 0;
    REQUIRE(of.ReadOggPage(op));
    REQUIRE(op.header_len>0);
    REQUIRE(op.body_len>0);
    pages++;
    while (of.ReadOggPage(op))pages++;
}
#endif

#if defined(ENABLE_ENCRYPTION) && 0 /* doesn't work on GitHub */
TEST_CASE("TestHTTPS")
{
    //ACE::HTTPS::Context::set_default_ssl_mode(ACE_SSL_Context::SSLv23);
    //ACE::HTTPS::Context::set_default_verify_mode(true);
    //ACE::HTTPS::Context::instance().use_default_ca();
    //ACE::INet::SSL_CallbackManager::instance()->set_certificate_callback(new ACE::INet::SSL_CertificateAcceptor);

    std::string response1, response2, response3;
    REQUIRE(1 == HttpRequest("http://www.bearware.dk/teamtalk/weblogin.php?ping=1", response1));
    REQUIRE(1 == HttpRequest("https://www.bearware.dk/teamtalk/weblogin.php?ping=1", response2));
    REQUIRE(response1 == response2);
    REQUIRE(1 == HttpRequest("https://www.google.com", response3));
}
#endif

#if defined(WIN32)

TEST_CASE("CLSID_CWMAudioAEC")
{
    std::vector<SoundDevice> devs(100);
    INT32 nDevs = 100, indev, outdev;
    REQUIRE(TT_GetSoundDevices(&devs[0], &nDevs));
    nDevs = nDevs;
    REQUIRE(TT_GetDefaultSoundDevicesEx(SOUNDSYSTEM_WASAPI, &indev, &outdev));

    CComPtr<IMMDeviceEnumerator> spEnumerator;
    CComPtr<IMMDeviceCollection> spEndpoints;
    UINT dwCount;

    REQUIRE(SUCCEEDED(spEnumerator.CoCreateInstance(__uuidof(MMDeviceEnumerator))));

    std::map<std::wstring, UINT> capdevs, spkdevs;

    REQUIRE(SUCCEEDED(spEnumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &spEndpoints)));
    REQUIRE(SUCCEEDED(spEndpoints->GetCount(&dwCount)));
    for (UINT index = 0; index < dwCount; index++)
    {
        WCHAR* pszDeviceId = NULL;
        PROPVARIANT value;
        CComPtr<IMMDevice> spDevice;
        CComPtr<IPropertyStore> spProperties;

        PropVariantInit(&value);
        REQUIRE(SUCCEEDED(spEndpoints->Item(index, &spDevice)));
        REQUIRE(SUCCEEDED(spDevice->GetId(&pszDeviceId)));

        capdevs[pszDeviceId] = index;

        PropVariantClear(&value);
        CoTaskMemFree(pszDeviceId);
    }
    spEndpoints.Release();

    REQUIRE(SUCCEEDED(spEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &spEndpoints)));
    REQUIRE(SUCCEEDED(spEndpoints->GetCount(&dwCount)));
    for(UINT index = 0; index < dwCount; index++)
    {
        WCHAR* pszDeviceId = NULL;
        PROPVARIANT value;
        CComPtr<IMMDevice> spDevice;
        CComPtr<IPropertyStore> spProperties;

        PropVariantInit(&value);
        REQUIRE(SUCCEEDED(spEndpoints->Item(index, &spDevice)));
        REQUIRE(SUCCEEDED(spDevice->GetId(&pszDeviceId)));

        spkdevs[pszDeviceId] = index;

        PropVariantClear(&value);
        CoTaskMemFree(pszDeviceId);
    }

    CComPtr<IMediaObject> pDMO;
    CComPtr<IPropertyStore> pPS;

    REQUIRE(SUCCEEDED(CoCreateInstance(CLSID_CWMAudioAEC, NULL, CLSCTX_INPROC_SERVER, IID_IMediaObject, (LPVOID*)&pDMO)));
    REQUIRE(SUCCEEDED(pDMO->QueryInterface(IID_IPropertyStore, (LPVOID*)&pPS)));

    PROPVARIANT pvSysMode;
    PropVariantInit(&pvSysMode);
    pvSysMode.vt = VT_I4;
    pvSysMode.lVal = SINGLE_CHANNEL_AEC;
    REQUIRE(SUCCEEDED(pPS->SetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, pvSysMode)));
    REQUIRE(SUCCEEDED(pPS->GetValue(MFPKEY_WMAAECMA_SYSTEM_MODE, &pvSysMode)));
    PropVariantClear(&pvSysMode);

    REQUIRE(capdevs.find(devs[indev].szDeviceID) != capdevs.end());
    REQUIRE(spkdevs.find(devs[outdev].szDeviceID) != spkdevs.end());

    indev = capdevs[devs[indev].szDeviceID];
    outdev = spkdevs[devs[outdev].szDeviceID];

    PROPVARIANT pvDeviceId;
    PropVariantInit(&pvDeviceId);
    pvDeviceId.vt = VT_I4;
    pvDeviceId.lVal = (outdev << 16) | indev;
    REQUIRE(SUCCEEDED(pPS->SetValue(MFPKEY_WMAAECMA_DEVICE_INDEXES, pvDeviceId)));
    REQUIRE(SUCCEEDED(pPS->GetValue(MFPKEY_WMAAECMA_DEVICE_INDEXES, &pvDeviceId)));
    PropVariantClear(&pvDeviceId);

    // Turn on feature modes
    PROPVARIANT pvFeatrModeOn;
    PropVariantInit(&pvFeatrModeOn);
    pvFeatrModeOn.vt = VT_BOOL;
    pvFeatrModeOn.boolVal = VARIANT_TRUE;
    REQUIRE(SUCCEEDED(pPS->SetValue(MFPKEY_WMAAECMA_FEATURE_MODE, pvFeatrModeOn)));
    REQUIRE(SUCCEEDED(pPS->GetValue(MFPKEY_WMAAECMA_FEATURE_MODE, &pvFeatrModeOn)));
    PropVariantClear(&pvFeatrModeOn);

    const int SAMPLERATE = 22050;
    const int CHANNELS = 1;

    DMO_MEDIA_TYPE mt = {};
    HRESULT hr = MoInitMediaType(&mt, sizeof(WAVEFORMATEX));
    REQUIRE(SUCCEEDED(hr));
    REQUIRE(SetWaveMediaType(SAMPLEFORMAT_INT16, CHANNELS, SAMPLERATE, mt));

    REQUIRE(SUCCEEDED(pDMO->SetOutputType(0, &mt, 0)));
    REQUIRE(SUCCEEDED(MoFreeMediaType(&mt)));

    REQUIRE(SUCCEEDED(pDMO->AllocateStreamingResources()));

    int iFrameSize;
    PROPVARIANT pvFrameSize;
    PropVariantInit(&pvFrameSize);
    REQUIRE(SUCCEEDED(pPS->GetValue(MFPKEY_WMAAECMA_FEATR_FRAME_SIZE, &pvFrameSize)));
    iFrameSize = pvFrameSize.lVal;
    PropVariantClear(&pvFrameSize);

    WavePCMFile wavefile;
    REQUIRE(wavefile.NewFile(ACE_TEXT("Echo_cancelled.wav"), SAMPLERATE, CHANNELS));

    int delay = iFrameSize * 1000 / SAMPLERATE;
    int waitMSec = 10000;
    std::vector<BYTE> outputbuf(PCM16_BYTES(SAMPLERATE, CHANNELS));
    do
    {
        CComPtr<IMediaBuffer> ioutputbuf;
        REQUIRE(SUCCEEDED(CMediaBuffer::CreateBuffer(&outputbuf[0], 0, outputbuf.size(), (void**)&ioutputbuf)));
        DMO_OUTPUT_DATA_BUFFER dmodatabuf = {};
        dmodatabuf.pBuffer = ioutputbuf;
        DWORD dwStatus, dwOutputLen = 0;
        hr = pDMO->ProcessOutput(0, 1, &dmodatabuf, &dwStatus);

        BYTE* outputbufptr;
        switch (hr)
        {
            case S_FALSE :
                dwOutputLen = 0;
                break;
            case S_OK :
                hr = ioutputbuf->GetBufferAndLength(&outputbufptr, &dwOutputLen);
                REQUIRE(SUCCEEDED(hr));
                REQUIRE(wavefile.AppendSamples(reinterpret_cast<const short*>(outputbufptr), dwOutputLen / sizeof(short) / CHANNELS));
                break;
            case E_FAIL :
                REQUIRE(SUCCEEDED(hr));
                break;
            case E_INVALIDARG :
                REQUIRE(SUCCEEDED(hr));
                break;
            case E_POINTER :
                REQUIRE(SUCCEEDED(hr));
                break;
            case WMAAECMA_E_NO_ACTIVE_RENDER_STREAM :
                MYTRACE(ACE_TEXT("No audio rendered on device: %s\n"), devs[outdev].szDeviceID);
                REQUIRE(SUCCEEDED(hr));
                break;
            default :
                MYTRACE(ACE_TEXT("Unknown HRESULT from echo cancellor 0x%x\n"), hr);
                REQUIRE(SUCCEEDED(hr));
                break;
        }

        Sleep(delay);

    } while ((waitMSec -= delay) > 0);
}

TEST_CASE("CWMAudioAEC_Callback")
{
    using namespace soundsystem;
    class MyClass : public StreamDuplex
    {
    public:
        int callbacks = 0;

        void StreamDuplexCb(const DuplexStreamer& streamer,
                            const short* input_buffer,
                            short* output_buffer, int samples) override
        {
            MYTRACE(ACE_TEXT("Callback of %d samples\n"), samples);
            callbacks++;
        }

        soundsystem::SoundDeviceFeatures GetDuplexFeatures() override
        {
            return soundsystem::SOUNDDEVICEFEATURE_NONE;
        }

    } myduplex, myduplex2;

    media::AudioFormat fmt(48000, 1);
    auto sndsys = soundsystem::GetInstance();
    int sndgrpid = sndsys->OpenSoundGroup();
    int indev, outdev;
    REQUIRE(sndsys->GetDefaultDevices(SOUND_API_WASAPI, indev, outdev));
    // test when framesize > CWMAudioAEC framesize
    {
        PaDuplexStreamer paduplex(&myduplex, sndgrpid, fmt.samplerate * .1, fmt.samplerate, fmt.channels, fmt.channels, SOUND_API_WASAPI, indev, outdev);
        paduplex.winaec.reset(new CWMAudioAECCapture(&paduplex, soundsystem::SOUNDDEVICEFEATURE_AEC));
        REQUIRE(paduplex.winaec->Open());
        while (myduplex.callbacks <= 10)
        {
            Sleep(PCM16_SAMPLES_DURATION(paduplex.framesize, fmt.samplerate));
            short* recorded = paduplex.winaec->AcquireBuffer();
            if (recorded)
            {
                std::vector<short> playback(paduplex.framesize * paduplex.output_channels);
                paduplex.duplex->StreamDuplexCb(paduplex, recorded, &playback[0], paduplex.framesize);
                paduplex.winaec->ReleaseBuffer();
            }
        }
    }

    // test when framesize < CWMAudioAEC framesize
    {
        fmt = media::AudioFormat(32000, 2);
        PaDuplexStreamer paduplex2(&myduplex2, sndgrpid, fmt.samplerate * .005, fmt.samplerate, fmt.channels, fmt.channels, SOUND_API_WASAPI, indev, outdev);
        paduplex2.winaec.reset(new CWMAudioAECCapture(&paduplex2, soundsystem::SOUNDDEVICEFEATURE_AEC));
        REQUIRE(paduplex2.winaec->Open());
        while(myduplex2.callbacks <= 200)
        {
            Sleep(PCM16_SAMPLES_DURATION(paduplex2.framesize, fmt.samplerate));
            short* recorded = paduplex2.winaec->AcquireBuffer();
            if(recorded)
            {
                std::vector<short> playback(paduplex2.framesize * paduplex2.output_channels);
                paduplex2.duplex->StreamDuplexCb(paduplex2, recorded, &playback[0], paduplex2.framesize);
                paduplex2.winaec->ReleaseBuffer();
            }
        }
        // Ensure queue is used instead of direct callback
        Sleep(1000);

        // Ensure we can resume
        myduplex2.callbacks = 0;
        while(myduplex2.callbacks <= 200)
        {
            Sleep(PCM16_SAMPLES_DURATION(paduplex2.framesize, fmt.samplerate));
            short* recorded = paduplex2.winaec->AcquireBuffer();
            if(recorded)
            {
                std::vector<short> playback(paduplex2.framesize * paduplex2.output_channels);
                paduplex2.duplex->StreamDuplexCb(paduplex2, recorded, &playback[0], paduplex2.framesize);
                paduplex2.winaec->ReleaseBuffer();
            }
        }

    }

    sndsys->RemoveSoundGroup(sndgrpid);
}

#define ECHOPLAYBACKFILENAME ACE_TEXT("playfile.wav")
TEST_CASE("CWMAudioAEC_DuplexMode")
{
    using namespace soundsystem;

    {
        // Ensure wave file exists before running unit-test (otherwise SoundSystemBase destructor will complain with abort)
        WavePCMFile ww;
        REQUIRE(ww.OpenFile(ECHOPLAYBACKFILENAME, true));
    }

    auto sndsys = soundsystem::GetInstance();
    int sndgrpid = sndsys->OpenSoundGroup();
    int indev, outdev;
    REQUIRE(sndsys->GetDefaultDevices(SOUND_API_WASAPI, indev, outdev));
    INT32 n_devs = 25;
    std::vector<SoundDevice> devs(n_devs);
    REQUIRE(TT_GetSoundDevices(&devs[0], &n_devs));
    auto idev = std::find_if(devs.begin(), devs.end(), [outdev] (SoundDevice d) { return d.nDeviceID == outdev; });
    REQUIRE(idev != devs.end());

    std::vector<int> channels = {1, 2};
    std::vector<double> frmdurations = { 0.02, 0.04, 0.1, 0.24, 0.5, };
    for (auto chans : channels)
    {
        for (auto frmduration : frmdurations)
        {
            media::AudioFormat fmt(idev->nDefaultSampleRate, chans);
            int framesize = fmt.samplerate * frmduration;

            class MyClass : public StreamDuplex
            {
                WavePCMFile m_echofile, m_playfile;
                audio_resampler_t m_resampler;
                int m_playframesize;
                std::vector<short> m_playbuffer;

            public:
                MyClass(media::AudioFormat fmt, int framesize)
                {
                    TTCHAR filename[MAX_PATH];
                    ACE_OS::snprintf(filename, MAX_PATH, ACE_TEXT("EchoCancel_%dHz_%s_txinterval_%03dmsec.wav"),
                                     fmt.samplerate, (fmt.channels == 2?ACE_TEXT("Stereo"):ACE_TEXT("Mono")),
                                         PCM16_SAMPLES_DURATION(framesize, fmt.samplerate));
                    REQUIRE(m_echofile.NewFile(filename, fmt.samplerate, fmt.channels));

                    REQUIRE(m_playfile.OpenFile(ECHOPLAYBACKFILENAME, true));

                    media::AudioFormat filefmt(m_playfile.GetSampleRate(), m_playfile.GetChannels());
                    m_playframesize = CalcSamples(fmt.samplerate, framesize, filefmt.samplerate);
                    m_resampler = MakeAudioResampler(filefmt, fmt, m_playframesize);
                    REQUIRE(m_resampler);
                    m_playbuffer.resize(m_playframesize * m_playfile.GetChannels());
                }
                int callbacks = 0;

                void StreamDuplexCb(const DuplexStreamer& streamer,
                    const short* input_buffer,
                    short* output_buffer, int samples) override
                {
                    //MYTRACE(ACE_TEXT("Callback of %d samples\n"), samples);
                    callbacks++;
                    REQUIRE(m_echofile.AppendSamples(input_buffer, samples));

                    REQUIRE(m_playfile.ReadSamples(&m_playbuffer[0], m_playframesize)>0);
                    int resampled = m_resampler->Resample(&m_playbuffer[0], output_buffer);
                    REQUIRE(resampled <= samples);
                }

                soundsystem::SoundDeviceFeatures GetDuplexFeatures() override
                {
                    return soundsystem::SOUNDDEVICEFEATURE_AEC | soundsystem::SOUNDDEVICEFEATURE_AGC | soundsystem::SOUNDDEVICEFEATURE_DENOISE;
                }

            } myduplex(fmt, framesize);

            REQUIRE(sndsys->OpenDuplexStream(&myduplex, indev, outdev, sndgrpid, fmt.samplerate, fmt.channels, fmt.channels, framesize));

            int frameduration = PCM16_SAMPLES_DURATION(framesize, fmt.samplerate);
            while(myduplex.callbacks * frameduration <= 10000)
            {
                Sleep(frameduration);
            }

            sndsys->CloseDuplexStream(&myduplex);
        }
    }

    sndsys->RemoveSoundGroup(sndgrpid);
}

TEST_CASE("TT_AEC")
{
    std::vector<ttinst> clients;
    auto ttclient = TT_InitTeamTalkPoll();
    clients.push_back(ttclient);

    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));

    // Only WASAPI supported by CWMAudioAEC
    INT32 indev, outdev;
    REQUIRE(TT_GetDefaultSoundDevicesEx(SOUNDSYSTEM_WASAPI, &indev, &outdev));

    // Set sound effects prior to joining a channel results in CWMAudioAEC being enabled
    SoundDeviceEffects effects = {};
    effects.bEnableEchoCancellation = TRUE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects));
    // When using CWMAudioAEC the requirement for shared sample rate between input and output device doesn't apply
    REQUIRE(InitSound(ttclient, DUPLEX, indev, outdev));

    int chanid = TT_GetRootChannelID(ttclient);
    int waitms = DEFWAIT;
    TTMessage msg;
    int cmdid = TT_DoJoinChannelByID(ttclient, chanid, _T(""));
    REQUIRE(cmdid>0);
    while(TT_GetMessage(ttclient, &msg, &waitms))
    {
        REQUIRE(msg.nClientEvent != CLIENTEVENT_INTERNAL_ERROR);
        if (msg.nClientEvent == CLIENTEVENT_CMD_PROCESSING && msg.bActive == FALSE)
            break;
    }

    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_USERID, STREAMTYPE_VOICE, TRUE));
    int abCount = 20;
    AudioBlock* ab;
    while (abCount--)
    {
        REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK, msg));
        ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_USERID);
        REQUIRE(ab != nullptr);
        REQUIRE(TT_ReleaseUserAudioBlock(ttclient, ab));
    }

    // Reset state
    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoLeaveChannel(ttclient)));
    REQUIRE(TT_CloseSoundDuplexDevices(ttclient));
    effects.bEnableEchoCancellation = FALSE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_USERID, STREAMTYPE_VOICE, FALSE));
    while (ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_USERID))
        TT_ReleaseUserAudioBlock(ttclient, ab);

    // Test that we can also only run with AGC and NS
    effects.bEnableAGC = TRUE;
    effects.bEnableDenoise = TRUE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects));
    REQUIRE(InitSound(ttclient, DUPLEX, indev, outdev));
    cmdid = TT_DoJoinChannelByID(ttclient, chanid, _T(""));
    REQUIRE(cmdid>0);
    while(TT_GetMessage(ttclient, &msg, &waitms))
    {
        REQUIRE(msg.nClientEvent != CLIENTEVENT_INTERNAL_ERROR);
        if (msg.nClientEvent == CLIENTEVENT_CMD_PROCESSING && msg.bActive == FALSE)
            break;
    }

    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_USERID, STREAMTYPE_VOICE, TRUE));

    abCount = 20;
    while (abCount--)
    {
        REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK, msg));
        ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_USERID);
        REQUIRE(ab != nullptr);
        REQUIRE(TT_ReleaseUserAudioBlock(ttclient, ab));
    }

    // It's not possible to change sound effects when sound device is active (in channel)
    effects.bEnableEchoCancellation = TRUE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects) == FALSE);

    REQUIRE(TT_CloseSoundDuplexDevices(ttclient));
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects));
    REQUIRE(InitSound(ttclient, DUPLEX, indev, outdev));
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_INTERNAL_ERROR, 500) == false);

    // cannot disable sound effects either
    effects.bEnableAGC = effects.bEnableDenoise = effects.bEnableEchoCancellation = FALSE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects) == FALSE);
}
#endif

TEST_CASE("testMuxedAudioBlockSoundInputDisabled")
{
    std::vector<ttinst> clients;
    auto ttclient = TT_InitTeamTalkPoll();
    clients.push_back(ttclient);

    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(InitSound(ttclient));
    REQUIRE(JoinRoot(ttclient));

    REQUIRE(TT_CloseSoundInputDevice(ttclient));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_MUXED_USERID, STREAMTYPE_VOICE, TRUE));

    int n_blocks = 10;
    do
    {
        TTMessage msg;
        REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK, msg));
        REQUIRE(msg.nSource == TT_MUXED_USERID);
        AudioBlock* ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_MUXED_USERID);
        REQUIRE(ab);
        REQUIRE(ab->nSamples>0);
        REQUIRE(TT_ReleaseUserAudioBlock(ttclient, ab));
    } while (n_blocks--);
}

#if defined(ENABLE_FFMPEG3) && 0
TEST_CASE("testThumbnail")
{
    // ffmpeg -i in.mp3 -i teamtalk.png -map 0:0 -map 1:0 -c copy -id3v2_version 3 -metadata:s:v title="Album cover" -metadata:s:v comment="Cover (front)" out.mp3
    FFMpegStreamer ffmpeg;
    MediaStreamOutput prop(media::AudioFormat(16000, 2), 1600, media::FOURCC_RGB32);
    auto filename = "out.mp3";

    REQUIRE(ffmpeg.OpenFile(filename, prop));

    std::promise<bool> done;
    auto sig_done = done.get_future();

    auto status = [&] (const MediaFileProp& mfp, MediaStreamStatus status) {
                      std::cout << mfp.filename.c_str() << " status: " << (int)status << std::endl;
                      if (status == MEDIASTREAM_FINISHED)
                          done.set_value(true);
                  };

    auto audio = [] (media::AudioFrame& audio_frame, ACE_Message_Block* mb_audio) {
                     return false;
                 };

    auto video = [] (media::VideoFrame& video_frame, ACE_Message_Block* mb_video) {
                    return false;
                };


    ffmpeg.RegisterStatusCallback(status, true);
    ffmpeg.RegisterAudioCallback(audio, true);
    ffmpeg.RegisterVideoCallback(video, true);

    REQUIRE(ffmpeg.StartStream());

    REQUIRE(sig_done.get());
}
#endif

#if defined(ENABLE_ENCRYPTION) && 0
// Encryption context should apparently not be set on client unless it
// is meant for peer verification
TEST_CASE("testSSLSetup")
{
    std::vector<ttinst> clients;
    auto ttclient = TT_InitTeamTalkPoll();
    clients.push_back(ttclient);

    EncryptionContext context = {};
    ACE_OS::strsncpy(context.szCertificateFile, ACE_TEXT("ttclientcert.pem"), TT_STRLEN);
    ACE_OS::strsncpy(context.szPrivateKeyFile, ACE_TEXT("ttclientkey.pem"), TT_STRLEN);
    ACE_OS::strsncpy(context.szCAFile, ACE_TEXT("ca.cer"), TT_STRLEN);
    context.bVerifyPeer = FALSE;
    context.bVerifyClientOnce = TRUE;
    context.nVerifyDepth = 0;

    REQUIRE(TT_SetEncryptionContext(ttclient, &context));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), DEFAULT_ENCRYPTED_TCPPORT, DEFAULT_ENCRYPTED_UDPPORT, TRUE));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
}
#endif

TEST_CASE("Last voice packet - wav files")
{
    TTCHAR curdir[1024] = {};
    ACE_OS::getcwd(curdir, 1024);


    //clean up wav files from previous runs
    //they are not deleted after the test, so they are available for inspection afterwards
    ACE_TCHAR delim = ACE_DIRECTORY_SEPARATOR_CHAR;
    ACE_OS::strncat(curdir, &delim, 1);
    ACE_OS::strncat(curdir, ACE_TEXT("wav"), 4);

    ACE_stat fileInfo;
    ACE_DIR* dir = ACE_OS::opendir(curdir);
    if (!dir)
    {
        ACE_OS::mkdir(curdir);
        dir = ACE_OS::opendir(curdir);
    }
    REQUIRE(dir);

    if (ACE_OS::stat(curdir, &fileInfo) == -1)
    {
        ACE_OS::mkdir(curdir);
        dir = ACE_OS::opendir(curdir);
    }
    dirent* dirInfo = ACE_OS::readdir(dir);
    ACE_TCHAR fileToDelete[1024]{};
    do
    {
        if ((ACE_OS::strcmp(dirInfo->d_name, ACE_TEXT(".")) == 0) || (ACE_OS::strcmp(dirInfo->d_name, ACE_TEXT("..")) == 0))
        {
            continue;
        }

        TTCHAR buf[1024]{};
        ACE_OS::strncpy(buf, dirInfo->d_name + ACE_OS::strlen(dirInfo->d_name) - 4, 4);
        int index = ACE_OS::strncmp(buf, ACE_TEXT(".wav"), 4);

        if (index == 0)
        {
            ACE_OS::strcpy(fileToDelete, curdir);
            ACE_OS::strncat(fileToDelete, &delim, 1);
            ACE_OS::strncat(fileToDelete, dirInfo->d_name, ACE_OS::strlen(dirInfo->d_name));
            ACE_OS::unlink(fileToDelete);
        }


    } while ((dirInfo = ACE_OS::readdir(dir)) != NULL);

    ACE_OS::closedir(dir);

    SoundDevice indev, outdev;
    REQUIRE(GetSoundDevices(indev, outdev));
    class SharedInputReset
    {
        SoundDevice indev;
    public:
        SharedInputReset(SoundDevice in) : indev(in)
        {
            // By default a shared device uses default sample rate, max channels and 40 msec framesize.
            // We reduce it to 20 msec in order to get the 240 msec frame faster.
            REQUIRE(TT_InitSoundInputSharedDevice(indev.nDefaultSampleRate, indev.nMaxInputChannels, int(indev.nDefaultSampleRate * 0.02)));
        }
        ~SharedInputReset()
        {
            // reset back to default
            REQUIRE(TT_InitSoundInputSharedDevice(indev.nDefaultSampleRate, indev.nMaxInputChannels, int(indev.nDefaultSampleRate * 0.04)));
        }
    } a(indev);

    //run multiple times to get the wav output files with different number of frames.
    //running 10 times, gives me 1-4 files with a missing frame, sometimes I get no file with a missing frame, but then you can run this test again or increase the number
    //of runs from 10 to e.g. 20.
    for (int i = 0; i < 10; i++)
    {
        std::vector<ttinst> clients;
        auto txclient = TT_InitTeamTalkPoll();
        auto rxclient = TT_InitTeamTalkPoll();
        clients.push_back(txclient);
        clients.push_back(rxclient);

        REQUIRE(InitSound(txclient, SHARED_INPUT, indev.nDeviceID, outdev.nDeviceID));
        REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
        REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));

        REQUIRE(InitSound(rxclient, SHARED_INPUT, indev.nDeviceID, outdev.nDeviceID));
        REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
        REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));

        AudioCodec audiocodec = {};
        audiocodec.nCodec = OPUS_CODEC;
        audiocodec.opus.nApplication = OPUS_APPLICATION_VOIP;
        audiocodec.opus.nTxIntervalMSec = 240;
#if defined(OPUS_FRAMESIZE_120_MS)
        audiocodec.opus.nFrameSizeMSec = 120;
#else
        audiocodec.opus.nFrameSizeMSec = 60;
#endif
        audiocodec.opus.nBitRate = OPUS_MIN_BITRATE;
        audiocodec.opus.nChannels = 2;
        audiocodec.opus.nComplexity = 10;
        audiocodec.opus.nSampleRate = 48000;
        audiocodec.opus.bDTX = true;
        audiocodec.opus.bFEC = true;
        audiocodec.opus.bVBR = false;
        audiocodec.opus.bVBRConstraint = false;

        Channel chan = MakeChannel(txclient, ACE_TEXT("foo"), TT_GetRootChannelID(txclient), audiocodec);
        REQUIRE(WaitForCmdSuccess(txclient, TT_DoJoinChannel(txclient, &chan)));
        REQUIRE(WaitForCmdSuccess(rxclient, TT_DoJoinChannelByID(rxclient, TT_GetMyChannelID(txclient), ACE_TEXT(""))));

        REQUIRE(TT_DBG_SetSoundInputTone(txclient, STREAMTYPE_VOICE, 600));
        REQUIRE(TT_SetUserMediaStorageDir(rxclient, TT_GetMyUserID(txclient), curdir, ACE_TEXT(""), AFF_WAVE_FORMAT));

        auto voicestop = [&](TTMessage msg)
            {
                if (msg.nClientEvent == CLIENTEVENT_USER_STATECHANGE &&
                    msg.user.nUserID == TT_GetMyUserID(txclient) &&
                    (msg.user.uUserState & USERSTATE_VOICE) == 0)
                {
                    return true;
                }

                return false;
            };

        REQUIRE(TT_EnableVoiceTransmission(txclient, true));
        /*
         * A duration which ends on a third of a package size, will produce different wav outputs (files are generated which last 1220 ms (5x nTxIntervalMSec)
         * and files are generated which last 1440ms (6x nTxIntervalMSec)
         */
        int duration = int(audiocodec.opus.nTxIntervalMSec * 5 + audiocodec.opus.nTxIntervalMSec * 0.33);
        WaitForEvent(txclient, CLIENTEVENT_NONE, duration);
        ClientStatistics stats = {};
        REQUIRE(TT_GetClientStatistics(txclient, &stats));
        // take sound device start into account before stopping PTT
        WaitForEvent(txclient, CLIENTEVENT_NONE, stats.nSoundInputDeviceDelayMSec);

        REQUIRE(TT_EnableVoiceTransmission(txclient, false));

        std::cout << "Initial audio frame delay: " << stats.nSoundInputDeviceDelayMSec << " msec. ";

        REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_STATECHANGE, voicestop));

        REQUIRE(TT_GetClientStatistics(txclient, &stats));
        std::cout << "Encoded voice sent: " << stats.nVoiceBytesSent << " bytes. ";

        UserStatistics ustats;
        REQUIRE(TT_GetUserStatistics(rxclient, TT_GetMyUserID(txclient), &ustats));
        std::cout << "Voice packets received: " << ustats.nVoicePacketsRecv << std::endl;
    }

    //check file sizes. All files should hvae the same size (but some are missing the last frame, so this test fails)
    long long fileSize = -1;
    dir = ACE_OS::opendir(curdir);
    dirInfo = ACE_OS::readdir(dir);
    do
    {
        if ((ACE_OS::strcmp(dirInfo->d_name, ACE_TEXT(".")) == 0) || (ACE_OS::strcmp(dirInfo->d_name, ACE_TEXT("..")) == 0))
        {
            continue;
        }

        TTCHAR buf[1024]{};
        ACE_OS::strncpy(buf, dirInfo->d_name + ACE_OS::strlen(dirInfo->d_name) - 4, 4);
        int index = ACE_OS::strncmp(buf, ACE_TEXT(".wav"), 4);

        ACE_TCHAR fileToCheck[1024]{};
        if (index == 0)
        {
            ACE_OS::strcpy(fileToCheck, curdir);
            ACE_OS::strncat(fileToCheck, &delim, 1);
            ACE_OS::strncat(fileToCheck, dirInfo->d_name, ACE_OS::strlen(dirInfo->d_name));

            if (ACE_OS::stat(fileToCheck, &fileInfo) != -1)
            {
                if (fileSize == -1)
                {
                    fileSize = fileInfo.st_size;
                }

                REQUIRE(fileSize == fileInfo.st_size);
            }
        }
    } while ((dirInfo = ACE_OS::readdir(dir)) != NULL);

    ACE_OS::closedir(dir);
}

#if defined(ENABLE_WEBRTC)

#if 0 /* gain_controller1 doesn't work */
TEST_CASE("SoundLoopbackDuplexDBFS1")
{
    SoundDevice indev, outdev;
    REQUIRE(GetSoundDevices(indev, outdev));

    std::cout << "input: " << indev.nDeviceID << " name: " << indev.szDeviceName
              << " channels: " << indev.nMaxInputChannels << " samplerate: " << indev.nDefaultSampleRate
              << " output: " << outdev.nDeviceID << " name: " << outdev.szDeviceName << std::endl;
    ttinst ttclient(TT_InitTeamTalkPoll());

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.gaincontroller1.bEnable = TRUE;
    preprocess.webrtc.gaincontroller1.nTargetLevelDBFS = 1;

    auto sndloop = TT_StartSoundLoopbackTestEx(indev.nDeviceID, outdev.nDeviceID, indev.nDefaultSampleRate,
                                          1, TRUE, &preprocess, nullptr);
    REQUIRE(sndloop);

    std::cout << "Recording...." << std::endl;

    WaitForEvent(ttclient, CLIENTEVENT_NONE, 10000);

    REQUIRE(TT_CloseSoundLoopbackTest(sndloop));
}

TEST_CASE("SoundLoopbackDuplexDBFS30")
{
    SoundDevice indev, outdev;
    REQUIRE(GetSoundDevices(indev, outdev));

    std::cout << "input: " << indev.nDeviceID << " name: " << indev.szDeviceName
              << " channels: " << indev.nMaxInputChannels << " samplerate: " << indev.nDefaultSampleRate
              << " output: " << outdev.nDeviceID << " name: " << outdev.szDeviceName << std::endl;
    ttinst ttclient(TT_InitTeamTalkPoll());

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.gaincontroller1.bEnable = TRUE;
    preprocess.webrtc.gaincontroller1.nTargetLevelDBFS = 30;

    auto sndloop = TT_StartSoundLoopbackTestEx(indev.nDeviceID, outdev.nDeviceID, indev.nDefaultSampleRate,
                                          1, TRUE, &preprocess, nullptr);
    REQUIRE(sndloop);

    std::cout << "Recording...." << std::endl;

    WaitForEvent(ttclient, CLIENTEVENT_NONE, 10000);

    REQUIRE(TT_CloseSoundLoopbackTest(sndloop));
}
#endif /* gain_controller1 */

TEST_CASE("SoundLoopbackDefault")
{
    SoundDevice indev, outdev;
    REQUIRE(GetSoundDevices(indev, outdev));

#if defined(UNICODE)
    std::wcout <<
#else
    std::cout <<
#endif
        "input: " << indev.nDeviceID << " name: " << indev.szDeviceName
               << " channels: " << indev.nMaxInputChannels << " samplerate: " << indev.nDefaultSampleRate
               << " output: " << outdev.nDeviceID << " name: " << outdev.szDeviceName << std::endl;

    ttinst ttclient(TT_InitTeamTalkPoll());

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.gaincontroller2.bEnable = TRUE;
    preprocess.webrtc.gaincontroller2.fixeddigital.fGainDB = 25;

    preprocess.webrtc.noisesuppression.bEnable = FALSE;
    preprocess.webrtc.noisesuppression.nLevel = 0;

    auto sndloop = TT_StartSoundLoopbackTestEx(indev.nDeviceID, outdev.nDeviceID, indev.nDefaultSampleRate,
                                          1, FALSE, &preprocess, nullptr);
    REQUIRE(sndloop);

    std::cout << "Recording...." << std::endl;

    WaitForEvent(ttclient, CLIENTEVENT_NONE, 30000);

    REQUIRE(TT_CloseSoundLoopbackTest(sndloop));
}

TEST_CASE("WebRTC_SampleRates")
{
    ttinst ttclient(TT_InitTeamTalkPoll());

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.gaincontroller2.bEnable = TRUE;
    preprocess.webrtc.gaincontroller2.fixeddigital.fGainDB = 25;

    const std::vector<int> standardSampleRates = {8000, 12000, 16000, 24000, 32000, 44100, 48000};

    for (auto samplerate : standardSampleRates)
    {
        auto sndloop = TT_StartSoundLoopbackTestEx(TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL,
                                                   TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL,
                                                   samplerate, 2, TRUE, &preprocess, nullptr);
        REQUIRE(sndloop);

        REQUIRE(TT_CloseSoundLoopbackTest(sndloop));
    }
}

TEST_CASE("WebRTCPreprocessor")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));
    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoSubscribe(ttclient, TT_GetMyUserID(ttclient), SUBSCRIBE_VOICE)));

    TTCHAR curdir[1024] = {};
    ACE_OS::getcwd(curdir, 1024);
    REQUIRE(TT_SetUserMediaStorageDir(ttclient, TT_GetMyUserID(ttclient), curdir, ACE_TEXT(""), AFF_WAVE_FORMAT));

    REQUIRE(TT_EnableVoiceTransmission(ttclient, true));
    WaitForEvent(ttclient, CLIENTEVENT_NONE, 5000);

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;

    preprocess.webrtc.noisesuppression.bEnable = TRUE;
    preprocess.webrtc.noisesuppression.nLevel = 3;

    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));
    WaitForEvent(ttclient, CLIENTEVENT_NONE, 5000);

    preprocess.webrtc.gaincontroller2.bEnable = TRUE;
    preprocess.webrtc.gaincontroller2.fixeddigital.fGainDB = 10;

    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));
    WaitForEvent(ttclient, CLIENTEVENT_NONE, 5000);

    REQUIRE(TT_EnableVoiceTransmission(ttclient, false));

}

#if 0 /* gain_controller1 doesn't work */
TEST_CASE("WebRTC_gaincontroller1")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));

    MediaFilePlayback mfp = {};
    mfp.audioPreprocessor.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    mfp.audioPreprocessor.webrtc.gaincontroller1.bEnable = TRUE;
    mfp.audioPreprocessor.webrtc.gaincontroller1.nTargetLevelDBFS = 25;

    auto session = TT_InitLocalPlayback(ttclient, ACE_TEXT("input_low.wav"), &mfp);
    REQUIRE(session > 0);

    bool success = false, toggled = false, stop = false;
    TTMessage msg;
    while (WaitForEvent(ttclient, CLIENTEVENT_LOCAL_MEDIAFILE, msg, 5000) && !stop)
    {
        switch(msg.mediafileinfo.nStatus)
        {
        case MFS_PLAYING :
            if (msg.mediafileinfo.uElapsedMSec >= 3000 && !toggled)
            {
                mfp.uOffsetMSec = TT_MEDIAPLAYBACK_OFFSET_IGNORE;
                mfp.audioPreprocessor.webrtc.gaincontroller1.bEnable = TRUE;
                mfp.audioPreprocessor.webrtc.gaincontroller1.nTargetLevelDBFS = 0;
                REQUIRE(TT_UpdateLocalPlayback(ttclient, session, &mfp));
                toggled = true;
                std::cout << "Toggled: " << msg.mediafileinfo.uElapsedMSec << std::endl;
            }
            if (msg.mediafileinfo.uElapsedMSec >= 10000)
            {
                std::cout << "Elapsed: " << msg.mediafileinfo.uElapsedMSec << std::endl;
                stop = true;
            }
            break;
        case MFS_FINISHED :
            success = true;
            break;
        }
    }
    REQUIRE(toggled);
    REQUIRE(success);
}
#endif /* gain_controller1 */

TEST_CASE("WebRTC_gaincontroller2")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));

    MediaFilePlayback mfp = {};
    mfp.audioPreprocessor.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;

    mfp.audioPreprocessor.webrtc.gaincontroller2.bEnable = FALSE;
    mfp.audioPreprocessor.webrtc.gaincontroller2.fixeddigital.fGainDB = 0;

    mfp.audioPreprocessor.webrtc.noisesuppression.bEnable = FALSE;
    mfp.audioPreprocessor.webrtc.noisesuppression.nLevel = 3;

    auto session = TT_InitLocalPlayback(ttclient, ACE_TEXT("input_low.wav"), &mfp);
    REQUIRE(session > 0);

    bool success = false, toggled = false, stop = false;
    TTMessage msg;
    while (WaitForEvent(ttclient, CLIENTEVENT_LOCAL_MEDIAFILE, msg, 5000) && !stop)
    {
        switch(msg.mediafileinfo.nStatus)
        {
        case MFS_PLAYING :
            if (msg.mediafileinfo.uElapsedMSec >= 3000 && !toggled)
            {
                mfp.uOffsetMSec = TT_MEDIAPLAYBACK_OFFSET_IGNORE;
                mfp.audioPreprocessor.webrtc.gaincontroller2.bEnable = TRUE;
                mfp.audioPreprocessor.webrtc.gaincontroller2.fixeddigital.fGainDB = 25;
                REQUIRE(TT_UpdateLocalPlayback(ttclient, session, &mfp));
                toggled = true;
                std::cout << "Toggled: " << msg.mediafileinfo.uElapsedMSec << std::endl;
            }
            if (msg.mediafileinfo.uElapsedMSec >= 10000)
            {
                std::cout << "Elapsed: " << msg.mediafileinfo.uElapsedMSec << std::endl;
                stop = true;
            }
            break;
        case MFS_FINISHED :
            success = true;
            break;
        }
    }
    REQUIRE(toggled);
    REQUIRE(success);
}

TEST_CASE("WebRTC_echocancel")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    SoundDeviceEffects effects = {};
    effects.bEnableEchoCancellation = FALSE;
    REQUIRE(TT_SetSoundDeviceEffects(ttclient, &effects));
    REQUIRE(InitSound(ttclient, DUPLEX));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    // REQUIRE(JoinRoot(ttclient));

    AudioCodec codec = {};
    codec.nCodec = SPEEX_VBR_CODEC;
    codec.speex_vbr.nBandmode = 2;
    codec.speex_vbr.nBitRate = 16000;
    codec.speex_vbr.nMaxBitRate = SPEEX_UWB_MAX_BITRATE;
    codec.speex_vbr.nQuality = 10;
    codec.speex_vbr.nTxIntervalMSec = 40;
    Channel chan = MakeChannel(ttclient, ACE_TEXT("speex"), TT_GetRootChannelID(ttclient), codec);
    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoJoinChannel(ttclient, &chan)));

    AudioPreprocessor preprocess = {};

    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;

    switch (preprocess.nPreprocessor)
    {
    case WEBRTC_AUDIOPREPROCESSOR :
        preprocess.webrtc.echocanceller.bEnable = TRUE;

        preprocess.webrtc.noisesuppression.bEnable = TRUE;
        preprocess.webrtc.noisesuppression.nLevel = 2;

        preprocess.webrtc.gaincontroller2.bEnable = TRUE;
        preprocess.webrtc.gaincontroller2.fixeddigital.fGainDB = 25;
        break;
    case SPEEXDSP_AUDIOPREPROCESSOR :
#define DEFAULT_AGC_ENABLE              TRUE
#define DEFAULT_AGC_GAINLEVEL           8000
#define DEFAULT_AGC_INC_MAXDB           12
#define DEFAULT_AGC_DEC_MAXDB           -40
#define DEFAULT_AGC_GAINMAXDB           30
#define DEFAULT_DENOISE_ENABLE          TRUE
#define DEFAULT_DENOISE_SUPPRESS        -30
#define DEFAULT_ECHO_ENABLE             TRUE
#define DEFAULT_ECHO_SUPPRESS           -40
#define DEFAULT_ECHO_SUPPRESSACTIVE     -15
        preprocess.speexdsp.bEnableAGC = DEFAULT_AGC_ENABLE;
        preprocess.speexdsp.nGainLevel = DEFAULT_AGC_GAINLEVEL;
        preprocess.speexdsp.nMaxIncDBSec = DEFAULT_AGC_INC_MAXDB;
        preprocess.speexdsp.nMaxDecDBSec = DEFAULT_AGC_DEC_MAXDB;
        preprocess.speexdsp.nMaxGainDB = DEFAULT_AGC_GAINMAXDB;
        preprocess.speexdsp.bEnableDenoise = DEFAULT_DENOISE_ENABLE;
        preprocess.speexdsp.nMaxNoiseSuppressDB = DEFAULT_DENOISE_SUPPRESS;
        preprocess.speexdsp.bEnableEchoCancellation = DEFAULT_ECHO_ENABLE;
        preprocess.speexdsp.nEchoSuppress = DEFAULT_ECHO_SUPPRESS;
        preprocess.speexdsp.nEchoSuppressActive = DEFAULT_ECHO_SUPPRESSACTIVE;
        break;
    }

    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));

    REQUIRE(TT_EnableVoiceTransmission(ttclient, true));

    WaitForEvent(ttclient, CLIENTEVENT_NONE, 5000);
}

TEST_CASE("WebRTC_Preamplifier")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));

    int level = 0;
    
    REQUIRE(TT_EnableVoiceTransmission(ttclient, TRUE));
    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 300));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_TX_USERID, STREAMTYPE_VOICE, TRUE));

    // no gain
    AudioPreprocessor preprocess = {};
    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));

    TTMessage msg = {};
    int streamid = 0;
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK, msg));
    auto ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_TX_USERID);
    REQUIRE(ab);
    TT_ReleaseUserAudioBlock(ttclient, ab);
    streamid = ab->nStreamID;
    level = TT_GetSoundInputLevel(ttclient);
    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoLeaveChannel(ttclient)));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_TX_USERID, STREAMTYPE_VOICE, FALSE));
    REQUIRE(TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_TX_USERID) == nullptr);
    REQUIRE(TT_EnableVoiceTransmission(ttclient, FALSE));

    // half gain
    preprocess.webrtc.preamplifier.bEnable = TRUE;
    preprocess.webrtc.preamplifier.fFixedGainFactor = .5f;
    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));

    REQUIRE(JoinRoot(ttclient));
    REQUIRE(TT_EnableVoiceTransmission(ttclient, TRUE));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_TX_USERID, STREAMTYPE_VOICE, TRUE));
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK));
    ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_TX_USERID);
    REQUIRE(ab);
    TT_ReleaseUserAudioBlock(ttclient, ab);
    REQUIRE(streamid + 1 == ab->nStreamID);
    REQUIRE(level / 2 == TT_GetSoundInputLevel(ttclient));
    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoLeaveChannel(ttclient)));
    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_TX_USERID, STREAMTYPE_VOICE, FALSE));
    REQUIRE(TT_EnableVoiceTransmission(ttclient, FALSE));
}

TEST_CASE("WebRTC_LevelEstimation")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));

    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 500));

    REQUIRE(TT_EnableAudioBlockEvent(ttclient, TT_LOCAL_USERID, STREAMTYPE_VOICE, TRUE));

    AudioPreprocessor preprocess = {};
    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.levelestimation.bEnable = TRUE;
    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));
    int n = 10;
    do
    {
        REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_USER_AUDIOBLOCK));
        auto ab = TT_AcquireUserAudioBlock(ttclient, STREAMTYPE_VOICE, TT_LOCAL_USERID);
        REQUIRE(ab);
        REQUIRE(TT_ReleaseUserAudioBlock(ttclient, ab));
        REQUIRE(TT_GetSoundInputLevel(ttclient) >= 88);
    } while (n-- > 0);
}

TEST_CASE("WebRTC_VAD")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));

    AudioPreprocessor preprocess = {};
    preprocess.nPreprocessor = WEBRTC_AUDIOPREPROCESSOR;
    preprocess.webrtc.voicedetection.bEnable = TRUE;
    REQUIRE(TT_SetSoundInputPreprocessEx(ttclient, &preprocess));

    REQUIRE(TT_SetVoiceActivationStopDelay(ttclient, 200));
    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 500));

    REQUIRE(TT_EnableVoiceActivation(ttclient, TRUE));
    TTMessage msg = {};
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_VOICE_ACTIVATION, msg));
    REQUIRE(msg.bActive);

    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 0));
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_VOICE_ACTIVATION, msg));
    REQUIRE(!msg.bActive);
}

#endif /* ENABLE_WEBRTC */

TEST_CASE("TeamTalk_VAD")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));

    REQUIRE(TT_EnableVoiceActivation(ttclient, TRUE));
    REQUIRE(TT_SetVoiceActivationLevel(ttclient, 63));
    REQUIRE(TT_SetVoiceActivationStopDelay(ttclient, 100));

    TTMessage msg = {};
    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 500));
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_VOICE_ACTIVATION, msg));
    REQUIRE(msg.bActive);

    REQUIRE(TT_DBG_SetSoundInputTone(ttclient, STREAMTYPE_VOICE, 0));
    REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_VOICE_ACTIVATION, msg));
    REQUIRE(!msg.bActive);
}

#if defined(ENABLE_PORTAUDIO) && defined(WIN32)

int paSamples = 0;
uint32_t paTimeStamp = 0;

int Foo_StreamCallback(const void* inputBuffer, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* pUserData)
{
    paSamples += framesPerBuffer;
    if (!paTimeStamp)
        paTimeStamp = GETTIMESTAMP();

    return paContinue;
}

TEST_CASE("PortAudioRaw_SamplesPerSec")
{
    PaError err = Pa_Initialize();

    PaDeviceIndex inputdeviceid = -1, outputdeviceid = -1;
    PaHostApiIndex hostApi = Pa_HostApiTypeIdToHostApiIndex(paWASAPI);
    if (hostApi != paHostApiNotFound)
    {
        const PaHostApiInfo* hostapi = Pa_GetHostApiInfo(hostApi);
        if (hostapi)
        {
            inputdeviceid = hostapi->defaultInputDevice;
            outputdeviceid = hostapi->defaultOutputDevice;
        }
    }
    REQUIRE(outputdeviceid >= 0);

    const PaDeviceInfo* ininfo = Pa_GetDeviceInfo(outputdeviceid);
    REQUIRE(ininfo);
    PaStreamParameters outputParameters = {};
    outputParameters.device = outputdeviceid;
    outputParameters.channelCount = 1;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    outputParameters.sampleFormat = paInt16;
    outputParameters.suggestedLatency = ininfo->defaultLowOutputLatency;

    PaStream* outstream;
    err = Pa_OpenStream(&outstream, nullptr, &outputParameters,
                        ininfo->defaultSampleRate, ininfo->defaultSampleRate * .04,
                        paClipOff, Foo_StreamCallback, static_cast<void*> (0));

    REQUIRE(Pa_StartStream(outstream) == paNoError);
    while (paSamples < ininfo->defaultSampleRate * 500)
    {
        Pa_Sleep(1000);

        auto samplesDurationMSec = PCM16_SAMPLES_DURATION(paSamples, int(ininfo->defaultSampleRate));
        auto durationMSec = GETTIMESTAMP() - paTimeStamp;
        auto skew = int(samplesDurationMSec - durationMSec);
        std::cout << "Samples duration: " << samplesDurationMSec << " / " << durationMSec << "  " << skew << std::endl;

        REQUIRE(skew < 0.08 * 1000);
    }

    Pa_Terminate();
}

TEST_CASE("PortAudio_SamplesPerSec")
{
    auto snd = soundsystem::GetInstance();
    auto grp = snd->OpenSoundGroup();

    int inputdeviceid, outputdeviceid;
    REQUIRE(snd->GetDefaultDevices(soundsystem::SOUND_API_WASAPI, inputdeviceid, outputdeviceid));
    soundsystem::devices_t devs;
    REQUIRE(snd->GetSoundDevices(devs));

    auto ioutdev = std::find_if(devs.begin(), devs.end(), [outputdeviceid](soundsystem::DeviceInfo& d)
        {
            return d.id == outputdeviceid;
        });

    REQUIRE(ioutdev != devs.end());

    soundsystem::DeviceInfo& outdev = *ioutdev;

    uint32_t samples = 0, starttime = 0;
    const int SAMPLERATE = 48000, CHANNELS = 1;

    class MyStream : public soundsystem::StreamPlayer
    {
        uint32_t& samples, &starttime;
    public:
        MyStream(uint32_t& s, uint32_t& st) : samples(s), starttime(st) {}
        bool StreamPlayerCb(const soundsystem::OutputStreamer& streamer, short* buffer, int framesPerBuffer)
        {
            if (!starttime)
                starttime = GETTIMESTAMP();
            samples += framesPerBuffer;
            return true;
        }
    } player(samples, starttime);

    REQUIRE(snd->OpenOutputStream(&player, outputdeviceid, grp, SAMPLERATE, CHANNELS, SAMPLERATE * 0.04));
    REQUIRE(snd->StartStream(&player));

    while (samples < outdev.default_samplerate * 500)
    {
        Pa_Sleep(1000);

        auto samplesDurationMSec = PCM16_SAMPLES_DURATION(samples, int(outdev.default_samplerate));
        auto durationMSec = GETTIMESTAMP() - starttime;
        auto skew = int(samplesDurationMSec - durationMSec);

        std::cout << "Samples duration: " << samplesDurationMSec << " / " << durationMSec << "  " << skew << std::endl;

        REQUIRE(skew < 0.08 * 1000);
    }
}

#endif

TEST_CASE("InjectAudio")
{
    ttinst ttclient(TT_InitTeamTalkPoll());
    REQUIRE(InitSound(ttclient, DEFAULT, TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL, TT_SOUNDDEVICE_ID_TEAMTALK_VIRTUAL));
    REQUIRE(Connect(ttclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(ttclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(ttclient));

    REQUIRE(WaitForCmdSuccess(ttclient, TT_DoSubscribe(ttclient, TT_GetMyUserID(ttclient), SUBSCRIBE_VOICE)));

    int SAMPLERATE = 48000, CHANNELS = 2;
    std::vector<short> buf(SAMPLERATE * CHANNELS);
    AudioBlock ab = {};
    ab.nStreamID = 1;
    ab.nSampleRate = SAMPLERATE;
    ab.nChannels = CHANNELS;
    ab.lpRawAudio = &buf[0];
    ab.nSamples = SAMPLERATE;

    // 3 secs
    int samples = SAMPLERATE * 3;

    do
    {
        std::cout << "Insert Audio" << std::endl;
        REQUIRE(TT_InsertAudioBlock(ttclient, &ab));

        TTMessage msg;
        do
        {
            REQUIRE(WaitForEvent(ttclient, CLIENTEVENT_AUDIOINPUT, msg));
        } while (msg.audioinputprogress.uQueueMSec > 0);

    } while ((samples -= SAMPLERATE) > 0);
}

TEST_CASE("FixedJitterBuffer")
{
    std::vector<ttinst> clients;
    auto txclient = TT_InitTeamTalkPoll();
    auto rxclient = TT_InitTeamTalkPoll();
    clients.push_back(txclient);
    clients.push_back(rxclient);

    REQUIRE(InitSound(txclient));
    REQUIRE(Connect(txclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(txclient, ACE_TEXT("TxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(txclient));

    REQUIRE(InitSound(rxclient));
    REQUIRE(Connect(rxclient, ACE_TEXT("127.0.0.1"), 10333, 10333));
    REQUIRE(Login(rxclient, ACE_TEXT("RxClient"), ACE_TEXT("guest"), ACE_TEXT("guest")));
    REQUIRE(JoinRoot(rxclient));

    uint32_t fixeddelay = 240;

    JitterConfig jitterconf{};
    jitterconf.nFixedDelayMSec = fixeddelay;
    jitterconf.bUseAdativeDejitter = false;
    jitterconf.nMaxAdaptiveDelayMSec = 10000;

    TT_SetUserJitterControl(rxclient, TT_GetMyUserID(txclient), STREAMTYPE_VOICE, &jitterconf);

    REQUIRE(TT_DBG_SetSoundInputTone(txclient, STREAMTYPE_VOICE, 500));
    REQUIRE(TT_EnableVoiceTransmission(txclient, true));

    auto voicestart = [&](TTMessage msg)
    {
        if (msg.nClientEvent == CLIENTEVENT_USER_STATECHANGE &&
            msg.user.nUserID == TT_GetMyUserID(txclient) &&
            (msg.user.uUserState & USERSTATE_VOICE) == USERSTATE_VOICE)
        {
            return true;
        }

        return false;
    };

    uint32_t starttime = GETTIMESTAMP();
    REQUIRE(WaitForEvent(rxclient, CLIENTEVENT_USER_STATECHANGE, voicestart));

    uint32_t endtime = GETTIMESTAMP();
    uint32_t delay = (endtime - starttime);
    INFO("Measured voice delay is " << delay);

    REQUIRE((delay >= fixeddelay));
    //Measuring the maximum deviation of the delay is not reliably possible because the CLIENTEVENT_USER_STATECHANGE is detected/notified on it's own timer.
}