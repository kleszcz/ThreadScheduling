#include "utils.h"
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#include <atomic>

#include <thread>
#include <chrono>
#include <iostream>

#define NUMBER_OF_THREADS 20
#define DEQUE_SIZE 32

namespace abp
{
	using Task = void(*)();
	class TaskPool
	{
		struct Age
		{
			unsigned short tag;
			unsigned short top;
			//size_t tag : 2;
			//size_t top : 30;
		};
		static_assert(sizeof(Age) == sizeof(size_t), "Age must be same size as size_t");

		Task deq[DEQUE_SIZE];
		std::atomic<size_t> bot{};
		std::atomic<Age> age{};
		
	public:
		Task pop();
		void push(Task);
		Task steal();
	};

	Task TaskPool::pop()
	{
		auto localBot = bot.load();
		if (localBot == 0)
		{
			return nullptr;
		}
		localBot--;
		bot.store( localBot);
		auto task = deq[localBot];
		auto oldAge = age.load();
		if (localBot > oldAge.top)
		{
			return task;
		}
		bot.store(0);
		decltype(oldAge) newAge{ oldAge.tag + 1, 0 };
		if (localBot == oldAge.top)
		{
			if (std::atomic_compare_exchange_strong(&age, &oldAge, newAge))
			{
				return task;
			}
		}
		age.store(newAge);
		return nullptr; //NULL
	}

	void TaskPool::push(Task task)
	{
		auto localBot = bot.load();
		deq[localBot] = task;
		localBot++;
		bot.store(localBot);
	}

	Task TaskPool::steal()
	{
		auto oldAge = age.load();
		auto localBot = bot.load();
		if (localBot <= oldAge.top)
		{
			return nullptr; //NULL
		}
		auto task = deq[oldAge.top];
		auto newAge = oldAge;
		newAge.top++;
		if (std::atomic_compare_exchange_strong(&age, &oldAge, newAge))
		{
			return task;
		}
		return nullptr; //ABORT
	}

	TaskPool taskPools[NUMBER_OF_THREADS]{};

	void execute(Task task)
	{
		task();
	}
	
	void runWorkingThread(size_t id, const bool* done)
	{
		Task current = nullptr;
		while (!*done)
		{
			while (current)
			{
				execute(current);
				current = taskPools[id].pop();
			}

			//steal
			auto victimId = utils::uniform_int_rand<size_t>(0, NUMBER_OF_THREADS-1);
			current = taskPools[victimId].steal();
		}
	}
}

int main(int argc, char* argv[])
{
	using namespace std::chrono_literals;
	
	bool done = false;

	std::thread threads[NUMBER_OF_THREADS];
	for(size_t i = 0; i < NUMBER_OF_THREADS; ++i)
		threads[i] = std::thread(abp::runWorkingThread, i, &done);

	

	std::this_thread::sleep_for(10s);
	done = true;

	for (size_t i = 0; i < NUMBER_OF_THREADS; ++i)
		threads[i].join();

	return 0;
}
