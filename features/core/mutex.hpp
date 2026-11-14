#include <atomic>

class SpinMtx{
public:
	void lock(){
		bool expected = true;
		while (!flag.compare_exchange_weak(expected, false, std::memory_order_acquire))
			expected = true;
	}

	void unlock(){
		flag.store(true, std::memory_order_release);
	}


private:
	std::atomic<bool> flag = true;
};
