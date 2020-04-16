#include "KxStdAfx.h"
#include "UxTheme.h"
#include "DCOperations.h"
#include "Kx/Utility/Common.h"
#include "Kx/Utility/Drawing.h"
#include "Kx/System/SystemInformation.h"
#include <wx/fontutil.h>
#include <Uxtheme.h>
#include "Private/UxThemeDefines.h"

namespace
{
	constexpr const wchar_t* MapKxUxThemeClassToName(KxFramework::UxThemeClass themeClass) noexcept
	{
		using namespace KxFramework;

		switch (themeClass)
		{
			case UxThemeClass::Button:
			{
				return L"BUTTON";
			}
			case UxThemeClass::Clock:
			{
				return L"CLOCK";
			}
			case UxThemeClass::ComboBox:
			{
				return L"COMBOBOX";
			}
			case UxThemeClass::Communications:
			{
				return L"COMMUNICATIONS";
			}
			case UxThemeClass::ControlPanel:
			{
				return L"CONTROLPANEL";
			}
			case UxThemeClass::DatePicker:
			{
				return L"DATEPICKER";
			}
			case UxThemeClass::DragDrop:
			{
				return L"DRAGDROP";
			}
			case UxThemeClass::Edit:
			{
				return L"EDIT";
			}
			case UxThemeClass::ExplorerBar:
			{
				return L"EXPLORERBAR";
			}
			case UxThemeClass::FlyOut:
			{
				return L"FLYOUT";
			}
			case UxThemeClass::Globals:
			{
				return L"GLOBALS";
			}
			case UxThemeClass::Header:
			{
				return L"HEADER";
			}
			case UxThemeClass::ListBox:
			{
				return L"LISTBOX";
			}
			case UxThemeClass::ListView:
			{
				return L"LISTVIEW";
			}
			case UxThemeClass::Menu:
			{
				return L"MENU";
			}
			case UxThemeClass::MenuBand:
			{
				return L"MENUBAND";
			}
			case UxThemeClass::Navigation:
			{
				return L"NAVIGATION";
			}
			case UxThemeClass::Page:
			{
				return L"PAGE";
			}
			case UxThemeClass::Progress:
			{
				return L"PROGRESS";
			}
			case UxThemeClass::Rebar:
			{
				return L"REBAR";
			}
			case UxThemeClass::ScrollBar:
			{
				return L"SCROLLBAR";
			}
			case UxThemeClass::SearchEditBox:
			{
				return L"SEARCHEDITBOX";
			}
			case UxThemeClass::Spin:
			{
				return L"SPIN";
			}
			case UxThemeClass::StartPanel:
			{
				return L"STARTPANEL";
			}
			case UxThemeClass::Status:
			{
				return L"STATUS";
			}
			case UxThemeClass::Tab:
			{
				return L"TAB";
			}
			case UxThemeClass::TaskBand:
			{
				return L"TASKBAND";
			}
			case UxThemeClass::TaskBar:
			{
				return L"TASKBAR";
			}
			case UxThemeClass::TaskDialog:
			{
				return L"TASKDIALOG";
			}
			case UxThemeClass::TextStyle:
			{
				return L"TEXTSTYLE";
			}
			case UxThemeClass::ToolBar:
			{
				return L"TOOLBAR";
			}
			case UxThemeClass::ToolTip:
			{
				return L"TOOLTIP";
			}
			case UxThemeClass::TrackBar:
			{
				return L"TRACKBAR";
			}
			case UxThemeClass::TrayNotify:
			{
				return L"TRAYNOTIFY";
			}
			case UxThemeClass::TreeView:
			{
				return L"TREEVIEW";
			}
			case UxThemeClass::Window:
			{
				return L"WINDOW";
			}
		};
		return nullptr;
	}
}

