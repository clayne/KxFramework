#include "KxStdAfx.h"
#include "MainWindow.h"
#include "Renderer.h"
#include "Editor.h"
#include "Column.h"
#include "Node.h"
#include "View.h"
#include "KxFramework/KxDataView2Event.h"
#include "KxFramework/KxSystemSettings.h"
#include "KxFramework/KxSplashWindow.h"
#include "KxFramework/KxDCClipper.h"
#include "KxFramework/KxUtility.h"
#include "KxFramework/KxFrame.h"
#include <wx/popupwin.h>
#include <wx/generic/private/widthcalc.h>
#include <wx/minifram.h>

namespace Kx::DataView2
{
	class MaxWidthCalculator: public wxMaxWidthCalculatorBase
	{
		private:
			MainWindow* m_MainWindow = nullptr;
			Column& m_Column;

			bool m_IsExpanderColumn = false;
			int m_ExpanderSize = 0;

		protected:
			void UpdateWithRow(int row) override
			{
				Node* node = m_MainWindow->GetNodeByRow(row);
				if (node)
				{
					int indent = 0;
					if (m_IsExpanderColumn)
					{
						indent = m_MainWindow->m_Indent * node->GetIndentLevel() + m_ExpanderSize;
					}

					Renderer& renderer = node->GetRenderer(m_Column);
					renderer.SetupCellAttributes(*node, m_Column, m_MainWindow->GetCellStateForRow(row));
					UpdateWithWidth(renderer.GetCellSize().x + indent);
					renderer.EndRendering();
				}
			}

		public:
			MaxWidthCalculator(MainWindow* mainWindow, Column& column, int expanderSize)
				:wxMaxWidthCalculatorBase(column.GetDisplayIndex()), m_MainWindow(mainWindow), m_Column(column), m_ExpanderSize(expanderSize)
			{
				m_IsExpanderColumn = !m_MainWindow->IsList() && m_MainWindow->m_View->GetExpanderColumnOrFirstOne() == &m_Column;
			}
	};
}

namespace Kx::DataView2
{
	wxIMPLEMENT_ABSTRACT_CLASS(MainWindow, wxWindow);

	/* Events */
	void MainWindow::OnChar(wxKeyEvent& event)
	{
		// Propagate the char event upwards
		wxKeyEvent eventForParent(event);
		eventForParent.SetEventObject(m_View);
		if (m_View->ProcessWindowEvent(eventForParent))
		{
			return;
		}
		if (m_View->HandleAsNavigationKey(event))
		{
			return;
		}

		// No item then nothing to do
		if (!HasCurrentRow())
		{
			event.Skip();
			return;
		}

		switch (event.GetKeyCode())
		{
			case WXK_RETURN:
			{
				if (event.HasModifiers())
				{
					event.Skip();
					break;
				}
				else
				{
					// Enter activates the item, i.e. sends KxEVT_DATAVIEW_ITEM_ACTIVATED to it.
					// Only if that event is not handled do we activate column renderer (which
					// is normally done by Space) or even inline editing.
					Event evt(KxEVT_DATAVIEW_ITEM_ACTIVATED);
					CreateEventTemplate(evt, GetNodeByRow(m_CurrentRow));

					if (m_View->ProcessWindowEvent(evt))
					{
						break;
					}
					// Else fall through to WXK_SPACE handling
				}
			}
			case WXK_SPACE:
			{
				if (event.HasModifiers())
				{
					event.Skip();
					break;
				}
				else
				{
					Node* node = GetNodeByRow(m_CurrentRow);

					// Activate the current activatable column. If not column is focused (typically
					// because the user has full row selected), try to find the first activatable
					// column (this would typically be a checkbox and we don't want to force the user
					// to set focus on the checkbox column).
					Column* activatableColumn = FindColumnForEditing(*node);
					if (activatableColumn)
					{
						const wxRect cellRect = m_View->GetItemRect(*node, activatableColumn);

						Renderer& renderer = node->GetRenderer(*activatableColumn);
						renderer.SetupCellAttributes(*node, *activatableColumn, GetCellStateForRow(m_CurrentRow));
						renderer.OnActivateCell(*node, cellRect, nullptr);
						renderer.EndRendering();
						break;
					}
					break;
				}
			}
			case WXK_F2:
			{
				if (event.HasModifiers())
				{
					event.Skip();
					break;
				}
				else
				{
					if (!m_SelectionStore.IsEmpty())
					{
						// Mimic Windows 7 behavior: edit the item that has focus
						// if it is selected and the first selected item if focus
						// is out of selection.
						Row selectedRow;
						if (m_SelectionStore.IsSelected(m_CurrentRow))
						{
							selectedRow = m_CurrentRow;
						}
						else
						{
							// Focused item is not selected.
							wxSelectionStore::IterationState cookie;
							selectedRow = m_SelectionStore.GetFirstSelectedItem(cookie);
						}
						Node* node = GetNodeByRow(selectedRow);

						// Edit the current column. If no column is focused (typically because the user has full row selected),
						// try to find the first editable column.
						if (Column* editableColumn = FindColumnForEditing(*node))
						{
							m_View->EditItem(*node, *editableColumn);
						}
					}
				}
				break;
			}
			case WXK_UP:
			{
				OnVerticalNavigation(event, -1);
				break;
			}
			case WXK_DOWN:
			{
				OnVerticalNavigation(event, +1);
				break;
			}
			case WXK_LEFT:
			{
				OnLeftKey(event);
				break;
			}
			case WXK_RIGHT:
			{
				OnRightKey(event);
				break;
			}
			case WXK_END:
			{
				OnVerticalNavigation(event, +(int)GetRowCount());
				break;
			}
			case WXK_HOME:
			{
				OnVerticalNavigation(event, -(int)GetRowCount());
				break;
			}
			case WXK_PAGEUP:
			{
				OnVerticalNavigation(event, -(int)(GetCountPerPage() - 1));
				break;
			}
			case WXK_PAGEDOWN:
			{
				OnVerticalNavigation(event, +(int)(GetCountPerPage() - 1));
				break;
			}
			default:
			{
				event.Skip();
			}
		};
	}
	void MainWindow::OnCharHook(wxKeyEvent& event)
	{
		if (m_CurrentEditor)
		{
			// Handle any keys special for the in-place editor and return without calling Skip() below.
			switch (event.GetKeyCode())
			{
				case WXK_ESCAPE:
				{
					CancelEdit();
					return;
				}
				case WXK_RETURN:
				{
					// Shift-Enter is not special neither.
					if (event.ShiftDown())
					{
						break;
					}
					[[fallthrough]];
				}
				case WXK_TAB:
				{
					// Ctrl/Alt-Tab or Enter could be used for something else, so don't handle them here.
					if (event.HasModifiers())
					{
						break;
					}

					EndEdit();
					return;
				}
			};
		}
		else if (m_UseCellFocus)
		{
			if (event.GetKeyCode() == WXK_TAB && !event.HasModifiers())
			{
				event.ShiftDown() ? OnLeftKey(event) : OnRightKey(event);
			}
		}
		event.Skip();
	}
	void MainWindow::OnVerticalNavigation(const wxKeyEvent& event, int delta)
	{
		// If there is no selection, we cannot move it anywhere
		if (!HasCurrentRow() || IsEmpty())
		{
			return;
		}

		// Let's keep the new row inside the allowed range
		intptr_t newRow = (intptr_t)m_CurrentRow + delta;
		if (newRow < 0)
		{
			newRow = 0;
		}

		size_t rowCount = GetRowCount();
		if (newRow >= (intptr_t)rowCount)
		{
			newRow = rowCount - 1;
		}

		Row oldCurrent = m_CurrentRow;
		Row newCurrent = newRow;
		if (newCurrent == oldCurrent)
		{
			return;
		}

		// In single selection we just ignore Shift as we can't select several items anyhow
		if (event.ShiftDown() && !IsSingleSelection())
		{
			RefreshRow(oldCurrent);
			ChangeCurrentRow(newCurrent);

			// Select all the items between the old and the new one
			if (oldCurrent > newCurrent)
			{
				newCurrent = oldCurrent;
				oldCurrent = m_CurrentRow;
			}
			SelectRows(oldCurrent, newCurrent);

			wxSelectionStore::IterationState cookie;
			auto firstSel = m_SelectionStore.GetFirstSelectedItem(cookie);
			if (firstSel != wxSelectionStore::NO_SELECTION)
			{
				SendSelectionChangedEvent(GetNodeByRow(firstSel), m_CurrentColumn);
			}
		}
		else
		{
			RefreshRow(oldCurrent);

			// All previously selected items are unselected unless ctrl is held
			if (!event.ControlDown())
			{
				UnselectAllRows();
			}
			ChangeCurrentRow(newCurrent);

			if (!event.ControlDown())
			{
				SelectRow(m_CurrentRow, true);
				SendSelectionChangedEvent(GetNodeByRow(m_CurrentRow), m_CurrentColumn);
			}
			else
			{
				RefreshRow(m_CurrentRow);
			}
		}
		EnsureVisible(m_CurrentRow);
	}
	void MainWindow::OnLeftKey(wxKeyEvent& event)
	{
		if (IsList())
		{
			TryAdvanceCurrentColumn(nullptr, event, false);
		}
		else
		{
			Node* node = GetNodeByRow(m_CurrentRow);
			if (!node)
			{
				return;
			}
			if (TryAdvanceCurrentColumn(node, event, false))
			{
				return;
			}

			const bool dontCollapseNodes = event.GetKeyCode() == WXK_TAB;
			if (dontCollapseNodes)
			{
				m_CurrentColumn = nullptr;

				// Allow focus change
				event.Skip();
				return;
			}

			// Because TryAdvanceCurrentColumn() return false, we are at the first
			// column or using whole-row selection. In this situation, we can use
			// the standard TreeView handling of the left key.
			if (node->HasChildren() && node->IsNodeExpanded())
			{
				Collapse(m_CurrentRow);
			}
			else
			{
				// If the node is already closed, we move the selection to its parent
				Node* parentNode = node->GetParent();
				if (parentNode)
				{
					Row parent = GetRowByNode(*parentNode);
					if (parent)
					{
						SelectRow(m_CurrentRow, false);
						SelectRow(parent, true);
						ChangeCurrentRow(parent);
						EnsureVisible(parent);
						SendSelectionChangedEvent(parentNode, m_CurrentColumn);
					}
				}
			}
		}
	}
	void MainWindow::OnRightKey(wxKeyEvent& event)
	{
		if (IsList())
		{
			TryAdvanceCurrentColumn(nullptr, event, true);
		}
		else if (Node* node = GetNodeByRow(m_CurrentRow))
		{
			if (node->HasChildren())
			{
				if (!node->IsNodeExpanded())
				{
					Expand(m_CurrentRow);
				}
				else
				{
					// If the node is already open, we move the selection to the first child
					SelectRow(m_CurrentRow, false);
					SelectRow(m_CurrentRow + 1, true);
					ChangeCurrentRow(m_CurrentRow + 1);
					EnsureVisible(m_CurrentRow + 1);
					SendSelectionChangedEvent(GetNodeByRow(m_CurrentRow + 1), m_CurrentColumn);
				}
			}
			else
			{
				TryAdvanceCurrentColumn(node, event, true);
			}
		}
	}

