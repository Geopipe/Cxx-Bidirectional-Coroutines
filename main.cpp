//
//  main.cpp
//  BoostCallCCTests
//
//  Created by Thomas Dickerson on 4/24/18.
//  Copyright © 2018 Geopipe. All rights reserved.
//

#include <iostream>

#include <BidirectionalCoroutine.hpp>

using com::geopipe::functional::SymmetricCoroutine;
using boost::context::continuation;

class Fibonacci : public SymmetricCoroutine<int> {
public:
	Fibonacci() : SymmetricCoroutine<int>([this](continuation && yield) -> continuation && {
		int a = 0;
		int b = 1;
		for(;;) {
			yield=yield.resume();
			setReturnValue(a);
			int next = a + b;
			a = b;
			b = next;
		}
		return std::move(yield);
	}) {}
};

class RunningBitCount : public SymmetricCoroutine<size_t, bool> {
public:
	RunningBitCount() : SymmetricCoroutine<size_t, bool>([this](continuation && yield) -> continuation && {
		size_t a = 0;
		for(;;) {
			yield=yield.resume();
			a += std::get<0>(getArgValues());
			setReturnValue(a);
		}
		return std::move(yield);
	}) {}
};

class NoiseMaker : public SymmetricCoroutine<void, std::string, size_t> {
public:
	NoiseMaker() : SymmetricCoroutine<void, std::string, size_t>([this](continuation && yield) -> continuation && {
		for(;;){
			yield = yield.resume();
			std::string foo;
			size_t bar;
			std::tie(foo, bar) = getArgValues();
			std::cout << foo << "/" << bar << std::endl;
		}
	}) {}
};

int main(int argc, const char * argv[]) {
	// insert code here...
	std::cout << "Fibs" << std::endl;
	Fibonacci fib;
	for(size_t j = 0; j < 10; ++j) {
		std::cout << fib() << std ::endl;
	}
	
	std::cout << "RBC" << std::endl;
	RunningBitCount rbc;
	bool bits[7] = { true, false, false, true, true, false, true};
	for(size_t j = 0; j < 7; ++j){
		std::cout << rbc(bits[j]) << std::endl;
	}
	
	std::cout << "Noise" << std::endl;
	NoiseMaker nm;
	for(size_t j = 0; j < 7; ++j){
		nm("Moo", j);
	}
	
	return 0;
}
