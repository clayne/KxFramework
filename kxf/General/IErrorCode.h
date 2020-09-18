#pragma once
#include "Common.h"
#include "kxf/General/String.h"
#include "kxf/Localization/Locale.h"

namespace kxf
{
	class KX_API IErrorCode: public RTTI::Interface<IErrorCode>
	{
		KxRTTI_DeclareIID(IErrorCode, {0x79dd38bf, 0xec09, 0x4d32, {0x99, 0x4b, 0xa7, 0x82, 0x51, 0x90, 0x34, 0x12}});

		public:
			IErrorCode() noexcept = default;
			virtual ~IErrorCode() = default;

		public:
			virtual bool IsSuccess() const noexcept = 0;
			virtual bool IsFail() const noexcept = 0;

			virtual uint32_t GetValue() const noexcept = 0;
			virtual void SetValue(uint32_t value) noexcept = 0;
			
			virtual String ToString() const = 0;
			virtual String GetMessage(const Locale& locale = {}) const = 0;

		public:
			explicit operator bool() const noexcept
			{
				return IsSuccess();
			}
			bool operator!() const noexcept
			{
				return IsFail();
			}

			uint32_t operator*() const noexcept
			{
				return GetValue();
			}
	};
}
