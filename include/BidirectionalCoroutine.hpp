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
#include <utility>

#include <boost/context/continuation.hpp>

namespace com {
	namespace geopipe {
		namespace functional {
			
			template<class R, class ...Args> class BidirectionalCoroutine : protected BidirectionalCoroutine<void, Args...> {
				R ret_;
			protected:
				using BidirectionalCoroutine<void, Args...>::yield;
				
				std::tuple<Args...>& yield(boost::context::continuation& to, R& r) {
					ret_ = r;
					return yield(to);
				}
				
			public:
				template<class F> BidirectionalCoroutine(F f) : BidirectionalCoroutine<void, Args...>(f) {}
				
				R operator()(Args ...args){
					(*(BidirectionalCoroutine<void, Args...>*)(this))(args...);
					return ret_;
				}
				
			};
			
			template<class ...Args> class BidirectionalCoroutine<void, Args...> {
				boost::context::continuation next_;
				std::tuple<Args...> args_;
				
			protected:
				std::tuple<Args...>& yield(boost::context::continuation& to) {
					to = to.resume();
					return args_;
				}
				
			public:
				template<class F> BidirectionalCoroutine(F f) : next_(boost::context::callcc(f)) {}
				
				void operator()(Args ...args){
					args_ = std::make_tuple(args...);
					next_ = next_.resume();
				}
			};

		}
	}
}
