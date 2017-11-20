#ifndef __POOL_H__
#define __POOL_H__

#include <stdio.h>
#include <assert.h>

class Element{
	public:
		Element()
		{
			next = NULL; 
		}
		class Element *next; 
		class Element *prev; 
}; 



template <class T> 
class Pool{
	public:
		Pool(size_t size = 20 ,size_t grow  = 20 ){
			Element * pItem = new T(); 
			m_head = pItem; 
			m_tail = m_head; 
			m_grow = grow; 
			this->inflate(m_grow); 
			printf("init pool size %d  head is %p \n",size,m_head); 
		}

		T * acquire()
		{
			if (m_head)
			{
				Element* pItem = m_head; 
				m_head = pItem->next; 
				if (!m_head)
				{
					this->inflate(m_grow); 
				}
				return (T*)pItem; 
			}
			else 
			{
				assert(false); 
			}
		}

		void release(T* pElem)
		{
			if (m_tail)
			{
				m_tail->next = pElem; 
				pElem->next = NULL; 
				m_tail = pElem; 
			}
			else
			{
				assert(false); 
			}
		}

	private:
		void inflate(size_t size = 10 )
		{
			int i = 0; 
			while(i++ < size)
			{
				Element * pTmp = m_tail; 
				if (pTmp)
				{
					pTmp->next = new Element(); 
					m_tail = pTmp->next; 
				}
			}	

		}

		struct Element * m_head; 
		struct Element * m_tail; 
		int m_grow; 

}; 


#endif //
