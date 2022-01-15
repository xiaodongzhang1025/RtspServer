// ZXD
// 2022-1-15

#ifndef XOP_JPEG_SOURCE_H
#define XOP_JPEG_SOURCE_H

#include "MediaSource.h"
#include "rtp.h"

namespace xop
{ 

class JPEGSource : public MediaSource
{
public:
	static JPEGSource* CreateNew(uint32_t framerate=25);
	~JPEGSource();

	void SetFramerate(uint32_t framerate)
	{ framerate_ = framerate; }

	uint32_t GetFramerate() const 
	{ return framerate_; }

	virtual std::string GetMediaDescription(uint16_t port); 

	virtual std::string GetAttribute(); 

	virtual bool HandleFrame(MediaChannelId channel_id, AVFrame frame);

	static uint32_t GetTimestamp();
	
private:
	JPEGSource(uint32_t framerate);

	uint32_t framerate_ = 25;
};
	
}

#endif



