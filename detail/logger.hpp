#pragma once 

#include <iostream>
#include <stdio.h>
#include <errno.h>



#ifndef NDEBUG

#define DLog( fmt , ...  ) printf( "%s(%d)" fmt "\n",  __FUNCTION__,  __LINE__, ##__VA_ARGS__ )
#define ILog( fmt , ...  ) printf( "%s(%d)" fmt "\n",  __FUNCTION__,  __LINE__, ##__VA_ARGS__ )
#define ELog( fmt , ...  ) printf( "%s(%d)" fmt "\n",  __FUNCTION__,  __LINE__, ##__VA_ARGS__ )
#define WLog( fmt , ...  ) printf( "%s(%d)" fmt "\n",  __FUNCTION__,  __LINE__, ##__VA_ARGS__ )
//#define Trace( format, ... )   printf( "%s::%s(%d)" format, __FILE__, __FUNCTION__,  __LINE__, ##__VA_ARGS__ )

#else 

#define ILog 
#define DLog
#define ELog
#define WLog


#endif // NODEBUG /


