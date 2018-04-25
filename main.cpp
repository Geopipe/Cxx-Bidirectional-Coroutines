#include <iostream>

#include <BidirectionalCoroutine.hpp>

using com::geopipe::functional::BidirectionalCoroutine;
using boost::context::continuation;

class Fibonacci : public BidirectionalCoroutine<int> {
public:
	Fibonacci() : BidirectionalCoroutine<int>([this](continuation && to) -> continuation && {
		int a = 0;
		int b = 1;
		for(yield(to);(yield(to,a),true);) {
			int next = a + b;
			a = b;
			b = next;
		}
		return std::move(to);
	}) {}
};

class RunningBitCount : public BidirectionalCoroutine<size_t, bool> {
public:
	RunningBitCount() : BidirectionalCoroutine<size_t, bool>([this](continuation && to) -> continuation && {
		size_t a = 0;
		for(auto args = yield(to);;args = (yield(to,std::get<0>(args) ? (++a) : a)));
		return std::move(to);
	}) {}
};

class NoiseMaker : public BidirectionalCoroutine<void, std::string, size_t> {
public:
	NoiseMaker() : BidirectionalCoroutine<void, std::string, size_t>([this](continuation && to) -> continuation && {
		std::string foo; size_t bar;
		while(true){
			std::tie(foo, bar) = yield(to);
			std::cout << foo << "/" << bar << std::endl;
		}
		return std::move(to);
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
