# tinynet
a very simple ,cross-platform net library

# Usage
it's a header only library . just include "tinynet.hpp" to your project. 

support multi-thread process 

use iocp, kqueue,epoll on different platform. 

very basic functions, you can use it to create tcp server or client . ipv4 address only. 

welcome to improve it 

create a session :

 
    //     sample code :
     class TcpSession : public EventHandler<8192, 8192,char>
     {
     public: 
       //implement some callback :on_connected,on_recv, on_send etc
         ... 
     }; 
     TCPService<TcpSession<>  > server;
     server.config.threads  = 1;  //set process thread number 
     server.config.backlogs = 10; //tcp backlogs number
     server.addListener("0.0.0.0",  7788); //add a listener on localhost with port 7788 
    // server.addConnector("127.0.0.1", 7788); // or add a tcp client to connect to server 127.0.0.1:7788    
    
    //server.start(); // start it with no event monitor or start with a lambda function to get inner events
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



# Build Test program.

cmake . <br>

make 

# notice 
it's very alpha version , don't use it on product environment ,especial iocp on windows is delayed 

# contact 
contact me if you are intresting with this library : fatih#qq.com 
