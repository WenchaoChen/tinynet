//******************************************************************************
//	created:	7-2-2017    
//	filename: 	kqueuenet.h
//	author:		arthur
//	purpose:	mac kqueue version 
//******************************************************************************

#ifndef __KQUEUE_H__
#define __KQUEUE_H__

#include <assert.h>
#include <list>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <atomic>
#include "libs/object_pool.hpp"
#include "tinynet.hpp"
#include <vector>
#include "libs/spscqueue.hpp"

namespace tinynet
{
	namespace net
	{

#define MAX_BUFFER_SIZE  2048*64
		const int kReadEvent = 1;
		const int kWriteEvent = 2;

		template<typename T> struct Environment; 


		void updateEvents(int efd, int fd , void * pSession, int events, bool modify) {

			struct kevent ev[2] = {0};
			int n = 0;
			if (events & kReadEvent) {
				EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)pSession);
			} else if (modify){
				EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, (void*)(intptr_t)pSession);
			}
			if (events & kWriteEvent) {
				EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)pSession);
			} else if (modify){
				EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*)(intptr_t)pSession);
			}
			DLog("%s fd %d events read %d write %d\n",
					modify ? "mod" : "add", fd, events & kReadEvent, events & kWriteEvent);
			int ret = kevent(efd, ev, n, NULL, 0, NULL);
			if (ret < 0)
			{
				ELog("kevent failed\n"); 
			}
		}

		void updateEvents(int efd, int fd, int events, bool modify) {
			struct kevent ev[2] = {0};
			int n = 0;
			if (events & kReadEvent) {
				EV_SET(&ev[n++], fd, EVFILT_READ, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
			} else if (modify){
				EV_SET(&ev[n++], fd, EVFILT_READ, EV_DELETE, 0, 0, (void*)(intptr_t)fd);
			}
			if (events & kWriteEvent) {
				EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_ADD|EV_ENABLE, 0, 0, (void*)(intptr_t)fd);
			} else if (modify){
				EV_SET(&ev[n++], fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void*)(intptr_t)fd);
			}
			DLog("%s fd %d events read %d write %d\n",
					modify ? "mod" : "add", fd, events & kReadEvent, events & kWriteEvent);
			int ret = kevent(efd, ev, n, NULL, 0, NULL);
			if (ret < 0)
			{
				ELog("kevent failed\n"); 
			}
		}

		template<typename T>
			struct Scheduler
			{
				int scheds;
				int kqueueFd;
				int threads ; 
				int procFds[MAX_THREAD]; 

				int start()
				{
					kqueueFd = kqueue();
					if (-1 == kqueueFd)
					{
						DLog("create kqueue failed\n"); 
						throw errno;
					}

					//read,write epoll 
					for(int i =0;i < MAX_THREAD; i ++)
					{
						procFds[i] = kqueue(); 
						if ( -1 == procFds[i])
						{
							DLog("throw error %d\n",__LINE__); 	
							return -1; 
						}
					}
					DLog("create kqueue success\n");
					return 0; 
				}

				void stop()
				{
					DLog("close kqueue success\n");
					close(kqueueFd); 
					kqueueFd = 0xFFFFFF;

					for(int i =0;i < MAX_THREAD; i ++)
					{
						close(procFds[i]); 
						procFds[i] = 0xFFFFFF; 
					}
				}

				void push(T* pSession)
				{ 
					int procIdx = pSession->fd % threads; 
					int procFd = procFds[procIdx];

					updateEvents(procFd,pSession->fd ,pSession,kReadEvent|kWriteEvent,false);
					pSession->procFd = procFd; 
					pSession->on_connect();
				}

				void associate(T* pSession)
				{
					//	if (NULL == ::CreateIoCompletionPort((HANDLE)pSession->fd, kqueueFd, (ULONG_PTR)pSession, 0))
					//	{
					//		throw (int)::GetLastError();
					//	}
				}
			};

		//////////////////////////////////////////////////////////////////////////
		template<typename T,typename Factory ,typename Msg=char>
			class Processor
			{
				public:
					std::atomic<bool> isRunning; 
					std::vector<std::thread> runThreads; 
					//typedef std::shared_ptr<Factory> FactoryPtr; 
					typedef Factory*  FactoryPtr; 

					Processor() {
						innerFactory = new Factory(); 
						factory = innerFactory; 
                        isRunning = false; 
					}
                    Processor (FactoryPtr &fact):factory(fact) {
                        isRunning = false; 
                    }
					virtual ~Processor()
					{
					}


					int start(const ITCPService::Properties &config ,EventHandleFunc handler = nullptr)
					{
						isRunning = true; 
						int threads = config.threads >threads > MAX_THREAD?MAX_THREAD:config.threads; 
						Scheduler<T>& scheder = Environment<T >::iocper;
						scheder.threads = threads; 

						for (int i = 0;i < threads; ++i)
						{
							DLog("start proc  thread ==============\n"); 
							runThreads.push_back(std::thread(&Processor<T,Factory>::run,this,i )); 
						}
                        return 0; 
					}

					void stop()
					{
						isRunning = false; 
						for(auto &thrd : runThreads)
						{
							thrd.join(); 
						}
					}

					static void handle_event(Processor<T,Factory>& procor,Scheduler<T>& scheder, void * key)
					{
						int ready;
						T * clt = (T *)key;
						switch(clt->event)
						{
							case EVT_RECV:
								{
									if (0 >= ready)
									{
										clt->event = EVT_DISCONNECT;
									}
									else
									{
										clt->recvBufPos += ready; 
										int readPos =0; 
										int pkgLen = clt->read_packet(clt->recvBuf,clt->recvBufPos); 
										while (pkgLen >0 )
										{						
											if (readPos + pkgLen <= clt->recvBufPos )
											{									
												char * pEnd = (char*)(clt->recvBuf)+readPos+ pkgLen;
												char endVal = *pEnd; 
												*pEnd=0; 									 
												clt->on_recv((char*)(clt->recvBuf)+readPos, pkgLen);		
												*pEnd=endVal; 
												readPos += pkgLen; 
											}

											if (readPos  < clt->recvBufPos)
											{
												pkgLen = clt->read_packet(clt->recvBuf + readPos, clt->recvBufPos - readPos); 
												if (pkgLen <=0)
												{
													memmove(clt->recvBuf,(char*)clt->recvBuf + readPos, clt->recvBufPos - readPos);											
													clt->recvBufPos -= readPos;
													*(clt->recvBuf +clt->recvBufPos )=0;
													break; 
												}
											}
											else 
											{
												clt->recvBufPos =0 ; 
												break; 
											}					 
										}

										clt->launch_recv(/*clt->recvBuf, */512);
									}
								}
								break;
							case EVT_CONNECT:
								{
									// bind this socket to the kqueueFd handle
									try 
									{
										scheder.associate(clt);								
										clt->on_connect();
										clt->launch_recv(/*clt->recvBuf,*/512);
									}
									catch(...)
									{
										//::closesocket(clt->fd);
										close (clt->fd); 
										delete clt;
									}

								}
								break;

							case EVT_DISCONNECT:
								{
									clt->on_disconnect();
									//::closesocket(clt->fd);
									close(clt->fd); 
									delete clt;
								}
								break;


							case EVT_SEND:
								{
									if (0 >= ready)
									{
										// TO-DO:
										// WSAGetLastError() 
										clt->event = EVT_DISCONNECT;
										//::PostQueuedCompletionStatus(kqueueFd, 0, (int *)clt, NULL);
									}
									else
									{
										clt->on_send(ready);
									}
								}
								break;
						}
					}

					void * run(int index)
					{
						Scheduler<T>& scheder = Environment<T >::iocper;
						int procFd = scheder.procFds[index]; 


						while(isRunning) 
						{ 
							int key;
							int waitms = 10000; 
							struct timespec timeout;
							timeout.tv_sec = waitms / 1000;
							timeout.tv_nsec = (waitms % 1000) * 1000 * 1000;
							struct kevent activeEvs[MAX_WAIT_EVENT ];
							int n = kevent(procFd, NULL, 0, activeEvs, MAX_WAIT_EVENT , &timeout);

							//DLog("proc epoll_wait return %d\n", n);
							for (int i = 0; i < n; i ++) {
								T * pSession= (T*)activeEvs[i].udata;
								int fd = pSession->fd; 

								int events = activeEvs[i].filter;
								if (events == EVFILT_READ) {

									int recvLen  = recv(fd, (char*)pSession->recvBuf+ pSession->recvBufPos, pSession->recv_size(), 0);

									if (recvLen <0 && (errno == EAGAIN || errno == EWOULDBLOCK))
									{
										DLog ( " read done %d\n",recvLen); 
									}else if ( recvLen < 0 )
									{
										DLog("read error \n"); //实际应用中，recvLen <0应当检查各类错误，如EINTR
										DLog("fd %d closed\n", fd); 
										updateEvents(procFd,fd,pSession,kReadEvent,true);
										close(fd);  
										if (evtHandler)
										{
											evtHandler(EVT_SESSION_RELEASE,pSession); 
										}
									}else if (recvLen == 0)
									{
										DLog("client closed\n");
										updateEvents(procFd,fd,pSession,kReadEvent|kWriteEvent,true);
										close(fd);
										if (evtHandler)
										{
											evtHandler(EVT_SESSION_RELEASE,pSession); 
										}
									}else { 

										pSession->recvBufPos += recvLen; 
										int readPos =0; 
										int pkgLen = pSession->read_packet(pSession->recvBuf,pSession->recvBufPos); 
										DLog("package length:  is %d\n",pkgLen); 
										while (pkgLen >0 )
										{						
											if (readPos + pkgLen <= pSession->recvBufPos )
											{									
												char * pEnd = (char*)(pSession->recvBuf)+readPos+ pkgLen;
												char endVal = *pEnd; 
												*pEnd=0; 									 
												pSession->on_recv((char*)(pSession->recvBuf)+readPos, pkgLen);		
												*pEnd=endVal; 
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
												pSession->recvBufPos =0; 
												break; 
											}					 
										}

										DLog("recv buf pos is %d\n",pSession->recvBufPos ); 
									}

								}else if (events == EVFILT_WRITE)
								{

									DLog("async write "); 
									std::atomic_thread_fence(std::memory_order_consume);
									ssize_t dataLen = pSession->sndBufTail - pSession->sndBufHead;


									{
										ssize_t sntLen = ::send(pSession->fd,                                        
												(char*)pSession->sendBuf+pSession->sndBufHead,
												dataLen,0);
										if (sntLen > 0)
										{
											std::atomic_thread_fence(std::memory_order_acq_rel);
											pSession->sndBufHead += sntLen;
										}
									}

									if ( errno == EAGAIN ||  errno == EWOULDBLOCK )
									{
										updateEvents(procFd, pSession->fd,pSession , kWriteEvent, true);
										continue;
									}

									updateEvents(procFd, pSession->fd ,pSession , kReadEvent, true);

								}
							}

						}

						return 0;
					}

					FactoryPtr innerFactory; 
					FactoryPtr factory; 
					EventHandleFunc evtHandler; 
			};

		//////////////////////////////////////////////////////////////////////////	

		template<typename T >
			struct Environment
			{
				static Scheduler<T> iocper;
				//static Processor<T> workers;
				//static Factory  factory;

				~Environment()
				{

				}
			};

		template<typename T>
			Scheduler<T> Environment<T>::iocper;

		//template<typename T>
		//	Processor<T> Environment<T>::workers;

		//////////////////////////////////////////////////////////////////////////
		template<typename T,typename Factory >
			struct Listener
			{
				int ip;
				int port;
				int backlogs;
				void* srv;
				int listenFd;
				std::atomic<bool> isRunning; 
				std::thread lisThread; 
				ITCPService::Properties m_config; 
				Processor<T,Factory,typename T::Message> &processor; 
				EventHandleFunc evtHandler; 

				Listener( Processor<T,Factory,typename T::Message> &procor):processor(procor)
				{
				}

				int start(const ITCPService::Properties &config ,EventHandleFunc handler = nullptr)
				{
					evtHandler = handler; 
					m_config = config; 
					isRunning = true; 
					Scheduler<T>& scheder = Environment<T>::iocper;
					int kfd = scheder.kqueueFd;
					int lisFd =-1 ;
					try
					{
						lisFd = socket(AF_INET, SOCK_STREAM, 0);
						if (-1 == lisFd)
						{
							DLog ("create listen socket failed"); 

							//throw errno;
							return -1; 
						}

						struct sockaddr_in addr;
						memset(&addr, 0, sizeof addr);
						addr.sin_family = AF_INET;
						//addr.sin_addr.s_addr = ip;
						addr.sin_addr.s_addr = INADDR_ANY; 
						addr.sin_port = htons(port);
						if (-1 == bind(lisFd, (struct sockaddr *)&addr, sizeof(struct sockaddr) ))
						{
							ELog("bind  error \n"); 
							return -1 ; 
						}

						backlogs = 20; 
						if (-1 == listen(lisFd, backlogs))
						{
							ELog("listen error \n"); 
							return -1; 
						}

						setNoBlock(lisFd); 
						updateEvents(kfd, lisFd, kReadEvent, false);
						this->listenFd = lisFd;
						lisThread = std::thread(&Listener<T,Factory>::run,this); 
					}
					catch(...)
					{
						if (-1 != lisFd)
						{
							close(lisFd);
						}
						//		throw;
						return -1; 
					}
					return 0; 
				}

				void stop()
				{
					isRunning  = false; 
					close(this->listenFd); 
					lisThread.join(); 
				}


				static void setNoBlock(int fd)
				{
					int flags = fcntl(fd, F_GETFL, 0);
					if (flags < 0)
					{
						ELog("fcntl failed"); 
					}

					int ret = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
					if (ret < 0)
					{
						ELog("fcntl failed"); 
					}
				}

				static void handleRead(int efd,int fd)
				{
					char buf[4096];
					int n = 0;
					while ((n=::read(fd, buf, sizeof buf)) > 0) {
						DLog("read %d bytes\n", n);
						int ret = ::write(fd, buf, n); //写出读取的数据
						//实际应用中，写出数据可能会返回EAGAIN，此时应当监听可写事件，当可写时再把数据写出

						if (ret <= 0 )
						{
							DLog("write error \n"); 
						}
					}
					if (n<0 && (errno == EAGAIN || errno == EWOULDBLOCK))
					{
						DLog ( " read done %d\n",n); 
						return;
					}
					if ( n < 0 )
					{
						DLog("read error \n"); //实际应用中，n<0应当检查各类错误，如EINTR
					}
					DLog("fd %d closed\n", fd);
					close(fd);
				}
				static void handleWrite(int efd,int fd)
				{
					updateEvents(efd, fd, kReadEvent, true);
				}

				void * run()
				{
					Scheduler<T>& scheder = Environment<T>::iocper;

					int lisFd = this->listenFd;
					int kfd = scheder.kqueueFd;
					struct sockaddr_in addr;
					int addrlen = sizeof(addr);
					while (isRunning)
					{ 
						int waitms = 10000; 
						struct timespec timeout;
						timeout.tv_sec = waitms / 1000;
						timeout.tv_nsec = (waitms % 1000) * 1000 * 1000;
						struct kevent activeEvs[MAX_WAIT_EVENT ];
						int n = kevent(kfd, NULL, 0, activeEvs, MAX_WAIT_EVENT , &timeout);
						//DLog("epoll_wait return %d\n", n);
						for (int i = 0; i < n; i ++) {
							DLog("kevent count  %d\n",n); 
							int fd = (int)(intptr_t)activeEvs[i].udata;
							int events = activeEvs[i].filter;
							if (events == EVFILT_READ) {
								DLog("handle read event\n"); 
								if (fd == lisFd) {
									DLog("handle accept \n"); 
									struct sockaddr_in raddr;
									socklen_t rsz = sizeof(raddr);
									int cfd = accept(fd,(struct sockaddr *)&raddr,&rsz);

									if (cfd < 0)
									{
										ELog("accept failed\n");  
										continue; 
									}

									sockaddr_in peer, local;
									socklen_t alen = sizeof(peer);
									int ret = getpeername(cfd, (sockaddr*)&peer, &alen);

									if (ret < 0)
									{
										ELog("getpeername failed"); 
									}

									DLog("accept a connection from %s\n", inet_ntoa(raddr.sin_addr));

									setNoBlock(cfd); 
									int procIdx = cfd % m_config.threads; 
									int procFd = scheder.procFds[procIdx];


									T * pSession = processor.factory->create(); 
									pSession->fd = cfd; 
									pSession->procFd = procFd; 

									assert(pSession != NULL);
									if (evtHandler)
									{
										evtHandler(EVT_SESSION_CREATE,pSession); 
									}

									updateEvents(procFd, pSession->fd ,pSession, kReadEvent|kWriteEvent, false);
									// 	pSession->iocp = procFd;
									// 	pSession->fd   = fd;
									// 	pSession->ip   = raddr.sin_addr.s_addr;
									// 	pSession->port = htons(raddr.sin_port);
									// 	pSession->event= EVT_CONNECT;
									// //	pSession->srv  = listener.srv;
									// 	pSession->isPassive = true;
									// 	scheder.push(pSession);


									pSession->on_connect(); 

								} else {
									handleRead(kfd, fd);
								}
							} else if (events == EVFILT_WRITE) {
								handleWrite(kfd, fd);
							} else {
								DLog("unknown event\n"); 
							}
						}
					}

					return 0;
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
				Processor<T,Factory,typename T::Message> &processor; 
				Connector(Processor<T,Factory,typename T::Message> &procor): processor(procor)
				{

				}
				int start()
				{
					return connect();
				}

				void stop()
				{
					::close(fd); 
				}

				int connect(/*const char* szIp, int port*/)
				{
					Scheduler<T>& scheder = Environment<T>::iocper;

					fd = socket(AF_INET, SOCK_STREAM, 0);
					if (-1 == fd)
					{
						throw errno;
					}

					struct sockaddr_in addr;
					int addrlen = sizeof(addr);
					addr.sin_family = AF_INET;
					addr.sin_addr.s_addr = ip/*inet_addr(szIp)*/;
					addr.sin_port = htons(port);

					if (-1 == ::connect(fd, (sockaddr*)&addr, addrlen))
					{				
						throw errno;
					}

					T * clt = new T();
					assert(clt != NULL);

					clt->kqueueFd = scheder.kqueueFd;
					clt->fd   = fd;
					clt->ip   = addr.sin_addr.s_addr;
					clt->port = ntohs(addr.sin_port);
					clt->event= EVT_CONNECT;
					clt->srv  = this;
					clt->isPassive = false;
					scheder.push(clt);	
					return 0;
				}

			};

		//////////////////////////////////////////////////////////////////////////
		template<int RecvBufSize=8192, int SendBufSize=8192,typename Msg = char  >
			struct EventHandler : public IEventHandler, public IConnection
		{
			std::string id; 
			typedef Msg Message; 
			int kqueueFd;
			int fd;
			int procFd; 
			NetEvent event;
			void*		  srv;
			int			  ip;
			int			  port;
			bool		  isPassive;
			char		  sendBuf[SendBufSize];
			char		  recvBuf[RecvBufSize];
			int				recvBufPos ; 
			std::atomic<int> sndBufHead; 
			std::atomic<int> sndBufTail; 
			EventHandler():recvBufPos(0)
			{
				sndBufHead = 0; 
				sndBufTail = 0; 
			}

			virtual ~EventHandler()
			{
			}

			virtual int send(const char* pData, unsigned int dataLen)
			{
				DLog("send data length is %d\n",dataLen); 
				return ::send(fd,pData,dataLen,0); 
			}

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
						return -1; 
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
					return -1; 
				}
				updateEvents(procFd,fd,this,kWriteEvent,true);
				return 0; 
			}

			virtual void launch_recv(unsigned int len = RecvBufSize)
			{
				this->event = EVT_RECV;
				if (recvBufPos >= RecvBufSize)
				{
					return; 
				}

				char buf[MAX_BUFFER_SIZE] = "";
				int n = 0;
				while ((n=::read(fd, buf, sizeof buf)) > 0) {
					DLog("read %d bytes\n", n);
					//		int ret = ::write(fd, buf, n); //写出读取的数据
					//实际应用中，写出数据可能会返回EAGAIN，此时应当监听可写事件，当可写时再把数据写出
				}
				if (n<0 && (errno == EAGAIN || errno == EWOULDBLOCK))
				{
					return;
				}
				//实际应用中，n<0应当检查各类错误，如EINTR
				if (n < 0)
				{
					DLog("read error !!!! \n"); 
				}
				DLog("fd %d closed\n", fd);
				::close(fd);
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

			virtual void on_send(const char* pBuf, unsigned int len)
			{
			}

		};

		//////////////////////////////////////////////////////////////////////////
		template<typename T,typename Factory = SessionFactory<T> >
			struct TCPService : public ITCPService
		{
			typedef std::list<Listener<T,Factory>* > ListenerList; 
			typedef std::list<Connector<T,Factory>* > ConnectorList; 
			//typedef std::shared_ptr<Factory> FactoryPtr; 
			typedef Factory*  FactoryPtr; 
			ListenerList lstListener;
			ConnectorList lstConnector;
			Processor<T,Factory,typename T::Message> processor; 


			TCPService() { }
			TCPService(FactoryPtr  factory = nullptr):processor(factory) { }

			int start(EventHandleFunc handler = nullptr)
			{
				if (config.backlogs <= 0)
				{
					config.backlogs = 10;
				}
				if (config.threads <= 0)
				{
					config.threads = 1;
				}

				Environment<T>::iocper.scheds = config.scheds;
				Environment<T>::iocper.start();

				//Environment<T>::workers.threads = config.threads;				
				//Environment<T>::workers.start();

				processor.start(config,handler); 

				for(auto &lis : lstListener)
				{
					int ret = lis->start(config,handler); 
					if (handler)
					{
						handler(ret== 0?EVT_LISTEN:EVT_LISTEN_FAIL,NULL); 
					}
				}

				std::this_thread::sleep_for(std::chrono::microseconds(1)); 

				for(auto &conn :  lstConnector)
				{
					int ret = conn->start(); 
					if (handler)
					{
						handler(ret== 0?EVT_CONNECT:EVT_CONNECT_FAIL,NULL); 
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
				//Environment<T>::workers.stop();

				Environment<T>::iocper.stop();
			}

			void addListener(const char* pIp, int port)
			{
				auto  listener = new Listener<T,Factory>(processor);
				assert(listener != NULL);

				listener->ip = inet_addr(pIp);
				listener->port = port;
				listener->backlogs = config.backlogs;
				listener->srv = this;
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

		// TO-DO: 
		// 1.packet buffer management like way of envelope dispatching
	}
}


#endif //__KQUEUE_H__

