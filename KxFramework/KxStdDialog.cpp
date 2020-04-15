#include "KxStdAfx.h"
#include "KxFramework/KxStdDialog.h"
#include "KxFramework/KxTopLevelWindow.h"
#include "KxFramework/KxButton.h"
#include "KxFramework/KxIncludeWindows.h"
#include "Kx/Drawing/UxTheme.h"
#include "Kx/Utility/Common.h"
#include "Kx/Utility/System.h"

using namespace KxFramework;

KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_BUTTON);
KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_NAVIGATE);
KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_NAVIGATING);
KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_OVERWRITE);
KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_SELECT);
KxEVENT_DEFINE_GLOBAL(wxNotifyEvent, STDDIALOG_TYPE_CHANGED);

wxIMPLEMENT_ABSTRACT_CLASS(KxStdDialog, wxDialog);

namespace
{
	int FromDIPX(const wxWindow* window, int value)
	{
		return window->FromDIP(wxSize(value, wxDefaultCoord)).GetWidth();
	}
	int FromDIPY(const wxWindow* window, int value)
	{
		return window->FromDIP(wxSize(wxDefaultCoord, value)).GetHeight();
	}
}

bool KxStdDialog::ms_IsBellAllowed = false;

const KxStdDialog::StdButtonsIDs KxStdDialog::ms_DefaultCloseIDs = {wxID_OK, wxID_YES, wxID_NO, wxID_CANCEL, wxID_APPLY, wxID_CLOSE};
const KxStdDialog::StdButtonsIDs KxStdDialog::ms_DefaultEnterIDs = {wxID_OK, wxID_YES, wxID_APPLY};
const wxColour KxStdDialog::ms_WindowBackgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_MENU);
const wxColour KxStdDialog::ms_LineBackgroundColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);

wxWindowID KxStdDialog::TranslateButtonConstantsToIDs(int btnValue)
{
	switch (FromInt<StdButton>(btnValue))
	{
		case StdButton::OK:
		{
			return wxID_OK;
		}
		case StdButton::Yes:
		{
			return wxID_YES;
		}
		case StdButton::No:
		{
			return wxID_NO;
		}
		case StdButton::Cancel:
		{
			return wxID_CANCEL;
		}
		case StdButton::Apply:
		{
			return wxID_APPLY;
		}
		case StdButton::Close:
		{
			return wxID_CLOSE;
		}
		case StdButton::Help:
		{
			return wxID_HELP;
		}
		case StdButton::Forward:
		{
			return wxID_FORWARD;
		}
		case StdButton::Backward:
		{
			return wxID_BACKWARD;
		}
		case StdButton::Reset:
		{
			return wxID_RESET;
		}
		case StdButton::More:
		{
			return wxID_MORE;
		}
		case StdButton::Setup:
		{
			return wxID_SETUP;
		}
		case StdButton::Retry:
		{
			return wxID_RETRY;
		}
	};
	return wxID_NONE;
}
bool KxStdDialog::MSWTranslateMessage(WXMSG* msg)
{
	if (false && msg->message == WM_SYSKEYDOWN && msg->wParam == VK_F4 && ::GetKeyState(VK_MENU) < 0)
	{
		//wxMessageBox("MSWTranslateMessage: Alt+F4");
		return true; //Do nothing, we are processing, not the System
	}
	return KxDialog::MSWTranslateMessage(msg);
}

void KxStdDialog::EnableGlassFrame()
{
	SetGripperVisible(!m_IsGlassFrameEnabled);
	if (m_IsGlassFrameEnabled)
	{
		wxColour color = DWMGetColorKey();
		SetOwnBackgroundColour(color);
		m_ContentPanelLine->SetOwnBackgroundColour(color);
		DWMExtendFrame(GetGlassRect(), color);
		//GetContentWindow()->Bind(wxEVT_PAINT, &KxStdDialog::OnDrawFrameBorder, this);
	}
	else
	{
		SetOwnBackgroundColour(ms_WindowBackgroundColor);
		m_ContentPanelLine->SetOwnBackgroundColour(ms_LineBackgroundColor);
		DWMExtendFrame();
		//GetContentWindow()->Unbind(wxEVT_PAINT, &KxStdDialog::OnDrawFrameBorder, this);
	}
}
void KxStdDialog::OnDrawFrameBorder(wxPaintEvent& event)
{
	using namespace KxFramework;

	wxAutoBufferedPaintDC dc(GetContentWindow());
	dc.Clear();
	dc.SetPen(KxTopLevelWindow::DWMGetGlassColor().ChangeLightness(Angle::FromNormalized(0.6f)));
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	dc.SetBackground(*wxTRANSPARENT_BRUSH);

	dc.DrawRectangle(GetContentWindow()->GetRect());
	event.Skip();
}

