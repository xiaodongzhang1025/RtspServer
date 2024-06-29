#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include "xop/RtspServer.h"
#include "xop/media.h"
#include "net/NetInterface.h"
#include "xop/JPEGSource.h"

#include "vendor_rtsp_server.h"

static unsigned int gPort = 0;
static xop::EventLoop* gpEventLoop;
static std::shared_ptr<xop::RtspServer> gpRtspServer;
static xop::MediaSessionId gMediaSessionIdTabe[xop::MAX_MEDIA_CHANNEL];
static RtspServerMediaSt *gpRtspServerMediaInfo[xop::MAX_MEDIA_CHANNEL];

int RtspServerInit(uint16_t port, char *mAuthName, char *mAuthPasswd)
{
	gPort = port;
	gpEventLoop = new xop::EventLoop();
	gpRtspServer = xop::RtspServer::Create(gpEventLoop);
	if (!gpRtspServer->Start("0.0.0.0", port)) {
		printf("RTSP Server listen on %d failed.\n", port);
		return 0;
	}

#if 1
	if(mAuthName && mAuthPasswd){
		gpRtspServer->SetAuthConfig("-_-", mAuthName, mAuthPasswd);
	}
#endif

#if 0
	//gpEventLoop->loop();
	while(1){
		xop::Timer::Sleep(100);
	}
#endif
	return 0;
}

int RtspServerDeInit(void){
	gpRtspServer->Stop();
	return 0;
}

static void RtspClientConnectCallback(xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
	RtspServerMediaSt *pRtspServerMediaInfo = NULL;
	unsigned int chn_id = 0;
	for(chn_id = 0; chn_id < xop::MAX_MEDIA_CHANNEL; chn_id++){
		if(sessionId == gMediaSessionIdTabe[(xop::MediaChannelId)chn_id]){
			pRtspServerMediaInfo = gpRtspServerMediaInfo[chn_id];
		}
	}
	if(pRtspServerMediaInfo && pRtspServerMediaInfo->mConnectCallback){
	    pRtspServerMediaInfo->mConnectCallback(peer_ip.c_str(), peer_port);
	}else{
		printf("__RTSP client connect, ip=%s, port=%hu, sessionId=%d\n", peer_ip.c_str(), peer_port, sessionId);
	}
}

static void RtspClientDisConnectCallback(xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
	RtspServerMediaSt *pRtspServerMediaInfo = NULL;
	unsigned int chn_id = 0;
	for(chn_id = 0; chn_id < xop::MAX_MEDIA_CHANNEL; chn_id++){
		if(sessionId == gMediaSessionIdTabe[(xop::MediaChannelId)chn_id]){
			pRtspServerMediaInfo = gpRtspServerMediaInfo[chn_id];
		}
	}
	if(pRtspServerMediaInfo && pRtspServerMediaInfo->mDisConnectCallback){
		pRtspServerMediaInfo->mDisConnectCallback(peer_ip.c_str(), peer_port);
	}else{
		printf("__RTSP client Disconnect, ip=%s, port=%hu, sessionId=%d\n", peer_ip.c_str(), peer_port, sessionId);
	}
}

int RtspServerMediaInit(RtspServerMediaSt *pRtspServerMediaInfo){
	std::string suffix = pRtspServerMediaInfo->mSuffix;
#if 1
	std::string ip = xop::NetInterface::GetLocalIPAddress(); //获取网卡ip地址
#else
	std::string ip = "127.0.0.1";
#endif
	xop::MediaSession *session = xop::MediaSession::CreateNew(suffix);
	//void AddNotifyConnectedCallback(const NotifyConnectedCallback& callback);
#if 0
	session->AddNotifyConnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu, sessionId=%d\n", peer_ip.c_str(), peer_port, sessionId);
		if(pRtspServerMediaInfo->mConnectCallback){
			pRtspServerMediaInfo->mConnectCallback(peer_ip.c_str(), peer_port);
		}
	});
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu, sessionId=%d\n", peer_ip.c_str(), peer_port, sessionId);
		if(pRtspServerMediaInfo->mDisConnectCallback){
			pRtspServerMediaInfo->mDisConnectCallback(peer_ip.c_str(), peer_port);
		}
	});
#else
	session->AddNotifyConnectedCallback(RtspClientConnectCallback);
	session->AddNotifyDisconnectedCallback(RtspClientDisConnectCallback);
#endif
	if(pRtspServerMediaInfo->mMediaType == xop::H264){
		session->AddSource((xop::MediaChannelId )pRtspServerMediaInfo->mChnId, xop::H264Source::CreateNew());
	}else if(pRtspServerMediaInfo->mMediaType == xop::H265){
		session->AddSource((xop::MediaChannelId )pRtspServerMediaInfo->mChnId, xop::H265Source::CreateNew());
	}else if(pRtspServerMediaInfo->mMediaType == xop::JPEG){
		session->AddSource((xop::MediaChannelId )pRtspServerMediaInfo->mChnId, xop::JPEGSource::CreateNew());
	}
#if 1
	if(pRtspServerMediaInfo->mMultiCast){
		session->StartMulticast();
	}
#endif
	gMediaSessionIdTabe[pRtspServerMediaInfo->mChnId] = gpRtspServer->AddSession(session);
	gpRtspServerMediaInfo[pRtspServerMediaInfo->mChnId] = pRtspServerMediaInfo;
    printf("Play URL: rtsp://%s:%d/%s \n\n", ip.c_str(), gPort, suffix.c_str());
	return 0;
}

int RtspServerMediaDeInit(RtspServerMediaSt *pRtspServerMediaInfo){
	gpRtspServerMediaInfo[pRtspServerMediaInfo->mChnId] = NULL;
	//xop::MediaSession *session = GetMediaSource(gMediaSessionIdTabe[(xop::MediaChannelId)pRtspServerMediaInfo->mChnId]);
	//session->RemoveSource((xop::MediaChannelId)pRtspServerMediaInfo->mChnId);
	return 0;
}


int RtspServerPushFrame(RtspServerMediaSt *pRtspServerMediaInfo, uint32_t bufCnt, uint8_t *frameBufList[], uint32_t frameSizeList[], enum RtspFrameType frameType){
	xop::AVFrame avFrame = {0};
	avFrame.type = frameType; 
	avFrame.size = 0;
	for(int i = 0; i < bufCnt; i++){
		avFrame.size += frameSizeList[i];
	}

	if(pRtspServerMediaInfo->mMediaType == xop::H264){
		avFrame.timestamp = xop::H264Source::GetTimestamp();
	}else if(pRtspServerMediaInfo->mMediaType == xop::H265){
		avFrame.timestamp = xop::H265Source::GetTimestamp();
	}else if(pRtspServerMediaInfo->mMediaType == xop::JPEG){
		avFrame.timestamp = xop::JPEGSource::GetTimestamp();
	}
	avFrame.buffer.reset(new uint8_t[avFrame.size]);

	int pos = 0;
	for(int i = 0; i < bufCnt; i++){
		memcpy((uint8_t *)avFrame.buffer.get() + pos, frameBufList[i], frameSizeList[i]);//
		pos += frameSizeList[i];
	}
	
	gpRtspServer->PushFrame((xop::MediaSessionId )gMediaSessionIdTabe[pRtspServerMediaInfo->mChnId], (xop::MediaChannelId )pRtspServerMediaInfo->mChnId, avFrame);
	return 0;
}
