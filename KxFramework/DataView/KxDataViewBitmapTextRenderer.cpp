#include "KxStdAfx.h"
#include "KxFramework/DataView/KxDataViewBitmapTextRenderer.h"
#include "KxFramework/DataView/KxDataViewBitmapRenderer.h"
#include "KxFramework/DataView/KxDataViewTextRenderer.h"

wxSize KxDataViewBitmapTextRenderer::GetBitmapMargins(wxWindow* window)
{
	return window->FromDIP(wxSize(2, 2));
}
int KxDataViewBitmapTextRenderer::DrawBitmapWithText(KxDataViewRenderer* rederer, const wxRect& cellRect, KxDataViewCellState cellState, int offsetX, KxDataViewBitmapTextValue& value)
{
	const wxSize bitmapMargin = GetBitmapMargins(rederer->GetView());
	if (value.HasBitmap())
	{
		const wxBitmap& bitmap = value.GetBitmap();
		rederer->DoDrawBitmap(wxRect(cellRect.GetX() + offsetX, cellRect.GetY(), cellRect.GetWidth() - offsetX, cellRect.GetHeight()), cellState, bitmap);

		offsetX += bitmap.GetWidth() + bitmapMargin.GetWidth();
	}
	if (value.HasText())
	{
		if (value.HasBitmap() && value.ShouldVCenterText())
		{
			wxSize textExtent = rederer->DoGetTextExtent(value.GetText());
			wxSize size = wxSize(cellRect.GetWidth(), textExtent.GetHeight());
			wxPoint pos = cellRect.GetPosition();
			pos.y += textExtent.GetHeight() / 2;

			rederer->DoDrawText(wxRect(pos, size), cellState, value.GetText(), offsetX);
		}
		else
		{
			rederer->DoDrawText(cellRect, cellState, value.GetText(), offsetX);
		}
	}
	return offsetX;
}

bool KxDataViewBitmapTextRenderer::SetValue(const wxAny& value)
{
	m_Value = KxDataViewBitmapTextValue();
	if (value.GetAs<KxDataViewBitmapTextValue>(&m_Value))
	{
		return true;
	}
	if (KxDataViewBitmapRenderer::GetValueAsBitmap(value, m_Value.GetBitmap()))
	{
		return true;
	}
	if (KxDataViewTextRenderer::GetValueAsString(value, m_Value.GetText()))
	{
		return true;
	}
	return false;
}

void KxDataViewBitmapTextRenderer::DrawCellContent(const wxRect& cellRect, KxDataViewCellState cellState)
{
	DrawBitmapWithText(this, cellRect, cellState, 0, m_Value);
}
wxSize KxDataViewBitmapTextRenderer::GetCellSize() const
{
	wxSize size(0, 0);
	if (m_Value.HasText())
	{
		size += DoGetTextExtent(m_Value.GetText());
	}
	if (m_Value.HasBitmap())
	{
		wxSize margins = GetBitmapMargins(GetView());
		const wxBitmap& bitmap = m_Value.GetBitmap();

		size.x += bitmap.GetWidth() + margins.x;
		if (size.y < bitmap.GetHeight())
		{
			size.y = bitmap.GetHeight() + margins.y;
		}
	}
	return size != wxSize(0, 0) ? size : wxSize(0, KxDataViewRenderer::GetCellSize().GetHeight());
}
