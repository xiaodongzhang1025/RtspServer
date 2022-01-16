// ZXD
// 2022-1-15

#if defined(WIN32) || defined(_WIN32)
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#endif

#include "JPEGSource.h"
#include "JPEGParser.h"
#include <cstdio>
#include <chrono>
#if defined(__linux) || defined(__linux__)
#include <sys/time.h>
#endif

using namespace xop;
using namespace std;

JPEGSource::JPEGSource(uint32_t framerate)
	: framerate_(framerate)
{
    payload_    = 26; 
    media_type_ = JPEG;
    clock_rate_ = 90000;
}

JPEGSource* JPEGSource::CreateNew(uint32_t framerate)
{
    return new JPEGSource(framerate);
}

JPEGSource::~JPEGSource()
{

}

string JPEGSource::GetMediaDescription(uint16_t port)
{
    char buf[100] = {0};
    sprintf(buf, "m=video %hu RTP/AVP 26", port); // \r\nb=AS:2000
    return string(buf);
}

string JPEGSource::GetAttribute()
{
    return string("a=rtpmap:26 JPEG/90000");
}


typedef struct rtp_hdr {
        unsigned int version:2;   /* protocol version */
        unsigned int p:1;         /* padding flag */
        unsigned int x:1;         /* header extension flag */
        unsigned int cc:4;        /* CSRC count */
        unsigned int m:1;         /* marker bit */
        unsigned int pt:7;        /* payload type */
        unsigned short seq;      /* sequence number */
        unsigned int ts;               /* timestamp */
        unsigned int ssrc;             /* synchronization source */
        //unsigned int csrc[1];          /* optional CSRC list */
}rtp_hdr_t;
typedef struct jpeghdr {
        unsigned int tspec:8;   /* type-specific field */
        unsigned int off:24;    /* fragment byte offset */
        unsigned char type;            /* id of jpeg decoder params */
        unsigned char q;               /* quantization factor (or table id) */
        unsigned char width;           /* frame width in 8 pixel blocks */
        unsigned char height;          /* frame height in 8 pixel blocks */
}jpeghdr_t;
typedef struct jpeghdr_rst {
        unsigned short dri;
        unsigned int f:1;
        unsigned int l:1;
        unsigned int count:14;
}jpeghdr_rst_t;
typedef struct jpeghdr_qtable {
        unsigned char  mbz;
        unsigned char  precision;
        unsigned short length;
}jpeghdr_qtable_t;
#define RTP_JPEG_RESTART        0x40
#define RTP_PT_JPEG             26
#define PACKET_SIZE             (MAX_RTP_PAYLOAD_SIZE + RTP_HEADER_SIZE)

