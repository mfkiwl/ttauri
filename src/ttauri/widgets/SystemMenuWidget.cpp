// Copyright 2020 Pokitec
// All rights reserved.

#include "SystemMenuWidget.hpp"
#include "../GUI/utils.hpp"
#include "../text/TTauriIcons.hpp"
#include "../utils.hpp"
#include <Windows.h>
#include <WinUser.h>
#include <cmath>
#include <typeinfo>

namespace tt {

SystemMenuWidget::SystemMenuWidget(Window &window, Widget *parent, Image const &icon) noexcept :
    Widget(window, parent), iconCell(icon.makeCell())
{
    // Toolbar buttons hug the toolbar and neighbour widgets.
    _margin = 0.0f;
}

[[nodiscard]] bool SystemMenuWidget::updateConstraints() noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    if (Widget::updateConstraints()) {
        ttlet width = Theme::toolbarDecorationButtonWidth;
        ttlet height = Theme::toolbarHeight;
        _preferred_size = {vec{width, height}, vec{width, std::numeric_limits<float>::infinity()}};
        return true;
    } else {
        return false;
    }
}

[[nodiscard]] bool SystemMenuWidget::updateLayout(hires_utc_clock::time_point display_time_point, bool need_layout) noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    need_layout |= std::exchange(requestLayout, false);
    if (need_layout) {
        // Leave space for window resize handles on the left and top.
        system_menu_rectangle = aarect{
            rectangle().x() + Theme::margin,
            rectangle().y(),
            rectangle().width() - Theme::margin,
            rectangle().height() - Theme::margin};
    }

    return Widget::updateLayout(display_time_point, need_layout);
}

void SystemMenuWidget::draw(DrawContext context, hires_utc_clock::time_point display_time_point) noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    iconCell->draw(context, rectangle(), Alignment::MiddleCenter);
    Widget::draw(std::move(context), display_time_point);
}

HitBox SystemMenuWidget::hitBoxTest(vec window_position) const noexcept
{
    ttlet lock = std::scoped_lock(mutex);

    if (_window_clipping_rectangle.contains(window_position)) {
        // Only the top-left square should return ApplicationIcon, leave
        // the reset to the toolbar implementation.
        return HitBox{this, _draw_layer, HitBox::Type::ApplicationIcon};
    } else {
        return {};
    }
}

} // namespace tt
