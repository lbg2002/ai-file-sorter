#include "AppIconResources.hpp"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRectF>

namespace AppIconResources {

// Original vector-drawn FileSort Guard emblem: layered folders + sorting checkmark.
// No OS icon theme, SVG plugin or external image asset is required.
QIcon build_window_icon()
{
    QIcon icon;
    for (const int size : {16, 32, 64, 128, 256, 512}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size / 100.0, size / 100.0);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#233a67"));
        painter.drawRoundedRect(QRectF(3, 3, 94, 94), 23, 23);
        painter.setBrush(QColor("#456fe0"));
        painter.drawRoundedRect(QRectF(12, 24, 76, 61), 12, 12);
        painter.setBrush(QColor("#99c9ff"));
        painter.drawRoundedRect(QRectF(18, 17, 32, 18), 6, 6);
        painter.setBrush(QColor("#dcecff"));
        painter.drawRoundedRect(QRectF(17, 34, 65, 44), 8, 8);

        QPainterPath check;
        check.moveTo(31, 55);
        check.lineTo(45, 67);
        check.lineTo(70, 43);
        painter.setPen(QPen(QColor("#1c65dd"), 8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(check);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace AppIconResources
