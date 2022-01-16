// RTSP Server

#include "xop/RtspServer.h"
#include "net/Timer.h"
#include <thread>
#include <memory>
#include <iostream>
#include <string>
#include "JPEGSource.h"

class JPEGFile
{
public:
	JPEGFile(int buf_size=500000);
	~JPEGFile();

	bool Open(const char *path);
	void Close();

	bool IsOpened() const
	{ return (m_file != NULL); }

	int ReadFrame(char* in_buf, int in_buf_size, bool* end);
    
private:
	FILE *m_file = NULL;
	unsigned char *m_buf = NULL;
	int  m_buf_size = 0;
	int  m_bytes_used = 0;
	int  m_count = 0;
};

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, JPEGFile* JPEG_file);

int main(int argc, char **argv)
{	
	if(argc != 2) {
		printf("Usage: %s test.JPEG \n", argv[0]);
		return 0;
	}

	JPEGFile JPEG_file;
	if(!JPEG_file.Open(argv[1])) {
		printf("Open %s failed.\n", argv[1]);
		return 0;
	}

	std::string suffix = "live";
	std::string ip = "127.0.0.1";
	std::string port = "1554";
	std::string rtsp_url = "rtsp://" + ip + ":" + port + "/" + suffix;
	
	std::shared_ptr<xop::EventLoop> event_loop(new xop::EventLoop());
	std::shared_ptr<xop::RtspServer> server = xop::RtspServer::Create(event_loop.get());

	if (!server->Start("0.0.0.0", atoi(port.c_str()))) {
		printf("RTSP Server listen on %s failed.\n", port.c_str());
		return 0;
	}

#ifdef AUTH_CONFIG
	server->SetAuthConfig("-_-", "admin", "12345");
#endif

	xop::MediaSession *session = xop::MediaSession::CreateNew("live"); 
	session->AddSource(xop::channel_0, xop::JPEGSource::CreateNew()); 
	//session->StartMulticast(); 
	session->AddNotifyConnectedCallback([] (xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port){
		printf("RTSP client connect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});
   
	session->AddNotifyDisconnectedCallback([](xop::MediaSessionId sessionId, std::string peer_ip, uint16_t peer_port) {
		printf("RTSP client disconnect, ip=%s, port=%hu \n", peer_ip.c_str(), peer_port);
	});

	xop::MediaSessionId session_id = server->AddSession(session);
         
	std::thread t1(SendFrameThread, server.get(), session_id, &JPEG_file);
	t1.detach(); 

	std::cout << "Play URL: " << rtsp_url << std::endl;

	while (1) {
		xop::Timer::Sleep(100);
	}

	getchar();
	return 0;
}

void SendFrameThread(xop::RtspServer* rtsp_server, xop::MediaSessionId session_id, JPEGFile* JPEG_file)
{       
	int buf_size = 2000000;
	std::unique_ptr<uint8_t> frame_buf(new uint8_t[buf_size]);

	while(1) {
		bool end_of_frame = false;
		int frame_size = JPEG_file->ReadFrame((char*)frame_buf.get(), buf_size, &end_of_frame);
		if(frame_size > 0) {
			xop::AVFrame videoFrame = {0};
			videoFrame.type = 0; 
			videoFrame.size = frame_size;
			videoFrame.timestamp = xop::JPEGSource::GetTimestamp();
			videoFrame.buffer.reset(new uint8_t[videoFrame.size]);    
			memcpy(videoFrame.buffer.get(), frame_buf.get(), videoFrame.size);
			rtsp_server->PushFrame(session_id, xop::channel_0, videoFrame);
		}
		else {
			break;
		}
      
		xop::Timer::Sleep(40); 
	};
}

JPEGFile::JPEGFile(int buf_size)
    : m_buf_size(buf_size)
{
	m_buf = new unsigned char[m_buf_size];
}

JPEGFile::~JPEGFile()
{
	delete [] m_buf;
}

bool JPEGFile::Open(const char *path)
{
	m_file = fopen(path, "rb");
	if(m_file == NULL) {      
		return false;
	}

	return true;
}

void JPEGFile::Close()
{
	if(m_file) {
		fclose(m_file);
		m_file = NULL;
		m_count = 0;
		m_bytes_used = 0;
	}
}

int JPEGFile::ReadFrame(char* in_buf, int in_buf_size, bool* end)
{
	if(m_file == NULL || m_buf == NULL) {
		return -1;
	}
	int i = 0;
	int bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
	if(bytes_read == 0) {
		fseek(m_file, 0, SEEK_SET); 
		m_count = 0;
		m_bytes_used = 0;
		bytes_read = (int)fread(m_buf, 1, m_buf_size, m_file);
		if(bytes_read == 0)         {            
			this->Close();
			printf("error read file return zero\n");
			return -1;
		}
	}
	//printf("bytes_read = %d\n", bytes_read);
#if 0
	//for pic
	int size = (bytes_read<=in_buf_size ? bytes_read : in_buf_size);
	memcpy(in_buf, m_buf, size); 
	fseek(m_file, 0, SEEK_SET);
	return bytes_read;
#else
	//for mjpeg
	bool is_find_start = false;
	bool is_find_end = false;
	unsigned int start_pos = 0;
	unsigned int end_pos = 0;
	*end = false;
	//printf("0x%x 0x%x\n", m_buf[0], m_buf[1]);
	for (i=0; i<bytes_read-1; i++) {
		unsigned char cur = m_buf[i+0];
		unsigned char next = m_buf[i+1];
		
		if(is_find_start == false && cur == 0xff && next == 0xd8){
			is_find_start = true;
			start_pos = i;
			//printf("start_pos = %d\n", start_pos);
			continue;
		}
		if(is_find_start == true && cur == 0xff && next == 0xd9){//EOI
			is_find_end = true;
			end_pos = i + 2;
			//printf("end_pos = %d\n", end_pos);
			break;
		}else if(is_find_start == true && cur == 0xff && next == 0xd8){//
			is_find_end = true;
			end_pos = i;
			//printf("end_pos = %d\n", end_pos);
			break;
		}
	}
	//printf("-----------------------\n");
	if(is_find_start && is_find_end){

	}

	if(is_find_start && !is_find_end) {        
		is_find_end = true;
		end_pos = i + 1;
		//*end = true;
		//printf("!!end_pos = %d\n", end_pos);
	}

	if(!is_find_start || !is_find_end) {
		m_count = 0;
		m_bytes_used = 0;
		printf("check your file, no start or end flag\n");
		this->Close();
		return -1;
	}

	int start_end_len = end_pos - start_pos;
	int size = (start_end_len<=in_buf_size ? start_end_len : in_buf_size);
	//printf("start_end_len = %d, size = %d\n", start_end_len, size);
	memcpy(in_buf, &m_buf[start_pos], size); 
	m_bytes_used += size;
	fseek(m_file, m_bytes_used, SEEK_SET);
	return size;
#endif
}


