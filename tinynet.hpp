//******************************************************************************
//	created:	7-2-2017    
//	filename: 	tinynet.h
//	author:		arthur
//	purpose:	
//******************************************************************************

#ifndef __TINYNET_H__
#define __TINYNET_H__

#include <functional>
#ifdef TINYLOG
#include "tinylog.hpp"
#else 
#include "detail/logger.hpp"
#endif //
#include "detail/plat.h"
#include <string>
namespace tinynet
{
    namespace net
    {
        enum NetRet{

            RET_OK = 0, 
            RET_ERR = -1,
            RET_ERR_SOCKET = -100,
            RET_ERR_CONNECT = -101,
            RET_ERR_LISTEN =  -102,
            RET_ERR_BIND    = -103,
            RET_ERR_BUFSIZE = -104,
        }; 
        enum NetEvent
        {
            EVT_BIND,
            EVT_LISTEN,
            EVT_LISTEN_FAIL,
            EVT_CONNECT,
            EVT_CONNECT_FAIL,
            EVT_RECV,
            EVT_SEND,
            EVT_DISCONNECT,
            EVT_SESSION_CREATE,
            EVT_SESSION_RELEASE
        };
       
        typedef std::function<void *  (NetEvent evt,void*)>   EventHandleFunc; 

        template <class T> 
            class SessionFactory  
            {
                public:
                    typedef T Session ; 
                    struct MsgPackage{
                        T * session; 
                        typename T::Message * message; 
                    }; 


                    T * create()
                    {
                        return new T(); 
                    }
                    void release(T * pT)
                    {
                        delete  pT;
                    }

            };

        template<int , int , typename >
            struct EventHandler ; 


        struct IEventHandler
        {
            virtual ~IEventHandler() {}
            virtual int  read_packet(const char * pData , int dataLen) = 0; 
            virtual void on_connect()     = 0;
            virtual void on_disconnect()  = 0;
            virtual void on_send(unsigned int len) = 0;
            virtual void on_recv(const char* pBuf, unsigned int len) = 0;
        };

        struct IConnection
        {
            virtual ~IConnection() {}

            virtual int send(const char* pData, unsigned int dataLen) = 0;
            virtual void launch_recv(/*char* pBuf, */unsigned int bufLen = 512) = 0;
            virtual void close() = 0;
        };

        struct IListener
        {
            virtual ~IListener() {}
            virtual void listen(const char* pIp, int port) = 0;
        };

        struct IConnector
        {
            virtual ~IConnector() {}
            virtual void connect(const char* pIp, int port) = 0;
        };

        struct ITCPService
        {
            virtual ~ITCPService()  {}

            virtual int start(EventHandleFunc handle = nullptr) = 0; 
            virtual void stop()  = 0;

            // should be called before the start function
            virtual void addListener(const char* pIp, int port) = 0;
            virtual void addConnector(const char* pIp, int port) = 0;

            struct Properties 
            {
                Properties()
                {
                    parser   = false; 
                    reuse    = true; 
                    app_run  = false; 
                    backlogs = 0; 
                    scheds   = 1; 
                    threads  = 1; 
                    ptr      = NULL; 
                }

                bool parser; 
                bool reuse ; 
                bool app_run; 
                int backlogs;   // listening queue size
                int scheds;     // iocp/epoll concurrency threads number
                int threads;    // iocp/epoll worker threads number
                void* ptr;    

            } config;
        };
    }
}

#ifdef _WIN32
#include "detail/iocpnet.hpp"
#elif  __APPLE__ 
#include "detail/kqueuenet.hpp"
#elif __linux__
#include "detail/epollnet.hpp"
#endif 

#endif //__TINYNET_H__

