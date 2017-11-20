//******************************************************************************
//	created:	7-2-2017    
//	filename: 	epollnet.h
//	author:		arthur
//	purpose:	linux epoll version 
//******************************************************************************

#ifndef __EPOLLNET_H__
#define __EPOLLNET_H__

#include <assert.h>
#include <list>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include "libs/object_pool.hpp"
#include "tinynet.hpp"
#include <vector>
#include <aio.h>
#include "libs/spscqueue.hpp"
#include "sendbuffer.hpp"


namespace tinynet
{
    namespace net
    {

        template<typename T> struct Environment; 
        template<typename T>
            struct Scheduler
            {
                int epollFd;
                int procFds[MAX_THREAD]; 
                unsigned int threads  =1; 

                void start()
                {
                    epollFd = epoll_create(10);  
                    if( -1 == epollFd ){
                        DLog("throw error %d",__LINE__); 	
                        throw errno;
                    }

                    int ret = fcntl (epollFd, F_SETFD, FD_CLOEXEC);
                    assert(ret != -1); 

                    //read,write epoll 
                    for(int i =0;i < MAX_THREAD; i ++)
                    {
                        procFds[i] = epoll_create(10); 
                        if ( -1 == procFds[i])
                        {
                            DLog("throw error %d",__LINE__); 	
                            throw errno ; 
                        }
                    }
                }

                void stop()
                {
                    ::close(epollFd); 
                    epollFd = 0xFFFFFF;
                    for(int i =0;i < MAX_THREAD; i ++)
                    {
                        ::close(procFds[i]); 
                        procFds[i] = 0xFFFFFF; 
                    }
                }

                void push(T* pSession)
                {
                    int procIdx = pSession->fd % threads; 
                    int procFd = procFds[procIdx];

                    pSession->procFd = procFd; 
                    struct epoll_event event;   // 告诉内核要监听什么事件
                    event.events   = EPOLLIN|EPOLLERR; // 表示对应的文件描述符可以读
                    event.data.ptr = pSession; 
                    int ret = epoll_ctl(procFd, EPOLL_CTL_ADD, pSession->fd, &event);
                    pSession->on_connect();
                }
            };

        //////////////////////////////////////////////////////////////////////////
        template<typename T,typename Factory ,typename Msg=char>
            class Processor
            {
                public:

                    //typedef std::shared_ptr<Factory>  FactoryPtr; 
                    typedef Factory *   FactoryPtr; 
                    Processor(){
                        isRunning = false; 
                        innerFactory = new Factory(); 
                        factory = innerFactory; 
                    }

                    Processor (FactoryPtr &fact):factory(fact) {
                        isRunning = false; 
                    }

                    virtual ~Processor() {
                        if (innerFactory)
                        {
                            delete innerFactory; 
                        }
                    }

                    int start(const ITCPService::Properties &cfg,EventHandleFunc handler = nullptr)
                    {
                        if (this->isRunning)
                        {
                            return 0; 
                        }
                        DLog("start processors "); 
                        this->isRunning = true; 
                        evtHandler = handler; 
                        config = cfg; 
                        int threads = config.threads > MAX_THREAD?MAX_THREAD:config.threads; 

                        Scheduler<T>& scheder = Environment<T>::scheder;
                        scheder.threads = threads; 
                        for (int i = 0;i < threads; ++i)
                        {
                            DLog("start proc  thread "); 
                            msgQueues.push_back( new MsgQueue()); 
                            procThreads.push_back(std::thread(&Processor<T,Factory,Msg>::run, this,i)); 
                        }

                        if (config.app_run)
                        {
                            appThread = std::thread(&Processor<T,Factory,Msg>::app_run, this); 
                        }
                        return 0; 
                    }

