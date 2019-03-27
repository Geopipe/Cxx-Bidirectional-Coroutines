/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2018, Geopipe, Inc.
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
				
				
				template<class Coro, class F> boost::context::continuation startCoroutine(Coro& bdc, F & f){
					return boost::context::callcc([&](boost::context::continuation && c){
						typename Coro::Yield yield(bdc, std::move(c));
						FinishCoroutine<Coro>::apply(yield,f);
						return std::move(yield.to_);
					});
				}
			}
			
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
				
				template<class F> BidirectionalCoroutine(F f) : BidirectionalCoroutine<void, Args...>(detail::startCoroutine(*this, f)) {}
				
				R operator()(Args ...args){
					(*(BidirectionalCoroutine<void, Args...>*)(this))(args...);
					return ret_;
				}
				
			};
			
			template<class ...Args> class BidirectionalCoroutine<void, Args...> {
				boost::context::continuation next_;
				std::tuple<Args...> args_;
			protected:
				BidirectionalCoroutine(boost::context::continuation && next) : next_(std::move(next)) {}
			public:
				class Yield {
					template<class Rp, class ...ArgsP> friend class BidirectionalCoroutine;
					template<class BDCp, class Fp> friend boost::context::continuation detail::startCoroutine(BDCp&, Fp&);
					BidirectionalCoroutine<void, Args...> &handle_;
					boost::context::continuation to_;
				public:
					Yield(BidirectionalCoroutine<void, Args...> &handle, boost::context::continuation && to) : handle_(handle), to_(std::move(to)) {}
					
					std::tuple<Args...>& operator()() {
						to_ = to_.resume();
						return handle_.args_;
					}
					
				};
				
				template<class F> BidirectionalCoroutine(F f) {
					next_ = detail::startCoroutine(*this, f);
				}
				
				void operator()(Args ...args){
					args_ = std::make_tuple(args...);
					next_ = next_.resume();
				}
			};
			
			namespace detail {
				template<class R, class ...Args> class FinishCoroutine<com::geopipe::functional::BidirectionalCoroutine<R, Args...>> {
					
					using Coro = com::geopipe::functional::BidirectionalCoroutine<R, Args...>;
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
