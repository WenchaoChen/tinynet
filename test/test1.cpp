#include <stdio.h>
#include <iostream>

#include "tinynet.hpp"
using namespace tinynet::net;


template<int RecvBufSize=8192, int SendBufSize=8192>
class TcpSession : public EventHandler<RecvBufSize, SendBufSize>
{
public:
	virtual void on_connect() { trace; }
	virtual int read_packet(const char * pData , int dataLen) { if (dataLen < 12 ) { return -1; } return 12; }
	virtual void on_disconnect() { trace; }
	virtual void on_send(unsigned int len) { trace; }
	virtual void on_recv(const char* pBuf, unsigned int len) { trace; printf("received data %s \n",pBuf); }
};

int main(int argc, char** argv)
{
	TCPService<TcpSession<> ,SessionFactory<TcpSession<> >,true>	server;//创建服务
	server.config.threads  = 10; // 配置选项，还有更多
	server.addListener("0.0.0.0",  7788);// 创建服务器
	server.addConnector("127.0.0.1", 7788);//创建客户端
	server.start();
 
	char c = getchar()	; 
	while (c)
	{
		if (c == 'q')
		{
			break;
		}	
		c = getchar()	; 
	}
	
	server.stop();

	return 0;
}
