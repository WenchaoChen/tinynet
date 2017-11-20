#include <stdio.h>
#include "tinynet.hpp"
#include <iostream>

using namespace tinynet::net;

template<int RecvBufSize=8192, int SendBufSize=64>
class TcpSession : public EventHandler<RecvBufSize, SendBufSize,char>
{
    public:
        bool isPassive; 

        virtual void on_connect()
        {
            //trace;
            if (isPassive)
            {
                DLog("i am from listener");
            }
            else
            {
                DLog("i am from connector");
            }


			std::string msg("hello world"); 
			this->async_send(msg.c_str(),msg.length()); 
        }

        void send_packet()
        {
            
        }

        virtual int read_packet(const char * pData , int dataLen)
        {
            return dataLen; 
        }

        virtual void on_disconnect()
        {
            trace;
        }

        virtual void on_send(unsigned int len)
        {
            trace;
        }

        virtual void on_message(const char* )
        {

        }
        virtual void on_recv(const char* pBuf, unsigned int len)
        {
        //    trace;
            DLog("received data %s ",pBuf); 

            // 		std::string msg("");
            // 		msg.append(pBuf, len);
            
            std::string msg(pBuf,len); 
            this->async_send(msg.c_str(),len); 

            if (isPassive)
            {
                DLog("from listener: len:%d, msg:%s", len, pBuf);
            }
            else 
            {
                DLog("from connector:len:%d, msg:%s", len, pBuf);
            }

        }
};
void * netHandler(NetEvent evt, void *pData)
{
    switch(evt)
    {
	case EVT_CONNECT:
	    break; 
	case EVT_CONNECT_FAIL:
	    break; 
	case EVT_LISTEN:
	    break; 
	case EVT_LISTEN_FAIL:

	    break; 
	case EVT_SESSION_CREATE:
	    break; 
	case EVT_SESSION_RELEASE:
	    break; 
	default:
	    ;
    }

    return NULL; 
}

int main(int argc, char** argv)
{
    DLog("init main"); 

    TCPService<TcpSession<>  >	server;

    server.config.threads  = 10;
    server.config.scheds   = 1;
    server.config.backlogs = 10;

    //server.addListener("0.0.0.0",  7788);
    //DLog ("start server on port 7788 " ); 

    //server.addConnector("192.168.8.108", 7788);
    server.addConnector("127.0.0.1", 7788);

    server.start(netHandler);

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

