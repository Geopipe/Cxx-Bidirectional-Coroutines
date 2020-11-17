#include <iostream>

#include <coroutine/BidirectionalCoroutine.hpp>

template<typename R, typename ...Args>
using BidirectionalCoroutine = com::geopipe::functional::CoroutineContext<>::BidirectionalCoroutine<R, Args...>;

class Fibonacci : public BidirectionalCoroutine<int> {
public:
	Fibonacci() : BidirectionalCoroutine<int>([](BidirectionalCoroutine<int>::Yield & yield) {
		int a = 0;
		int b = 1;
		for(yield();(yield(a),true);) {
			int next = a + b;
			a = b;
			b = next;
		}
	}) {}
};

class RunningBitCount : public BidirectionalCoroutine<size_t, bool> {
public:
	RunningBitCount() : BidirectionalCoroutine<size_t, bool>([](BidirectionalCoroutine<size_t,bool>::Yield &yield) {
		size_t a = 0;
		for(auto args = yield();;args = (yield(std::get<0>(args) ? (++a) : a)));
	}) {}
};

class NoiseMaker : public BidirectionalCoroutine<void, std::string, size_t> {
public:
	NoiseMaker() : BidirectionalCoroutine<void, std::string, size_t>([](BidirectionalCoroutine<void, std::string, size_t>::Yield &yield) {
		std::string foo; size_t bar;
		while(true){
			std::tie(foo, bar) = yield();
			std::cout << foo << "/" << bar << std::endl;
		}
	}) {}
};

int main(int argc, const char * argv[]) {
	{
		std::cout << "Fibs" << std::endl;
		Fibonacci fib;
		for(size_t j = 0; j < 10; ++j) {
			std::cout << fib() << std ::endl;
		}
	}
	
	{
		std::cout << "RBC" << std::endl;
		RunningBitCount rbc;
		bool bits[7] = { true, false, false, true, true, false, true};
		for(size_t j = 0; j < 7; ++j){
			std::cout << rbc(bits[j]) << std::endl;
		}
	}
	
	{
		std::cout << "Noise" << std::endl;
		NoiseMaker nm;
		for(size_t j = 0; j < 7; ++j){
			nm("Moo", j);
		}
	}
	
	return 0;
}