	void MainWindow::OnMouse(wxMouseEvent& event)
	{
		auto ResetHotTrackedExpander = [this]()
		{
			if (m_TreeNodeUnderMouse)
			{
				Row row = GetRowByNode(*m_TreeNodeUnderMouse);
				m_TreeNodeUnderMouse = nullptr;
				RefreshRow(row);
			}
		};

		const wxEventType eventType = event.GetEventType();
		if (eventType == wxEVT_LEAVE_WINDOW || eventType == wxEVT_ENTER_WINDOW)
		{
			ResetHotTrackedExpander();

			event.Skip();
			return;
		}

		if (eventType == wxEVT_MOUSEWHEEL)
		{
			#if 0
			if (event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
			{
				int rateX = 0;
				int rateY = 0;
				m_Owner->GetScrollPixelsPerUnit(&rateX, &rateY);

				event.SetY(event.GetY() + event.GetLinesPerAction() * rateY);
			}
			#endif
			event.Skip();
		}

		if (event.ButtonDown())
		{
			// Not skipping button down events would prevent the system from
			// setting focus to this window as most (all?) of them do by default,
			// so skip it to enable default handling.
			event.Skip();
		}

		int x = event.GetX();
		int y = event.GetY();
		m_View->CalcUnscrolledPosition(x, y, &x, &y);
		Column* col = nullptr;

		int xpos = 0;
		size_t columnsCount = m_View->GetColumnCount();
		for (size_t i = 0; i < columnsCount; i++)
		{
			Column* column = m_View->GetColumnDisplayedAt(i);

			int width = 0;
			if (column->IsExposed(width))
			{
				if (x < xpos + width)
				{
					col = column;
					break;
				}
				xpos += width;
			}
		}

		const Row current = GetRowAt(y);
		Node* item = GetNodeByRow(current);

		// Hot track
		{
			bool rowChnaged = m_HotTrackRow != current;
			bool columnChanged = m_HotTrackColumn != col;
			m_HotTrackColumn = col;

			if (rowChnaged || columnChanged)
			{
				if (rowChnaged)
				{
					m_HotTrackRowEnabled = false;
					RefreshRow(m_HotTrackRow);

					if (item == nullptr)
					{
						ResetHotTrackedExpander();
					}
				}

				m_HotTrackRow = item ? current : INVALID_ROW;
				m_HotTrackRowEnabled = m_HotTrackRow && m_HotTrackColumn;
				RefreshRow(m_HotTrackRow);

				Event hoverEvent(KxEVT_DATAVIEW_ITEM_HOVERED);
				CreateEventTemplate(hoverEvent, item, m_HotTrackColumn);
				m_View->ProcessWindowEvent(hoverEvent);
			}
		}

		// Handle right clicking here, before everything else as context menu
		// events should be sent even when we click outside of any item, unlike all
		// the other ones.
		if (event.RightUp())
		{
			CancelEdit();

			Event evt(KxEVT_DATAVIEW_ITEM_CONTEXT_MENU);
			CreateEventTemplate(evt, item, col);
			m_View->ProcessWindowEvent(evt);
			return;
		}

		#if wxUSE_DRAG_AND_DROP
		if (event.Dragging() || ((m_DragCount > 0) && event.Leaving()))
		{
			if (m_DragCount == 0)
			{
				// We have to report the raw, physical coords as we want to be
				// able to call HitTest(event.m_pointDrag) from the user code to
				// get the item being dragged
				m_DragStart = event.GetPosition();
			}

			m_DragCount++;
			if (m_DragCount < 3 && event.Leaving())
			{
				m_DragCount = 3;
			}
			else if (m_DragCount != 3)
			{
				return;
			}

			if (event.LeftIsDown())
			{
				m_View->CalcUnscrolledPosition(m_DragStart.x, m_DragStart.y, &m_DragStart.x, &m_DragStart.y);
				size_t drag_item_row = GetRowAt(m_DragStart.y);
				Node* itemDragged = GetNodeByRow(drag_item_row);

				// Don't allow invalid items
				if (itemDragged)
				{
					// Notify cell about drag
					EventDND evt(KxEVT_DATAVIEW_ITEM_DRAG);
					CreateEventTemplate(evt, itemDragged, col);
					if (!m_View->HandleWindowEvent(evt) || !evt.IsAllowed())
					{
						return;
					}

					wxDataObject* dragObject = evt.GetDataObject();
					if (!dragObject)
					{
						return;
					}

					DropSource drag(this, drag_item_row);
					drag.SetData(*dragObject);
					drag.DoDragDrop(evt.GetDragFlags());
					delete dragObject;
				}
			}
			return;
		}
		else
		{
			m_DragCount = 0;
		}
		#endif

		// Check if we clicked outside the item area.
		if (current >= GetRowCount() || !col)
		{
			// Follow Windows convention here - clicking either left or right (but not middle) button clears the existing selection.
			if (m_View && (event.LeftDown() || event.RightDown()))
			{
				if (!m_SelectionStore.IsEmpty())
				{
					m_View->UnselectAll();
					SendSelectionChangedEvent(nullptr, col);
				}
			}
			event.Skip();
			return;
		}

		Renderer& renderer = col->GetRenderer();
		Column* expander = m_View->GetExpanderColumnOrFirstOne();

		// Test whether the mouse is hovering over the expander (a.k.a tree "+"
		// button) and also determine the offset of the real cell start, skipping
		// the indentation and the expander itself.
		bool isHoverOverExpander = false;
		int itemOffset = 0;
		if (!IsList() && expander == col)
		{
			Node* node = GetNodeByRow(current);
			itemOffset = m_Indent * node->GetIndentLevel();

			if (node->HasChildren())
			{
				// We make the rectangle we are looking in a bit bigger than the actual
				// visual expander so the user can hit that little thing reliably
				wxRect rect(xpos + itemOffset, GetRowStart(current) + (GetRowHeight(current) - m_UniformRowHeight) / 2, m_UniformRowHeight, m_UniformRowHeight);

				if (rect.Contains(x, y))
				{
					// So the mouse is over the expander
					isHoverOverExpander = true;
					if (m_TreeNodeUnderMouse && m_TreeNodeUnderMouse != node)
					{
						RefreshRow(GetRowByNode(*m_TreeNodeUnderMouse));
					}
					if (m_TreeNodeUnderMouse != node)
					{
						RefreshRow(current);
					}
					m_TreeNodeUnderMouse = node;
				}
			}

			// Account for the expander as well, even if this item doesn't have it,
			// its parent does so it still counts for the offset.
			itemOffset += m_UniformRowHeight;
		}

		if (!isHoverOverExpander && m_TreeNodeUnderMouse != nullptr)
		{
			size_t row = GetRowByNode(*m_TreeNodeUnderMouse);
			m_TreeNodeUnderMouse = nullptr;
			RefreshRow(row);
		}

		bool simulateClick = false;
		bool ignoreOtherColumns = expander != col && item->HasChildren();
		if (event.ButtonDClick())
		{
			m_LastOnSame = false;
		}

		if (event.LeftDClick())
		{
			if (!isHoverOverExpander && (current == m_RowLastClicked))
			{
				Event evt(KxEVT_DATAVIEW_ITEM_ACTIVATED);
				CreateEventTemplate(evt, item, col);
				if (m_View->ProcessWindowEvent(evt))
				{
					// Item activation was handled from the user code.
					return;
				}
			}

			// Either it was a double click over the expander, or the second click
			// happened on another item than the first one or it was a bona fide
			// double click which was unhandled. In all these cases we continue
			// processing this event as a simple click, e.g. to select the item or
			// activate the renderer.
			simulateClick = true;
		}

		if (event.LeftUp() && !isHoverOverExpander)
		{
			if (m_RowSelectSingleOnUp)
			{
				// Select single line
				if (UnselectAllRows(m_RowSelectSingleOnUp))
				{
					SelectRow(m_RowSelectSingleOnUp, true);
					SendSelectionChangedEvent(GetNodeByRow(m_RowSelectSingleOnUp), col);
				}
				// Else it was already selected, nothing to do.
			}

			m_LastOnSame = false;
			m_RowSelectSingleOnUp.MakeInvalid();
		}
		else if (!event.LeftUp())
		{
			// This is necessary, because after a DnD operation in
			// from and to ourself, the up event is swallowed by the
			// DnD code. So on next non-up event (which means here and
			// now) m_lineSelectSingleOnUp should be reset.
			m_RowSelectSingleOnUp = INVALID_ROW;
		}

		if (event.RightDown())
		{
			m_RowBeforeLastClicked = m_RowLastClicked;
			m_RowLastClicked = current;

			// If the item is already selected, do not update the selection.
			// Multi-selections should not be cleared if a selected item is clicked.
			if (!IsRowSelected(current))
			{
				UnselectAllRows();

				const size_t oldCurrent = m_CurrentRow;
				ChangeCurrentRow(current);
				SelectRow(m_CurrentRow, true);
				RefreshRow(oldCurrent);
				SendSelectionChangedEvent(GetNodeByRow(m_CurrentRow), col);
			}
		}
		//else if (event.MiddleDown())
		//{
		//}

	
		if ((event.LeftDown() || simulateClick) && isHoverOverExpander && !event.LeftDClick())
		{
			Node* node = GetNodeByRow(current);

			// hoverOverExpander being true tells us that our node must be
			// valid and have children. So we don't need any extra checks.
			if (node->IsNodeExpanded())
			{
				Collapse(current);
			}
			else
			{
				Expand(current);
			}
		}
		else if ((event.LeftDown() || simulateClick) && !isHoverOverExpander)
		{
			m_RowBeforeLastClicked = m_RowLastClicked;
			m_RowLastClicked = current;

			Row oldCurrentRow = m_CurrentRow;
			bool oldWasSelected = IsRowSelected(m_CurrentRow);

			bool cmdModifierDown = event.CmdDown();
			if (IsSingleSelection() || !(cmdModifierDown || event.ShiftDown()))
			{
				if (IsSingleSelection() || !IsRowSelected(current))
				{
					ChangeCurrentRow(current);
					if (UnselectAllRows(current))
					{
						SelectRow(m_CurrentRow, true);
						SendSelectionChangedEvent(GetNodeByRow(m_CurrentRow), col);
					}
				}
				else 
				{
					// Multi selection & current is highlighted & no mod keys
					m_RowSelectSingleOnUp = current;
					ChangeCurrentRow(current); // change focus
				}
			}
			else
			{
				// Multi selection & either ctrl or shift is down
				if (cmdModifierDown)
				{
					ChangeCurrentRow(current);
					ReverseRowSelection(m_CurrentRow);
					SendSelectionChangedEvent(GetNodeByRow(m_CurrentRow), col);
				}
				else if (event.ShiftDown())
				{
					ChangeCurrentRow(current);

					Row lineFrom = oldCurrentRow;
					Row lineTo = current;

					if (!lineFrom)
					{
						// If we hadn't had any current row before, treat this as a simple click and select the new row only.
						lineFrom = current;
					}

					if (lineTo < lineFrom)
					{
						lineTo = lineFrom;
						lineFrom = m_CurrentRow;
					}

					SelectRows(lineFrom, lineTo);

					wxSelectionStore::IterationState cookie;
					auto firstSel = m_SelectionStore.GetFirstSelectedItem(cookie);
					if (firstSel != wxSelectionStore::NO_SELECTION)
					{
						SendSelectionChangedEvent(GetNodeByRow(firstSel), col);
					}
				}
				else
				{
					// !ctrl, !shift
					// Test in the enclosing if should make it impossible
					wxFAIL_MSG(wxT("how did we get here?"));
				}
			}

			if (m_CurrentRow != oldCurrentRow)
			{
				RefreshRow(oldCurrentRow);
			}
			Column* oldCurrentCol = m_CurrentColumn;

			// Update selection here...
			m_CurrentColumn = col;
			m_IsCurrentColumnSetByKeyboard = false;

			// This flag is used to decide whether we should start editing the item
			// label. We do it if the user clicks twice (but not double clicks,
			// i.e. simulateClick is false) on the same item but not if the click
			// was used for something else already, e.g. selecting the item (so it
			// must have been already selected) or giving the focus to the control
			// (so it must have had focus already).
			m_LastOnSame = !simulateClick && ((col == oldCurrentCol) &&	(current == oldCurrentRow)) && oldWasSelected && HasFocus();

			// Call ActivateCell() after everything else as under GTK+
			if (item->IsEditable(*col))
			{
				// notify cell about click
				wxRect cellRect(xpos + itemOffset, GetRowStart(current), col->GetWidth() - itemOffset, GetRowHeight(current));

				// Note that SetupCellAttributes() should be called after GetRowStart()
				// call in 'cellRect' initialization above as GetRowStart() calls
				// SetupCellAttributes() for other items from inside it.
				renderer.EndRendering();
				renderer.SetupCellAttributes(*item, *col, GetCellStateForRow(oldCurrentRow));

				// Report position relative to the cell's custom area, i.e.
				// not the entire space as given by the control but the one
				// used by the renderer after calculation of alignment etc.
				//
				// Notice that this results in negative coordinates when clicking
				// in the upper left corner of a center-aligned cell which doesn't
				// fill its column entirely so this is somewhat surprising, but we
				// do it like this for compatibility with the native GTK+ version,
				// see #12270.

				// adjust the rectangle ourselves to account for the alignment
				const wxAlignment align = renderer.GetEffectiveAlignment();

				wxRect rectItem = cellRect;
				const wxSize size = renderer.GetCellSize();
				if (size.x >= 0 && size.x < cellRect.width)
				{
					if (align & wxALIGN_CENTER_HORIZONTAL)
					{
						rectItem.x += (cellRect.width - size.x) / 2;
					}
					else if (align & wxALIGN_RIGHT)
					{
						rectItem.x += cellRect.width - size.x;
					}
					// else: wxALIGN_LEFT is the default
				}

				if (size.y >= 0 && size.y < cellRect.height)
				{
					if (align & wxALIGN_CENTER_VERTICAL)
					{
						rectItem.y += (cellRect.height - size.y)/2;
					}
					else if (align & wxALIGN_BOTTOM)
					{
						rectItem.y += cellRect.height - size.y;
					}
					// else: wxALIGN_TOP is the default
				}

				wxMouseEvent event2(event);
				event2.m_x -= rectItem.x;
				event2.m_y -= rectItem.y;
				m_View->CalcUnscrolledPosition(event2.m_x, event2.m_y, &event2.m_x, &event2.m_y);

				renderer.OnActivateCell(*item, cellRect, &event2);
				renderer.EndRendering();
			}
		}

		// For selection rect
		if (event.LeftDown() || event.RightDown())
		{
			RefreshRow(m_CurrentRow);
		}
	}
	void MainWindow::OnSetFocus(wxFocusEvent& event)
	{
		m_HasFocus = true;

		// Make the control usable from keyboard once it gets focus by ensuring
		// that it has a current row, if at all possible.
		if (!HasCurrentRow() && !IsEmpty())
		{
			ChangeCurrentRow(0);
		}
		if (HasCurrentRow())
		{
			Refresh();
		}
		event.Skip();
	}
	void MainWindow::OnKillFocus(wxFocusEvent& event)
	{
		m_HasFocus = false;

		if (HasCurrentRow())
		{
			Refresh();
		}
		event.Skip();
	}

	bool MainWindow::SendExpanderEvent(wxEventType type, Node& item)
	{
		Event event(type);
		CreateEventTemplate(event, &item);

		return !m_View->ProcessWindowEvent(event) || event.IsAllowed();
	}
	void MainWindow::SendSelectionChangedEvent(Node* item, Column* column)
	{
		if (item)
		{
			RefreshRow(GetRowByNode(*item));
		}

		Event event(KxEVT_DATAVIEW_ITEM_SELECTED);
		CreateEventTemplate(event, item, column);
		m_View->ProcessWindowEvent(event);
	}
	bool MainWindow::SendEditingStartedEvent(Node& item, Editor* editor)
	{
		Event event(KxEVT_DATAVIEW_ITEM_EDIT_STARTED);
		CreateEventTemplate(event, &item, editor->GetColumn());

		m_View->ProcessWindowEvent(event);
		return event.IsAllowed();
	}
	bool MainWindow::SendEditingDoneEvent(Node& item, Editor* editor, bool canceled, const wxAny& value)
	{
		EditorEvent event(KxEVT_DATAVIEW_ITEM_EDIT_DONE);
		CreateEventTemplate(event, &item, editor->GetColumn());
		event.SetEditCanceled(canceled);
		event.SetValue(value);

		m_View->ProcessWindowEvent(event);
		return event.IsAllowed();
	}

	/* Drawing */
	void MainWindow::OnPaint(wxPaintEvent& event)
	{
		wxAutoBufferedPaintDC paintDC(this);
		
		const wxSize clientSize = GetClientSize();
		paintDC.SetPen(*wxTRANSPARENT_PEN);
		paintDC.SetBrush(m_View->GetBackgroundColour());
		paintDC.DrawRectangle(clientSize);

		if (IsEmpty())
		{
			// No items to draw.
			return;
		}

		m_View->PrepareDC(paintDC);
		wxGCDC dc(paintDC);

		wxRect updateRect = GetUpdateRegion().GetBox();
		m_View->CalcUnscrolledPosition(updateRect.x, updateRect.y, &updateRect.x, &updateRect.y);

		// Compute which rows needs to be redrawn
		const Row rowStart = GetRowAt(std::max(0, updateRect.y));
		const size_t rowCount = std::min(GetRowAt(std::max(0, updateRect.y + updateRect.height)) - rowStart + 1, GetRowCount() - rowStart);
		const Row rowEnd = rowStart + rowCount;

		// Send the event to the control itself.
		{
			Event cacheEvent(KxEVT_DATAVIEW_CACHE_HINT);
			CreateEventTemplate(cacheEvent);
			cacheEvent.SetCacheHints(rowStart, rowEnd - 1);
			m_View->ProcessWindowEvent(cacheEvent);
		}

		// Compute which columns needs to be redrawn
		const size_t columnCount = m_View->GetColumnCount();
		if (columnCount == 0)
		{
			// We assume that we have at least one column below and painting an empty control is unnecessary anyhow
			return;
		}

		size_t coulumnIndexStart = 0;
		int xCoordStart = 0;
		for (coulumnIndexStart = 0; coulumnIndexStart < columnCount; coulumnIndexStart++)
		{
			Column* col = m_View->GetColumnDisplayedAt(coulumnIndexStart);

			int width = 0;
			if (col->IsExposed(width))
			{
				if (xCoordStart + width >= updateRect.x)
				{
					break;
				}
				xCoordStart += width;
			}
		}

		size_t coulmnIndexEnd = coulumnIndexStart;
		int xCoordEnd = xCoordStart;
		for (; coulmnIndexEnd < columnCount; coulmnIndexEnd++)
		{
			Column* column = m_View->GetColumnDisplayedAt(coulmnIndexEnd);

			int width = 0;
			if (column->IsExposed(width))
			{
				if (xCoordEnd > updateRect.GetRight())
				{
					// If we drawing only part of the control, draw it one pixel wider, to hide not drawn regions.
					if (xCoordEnd + width < GetRowWidth())
					{
						xCoordEnd++;
					}
					break;
				}
				xCoordEnd += width;
			}
		}

		// Draw background of alternate rows specially if required
		if (m_View->IsOptionEnabled(CtrlStyle::AlternatingRowColors))
		{
			KxColor altRowColor = m_View->m_AlternateRowColor;
			if (!altRowColor.IsOk())
			{
				// Determine the alternate rows colour automatically from the background colour.
				const wxColour bgColor = m_View->GetBackgroundColour();

				// Depending on the background, alternate row color will be 3% more dark or 50% brighter.
				int alpha = bgColor.GetRGB() > 0x808080 ? 97 : 150;
				altRowColor = bgColor.ChangeLightness(alpha);
			}

			dc.SetPen(*wxTRANSPARENT_PEN);
			dc.SetBrush(altRowColor);

			// We only need to draw the visible part, so limit the rectangle to it.
			const int x = m_View->CalcUnscrolledPosition(wxPoint(0, 0)).x;
			const int widthRect = clientSize.x;
			for (size_t currentRow = rowStart; currentRow < rowEnd; currentRow++)
			{
				if (currentRow % 2)
				{
					dc.DrawRectangle(x, GetRowStart(currentRow), widthRect, GetRowHeight(currentRow));
				}
			}
		}

		// Redraw the background for the items which are selected/current
		for (size_t currentRow = rowStart; currentRow < rowEnd; currentRow++)
		{
			const bool isCurrentRowSelected = m_SelectionStore.IsSelected(currentRow);
			if (isCurrentRowSelected || currentRow == m_CurrentRow)
			{
				wxRect rowRect(xCoordStart, GetRowStart(currentRow), xCoordEnd - xCoordStart, GetRowHeight(currentRow));
				bool renderColumnFocus = false;

				int flags = wxCONTROL_SELECTED;
				if (m_HasFocus)
				{
					flags |= wxCONTROL_FOCUSED;
				}

				// Draw selection and whole-item focus:
				if (isCurrentRowSelected && !renderColumnFocus)
				{
					wxRendererNative::Get().DrawItemSelectionRect(this, dc, rowRect, flags|wxCONTROL_CURRENT);
				}
			}
		}

		if (m_HotTrackRowEnabled && m_HotTrackRow && m_HotTrackColumn)
		{
			wxRect itemRect(xCoordStart, GetRowStart(m_HotTrackRow), xCoordEnd - xCoordStart, GetRowHeight(m_HotTrackRow));
			wxRendererNative::Get().DrawItemSelectionRect(this, dc, itemRect, wxCONTROL_FOCUSED|wxCONTROL_CURRENT);
		}

		#if wxUSE_DRAG_AND_DROP
		if (m_DropHint)
		{
			wxRect rect(xCoordStart, GetRowStart(m_DropHintLine), xCoordEnd - xCoordStart, GetRowHeight(m_DropHintLine));
			wxRendererNative::Get().DrawFocusRect(this, dc, rect, wxCONTROL_CURRENT);
		}
		#endif

		const Column* expanderColumn = m_View->GetExpanderColumnOrFirstOne();

		// Redraw all cells for all rows which must be repainted and all columns
		wxRect cellRect(xCoordStart, 0, 0, 0);
		for (size_t currentColumnIndex = coulumnIndexStart; currentColumnIndex < coulmnIndexEnd; currentColumnIndex++)
		{
			Column* column = m_View->GetColumnDisplayedAt(currentColumnIndex);
			cellRect.width = column->GetWidth();

			if (!column->IsExposed())
			{
				continue;
			}

			const wxRect columnRect(cellRect.GetX(), 0, cellRect.GetWidth(), m_virtualSize.GetHeight());
			wxDCClipper clip(paintDC, columnRect);

			for (Row currentRow = rowStart; currentRow < rowEnd; ++currentRow)
			{
				// Get the cell value and set it into the renderer
				Node* node = nullptr;
				if (m_VirtualListModel)
				{
					node = static_cast<VirtualListModel*>(m_Model)->GetNode(currentRow);
				}
				else
				{
					node = GetNodeByRow(currentRow);
					if (!node)
					{
						continue;
					}
				}

				Renderer& renderer = node->GetRenderer(*column);
				renderer.BeginRendering(*column, dc, &paintDC);

				// Update cell rect
				cellRect.y = GetRowStart(currentRow);
				cellRect.height = GetRowHeight(currentRow);

				const CellState cellState = GetCellStateForRow(currentRow);
				renderer.SetupCellAttributes(*node, *column, cellState);

				// Draw the background
				renderer.CallDrawCellBackground(cellRect, cellState);

				// deal with the expander
				int indent = 0;
				if (!IsList() && column == expanderColumn)
				{
					wxRect expanderRect;

					// Calculate the indent first
					indent = m_Indent * node->GetIndentLevel();

					// We reserve 'm_UniformRowHeight' of horizontal space for the expander but leave EXPANDER_MARGIN around the expander itself
					expanderRect.SetX(cellRect.x + indent + EXPANDER_MARGIN);

					// Expander X position is calculated, now adjust indent
					indent += m_UniformRowHeight;
					
					// Draw expander if needed and visible
					if (node->HasChildren() && expanderRect.GetX() < cellRect.GetRight())
					{
						dc.SetPen(m_PenExpander);
						dc.SetBrush(wxNullBrush);

						expanderRect.SetWidth(m_UniformRowHeight);
						expanderRect.SetHeight(m_UniformRowHeight);
						expanderRect.SetY(cellRect.y + (cellRect.height - m_UniformRowHeight) / 2 + EXPANDER_MARGIN - EXPANDER_OFFSET);

						int flag = 0;
						if (m_TreeNodeUnderMouse == node)
						{
							flag |= wxCONTROL_CURRENT;
						}
						if (node->IsNodeExpanded())
						{
							flag |= wxCONTROL_EXPANDED;
						}

						constexpr int TVP_GLYPH = 2;
						constexpr int TVP_HOTGLYPH = 4;
						constexpr int GLPS_OPENED = 2;
						constexpr int GLPS_CLOSED = 1;

						if (!m_View->m_UsingSystemTheme || !KxUtility::DrawThemeBackground(this, wxS("TREEVIEW"), dc, flag & wxCONTROL_CURRENT ? TVP_HOTGLYPH : TVP_GLYPH, flag & wxCONTROL_EXPANDED ? GLPS_OPENED : GLPS_CLOSED, expanderRect))
						{
							wxRendererNative::Get().DrawTreeItemButton(this, dc, expanderRect, flag);
						}
					}
				}

				wxRect adjustedCellRect = cellRect;
				adjustedCellRect.Deflate(PADDING_RIGHTLEFT, 0);

				// Account for the tree indent (harmless if we're not indented)
				adjustedCellRect.x += indent;
				adjustedCellRect.width -= indent;

				if (adjustedCellRect.width <= 0)
				{
					continue;
				}
				renderer.CallDrawCellContent(adjustedCellRect, GetCellStateForRow(currentRow));
				renderer.EndRendering();
			}
			cellRect.x += cellRect.width;
		}

		// Draw horizontal rules
		if (m_View->IsOptionEnabled(CtrlStyle::HorizontalRules))
		{
			dc.SetPen(m_PenRuleH);
			dc.SetBrush(*wxTRANSPARENT_BRUSH);

			for (Row currentRow = rowStart; currentRow <= rowEnd; ++currentRow)
			{
				int y = GetRowStart(currentRow);
				dc.DrawLine(xCoordStart, y, xCoordEnd, y);
			}
		}

		// Draw vertical rules
		if (m_View->IsOptionEnabled(CtrlStyle::VerticalRules))
		{
			dc.SetPen(m_PenRuleV);
			dc.SetBrush(*wxTRANSPARENT_BRUSH);

			// Vertical rules are drawn in the last pixel of a column so that
			// they align perfectly with native MSW wxHeaderCtrl as well as for
			// consistency with MSW native list control. There's no vertical
			// rule at the most-left side of the control.

			int x = xCoordStart - 1;
			for (size_t currentColumnIndex = coulumnIndexStart; currentColumnIndex < coulmnIndexEnd; currentColumnIndex++)
			{
				Column* column = m_View->GetColumnDisplayedAt(currentColumnIndex);

				int width = 0;
				if (column->IsExposed(width))
				{
					x += width;
					dc.DrawLine(x, GetRowStart(rowStart), x, GetRowStart(rowEnd) + clientSize.y);
				}
			}
		}
	}
	CellState MainWindow::GetCellStateForRow(Row row) const
	{
		CellState state;
		if (row != INVALID_ROW)
		{
			if (m_HasFocus && IsRowSelected(row))
			{
				state.SetSelected();
			}
			if (m_HotTrackRowEnabled && row == m_HotTrackRow && m_HotTrackColumn)
			{
				state.SetHighlighted();
			}
			if (m_DropHint && row == m_DropHintLine)
			{
				state.SetDropTarget();
			}
		}
		return state;
	}

	void MainWindow::UpdateDisplay()
	{
		m_Dirty = true;
		m_TreeNodeUnderMouse = nullptr;
	}
	void MainWindow::RecalculateDisplay()
	{
		if (m_Model)
		{
			SetVirtualSize(GetRowWidth(), GetRowStart(GetRowCount()));
			m_View->SetScrollRate(10, m_UniformRowHeight);
		}
		Refresh();
	}

	// Columns
	void MainWindow::OnColumnsCountChanged()
	{
		int editableCount = 0;

		size_t count = m_View->GetColumnCount();
		for (size_t i = 0; i < count; i++)
		{
			Column* column = m_View->GetColumnDisplayedAt(i);
			if (column->IsVisible() && column->IsEditable())
			{
				editableCount++;
			}
		}

		m_UseCellFocus = editableCount > 0;
		UpdateDisplay();
	}
	Column* MainWindow::FindColumnForEditing(const Node& item)
	{
		// Edit the current column editable in 'mode'. If no column is focused
		// (typically because the user has full row selected), try to find the
		// first editable column (this would typically be a checkbox for
		// wxDATAVIEW_CELL_ACTIVATABLE and we don't want to force the user to set
		// focus on the checkbox column; or on the only editable text column).

		Column* candidate = m_CurrentColumn;
		if (candidate && !item.IsEditable(*candidate) && !m_IsCurrentColumnSetByKeyboard)
		{
			// If current column was set by mouse to something not editable
			// and the user pressed Space/F2 to edit it, treat the
			// situation as if there was whole-row focus, because that's what is
			// visually indicated and the mouse click could very well be targeted
			// on the row rather than on an individual cell.
			//
			// But if it was done by keyboard, respect that even if the column
			// isn't editable, because focus is visually on that column and editing
			// something else would be surprising.
			candidate = nullptr;
		}

		if (!candidate)
		{
			size_t columnCount = m_View->GetColumnCount();
			for (size_t columnIndex = 0; columnIndex < columnCount; columnIndex++)
			{
				Column* column = m_View->GetColumnDisplayedAt(columnIndex);
				if (column->IsVisible() && item.IsEditable(*column))
				{
					candidate = column;
					break;
				}
			}
		}

		// If on container item without columns, only the expander column may be directly editable:
		if (candidate && m_View->GetExpanderColumn() != candidate && item.HasChildren())
		{
			candidate = m_View->GetExpanderColumn();
		}

		if (candidate && item.IsEditable(*candidate))
		{
			return candidate;
		}
		return nullptr;
	}
	int MainWindow::CalcBestColumnWidth(Column& column) const
	{
		if (column.HasBestWidth())
		{
			return column.GetBestWidth();
		}

		const size_t rowCount = GetRowCount();
		MaxWidthCalculator calculator(const_cast<MainWindow*>(this), column, m_UniformRowHeight);
		calculator.UpdateWithWidth(column.GetMinWidth());

		if (m_View->HasHeaderCtrl())
		{
			calculator.UpdateWithWidth(m_View->GetHeaderCtrl()->GetColumnTitleWidth(column.GetNativeColumn()));
		}

		const wxPoint origin = m_View->CalcUnscrolledPosition(wxPoint(0, 0));
		calculator.ComputeBestColumnWidth(rowCount, GetRowAt(origin.y), GetRowAt(origin.y + GetClientSize().y));

		int maxWidth = calculator.GetMaxWidth();
		if (maxWidth > 0)
		{
			maxWidth += 2 * PADDING_RIGHTLEFT;
		}
		column.SetBestWidth(maxWidth);
		return maxWidth;
	}

	// Items
	size_t MainWindow::RecalculateItemCount() const
	{
		if (m_VirtualListModel)
		{
			return static_cast<VirtualListModel*>(m_Model)->GetItemCount();
		}
		else
		{
			m_TreeRoot->ChangeSubTreeCount(m_TreeRoot->GetChildrenCount());
			return m_TreeRoot->GetSubTreeCount();
		}
	}
	void MainWindow::OnCellChanged(Node& node, Column* column)
	{
		// Move this node to its new correct place after it was updated.
		//
		// In principle, we could skip the call to PutInSortOrder() if the modified
		// column is not the sort column, but in real-world applications it's fully
		// possible and likely that custom compare uses not only the selected model
		// column but also falls back to other values for comparison. To ensure consistency
		// it is better to treat a value change as if it was an item change.

		node.PutInSortOrder();

		if (column == nullptr)
		{
			m_View->InvalidateColumnsBestWidth();
		}
		else
		{
			column->InvalidateBestWidth();
		}

		// Update the displayed value(s).
		RefreshRow(GetRowByNode(node));

		// Send event
		Event event(KxEVT_DATAVIEW_ITEM_VALUE_CHANGED);
		CreateEventTemplate(event, &node, column);
		m_View->ProcessWindowEvent(event);
	}
	void MainWindow::OnNodeAdded(Node& node)
	{
		if (IsVirtualList())
		{
			RecalculateItemCount();
			m_SelectionStore.OnItemsInserted(node.GetRow(), 1);
		}
		else
		{
			InvalidateItemCount();
		}

		m_View->InvalidateColumnsBestWidth();
		UpdateDisplay();
	}
	void MainWindow::OnNodeRemoved(Node& item, intptr_t removedCount)
	{
		if (IsVirtualList())
		{
			RecalculateItemCount();
			m_SelectionStore.OnItemDelete(item.GetRow());
		}
		else
		{
			InvalidateItemCount();

			// Update selection by removing this node and its entire children tree from the selection.
			if (!m_SelectionStore.IsEmpty())
			{
				m_SelectionStore.OnItemsDeleted(item.GetRow(), removedCount);
			}
		}

		// Change the current row to the last row if the current exceed the max row number
		if (m_CurrentRow >= GetRowCount())
		{
			ChangeCurrentRow(m_ItemsCount - 1);
		}

		m_View->InvalidateColumnsBestWidth();
		UpdateDisplay();
	}
	void MainWindow::OnItemsCleared()
	{
		m_SelectionStore.Clear();
		m_CurrentRow.MakeInvalid();
		m_HotTrackRow.MakeInvalid();
		m_CurrentColumn = nullptr;
		m_HotTrackColumn = nullptr;
		m_TreeNodeUnderMouse = nullptr;
		BuildTree();

		m_View->InvalidateColumnsBestWidth();
		UpdateDisplay();
	}
	void MainWindow::OnShouldResort()
	{
		if (!IsVirtualList())
		{
			m_TreeRoot->Resort();
		}
		UpdateDisplay();
	}

	void MainWindow::BuildTree()
	{
		DestroyTree();

		if (!IsVirtualList())
		{
			m_TreeRoot.reset(Node::CreateRootNode(this));
		}
		InvalidateItemCount();
	}
	void MainWindow::DestroyTree()
	{
		m_TreeRoot.reset();
		if (!IsVirtualList())
		{
			m_ItemsCount = 0;
		}
	}

	// Misc
	void MainWindow::OnInternalIdle()
	{
		wxWindow::OnInternalIdle();

		if (!IsMouseInWindow() && m_HotTrackRow)
		{
			m_HotTrackRowEnabled = false;
			RefreshRow(m_HotTrackRow);

			m_HotTrackRow.MakeInvalid();
			m_HotTrackColumn = nullptr;
		}

		if (m_Dirty)
		{
			UpdateColumnSizes();
			RecalculateDisplay();
			m_Dirty = false;
		}
	}

	MainWindow::MainWindow(View* parent, wxWindowID id)
		:wxWindow(parent, id, wxDefaultPosition, wxDefaultSize, wxWANTS_CHARS|wxBORDER_NONE, GetClassInfo()->GetClassName()),
		m_View(parent)
	{
		// Setup drawing
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX));