                    void stop()
                    {
                        DLog("stop process thread"); 
                        if (!isRunning)
                        {
                            return ; 
                        }
                        this->isRunning = false; 
                        Scheduler<T>& scheder = Environment<T>::scheder;

                        ::close(scheder.epollFd); 

                        for(auto &thrd : procThreads)
                        {
                            thrd.join(); 
                        }

                        procThreads.clear(); 

                        for(auto &queue : msgQueues) 
                        {
                            delete queue; 
                        }
                        msgQueues.clear(); 
                        appThread.join(); 
                    }

                    void * app_run()
                    {
                        DLog("start app thread"); 
                        while(isRunning)
                        {
                            for(auto &queue : msgQueues) 
                            {
                                if (!queue->isEmpty())
                                {
                                    typename Factory::MsgPackage * pMsgPkg  = nullptr; 
                                    bool result = queue->try_dequeue(pMsgPkg); 
                                    if (result && pMsgPkg)
                                    {
                                        pMsgPkg->session->on_message(pMsgPkg->message); 
                                        msgPool.release(pMsgPkg); 
                                    }
                                    else 
                                    {
                                        Log("dequeue failed"); 
                                    }
                                }
                                else 
                                {
                                    usleep(1); 
                                }
                            }

                        }
                    }

                    void * run(int procIdx)
                    {
                        Scheduler<T>& scheder = Environment<T>::scheder;
                        int procFd = scheder.procFds[procIdx]; 

                        struct epoll_event waitEvents[MAX_WAIT_EVENT]; 
                        while (isRunning)
                        {
                            DLog("process in thread %d",procIdx); 
                            int ret = epoll_wait(procFd, waitEvents, MAX_WAIT_EVENT, -1);
                            if (ret < 0 )
                            {
                                ELog ("wait failed "); 
                                assert(false); 
                                continue; 
                            }
                            DLog("epoll wait result %d",ret); 

                            for(int  i =0; i < ret ; i++)
                            {
                                T * pSession = (T*)waitEvents[i].data.ptr; 
                                if (pSession == nullptr)
                                {
                                    DLog("empty session "); 
                                    continue; 
                                }

                                if (EPOLLIN == (waitEvents[i].events & EPOLLIN ))
                                {
                                    DLog ("epoll in event "); 
                                    int len = recv(pSession->fd, (char*)pSession->recvBuf+ pSession->recvBufPos, 
                                            pSession->recv_size(), 0); 
                                    if(len < 0)
                                    {
                                        int ret = epoll_ctl(procFd, EPOLL_CTL_DEL, pSession->fd, NULL);
                                        if(errno == ECONNRESET)//tcp连接超时、RST
                                        {
                                            ELog("connect timeoue");
                                        }
                                        else
                                        {
                                            ELog("read error:");
                                        }
                                        ::close(pSession->fd); 
                                        pSession->on_disconnect(); 
                                        factory->release(pSession); 
                                        if (evtHandler )
                                        {
                                            evtHandler(EVT_SESSION_RELEASE,pSession); 
                                        }
                                    }
                                    else if(len == 0)//client close 
                                    {
                                        int ret = epoll_ctl(procFd, EPOLL_CTL_DEL, pSession->fd, NULL);
                                        DLog("client request closed "); 
                                        ::close(pSession->fd);
                                        pSession->on_disconnect(); 
                                        factory->release(pSession); 

                                        if (evtHandler )
                                        {
                                            evtHandler(EVT_SESSION_RELEASE,pSession); 
                                        }
                                    }
                                    else //normal data 
                                    {
                                        pSession->recvBufPos += len; 
                                        unsigned int readPos =0; 
                                        int pkgLen = pSession->read_packet(pSession->recvBuf,pSession->recvBufPos); 
                                        while (pkgLen >0 )
                                        {						
                                            if (readPos + pkgLen <= pSession->recvBufPos )
                                            {									
                                                char * pEnd = (char*)(pSession->recvBuf)+readPos+ pkgLen;
                                                //char endVal = *pEnd; 
                                                //*pEnd = 0; 									 
                                                pSession->on_recv((char*)(pSession->recvBuf)+readPos, pkgLen);		

                                                if (config.app_run)
                                                {
                                                    const Msg * pMsg =  pSession->parse_message((char*)(pSession->recvBuf)+readPos, pkgLen); 
                                                    MsgQueue * pMsgQueue = msgQueues[procIdx]; 
                                                    typename Factory::MsgPackage *pMsgPkg = msgPool.acquire();; 
                                                    pMsgPkg->session  = pSession; 
                                                    pMsgPkg->message = (Msg*)pMsg; 
                                                    pMsgQueue->enqueue(pMsgPkg); 
                                                }
                                                else 
                                                {
                                                    if (config.parser)
                                                    {
                                                        const Msg * pMsg =  pSession->parse_message((char*)(pSession->recvBuf)+readPos, pkgLen); 
                                                        pSession->on_message(pMsg); 
                                                    }
                                                }

                                                //*pEnd=endVal; 
                                                readPos += pkgLen; 
                                            }

                                            if (readPos  < pSession->recvBufPos)
                                            {
                                                pkgLen = pSession->read_packet(pSession->recvBuf + readPos, pSession->recvBufPos - readPos); 
                                                if (pkgLen <=0)
                                                {
                                                    memmove(pSession->recvBuf,(char*)pSession->recvBuf + readPos, pSession->recvBufPos - readPos);											
                                                    pSession->recvBufPos -= readPos;
                                                    *(pSession->recvBuf +pSession->recvBufPos )=0;
                                                    break; 
                                                }
                                            }
                                            else 
                                            {
                                                pSession->recvBufPos = 0; 
                                                break; 
                                            }					 
                                        }

                                    }

                                }
                                else if (EPOLLOUT == (waitEvents[i].events & EPOLLOUT))
                                {

                                    pSession->sendBuffer.pop([&](const char * pData,unsigned int dataLen){

                                            return ::send(pSession->fd, pData , dataLen,0); 


                                            }); 

                                    {
                                        //std::atomic_thread_fence(std::memory_order_consume);
                                        //dataLen = pSession->sndBufTail - pSession->sndBufHead; 

                                        //ssize_t sntLen = ::send(pSession->fd,
                                        //        (char*)pSession->sendBuf+pSession->sndBufHead,
                                        //        dataLen,0); 
                                        //if (sntLen > 0)
                                        //{
                                        //    std::atomic_thread_fence(std::memory_order_acq_rel);
                                        //    pSession->sndBufHead += sntLen; 
                                        //}
                                    }

                                    if ( errno == EAGAIN ||  errno == EWOULDBLOCK )
                                    {
                                        struct epoll_event event;
                                        event.events = EPOLLIN|EPOLLOUT|EPOLLERR;
                                        event.data.ptr = pSession;
                                        epoll_ctl(pSession->procFd, EPOLL_CTL_MOD,pSession->fd , &event); 
                                        continue; 
                                    }


                                    struct epoll_event event;
                                    event.events = EPOLLIN|EPOLLERR;
                                    event.data.ptr = pSession;
                                    epoll_ctl(pSession->procFd, EPOLL_CTL_MOD,pSession->fd , &event); 

                                }
                                else if (EPOLLERR == (waitEvents[i].events & EPOLLERR ))
                                {
                                    DLog ("epoll error event "); 
                                    int ret = epoll_ctl(procFd, EPOLL_CTL_DEL, pSession->fd, NULL);
                                    ::close(pSession->fd); 
                                    DLog("EPOLLERROR event %d ",waitEvents[i].events); 
                                    factory->release(pSession); 
                                }
                                else 
                                {
                                    DLog ("epoll other event "); 
                                    int ret = epoll_ctl(procFd, EPOLL_CTL_DEL, pSession->fd, NULL);
                                    if (ret)
                                    {
                                    }
                                }

                            }


                        } //end while 

                        return 0;
                    }