bool JPEGSource::HandleFrame(MediaChannelId channel_id, AVFrame frame)
{
    uint8_t* frame_buf  = frame.buffer.get();
    uint32_t frame_size = frame.size;

    if (frame.timestamp == 0) {
	    frame.timestamp = GetTimestamp();
    }    

    if (frame_size <= MAX_RTP_PAYLOAD_SIZE) {
        RtpPacket rtp_pkt;
	    rtp_pkt.type = frame.type;
	    rtp_pkt.timestamp = frame.timestamp;
	    rtp_pkt.size = frame_size + RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE;
	    rtp_pkt.last = 1;
        memcpy(rtp_pkt.data.get()+RTP_TCP_HEAD_SIZE+RTP_HEADER_SIZE, frame_buf, frame_size);

        if (send_frame_callback_) {
		    if (!send_frame_callback_(channel_id, rtp_pkt)) {
			    return false;
		    }               
        }
    }
    else {
        JPEGFrameParser frameParser;
        frameParser.parse(frame_buf, frame_size);
        unsigned short qTableLen = 0;
        unsigned int scanDataLen = 0;
        const unsigned char *qTableBuf = frameParser.quantizationTables(qTableLen);
        const unsigned char *scanDataBuf = frameParser.scandata(scanDataLen);
#if 0
        printf("frame_size = %d\n", frame_size);
        printf("sizeof(rtp_hdr_t) = %d\n", sizeof(rtp_hdr_t));
        printf("sizeof(jpeghdr_t) = %d\n", sizeof(jpeghdr_t));
        printf("sizeof(jpeghdr_rst_t) = %d\n", sizeof(jpeghdr_rst_t));
        printf("sizeof(jpeghdr_qtable_t) = %d\n", sizeof(jpeghdr_qtable_t));
        for(int i = 0; i < 0xc2; i++){
            printf("%02x ", frame_buf[i]);
            if((i+1)%16 == 0){
                printf("\n");
            }
        }
        printf("\n");
        printf("qTableBuf 0x%x, qTableLen = %d\n", qTableBuf, qTableLen);
        printf("scanDataBuf 0x%x, scanDataLen = %d\n", scanDataBuf, scanDataLen);
        printf("width = %d\n", frameParser.width());
        printf("height = %d\n", frameParser.height());
        printf("type = %d\n", frameParser.type());
        printf("precision = %d\n", frameParser.precision());
        printf("qFactor = %d\n", frameParser.qFactor());
        printf("restartInterval = %d\n", frameParser.restartInterval());
        printf("jpegheaderLength = %d\n", frameParser.jpegheaderLength());
        printf("driFound = %d\n", frameParser.driFound());
#endif
        if(frameParser.width() == 0 || frameParser.height() == 0 || scanDataLen == 0){
            printf("frame error ?????\n");
            return false;
        }
        static unsigned int start_seq = 0;
        start_seq++;
        int dri = frameParser.driFound();
        int jpegdata_pos = frameParser.jpegheaderLength();
        ///////////////
        unsigned char *ptr;
        unsigned char *packet_buf;
        uint8_t *jpeg_data;
        rtp_hdr_t rtphdr;
        struct jpeghdr jpghdr;
        struct jpeghdr_rst rsthdr;
        struct jpeghdr_qtable qtblhdr;
        int seq = 0;//start seq
        /* Initialize RTP header
         */
        rtphdr.version = 2;
        rtphdr.p = 0;
        rtphdr.x = 0;
        rtphdr.cc = 0;
        rtphdr.m = 0;
        rtphdr.pt = RTP_PT_JPEG;
        rtphdr.seq = start_seq;
        rtphdr.ts = frame.timestamp;
        rtphdr.ssrc = 0;
        /* Initialize JPEG header
         */
        jpghdr.tspec = 0;
        jpghdr.off = 0;
        jpghdr.type = frameParser.type() | ((dri != 0) ? RTP_JPEG_RESTART : 0);
        jpghdr.q = frameParser.qFactor();
        jpghdr.width = frameParser.width();
        jpghdr.height = frameParser.height();
        /* Initialize DRI header
         */
        if (dri != 0) {
                rsthdr.dri = dri;
                rsthdr.f = 1;        /* This code does not align Ris */
                rsthdr.l = 1;
                rsthdr.count = 0x3fff;
        }
        /* Initialize quantization table header
         */
        if (jpghdr.q >= 128) {
                qtblhdr.mbz = 0;
                qtblhdr.precision = 0; /* This code uses 8 bit tables only */
                qtblhdr.length = qTableLen;  /* 2 64-byte tables */
        }
        RtpPacket rtp_pkt;
        rtp_pkt.type = frame.type;
        rtp_pkt.timestamp = frame.timestamp;
        rtp_pkt.size = RTP_TCP_HEAD_SIZE + RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
        rtp_pkt.last = 0;

        //frame_size -= jpegdata_pos;//for jpeg ok, but not for mjpeg
        while (frame_size > 0) {
            //jpeg_data = frame_buf + jpegdata_pos;//for jpeg ok, but not for mjpeg
            jpeg_data = frame_buf;
            packet_buf = &rtp_pkt.data.get()[RTP_TCP_HEAD_SIZE + 0];
            ptr = packet_buf + RTP_HEADER_SIZE;

            //memcpy(ptr, &jpghdr, sizeof(jpghdr));//notice cpu endian
            ptr[0] = 0;
            ptr[1] = jpghdr.off>>16;
            ptr[2] = jpghdr.off>>8;
            ptr[3] = jpghdr.off>>0;
            ptr[4] = jpghdr.type;
            ptr[5] = jpghdr.q;
            ptr[6] = jpghdr.width;
            ptr[7] = jpghdr.height;
            ptr += sizeof(jpghdr);

            if (dri != 0) {
                memcpy(ptr, &rsthdr, sizeof(rsthdr));
                ptr += sizeof(rsthdr);
            }

            if (jpghdr.q >= 128 && jpghdr.off == 0) {
                //memcpy(ptr, &qtblhdr, sizeof(qtblhdr));//notice cpu endian
                ptr[0] = 0;
                ptr[1] = 0;
                ptr[2] = qTableLen>>8;
                ptr[3] = qTableLen&0xff;
                ptr += sizeof(qtblhdr);
                memcpy(ptr, qTableBuf, qTableLen);
                ptr += qTableLen;
            }

            int data_len = PACKET_SIZE - (ptr - packet_buf);
            if (data_len >= frame_size) {
                rtp_pkt.last = 1;
                data_len = frame_size;
                rtphdr.m = 1;
            }

            //memcpy(packet_buf, &rtphdr, RTP_HEADER_SIZE);//notice cpu endian
            memcpy(ptr, jpeg_data + jpghdr.off, data_len);
            //printf("data_len = %d, PACKET_SIZE = %d\n", data_len, PACKET_SIZE);
            if (send_frame_callback_) {
                if (!send_frame_callback_(channel_id, rtp_pkt)) {
                    return false;
                }              
            }

            jpghdr.off += data_len;
            frame_size -= data_len;
            rtphdr.seq++;
        }
    }

    return true;
}

uint32_t JPEGSource::GetTimestamp()
{
/* #if defined(__linux) || defined(__linux__)
    struct timeval tv = {0};
    gettimeofday(&tv, NULL);
    uint32_t ts = ((tv.tv_sec*1000)+((tv.tv_usec+500)/1000))*90; // 90: _clockRate/1000;
    return ts;
#else  */
    auto time_point = chrono::time_point_cast<chrono::microseconds>(chrono::steady_clock::now());
    return (uint32_t)((time_point.time_since_epoch().count() + 500) / 1000 * 90 );
//#endif
}
 