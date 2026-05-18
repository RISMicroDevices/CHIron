#pragma once

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QRect>
#include <QSize>
#include <QString>

namespace CHIron::Gui {

namespace ChannelBadgeStyle {

constexpr int kCellHorizontalMargin = 8;
constexpr int kCellVerticalMargin = 5;
constexpr int kBadgeExtraWidth = 24;
constexpr int kTagExtraWidth = 16;
constexpr int kTagGap = 6;
constexpr int kTagMinimumExtraWidth = 18;
constexpr int kBadgePreferredHeight = 12;
constexpr qreal kBadgeRadius = 8.0;
constexpr qreal kTagRadius = 6.0;
constexpr qreal kMinimumFontPointSize = 6.0;

}  // namespace ChannelBadgeStyle

inline QFont ChannelBadgeScaledFont(const QFont& baseFont, const qreal fontScale)
{
    QFont badgeFont(baseFont);
    if (fontScale > 0.0 && fontScale < 1.0) {
        badgeFont.setPointSizeF(qMax<qreal>(ChannelBadgeStyle::kMinimumFontPointSize,
                                            badgeFont.pointSizeF() * fontScale));
    }
    return badgeFont;
}

inline QRect ChannelBadgeContentRect(const QRect& cellRect,
                                     const int verticalMargin = ChannelBadgeStyle::kCellVerticalMargin)
{
    return cellRect.adjusted(ChannelBadgeStyle::kCellHorizontalMargin,
                             verticalMargin,
                             -ChannelBadgeStyle::kCellHorizontalMargin,
                             -verticalMargin);
}

inline int ChannelBadgeMainWidth(const QFontMetrics& metrics, const QString& text)
{
    return metrics.horizontalAdvance(text) + ChannelBadgeStyle::kBadgeExtraWidth;
}

inline int ChannelBadgeTagWidth(const QFontMetrics& metrics, const QString& tag)
{
    return tag.isEmpty() ? 0 : metrics.horizontalAdvance(tag) + ChannelBadgeStyle::kTagExtraWidth;
}

inline int ChannelBadgeContentWidth(const QFontMetrics& metrics,
                                    const QString& text,
                                    const QString& tag = QString())
{
    return ChannelBadgeMainWidth(metrics, text)
        + (tag.isEmpty() ? 0 : ChannelBadgeStyle::kTagGap)
        + ChannelBadgeTagWidth(metrics, tag);
}

inline int ChannelBadgePreferredCellWidth(const QFontMetrics& metrics,
                                          const QString& text,
                                          const QString& tag = QString())
{
    return ChannelBadgeContentWidth(metrics, text, tag)
        + 2 * ChannelBadgeStyle::kCellHorizontalMargin;
}

inline QSize ChannelBadgeSizeHint(const QSize& base,
                                  const QFontMetrics& metrics,
                                  const QString& text,
                                  const QString& tag = QString())
{
    return QSize(qMax(base.width(), ChannelBadgePreferredCellWidth(metrics, text, tag)),
                 base.height());
}

inline void PaintChannelBadge(QPainter* painter,
                              const QRect& availableRect,
                              const QFont& badgeFont,
                              const QString& text,
                              const QString& tag,
                              const QColor& accent,
                              const bool selected,
                              const bool highlighted,
                              const bool leftAligned)
{
    if (!painter || !accent.isValid() || text.isEmpty() || availableRect.width() <= 0) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setFont(badgeFont);
    const QFontMetrics metrics(badgeFont);

    const int badgeWidth = ChannelBadgeMainWidth(metrics, text);
    const int requestedTagWidth = ChannelBadgeTagWidth(metrics, tag);
    const int gap = tag.isEmpty() ? 0 : ChannelBadgeStyle::kTagGap;
    const int totalWidth = qMin(availableRect.width(), badgeWidth + gap + requestedTagWidth);
    const int left = leftAligned
        ? availableRect.left()
        : availableRect.left() + (availableRect.width() - totalWidth) / 2;

    QRect badgeRect(left,
                    availableRect.top(),
                    qMin(badgeWidth, totalWidth),
                    availableRect.height());
    QRect tagRect;
    if (!tag.isEmpty()) {
        const int remainingWidth = totalWidth - badgeRect.width() - gap;
        if (remainingWidth > metrics.horizontalAdvance(QStringLiteral("SAM"))
                + ChannelBadgeStyle::kTagMinimumExtraWidth) {
            tagRect = QRect(badgeRect.right() + 1 + gap,
                            availableRect.top() + 2,
                            remainingWidth,
                            qMax(12, availableRect.height() - 4));
        }
    }

    QColor fill = accent;
    if (selected) {
        fill = fill.darker(105);
    } else if (highlighted) {
        fill = fill.darker(118);
    }

    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRoundedRect(badgeRect,
                             ChannelBadgeStyle::kBadgeRadius,
                             ChannelBadgeStyle::kBadgeRadius);

    painter->setPen(selected ? QColor(QStringLiteral("#FFFFFF"))
                             : QColor(QStringLiteral("#000000")));
    painter->drawText(badgeRect, Qt::AlignCenter, text);

    if (!tagRect.isNull()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(QStringLiteral("#D5DAE1")));
        painter->drawRoundedRect(tagRect,
                                 ChannelBadgeStyle::kTagRadius,
                                 ChannelBadgeStyle::kTagRadius);
        painter->setPen(QColor(QStringLiteral("#3F4752")));
        painter->drawText(tagRect, Qt::AlignCenter, tag);
    }

    painter->restore();
}

}  // namespace CHIron::Gui