                    FactoryPtr innerFactory; 
                    FactoryPtr factory; 
                private:
                    std::atomic<bool> isRunning ; 
                    std::vector<std::thread> procThreads; 
                    typedef utils::spscqueue<typename Factory::MsgPackage *>  MsgQueue; 
                    std::vector< MsgQueue * > msgQueues; 
                    typedef ObjectPool<typename Factory::MsgPackage >  MsgPool; 
                    MsgPool msgPool; 
                    ITCPService::Properties config ; 
                    std::thread appThread; 
                    EventHandleFunc  evtHandler; 

            };


        //////////////////////////////////////////////////////////////////////////	

        template<typename T>
            struct Environment
            {
                static Scheduler<T> scheder;
            };

        template<typename T>
            Scheduler<T> Environment<T>::scheder;


        //////////////////////////////////////////////////////////////////////////
        template<typename T,typename Factory>
            struct Listener
            {
                int ip;
                int port;
                int backlogs;
                void * context;
                int lisFd = -1;
                std::atomic<bool> isRunning; 
                std::thread runThread; 
                EventHandleFunc  evtHandler ; 

                Processor<T,Factory,typename T::Message> &processor; 

                Listener( Processor<T,Factory,typename T::Message> &procor):processor(procor)
                {
                    isRunning = false; 
                }