		KxColor rulesColor = wxSystemSettings::GetColour(wxSYS_COLOUR_3DLIGHT);
		rulesColor.SetA(127);

		m_PenRuleH = rulesColor;
		m_PenRuleV = rulesColor;
		m_PenExpander = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
		m_UniformRowHeight = GetDefaultRowHeight();
		m_Indent = wxSystemSettings::GetMetric(wxSYS_SMALLICON_Y);

		// Create root node
		m_TreeRoot.reset(Node::CreateRootNode(this));

		// Bind events
		Bind(wxEVT_PAINT, &MainWindow::OnPaint, this);
		Bind(wxEVT_SET_FOCUS, &MainWindow::OnSetFocus, this);
		Bind(wxEVT_KILL_FOCUS, &MainWindow::OnKillFocus, this);
		Bind(wxEVT_CHAR_HOOK, &MainWindow::OnCharHook, this);
		Bind(wxEVT_CHAR, &MainWindow::OnChar, this);

		Bind(wxEVT_LEFT_DOWN, &MainWindow::OnMouse, this);
		Bind(wxEVT_LEFT_UP, &MainWindow::OnMouse, this);
		Bind(wxEVT_LEFT_DCLICK, &MainWindow::OnMouse, this);

		//Bind(wxEVT_MIDDLE_DOWN, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_MIDDLE_UP, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_MIDDLE_DCLICK, &KxDataViewMainWindow::OnMouse, this);

