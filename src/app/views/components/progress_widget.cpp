/*
 * BSD 3-Clause License
 * 
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
