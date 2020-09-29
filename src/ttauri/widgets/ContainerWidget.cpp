
#include "ContainerWidget.hpp"
#include "../GUI/DrawContext.hpp"

namespace tt {

Widget &ContainerWidget::addWidget(std::unique_ptr<Widget> childWidget) noexcept
{
    ttlet lock = std::scoped_lock(mutex);

    children.push_back(std::move(childWidget));
    requestConstraint = true;
    window.requestLayout = true;

    ttlet widget_ptr = children.back().get();
    tt_assume(widget_ptr);
    return *widget_ptr;
}

[[nodiscard]] bool ContainerWidget::updateConstraints() noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    auto has_constrainted = Widget::updateConstraints();

    for (auto &&child : children) {
        ttlet child_lock = std::scoped_lock(child->mutex);
        has_constrainted |= child->updateConstraints();
    }

    return has_constrainted;
}

bool ContainerWidget::updateLayout(hires_utc_clock::time_point display_time_point, bool need_layout) noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    auto need_redraw = need_layout |= requestLayout.exchange(false);
    for (auto &&child : children) {
        ttlet child_lock = std::scoped_lock(child->mutex);
        need_redraw |= child->updateLayout(display_time_point, need_layout);
    }

    return Widget::updateLayout(display_time_point, need_layout) || need_redraw;
}

void ContainerWidget::draw(DrawContext const &drawContext, hires_utc_clock::time_point displayTimePoint) noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    auto childContext = drawContext;
    for (auto &child : children) {
        ttlet child_lock = std::scoped_lock(child->mutex);

        childContext.clippingRectangle = child->clipping_rectangle();
        childContext.transform = child->toWindowTransform;

        // The default fill and border colors.
        ttlet childNestingLevel = child->nestingLevel();
        childContext.color = theme->borderColor(childNestingLevel);
        childContext.fillColor = theme->fillColor(childNestingLevel);

        if (*child->enabled) {
            if (child->focus && window.active) {
                childContext.color = theme->accentColor;
            } else if (child->hover) {
                childContext.color = theme->borderColor(childNestingLevel + 1);
            }

            if (child->hover) {
                childContext.fillColor = theme->fillColor(childNestingLevel + 1);
            }

        } else {
            // Disabled, only the outline is shown.
            childContext.color = theme->borderColor(childNestingLevel - 1);
            childContext.fillColor = theme->fillColor(childNestingLevel - 1);
        }

        child->draw(childContext, displayTimePoint);
    }

    Widget::draw(drawContext, displayTimePoint);
}

HitBox ContainerWidget::hitBoxTest(vec position) const noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    auto r = rectangle().contains(position) ? HitBox{this, elevation} : HitBox{};

    for (ttlet &child : children) {
        ttlet child_lock = std::scoped_lock(child->mutex);
        r = std::max(r, child->hitBoxTest(position - child->offsetFromParent));
    }
    return r;
}

std::vector<Widget *> ContainerWidget::childPointers(bool reverse) const noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    std::vector<Widget *> r;
    r.reserve(std::ssize(children));
    for (ttlet &child : children) {
        r.push_back(child.get());
    }
    if (reverse) {
        std::reverse(r.begin(), r.end());
    }
    return r;
}

Widget *ContainerWidget::nextKeyboardWidget(Widget const *currentKeyboardWidget, bool reverse) const noexcept
{
    tt_assume(mutex.is_locked_by_current_thread());

    if (currentKeyboardWidget == nullptr && acceptsFocus()) {
        // The first widget that accepts focus.
        return const_cast<ContainerWidget *>(this);

    } else {
        bool found = false;

        for (auto *child : childPointers(reverse)) {
            ttlet child_lock = std::scoped_lock(child->mutex);

            if (found) {
                // Find the first focus accepting widget.
                if (auto *tmp = child->nextKeyboardWidget(nullptr, reverse)) {
                    return tmp;
                }

            } else if (child == currentKeyboardWidget) {
                found = true;

            } else {
                auto *tmp = child->nextKeyboardWidget(currentKeyboardWidget, reverse);
                if (tmp == foundWidgetPtr) {
                    // The current widget was found, but no next widget available in the child.
                    found = true;

                } else if (tmp) {
                    return tmp;
                }
            }
        }
        return found ? foundWidgetPtr : nullptr;
    }
}

} // namespace tt