		Bind(wxEVT_RIGHT_DOWN, &MainWindow::OnMouse, this);
		Bind(wxEVT_RIGHT_UP, &MainWindow::OnMouse, this);
		Bind(wxEVT_RIGHT_DCLICK, &MainWindow::OnMouse, this);

		Bind(wxEVT_MOTION, &MainWindow::OnMouse, this);
		Bind(wxEVT_ENTER_WINDOW, &MainWindow::OnMouse, this);
		Bind(wxEVT_LEAVE_WINDOW, &MainWindow::OnMouse, this);
		Bind(wxEVT_MOUSEWHEEL, &MainWindow::OnMouse, this);
		//Bind(wxEVT_CHILD_FOCUS, &KxDataViewMainWindow::OnMouse, this);
	
		//Bind(wxEVT_AUX1_DOWN, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_AUX1_UP, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_AUX1_DCLICK, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_AUX2_DOWN, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_AUX2_UP, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_AUX2_DCLICK, &KxDataViewMainWindow::OnMouse, this);
		//Bind(wxEVT_MAGNIFY, &KxDataViewMainWindow::OnMouse, this);

		// Do update
		UpdateDisplay();
	}
	MainWindow::~MainWindow()
	{
		if (m_OwnModel)
		{
			delete m_Model;
		}
	}

	void MainWindow::CreateEventTemplate(Event& event, Node* node, Column* column)
	{
		event.SetId(m_View->GetId());
		event.SetEventObject(m_View);
		event.SetItem(node);
		event.SetColumn(column);
	}
	
	// Model
	void MainWindow::AssignModel(Model* model)
	{
		if (m_OwnModel)
		{
			delete m_Model;
		}

		m_OwnModel = true;
		m_ListModel = model && dynamic_cast<ListModel*>(model) != nullptr;
		m_Model = model;
	}
	void MainWindow::SetModel(Model* model)
	{
		if (m_OwnModel)
		{
			delete m_Model;
		}

		m_OwnModel = false;
		m_ListModel = model && dynamic_cast<ListModel*>(model) != nullptr;
		m_Model = model;
	}

	// Refreshing
	void MainWindow::RefreshRows(Row from, Row to)
	{
		wxRect rect = GetRowsRect(from, to);
		m_View->CalcScrolledPosition(rect.x, rect.y, &rect.x, &rect.y);

		wxSize clientSize = GetClientSize();
		wxRect clientRect(0, 0, clientSize.x, clientSize.y);
		wxRect intersectRect = clientRect.Intersect(rect);
		if (!intersectRect.IsEmpty())
		{
			RefreshRect(intersectRect, true);
		}
	}
	void MainWindow::RefreshRowsAfter(Row firstRow)
	{
		wxSize clientSize = GetClientSize();
		int start = GetRowStart(firstRow);
		m_View->CalcScrolledPosition(start, 0, &start, nullptr);

		if (start <= clientSize.y)
		{
			wxRect rect(0, start, clientSize.x, clientSize.y - start);
			RefreshRect(rect, true);
		}
	}
	void MainWindow::RefreshColumn(const Column& column)
	{
		wxSize size = GetClientSize();

		// Find X coordinate of this column
		int left = 0;
		for (size_t i = 0; i < m_View->GetColumnCount(); i++)
		{
			Column* currentColumn = m_View->GetColumnDisplayedAt(i);
			if (currentColumn)
			{
				if (currentColumn == &column)
				{
					break;
				}
				left += currentColumn->GetWidth();
			}
		}

		RefreshRect(wxRect(left, 0, column.GetWidth(), size.GetHeight()));
	}

	// Item rect
	wxRect MainWindow::GetRowsRect(Row rowFrom, Row rowTo) const
	{
		if (rowFrom > rowTo)
		{
			std::swap(rowFrom, rowTo);
		}

		wxRect rect;
		rect.x = 0;
		rect.y = GetRowStart(rowFrom);

		// Don't calculate exact width of the row, because GetEndOfLastCol() is
		// expensive to call, and controls with rows not spanning entire width rare.
		// It is more efficient to e.g. repaint empty parts of the window needlessly.
		rect.width = std::numeric_limits<decltype(rect.width)>::max();
		if (rowFrom == rowTo)
		{
			rect.height = GetRowHeight(rowFrom);
		}
		else
		{
			rect.height = GetRowStart(rowTo) - rect.y + GetRowHeight(rowTo);
		}
		return rect;
	}
	wxRect MainWindow::GetRowRect(Row row) const
	{
		return GetRowsRect(row, row);
	}
	int MainWindow::GetRowStart(Row row) const
	{
		if (m_View->IsOptionEnabled(CtrlStyle::VariableRowHeight))
		{
			size_t columnCount = m_View->GetColumnCount();
			int start = 0;

			for (size_t currentRow = 0; currentRow < row; currentRow++)
			{
				const Node* node = GetNodeByRow(currentRow);
				if (node)
				{
					int height = m_UniformRowHeight;

					for (size_t currentColumn = 0; currentColumn < columnCount; currentColumn++)
					{
						const Column* column = m_View->GetColumn(currentColumn);
						if (column->IsVisible())
						{
							height = GetVariableRowHeight(*node);
						}
					}
					start += height;
				}
				else
				{
					break;
				}
			}
			return start;
		}
		return row * m_UniformRowHeight;
	}
	int MainWindow::GetRowHeight(Row row) const
	{
		if (m_View->IsOptionEnabled(CtrlStyle::VariableRowHeight))
		{
			const Node* node = GetNodeByRow(row);
			if (node)
			{
				size_t columnCount = m_View->GetColumnCount();

				int height = m_UniformRowHeight;
				for (size_t currentColumn = 0; currentColumn < columnCount; currentColumn++)
				{
					const Column* column = m_View->GetColumn(currentColumn);
					if (column->IsVisible())
					{
						height = GetVariableRowHeight(*node);
					}
				}
				return height;
			}
		}
		return m_UniformRowHeight;
	}
	int MainWindow::GetVariableRowHeight(const Node& node) const
	{
		int height = node.GetRowHeight();
		return height > 0 ? height : m_UniformRowHeight;
	}
	int MainWindow::GetVariableRowHeight(Row row) const
	{
		const Node* node = GetNodeByRow(row);
		return node ? GetVariableRowHeight(*node) : m_UniformRowHeight;
	}
	int MainWindow::GetRowWidth() const
	{
		int width = 0;
		for (size_t i = 0; i < m_View->GetColumnCount(); i++)
		{
			int widthThis = 0;
			if (m_View->GetColumn(i)->IsExposed(widthThis))
			{
				width += widthThis;
			}
		}
		return width;
	}
	Row MainWindow::GetRowAt(int yCoord) const
	{
		if (m_View->IsOptionEnabled(CtrlStyle::VariableRowHeight))
		{
			// TODO: make more efficient

			Row row = 0;
			int yPos = 0;
			while (true)
			{
				const Node* node = GetNodeByRow(row);
				if (!node)
				{
					// Not really correct
					return row + ((yCoord - yPos) / m_UniformRowHeight);
				}

				yPos += GetVariableRowHeight(*node);
				if (yCoord < yPos)
				{
					return row;
				}
				++row;
			}
		}
		return std::abs(yCoord) / m_UniformRowHeight;
	}

	void MainWindow::SetUniformRowHeight(int height)
	{
		m_UniformRowHeight = height > 0 ? height : GetDefaultRowHeight();
	}
	int MainWindow::GetDefaultRowHeight(UniformHeight type) const
	{
		int resultHeight = 0;
		const int iconMargin = 6 * KxSystemSettings::GetMetric(wxSYS_BORDER_Y, this);
		const int iconHeight = KxSystemSettings::GetMetric(wxSYS_SMALLICON_Y, this);

		switch (type)
		{
			case UniformHeight::Default:
			{
				int userHeight = wxSystemOptions::GetOptionInt("KxDataView2::DefaultRowHeight");
				if (userHeight > 0)
				{
					return m_View->FromDIP(userHeight);
				}
				else
				{
					resultHeight = std::max(iconHeight + iconMargin, GetCharHeight() + iconMargin);
				}
				break;
			}
			case UniformHeight::ListView:
			{
				resultHeight = std::max(m_View->FromDIP(17), GetCharHeight() + iconMargin);
				break;
			}
			case UniformHeight::Explorer:
			{
				resultHeight = std::max(m_View->FromDIP(22), GetCharHeight() + iconMargin);
				break;
			}
		};
		return resultHeight >= iconHeight ? resultHeight : iconHeight;
	}

	// Drag and Drop
	wxBitmap MainWindow::CreateItemBitmap(Row row, int& indent)
	{
		int height = GetRowHeight(row);
		int width = GetRowWidth();

		indent = 0;
		if (!IsList())
		{
			Node* node = GetNodeByRow(row);
			indent = m_Indent * node->GetIndentLevel();
			indent = indent + m_UniformRowHeight;

			// Try to use the 'm_UniformLineHeight' as the expander space
		}
		width -= indent;

		wxBitmap bitmap(width, height, 32);
		wxMemoryDC memoryDC(bitmap);
		wxGCDC dc(memoryDC);

		dc.SetFont(GetFont());
		dc.SetBackground(m_View->GetBackgroundColour());
		dc.SetTextBackground(m_View->GetBackgroundColour());
		dc.SetTextForeground(m_View->GetForegroundColour());
		dc.DrawRectangle(wxRect(0, 0, width, height));
		wxRendererNative::Get().DrawItemSelectionRect(this, dc, wxRect(0, 0, width, height), wxCONTROL_CURRENT|wxCONTROL_SELECTED|wxCONTROL_FOCUSED);

		Column* expander = m_View->GetExpanderColumnOrFirstOne();

		int x = 0;
		for (size_t columnIndex = 0; columnIndex < m_View->GetColumnCount(); columnIndex++)
		{
			Column* column = m_View->GetColumnDisplayedAt(columnIndex);
			Renderer& renderer = column->GetRenderer();

			if (column->IsExposed(width))
			{
				if (column == expander)
				{
					width -= indent;
				}

				CellState cellState = GetCellStateForRow(row);

				Node* item = GetNodeByRow(row);

				wxRect itemRect(x, 0, width, height);
				itemRect.Deflate(PADDING_RIGHTLEFT, 0);

				renderer.BeginRendering(*column, dc, &memoryDC);
				renderer.SetupCellAttributes(*item, *column, cellState);

				renderer.CallDrawCellBackground(itemRect, cellState);
				renderer.CallDrawCellContent(itemRect, cellState);
				
				renderer.EndRendering();
				x += width;
			}
		}

		memoryDC.SelectObject(wxNullBitmap);
		return bitmap;
	}
	bool MainWindow::EnableDragSource(const wxDataFormat& format)
	{
		m_DragFormat = format;
		m_DragEnabled = format != wxDF_INVALID;

		return true;
	}
	bool MainWindow::EnableDropTarget(const wxDataFormat& format)
	{
		m_DropFormat = format;
		m_DropEnabled = format != wxDF_INVALID;

		if (m_DropEnabled)
		{
			SetDropTarget(new DropTarget(new wxCustomDataObject(format), this));
		}
		return true;
	}

	void MainWindow::OnDragDropGetRowNode(const wxPoint& pos, Row& row, Node*& item)
	{
		// Get row
		wxPoint unscrolledPos;
		m_View->CalcUnscrolledPosition(pos.x, pos.y, &unscrolledPos.x, &unscrolledPos.y);
		row = GetRowAt(unscrolledPos.y);

		// Get item
		if (row < GetRowCount() && pos.y <= GetRowWidth())
		{
			item = GetNodeByRow(row);
		}
	}
	void MainWindow::RemoveDropHint()
	{
		if (m_DropHint)
		{
			m_DropHint = false;
			RefreshRow(m_DropHintLine);
			m_DropHintLine = INVALID_ROW;
		}
	}
	wxDragResult MainWindow::OnDragOver(const wxDataFormat& format, const wxPoint& pos, wxDragResult dragResult)
	{
		Row row;
		Node* item = nullptr;
		OnDragDropGetRowNode(pos, row, item);

		if (row != 0 && row < GetRowCount())
		{
			size_t firstVisible = GetFirstVisibleRow();
			size_t lastVisible = GetLastVisibleRow();
			if (row == firstVisible || row == firstVisible + 1)
			{
				ScrollTo(row - 1);
			}
			else if (row == lastVisible || row == lastVisible - 1)
			{
				ScrollTo(firstVisible + 1);
			}
		}

		EventDND event(KxEVT_DATAVIEW_ITEM_DROP_POSSIBLE);
		CreateEventTemplate(event, item);
		event.SetDataFormat(format);
		event.SetDropEffect(dragResult);

		if (!m_View->HandleWindowEvent(event) || !event.IsAllowed())
		{
			RemoveDropHint();
			return wxDragNone;
		}

		if (item)
		{
			if (m_DropHint && (row != m_DropHintLine))
			{
				RefreshRow(m_DropHintLine);
			}
			m_DropHint = true;
			m_DropHintLine = row;
			RefreshRow(row);
		}
		else
		{
			RemoveDropHint();
		}
		return dragResult;
	}
	wxDragResult MainWindow::OnDragData(const wxDataFormat& format, const wxPoint& pos, wxDragResult dragResult)
	{
		Row row;
		Node* item = nullptr;
		OnDragDropGetRowNode(pos, row, item);

		wxCustomDataObject* dataObject = (wxCustomDataObject*)GetDropTarget()->GetDataObject();

		EventDND event(KxEVT_DATAVIEW_ITEM_DROP);
		CreateEventTemplate(event, item);
		event.SetDataFormat(format);
		event.SetDataSize(dataObject->GetSize());
		event.SetDataBuffer(dataObject->GetData());
		event.SetDropEffect(dragResult);

		if (!m_View->HandleWindowEvent(event) || !event.IsAllowed())
		{
			return wxDragNone;
		}
		return dragResult;
	}
	bool MainWindow::OnDrop(const wxDataFormat& format, const wxPoint& pos)
	{
		RemoveDropHint();

		Row row;
		Node* item = nullptr;
		OnDragDropGetRowNode(pos, row, item);

		EventDND event(KxEVT_DATAVIEW_ITEM_DROP_POSSIBLE);
		CreateEventTemplate(event, item);
		event.SetDataFormat(format);

		if (!m_View->HandleWindowEvent(event) || !event.IsAllowed())
		{
			return false;
		}
		return true;
	}
	void MainWindow::OnDragDropLeave()
	{
		RemoveDropHint();
	}

	// Scrolling
	void MainWindow::ScrollWindow(int dx, int dy, const wxRect* rect)
	{
		m_TreeNodeUnderMouse = nullptr;
		wxWindow::ScrollWindow(dx, dy, rect);

		if (wxHeaderCtrl* header = m_View->GetHeaderCtrl())
		{
			header->ScrollWindow(dx, 0);
		}
	}
	void MainWindow::ScrollTo(Row row, size_t column)
	{
		m_TreeNodeUnderMouse = nullptr;

		wxPoint pos;
		m_View->GetScrollPixelsPerUnit(&pos.x, &pos.y);
		wxPoint scrollPos(-1, GetRowStart(row) / pos.y);

		if (column != INVALID_COLUMN)
		{
			wxRect rect = GetClientRect();

			wxPoint unscrolledPos;
			m_View->CalcUnscrolledPosition(rect.x, rect.y, &unscrolledPos.x, &unscrolledPos.y);

			int columnWidth = 0;
			int x_start = 0;
			for (size_t colnum = 0; colnum < column; colnum++)
			{
				if (m_View->GetColumnDisplayedAt(colnum)->IsExposed(columnWidth))
				{
					x_start += columnWidth;
				}
			}

			int x_end = x_start + columnWidth;
			int xe = unscrolledPos.x + rect.width;
			if (x_end > xe && scrollPos.x != 0)
			{
				scrollPos.x = (unscrolledPos.x + x_end - xe) / scrollPos.x;
			}
			if (x_start < unscrolledPos.x && scrollPos.x)
			{
				scrollPos.x = x_start / scrollPos.x;
			}
		}
		m_View->Scroll(scrollPos);
	}
	void MainWindow::EnsureVisible(Row row, size_t column)
	{
		if (row > GetRowCount())
		{
			row = GetRowCount();
		}

		size_t first = GetFirstVisibleRow();
		size_t last = GetLastVisibleRow();
		if (row < first)
		{
			ScrollTo(row, column);
		}
		else if (row > last)
		{
			ScrollTo((intptr_t)row - (intptr_t)last + (intptr_t)first, column);
		}
		else
		{
			ScrollTo(first, column);
		}
	}

	// Current row and column
	void MainWindow::ChangeCurrentRow(Row row)
	{
		m_CurrentRow = row;
		// Send event ?
	}
	bool MainWindow::TryAdvanceCurrentColumn(Node* node, wxKeyEvent& event, bool moveForward)
	{
		if (m_View->GetColumnCount() == 0)
		{
			return false;
		}

		if (!m_UseCellFocus)
		{
			return false;
		}
		const bool wrapAround = event.GetKeyCode() == WXK_TAB;

		if (node && node->HasChildren())
		{
			return false;
		}

		if (m_CurrentColumn == nullptr || !m_IsCurrentColumnSetByKeyboard)
		{
			if (moveForward)
			{
				m_CurrentColumn = m_View->GetColumnDisplayedAt(1);
				m_IsCurrentColumnSetByKeyboard = true;
				RefreshRow(m_CurrentRow);
				return true;
			}
			else
			{
				if (!wrapAround)
				{
					return false;
				}
			}
		}

		size_t nextColumn = m_CurrentColumn->GetIndex() + (moveForward ? +1 : -1);
		if (nextColumn >= m_View->GetColumnCount())
		{
			if (!wrapAround)
			{
				return false;
			}

			if (GetCurrentRow() < GetRowCount() - 1)
			{
				// Go to the first column of the next row:
				nextColumn = 0;
				OnVerticalNavigation(wxKeyEvent(), +1);
			}
			else
			{
				// allow focus change
				event.Skip();
				return false;
			}
		}

		if (nextColumn < 0 && wrapAround)
		{
			if (GetCurrentRow() > 0)
			{
				// Go to the last column of the previous row
				nextColumn = m_View->GetColumnCount() - 1;
				OnVerticalNavigation(wxKeyEvent(), -1);
			}
			else
			{
				// Allow focus change
				event.Skip();
				return false;
			}
		}

		EnsureVisible(m_CurrentRow, nextColumn);

		if (nextColumn < 1)
		{
			// We are going to the left of the second column. Reset to whole-row
			// focus (which means first column would be edited).
			m_CurrentColumn = nullptr;
			RefreshRow(m_CurrentRow);
			return true;
		}

		m_CurrentColumn = m_View->GetColumnDisplayedAt(nextColumn);
		m_IsCurrentColumnSetByKeyboard = true;
		RefreshRow(m_CurrentRow);
		return true;
	}

	Node* MainWindow::GetHotTrackItem() const
	{
		if (m_HotTrackRow)
		{
			return GetNodeByRow(m_HotTrackRow);
		}
		return nullptr;
	}
	Column* MainWindow::GetHotTrackColumn() const
	{
		return m_HotTrackColumn;
	}

	// Selection
	bool MainWindow::UnselectAllRows(Row exceptThisRow)
	{
		if (!IsSelectionEmpty())
		{
			for (size_t i = GetFirstVisibleRow(); i <= GetLastVisibleRow(); i++)
			{
				if (m_SelectionStore.IsSelected(i) && i != exceptThisRow)
				{
					RefreshRow(i);
				}
			}

			if (exceptThisRow)
			{
				const bool wasSelected = m_SelectionStore.IsSelected(exceptThisRow);
				ClearSelection();
				if (wasSelected)
				{
					m_SelectionStore.SelectItem(exceptThisRow);

					// The special item is still selected.
					return false;
				}
			}
			else
			{
				ClearSelection();
			}
		}

		// There are no selected items left.
		return true;
	}
	void MainWindow::ReverseRowSelection(Row row)
	{
		m_SelectionStore.SelectItem(row, !IsRowSelected(row));
		RefreshRow(row);
	}
	void MainWindow::SelectRow(Row row, bool select)
	{
		if (m_SelectionStore.SelectItem(row, select))
		{
			RefreshRow(row);
		}
	}
	void MainWindow::SelectRows(Row from, Row to)
	{
		wxArrayInt changed;
		if (m_SelectionStore.SelectRange(from, to, true, &changed))
		{
			for (size_t i = 0; i < changed.size(); i++)
			{
				RefreshRow(changed[i]);
			}
		}
		else
		{
			// Selection of too many rows has changed.
			RefreshRows(from, to);
		}
	}
	void MainWindow::SelectRows(const Row::Vector& selection)
	{
		for (const Row& row: selection)
		{
			if (m_SelectionStore.SelectItem(row))
			{
				RefreshRow(row);
			}
		}
	}

	// View
	size_t MainWindow::GetRowCount() const
	{
		if (m_ItemsCount == INVALID_COUNT)
		{
			MainWindow* self = const_cast<MainWindow*>(this);
			self->UpdateItemCount(RecalculateItemCount());
			self->UpdateDisplay();
		}
		return m_ItemsCount;
	}
	size_t MainWindow::GetCountPerPage() const
	{
		wxSize size = GetClientSize();
		return size.y / m_UniformRowHeight;
	}
	Row MainWindow::GetFirstVisibleRow() const
	{
		wxPoint pos(0, 0);
		m_View->CalcUnscrolledPosition(pos.x, pos.y, &pos.x, &pos.y);
		return GetRowAt(pos.y);
	}
	Row MainWindow::GetLastVisibleRow() const
	{
		wxSize size = GetClientSize();
		m_View->CalcUnscrolledPosition(size.x, size.y, &size.x, &size.y);

		// We should deal with the pixel here.
		size_t row = GetRowAt(size.y) - 1;
		return std::min(GetRowCount() - 1, row);
	}

	void MainWindow::HitTest(const wxPoint& pos, Node*& item, Column*& column)
	{
		Column* columnFound = nullptr;
		size_t columnCount = m_View->GetColumnCount();

		wxPoint unscrolledPos;
		m_View->CalcUnscrolledPosition(pos.x, pos.y, &unscrolledPos.x, &unscrolledPos.y);

		int x_start = 0;
		for (size_t colnumIndex = 0; colnumIndex < columnCount; colnumIndex++)
		{
			Column* testColumn = m_View->GetColumnDisplayedAt(colnumIndex);

			int width = 0;
			if (testColumn->IsExposed(width))
			{
				if (x_start + width >= unscrolledPos.x)
				{
					columnFound = testColumn;
					break;
				}
				x_start += width;
			}
		}

		column = columnFound;
		item = GetNodeByRow(GetRowAt(unscrolledPos.y));
	}
	wxRect MainWindow::GetItemRect(const Node& item, const Column* column)
	{
		int xpos = 0;
		int width = 0;
		size_t columnCount = m_View->GetColumnCount();

		// If column is null the loop will compute the combined width of all columns.
		// Otherwise, it will compute the x position of the column we are looking for.
		for (size_t i = 0; i < columnCount; i++)
		{
			Column* currentColumn = m_View->GetColumnDisplayedAt(i);
			if (currentColumn == column)
			{
				break;
			}

			int widthCurrent = 0;
			if (currentColumn->IsExposed(widthCurrent))
			{
				xpos += widthCurrent;
				width += widthCurrent;
			}
		}

		if (column)
		{
			// If we have a column, we need can get its width directly.
			width = 0;
			column->IsExposed(width);
		}
		else
		{
			// If we have no column, we reset the x position back to zero.
			xpos = 0;
		}

		Row row = GetRowByNode(item);
		if (!row)
		{
			// This means the row is currently not visible at all.
			return wxRect();
		}

		// We have to take an expander column into account and compute its indentation
		// to get the correct x position where the actual text is.
		int indent = 0;
		if (!IsList() && (!column || m_View->GetExpanderColumnOrFirstOne() == column))
		{
			Node* node = GetNodeByRow(row);
			indent = m_Indent * node->GetIndentLevel();
		
			// Use 'm_UniformLineHeight' as the width of the expander
			indent += m_UniformRowHeight;
		}

		wxRect itemRect(xpos + indent, GetRowStart(row), width - indent, GetRowHeight(row));
		m_View->CalcScrolledPosition(itemRect.x, itemRect.y, &itemRect.x, &itemRect.y);
		return itemRect;
	}

	void MainWindow::UpdateColumnSizes()
	{
		size_t columnCount = m_View->GetVisibleColumnCount();
		if (columnCount != 0)
		{
			Column* lastColumn = m_View->GetColumnDisplayedAt(columnCount - 1);
			if (lastColumn)
			{
				const int fullWinWidth = GetSize().x;
				const int columnsFullWidth = GetRowWidth();
				const int lastColumnLeft = columnsFullWidth - lastColumn->GetWidth();

				if (lastColumnLeft < fullWinWidth)
				{
					int desiredWidth = std::max(fullWinWidth - lastColumnLeft, lastColumn->GetMinWidth());
					if (desiredWidth < lastColumn->CalcBestSize())
					{
						lastColumn->SetWidth(lastColumn->GetWidth());
						SetVirtualSize(columnsFullWidth, m_virtualSize.y);
						return;
					}
					lastColumn->SetWidth(desiredWidth);

					// All columns fit on screen, so we don't need horizontal scrolling.
					// To prevent flickering scrollbar when resizing the window to be
					// narrower, force-set the virtual width to 0 here. It will eventually
					// be corrected at idle time.
					SetVirtualSize(0, m_virtualSize.y);
					RefreshRect(wxRect(lastColumnLeft, 0, fullWinWidth - lastColumnLeft, GetSize().y));
				}
				else
				{
					lastColumn->SetWidth(lastColumn->GetWidth());

					// Don't bother, the columns won't fit anyway
					SetVirtualSize(columnsFullWidth, m_virtualSize.y);
				}
			}
		}
	}

	// Rows
	void MainWindow::Expand(Row row)
	{
		if (!IsList())
		{
			if (Node* node = GetNodeByRow(row))
			{
				Expand(*node, row);
			}
		}
	}
	void MainWindow::Expand(Node& node, Row row)
	{
		if (node.HasChildren() && !node.IsNodeExpanded())
		{
			if (!SendExpanderEvent(KxEVT_DATAVIEW_ITEM_EXPANDING, node))
			{
				// Vetoed by the event handler.
				return;
			}
			node.ToggleNodeExpanded();

			const intptr_t countNewRows = node.GetSubTreeCount();

			// Shift all stored indices after this row by the number of newly added rows.
			if (!row)
			{
				row = GetRowByNode(node);
			}

			m_SelectionStore.OnItemsInserted(row + 1, countNewRows);
			if (m_CurrentRow > row)
			{
				ChangeCurrentRow(m_CurrentRow + countNewRows);
			}

			if (m_ItemsCount != INVALID_COUNT)
			{
				m_ItemsCount += countNewRows;
			}

			// Expanding this item means the previously cached column widths could
			// have become invalid as new items are now visible.
			m_View->InvalidateColumnsBestWidth();
			UpdateDisplay();

			// Send the expanded event
			SendExpanderEvent(KxEVT_DATAVIEW_ITEM_EXPANDED, node);
		}
	}

	void MainWindow::Collapse(Row row)
	{
		if (!IsList())
		{
			if (Node* node = GetNodeByRow(row))
			{
				Collapse(*node, row);
			}
		}
	}
	void MainWindow::Collapse(Node& node, Row row)
	{
		if (node.HasChildren() && node.IsNodeExpanded())
		{
			if (!SendExpanderEvent(KxEVT_DATAVIEW_ITEM_COLLAPSING, node))
			{
				// Vetoed by the event handler.
				return;
			}

			if (!row)
			{
				row = GetRowByNode(node);
			}

			intptr_t countDeletedRows = node.GetSubTreeCount();
			if (m_SelectionStore.OnItemsDeleted(row + 1, countDeletedRows))
			{
				RefreshRow(row);
				SendSelectionChangedEvent(GetNodeByRow(row), m_CurrentColumn);
			}
			node.ToggleNodeExpanded();

			// Adjust the current row if necessary.
			if (m_CurrentRow > row)
			{
				// If the current row was among the collapsed items, make the
				// parent itself current.
				if (m_CurrentRow <= row + countDeletedRows)
				{
					ChangeCurrentRow(row);
				}
				else
				{
					// Otherwise just update the index.
					ChangeCurrentRow(m_CurrentRow - countDeletedRows);
				}
			}

			if (m_ItemsCount != INVALID_COUNT)
			{
				m_ItemsCount -= countDeletedRows;
			}

			m_View->InvalidateColumnsBestWidth();
			UpdateDisplay();

			SendExpanderEvent(KxEVT_DATAVIEW_ITEM_COLLAPSED, node);
		}
	}

	void MainWindow::ToggleExpand(Row row)
	{
		IsExpanded(row) ? Collapse(row) : Expand(row);
	}
	bool MainWindow::IsExpanded(Row row) const
	{
		if (!IsList())
		{
			Node* node = GetNodeByRow(row);
			if (node && node->HasChildren())
			{
				return node->IsNodeExpanded();
			}
		}
		return false;
	}
	bool MainWindow::HasChildren(Row row) const
	{
		if (!m_ListModel)
		{
			Node* node = GetNodeByRow(row);
			return node && node->HasChildren();
		}
		return false;
	}

	Node* MainWindow::GetNodeByRow(Row row) const
	{
		if (m_VirtualListModel)
		{
			if (row < GetRowCount())
			{
				return static_cast<VirtualListModel*>(m_Model)->GetNode(row);
			}
		}
		else if (row)
		{
			NodeOperation_RowToNode operation(row, -2);
			operation.Walk(*m_TreeRoot);
			return operation.GetResult();
		}
		return nullptr;
	}
	Row MainWindow::GetRowByNode(const Node& node) const
	{
		if (m_VirtualListModel)
		{
			return static_cast<VirtualListModel*>(m_Model)->GetRow(node);
		}
		else if (m_Model)
		{
			// Compose the parent-chain of the item we are looking for
			Node::Vector parentChain;
			Node* currentNode = const_cast<Node*>(&node);
			while (currentNode)
			{
				parentChain.push_back(currentNode);
				currentNode = currentNode->GetParent();
			}

			// The parent chain was created by adding the deepest parent first.
			// so if we want to start at the root node, we have to iterate backwards through the vector
			NodeOperation_NodeToRow operation(node, parentChain.rbegin());

			// If the item was not found at all, which can happen if all its parents
			// are not expanded, this function still returned a valid but completely
			// wrong row index.

			// This affected many functions which could call it for the items which
			// were not necessarily visible, i.e. all of them except for the event
			// handlers as events can only affect the visible items, including but not
			// limited to SetCurrentItem(), all the selection-related functions, all
			// the expansion-related functions, EnsureVisible(), HitTest() and GetItemRect().
			if (operation.Walk(*m_TreeRoot))
			{
				return operation.GetResult();
			}
		}
		return Row();
	}

	bool MainWindow::BeginEdit(Node& node, Column& column)
	{
		// Cancel any previous editing
		CancelEdit();

		Editor* editor = node.GetEditor(column);
		if (editor)
		{
			m_View->EnsureVisible(node, &column);
			m_View->SetFocus();

			const wxRect itemRect = GetItemRect(node, &column);
			if (editor->BeginEdit(node, column, itemRect))
			{
				// Save the renderer to be able to finish/cancel editing it later.
				m_CurrentEditor = editor;
				return true;
			}
		}
		return false;
	}
	void MainWindow::EndEdit()
	{
		if (m_CurrentEditor)
		{
			m_CurrentEditor->EndEdit();
			m_CurrentEditor = nullptr;
		}
	}
	void MainWindow::CancelEdit()
	{
		if (m_CurrentEditor)
		{
			m_CurrentEditor->CancelEdit();
			m_CurrentEditor = nullptr;
		}
	}
}

