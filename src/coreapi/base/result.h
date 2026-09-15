/*
 * result.h - a result carrying either a value or an error
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __coreapi_result_h__
#define __coreapi_result_h__

#include "errors.h"

#include <cstdlib>
#include <string>
#include <type_traits>
#include <utility>

// Every entry point returns a Result. Hand a container over with
// ok(std::move(out)), otherwise the payload is copied.
//
// A Result answers for what the box did, not for the memory this layer needed to
// say it. Allocation here is left to throw, and the layer is built with
// exceptions so that a throw can unwind to a single handler where its consumer
// enters it; writing one allocation nothrow buys nothing while the containers
// beside it still throw, and the answer that reports a failed allocation
// allocates to carry its own words. No such handler exists in the shipped
// program, which is built without exceptions, so a throw out of here ends it.
// The test binary is built with them and reports a throw as a failed case.

namespace coreapi
{

// What a caller branches on before it reads the code beside it. Denied is its
// own member and not a fault: a caller refused for who it is can offer a
// credential, which is a different act from correcting a wrong request, and
// folding it into Internal would tell that caller the box broke.
enum class Status
{
	Ok,
	NotFound,
	InvalidArgument,
	Conflict,
	NotSupported,
	Busy,
	Denied,
	Internal
};

struct Error
{
	Status      status;
	// One of a set this layer wrote, so a consumer branches on a value the
	// compiler knows and not on a string somebody typed. codeString() in
	// errors.h turns it into what goes on a wire.
	ErrorCode   code;
	std::string message;

	Error() : status(Status::Internal), code(ErrorCode::BoxUnreadable) {}
	Error(Status s, ErrorCode c, const std::string &m)
		: status(s), code(c), message(m) {}
};

// value() aborts rather than throws so the type stays usable from the
// -fno-exceptions translation units that make up most of the tree.
template <typename T>
class Result
{
	public:
		static Result success(const T &v) { Result r; r.ok_ = true; r.value_ = v; return r; }
		static Result success(T &&v) { Result r; r.ok_ = true; r.value_ = std::move(v); return r; }
		static Result failure(const Error &e) { Result r; r.ok_ = false; r.error_ = e; return r; }

		bool ok() const { return ok_; }

		// The rvalue overloads return by value, so a reference bound to what a
		// temporary result carries cannot outlive that temporary. The
		// non-const ones move, so extracting a container from a result the
		// caller is done with does not reallocate it.
		const T &value() const &  { if (!ok_) std::abort(); return value_; }
		T        value() &&       { if (!ok_) std::abort(); return std::move(value_); }
		T        value() const && { if (!ok_) std::abort(); return value_; }
		const Error &error() const &  { if (ok_) std::abort(); return error_; }
		Error        error() &&       { if (ok_) std::abort(); return std::move(error_); }
		Error        error() const && { if (ok_) std::abort(); return error_; }

	private:
		// value_ is value initialised so the unread payload of a failure is
		// never an indeterminate value that copying the result would read.
		Result() : ok_(false), value_() {}
		bool  ok_;
		T     value_;
		Error error_;
};

template <>
class Result<void>
{
	public:
		static Result success() { Result r; r.ok_ = true; return r; }
		static Result failure(const Error &e) { Result r; r.ok_ = false; r.error_ = e; return r; }

		bool ok() const { return ok_; }

		const Error &error() const &  { if (ok_) std::abort(); return error_; }
		Error        error() &&       { if (ok_) std::abort(); return std::move(error_); }
		Error        error() const && { if (ok_) std::abort(); return error_; }

	private:
		Result() : ok_(false) {}
		bool  ok_;
		Error error_;
};

// Converts to a Result of any payload, so a failure can be returned or
// propagated without naming the payload type at the call site.
class Failure
{
	public:
		explicit Failure(const Error &e) : error_(e) {}
		template <typename T> operator Result<T>() const { return Result<T>::failure(error_); }

	private:
		Error error_;
};

// decay, not remove_reference: a const lvalue would otherwise deduce a const
// payload type and fail inside the class with an unreadable diagnostic.
template <typename T>
inline Result<typename std::decay<T>::type> ok(T &&v)
{
	return Result<typename std::decay<T>::type>::success(std::forward<T>(v));
}

inline Result<void> ok() { return Result<void>::success(); }

template <typename T>
inline Result<T> fail(Status s, ErrorCode code, const std::string &message)
{
	return Result<T>::failure(Error(s, code, message));
}

inline Failure fail(Status s, ErrorCode code, const std::string &message) { return Failure(Error(s, code, message)); }

inline Failure fail(const Error &e) { return Failure(e); }

} // namespace coreapi

#endif
