#include "progress_widget.h"

#include <QtCore/qpropertyanimation.h>
#include <QtGui/qpainter.h>
#include <QtGui/qpainterpath.h>

namespace qt_componets {

ProgressWidgetDelegate::ProgressWidgetDelegate(ProgressWidget* parent)
    : QObject(parent)
    , progress_(parent)
    , offset_(0) {}

void ProgressWidgetDelegate::setOffset(qreal offset) {
    offset_ = offset;
    progress_->update();
}

void ProgressWidgetPrivate::init() {
    Q_Q(ProgressWidget);

    QPropertyAnimation* animation;
    delegate = new ProgressWidgetDelegate(q);
    animation = new QPropertyAnimation(q);
    animation->setPropertyName("offset");
    animation->setTargetObject(delegate);
    animation->setStartValue(0);
    animation->setEndValue(1);
    animation->setDuration(1000);

    animation->setLoopCount(-1);

    animation->start();
}

ProgressWidget::ProgressWidget(QWidget* parent)
    : QProgressBar(parent)
    , d_ptr(new ProgressWidgetPrivate(this)) {
    d_func()->init();
}

void ProgressWidget::setProgressColor(const QColor& color) {
    Q_D(ProgressWidget);

    d->progressColor = color;

    update();
}

QColor ProgressWidget::progressColor() const {
    Q_D(const ProgressWidget);

    if (d->progressColor.isValid()) {
        return d->progressColor;
    }

    return Qt::blue;
}

void ProgressWidget::paintEvent(QPaintEvent* event) {
    (void)event;
    Q_D(ProgressWidget);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QBrush brush;
    brush.setStyle(Qt::SolidPattern);
    // brush.setColor(isEnabled() ? backgroundColor());

    painter.setBrush(brush);
    painter.setPen(Qt::NoPen);

    QPainterPath path;
    path.addRoundedRect(0, height() / 2 - 3, width(), 6, 3, 3);
    painter.setClipPath(path);
    painter.drawRect(0, 0, width(), height());

    if (isEnabled()) {
        brush.setColor(progressColor());
        painter.setBrush(brush);

        painter.drawRect(d->delegate->offset() * width() * 2 - width(), 0, width(), height());
    }
}

} // namespace qt_componets