namespace Kx::DataView2
{
	void DropSource::OnPaint(wxPaintEvent& event)
	{
		wxPaintDC dc(m_DragImage);
		dc.DrawBitmap(m_HintBitmap, 0, 0);
	}
	void DropSource::OnScroll(wxMouseEvent& event)
	{
		if (m_MainWindow)
		{
			View* view = m_MainWindow->GetView();

			int rateX = 0;
			int rateY = 0;
			view->GetScrollPixelsPerUnit(&rateX, &rateY);
			wxPoint startPos = view->GetViewStart();

			wxCoord value = -event.GetWheelRotation();
			if (event.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
			{
				view->Scroll(wxDefaultCoord, startPos.y + (float)value / (rateY != 0 ? rateY : 1));
			}
			else
			{
				view->Scroll(startPos.x + (float)value / (rateX != 0 ? rateX : 1), wxDefaultCoord);
			}
		}
		event.Skip();
	}
	bool DropSource::GiveFeedback(wxDragResult effect)
	{
		wxPoint mousePos = wxGetMousePosition();

		if (!m_DragImage)
		{
			wxPoint linePos(0, m_MainWindow->GetRowStart(m_Row));

			m_MainWindow->GetView()->CalcUnscrolledPosition(0, linePos.y, nullptr, &linePos.y);
			m_MainWindow->ClientToScreen(&linePos.x, &linePos.y);

			m_Distance.x = mousePos.x - linePos.x;
			m_Distance.y = mousePos.y - linePos.y;

			int rowIndent = 0;
			m_HintBitmap = m_MainWindow->CreateItemBitmap(m_Row, rowIndent);

			m_Distance.x -= rowIndent;
			m_HintPosition = GetHintPosition(mousePos);

			int frameStyle = wxFRAME_FLOAT_ON_PARENT|wxFRAME_NO_TASKBAR|wxNO_BORDER;
			m_DragImage = new wxMiniFrame(m_MainWindow, wxID_NONE, wxEmptyString, m_HintPosition, m_HintBitmap.GetSize(), frameStyle);
			//m_DragImage = new wxPopupWindow(m_MainWindow);
			//m_DragImage->SetInitialSize(m_HintBitmap.GetSize());

			m_DragImage->Bind(wxEVT_PAINT, &DropSource::OnPaint, this);
			m_DragImage->SetTransparent(128);
			m_DragImage->Show();
		}
		else
		{
			m_HintPosition = GetHintPosition(mousePos);
			m_DragImage->Move(m_HintPosition);
		}
		return false;
	}

	wxPoint DropSource::GetHintPosition(const wxPoint& mousePos) const
	{
		return wxPoint(mousePos.x - m_Distance.x, mousePos.y + 5);
	}

	DropSource::DropSource(MainWindow* mainWindow, size_t row)
		:wxDropSource(mainWindow), m_MainWindow(mainWindow), m_Row(row), m_Distance(0, 0)
	{
		//m_MainWindow->Bind(wxEVT_MOUSEWHEEL, &KxDataViewMainWindowDropSource::OnScroll, this);
	}
	DropSource::~DropSource()
	{
		m_HintPosition = wxDefaultPosition;
		delete m_DragImage;
	}
}

namespace Kx::DataView2
{
	wxDragResult DropTarget::OnDragOver(wxCoord x, wxCoord y, wxDragResult dragResult)
	{
		wxDataFormat format = GetMatchingPair();
		if (format != wxDF_INVALID)
		{
			return m_MainWindow->OnDragOver(format, wxPoint(x, y), dragResult);
		}
		return wxDragNone;
	}
	bool DropTarget::OnDrop(wxCoord x, wxCoord y)
	{
		wxDataFormat format = GetMatchingPair();
		if (format != wxDF_INVALID)
		{
			return m_MainWindow->OnDrop(format, wxPoint(x, y));
		}
		return false;
	}
	wxDragResult DropTarget::OnData(wxCoord x, wxCoord y, wxDragResult dragResult)
	{
		wxDataFormat format = GetMatchingPair();
		if (format == wxDF_INVALID || !GetData())
		{
			return wxDragNone;
		}
		else
		{
			return m_MainWindow->OnDragData(format, wxPoint(x, y), dragResult);
		}
	}
	void DropTarget::OnLeave()
	{
		m_MainWindow->OnDragDropLeave();
	}

	DropTarget::DropTarget(wxDataObject* dataObject, MainWindow* mainWindow)
		:wxDropTarget(dataObject), m_MainWindow(mainWindow)
	{
	}
}