namespace KxFramework
{
	bool UxTheme::ClearDC(wxWindow& window, wxDC& dc) noexcept
	{
		return DrawParentBackground(window, dc, wxRect(wxPoint(0, 0), dc.GetSize()));
	}
	bool UxTheme::DrawParentBackground(wxWindow& window, wxDC& dc) noexcept
	{
		return ::DrawThemeParentBackground(window.GetHandle(), dc.GetHDC(), nullptr) == S_OK;
	}
	bool UxTheme::DrawParentBackground(wxWindow& window, wxDC& dc, const wxRect& rect) noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		return ::DrawThemeParentBackground(window.GetHandle(), dc.GetHDC(), &rectWin) == S_OK;
	}
	
	Color UxTheme::GetDialogMainInstructionColor(const wxWindow& window) noexcept
	{
		if (UxTheme theme(const_cast<wxWindow&>(window), L"TEXTSTYLE"); theme)
		{
			Color color = theme.GetColor(TEXT_MAININSTRUCTION, 0, TMT_TEXTCOLOR);

			// Approximation of caption color for default Aero style
			return color ? color : System::GetColor(wxSYS_COLOUR_HOTLIGHT).ChangeLightness(Angle::FromDegrees(65));
		}
		return {};
	}

	void UxTheme::Open(wxWindow& window, const wchar_t* classes, UxThemeFlag flags) noexcept
	{
		DWORD dwFlags = 0;
		if (flags & UxThemeFlag::ForceRectSizing)
		{
			dwFlags |= OTD_FORCE_RECT_SIZING;
		}
		if (flags & UxThemeFlag::NonClient)
		{
			dwFlags |= OTD_NONCLIENT;
		}

		m_Window = &window;
		if (dwFlags != 0)
		{
			m_Handle = ::OpenThemeDataEx(window.GetHandle(), classes, dwFlags);
		}
		else
		{
			m_Handle = ::OpenThemeData(window.GetHandle(), classes);
		}
	}
	void UxTheme::Close() noexcept
	{
		if (m_Handle)
		{
			::CloseThemeData(m_Handle);
			m_Handle = nullptr;
		}
		m_Window = nullptr;
	}

	UxTheme::UxTheme(wxWindow& window, UxThemeClass KxUxThemeClass, UxThemeFlag flags) noexcept
	{
		if (const wchar_t* name = MapKxUxThemeClassToName(KxUxThemeClass))
		{
			Open(window, name, flags);
		}
	}

	wxSize UxTheme::GetPartSize(const wxDC& dc, int iPartId, int iStateId, std::optional<int> sizeVariant) const noexcept
	{
		const THEMESIZE themeSize = static_cast<THEMESIZE>(sizeVariant ? *sizeVariant : TS_DRAW);

		SIZE size = {};
		if (::GetThemePartSize(m_Handle, dc.GetHDC(), iPartId, iStateId, nullptr, themeSize, &size) == S_OK)
		{
			return wxSize(size.cx, size.cy);
		}
		return wxDefaultSize;
	}
	wxRegion UxTheme::GetBackgroundRegion(const wxDC& dc, int iPartId, int iStateId, const wxRect& rect) const noexcept
	{
		HRGN region = nullptr;
		RECT rectWin = Utility::ToWindowsRect(rect);
		if (::GetThemeBackgroundRegion(m_Window->GetHandle(), dc.GetHDC(), iPartId, iStateId, &rectWin, &region) == S_OK)
		{
			return region;
		}
		return {};
	}
	std::optional<wxRect> UxTheme::GetBackgroundContentRect(const wxDC& dc, int iPartId, int iStateId, const wxRect& rect) const noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		RECT value = {};
		if (::GetThemeBackgroundContentRect(m_Window->GetHandle(), dc.GetHDC(), iPartId, iStateId, &rectWin, &value) == S_OK)
		{
			return Utility::FromWindowsRect(value);
		}
		return std::nullopt;
	}

	Color UxTheme::GetColor(int iPartId, int iStateId, int iPropId) const noexcept
	{
		COLORREF value = 0;
		if (::GetThemeColor(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return Color::FromCOLORREF(value);
		}
		return {};
	}
	wxFont UxTheme::GetFont(const wxDC& dc, int iPartId, int iStateId, int iPropId) const noexcept
	{
		LOGFONTW value = {};
		if (::GetThemeFont(m_Window->GetHandle(), dc.GetHDC(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return wxNativeFontInfo(value);
		}
		return wxNullFont;
	}
	std::optional<bool> UxTheme::GetBool(int iPartId, int iStateId, int iPropId) const noexcept
	{
		BOOL value = FALSE;
		if (::GetThemeBool(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return value;
		}
		return std::nullopt;
	}
	std::optional<int> UxTheme::GetInt(int iPartId, int iStateId, int iPropId) const noexcept
	{
		int value = -1;
		if (::GetThemeInt(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return value;
		}
		return std::nullopt;
	}
	std::optional<int> UxTheme::GetEnum(int iPartId, int iStateId, int iPropId) const noexcept
	{
		int value = -1;
		if (::GetThemeEnumValue(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return value;
		}
		return std::nullopt;
	}
	size_t UxTheme::GetIntList(int iPartId, int iStateId, int iPropId, std::function<bool(int)> func) const
	{
		INTLIST items = {};
		if (::GetThemeIntList(m_Window->GetHandle(), iPartId, iStateId, iPropId, &items) == S_OK)
		{
			size_t count = 0;
			for (size_t i = 0; i < static_cast<size_t>(items.iValueCount); i++)
			{
				count++;
				if (!std::invoke(func, items.iValues[i]))
				{
					break;
				}
			}
			return 0;
		}
		return 0;
	}
	wxRect UxTheme::GetRect(int iPartId, int iStateId, int iPropId) const noexcept
	{
		RECT value = {};
		if (::GetThemeRect(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return Utility::FromWindowsRect(value);
		}
		return {};
	}
	wxPoint UxTheme::GetPosition(int iPartId, int iStateId, int iPropId) const noexcept
	{
		POINT value = {};
		if (::GetThemePosition(m_Window->GetHandle(), iPartId, iStateId, iPropId, &value) == S_OK)
		{
			return wxPoint(value.x, value.y);
		}
		return wxDefaultPosition;
	}

	bool UxTheme::DrawEdge(wxDC& dc, int iPartId, int iStateId, uint32_t edge, uint32_t flags, const wxRect& rect, wxRect* boundingRect) noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		RECT clipRectWin = {};
		if (::DrawThemeEdge(m_Handle, dc.GetHDC(), iPartId, iStateId, &rectWin, edge, flags, &clipRectWin) == S_OK)
		{
			Utility::SetIfNotNull(boundingRect, Utility::FromWindowsRect(clipRectWin));
			return true;
		}
		return false;
	}
	bool UxTheme::DrawIcon(wxDC& dc, int iPartId, int iStateId, const wxImageList& imageList, int index, const wxRect& rect, wxRect* boundingRect) noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		if (::DrawThemeIcon(m_Handle, dc.GetHDC(), iPartId, iStateId, &rectWin, reinterpret_cast<HIMAGELIST>(imageList.GetHIMAGELIST()), index) == S_OK)
		{
			if (boundingRect)
			{
				boundingRect->x = rect.x;
				boundingRect->y = rect.y;
				imageList.GetSize(index, boundingRect->width, boundingRect->height);
			}
			return true;
		}
		return false;
	}
	bool UxTheme::DrawText(wxDC& dc, int iPartId, int iStateId, std::wstring_view text, uint32_t flags1, uint32_t flags2, const wxRect& rect) noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		return ::DrawThemeText(m_Handle, dc.GetHDC(), iPartId, iStateId, text.data(), text.length(), flags1, flags2, &rectWin) == S_OK;
	}

	bool UxTheme::DrawBackground(wxDC& dc, int iPartId, int iStateId, const wxRect& rect) noexcept
	{
		RECT rectWin = Utility::ToWindowsRect(rect);
		return ::DrawThemeBackground(m_Handle, dc.GetHDC(), iPartId, iStateId, &rectWin, nullptr) == S_OK;
	}
	bool UxTheme::DrawProgressBar(wxDC& dc, int iBarPartId, int iFillPartId, int iFillStateId, const wxRect& rect, int position, int range, Color* averageBackgroundColor) noexcept
	{
		const bool isVertical = iBarPartId == PP_BARVERT && iFillPartId == PP_FILLVERT;
		const wxSize padding = isVertical ? wxSize(0, 0) : m_Window->FromDIP(wxSize(2, 0));

		// Draw background part
		bool result = true;
		wxRect fillRect = rect;
		if (iBarPartId > 0)
		{
			result = DrawBackground(dc, iBarPartId, 0, rect.Deflate(padding.GetWidth(), padding.GetHeight()));
			fillRect = GetBackgroundContentRect(dc, iBarPartId, 0, rect).value_or(rect);
		}

		// Draw filled part
		if (iFillStateId > 0)
		{
			if (position != range)
			{
				fillRect.SetWidth(fillRect.GetWidth() * ((double)position / (double)range));
			}

			if (iBarPartId > 0)
			{
				fillRect.Deflate(m_Window->FromDIP(wxSize(1, 1)) + padding);
			}
			result = DrawBackground(dc, iFillPartId, iFillStateId, fillRect);

			if (averageBackgroundColor)
			{
				*averageBackgroundColor = KxFramework::Drawing::GetAreaAverageColor(dc, fillRect);
			}
		}
		return result;
	}

	UxTheme& UxTheme::operator=(UxTheme&& other) noexcept
	{
		Close();

		m_Handle = Utility::ExchangeResetAndReturn(other.m_Handle, nullptr);
		m_Window = Utility::ExchangeResetAndReturn(other.m_Window, nullptr);
		return *this;
	}
}