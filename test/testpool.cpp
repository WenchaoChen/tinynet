#include "pool.h"


static int g_idx = 0; 
class MyElem : public Element{

	public:
		MyElem()
		{
			idx = g_idx++; 
			printf("elem index %d\n",idx); 
		}

	private:
		int idx; 

}; 
Pool<MyElem> elements; 

int main()
{

	for(int i = 0; i < 50 ; i ++)
	{
		printf("acquire item %d\n",i); 
		MyElem * pElem =  elements.acquire(); 
	}


	return 0; 
}
