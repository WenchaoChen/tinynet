#pragma once 

#include <mutex>
#include <functional>

template <int BufferSize = 8192> 
class SendBuffer{

    public:

        SendBuffer()
        {
            dataHead = 0; 
            dataTail = 0; 
        }

        typedef std::function<int (const char * ,unsigned int )>   DataHandler; 

        bool push(const char * pData,unsigned int dataLen)
        {
            assert(dataLen <= BufferSize); 

            std::lock_guard<std::mutex> guard(dataMutex);
            if (BufferSize - dataTail < dataLen)
            {
                if (dataHead > 0)
                {
                    int leftSize = dataTail - dataHead; 
                    if (leftSize > 0 )
                    {
                        memmove(dataBuf,dataBuf+ dataHead , leftSize ); 
                        dataHead = 0; 
                        dataTail = leftSize ; 
                    }
                    else {
                        dataHead = 0; 
                        dataTail = 0; 
                    }
                }
                else 
                {
                    return false; 
                }
            }

            if (BufferSize - dataTail > dataLen)
            {
                memcpy(dataBuf+ dataTail, pData,dataLen); 
                dataTail += dataLen; 
            }else 
            {
                return false; 
            }
            return true; 
        } 


        bool pop(DataHandler handler )
        {
            dataMutex.lock(); 
            int dataLen = dataTail - dataHead; 
            dataMutex.unlock(); 
            int popLen = handler(dataBuf + dataHead, dataLen ); 
            if (popLen <=0)
            {
                return false; 
            }
            if (popLen  < dataLen  ) 
            {

                dataMutex.lock(); 
                dataHead += popLen; 
                dataMutex.unlock(); 

            }
            else {
                dataMutex.lock(); 
                dataHead = dataTail  = 0; 
                dataMutex.unlock(); 
            }
            return true; 
        }

    private:


        std::mutex dataMutex; 
        //atomic_uint  dataHead; 
        int dataHead; 
        //atomic_uint  dataTail; 
        int dataTail; 
        char dataBuf[BufferSize]; 

}; 
