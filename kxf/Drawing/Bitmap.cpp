#include "stdafx.h"
#include "Bitmap.h"
#include "Cursor.h"
#include "Icon.h"
#include "Image.h"
#include "GDIRenderer.h"
#include "Private/GDI.h"

namespace kxf
{
	// Bitmap
	void Bitmap::Initialize()
	{
		m_Bitmap.UseAlpha(true);
	}

	Bitmap::Bitmap(const Cursor& other)
		:m_Bitmap(std::move(other.ToBitmap().m_Bitmap))
	{
		Initialize();
	}
	Bitmap::Bitmap(const Icon& other)
		:m_Bitmap(std::move(other.ToBitmap().m_Bitmap))
	{
		Initialize();
	}
	Bitmap::Bitmap(const Image& other)
		:m_Bitmap(std::move(other.ToBitmap().m_Bitmap))
	{
		Initialize();
	}
	Bitmap::Bitmap(const Image& other, const GDIContext& dc)
		:m_Bitmap(other.ToWxImage(), dc.ToWxDC())
	{
		Initialize();
	}
	Bitmap::Bitmap(const Size& size, const GDIContext& dc)
		:m_Bitmap(size.GetWidth(), size.GetHeight(), dc.ToWxDC())
	{
		Initialize();
	}

	// IGDIObject
	void* Bitmap::GetHandle() const
	{
		return m_Bitmap.GetHandle();
	}
	void* Bitmap::DetachHandle()
	{
		return Drawing::Private::DetachGDIImageHandle(m_Bitmap);
	}
	void Bitmap::AttachHandle(void* handle)
	{
		m_Bitmap = wxBitmap();
		Initialize();

		if (handle)
		{
			BITMAP bitmap = {};
			if (::GetObject(handle, sizeof(bitmap), &bitmap) != 0)
			{
				m_Bitmap.InitFromHBITMAP(reinterpret_cast<WXHBITMAP>(handle), bitmap.bmWidth, bitmap.bmHeight, bitmap.bmBitsPixel);
				return;
			}
		}
		m_Bitmap.SetHandle(handle);
	}

	// IGDIImage
	bool Bitmap::Load(IInputStream& stream, ImageFormat format)
	{
		Image image;
		if (image.Load(stream, format))
		{
			m_Bitmap = std::move(image.ToBitmap().m_Bitmap);
			return m_Bitmap.IsOk();
		}
		return false;
	}
	bool Bitmap::Save(IOutputStream& stream, ImageFormat format) const
	{
		if (m_Bitmap.IsOk() && format != ImageFormat::Any && format != ImageFormat::None)
		{
			return ToImage().Save(stream, format);
		}
		return false;
	}

	// Bitmap
	Cursor Bitmap::ToCursor(const Point& hotSpot) const
	{
		Cursor cursor(wxCursor(m_Bitmap.ConvertToImage()));
		cursor.SetHotSpot(hotSpot);

		return cursor;
	}
	Image Bitmap::ToImage() const
	{
		return m_Bitmap.ConvertToImage();
	}
	Icon Bitmap::ToIcon() const
	{
		wxIcon icon;
		icon.CopyFromBitmap(m_Bitmap);

		return icon;
	}
}