void KxStdDialog::OnStdButtonClick(wxCommandEvent& event)
{
	const wxWindowID oldReturnCode = GetReturnCode();
	const wxWindowID buttonID = event.GetId();

	for (wxWindowID closeID: m_CloseIDs)
	{
		if (closeID == buttonID)
		{
			const wxWindow* button = GetButton(buttonID).GetControl();

			wxNotifyEvent onEscapeEvent(KxEVT_STDDIALOG_BUTTON, buttonID);
			onEscapeEvent.SetEventObject(this);
			ProcessEvent(onEscapeEvent);
			if (onEscapeEvent.IsAllowed() && (button == nullptr || button->IsThisEnabled()))
			{
				SetReturnCode(buttonID);
				EndModal(buttonID);
				Close();
			}
			else
			{
				SetReturnCode(oldReturnCode);
			}
			break;
		}
	}
	event.Skip(false);
}
void KxStdDialog::OnClose(wxCloseEvent& event)
{
	if (!event.GetVeto())
	{
		if (IsModal())
		{
			EndModal(GetReturnCode());
		}
		Destroy();
	}
}
void KxStdDialog::OnEscape(wxKeyEvent& event)
{
	event.Skip();
	wxWindowID id = wxID_NONE;
	if (event.GetKeyCode() == WXK_ESCAPE && IsEscapeAllowed())
	{
		wxNotifyEvent onEscapeEvent(KxEVT_STDDIALOG_BUTTON, wxID_CANCEL);
		onEscapeEvent.SetEventObject(this);
		ProcessEvent(onEscapeEvent);

		EndModal(wxID_CANCEL);
		Close();
	}
	else if (event.GetKeyCode() == WXK_RETURN && IsEnterAllowed(event, &id))
	{
		if (id != wxID_NONE)
		{
			wxCommandEvent buttonEvent(KxEVT_STDDIALOG_BUTTON, id);
			OnStdButtonClick(buttonEvent);
			event.Skip(false);
		}
	}
	else 
	{
		if (IsBellAllowed())
		{
			if (!GetDialogMainCtrl()->HasFocus() && !event.HasAnyModifiers() && event.GetKeyCode() != WXK_TAB && !IsUserWindowHasFocus())
			{
				wxBell();
			}
		}
	}
}

