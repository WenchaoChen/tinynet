#ifndef __OBJECT_POOL_HPP__
#define __OBJECT_POOL_HPP__
#include <atomic>
#include <deque>
#include "AtomicLinkedList.h"

namespace tinynet 
{
    template <class T> 
        class MemoryAlloc{

            public:
                static T * alloc()
                {
                    return new T(); 
                }

                static void release(T* pObj)
                {
                    delete pObj; 
                }

        } ; 
    template <class T>
        class AtomicAlloc{

            public:
                T * alloc()
                {

                    if (m_objList.empty())
                    {
                        for(int i = 0;i < 20;i++)
                        {
                            T* pObj = new T(); 
                            m_objList.insertHead(pObj); 
                        }
                    }
                    T * pItem = nullptr;
                    m_objList.sweep([&](T* pObj){
                            pItem = pObj ; 
                            }); 
                    return pItem; 
                }

                void release(T* pObj)
                {
                    pObj->~T(); 
                    m_objList.insertHead(pObj); 
                }

            private:
                AtomicLinkedList<T*> m_objList; 

        }; 

    template <class T,class Alloc= AtomicAlloc<T> >
        class ObjectPool{
            Alloc _alloc; 

            public:
            typedef T* pointer; 

            ObjectPool(size_t size = 100, int grow = 10):m_grow(grow) 
            {
                m_alloc_count = 0; 
                m_release_count = 0; 
            }
            pointer acquire()
            {
                m_alloc_count ++; 
                T* pT = _alloc.alloc(); 
                //printf("alloc point %p\n",pT); 
                return pT; 
            }

            void release(pointer pItem)
            {
                m_release_count++; 
                //printf("release point %p\n",pItem); 
                _alloc.release(pItem); 
                pItem = NULL; 
                this->dump(); 
            }


            bool empty() const {
                return false;
            }
            size_t size() const {
                size_t left = m_alloc_count-m_release_count +1 ;
                if (left <1)
                {
                    left = 1; 
                }
                return left; 
            }

            void dump()
            {
                //printf("dump object pool, %d, %d\n",m_alloc_count.load(),m_release_count.load()); 
            }

            private:
            int m_grow; 
            std::atomic_int m_alloc_count;
            std::atomic_int m_release_count; 

        }; 


}
#endif //


