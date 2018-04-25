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
			
			template<class R, class ...Args> class SymmetricCoroutine : protected SymmetricCoroutine<void, Args...> {
				R ret_;
			protected:
				void setReturnValue(R& r){ ret_ = r; }
				
			public:
				template<class F> SymmetricCoroutine(F f) : SymmetricCoroutine<void, Args...>(f) {}
				
				R operator()(Args ...args){
					(*(SymmetricCoroutine<void, Args...>*)(this))(args...);
					return ret_;
				}
				
			};
			
			template<class ...Args> class SymmetricCoroutine<void, Args...> {
				boost::context::continuation next_;
				std::tuple<Args...> args_;
				
			protected:
				const std::tuple<Args...>& getArgValues() const { return args_; }
				
			public:
				template<class F> SymmetricCoroutine(F f) : next_(boost::context::callcc(f)) {}
				
				void operator()(Args ...args){
					args_ = std::make_tuple(args...);
					next_ = next_.resume();
				}
			};

		}
	}
}
