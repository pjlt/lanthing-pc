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
