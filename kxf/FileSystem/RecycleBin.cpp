#include "stdafx.h"
#include "RecycleBin.h"
#include "NativeFileSystem.h"
#include "kxf/System/ShellOperations.h"
#include "kxf/Utility/Common.h"
#include <shellapi.h>
#include "kxf/System/UndefWindows.h"

namespace
{
	kxf::NativeFileSystem g_NativeFS;

	// https://stackoverflow.com/questions/16160052/win32api-restore-file-from-recyclebin-using-shfilestruct
	// https://stackoverflow.com/questions/23720519/how-to-safely-delete-folder-into-recycle-bin
	// https://www.codeproject.com/Articles/2783/How-to-programmatically-use-the-Recycle-Bin
	// https://oipapio.com/question-589504

	std::optional<SHQUERYRBINFO> QueryRecycleBin(const wxChar* path)
	{
		SHQUERYRBINFO queryInfo = {};
		queryInfo.cbSize = sizeof(queryInfo);

		if (::SHQueryRecycleBinW(path, &queryInfo) == S_OK)
		{
			return queryInfo;
		}
		return {};
	}
}

namespace kxf
{
	RecycleBin::RecycleBin(LegacyVolume volume)
		:RecycleBin(volume, g_NativeFS)
	{
	}
	RecycleBin::RecycleBin(LegacyVolume volume, IFileSystem& fileSystem)
		:m_Volume(volume), m_FileSystem(&fileSystem)
	{
		if (volume)
		{
			m_Path[0] = volume.GetChar();
			m_Path[1] = wxS(':');
			m_Path[2] = wxS('\\');
			m_Path[3] = wxS('\0');
		}
	}

	bool RecycleBin::IsEnabled() const
	{
		return m_Volume && !GetSize().IsValid();
	}
	void RecycleBin::SetWindow(wxWindow* window)
	{
		m_Window = wxGetTopLevelParent(window);
	}

	BinarySize RecycleBin::GetSize() const
	{
		if (auto queryInfo = QueryRecycleBin(m_Path))
		{
			return BinarySize::FromBytes(queryInfo->i64Size);
		}
		return {};
	}
	size_t RecycleBin::GetItemCount() const
	{
		if (auto queryInfo = QueryRecycleBin(m_Path))
		{
			return queryInfo->i64NumItems;
		}
		return 0;
	}
	bool RecycleBin::ClearItems(FlagSet<FSRecycleBinOpFlag> flags)
	{
		DWORD emptyFlags = m_Window ? 0 : SHERB_NOCONFIRMATION|SHERB_NOPROGRESSUI|SHERB_NOSOUND;
		return ::SHEmptyRecycleBinW(m_Window ? m_Window->GetHandle() : nullptr, m_Path, emptyFlags) == S_OK;
	}

	FileItem RecycleBin::GetItem(const FSPath& path) const
	{
		throw std::logic_error(__FUNCTION__ ": the method or operation is not implemented.");
	}
	size_t RecycleBin::EnumItems(std::function<bool(FileItem)> func) const
	{
		throw std::logic_error(__FUNCTION__ ": the method or operation is not implemented.");
	}
	
	bool RecycleBin::Recycle(const FSPath& path, FlagSet<FSRecycleBinOpFlag> flags)
	{
		if (path.ContainsSearchMask())
		{
			if (flags & FSRecycleBinOpFlag::Recursive)
			{
				FlagSet<SHOperationFlags> shellFlags = SHOperationFlags::AllowUndo|SHOperationFlags::Recursive;
				shellFlags.Add(SHOperationFlags::LimitToFiles, flags & FSRecycleBinOpFlag::LimitToFiles);

				return Shell::FileOperation(SHOperationType::Delete, path, {}, m_Window, shellFlags);
			}
			return false;
		}
		else
		{
			if (FileItem fileItem = m_FileSystem->GetItem(path))
			{
				if (fileItem.IsDirectory())
				{
					FlagSet<SHOperationFlags> shellFlags = SHOperationFlags::AllowUndo;
					shellFlags.Add(SHOperationFlags::Recursive, flags & FSRecycleBinOpFlag::Recursive);

					return Shell::FileOperation(SHOperationType::Delete, path, {}, m_Window, shellFlags);
				}
				else
				{
					return Shell::FileOperation(SHOperationType::Delete, path, {}, m_Window, SHOperationFlags::AllowUndo|SHOperationFlags::LimitToFiles);
				}
			}
		}
		return false;
	}
	bool RecycleBin::Restore(const FSPath& path, FlagSet<FSRecycleBinOpFlag> flags)
	{
		throw std::logic_error(__FUNCTION__ ": the method or operation is not implemented.");
	}
}
