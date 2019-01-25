#pragma once
#include "KxFramework/KxFramework.h"
#include "KxFramework/DataView2/Editor.h"
#include <KxFramework/KxWithImageList.h>

namespace Kx::DataView2
{
	class KX_API ComboBoxEditorBase
	{
		private:
			int m_MaxVisibleItems = -1;
			bool m_IsControlEditable = false;
			bool m_EndEditOnCloseup = false;

		public:
			int GetMaxVisibleItems() const
			{
				return m_MaxVisibleItems;
			}
			void SetMaxVisibleItems(int max = -1)
			{
				m_MaxVisibleItems = max;
			}
		
			bool ShouldEndEditOnCloseup() const
			{
				return m_EndEditOnCloseup;
			}
			void EndEditOnCloseup(bool value)
			{
				m_EndEditOnCloseup = value;
			}

			bool IsEditable() const
			{
				return m_IsControlEditable;
			}
			void SetEditable(bool isEditable)
			{
				m_IsControlEditable = isEditable;
			}
	};
}

namespace Kx::DataView2
{
	class KX_API ComboBoxEditor: public Editor, public ComboBoxEditorBase, public KxWithImageList
	{
		private:
			KxStringVector m_Items;
			KxIntVector m_BitmapIndexes;
			bool m_AlwaysUseStringSelection = false;
			bool m_UseBitmap = false;

		protected:
			EditorControlHandler* CreateControlHandler() override;

			wxWindow* CreateControl(wxWindow* parent, const wxRect& cellRect, const wxAny& value) override;
			wxAny GetValue(wxWindow* control) const override;

		public:
			const KxStringVector& GetItems() const
			{
				return m_Items;
			}
			void SetItems(const KxStringVector& tValues)
			{
				m_Items = tValues;
			}
			void ClearItems()
			{
				m_Items.clear();
			}

			const KxIntVector& GetBitmapIndexes() const
			{
				return m_BitmapIndexes;
			}
			void SetBitmapIndexes(const KxIntVector& tValues)
			{
				m_BitmapIndexes = tValues;
			}
			void ClearBitmapIndexes()
			{
				m_BitmapIndexes.clear();
			}

		public:
			bool ShouldAlwaysUseStringSelection() const
			{
				return m_AlwaysUseStringSelection;
			}
			void AlwaysUseStringSelection(bool use = true)
			{
				m_AlwaysUseStringSelection = use;
			}

			bool IsBitmapsEnabled() const
			{
				return m_UseBitmap;
			}
			void EnableBitmaps(bool enabled = true)
			{
				m_UseBitmap = enabled;
			}
	};
}

namespace Kx::DataView2
{
	class KX_API ComboBoxEditorControlHandler: public EditorControlHandler
	{
		protected:
			void DoOnKillFocus(wxFocusEvent& event) override;

		public:
			ComboBoxEditorControlHandler(ComboBoxEditor* editor, wxWindow* control)
				:EditorControlHandler(editor, control)
			{
			}
	};
}