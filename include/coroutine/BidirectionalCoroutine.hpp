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

#include <memory>
#include <new>
#include <stdexcept>
#include <tuple>
#include <type_traits>
#include <utility>

#include <boost/context/continuation.hpp>

#include <functional/support/memory-hacks.hpp>

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
				
				template<class R, class ...Args>
				class BidirectionalCoroutine : protected BidirectionalCoroutine<void, Args...> {
					detail::UniqueMaybePtr<R> ret_;
				protected:
					using YieldVoid = typename BidirectionalCoroutine<void, Args...>::Yield;
					using BidirectionalCoroutine<void, Args...>::next_;
					
				public:
					class Yield : public YieldVoid {
						R* ret_; // Contractually, this must not be null
						bool& rInit_; // The storage for this field is on the heap (cf. constructor's implementation)
					public:
						Yield(BidirectionalCoroutine<R, Args...> &handle, boost::context::continuation && to) 
						: YieldVoid(handle, std::move(to))
					   	, ret_(handle.ret_.get())
						, rInit_(handle.ret_.get_deleter().initialized()) {}
						
						template<class RP>
						std::tuple<Args...>& operator()(RP&& r) {
							detail::emplaceMaybeUninitialized(ret_, rInit_, std::forward<RP>(r));
							return (*(YieldVoid*)this)();
						}
						using YieldVoid::operator();
						
						
					};
					
					template<class F>
					BidirectionalCoroutine(F f, size_t stackSize = traits_type::default_size()) 
					: BidirectionalCoroutine<void, Args...>()
				   	, ret_(detail::make_unique_uninitialized<R>()) {
						next_ = detail::_CoroutineContext<StackAlloc>::startCoroutine(*this, f, stackSize);
					}

					BidirectionalCoroutine(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine(const BidirectionalCoroutine& other) = delete;
					BidirectionalCoroutine& operator=(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine& operator=(const BidirectionalCoroutine& other) = delete;
					
					template<typename ...ArgsP>
					R& operator()(ArgsP&& ...args) {
						(*(BidirectionalCoroutine<void, Args...>*)(this))(std::forward<ArgsP>(args)...);
						// If ret_'s not yet initialized, all hell is about to break loose.
						// This can be checked by inspecting the deleter's fields (or by proxy, the yield)
						// At some later point, we should restructure the preamble so that
						// a void-yield is not possible after construction completes.
						return *ret_;
					}
					
					explicit operator bool() const {
						return (bool)(BidirectionalCoroutine<void, Args...>&)(*this);
					}
					
				};
				
				template<class ...Args>
				class BidirectionalCoroutine<void, Args...> {
				protected:
					detail::UniqueMaybePtr<std::tuple<Args...> > args_;
					boost::context::continuation next_;
					BidirectionalCoroutine()
					: args_(detail::make_unique_uninitialized<std::tuple<Args...> >())
				   	, next_() {}
				public:
					class Yield {
						template<class Rp, class ...ArgsP>
						friend class BidirectionalCoroutine;
						friend struct detail::_CoroutineContext<StackAlloc>;
						
						std::tuple<Args...> *args_;
						boost::context::continuation to_;
					public:
						Yield(BidirectionalCoroutine<void, Args...> &handle, boost::context::continuation && to) 
						: args_(handle.args_.get())
						, to_(std::move(to)) {}
						
						std::tuple<Args...>& operator()() {
							to_ = to_.resume();
							return *args_;
						}
						
					};
					
					template<class F>
					BidirectionalCoroutine(F f, size_t stackSize = traits_type::default_size())
					: BidirectionalCoroutine<void, Args...>() {
						next_ = detail::_CoroutineContext<StackAlloc>::startCoroutine(*this, f, stackSize); 
					}

					BidirectionalCoroutine(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine(const BidirectionalCoroutine& other) = delete;
					BidirectionalCoroutine& operator=(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine& operator=(const BidirectionalCoroutine& other) = delete;
					
					template<typename ...ArgsP>
					void operator()(ArgsP&& ...args){
						detail::emplaceMaybeUninitialized(args_.get(), args_.get_deleter().initialized(), std::forward<ArgsP>(args)...);
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
					template<class F, std::enable_if_t<std::is_assignable<RType<F&, Yield&>, R>::value,int> = 0>
					static void apply(Yield & yield, F & f) {
						yield(f(yield)); // If our lambda returns a value, *and* we know what to do with it, assign it
					}
					
					
					template<class F, std::enable_if_t<std::is_void<RType<F&,Yield&>>::value,int> =0>
					static void apply(Yield & yield, F & f) {
						f(yield); // If our lambda doesn't return a value, ignore it.
					}
					
					
					template<class F, std::enable_if_t<!std::is_void<RType<F&,Yield&>>::value && !std::is_assignable<RType<F&, Yield&>, R>::value,int> =0>
					[[noreturn]] static void apply([[maybe_unused]] Yield & yield, [[maybe_unused]] F & f) {
						static_assert(	std::is_void<RType<F&,Yield&>>::value || std::is_assignable<RType<F&, Yield&>, R>::value,
										"return type of f must be void, or assignable to the return type of this coroutine");
						throw std::runtime_error("Contradiction: enable_if should only succeed if static_assert fails");
					}
				};
			}

		}
	}
}
