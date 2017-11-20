#include <stdio.h>
#include "tinynet.hpp"
#include <iostream>

using namespace tinynet::net;


template<int RecvBufSize=8192, int SendBufSize=8192>
class TcpSession : public EventHandler<RecvBufSize, SendBufSize,char>
{
    public:
        bool isPassive; 

        virtual void on_connect()
        {
            trace;
            if (isPassive)
            {
                printf("i am from listener\n");
            }
            else
            {
                printf("i am from connector\n");
            }
        }

        void send_packet()
        {
            for (int i =0;i < 10000; i ++)
            {
                //launch_send("*abcdefg", 8);	
            }
        }

        virtual int read_packet(const char * pData , int dataLen)
        {
            if (dataLen < 12 )
            {
                return -1; 
            }
            return 12; 
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
            trace;
            printf("received data %s \n",pBuf); 

            // 		std::string msg("");
            // 		msg.append(pBuf, len);

            if (isPassive)
            {
                printf("from listener: len:%d, msg:%s\n", len, pBuf);
            }
            else 
            {
                printf("from connector:len:%d, msg:%s\n", len, pBuf);
            }

        }
};

template <class T> 
class MySessionFactory
{
    public:
        typedef T Session ; 
        struct MsgPackage{
            T * session; 
            typename T::Message * message; 
        }; 


        T * create()
        {
            printf("create session\n");
            return new T(); 
        }
        void release(T * pT)
        {
            delete  pT;
        }

};

int main(int argc, char** argv)
{
    MySessionFactory<TcpSession<> > factory ; 
    TCPService<TcpSession<> ,MySessionFactory<TcpSession<> > >	server(factory);
    //TCPService<TcpSession<> ,MySessionFactory >	server(factory);

    server.config.threads  = 10;
    server.config.scheds   = 1;
    server.config.backlogs = 10;

    server.addListener("0.0.0.0",  7788);
    std::cout << "start server on port 7788 " << std::endl; 

    //server.addConnector("127.0.0.1", 7788);

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

