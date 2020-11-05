#pragma once
#include "../Renderer.h"

namespace kxf::UI::DataView
{
	class KX_API BitmapValueBase
	{
		protected:
			int m_Spacing = 0;
			int m_DefaultWidth = -1;
			bool m_DrawInvalid = false;

		public:
			BitmapValueBase() = default;

		public:
			void Clear()
			{
				*this = {};
			}

			int GetSpacing() const
			{
				return m_Spacing;
			}
			void SetSpacing(int value)
			{
				m_Spacing = value;
			}

			bool ShouldDrawInvalidBitmaps() const
			{
				return m_DrawInvalid;
			}
			void DrawInvalidBitmaps(bool value = true)
			{
				m_DrawInvalid = value;
			}

			bool IsDefaultBitmapWidthSpecified() const
			{
				return m_DefaultWidth > 0;
			}
			int GetDefaultBitmapWidth() const
			{
				return m_DefaultWidth;
			}
			void SetDefaultBitmapWidth(int value)
			{
				m_DefaultWidth = value;
			}
	};
}

namespace kxf::UI::DataView
{
	class KX_API BitmapValue: public BitmapValueBase
	{
		protected:
			Bitmap m_Bitmap;

		public:
			BitmapValue(const Bitmap& bitmap = wxNullBitmap)
				:m_Bitmap(bitmap)
			{
			}
			
		public:
			bool FromAny(const wxAny& value);
			void Clear()
			{
				BitmapValueBase::Clear();
				ClearBitmap();
			}

			bool HasBitmap() const
			{
				return !m_Bitmap.IsNull();
			}
			Bitmap& GetBitmap()
			{
				return m_Bitmap;
			}
			const Bitmap& GetBitmap() const
			{
				return m_Bitmap;
			}
			void SetBitmap(const Bitmap& bitmap)
			{
				m_Bitmap = bitmap;
			}
			void ClearBitmap()
			{
				m_Bitmap = wxNullBitmap;
			}
	};
}

namespace kxf::UI::DataView
{
	class KX_API BitmapRenderer: public Renderer
	{
		private:
			BitmapValue m_Value;

		protected:
			bool SetValue(const wxAny& value) override;
			void DrawCellContent(const Rect& cellRect, CellState cellState) override;
			Size GetCellSize() const override;

		public:
			BitmapRenderer(FlagSet<Alignment> alignment = Alignment::CenterHorizontal|Alignment::CenterVertical)
				:Renderer(alignment)
			{
			}
			
		public:
			String GetTextValue(const wxAny& value) const override
			{
				return {};
			}
	};
}
