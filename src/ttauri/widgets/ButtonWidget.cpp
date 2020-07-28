// Copyright 2019 Pokitec
// All rights reserved.

#include "ButtonWidget.hpp"
#include "../GUI/utils.hpp"
#include "../utils.hpp"
#include <cmath>
#include <typeinfo>

namespace tt {

using namespace std::literals;

ButtonWidget::ButtonWidget(Window &window, Widget *parent, std::string const label) noexcept :
    Widget(window, parent, Theme::smallSize, Theme::smallSize),
    label(std::move(label))
{
}

ButtonWidget::~ButtonWidget() {
}

bool ButtonWidget::layout(hires_utc_clock::time_point displayTimePoint, bool forceLayout) noexcept 
{
    if (!Widget::layout(displayTimePoint, forceLayout)) {
        return false;
    }

    ttlet lock = std::scoped_lock(mutex);

    ttlet label_x = Theme::margin;
    ttlet label_y = 0.0;
    ttlet label_width = rectangle().width() - Theme::margin * 2.0f;
    ttlet label_height = rectangle().height();
    ttlet label_rectangle = aarect{label_x, label_y, label_width, label_height};

    labelShapedText = ShapedText(label, theme->warningLabelStyle, label_width + 1.0f, Alignment::MiddleCenter);
    textTranslate = labelShapedText.T(label_rectangle);

    setMinimumExtent(vec{Theme::width, labelShapedText.boundingBox.height() + Theme::margin * 2.0f});

    setPreferredExtent(labelShapedText.preferredExtent + Theme::margin2D * 2.0f);
    return true;
}

void ButtonWidget::draw(DrawContext const &drawContext, hires_utc_clock::time_point displayTimePoint) noexcept
{
    auto context = drawContext;

    context.cornerShapes = vec{Theme::roundingRadius};
    if (value) {
        context.fillColor = theme->accentColor;
    }

    // Move the border of the button in the middle of a pixel.
    context.transform = drawContext.transform;
    context.drawBoxIncludeBorder(rectangle());

    if (*enabled) {
        context.color = theme->foregroundColor;
    }
    context.transform = drawContext.transform * (mat::T{0.0f, 0.0f, 0.001f} * textTranslate);
    context.drawText(labelShapedText, true);

    Widget::draw(drawContext, displayTimePoint);
}

void ButtonWidget::handleCommand(command command) noexcept {
    if (!*enabled) {
        return;
    }

    if (command == command::gui_activate) {
        if (assign_and_compare(value, !value)) {
            window.requestRedraw = true;
        }
    }
    Widget::handleCommand(command);
}

void ButtonWidget::handleMouseEvent(MouseEvent const &event) noexcept {
    Widget::handleMouseEvent(event);

    if (*enabled) {
        if (assign_and_compare(pressed, static_cast<bool>(event.down.leftButton))) {
            window.requestRedraw = true;
        }

        if (
            event.type == MouseEvent::Type::ButtonUp &&
            event.cause.leftButton &&
            rectangle().contains(event.position)
        ) {
            handleCommand(command::gui_activate);
        }
    }
}

HitBox ButtonWidget::hitBoxTest(vec position) const noexcept
{
    if (rectangle().contains(position)) {
        return HitBox{this, elevation, *enabled ? HitBox::Type::Button : HitBox::Type::Default};
    } else {
        return HitBox{};
    }
}

}