bool KxStdDialog::Create(wxWindow* parent,
						 wxWindowID id,
						 const wxString& caption,
						 const wxPoint& pos,
						 const wxSize& size,
						 StdButton buttons,
						 long style
)
{
	m_SelectedButtons = buttons;
	if (!IsCloseBoxEnabled())
	{
		style &= ~wxCLOSE_BOX;
	}

	if (KxDialog::Create(parent, id, GetDefaultTitle(), pos, size, style))
	{
		using namespace KxFramework;

		// Default interface
		m_GripperWindow.Create(this, wxID_NONE, GetGripperWindow());
		m_GripperWindowSize = m_GripperWindow.GetSize();
		SetBackgroundColour(ms_WindowBackgroundColor);

		m_ContentPanel = new KxPanel(this, wxID_NONE);
		m_ContentPanel->SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW));

		m_CaptionLabel = new KxLabel(m_ContentPanel, wxID_NONE, caption, KxLABEL_CAPTION); // wxLLabel::DefaultStyle|KxLABEL_CAPTION
		m_CaptionLabel->SetMaxSize(FromDIP(wxSize(wxDefaultCoord, 23)));
		m_CaptionLabel->SetForegroundColour(UxTheme::GetDialogMainInstructionColor(*m_ContentPanel));

		m_ViewLabel = new KxLabel(m_ContentPanel, wxID_NONE, wxEmptyString, KxLabel::DefaultStyle & ~(KxLABEL_LINE|KxLABEL_COLORED|KxLABEL_CAPTION|KxLABEL_SELECTION));
		m_ViewLabel->SetMaxSize(FromDIP(wxSize(wxDefaultCoord, 23)));

		m_ContentPanelLine = new KxPanel(this, wxID_NONE, wxBORDER_NONE);
		m_ContentPanelLine->SetPosition(wxPoint(0, 0));
		m_ContentPanelLine->SetBackgroundColour(ms_LineBackgroundColor);
		m_ContentPanelLine->SetMaxSize(FromDIP(wxSize(wxDefaultCoord, 1)));

		m_IconView = new wxStaticBitmap(m_ContentPanel, wxID_NONE, m_MainIcon);
		if (m_MainIcon.IsOk())
		{
			m_IconView->SetMinSize(m_MainIcon.GetSize());
		}
		else
		{
			m_IconView->SetSize(m_MainIcon.GetSize());
		}

		// Default sizers
		m_DialogSizer = new wxBoxSizer(wxVERTICAL);
		SetSizer(m_DialogSizer);

		m_ContentSizerBase = new wxBoxSizer(wxHORIZONTAL);

		m_IconSizer = new wxBoxSizer(wxVERTICAL);
		m_ContentSizer = new wxBoxSizer(wxVERTICAL);
		m_ButtonsSizer = CreateStdDialogButtonSizer(m_SelectedButtons);

		// Events
		Bind(wxEVT_BUTTON, &KxStdDialog::OnStdButtonClick, this);
		Bind(wxEVT_CLOSE_WINDOW, &KxStdDialog::OnClose, this);
		Bind(wxEVT_CHAR_HOOK, &KxStdDialog::OnEscape, this);

		SetEscapeID(wxID_NONE);
		SetAffirmativeID(wxID_AUTO_LOWEST - 1);
		SetReturnCode(wxID_CANCEL);
		return true;
	}
	return false;
}
KxStdDialog::~KxStdDialog()
{
}