                int start(const ITCPService::Properties &config ,EventHandleFunc handler = nullptr )
                {
                    evtHandler = handler; 
                    isRunning = true; 
                    try
                    {
                        lisFd = socket(AF_INET, SOCK_STREAM, 0);
                        if (-1 == lisFd)
                        {
                            DLog("throw error %d",__LINE__); 	
                            //throw errno;
                            return RET_ERR_SOCKET; 
                        }

                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_addr.s_addr = ip;
                        addr.sin_port = htons(port);

                        if (config.reuse)
                        {
                            int enable = 1;
                            if (setsockopt(lisFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
                            {
                                ELog("setsockopt(SO_REUSEADDR) failed");
                            }
                        }

                        setSockNonBlock(lisFd); 

                        if (-1 == bind(lisFd, (struct sockaddr *)&addr, sizeof(addr)))
                        {
                            DLog("bind address error "); 
                            ::close(lisFd); 
                            return RET_ERR_BIND; 
                        }

                        if (-1 == listen(lisFd, backlogs))
                        {
                            DLog("throw error %d",__LINE__); 	
                            //throw errno;
                            close(lisFd); 
                            return RET_ERR_LISTEN; 
                        }

                        DLog("start listen thread "); 
                        runThread = std::thread(&Listener<T,Factory>::run,this); 

                    }
                    catch(...)
                    {
                        if (-1 != lisFd)
                        {
                            ::close(lisFd);  
                            lisFd = -1; 
                        }
                        //trace ; 
                    }
                    return 0; 
                }

                void stop()
                {
                    DLog("stop listener thread"); 
                    if (!isRunning)
                    {
                        isRunning= false; 
                    }
                    Scheduler<T>& scheder = Environment<T>::scheder;
                    ::close(this->lisFd); 
                    ::close(scheder.epollFd); 
                    runThread.join();
                }

                static void setSockNonBlock(int sock)
                {
                    int opts = fcntl(sock,F_GETFL,0);
                    if(opts<0)
                    {
                        ELog("fcntl(sock,GETFL)");
                        return ; 
                    }
                    if(fcntl(sock,F_SETFL,opts|O_NONBLOCK)<0)
                    {
                        ELog("fcntl(sock,SETFL,opts)");
                    }  
                }

                void *  run()
                {
                    Scheduler<T>& scheder = Environment<T>::scheder;
                    int efd    = scheder.epollFd;
                    struct epoll_event event;   
                    event.data.fd = lisFd;     
                    event.events  = EPOLLIN; 

                    //register listen fd to epoll fd 
                    int ret = epoll_ctl(efd, EPOLL_CTL_ADD, lisFd, &event);
                    if(-1 == ret){
                        //trace; 
                        throw errno; 
                    }

                    struct sockaddr_in addr;
                    struct epoll_event waitEvents[MAX_WAIT_EVENT]; 
                    while (isRunning)
                    {
                        DLog("listener epoll wait"); 
                        //wait untill events  
                        int ret = epoll_wait(efd, waitEvents, MAX_WAIT_EVENT, -1);
                        if (ret < 0)
                        {
                            ELog("wait error "); 
                            return  nullptr; 
                        }

                        for (int i =0;i < ret ; i++)
                        {
                            //new connection, event fd will be equal listen fd 
                            if(( lisFd == waitEvents[i].data.fd )
                                    && ( EPOLLIN == waitEvents[i].events & EPOLLIN ) )
                            {
                                struct sockaddr_in lisAddr;
                                int addrlen = sizeof(lisAddr);
                                //get new fd 
                                int newFd = accept(lisFd, (struct sockaddr *)&lisAddr, (socklen_t*)&addrlen);

                                if (newFd < 0)
                                {
                                    if ( (errno == EAGAIN) || (errno == EWOULDBLOCK) ) {
                                        //non-blocking  mode has no connection
                                        continue;
                                    } else {
                                        ELog("accept failed");
                                        throw errno ; 
                                    }
                                }

                                setSockNonBlock(newFd); 

                                T * pSession = processor.factory->create(); 
                                if (pSession == nullptr)
                                {
                                    if (evtHandler )
                                    {
                                        void * rst = evtHandler(EVT_SESSION_CREATE,pSession); 
                                        if (rst != nullptr)
                                        {
                                            pSession = (T*) rst; 
                                        }
                                    }
                                }

                                assert(pSession != nullptr);

                                pSession->fd        = newFd;
                                pSession->ip        = addr.sin_addr.s_addr;
                                pSession->port      = ntohs(addr.sin_port);
                                pSession->context   = this;
                                pSession->isPassive = true;
                                scheder.push(pSession);	

                                //int nRecvBuf=128*1024;// 设置为 32K  
                                //    setsockopt(connfd, SOL_SOCKET,SO_RCVBUF,(const char*)&nRecvBuf,sizeof(int));  
                                // 发送缓冲区  
                                //int nSendBuf=128*1024;// 设置为 32K  
                                //  setsockopt(connfd, SOL_SOCKET,SO_SNDBUF,(const char*)&nSendBuf,sizeof(int));  

                                //int nNetTimeout=1000;//1 秒  
                                // 发送时限  
                                //setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&nNetTimeout, sizeof(int));  

                                // 接收时限  
                                //setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&nNetTimeout, sizeof(int));  
                            }
                        }
                    }

                    DLog("quiting listen thread "); 
                    return nullptr;
                }
            };

        //////////////////////////////////////////////////////////////////////////

        template <typename T,typename Factory>
            struct Connector
            {
                int ip;
                int port;
                int fd;
                T * session = nullptr; 
                EventHandleFunc evtHandler; 
                Processor<T,Factory,typename T::Message> &processor; 
                Connector(Processor<T,Factory,typename T::Message> &procor): processor(procor)
                {
                }

                int start(EventHandleFunc handler = nullptr )
                {
                    evtHandler =  handler ; 
                    return connect();
                }

                void stop()
                {
                    ::close(fd); 
                }

                int connect()
                {
                    //struct sockaddr_in sa;
                    char addrStr[INET_ADDRSTRLEN] = "";
                    inet_ntop(AF_INET, &ip, addrStr, INET_ADDRSTRLEN);

                    DLog("start connect to server %s, %d",addrStr,port); 
                    Scheduler<T>& scheder = Environment<T>::scheder;

                    fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (-1 == fd)
                    {
                        DLog("create socket error %d",__LINE__); 	
                        return RET_ERR_SOCKET; 
                    }

                    struct sockaddr_in addr;
                    int addrlen = sizeof(addr);
                    addr.sin_family = AF_INET;
                    addr.sin_addr.s_addr = ip/*inet_addr(szIp)*/;
                    addr.sin_port = htons(port);

                    if (-1 == ::connect(fd, (sockaddr*)&addr, addrlen))
                    {				
                        DLog("connect to server error %d",__LINE__); 	
                        evtHandler(EVT_CONNECT_FAIL,NULL); 
                        return RET_ERR_CONNECT; 
                    }

                    session = processor.factory->create(); 

                    if (session == nullptr)
                    {
                        if (evtHandler != nullptr )
                        {
                            void * rst =  evtHandler(EVT_SESSION_CREATE,session); 
                            if (rst != nullptr)
                            {
                                session  = (T*) rst; 
                            }
                        }
                    }

                    assert(session != NULL);

                    session->fd   = fd;
                    session->ip   = addr.sin_addr.s_addr;
                    session->port = ntohs(addr.sin_port);
                    session->context  = this;
                    session->isPassive = false;
                    scheder.push(session);	
                    return RET_OK; 
                }

            };

        //////////////////////////////////////////////////////////////////////////
        template<int RecvBufSize=8192, int SendBufSize=8192,typename Msg = char  >
            struct EventHandler : public IEventHandler, public IConnection
        {
            std::string id; 
            typedef Msg Message; 
            int procFd = -1; 
            int fd     = -1;
            void*		  context;
            int			  ip;
            int			  port;
            bool		  isPassive;
            std::atomic<int> sndBufHead; 
            std::atomic<int> sndBufTail; 
            //char		  sendBuf[SendBufSize];
            SendBuffer<SendBufSize> sendBuffer; 
            char		  recvBuf[RecvBufSize];
            int			  recvBufPos ; 
            EventHandler():recvBufPos(0)
            {
                sndBufHead = 0; 
                sndBufTail = 0; 
            }

            virtual ~EventHandler() { }
            int async_send(const char *pData,unsigned int dataLen)
            {
                DLog("async send data %d",dataLen); 
                bool ret = sendBuffer.push(pData,dataLen); 
                struct epoll_event event;
                event.events = EPOLLIN|EPOLLOUT|EPOLLERR;
                event.data.ptr = this;
                epoll_ctl(procFd, EPOLL_CTL_MOD,fd , &event); 
                return ret ?RET_OK:RET_ERR; 
            } 

            /*
               int async_send(const char *pData,unsigned int dataLen)
               {
               std::atomic_thread_fence(std::memory_order_consume);

               while(dataLen > SendBufSize - sndBufTail)
               {
               DLog("no more space "); 
               if (sndBufHead >0)
               {
               unsigned int dataSize = sndBufTail - sndBufHead; 
               memmove(sendBuf,(char*)sendBuf+ sndBufHead ,dataSize); 
               DLog("start memory move size %d ",dataSize); 

               std::atomic_thread_fence(std::memory_order_acq_rel);
               sndBufHead = 0 ; 
               sndBufTail = dataSize; 
               }
               else 
               {
               DLog("send failed ,tail is %d",sndBufTail.load()); 
               return RET_ERR; 
               }
               }

               if (dataLen < SendBufSize - sndBufTail)
               {
               char * pCurTail = (char*)sendBuf + sndBufTail; 
               std::atomic_thread_fence(std::memory_order_acq_rel);
               sndBufTail += dataLen; 
               memcpy(pCurTail,pData,dataLen); 
               }
               else 
               {
               DLog("message is to large failed"); 
               return RET_ERR_BUFSIZE;  
               }


               struct epoll_event event;
               event.events = EPOLLOUT|EPOLLERR;
               event.data.ptr = this;
               return epoll_ctl(procFd, EPOLL_CTL_MOD,fd , &event); 
               }
               */

            int send(const char* pData, unsigned int dataLen)
            {
                DLog("send data length is %d",dataLen);
                ssize_t sntLen = ::send(fd,pData,dataLen,0);

                if (sntLen > 0 && sntLen < dataLen )
                {

                    if ( errno == EAGAIN ||  errno == EWOULDBLOCK )
                    {

                    }

                }
                assert(sntLen == dataLen);

                this->on_send(pData+sntLen , sntLen);
                return sntLen;
            }


            virtual void launch_recv(/*char* pBuf, */unsigned int bufLen = 512)
            {

            }

            int recv_size()
            { 
                return RecvBufSize - recvBufPos; 
            }

            virtual int read_packet(const char * pData , int dataLen)
            {
                return dataLen; 
            }


            virtual void close()
            {
                ::close(fd); 
            }

            virtual void on_connect()
            {

            }

            virtual void on_disconnect()
            {
            }

            virtual const Msg * parse_message(const char *pData, unsigned int len)
            {
                return (const Msg*)pData; 
            }

            virtual void on_message(const Msg * )
            {

            }
            virtual void on_recv(unsigned int len)
            {
            }

            virtual void on_recv(const char * pData, unsigned int len)
            {
            }

            virtual void on_send(const char* pBuf, unsigned int len)
            {
            }

        };

        //////////////////////////////////////////////////////////////////////////
        template<typename T, typename Factory  = SessionFactory<T>>
            struct TCPService : public ITCPService
        {
            //typedef std::shared_ptr<Factory>  FactoryPtr; 
            typedef Factory*  FactoryPtr; 
            typedef std::list<Listener<T,Factory>* > ListenerList; 
            typedef std::list<Connector<T,Factory>* > ConnectorList; 
            ListenerList lstListener;
            ConnectorList lstConnector;
            Processor<T,Factory,typename T::Message> processor; 
            EventHandleFunc  evtHandler; 

            TCPService(FactoryPtr  factory= nullptr):processor(factory) { 

            }

            int start(EventHandleFunc handler = nullptr)
            {
                evtHandler = handler; 
                if (config.backlogs <= 0)
                {
                    config.backlogs = 10;
                }
                if (config.threads <= 0)
                {
                    config.threads = 1;
                }

                Environment<T>::scheder.start();

                //start processor 
                processor.start(config,evtHandler);

                for(auto &lis : lstListener)
                {
                    int ret  = lis->start(config,evtHandler); 
                    if (evtHandler)
                    {
                        evtHandler(ret == 0 ? EVT_LISTEN:EVT_LISTEN_FAIL ,NULL); 
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 

                for(auto &conn :  lstConnector)
                {
                    int ret = conn->start(evtHandler); 
                    if (evtHandler)
                    {
                        evtHandler(ret == 0 ? EVT_CONNECT:EVT_CONNECT_FAIL,NULL); 
                    }
                }
                return 0; 
            }

            void stop()
            {
                for(auto &lis : lstListener)
                {
                    lis->stop(); 
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(1)); 

                for(auto &conn : lstConnector)
                {
                    conn->stop(); 
                }
            }

            void addListener(const char* pIp, int port)
            {
                auto  listener = new Listener<T,Factory>(processor);
                assert(listener != NULL);

                listener->ip = inet_addr(pIp);
                listener->port = port;
                listener->backlogs = config.backlogs;
                listener->context = this;
                lstListener.push_back(listener);
            }

            void addConnector(const char* pIp, int port)
            {
                auto  connector = new Connector<T,Factory>(processor);
                assert(connector != NULL);
                connector->ip = inet_addr(pIp);
                connector->port = port;
                lstConnector.push_back(connector);
            }

        };

    }
    //TODO 
    //1.add event queue to close socket 
    //2.combine send and async_send using event queue 
}

#endif //__EPOLLNET_H__

