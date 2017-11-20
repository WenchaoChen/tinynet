#include <stdio.h>
#include "tinynet.hpp"
#include <iostream>

using namespace tinynet::net;

template<int RecvBufSize=48000, int SendBufSize=48000>
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
         //   trace;
        }

        virtual void on_send(unsigned int len)
        {
        //    trace;
        }

        virtual void on_message(const char* )
        {

        }
        virtual void on_recv(const char* pBuf, unsigned int len)
        {
            //trace;
            DLog("received data %s ",pBuf); 

            // 		std::string msg("");
            // 		msg.append(pBuf, len);
            
            //std::string msg(pBuf,len); 
            //this->send(pBuf,len); 
            this->async_send(pBuf,len); 

        }
};

class MySessionFactory
{
    public:

        typedef TcpSession<> Session; 
        struct MsgPackage{
            Session * session; 
            typename Session::Message * message; 
        }; 

        Session * create()
        {
            DLog("create session\n ");
            return new Session(); 
        }
        void release(Session * pT)
        {
            delete  pT;
        }

};

int main(int argc, char** argv)
{
    DLog("init main"); 

    //std::shared_ptr<MySessionFactory>  factory(new MySessionFactory()); 
    MySessionFactory factory; 
    TCPService<TcpSession<> ,MySessionFactory >	server(&factory);
    //TCPService<TcpSession<> ,MySessionFactory >	server(factory);

    server.config.threads  = 1;
    server.config.scheds   = 1;
    server.config.backlogs = 10;

    server.addListener("0.0.0.0",  7788);
    DLog ("start server on port 7788 " ); 

//    server.addConnector("127.0.0.1", 7788);

    server.start([](NetEvent evt, void * ){
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
				default:
				;
                }
                DLog(" %d ",evt); 
                return nullptr; 
            });

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

