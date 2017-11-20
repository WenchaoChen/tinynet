//******************************************************************************
//	created:	7-2-2017    
//	filename: 	epollnet.h
//	author:		arthur
//	purpose:	linux epoll version 
//******************************************************************************

#ifndef __EPOLL_H__
#define __EPOLL_H__

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
//#include "objects.h"
#include "fake_pool.h"
#include "tinynet.h"

namespace tinynet
{
	namespace net
	{





		struct PollEvent{
			int fd ; 
			void * session; 
		}; 

		template<typename T> struct Environment; 
		enum IocpEventEnum
		{
			EVT_CONNECT,
			EVT_RECV,
			EVT_SEND,
			EVT_DISCONNECT,
		};

		template<typename T>
			struct Scheduler
			{
				int scheds;
				int epollFd;
				int procFd; 

				void start()
				{
					epollFd = epoll_create(10); // 创建一个 epoll 的句柄，参数要大于 0， 没有太大意义
					if( -1 == epollFd ){
						printf("throw error %d\n",__LINE__); 	
						throw errno;
					}
					//read,write epoll 
					procFd = epoll_create(10); 
					if ( -1 == procFd)
					{
						printf("throw error %d\n",__LINE__); 	
						throw errno ; 
					}
				}

				void stop()
				{
					close(epollFd); 
					epollFd = 0xFFFFFF;
				}

				void push(T* clt)
				{
					//::PostQueuedCompletionStatus(epollFd, 0, (ULONG_PTR)clt, NULL);
				}

				void associate(T* clt)
				{
					//	if (NULL == ::CreateIoCompletionPort((HANDLE)clt->fd, epollFd, (ULONG_PTR)clt, 0))
					//	{
					//		throw (int)::GetLastError();
					//	}
				}
			};

		//////////////////////////////////////////////////////////////////////////
		template<typename T>
			struct Processor
			{
				int	threads;		

				int maxIdx; 

				ObjectPool<PollEvent> clients; 


				void start()
				{
					maxIdx = 1; 
					threads = 0; 

					for (int i = 0;i < threads; ++i)
					{

						pthread_t handle; 

						printf("start proc  thread ==============\n"); 
						int ret = pthread_create(&handle,NULL,run,this); 
						if (0 != ret)
						{
							printf("create error\n"); 
							throw errno; 
						}

					}
				}


				void stop()
				{
				}

