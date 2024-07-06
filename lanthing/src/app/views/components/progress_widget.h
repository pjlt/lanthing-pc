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

#pragma once

#include <QtWidgets/qprogressbar.h>

namespace qt_componets {

class ProgressWidget;
class ProgressWidgetDelegate;
class ProgressWidgetPrivate {
    Q_DISABLE_COPY(ProgressWidgetPrivate)
    Q_DECLARE_PUBLIC(ProgressWidget)

public:
    ProgressWidgetPrivate(ProgressWidget* q)
        : q_ptr(q) {}

    void init();

    ProgressWidget* const q_ptr;
    ProgressWidgetDelegate* delegate;
    QColor progressColor;
};

class ProgressWidgetDelegate : public QObject {
    Q_OBJECT

    Q_PROPERTY(qreal offset WRITE setOffset READ offset)

public:
    ProgressWidgetDelegate(ProgressWidget* parent);

    void setOffset(qreal offset);
    qreal offset() const { return offset_; }

private:
    Q_DISABLE_COPY(ProgressWidgetDelegate)

    ProgressWidget* const progress_;
    qreal offset_;
};

class ProgressWidget : public QProgressBar {
    Q_OBJECT

    Q_PROPERTY(QColor progressColor WRITE setProgressColor READ progressColor)

public:
    ProgressWidget(QWidget* parent = 0);

    QColor progressColor() const;
    void setProgressColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

private:
    const QScopedPointer<ProgressWidgetPrivate> d_ptr;
    Q_DECLARE_PRIVATE(ProgressWidget)
};
} // namespace qt_componets
