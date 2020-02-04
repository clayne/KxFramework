#include "KxStdAfx.h"
#include "IWindowRenderer.h"
#include "Renderers/DirectX.h"

namespace KxSciter
{
	std::unique_ptr<IWindowRenderer> IWindowRenderer::CreateInstance(WindowRenderer type, wxWindow& sciterWindow)
	{
		switch (type)
		{
			case WindowRenderer::DirectX:
			{
				return std::make_unique<DirectX>(sciterWindow);
			}
		};
		return nullptr;
	}
}
