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

#include <functional-cxx/support/memory-hacks.hpp>

namespace com {
	namespace geopipe {
		/**************************************************
		 * Tools for functional programming
		 **************************************************/
		namespace functional {
			/**************************************************
			 * Internal implementation details
			 **************************************************/
			namespace detail {
				/// Meta-programming magic to handle edge-cases at the end of coroutine execution.
				template<class Coro> class FinishCoroutine;
				
				/// Basically just type-level currying so that `StackAlloc` doesn't interfere with template argument deduction
				template<class StackAlloc> struct _CoroutineContext {
					/// Encapsulate boilerplate used to in the constructors for `CoroutineContext::BidirectionalCoroutine`.
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
			
			/// Basically just type-level currying so that `StackAlloc` doesn't interfere with template argument deduction
			template<class StackAlloc = boost::context::fixedsize_stack>
			struct CoroutineContext {
				using traits_type = typename StackAlloc::traits_type;
				
				/*************************************************************************************
				 * A coroutine is a restricted form of cooperative multitasking, where two "routines"
				 * take turns performing computation and then passing data back and forth.
				 * The two routines run in the same OS-level thread, but each maintain their own
				 * call stacks, which must be exchanged when control flow is transferred between them,
				 * which is an operation akin to the "context swap" performed when the OS swaps between
				 * threads running on the same physical core. The main difference from threads is that
				 * the programmer, rather than some external scheduling algorithm, has full control over
				 * when context swaps are executed, and can even step through them in a debugger (although
				 * because this involves replacing all of the active stack frames, it is difficult to tell
				 * exactly what is happening then the context swap is in progress, so it may be more helpful to
				 * set debugging breakpoints on either side of the context swap).
				 * 
				 * One form of coroutine that you may already be familiar with is 
				 * the use Python's `yield` statement to create "generators". For those unfamiliar
				 * with generators, [Chapter 33 of PAPL](https://papl.cs.brown.edu/2019/control-operations.html#%28part._.Generators%29)
				 * contains a good introduction.
				 * 
				 * Here we define `BidirectionalCoroutine`s, which, in addition to allowing the coroutine to
				 * yield `R` values to the "main" routine after each step, allows the "main" routine to pass 
				 * additional `Args...` values to the coroutine after each step.
				 * 
				 * We treat coroutines yielding `void` as a special base-case for coroutines yielding 
				 * a value-type, so the templated inheritance hierarchy is a little funky!
				 * 
				 * @warning Each coroutine represents a unique execution context, and so coroutines are move-only types.
				 *************************************************************************************/
				template<class R, class ...Args>
				class BidirectionalCoroutine : protected BidirectionalCoroutine<void, Args...> {
					detail::UniqueMaybePtr<R> ret_; ///< Possibly uninitialized storage for the most recently yielded value.
				protected:
					using YieldVoid = typename BidirectionalCoroutine<void, Args...>::Yield; ///< `Yield`ing something will rely on some of the logic for `Yield`ing nothing.
					using BidirectionalCoroutine<void, Args...>::next_;
					
				public:
					/*******************************************************************
					 * A non-`void` `Yield` operator.
					 * @warning It is not safe to mix-and-match `Yield` operators
					 * between coroutines, as they share internal state in a complex way
					 *******************************************************************/
					class Yield : public YieldVoid {
						R* ret_; ///< Contractually, this must not be null.
						bool& rInit_; ///< The storage for this field is on the heap (cf. constructor's implementation).
					public:
						/*******************************************************************
						 * @arg handle The coroutine which will be suspended when we invoke this `Yield`.
						 * @arg to A continuation to resume the "main" routine when we invoke this `Yield`.
						 *******************************************************************/
						Yield(BidirectionalCoroutine<R, Args...> &handle, boost::context::continuation && to) 
						: YieldVoid(handle, std::move(to))
					   	, ret_(handle.ret_.get())
						, rInit_(handle.ret_.get_deleter().initialized()) {}
						
						/*******************************************************************
						 * Suspend coroutine and execute context switch to "main" routine
						 * @arg r The value to be `Yield`ed.
						 * @return Whatever arguments are next passed to `BidirectionalCoroutine::operator()`.
						 *******************************************************************/
						template<class RP>
						std::tuple<Args...>& operator()(RP&& r) {
							detail::emplaceMaybeUninitialized(ret_, rInit_, std::forward<RP>(r));
							return (*(YieldVoid*)this)();
						}

						/*******************************************************************
						 * cf. documentation of `BidirectionalCoroutine<R, Args...>::BidirectionalCoroutine` for non-`void` `R`.
						 * 
						 * It would be nice if we could make this explicitly use-once
						 * somehow (i.e. a linear type), and use that first `void` `Yield`
						 * to obtain our non-`void` `Yield` for all future invocations,
						 * but that seems like needlessly complex metaprogramming for now
						 *******************************************************************/
						using YieldVoid::operator();
						
						
					};
					
					/*******************************************************************
					 * Construct a `BidirectionalCoroutine`, allow it to perform any
					 * setup actions specified by the client code, and then return control
					 * to the main routine.
					 * 
					 * @arg f The body of the coroutine. 
					 * All non-`void` coroutines must perform a single `void` `Yield`
					 * to transfer control back to their constructor, or else the first
					 * `Yield`ed value will be lost in the either. It would be very rude
					 * to never `Yield` at all.
					 * @arg stackSize How much space to allow for stack allocations in 
					 * the coroutine's context before overflowing. The default value
					 * comes from `boost::context` (or whatever other context trait
					 * you specify, I guess...)
					 *******************************************************************/
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
					
					/*******************************************************************
					 * Execute a context switch and transfer control to the coroutine.
					 * @arg args Data passed into the coroutine, will appear to be returned from the last `Yield::operator()` invocation.
					 * @return Whatever value is next `Yield`ed by the coroutine.
					 *******************************************************************/
					template<typename ...ArgsP>
					R& operator()(ArgsP&& ...args) {
						(*(BidirectionalCoroutine<void, Args...>*)(this))(std::forward<ArgsP>(args)...);
						// If ret_'s not yet initialized, all hell is about to break loose.
						// This can be checked by inspecting the deleter's fields (or by proxy, the yield)
						// At some later point, we should restructure the preamble so that
						// a void-yield is not possible after construction completes.
						return *ret_;
					}
					
					/// @return `false` if the coroutine has terminated, `true` otherwise.
					explicit operator bool() const {
						return (bool)(BidirectionalCoroutine<void, Args...>&)(*this);
					}
					
				};
				
				/**********************************************************
				 * Specialization implementing the case where the coroutine `Yield`s nothing 
				 * and therefore does not require storage for the yielded values.
				 * See the general-case `CoroutineContext::BidirectionalCoroutine` docs
				 * for more detailed information on coroutines.
				 **********************************************************/
				template<class ...Args>
				class BidirectionalCoroutine<void, Args...> {
				protected:
					detail::UniqueMaybePtr<std::tuple<Args...> > args_; ///< Store the arguments most recently passed in from "main" routine.
					boost::context::continuation next_; ///< The continuation to resume this coroutine
					/// An empty (unusable) coroutine.
					BidirectionalCoroutine()
					: args_(detail::make_unique_uninitialized<std::tuple<Args...> >())
				   	, next_() {}
				public:
					/*******************************************************************
					 * A `void` `Yield` operator, transfers control without providing a value to the "main" routine.
					 * @warning It is not safe to mix-and-match `Yield` operators
					 * between coroutines, as they share internal state in a complex way
					 *******************************************************************/
					class Yield {
						template<class Rp, class ...ArgsP>
						friend class BidirectionalCoroutine;
						friend struct detail::_CoroutineContext<StackAlloc>;
						
						std::tuple<Args...> *args_; ///< Contractually, this must not be null.
						boost::context::continuation to_; ///< The continuation for resuming the "main" routine.
					public:
						/*******************************************************************
						 * @arg handle The coroutine which will be suspended when we invoke this `Yield`.
						 * @arg to A continuation to resume the "main" routine when we invoke this `Yield`.
						 *******************************************************************/
						Yield(BidirectionalCoroutine<void, Args...> &handle, boost::context::continuation && to) 
						: args_(handle.args_.get())
						, to_(std::move(to)) {}
						
						/*******************************************************************
						 * Suspend coroutine and execute context switch to "main" routine
						 * @return Whatever arguments are next passed to `BidirectionalCoroutine::operator()`.
						 *******************************************************************/
						std::tuple<Args...>& operator()() {
							to_ = to_.resume();
							return *args_;
						}
						
					};
					
					/*******************************************************************
					 * Construct a `BidirectionalCoroutine`, allow it to perform any
					 * setup actions specified by the client code, and then return control
					 * to the main routine.
					 * 
					 * @arg f The body of the coroutine. 
					 * The first `Yield` simply transfers control back to their constructor,
					 * and typically occurs after any setup is performed.
					 * It would be very rude to never `Yield` at all.
					 * @arg stackSize How much space to allow for stack allocations in 
					 * the coroutine's context before overflowing. The default value
					 * comes from `boost::context` (or whatever other context trait
					 * you specify, I guess...)
					 *******************************************************************/
					template<class F>
					BidirectionalCoroutine(F f, size_t stackSize = traits_type::default_size())
					: BidirectionalCoroutine<void, Args...>() {
						next_ = detail::_CoroutineContext<StackAlloc>::startCoroutine(*this, f, stackSize); 
					}

					BidirectionalCoroutine(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine(const BidirectionalCoroutine& other) = delete;
					BidirectionalCoroutine& operator=(BidirectionalCoroutine&& other) = default;
					BidirectionalCoroutine& operator=(const BidirectionalCoroutine& other) = delete;
					
					/*******************************************************************
					 * Execute a context switch and transfer control to the coroutine.
					 * @arg args Data passed into the coroutine, will appear to be returned from the last `Yield::operator()` invocation.
					 *******************************************************************/
					template<typename ...ArgsP>
					void operator()(ArgsP&& ...args){
						detail::emplaceMaybeUninitialized(args_.get(), args_.get_deleter().initialized(), std::forward<ArgsP>(args)...);
						next_ = next_.resume();
					}
					
					/// @return `false` if the coroutine has terminated, `true` otherwise.
					explicit operator bool() const {
						return (bool)next_;
					}
				};
			};
			
			namespace detail {
				/// Handle's the special edge-cases around coroutine termination.
				template<template<class R, class ...Args> class BidirectionalCoroutine, class R, class ...Args>
				class FinishCoroutine<BidirectionalCoroutine<R, Args...>> {
					
					using Coro = BidirectionalCoroutine<R, Args...>; ///< The type of the coroutine being finished.
					using Yield = typename Coro::Yield; ///< The tpye of the yield for said coroutine.
					/// The return-type of the functor used as the coroutine body.
					template<class F, class ...FArgs> using RType = decltype((std::declval<F>())(std::declval<FArgs>() ...));
					
				public:
					/// If our coroutine's body returns a value matching the `Yield`ed type, perform one final implicit yield.
					template<class F, std::enable_if_t<std::is_assignable<RType<F&, Yield&>, R>::value,int> = 0>
					static void apply(Yield & yield, F & f) {
						yield(f(yield));
					}
					
					/// If our coroutine's body returns `void`, simply exit.
					template<class F, std::enable_if_t<std::is_void<RType<F&,Yield&>>::value,int> =0>
					static void apply(Yield & yield, F & f) {
						f(yield);
					}
					
					
					/// Create friendlier errors for the client code if the coroutine body would return a value not matching the `Yield`ed type.
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
