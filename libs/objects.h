#ifndef __OBJECTS_H__
#define __OBJECTS_H__

#include <deque>
template <class T>
class ObjectPool{

	public:
		typedef T* pointer; 

		ObjectPool(size_t size = 100, int grow = 10):m_grow(grow) 
		{
			m_alloc_count = 0; 
			m_release_count = 0; 
			this->inflate(size); 
		}
		pointer acquire()
		{
			if (this->empty())
			{
				this->inflate(m_grow); 
			}
			pointer item = m_pool.front(); 
			m_pool.pop_front(); 
			m_alloc_count ++; 
			return item; 
		}

		void release(pointer pItem)
		{
			m_release_count++; 
			m_pool.push_back(pItem); 

			this->dump(); 
		}

		
		bool empty() const {
			return m_pool.empty();
		}
		size_t size() const {
			return m_pool.size();
		}

		void dump()
		{
			printf("dump object pool, %d, %d\n",m_alloc_count,m_release_count); 
		}

	private:
		void inflate(size_t count)
		{
			for (int i = 0; i< count ; i++)
			{
				pointer _p = static_cast<pointer>(::operator new(sizeof(T))); 
				m_pool.push_back(_p); 
			}
		}
		std::deque<pointer>  m_pool; 
		int m_grow; 
		int m_alloc_count;
		int m_release_count; 

}; 


#endif //
