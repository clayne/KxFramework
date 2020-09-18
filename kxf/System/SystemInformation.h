#pragma once
#include "Common.h"
#include "SystemInformationDefines.h"
#include "UserName.h"
#include "kxf/General/String.h"
#include "kxf/General/Color.h"
#include "kxf/General/Version.h"
#include "kxf/General/BinarySize.h"
#include "kxf/General/DateTime.h"
#include "kxf/General/LocallyUniqueID.h"
#include "kxf/Drawing/Geometry.h"
#include <wx/settings.h>
#include <wx/gdicmn.h>
#include "UndefWindows.h"

namespace kxf::System
{
	struct KernelVersion final
	{
		public:
			int Major = -1;
			int Minor = -1;
			int Build = -1;

			int ServicePackMajor = -1;
			int ServicePackMinor = -1;

		public:
			KernelVersion() noexcept = default;
			KernelVersion(int major, int minor = 0, int build = 0) noexcept
				:Major(major), Minor(minor), Build(build)
			{
			}

		public:
			operator Version() const noexcept
			{
				return {Major, Minor, Build, ServicePackMajor, ServicePackMinor};
			}
	};
	struct VersionInfo final
	{
		public:
			KernelVersion Kernel;

			String ServicePack;
			SystemType SystemType = SystemType::Unknown;
			SystemPlatformID PlatformID = SystemPlatformID::Unknown;
			SystemProductType ProductType = SystemProductType::Unknown;
			FlagSet<SystemProductSuite> ProductSuite;

		public:
			VersionInfo() noexcept = default;
			VersionInfo(int major, int minor = 0, int build = 0) noexcept
				:Kernel(major, minor, build)
			{
			}
	};

	struct MemoryStatus final
	{
		BinarySize TotalRAM;
		BinarySize TotalVirtual;
		BinarySize TotalPageFile;
		BinarySize AvailableRAM;
		BinarySize AvailableVirtual;
		BinarySize AvailablePageFile;
		float MemoryLoad = 0.0f;
	};
	struct UserInfo final
	{
		UserName Name;
		String Organization;
		bool AdminRights = false;
		bool LimitedAdminRights = false;
	};
	struct DisplayInfo final
	{
		int Width = 0;
		int Height = 0;
		int BitDepth = 0;
		int RefreshRate = 0;
	};
	struct DisplayDeviceInfo final
	{
		String DeviceName;
		String DeviceDescription;
		FlagSet<DisplayDeviceFlag> Flags;
	};
	struct DisplayAdapterInfo final
	{
		String Name;
		uint32_t Index = 0;
		uint32_t VendorID = 0;
		uint32_t DeviceID = 0;
		uint32_t SubSystemID = 0;
		uint32_t Revision = 0;
		LocallyUniqueID UniqueID;
		FlagSet<DisplayAdapterFlag> Flags;
		BinarySize DedicatedVideoMemory;
		BinarySize DedicatedSystemMemory;
		BinarySize SharedSystemMemory;
	};
}

namespace kxf::System
{
	bool Is64Bit() noexcept;
	void GetRegistryQuota(BinarySize& used, BinarySize& allowed) noexcept;

	String GetProductName();
	String GetProductName(const VersionInfo& versionInfo, bool is64Bit);

	std::optional<KernelVersion> GetKernelVersion() noexcept;
	std::optional<VersionInfo> GetVersionInfo() noexcept;
	std::optional<MemoryStatus> GetGlobalMemoryStatus() noexcept;
	BinarySize GetPhysicallyInstalledMemory() noexcept;
	
	std::optional<UserInfo> GetUserInfo();
	String GetUserSID();

	Color GetColor(SystemColor index) noexcept;
	wxFont GetFont(SystemFont index);
	SystemScreenType GetScreenType() noexcept;
	int GetMetric(SystemMetric index, const wxWindow* window = nullptr) noexcept;
	Size GetMetric(SystemSizeMetric index, const wxWindow* window = nullptr) noexcept;
	TimeSpan GetMetric(SystemTimeMetric index, const wxWindow* window = nullptr) noexcept;
	bool HasFeature(SystemFeature feature) noexcept;
	size_t EnumStandardSounds(std::function<bool(String)> func);

	std::optional<DisplayInfo> GetDisplayInfo() noexcept;
	size_t EnumDisplayModes(std::function<bool(DisplayInfo)> func, const String& deviceName = {});
	size_t EnumDisplayDevices(std::function<bool(DisplayDeviceInfo)> func);
	size_t EnumDisplayAdapters(std::function<bool(DisplayAdapterInfo)> func);

	String ExpandEnvironmentStrings(const String& strings);
	String GetEnvironmentVariable(const String& name);
	bool SetEnvironmentVariable(const String& name, const String& value);
	size_t EnumEnvironmentVariables(std::function<bool(String, String)> func);

	bool LockWorkstation(LockWorkstationCommand command) noexcept;
	bool ExitWorkstation(FlagSet<ExitWorkstationCommand> command) noexcept;

	bool IsWindowsServer() noexcept;
	bool IsWindowsVersionOrGreater(int majorVersion, int minorVersion, int servicePackMajor = -1) noexcept;
	inline bool IsWindows10OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(10, 0);
	}
	inline bool IsWindows8Point1OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 3);
	}
	inline bool IsWindows8OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 2);
	}
	inline bool IsWindows7SP1OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 1, 1);
	}
	inline bool IsWindows7OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 1);
	}
	inline bool IsWindowsVistaSP2OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 0, 2);
	}
	inline bool IsWindowsVistaSP1OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 0, 1);
	}
	inline bool IsWindowsVistaOrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(6, 0);
	}
	inline bool IsWindowsXPSP3OrGreater()
	{
		return IsWindowsVersionOrGreater(5, 1, 3);
	}
	inline bool IsWindowsXPSP2OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(5, 1, 2);
	}
	inline bool IsWindowsXPSP1OrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(5, 1, 1);
	}
	inline bool IsWindowsXPOrGreater() noexcept
	{
		return IsWindowsVersionOrGreater(5, 1);
	}
};
