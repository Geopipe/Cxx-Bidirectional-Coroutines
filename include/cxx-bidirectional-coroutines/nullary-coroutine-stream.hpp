#pragma once
/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2019 - 2020, Geopipe, Inc.
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

#include <functional-cxx/stream.hpp>
#include <cxx-bidirectional-coroutines/bidirectional-coroutine.hpp>

#include <memory>
#include <stdexcept>
#include <type_traits>

namespace com {
	namespace geopipe {
		/**************************************************
		 * Tools for functional programming
		 **************************************************/
		namespace functional {
			/// Memoize the result of a `CoroutineContext::BidirectionalCoroutine` (which is not reusable) as a `Stream` (which is reusable).
			template<class Coro>
			class NullaryCoroutineStreamF {
				std::unique_ptr<Coro> coro_;
			public:
				using CellT = Stream<std::remove_reference_t<decltype(std::declval<Coro>()())>>;
				using StreamT = std::shared_ptr<CellT>;
				
				NullaryCoroutineStreamF(std::unique_ptr<Coro> && coro)
				: coro_(std::move(coro)) {
					if(coro_ == nullptr) {
						throw std::logic_error("NullaryCoroutineStreamF constructed from null coroutine\n");
					}
				}
				
				/********************************************************
				 * Advance the `CoroutineContext::BidirectionalCoroutine` and return the yielded result
				 * as a `Stream`. This can only be done once per step of the
				 * coroutine, so we rely on `Stream`'s memoization semantics
				 * to preserve the result.
				 ********************************************************/
				StreamT operator()() {
					
					if(coro_ && *coro_) {
						auto& head = (*coro_)();
						// finite, non-void coroutines appear to repeat their last yielded value
						// because of how the lambda's control-flow aligns with the continuations
						// (if the coroutine terminates before yielding anything, it will be uninitialized
						// and therefore it is important we don't look inside until after checking for
						// termination)
						//
						// It's probably possible to fix this, but it would take a *lot* of engineering
						// and likely some eager look-ahead execution that we might not actually want.
						return (*coro_) ? CellT::Cell(std::move(head), NullaryCoroutineStreamF<Coro>(std::move(coro_))) : CellT::Nil();
					} else {
						throw std::logic_error("This NullaryCoroutineStreamF has been exhausted. It shouldn't be reachable");
					}
				}
			};

		}
	}
}

