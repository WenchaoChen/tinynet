#include <iostream>
#include <thread>

template<class T>
class Task
{
public:
	T count; 
static 	void execute(std::string command)
	{
		for(int i = 0; i < 5; i++)
		{
			std::cout<<command<<" :: "<<i<<std::endl;
		}
	}

};

int main()
{
	auto * taskPtr = new Task<int>();

	// Create a thread using member function
	//std::thread th(&Task::execute, taskPtr, "Sample Task");
	std::thread th(&Task<int>::execute,  "Sample Task");

	th.join();

	delete taskPtr;
	return 0;
}