				static void * run(void * param)
				{
					Processor<T>& procor = *(Processor<T> *)param;
					Scheduler<T>& scheder = Environment<T>::iocper;
					int efd = scheder.epollFd;
					int procFd = scheder.procFd; 
					int maxIdx ;
					int ready;
					int key;
					struct epoll_event wait_event; //内核监听完的结果

					while (true)
					{

						maxIdx = procor.clients.size(); 
						printf("proc epoll wait,max fd idx is %d\n",maxIdx); 
						int ret = epoll_wait(procFd, &wait_event, maxIdx+1, -1);
						//继续响应就绪的描述符

						PollEvent* pEvent = (PollEvent*)wait_event.data.ptr; 

						T* pSession = (T*)pEvent->session; 
						if (EPOLLIN == wait_event.events & (EPOLLIN|EPOLLERR) )
						{
							char buf[2048] = "";
							int len = recv(pEvent->fd, buf, sizeof(buf), 0); 

							printf( "recv length %d ,event is %d\n",len,wait_event.events); 

							//接受客户端数据
							if(len < 0)
							{
								printf("client closed\n"); 
								if(errno == ECONNRESET)//tcp连接超时、RST
								{
									close(pEvent->fd); 
									procor.clients.release(pEvent); 
								}
								else
								{
									perror("read error:");
									procor.clients.release(pEvent); 
								}
							}
							else if(len == 0)//客户端关闭连接
							{
								printf("client request closed \n"); 
								close(pEvent->fd);
								procor.clients.release(pEvent); 
							}
							else//正常接收到服务器的数据
							{
								pSession->on_recv(buf,len); 
								printf("received message:%s\n",(char*)buf); 
								//				send(procor.clientFds[i], buf, len, 0);
							}


						}
						else {
							printf("other event %d \n",wait_event.events); 
						}

						//for(i = 1; i <= maxIdx; i++)
						//{
						//	if(procor.clientFds[i] < 0)
						//	{
						//		continue;
						//	}

						//	if(( procor.clientFds[i] == wait_event.data.fd )
						//			&& ( EPOLLIN == wait_event.events & (EPOLLIN|EPOLLERR) ))
						//	{
						//		printf("get new in event\n"); 
						//		char buf[128] = "";
						//		int len = recv(procor.clientFds[i], buf, sizeof(buf), 0); 

						//		printf( "recv length %d ,event is %d\n",len,wait_event.events); 

						//		//接受客户端数据
						//		if(len < 0)
						//		{
						//			printf("client closed\n"); 
						//			if(errno == ECONNRESET)//tcp连接超时、RST
						//			{
						//				close(procor.clientFds[i]);
						//				procor.clientFds[i] = -1;
						//			}
						//			else
						//			{
						//				perror("read error:");
						//			}
						//		}
						//		else if(len == 0)//客户端关闭连接
						//		{
						//			printf("client request closed \n"); 
						//			close(procor.clientFds[i]);
						//			procor.clientFds[i] = -1;
						//		}
						//		else//正常接收到服务器的数据
						//		{
						//			printf("received message:%s\n",(char*)buf); 
						//	//			send(procor.clientFds[i], buf, len, 0);
						//		}

						//		//所有的就绪描述符处理完了，就退出当前的for循环，继续poll监测
						//		if(--ret <= 0)
						//		{
						//			break;
						//		}
						//	}
						//	else 
						//	{
						//		printf("other event %d \n",wait_event.events); 

						//	}
						//}

						//					T * clt = (T *)key;
						//					switch(clt->event)
						//					{
						//						case EVT_RECV:
						//							{
						//								if (0 >= ready)
						//								{
						//									clt->event = EVT_DISCONNECT;
						//									//				::PostQueuedCompletionStatus(efd, 0, (ULONG_PTR)clt, NULL);
						//								}
						//								else
						//								{
						//									clt->recvBufPos += ready; 
						//									int readPos =0; 
						//									int pkgLen = clt->read_packet(clt->recvBuf,clt->recvBufPos); 
						//									while (pkgLen >0 )
						//									{						
						//										if (readPos + pkgLen <= clt->recvBufPos )
						//										{									
						//											char * pEnd = (char*)(clt->recvBuf)+readPos+ pkgLen;
						//											char endVal = *pEnd; 
						//											*pEnd=0; 									 
						//											clt->on_recv((char*)(clt->recvBuf)+readPos, pkgLen);		
						//											*pEnd=endVal; 
						//
						//											readPos += pkgLen; 
						//										}
						//
						//										if (readPos  < clt->recvBufPos)
						//										{
						//											pkgLen = clt->read_packet(clt->recvBuf + readPos, clt->recvBufPos - readPos); 
						//											if (pkgLen <=0)
						//											{
						//												memmove(clt->recvBuf,(char*)clt->recvBuf + readPos, clt->recvBufPos - readPos);											
						//												clt->recvBufPos -= readPos;
						//												*(clt->recvBuf +clt->recvBufPos )=0;
						//												break; 
						//											}
						//										}
						//										else 
						//										{
						//											break; 
						//										}					 
						//									}
						//
						//									clt->launch_recv(/*clt->recvBuf, */512);
						//								}
						//							}
						//							break;
						//						case EVT_CONNECT:
						//							{
						//								// bind this socket to the iocp handle
						//								try 
						//								{
						//									scheder.associate(clt);								
						//									clt->on_connect();
						//									clt->launch_recv(/*clt->recvBuf,*/512);
						//								}
						//								catch(...)
						//								{
						//									//::closesocket(clt->fd);
						//									close (clt->fd); 
						//									delete clt;
						//								}
						//
						//							}
						//							break;
						//
						//						case EVT_DISCONNECT:
						//							{
						//								clt->on_disconnect();
						//								//::closesocket(clt->fd);
						//								close(clt->fd); 
						//								delete clt;
						//							}
						//							break;
						//
						//
						//						case EVT_SEND:
						//							{
						//								if (0 >= ready)
						//								{
						//									// TO-DO:
						//									// WSAGetLastError() 
						//									clt->event = EVT_DISCONNECT;
						//									//::PostQueuedCompletionStatus(iocp, 0, (int *)clt, NULL);
						//								}
						//								else
						//								{
						//									clt->on_send(ready);
						//								}
						//							}
						//							break;
						//					}
					}

					return 0;
				}
			};

