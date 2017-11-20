#ifndef __MSG_QUEUE_HPP__
#define __MSG_QUEUE_HPP__

#include "spscqueue.hpp"

template <class T>
class MsgQueue{


	private:
		ObjectPool<T> _pool; 
		spscqueue<T> _queue; 
}; 


#endif 
