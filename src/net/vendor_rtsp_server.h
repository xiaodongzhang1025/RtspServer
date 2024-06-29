#ifndef __VENDOR_RTSP_SERVER_H__
#define __VENDOR_RTSP_SERVER_H__


#ifdef __cplusplus
extern "C" {
#endif
enum RtspMediaType
{
    //PCMU = 0,  
    RTSP_MEDIA_PCMA = 8,
    RTSP_MEDIA_JPEG = 26,
    RTSP_MEDIA_H264 = 96,
    RTSP_MEDIA_AAC  = 37,
    RTSP_MEDIA_H265 = 265,
    RTSP_MEDIA_NONE
};

enum RtspFrameType
{
    RTSP_VIDEO_FRAME_I = 0x01,
    RTSP_VIDEO_FRAME_P = 0x02,
    RTSP_VIDEO_FRAME_B = 0x03,
    RTSP_AUDIO_FRAME   = 0x11,
};

typedef struct RtspServerMedia {
	unsigned char mMultiCast;
	char mSuffix[256];
	enum RtspMediaType mMediaType;
	unsigned int mChnId;//xop::channel_0 //xop::MAX_MEDIA_CHANNEL

	void (*mConnectCallback)(const char* peer_ip, uint16_t peer_port);
	void (*mDisConnectCallback)(const char* peer_ip, uint16_t peer_port);
}RtspServerMediaSt;

int RtspServerInit(uint16_t port, char *mAuthName, char *mAuthPasswd);
int RtspServerDeInit(void);
int RtspServerMediaInit(RtspServerMediaSt *pRtspServerMediaInfo);
int RtspServerMediaDeInit(RtspServerMediaSt *pRtspServerMediaInfo);
int RtspServerPushFrame(RtspServerMediaSt *pRtspServerMediaInfo, uint32_t bufCnt, uint8_t *frameBufList[], uint32_t frameSizeList[], enum RtspFrameType frameType);

#ifdef __cplusplus
}
#endif

#endif /*End of file*/
