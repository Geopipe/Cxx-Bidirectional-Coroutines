#pragma once
/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2018 - 2020, Geopipe, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ************************************************************************************/

#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/context/continuation.hpp>

namespace com {
	namespace geopipe {
		namespace functional {
			
			namespace detail {
				template<class Coro> class FinishCoroutine;
				
				template<class StackAlloc> struct _CoroutineContext {
					template<class Coro, class F>
					static boost::context::continuation startCoroutine(Coro& bdc, F & f, size_t stackSize){
						return boost::context::callcc(std::allocator_arg, StackAlloc(stackSize), [&bdc,f](boost::context::continuation && c){
							typename Coro::Yield yield(bdc, std::move(c));
							FinishCoroutine<Coro>::apply(yield,f);
							return std::move(yield.to_);
						});
					}
				};
			}
			
			template<class StackAlloc = boost::context::fixedsize_stack>
			struct CoroutineContext {
				using traits_type = typename StackAlloc::traits_type;
				
				template<class R, class ...Args> class BidirectionalCoroutine : protected BidirectionalCoroutine<void, Args...> {
					R ret_;
				protected:
					using YieldVoid = typename BidirectionalCoroutine<void, Args...>::Yield;
					
				public:
					class Yield : public YieldVoid {
						using YieldVoid::handle_;
					public:
						Yield(BidirectionalCoroutine<R, Args...> &handle, boost::context::continuation && to) : YieldVoid(handle, std::move(to)) {}
						
						template<class RP> std::tuple<Args...>& operator()(RP&& r) {
							static_cast<BidirectionalCoroutine<R,Args...> &>(handle_).ret_ = std::forward<RP>(r);
							return (*(YieldVoid*)this)();
						}
						using YieldVoid::operator();
						
						
					};
					
					template<class F> BidirectionalCoroutine(F f, size_t stackSize = traits_type::default_size()) : BidirectionalCoroutine<void, Args...>(detail::_CoroutineContext<StackAlloc>::startCoroutine(*this, f, stackSize)) {}
					
					R operator()(Args ...args){
						(*(BidirectionalCoroutine<void, Args...>*)(this))(args...);
						return ret_;
					}
					
					explicit operator bool() const {
						return (bool)(BidirectionalCoroutine<void, Args...>&)(*this);
					}
					
				};
				
				template<class ...Args> class BidirectionalCoroutine<void, Args...> {
					boost::context::continuation next_;
					std::tuple<Args...> args_;
				protected:
					BidirectionalCoroutine(boost::context::continuation && next) : next_(std::move(next)) {}
				public:
					class Yield {
						template<class Rp, class ...ArgsP>
						friend class BidirectionalCoroutine;
						friend struct detail::_CoroutineContext<StackAlloc>;
						
						BidirectionalCoroutine<void, Args...> &handle_;
						boost::context::continuation to_;
					public:
						Yield(BidirectionalCoroutine<void, Args...> &handle, boost::context::continuation && to) : handle_(handle), to_(std::move(to)) {}
						
						std::tuple<Args...>& operator()() {
							to_ = to_.resume();
							return handle_.args_;
						}
						
					};
					
					template<class F> BidirectionalCoroutine(F f, size_t stackSize = traits_type::default_size()) {
						next_ = detail::_CoroutineContext<StackAlloc>::startCoroutine(*this, f, stackSize);
					}
					
					void operator()(Args ...args){
						args_ = std::make_tuple(args...);
						next_ = next_.resume();
					}
					
					explicit operator bool() const {
						return (bool)next_;
					}
				};
			};
			
			namespace detail {
				template<template<class R, class ...Args> class BidirectionalCoroutine, class R, class ...Args>
				class FinishCoroutine<BidirectionalCoroutine<R, Args...>> {
					
					using Coro = BidirectionalCoroutine<R, Args...>;
					using Yield = typename Coro::Yield;
					template<class F, class ...FArgs> using RType = decltype((std::declval<F>())(std::declval<FArgs>() ...));
					
				public:
					template<class F, typename std::enable_if<std::is_assignable<RType<F&, Yield&>, R>::value,int>::type = 0>
					static void apply(Yield & yield, F & f) {
						yield(f(yield)); // If our lambda returns a value, *and* we know what to do with it, assign it
					}
					
					
					template<class F, typename std::enable_if<std::is_void<RType<F&,Yield&>>::value,int>::type =0>
					static void apply(Yield & yield, F & f) {
						f(yield); // If our lambda doesn't return a value, ignore it.
					}
					
					
					template<class F, typename std::enable_if<!std::is_void<RType<F&,Yield&>>::value && !std::is_assignable<RType<F&, Yield&>, R>::value,int>::type =0>
					static void apply(Yield & yield, F & f) {
						static_assert(std::is_void<RType<F&,Yield&>>::value || std::is_assignable<RType<F&, Yield&>, R>::value,"return type of f must be void, or assignable to the return type of this coroutine");
						f(yield); // This will never execute.
					}
				};
			}

		}
	}
}
