#ifndef ICONHELPER_H
#define ICONHELPER_H

#include <QIcon>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QMap>
#include <QPair>
#include "SvgIcons.h"

class IconHelper {
public:
    static QIcon getIcon(const QString& name, const QString& color = "#cccccc", int size = 64) {
        QString key = QString("%1_%2_%3").arg(name, color, QString::number(size));
        if (m_iconCache.contains(key)) return m_iconCache[key];

        if (!SvgIcons::icons.contains(name)) return QIcon();

        QString svgData = SvgIcons::icons[name];
        svgData.replace("currentColor", color);

        QByteArray bytes = svgData.toUtf8();
        QSvgRenderer renderer(bytes);
        
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        renderer.render(&painter);
        
        QIcon icon;
        icon.addPixmap(pixmap, QIcon::Normal, QIcon::On);
        icon.addPixmap(pixmap, QIcon::Normal, QIcon::Off);
        icon.addPixmap(pixmap, QIcon::Active, QIcon::On);
        icon.addPixmap(pixmap, QIcon::Active, QIcon::Off);
        icon.addPixmap(pixmap, QIcon::Selected, QIcon::On);
        icon.addPixmap(pixmap, QIcon::Selected, QIcon::Off);

        m_iconCache[key] = icon;
        return icon;
    }

private:
    static inline QMap<QString, QIcon> m_iconCache;
};

#endif // ICONHELPER_H