wxOrientation KxStdDialog::GetViewSizerOrientation() const
{
	return wxVERTICAL;
}
wxOrientation KxStdDialog::GetViewLabelSizerOrientation() const
{
	return wxVERTICAL;
}
bool KxStdDialog::IsEscapeAllowed(wxWindowID* idOut) const
{
	if (m_SelectedButtons & StdButton::OK || m_SelectedButtons & StdButton::Cancel || m_SelectedButtons & StdButton::No || m_SelectedButtons & StdButton::Close)
	{
		for (int id: m_CloseIDs)
		{
			if (id == wxID_CANCEL || id == wxID_NO || id == wxID_CLOSE)
			{
				Utility::SetIfNotNull(idOut, id);
				return true;
			}
		}
	}

	Utility::SetIfNotNull(idOut, wxID_NONE);
	return false;
}
bool KxStdDialog::IsEnterAllowed(wxKeyEvent& event, wxWindowID* idOut) const
{
	auto Check = [this](StdButton btn, int id, int enterId)
	{
		return m_SelectedButtons & btn && enterId == id;
	};

	if (m_SelectedButtons & StdButton::OK || m_SelectedButtons & StdButton::Yes || m_SelectedButtons & StdButton::Apply)
	{
		for (int id: m_EnterIDs)
		{
			if (Check(StdButton::OK, wxID_OK, id) || Check(StdButton::Yes, wxID_YES, id) || Check(StdButton::Apply, wxID_APPLY, id))
			{
				Utility::SetIfNotNull(idOut, id);
				return true;
			}
		}
	}

	Utility::SetIfNotNull(idOut, wxID_NONE);
	return false;
}
void KxStdDialog::SetResizingBehavior()
{
	wxSize size = GetMinSize();
	wxOrientation nSide = GetWindowResizeSide();
	switch ((int)nSide)
	{
		case wxBOTH:
		{
			SetMaxSize(wxDefaultSize);
			break;
		}
		case wxHORIZONTAL:
		{
			SetMaxSize(wxSize(wxDefaultCoord, size.GetHeight()));
			break;
		}
		case wxVERTICAL:
		{
			SetMaxSize(wxSize(size.GetWidth(), wxDefaultCoord));
			break;
		}
		case 0:
		{
			SetWindowStyle(GetWindowStyle() & ~wxRESIZE_BORDER);
			break;
		}
	};
	Utility::ModWindowStyle(GetHandle(), GWL_STYLE, WS_MAXIMIZEBOX, nSide == wxBOTH);
}
void KxStdDialog::PostCreate(const wxPoint& pos)
{
	m_DialogSizer->Add(m_ContentPanel, 1, wxEXPAND);
	m_DialogSizer->Add(m_ContentPanelLine, 0, wxEXPAND);

	if (m_ContentSizerBase)
	{
		m_ContentPanel->SetSizer(m_ContentSizerBase);
		m_ContentSizerBase->Add(m_IconSizer, 0, wxEXPAND, FromDIPY(this, 5));
		m_ContentSizerBase->Add(m_ContentSizer, 1, wxEXPAND|wxALL, FromDIPY(this, 5));
	}

	if (m_IconSizer)
	{
		m_IconSizer->Add(m_IconView, 0, wxLEFT|wxTOP, FromDIPX(this, 10));
	}

	if (m_ContentSizer)
	{
		m_ContentSizer->AddSpacer(FromDIPY(this, 4));
		m_ContentSizer->Add(m_CaptionLabel, 0, wxLEFT|wxRIGHT|wxEXPAND, FromDIPX(this, 2));

		if (GetViewLabelSizerOrientation() == wxHORIZONTAL)
		{
			m_ContentSizer->AddSpacer(FromDIPY(this, 6));
			wxBoxSizer* sizer = new wxBoxSizer(wxHORIZONTAL);
			m_ContentSizer->Add(sizer, 1, wxEXPAND);

			m_ViewLabelSI = sizer->Add(m_ViewLabel, 0, wxLEFT|wxEXPAND, FromDIPX(this, 3));
			wxBoxSizer* viewSizer = new wxBoxSizer(GetViewSizerOrientation());
			sizer->Add(viewSizer, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, FromDIPX(this, 5));
			viewSizer->Add(GetDialogMainCtrl(), GetViewSizerProportion(), wxEXPAND);
		}
		else
		{
			m_ViewLabelSpacerSI = m_ContentSizer->AddSpacer(FromDIPX(this, 6));
			m_ViewLabelSI = m_ContentSizer->Add(m_ViewLabel, 0, wxLEFT|wxEXPAND, FromDIPX(this, 3));

			wxBoxSizer* viewSizer = new wxBoxSizer(GetViewSizerOrientation());
			m_ContentSizer->Add(viewSizer, 1, wxLEFT|wxRIGHT|wxBOTTOM|wxEXPAND, FromDIPX(this, 5));
			viewSizer->Add(GetDialogMainCtrl(), GetViewSizerProportion(), wxEXPAND);
		}
		m_ViewLabelSI->Show(false);
	}

	if (m_ButtonsSizer)
	{
		m_DialogSizer->Add(m_ButtonsSizer, 0, wxEXPAND);
		m_ButtonsSizer->AddSpacer(FromDIPX(this, 6));
		m_ButtonsSizer->SetMinSize(wxDefaultCoord, FromDIPY(this, 40));
	}

	InitIcon();
	AdjustWindow(pos);
}
void KxStdDialog::AdjustWindow(const wxPoint &pos, const wxSize& minSize)
{
	if (IsAutoSizeEnabled())
	{
		if (minSize.IsFullySpecified())
		{
			SetMinSize(minSize);
		}
		else
		{
			SetMaxSize(wxDefaultSize);
			SetMinSize(m_DialogSizer->Fit(this));
		}
		SetSize(GetMinSize());
	}

	SetResizingBehavior();
	if (pos != wxPoint(-2, -2))
	{
		CenterIfNoPosition(pos);
	}
}

