#pragma once
#include "kxf/Common.hpp"
#include "kxf/General/String.h"
#include <unordered_map>
#include <unordered_set>

namespace kxf
{
	class IEncodingConverter;
}

namespace kxf::Utility
{
	struct StringEqualToNoCase final
	{
		template<class T>
		bool operator()(T&& left, T&& right) const noexcept
		{
			return String::Compare(std::forward<T>(left), std::forward<T>(right), StringActionFlag::IgnoreCase) == 0;
		}
	};

	struct StringHashNoCase final
	{
		// From Boost
		template<class T>
		static void hash_combine(size_t& seed, const T& v) noexcept
		{
			std::hash<T> hasher;
			seed ^= hasher(v) + static_cast<size_t>(0x9e3779b9u) + (seed << 6) + (seed >> 2);
		}

		template<class T>
		size_t operator()(T&& value) const noexcept
		{
			size_t hashValue = 0;
			for (const auto& c: value)
			{
				hash_combine(hashValue, String::ToLower(c).GetValue());
			}
			return hashValue;
		}
	};

	template<class TKey, class TValue>
	using UnorderedMapNoCase = std::unordered_map<TKey, TValue, StringHashNoCase, StringEqualToNoCase>;
	
	template<class TValue>
	using UnorderedSetNoCase = std::unordered_set<TValue, StringHashNoCase, StringEqualToNoCase>;
}

namespace kxf::Utility
{
	class StringBuffer final
	{
		private:
			enum class Type
			{
				None = -1,

				NarrowChars,
				WideChars
			};

		private:
			String& m_Value;
			size_t m_Length = 0;

			Type m_Type = Type::None;
			std::string m_NarrowChars;
			std::wstring m_WideChars;
			IEncodingConverter* m_EncodingConverter = nullptr;

		private:
			char* PrepareNarrowChars();
			wchar_t* PrepareWideChars();
			void Finalize();

		public:
			StringBuffer(String& value, size_t length) noexcept
				:m_Value(value), m_Length(length)
			{
			}
			StringBuffer(String& value, size_t length, IEncodingConverter& encondigConverter) noexcept
				:m_Value(value), m_Length(length), m_EncodingConverter(&encondigConverter)
			{
			}
			~StringBuffer()
			{
				Finalize();
			}

		public:
			char* nc_str()
			{
				return PrepareNarrowChars();
			}
			wchar_t* wc_str()
			{
				return PrepareWideChars();
			}
			size_t length() const noexcept
			{
				return m_Length;
			}

		public:
			operator char*()
			{
				return PrepareNarrowChars();
			}
			operator wchar_t*()
			{
				return PrepareWideChars();
			}
	};
}

namespace kxf::Utility
{
	KX_API std::optional<bool> ParseBool(const String& value);
}
