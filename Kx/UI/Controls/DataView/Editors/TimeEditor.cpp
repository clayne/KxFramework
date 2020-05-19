#include "stdafx.h"
#include "TimeEditor.h"
#include "DateEditor.h"
#include <wx/timectrl.h>

namespace
{
	wxDateTime ResetDate(const wxDateTime& date)
	{
		wxDateTime copy = date;
		copy.SetDay(0);
		copy.SetMonth(wxDateTime::Inv_Month);
		copy.SetYear(wxDateTime::Inv_Year);

		return copy;
	}
}

namespace KxFramework::UI::DataView
{
	wxWindow* TimeEditor::CreateControl(wxWindow* parent, const wxRect& cellRect, const wxAny& value)
	{
		const DateTimeValue dateValue = FromAnyUsing<DateTimeValue>(value);

		return new wxTimePickerCtrl(parent,
									wxID_NONE,
									ResetDate(dateValue.GetDateTime()),
									cellRect.GetPosition(),
									cellRect.GetSize(),
									0,
									GetValidator()
		);
	}
	wxAny TimeEditor::GetValue(wxWindow* control) const
	{
		wxTimePickerCtrl* editor = static_cast<wxTimePickerCtrl*>(control);
		return ResetDate(editor->GetValue());
	}
}