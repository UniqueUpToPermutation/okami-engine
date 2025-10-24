#pragma once

#include <stdexcept>
#include <expected>
#include <functional>
#include <variant>
#include <string_view>
#include <string>
#include <iostream>

#define OKAMI_NO_COPY(x) \
	x(const x&) = delete; \
	x& operator=(const x&) = delete; \

#define OKAMI_NO_MOVE(x) \
	x(x&&) = delete; \
	x& operator=(x&&) = delete; \

#define OKAMI_MOVE(x) \
	x(x&&) = default; \
	x& operator=(x&&) = default; \

#ifndef NDEBUG 
#define OKAMI_ASSERT(condition, message) \
	do { \
		if (!(condition)) { \
			throw std::runtime_error(message); \
		} \
	} while (false) 
#else
#define OKAMI_ASSERT(condition, message) 
#endif

#define OKAMI_DEFER(x) auto OKAMI_DEFER_##__LINE__ = okami::ScopeGuard([&]() { x; })

#define OKAMI_ERROR_RETURN(x) if (IsError(x)) { return okami::MakeError(std::move(x)); }
#define OKAMI_UNEXPECTED_RETURN(x) \
{ \
	auto tmp = x; if (IsError(tmp)) { return std::unexpected(okami::MakeError(std::move(tmp))); } \
}

namespace okami {
	struct Error {
		std::variant<std::monostate, 
			std::string_view, 
			std::string,
			std::vector<Error>> m_contents;

		Error() : m_contents(std::monostate{}) {}
		Error(const std::string& msg) : m_contents(msg) {}
		Error(const char* msg) : m_contents(std::string_view{ msg }) {}
		Error(std::vector<Error> errors) : m_contents(std::move(errors)) {}
		Error& Union(const Error& other);

		inline Error& operator+=(const Error& other) {
			return Union(other);
		}

		inline bool IsOk() const {
			return std::holds_alternative<std::monostate>(m_contents);
		}

		inline bool IsError() const {
			return !IsOk();
		}

		inline friend std::ostream& operator<<(std::ostream& os, const Error& err) {
			os << err.Str();
			return os;
		}

		std::string Str() const;
	};
	
	template <typename T>
	using Expected = std::expected<T, Error>;

	inline bool IsError(Error const& err) {
		return err.IsError();
	}

	template <typename T> bool IsError(Expected<T> const& expected) {
		return !expected.has_value();
	}

	inline Error MakeError(Error err) {
		return err;
	}

	template <typename T>
	inline Error MakeError(Expected<T> expected) {
		if (!expected) {
			return expected.error();
		}
		return {};
	}

	template <typename T>
	struct TypeWrapper {
		using Type = T;
	};

	class ScopeGuard {
	public:
		inline explicit ScopeGuard(std::function<void()> onExitScope)
			: m_onExitScope(std::move(onExitScope)), m_active(true) {
		}

		inline ScopeGuard(ScopeGuard&& other) noexcept
			: m_onExitScope(std::move(other.m_onExitScope)), m_active(other.m_active) {
			other.m_active = false;
		}

		inline ScopeGuard& operator=(ScopeGuard&& other) noexcept {
			if (this != &other) {
				if (m_active && m_onExitScope) m_onExitScope();
				m_onExitScope = std::move(other.m_onExitScope);
				m_active = other.m_active;
				other.m_active = false;
			}
			return *this;
		}

		inline ~ScopeGuard() {
			if (m_active && m_onExitScope) m_onExitScope();
		}

		// Prevent copy
		ScopeGuard(const ScopeGuard&) = delete;
		ScopeGuard& operator=(const ScopeGuard&) = delete;

		inline void Dismiss() noexcept { m_active = false; }

	private:
		std::function<void()> m_onExitScope;
		bool m_active;
	};
}