		//////////////////////////////////////////////////////////////////////////	

		//WSADATA wsadata;
		//const int wsaversion = WSAStartup(MAKEWORD(2,2), &wsadata);

		template<typename T>
			struct Environment
			{
				static Scheduler<T> iocper;
				static Processor<T> workers;

				~Environment()
				{
					//				::WSACleanup();
				}
			};

		template<typename T>
			Scheduler<T> Environment<T>::iocper;

		template<typename T>
			Processor<T> Environment<T>::workers;

		//////////////////////////////////////////////////////////////////////////
		template<typename T>
			struct Listener
			{
				int ip;
				int port;
				int backlogs;
				void * srv;
				int lisFd = -1;

				bool isRunning ; 

				void start()
				{
					isRunning = true; 
					int sock =-1 ;
					try
					{
						sock = socket(AF_INET, SOCK_STREAM, 0);
						if (-1 == sock)
						{
							printf("throw error %d\n",__LINE__); 	
							throw errno;
						}

						struct sockaddr_in addr;
						addr.sin_family = AF_INET;
						addr.sin_addr.s_addr = ip;
						addr.sin_port = htons(port);

						setnonblocking(sock); 
						if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(addr)))
						{
							printf("bind address error \n"); 
							return ; 
						}

						if (-1 == listen(sock, backlogs))
						{
							printf("throw error %d\n",__LINE__); 	
							throw errno;
						}

						this->lisFd = sock;

						pthread_t thd; 

						printf("start listen thread  #############\n"); 
						int ret = pthread_create(&thd,NULL,run,this); 
						if (-1 == ret )
						{
							printf("throw error %d\n",__LINE__); 	
							throw errno;
						}
					}
					catch(...)
					{
						if (-1 != sock)
						{
							//	::closesocket(sock);
							close(sock);  
						}

						printf("throw error %d\n",__LINE__); 	
						throw;
					}
				}

				void stop()
				{

				}

				static void setnonblocking(int sock)
				{
					int opts = fcntl(sock,F_GETFL);
					if(opts<0)
					{
						perror("fcntl(sock,GETFL)");
						return ; 
					}
					opts = opts|O_NONBLOCK;
					if(fcntl(sock,F_SETFL,opts)<0)
					{
						perror("fcntl(sock,SETFL,opts)");
						return ; 
					}  
				}


