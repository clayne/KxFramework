#include "stdafx.h"
#include "CellState.h"
#include "MainWindow.h"

namespace kxf::UI::DataView
{
	FlagSet<NativeWidgetFlag> CellState::ToNativeWidgetFlags(const MainWindow& window) const
	{
		FlagSet<NativeWidgetFlag> itemState;
		itemState.Add(NativeWidgetFlag::Focused, IsHotTracked() || window.HasFocus());
		itemState.Add(NativeWidgetFlag::Selected, IsSelected());
		itemState.Add(NativeWidgetFlag::Current, IsCurrent());

		return itemState;
	}
}