int KxStdDialog::ShowModal()
{
	GetDialogFocusCtrl()->SetFocus();
	return KxDialog::ShowModal();
}
void KxStdDialog::SetLabel(const wxString& label)
{
	m_ViewLabel->SetLabel(label);
	m_ViewLabelSI->Show(!label.IsEmpty());
	if (m_ViewLabelSpacerSI)
	{
		m_ViewLabelSpacerSI->Show(label.IsEmpty());
	}

	int width = m_ViewLabel->GetTextExtent(label).GetWidth();
	m_ViewLabelSI->SetMinSize(width + 0.05*width, wxDefaultCoord);
	AdjustWindow();
}

void KxStdDialog::SetDefaultButton(wxWindowID id)
{
	auto control = GetButton(id);
	if (control.IsControl())
	{
		KxButton* button = control.As<KxButton>();
		if (button)
		{
			button->SetDefault();
		}
		SetDefaultItem(control.GetControl());
		control.GetControl()->SetFocus();
	}
	else
	{
		wxWindow* window = wxWindow::FindWindowById(control, this);
		if (window)
		{
			window->SetFocus();
		}
	}
}
KxStdDialogControl KxStdDialog::GetButton(wxWindowID id) const
{
	for (wxSizerItem* item: m_ButtonsSizer->GetChildren())
	{
		wxWindow* window = item->GetWindow();
		if (window && window->GetId() == id && window->GetId() != wxID_NONE)
		{
			KxButton* button = dynamic_cast<KxButton*>(window);
			if (button)
			{
				return button;
			}
			else
			{
				return window->GetId();
			}
		}
	}
	return static_cast<wxWindow*>(nullptr);
}
KxStdDialogControl KxStdDialog::AddButton(wxWindowID id, const wxString& label, bool prepend)
{
	KxButton* button = m_ButtonsSizer->CreateButton(this, StdButton::WX_LAST_STD, StdButton::WX_LAST_STD, id, false);
	m_ButtonsSizer->AddCustomButton(button, label, prepend);
	m_CloseIDs.push_back(id);

	return button;
}

//////////////////////////////////////////////////////////////////////////
bool KxIStdDialog::ShowNativeWindow(wxDialog* window, bool show)
{
	HWND hWnd = window->GetHandle();
	bool ret = false;
	if (show)
	{
		if (hWnd)
		{
			ret = ::ShowWindow(hWnd, SW_SHOW);
		}
		else
		{
			window->ShowModal();
			ret = true;
		}
	}
	else
	{
		ret = ::ShowWindow(hWnd, SW_HIDE);
	}
	return ret;
}
int KxIStdDialog::TranslateIconIDToWx(StdIcon id) const
{
	switch (id)
	{
		case StdIcon::Question:
		{
			return wxICON_QUESTION;
		}
		case StdIcon::Error:
		{
			return wxICON_ERROR;
		}
		case StdIcon::Warning:
		{
			return wxICON_WARNING;
		}
		case StdIcon::Information:
		{
			return wxICON_INFORMATION;
		}
	};
	return wxICON_NONE;
}
StdIcon KxIStdDialog::TranslateIconIDFromWx(int id) const
{
	switch (id)
	{
		case wxICON_QUESTION:
		{
			return StdIcon::Question;
		}
		case wxICON_ERROR:
		{
			return StdIcon::Error;
		}
		case wxICON_WARNING:
		{
			return StdIcon::Warning;
		}
		case wxICON_INFORMATION:
		{
			return StdIcon::Information;
		}
	};
	return StdIcon::None;
}

//////////////////////////////////////////////////////////////////////////
KxStdDialogControl::KxStdDialogControl(KxButton* control)
	:m_Type(Type::Button)
{
	m_Data.Control = control;
}

KxStdDialogControl::operator wxWindowID() const
{
	if (m_Type != Type::ID)
	{
		return m_Data.Control->GetId();
	}
	else
	{
		return m_Data.ID;
	}
}
KxStdDialogControl::operator wxWindow*() const
{
	if (m_Type == Type::Window)
	{
		return m_Data.Control;
	}
	return nullptr;
}
KxStdDialogControl::operator KxButton*() const
{
	if (m_Type == Type::Button)
	{
		return static_cast<KxButton*>(m_Data.Control);
	}
	return nullptr;
}
