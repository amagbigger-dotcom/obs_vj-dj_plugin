#include "widget/dj_slider.hpp"

DJSlider::DJSlider(Qt::Orientation orientation, int default_val, QWidget *parent)
    : QSlider(orientation, parent), default_val_(default_val)
{
}

void DJSlider::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        setValue(default_val_);
        emit sliderMoved(default_val_);
        event->accept();
        return;
    }
    QSlider::mouseDoubleClickEvent(event);
}
