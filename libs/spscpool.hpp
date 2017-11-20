#ifndef __SPSC_POOL_HPP__
#define __SPSC_POOL_HPP__

#include "spscqueue.hpp"


namespace tinynet 
{
	namespace utils{

		template <typename T >
			class SpscPool
			{
				public:

					T * alloc()
					{
						T * pMsg  = nullptr; 
						bool result = _queue->try_dequeue(pMsg); 
						if (result && pMsg)
						{
							return pMsg; 
						}
						else 
						{
							if (_queue.isEmpty())
							{

							}
							Log("dequeue failed"); 
						}

					}


					void release(T* pItem )
					{
						pItem->~T(); 
					}
					

				private:
					spscqueue<T> _queue; 
			};

	}

}

#endif 
