#include "utils.h"
#define _ENABLE_ATOMIC_ALIGNMENT_FIX
#include <atomic>
#include <cassert>
 
#include <functional>
#include <thread>
#include <chrono>
#include <iostream>

#define NUMBER_OF_THREADS 2
#define DEQUE_SIZE 32

namespace task_system
{
	class Task
	{
		void(*function)(void*) = nullptr;
		void* data = nullptr;

		std::atomic<int> childCount = 0;
		Task* parent = nullptr;
		bool done = false;

	public:

		Task(void(*function)(void*), void* data) : function(function), data(data)
		{
		}

		void operator()()
		{
			function(data);
			done = true;
			if (isDone())
			{
				if (parent)
					parent->decrementChildren();
			}
		}

		void incrementChildren()
		{
			childCount++;
		}
				
		void setParent(Task* parent)
		{
			this->parent = parent;
		}

		bool isDone()
		{
			return done && childCount.load() == 0;
		}

	private:
		void decrementChildren()
		{
			childCount--;
			if (isDone())
			{
				if (parent)
					parent->decrementChildren();
			}
		}
	};

	using TaskPointer = Task*;

	namespace abp
	{
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

			TaskPointer deq[DEQUE_SIZE];
			std::atomic<size_t> bot{};
			std::atomic<Age> age{};

		public:
			TaskPointer pop();
			void push(TaskPointer);
			TaskPointer steal();
		};

		TaskPointer TaskPool::pop()
		{
			auto localBot = bot.load();
			if (localBot == 0)
			{
				return nullptr;
			}
			localBot--;
			bot.store(localBot);
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

		void TaskPool::push(TaskPointer task)
		{
			auto localBot = bot.load();
			deq[localBot] = task;
			localBot++;
			bot.store(localBot);
		}

		TaskPointer TaskPool::steal()
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
	}

	namespace worker_thread
	{
		// pools for all threads
		abp::TaskPool taskPools[NUMBER_OF_THREADS]{};
		// current thread pool; set in run function based on thread id
		thread_local static abp::TaskPool* localPool = nullptr;
		thread_local static TaskPointer currentTask = nullptr;

		void execute(TaskPointer task)
		{
			currentTask = task;
			(*task)();			
		}

		abp::TaskPool& getRandomTaskPool()
		{
			auto victimId = utils::uniform_int_rand<size_t>(0, NUMBER_OF_THREADS - 1);
			return taskPools[victimId];
		}

		void run(size_t id, const bool* done)
		{
			localPool = &taskPools[id];
			TaskPointer current = localPool->pop();
			while (!*done)
			{
				while (current)
				{
					execute(current);
					current = localPool->pop();
				}

				current = getRandomTaskPool().steal();
			}
		}

		bool isDone(TaskPointer task)
		{
			return task->isDone();
		}

		// todo stack based wait will be nice as this implementation can easily overflow call stack
		void wait(TaskPointer task)
		{
			assert(localPool);
			auto* oldTask = currentTask;
			// as long as an awaited task is not ready
			while (!isDone(task))
			{
				// take new task from your pool
				TaskPointer current = localPool->pop();
				// if there is still no task try stealing
				if (!current)
					current = getRandomTaskPool().steal();
				// if we finally have something to do, execute
				if (current)
					execute(current);
			}
			currentTask = oldTask;
		}

		void scheduleChild(TaskPointer parent, TaskPointer child)
		{
			assert(localPool);
			parent->incrementChildren();
			child->setParent(parent);
			localPool->push(child);
		}

		void schedule(abp::TaskPool& taskPool, TaskPointer task)
		{
			taskPool.push(task);
		}
		
		void schedule(size_t workerThreadId, TaskPointer task)
		{
			assert(workerThreadId < NUMBER_OF_THREADS);
			schedule(taskPools[workerThreadId], task);
		}

		void schedule(TaskPointer task)
		{
			assert(localPool);
			schedule(*localPool, task);
		}

		
	}
}
		int cdata[10];

int main(int argc, char* argv[])
{
	using namespace std::chrono_literals;
	
	bool done = false;

	std::thread threads[NUMBER_OF_THREADS];
	for(size_t i = 0; i < NUMBER_OF_THREADS; ++i)
		threads[i] = std::thread(task_system::worker_thread::run, i, &done);

	task_system::Task task([](void*) {
		std::cout << std::chrono::high_resolution_clock::now().time_since_epoch().count() << " Start first task on tid:" << std::this_thread::get_id() << '\n';

		
		task_system::TaskPointer children[10];

		for (int i = 0; i < 10; ++i)
		{
			cdata[i] = i;
		}

		for (int i = 0; i < 10; ++i)
		{
			task_system::Task childtask([](void* data) {
				std::cout << std::chrono::high_resolution_clock::now().time_since_epoch().count() << " Child task run with data: " << *(int*)data << " on tid:" << std::this_thread::get_id() << '\n';
			}, (void*)&cdata[i]);
			
			children[i] = &childtask;
		}
		for (int i = 0; i < 10; ++i)
		{
			task_system::worker_thread::scheduleChild(task_system::worker_thread::currentTask, children[i]);
		}
		for (int i = 0; i < 10; ++i)
		{
			task_system::worker_thread::wait(children[i]);
		}
		std::cout << std::chrono::high_resolution_clock::now().time_since_epoch().count() << " First task done on  tid:" << std::this_thread::get_id() << '\n';
	}, nullptr);

	task_system::worker_thread::schedule(0, &task);

	std::this_thread::sleep_for(10s);
	done = true;

	for (size_t i = 0; i < NUMBER_OF_THREADS; ++i)
		threads[i].join();

	return 0;
}
