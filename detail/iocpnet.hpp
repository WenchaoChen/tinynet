//******************************************************************************
//	created:	7-2-2017    
//	filename: 	iocpnet.h
//	author:		arthur
//	purpose:	
//******************************************************************************

#ifndef __IOCPNET_H__
#define __IOCPNET_H__

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib")

#include <assert.h>
#include <list>
#include <vector>

#include "tinynet.hpp"

namespace tinynet
{
	namespace net
	{
		/*
		enum IocpEventEnum
		{
			EVT_CONNECT,
			EVT_RECV,
			EVT_SEND,
			EVT_DISCONNECT,
		};
		*/

		template<typename T>
		struct Scheduler
		{
			int    scheds;
			HANDLE iocp;

			void start()
			{
				iocp = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, scheds);
				if (NULL == iocp)
				{
					throw (int)::WSAGetLastError();
				}
			}

			void stop()
			{
				::CloseHandle(iocp);
				iocp = INVALID_HANDLE_VALUE;
			}

			void push(T* clt)
			{
				::PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)clt, NULL);
			}

			void associate(T* clt)
			{
				if (NULL == ::CreateIoCompletionPort((HANDLE)clt->fd, iocp, (ULONG_PTR)clt, 0))
				{
					throw (int)::GetLastError();
				}
			}
		};

		//////////////////////////////////////////////////////////////////////////
		template<typename T,typename Factory,typename Msg = char>
		struct Processor
		{
			int	threads;		
			bool isRunning; 

			typedef Factory * FactoryPtr; 


			FactoryPtr factory; 

			std::vector<std::thread> runThreads; 

			EventHandleFunc evtHandler; 

		 

			Processor()
			{


			}


			Processor(FactoryPtr &fact) :factory(fact)
			{

			}
			void start(const ITCPService::Properties &cfg,EventHandleFunc handler = nullptr)
			{
				evtHandler = handler; 


				isRunning = true; 
				for (int i = 0;i < threads; ++i)
				{
					//DWORD tid;
					//HANDLE thd = ::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)run, this, 0, &tid);	
					//if (NULL == thd)
					//{
					//	throw (int)::GetLastError();
					//}
					//::CloseHandle(thd);

					runThreads.push_back(std::thread(&Processor<T,Factory,Msg>::run, this));  

				}
			}

			void stop()
			{
				isRunning = false; 

				for (auto thrd : runThreads)
				{
					thrd.join();
				}

			}

			void   run()
			{
		 
				Scheduler<T>& scheder = Environment<T>::iocper;
				HANDLE iocp = scheder.iocp;

				DWORD ready;
				ULONG_PTR key;
				WSAOVERLAPPED* overlap;
				while (isRunning)
				{
					::GetQueuedCompletionStatus(iocp, &ready, &key, (LPOVERLAPPED *)&overlap, INFINITE);

					T * clt = (T *)key;
					switch(clt->event)
					{
					case EVT_RECV:
						{
							if (0 >= ready)
							{
								clt->event = EVT_DISCONNECT;
								::PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)clt, NULL);
							}
							else
							{
								printf("before recv buf pos is %d\n", clt->recvBufPos); 
								clt->recvBufPos += ready; 
								DWORD readPos =0; 
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
										clt->recvBufPos = 0; 
										break; 
									}					 
								}


								printf("after recv buf pos is %d\n", clt->recvBufPos);
								
								clt->launch_recv(/*clt->recvBuf, */512);
							}
						}
						break;
					case EVT_CONNECT:
						{
							// bind this socket to the iocp handle
							try 
							{
								scheder.associate(clt);								
								clt->on_connect();
								clt->launch_recv(/*clt->recvBuf,*/512);
							}
							catch(...)
							{
								::closesocket(clt->fd);
								delete clt;
							}

						}
						break;

					case EVT_DISCONNECT:
						{
							clt->on_disconnect();
							::closesocket(clt->fd);
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
								::PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)clt, NULL);
							}
							else
							{
								clt->on_send(ready);
							}
						}
						break;
					}
				}

			}
		};

		//////////////////////////////////////////////////////////////////////////	

		static WSADATA wsadata;
		const int wsaversion = WSAStartup(MAKEWORD(2,2), &wsadata);

		template<typename T>
		struct Environment
		{
			static Scheduler<T> iocper;
		//	static Processor<T> workers;
			
			~Environment()
			{
				::WSACleanup();
			}
		};

		template<typename T>
		Scheduler<T> Environment<T>::iocper;

	//	template<typename T>
	//	Processor<T> Environment<T>::workers;

		//////////////////////////////////////////////////////////////////////////
		template<typename T>
		struct Listener
		{
			int ip;
			int port;
			int backlogs;
			void* srv;
			SOCKET fd;

			bool isRunning; 
			std::thread listenThread; 

			void start()
			{
				isRunning = true; 
				SOCKET fd = INVALID_SOCKET;
				try
				{
					fd = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
					if (INVALID_SOCKET == fd)
					{
						throw (int)WSAGetLastError();
					}

					struct sockaddr_in addr;
					addr.sin_family = AF_INET;
					addr.sin_addr.s_addr = ip;
					addr.sin_port = htons(port);
					if (SOCKET_ERROR == ::bind(fd, (struct sockaddr *)&addr, sizeof(addr)))
					{
						throw (int)::WSAGetLastError();
					}

					if (SOCKET_ERROR == ::listen(fd, backlogs))
					{
						throw (int)::WSAGetLastError();
					}

					this->fd = fd;
					listenThread = std::thread(&Listener<T>::run, this); 

				}
				catch(...)
				{
					if (INVALID_SOCKET != fd)
					{
						::closesocket(fd);
					}

					throw;
				}
			}

			void stop()
			{
				isRunning = false; 
				listenThread.join(); 
			}

			  void  run()
			{
			 
				Scheduler<T>& scheder = Environment<T>::iocper;

				SOCKET lsfd = this->fd;
				HANDLE iocp = scheder.iocp;

		 
				struct sockaddr_in addr;
				int addrlen = sizeof(addr);
				while (isRunning)
				{
					SOCKET	fd = ::WSAAccept(lsfd, (struct sockaddr *)&addr, &addrlen, NULL, NULL);
					if (INVALID_SOCKET == fd)
					{
						break;
					}

					T * clt = new T();
					assert(clt != NULL);

					clt->iocp = iocp;
					clt->fd   = fd;
					clt->ip   = addr.sin_addr.s_addr;
					clt->port = htons(addr.sin_port);
					clt->event= EVT_CONNECT;
					clt->srv  = this->srv;
					clt->isPassive = true;
					scheder.push(clt);
				}

	 
			}
		};

		//////////////////////////////////////////////////////////////////////////

		template <typename T>
		struct Connector
		{
			int ip;
			int port;
			SOCKET fd;

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

				fd = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
				if (INVALID_SOCKET == fd)
				{
					throw (int)WSAGetLastError();
				}

				struct sockaddr_in addr;
				int addrlen = sizeof(addr);
				addr.sin_family = AF_INET;
				addr.sin_addr.s_addr = ip/*inet_addr(szIp)*/;
				addr.sin_port = ::htons(port);

				if (SOCKET_ERROR == ::connect(fd, (sockaddr*)&addr, addrlen))
				{				
					throw (int)::WSAGetLastError();
				}
				
				T * clt = new T();
				assert(clt != NULL);

				clt->iocp = scheder.iocp;
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
		template<int RecvBufSize=8192, int SendBufSize=8192,typename Msg = char>
		struct EventHandler : public IEventHandler, public IConnection
		{
			HANDLE		  iocp;
			SOCKET		  fd;

			typedef Msg Message;
			//	IocpEventEnum event;

			NetEvent event; 
			void*		  srv;
			int			  ip;
			int			  port;
			bool		  isPassive;


			char		  sendBuf[SendBufSize];
			char		  recvBuf[RecvBufSize];
			DWORD           recvBufPos;
			EventHandler() :recvBufPos(0)
			{

			}

			virtual ~EventHandler()
			{
			}


			virtual int async_send(const char * pData, unsigned int dataLen)
			{
				return ::send(fd, pData, dataLen, 0);

			}

			virtual int send(const char *pData, unsigned int dataLen)
			{

				return ::send(fd, pData, dataLen, 0);

			 }


			virtual void launch_send(const char* pData, unsigned int dataLen)
			{
				::send(fd, pData, dataLen, 0);
				//this->event = EVT_SEND;
			}

			virtual void launch_recv(unsigned int len = RecvBufSize)
			{
			//	this->event = EVT_RECV;

				WSAOVERLAPPED overlap;
				::memset(&overlap, 0, sizeof(overlap));
				if (recvBufPos >= RecvBufSize)
				{
					return; 
				}
				WSABUF buf;
				buf.buf = (char*)recvBuf+ recvBufPos;
				buf.len = RecvBufSize - recvBufPos;
				DWORD ready = 0;
				DWORD flags = 0;
				if (0 != ::WSARecv(fd, &buf, 1, &ready, &flags, &overlap, NULL)
					&& WSA_IO_PENDING != WSAGetLastError())
				{
		//			this->event = EVT_DISCONNECT;
					::PostQueuedCompletionStatus(iocp, 0, (ULONG_PTR)this, NULL);
				}
			}

			virtual int read_packet(const char * pData , int dataLen)
			{
				return dataLen; 
			}

			virtual void launch_close()
			{
				::shutdown(fd, SD_BOTH);
			}
			virtual void close()
			{
				::shutdown(fd, SD_BOTH);
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

			virtual void on_send(const char* pBuf, unsigned int len)
			{
			}
  
		};

		//////////////////////////////////////////////////////////////////////////
		template<typename T, typename Factory = SessoinFactory >
		struct TCPService : public ITCPService
		{
			typedef std::list<Listener<T>* > ListenerList; 
			typedef std::list<Connector<T>* > ConnectorList; 
			ListenerList lstListener;
			ConnectorList lstConnector;

			typedef Factory * FactoryPtr; 

			EventHandleFunc evtHandler; 


			Processor<T, Factory, typename T::Message> processor; 


			TCPService(FactoryPtr factory = nullptr) : processor(factory)
			{

			}

			virtual int start(EventHandleFunc handle = nullptr)
			{
				evtHandler = handle; 
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

			//	Environment<T>::workers.threads = config.threads;				
			//	Environment<T>::workers.start();

				processor.start(config,evtHandler); 

				
				for (ListenerList::iterator itr = lstListener.begin(); 
					itr != lstListener.end(); ++itr)
				{
					(*itr)->start();
				}

				Sleep(1000);
				
				for (ConnectorList::iterator itr2 = lstConnector.begin(); 
					itr2 != lstConnector.end(); ++itr2)
				{
					(*itr2)->start();
				}

				return 0;
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

				//newConnector->ip = inet_pton(pIp);
				newConnector->port = port;

				lstConnector.push_back(newConnector);
			}
		
		};

		// TO-DO: 
		// 1.implement asynchronous read
		// 2.packet buffer management like way of envelope dispatching
	}
}
 
#endif //win32
#endif //__IOCP_H__
 