				static void *  run(void * param)
				{
					Listener<T>& listener = *(Listener<T> *)param;
					Scheduler<T>& scheder = Environment<T>::iocper;
					Processor<T>& procor = Environment<T>::workers; 

					struct epoll_event wait_event; //内核监听完的结果
					int lisFd  = listener.lisFd;
					int efd    = scheder.epollFd;
					int procFd = scheder.procFd;

					struct epoll_event event;   // 告诉内核要监听什么事件

					PollEvent * pLisEvent = procor.clients.acquire(); 
					pLisEvent->fd = lisFd; 

					//event.data.fd = lisFd;     //监听套接字
					event.data.ptr = pLisEvent;     //监听套接字
					event.events   = EPOLLIN; // 表示对应的文件描述符可以读

					//事件注册函数，将监听套接字描述符 sockfd 加入监听事件
					int ret = epoll_ctl(efd, EPOLL_CTL_ADD, lisFd, &event);
					if(-1 == ret){
						printf("throw error %d\n",__LINE__); 	
						throw errno; 
					}

					struct sockaddr_in addr;
					int addrlen = sizeof(addr);
					while (listener.isRunning)
					{
						// 监视并等待多个文件（标准输入，udp套接字）描述符的属性变化（是否可读）
						// 没有属性变化，这个函数会阻塞，直到有变化才往下执行，这里没有设置超时
						printf("listener epoll wait\n"); 
						int ret = epoll_wait(efd, &wait_event, 1, -1);
						if (ret < 0)
						{
							//TODO
						}

						PollEvent * pollEvent = (PollEvent*)wait_event.data.ptr; 
						//监测lisFd(监听套接字)是否存在连接
						//if(( lisFd == wait_event.data.fd )
						if( lisFd == pollEvent->fd )
						{
							if ( EPOLLIN == wait_event.events & EPOLLIN )
							{

								struct sockaddr_in lisAddr;
								int addrlen = sizeof(lisAddr);

								//从tcp完成连接中提取客户端
								int connFd = accept(lisFd, (struct sockaddr *)&lisAddr, (socklen_t*)&addrlen);

								PollEvent * newEvent = procor.clients.acquire(); 

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

								setnonblocking(connFd); 

								//将提取到的connFd放入fd数组中，以便下面轮询客户端套接字
								//event.data.fd = connFd; //监听套接字
								event.data.ptr = newEvent; 
								newEvent->fd     = connFd; 
								event.events   = EPOLLIN|EPOLLERR; // 表示对应的文件描述符可以读

								//事件注册函数，将监听套接字描述符 connFd 加入监听事件
								//ret = epoll_ctl(procFd, EPOLL_CTL_ADD, connFd, &event);
								ret = epoll_ctl(efd, EPOLL_CTL_ADD, connFd, &event);
								if(-1 == ret){
									printf("throw error %d\n",__LINE__); 	
									procor.clients.release(newEvent); 
									throw errno; 
								}

								T * clt = new T();
								newEvent->session = clt; 
								clt->fd = connFd; 
								clt->on_connect(); 
							}
						}
						else if (EPOLLIN == wait_event.events & (EPOLLIN|EPOLLERR) )
						{

							T* pSession = (T*)pollEvent->session; 
							char buf[2048] = "";
							int len = recv(pollEvent->fd, buf, sizeof(buf), 0); 

							printf( "recv length %d ,event is %d\n",len,wait_event.events); 

							//接受客户端数据
							if(len < 0)
							{
								printf("client closed\n"); 
								if(errno == ECONNRESET)//tcp连接超时、RST
								{
									close(pollEvent->fd); 
									procor.clients.release(pollEvent); 
								}
								else
								{
									perror("read error:");
									procor.clients.release(pollEvent); 
								}
							}
							else if(len == 0)//客户端关闭连接
							{
								printf("client request closed \n"); 
								close(pollEvent->fd);
								procor.clients.release(pollEvent); 
							}
							else//正常接收到服务器的数据
							{
								pSession->on_recv(buf,len); 
								printf("received message:%s\n",(char*)buf); 
								//				send(procor.clientFds[i], buf, len, 0);
							}


						}
					}

					return 0;
				}
			};

		//////////////////////////////////////////////////////////////////////////

		template <typename T>
			struct Connector
			{
				int ip;
				int port;
				int fd;

				void start()
				{
					connect();
				}

				void stop()
				{
				}

				void connect(/*const char* szIp, int port*/)
				{
					Scheduler<T>& scheder = Environment<T>::iocper;

					fd = socket(AF_INET, SOCK_STREAM, 0);
					if (-1 == fd)
					{
						printf("throw error %d\n",__LINE__); 	
						throw errno;
					}

					struct sockaddr_in addr;
					int addrlen = sizeof(addr);
					addr.sin_family = AF_INET;
					addr.sin_addr.s_addr = ip/*inet_addr(szIp)*/;
					addr.sin_port = htons(port);

					if (-1 == ::connect(fd, (sockaddr*)&addr, addrlen))
					{				
						printf("throw error %d\n",__LINE__); 	
						throw errno;
					}

					T * clt = new T();
					assert(clt != NULL);

					clt->iocp = scheder.epollFd;
					clt->fd   = fd;
					clt->ip   = addr.sin_addr.s_addr;
					clt->port = ntohs(addr.sin_port);
					clt->event= EVT_CONNECT;
					clt->srv  = this;
					clt->isPassive = false;
					scheder.push(clt);	
				}

			};

		//////////////////////////////////////////////////////////////////////////
		template<int RecvBufSize=8192, int SendBufSize=8192>
			struct EventHandler : public IEventHandler, public IConnection
		{
			int iocp;
			int fd;
			IocpEventEnum event;
			void*		  srv;
			int			  ip;
			int			  port;
			bool		  isPassive;


			char		  sendBuf[SendBufSize];
			char		  recvBuf[RecvBufSize];
			int				recvBufPos ; 
			EventHandler():recvBufPos(0)
			{

			}

			virtual ~EventHandler()
			{
			}

			virtual void launch_send(const char* pData, unsigned int dataLen)
			{
				//::send(fd, pData, dataLen, 0);
				send(fd,pData,dataLen,0); 
				//this->event = EVT_SEND;
			}

			virtual void launch_recv(unsigned int len = RecvBufSize)
			{
				this->event = EVT_RECV;
				if (recvBufPos >= RecvBufSize)
				{
					return; 
				}


				char buf[4096];
				int n = 0;
				while ((n=::read(fd, buf, sizeof buf)) > 0) {
					printf("read %d bytes\n", n);
					//		int r = ::write(fd, buf, n); //写出读取的数据
					//实际应用中，写出数据可能会返回EAGAIN，此时应当监听可写事件，当可写时再把数据写出
				}
				if (n<0 && (errno == EAGAIN || errno == EWOULDBLOCK))
				{
					return;
				}
				//实际应用中，n<0应当检查各类错误，如EINTR
				if (n < 0)
				{
					printf("read error \n"); 
				}
				printf("fd %d closed\n", fd);
				close(fd);


				/*
				   WSAOVERLAPPED overlap;
				   ::memset(&overlap, 0, sizeof(overlap));
				   WSABUF buf;
				   buf.buf = (char*)recvBuf+ recvBufPos;
				   buf.len = RecvBufSize - recvBufPos;
				   DWORD ready = 0;
				   DWORD flags = 0;
				   if (0 != ::WSARecv(fd, &buf, 1, &ready, &flags, &overlap, NULL)
				   && WSA_IO_PENDING != WSAGetLastError())
				   {
				   this->event = EVT_DISCONNECT;
				   ::PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)this, NULL);
				   }
				   */
			}

			virtual int read_packet(const char * pData , int dataLen)
			{
				return dataLen; 
			}

			virtual void launch_close()
			{
				//::shutdown(fd, SD_BOTH);
				close(fd); 
			}

			virtual void on_connect()
			{
				printf(" parent on_connect"); 
			}

			virtual void on_disconnect()
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
		template<typename T>
			struct TCPSever : public ITCPSever
		{
			typedef std::list<Listener<T>* > ListenerList; 
			typedef std::list<Connector<T>* > ConnectorList; 
			ListenerList lstListener;
			ConnectorList lstConnector;

			void start()
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

				Environment<T>::workers.threads = config.threads;				
				Environment<T>::workers.start();


				for (typename ListenerList::iterator itr = lstListener.begin(); 
						itr != lstListener.end(); ++itr)
				{
					(*itr)->start();
				}

				sleep(1);

				for (typename ConnectorList::iterator itr2 = lstConnector.begin(); 
						itr2 != lstConnector.end(); ++itr2)
				{
					(*itr2)->start();
				}
			}

			void stop()
			{
			}

			void addListener(const char* pIp, int port)
			{
				Listener<T>* newListener = new Listener<T>;
				assert(newListener != NULL);

				newListener->ip = inet_addr(pIp);
				newListener->port = port;

				newListener->backlogs = config.backlogs;
				newListener->srv = this;

				lstListener.push_back(newListener);
			}

			void addConnector(const char* pIp, int port)
			{
				Connector<T>* newConnector = new Connector<T>;
				assert(newConnector != NULL);

				newConnector->ip = inet_addr(pIp);
				newConnector->port = port;

				lstConnector.push_back(newConnector);
			}

		};

		// TO-DO: 
		// 1.implement asynchronous read
		// 2.packet buffer management like way of envelope dispatching
	}
}


#endif //__EPOLL_H__

