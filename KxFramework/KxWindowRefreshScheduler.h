#pragma once
#include "KxFramework/KxFramework.h"

template<class TWindow>
class KxWindowRefreshScheduler: public TWindow
{
	private:
		bool m_RefreshScheduled = false;
		bool m_EraseBackground = false;
		std::optional<wxRect> m_Rect;

	private:
		void DoScheduleRefresh(bool eraseBackground, const wxRect* rect = nullptr)
		{
			m_RefreshScheduled = true;
			m_EraseBackground = eraseBackground;

			if (rect)
			{
				m_Rect = *rect;
			}
			else
			{
				m_Rect = std::nullopt;
			}
		}
		void DoOnInternalIdle(bool eraseBackground, std::optional<wxRect> rect)
		{
			wxWindow::Refresh(eraseBackground, rect ? &*rect : nullptr);
		}

	protected:
		void OnInternalIdle() override
		{
			TWindow::OnInternalIdle();

			if (m_RefreshScheduled)
			{
				m_RefreshScheduled = false;
				DoOnInternalIdle(m_EraseBackground, std::move(m_Rect));
			}
		}

	public:
		KxWindowRefreshScheduler() = default;
		template<class... Args> KxWindowRefreshScheduler(Args&&... arg)
			:TWindow(std::forward<Args>(arg)...)
		{
		}

	public:
		bool IsRefreshScheduled() const
		{
			return m_RefreshScheduled;
		}

		void ScheduleRefresh(bool eraseBackground = true)
		{
			DoScheduleRefresh(eraseBackground);
		}
		void ScheduleRefreshRect(const wxRect& rect, bool eraseBackground = true)
		{
			DoScheduleRefresh(eraseBackground, &rect);
		}
};
