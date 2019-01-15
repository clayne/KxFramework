#include "KxStdAfx.h"
#include "View.h"
#include "HeaderCtrl.h"
#include "MainWindow.h"
#include "Column.h"
#include "Renderer.h"
#include "KxFramework/KxMenu.h"

namespace Kx::DataView2
{
	wxIMPLEMENT_ABSTRACT_CLASS(View, wxControl)

	WXLRESULT View::MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam)
	{
		WXLRESULT rc = wxControl::MSWWindowProc(msg, wParam, lParam);

		// We need to process arrows ourselves for scrolling
		if (msg == WM_GETDLGCODE)
		{
			rc |= DLGC_WANTARROWS;
		}
		return rc;
	}

	void View::InvalidateColumnsBestWidth()
	{
		for (auto& column: m_Columns)
		{
			column->InvalidateBestWidth();
		}
	}
	void View::UpdateColumnsWidth()
	{
		m_IsColumnsDirty = false;

		if (m_HeaderArea)
		{
			for (auto& column: m_Columns)
			{
				// Note that we have to have an explicit 'dirty' flag here instead of
				// checking if the width == 0, as is done in CalBestColumnWidth().
				// 
				// Testing width == 0 wouldn't work correctly if some code called
				// GetWidth() after column width invalidation but before
				// View::UpdateColumnsWidth() was called at idle time. This
				// would result in the header's column width getting out of sync with
				// the control itself.

				if (column->IsDirty())
				{
					m_HeaderArea->UpdateColumn(column->GetIndex());
					column->MarkDirty(false);
				}
			}
		}
	}

	void View::DoEnable(bool value)
	{
		wxControl::DoEnable(value);
		Refresh();
	}
	void View::DoInsertColumn(Column* column, size_t index)
	{
		// Correct insertion position
		if (m_Columns.empty())
		{
			index = 0;
		}
		else if (index > m_Columns.size())
		{
			index = m_Columns.size();
		}

		// Set column data
		column->SetIndex(index);
		column->SetDisplayIndex(index);
		column->SetView(this);

		// Insert
		m_Columns.emplace(m_Columns.begin() + index, column);
		OnColumnsCountChanged();
	}

	void View::UseColumnForSorting(size_t index)
	{
		m_ColumnsSortingIndexes.push_back(index);
	}
	void View::DontUseColumnForSorting(size_t index)
	{
		for (auto it = m_ColumnsSortingIndexes.begin(); it < m_ColumnsSortingIndexes.end(); ++it)
		{
			if (*it == index)
			{
				m_ColumnsSortingIndexes.erase(it);
				return;
			}
		}
	}
	bool View::IsColumnSorted(size_t index) const
	{
		return std::find(m_ColumnsSortingIndexes.begin(), m_ColumnsSortingIndexes.end(), index) != m_ColumnsSortingIndexes.end();
	}

	void View::ResetAllSortColumns()
	{
		// Must make copy, because unsorting will remove it from original vector
		auto copy = m_ColumnsSortingIndexes;
		for (auto& index: copy)
		{
			GetColumn(index)->ResetSorting();
		}
	}
	void View::OnInternalIdle()
	{
		wxControl::OnInternalIdle();

		if (m_IsColumnsDirty)
		{
			UpdateColumnsWidth();
		}
	}
	void View::DoEnableSystemTheme(bool enable, wxWindow* window)
	{
		wxSystemThemedControlBase::DoEnableSystemTheme(enable, window);
		wxSystemThemedControlBase::DoEnableSystemTheme(enable, m_ClientArea);
		if (m_HeaderArea)
		{
			wxSystemThemedControlBase::DoEnableSystemTheme(enable, m_HeaderArea);
		}

		m_UsingSystemTheme = enable;
	}

	void View::OnSize(wxSizeEvent& event)
	{
		if (m_ClientArea && GetColumnCount() != 0)
		{
			m_ClientArea->UpdateColumnSizes();
		}

		// We need to override OnSize so that our scrolled window
		// a) does call Layout() to use sizers for positioning the controls but
		// b) does not query the sizer for their size and use that for setting
		// the scrollable area as set that ourselves by calling SetScrollbar() further down.

		Layout();
		AdjustScrollbars();

		// We must redraw the headers if their height changed. Normally this
		// shouldn't happen as the control shouldn't let itself be resized beneath
		// its minimal height but avoid the display artifacts that appear if it
		// does happen, e.g. because there is really not enough vertical space.
		if (m_HeaderArea && m_HeaderArea->GetSize().y <= m_HeaderArea->GetBestSize().y)
		{
			m_HeaderArea->Refresh();
		}
	}
	void View::OnPaint(wxPaintEvent& event)
	{
		wxPaintDC dc(this);
		if (m_BorderColor.IsOk())
		{
			dc.SetPen(m_BorderColor);
			dc.SetBrush(m_BorderColor);
			dc.DrawRectangle(GetClientSize());
		}
	}

	wxSize View::GetSizeAvailableForScrollTarget(const wxSize& size)
	{
		wxSize newSize = size;
		if (m_HeaderArea)
		{
			newSize.y -= m_HeaderArea->GetSize().y;
		}
		return newSize;
	}

	bool View::Create(wxWindow* parent, wxWindowID id, CtrlStyle style)
	{
		if (wxControl::Create(parent, id, wxDefaultPosition, wxDefaultSize, static_cast<long>(style)|wxScrolledWindowStyle, wxDefaultValidator, GetClassInfo()->GetClassName()))
		{
			m_ClientArea = new MainWindow(this, wxID_NONE);

			// We use the cursor keys for moving the selection, not scrolling, so call
			// this method to ensure wxScrollHelperEvtHandler doesn't catch all
			// keyboard events forwarded to us from 'MainWindow'.
			DisableKeyboardScrolling();

			if (!IsOptionEnabled(CtrlStyle::NoHeader))
			{
				m_HeaderArea = new HeaderCtrl(this);
			}
			SetTargetWindow(m_ClientArea);

			m_Sizer = new wxBoxSizer(wxVERTICAL);
			if (m_HeaderArea)
			{
				m_HeaderAreaSI = m_Sizer->Add(m_HeaderArea, 0, wxEXPAND|wxTOP|wxLEFT|wxRIGHT);
			}
			m_ClientAreaSI = m_Sizer->Add(m_ClientArea, 1, wxEXPAND|wxBOTTOM|wxLEFT|wxRIGHT);
			SetSizer(m_Sizer);

			EnableSystemTheme();
			Bind(wxEVT_SIZE, &View::OnSize, this);
			Bind(wxEVT_PAINT, &View::OnPaint, this);
			return true;
		}
		return false;
	}
	View::~View()
	{
	}

	Model* View::GetModel() const
	{
		return m_ClientArea->GetModel();
	}
	void View::SetModel(Model* model)
	{
		m_ClientArea->SetModel(model);
	}
	void View::AssignModel(Model* model)
	{
		m_ClientArea->AssignModel(model);
	}

	Node& View::GetRootNode() const
	{
		return m_ClientArea->GetRootNode();
	}
	void View::ItemsChanged()
	{
		m_ClientArea->ItemsChanged();
	}

	Renderer& View::AppendColumn(Column* column)
	{
		DoInsertColumn(column, GetColumnCount());
		return column->GetRenderer();
	}
	Renderer& View::PrependColumn(Column* column)
	{
		DoInsertColumn(column, 0);
		return column->GetRenderer();
	}
	Renderer& View::InsertColumn(size_t index, Column* column)
	{
		DoInsertColumn(column, index);
		return column->GetRenderer();
	}

	size_t View::GetVisibleColumnCount() const
	{
		size_t count = 0;
		for (const auto& column: m_Columns)
		{
			if (column->IsVisible())
			{
				count++;
			}
		}
		return count;
	}

	Column* View::GetColumn(size_t position) const
	{
		if (position < m_Columns.size())
		{
			return m_Columns[position].get();
		}
		return nullptr;
	}
	Column* View::GetColumnByID(int columnID) const
	{
		for (const auto& column: m_Columns)
		{
			if (column->GetID() == columnID)
			{
				return column.get();
			}
		}
		return nullptr;
	}
	Column* View::GetColumnDisplayedAt(size_t displayIndex) const
	{
		// Columns can't be reordered if there is no header window which allows to do this.
		if (HasHeaderCtrl())
		{
			for (const auto& column: m_Columns)
			{
				if (column->GetDisplayIndex() == displayIndex)
				{
					return column.get();
				}
			}
		}
		else
		{
			return GetColumn(displayIndex);
		}
		return nullptr;
	}

	bool View::DeleteColumn(Column& column)
	{
		size_t nextColumn = INVALID_COLUMN;
		size_t displayIndex = INVALID_COLUMN;
		for (auto it = m_Columns.begin(); it != m_Columns.end(); ++it)
		{
			if (it->get() == &column)
			{
				nextColumn = column.GetIndex();
				displayIndex = column.GetDisplayIndex();

				if (m_ClientArea->GetCurrentColumn() == &column)
				{
					m_ClientArea->ClearCurrentColumn();
				}

				m_Columns.erase(it);
				break;
			}
		}

		// Column was removed, update indexes.
		if (nextColumn != INVALID_COLUMN)
		{
			for (size_t i = nextColumn; i < m_Columns.size(); i++)
			{
				Column& currentColumn = *m_Columns[i];

				// Move actual index
				currentColumn.SetIndex(currentColumn.GetIndex() - 1);

				// Move display index
				size_t index = currentColumn.GetDisplayIndex();
				if (index > displayIndex)
				{
					currentColumn.SetDisplayIndex(index - 1);
				}
			}

			OnColumnsCountChanged();
			return true;
		}
		return false;
	}
	bool View::ClearColumns()
	{
		SetExpanderColumn(nullptr);
		m_Columns.clear();
		m_ColumnsSortingIndexes.clear();
		m_ClientArea->ClearCurrentColumn();
		OnColumnsCountChanged();

		return true;
	}

	Column* View::GetCurrentColumn() const
	{
		return m_ClientArea->GetCurrentColumn();
	}
	Column* View::GetExpanderColumnOrFirstOne()
	{
		if (!m_ExpanderColumn)
		{
			// TODO-RTL: Last column for RTL support
			m_ExpanderColumn = GetColumnDisplayedAt(0);
			SetExpanderColumn(m_ExpanderColumn);
		}
		return m_ExpanderColumn;
	}
	Column* View::GetExpanderColumn() const
	{
		return m_ExpanderColumn;
	}
	void View::SetExpanderColumn(Column *column)
	{
		m_ExpanderColumn = column;
		if (column)
		{
			column->InvalidateBestWidth();
		}
		m_ClientArea->UpdateDisplay();
	}

	Column* View::GetSortingColumn() const
	{
		return !m_ColumnsSortingIndexes.empty() ? GetColumn(m_ColumnsSortingIndexes.front()) : nullptr;
	}
	Column::Vector View::GetSortingColumns() const
	{
		Column::Vector columns;
		for (const auto& index: m_ColumnsSortingIndexes)
		{
			if (Column* column = GetColumn(index))
			{
				columns.push_back(column);
			}
		}
		return columns;
	}

	bool View::AllowMultiColumnSort(bool allow)
	{
		if (m_AllowMultiColumnSort == allow)
		{
			return true;
		}
		m_AllowMultiColumnSort = allow;

		// If disabling, must disable any multiple sort that are active
		if (!allow)
		{
			ResetAllSortColumns();
			m_ClientArea->OnShouldResort();
		}
		return true;
	}
	void View::ToggleSortByColumn(size_t position)
	{
		m_HeaderArea->ToggleSortByColumn(position);
	}

	int View::GetIndent() const
	{
		return m_ClientArea->m_Indent;
	}
	void View::SetIndent(int indent)
	{
		m_ClientArea->m_Indent = indent;
		m_ClientArea->UpdateDisplay();
	}

	Node* View::GetCurrentItem() const
	{
		if (IsOptionEnabled(CtrlStyle::MultipleSelection))
		{
			return m_ClientArea->GetNodeByRow(m_ClientArea->GetCurrentRow());
		}
		else
		{
			return GetSelection();
		}
	}
	void View::SetCurrentItem(Node& item)
	{
		if (IsOptionEnabled(CtrlStyle::MultipleSelection))
		{
			const size_t newCurrent = m_ClientArea->GetRowByNode(item);
			const size_t oldCurrent = m_ClientArea->GetCurrentRow();

			if (newCurrent != oldCurrent)
			{
				m_ClientArea->ChangeCurrentRow(newCurrent);
				m_ClientArea->RefreshRow(oldCurrent);
				m_ClientArea->RefreshRow(newCurrent);
			}
		}
		else
		{
			Select(item);
		}
	}

	size_t View::GetSelectedCount() const
	{
		return m_ClientArea->GetSelections().GetSelectedCount();
	}
	Node* View::GetSelection() const
	{
		const wxSelectionStore& selectionStore = m_ClientArea->GetSelections();
		if (selectionStore.GetSelectedCount() == 1)
		{
			wxSelectionStore::IterationState cookie = 0;
			return m_ClientArea->GetNodeByRow(selectionStore.GetFirstSelectedItem(cookie));
		}
		return nullptr;
	}
	size_t View::GetSelections(Node::Vector& selection) const
	{
		selection.clear();
		const wxSelectionStore& selectionStore = m_ClientArea->GetSelections();

		wxSelectionStore::IterationState cookie;
		for (auto row = selectionStore.GetFirstSelectedItem(cookie); row != wxSelectionStore::NO_SELECTION; row = selectionStore.GetNextSelectedItem(cookie))
		{
			Node* item = m_ClientArea->GetNodeByRow(row);
			if (item)
			{
				selection.push_back(item);
			}
		}
		return selection.size();
	}
	void View::SetSelections(const Node::Vector& selection)
	{
		m_ClientArea->ClearSelection();
		Node* lastParent = nullptr;

		for (size_t i = 0; i < selection.size(); i++)
		{
			Node* item = selection[i];
			Node* parent = item->GetParent();
			if (parent)
			{
				if (parent != lastParent)
				{
					ExpandAncestors(*item);
				}
			}

			lastParent = parent;
			Row row = m_ClientArea->GetRowByNode(*item);
			if (row)
			{
				m_ClientArea->SelectRow(row, true);
			}
		}
	}
	void View::Select(Node& node)
	{
		ExpandAncestors(node);

		Row row = m_ClientArea->GetRowByNode(node);
		if (row)
		{
			// Unselect all rows before select another in the single select mode
			if (m_ClientArea->IsSingleSelection())
			{
				m_ClientArea->UnselectAllRows();
			}
			m_ClientArea->SelectRow(row, true);

			// Also set focus to the selected item
			m_ClientArea->ChangeCurrentRow(row);
		}
	}
	void View::Unselect(Node& node)
	{
		Row row = m_ClientArea->GetRowByNode(node);
		if (row)
		{
			m_ClientArea->SelectRow(row, false);
		}
	}
	bool View::IsSelected(const Node& node) const
	{
		Row row = m_ClientArea->GetRowByNode(node);
		if (row)
		{
			return m_ClientArea->IsRowSelected(row);
		}
		return false;
	}

	Node* View::GetHotTrackedItem() const
	{
		return m_ClientArea->GetHotTrackItem();
	}
	Column* View::GetHotTrackedColumn() const
	{
		return m_ClientArea->GetHotTrackColumn();
	}

	void View::GenerateSelectionEvent(Node& item, const Column* column)
	{
		m_ClientArea->SendSelectionChangedEvent(&item, const_cast<Column*>(column));
	}

	void View::SelectAll()
	{
		m_ClientArea->SelectAllRows();
	}
	void View::UnselectAll()
	{
		m_ClientArea->UnselectAllRows();
	}

	void View::Expand(Node& item)
	{
		ExpandAncestors(item);

		Row row = m_ClientArea->GetRowByNode(item);
		if (row)
		{
			m_ClientArea->Expand(row);
		}
	}
	void View::ExpandAncestors(Node& item)
	{
		if (GetModel())
		{
			Node* parent = item.GetParent();
			while (parent)
			{
				Expand(*parent);
				parent = parent->GetParent();
			}
		}
	}
	void View::Collapse(Node& item)
	{
		size_t row = m_ClientArea->GetRowByNode(item);
		if (row != -1)
		{
			m_ClientArea->Collapse(row);
		}
	}
	bool View::IsExpanded(Node& item) const
	{
		size_t row = m_ClientArea->GetRowByNode(item);
		if (row != -1)
		{
			return m_ClientArea->IsExpanded(row);
		}
		return false;
	}

	void View::EnsureVisible(Node& item, const Column* column)
	{
		ExpandAncestors(item);
		m_ClientArea->RecalculateDisplay();

		Row row = m_ClientArea->GetRowByNode(item);
		if (row)
		{
			if (column)
			{
				m_ClientArea->EnsureVisible(row, column->GetIndex());
			}
			else
			{
				m_ClientArea->EnsureVisible(row);
			}
		}
	}
	void View::HitTest(const wxPoint& point, Node*& item, Column*& column) const
	{
		m_ClientArea->HitTest(point, item, column);
	}
	wxRect View::GetItemRect(Node& item, const Column* column) const
	{
		return m_ClientArea->GetItemRect(item, column);
	}
	wxRect View::GetAdjustedItemRect(Node& item, const Column* column) const
	{
		wxRect rect = GetItemRect(item, column);
		if (HasHeaderCtrl())
		{
			rect.SetTop(rect.GetTop() + GetHeaderCtrl()->GetSize().GetHeight());
		}
		return rect;
	}
	wxPoint View::GetDropdownMenuPosition(Node& item, const Column* column) const
	{
		return GetAdjustedItemRect(item, column).GetLeftBottom() + FromDIP(wxPoint(0, 1));
	}

	int View::GetUniformRowHeight() const
	{
		return m_ClientArea->GetUniformRowHeight();
	}
	void View::SetUniformRowHeight(int rowHeight)
	{
		m_ClientArea->SetUniformRowHeight(rowHeight);
	}
	int View::GetDefaultRowHeight(UniformHeight type) const
	{
		return m_ClientArea->GetDefaultRowHeight(type);
	}

	bool View::EditItem(Node& item, Column& column)
	{
		return m_ClientArea->BeginEdit(item, column);
	}

	/* Drag and drop */
	bool View::EnableDragSource(const wxDataFormat& format)
	{
		return m_ClientArea->EnableDragSource(format);
	}
	bool View::EnableDropTarget(const wxDataFormat& format)
	{
		return m_ClientArea->EnableDropTarget(format);
	}

	void View::SetFocus()
	{
		if (m_ClientArea)
		{
			m_ClientArea->SetFocus();
		}
	}
	bool View::SetFont(const wxFont& font)
	{
		if (!wxControl::SetFont(font))
		{
			return false;
		}

		if (m_HeaderArea)
		{
			m_HeaderArea->SetFont(font);
		}
		if (m_ClientArea)
		{
			m_ClientArea->SetFont(font);
			m_ClientArea->SetUniformRowHeight(m_ClientArea->GetDefaultRowHeight());
		}

		if (m_HeaderArea || m_ClientArea)
		{
			InvalidateColumnsBestWidth();
			Layout();
		}
		return true;
	}

	/* Window */
	wxHeaderCtrl* View::GetHeaderCtrl()
	{
		return m_HeaderArea;
	}
	const wxHeaderCtrl* View::GetHeaderCtrl() const
	{
		return m_HeaderArea;
	}

	bool View::CreateColumnSelectionMenu(KxMenu& menu)
	{
		size_t count = GetColumnCount();
		for (size_t i = 0; i < count; i++)
		{
			Column* column = GetColumn(i);
			wxString title = column->GetTitle();
			if (title.IsEmpty())
			{
				title << '<' << i + 1 << '>';
			}

			KxMenuItem* menuItem = menu.Add(new KxMenuItem(title, wxEmptyString, wxITEM_CHECK));
			menuItem->Check(column->IsVisible());
			menuItem->SetClientData(column);
		}
		return menu.GetMenuItemCount() != 0;
	}
	Column* View::OnColumnSelectionMenu(KxMenu& menu)
	{
		wxWindowID retID = menu.Show(this);
		if (retID != KxID_NONE)
		{
			KxMenuItem* menuItem = menu.FindItem(retID);
			Column* column = static_cast<Column*>(menuItem->GetClientData());
			column->SetVisible(menuItem->IsChecked());

			if (column->IsVisible() && column->GetWidth() == 0)
			{
				column->SetWidth(column->GetBestWidth());
			}

			OnColumnChange(column->GetIndex());
			return column;
		}
		return nullptr;
	}

	/* Control visuals */
	wxBorder View::GetDefaultBorder() const
	{
		return wxBORDER_THEME;
	}
	void View::SetBorderColor(const KxColor& color, int size)
	{
		m_BorderColor = color;
		int borderSize = m_BorderColor.IsOk() ? FromDIP(size) : 0;

		m_ClientAreaSI->SetBorder(borderSize);
		if (m_HeaderAreaSI)
		{
			m_HeaderAreaSI->SetBorder(borderSize);
		}
		Refresh();
	}

	/* Utility functions, not part of the API */
	void View::ColumnMoved(Column& column, size_t newIndex)
	{
		// Do *not* reorder 'm_Columns' elements here, they should always be in the order in which columns
		// were added, we only display the columns in different order.
		const size_t oldIndex = column.GetDisplayIndex();

		GetColumnDisplayedAt(newIndex)->SetDisplayIndex(oldIndex);
		column.SetDisplayIndex(newIndex);

		OnColumnChange(oldIndex);
		OnColumnChange(newIndex);
	}
	void View::OnColumnChange(size_t index)
	{
		if (m_HeaderArea)
		{
			m_HeaderArea->UpdateColumn(index);
		}
		m_ClientArea->UpdateDisplay();
	}
	void View::OnColumnsCountChanged()
	{
		if (m_HeaderArea)
		{
			m_HeaderArea->SetColumnCount(GetColumnCount());
		}
		m_ClientArea->OnColumnsCountChanged();
	}
}