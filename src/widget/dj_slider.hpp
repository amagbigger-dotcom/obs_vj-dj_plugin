#pragma once

#include <QSlider>
#include <QMouseEvent>

class DJSlider : public QSlider {
    Q_OBJECT
public:
    explicit DJSlider(Qt::Orientation orientation, int default_val = 0, QWidget *parent = nullptr);

    void setDefaultValue(int val) { default_val_ = val; }
    int defaultValue() const { return default_val_; }

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    int default_val_ = 0;
};
