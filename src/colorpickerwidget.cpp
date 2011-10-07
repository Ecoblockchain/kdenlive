/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "colorpickerwidget.h"

#include <QMouseEvent>
#include <QHBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QSpinBox>
#include <QDesktopWidget>
#include <QFrame>

#include <KApplication>
#include <KIcon>
#include <KLocalizedString>

#ifdef Q_WS_X11
#include <X11/Xutil.h>
#include <fixx11h.h>

class KCDPickerFilter: public QWidget
{
public:
    KCDPickerFilter(QWidget* parent): QWidget(parent) {}

    virtual bool x11Event(XEvent* event) {
        if (event->type == ButtonRelease) {
            QMouseEvent e(QEvent::MouseButtonRelease, QPoint(),
            QPoint(event->xmotion.x_root, event->xmotion.y_root) , Qt::NoButton, Qt::NoButton, Qt::NoModifier);
            QApplication::sendEvent(parentWidget(), &e);
            return true;
        }
        return false;
    }
};
#endif 


ColorPickerWidget::ColorPickerWidget(QWidget *parent) :
        QWidget(parent),
        m_filterActive(false)
{
#ifdef Q_WS_X11
    m_filter = 0;
    m_image = NULL;
#endif

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    QToolButton *button = new QToolButton(this);
    button->setIcon(KIcon("color-picker"));
    // TODO: make translatable after 0.8.2 release
    button->setToolTip(i18n("Pick a color on the screen") + QString("\nBy pressing the mouse button and then moving your mouse you can select a section of the screen from which to get an average color."));
    button->setAutoRaise(true);
    connect(button, SIGNAL(clicked()), this, SLOT(slotSetupEventFilter()));

    layout->addWidget(button);

    m_grabRectFrame = new QFrame();
    m_grabRectFrame->setFrameStyle(QFrame::Box | QFrame::Plain);
    m_grabRectFrame->setWindowOpacity(0.5);
    m_grabRectFrame->setWindowFlags(Qt::FramelessWindowHint);
    m_grabRectFrame->hide();
}

ColorPickerWidget::~ColorPickerWidget()
{
    delete m_grabRectFrame;
#ifdef Q_WS_X11
    if (m_filterActive && kapp)
        kapp->removeX11EventFilter(m_filter);
#endif
}

void ColorPickerWidget::slotGetAverageColor()
{
    m_grabRect = m_grabRect.normalized();

    int numPixel = m_grabRect.width() * m_grabRect.height();

    int sumR = 0;
    int sumG = 0;
    int sumB = 0;

    // only show message for larger rects because of the overhead displayMessage creates
    if (numPixel > 40000)
        emit displayMessage(i18n("Requesting color information..."), 0);

    /*
     Only getting the image once for the whole rect
     results in a vast speed improvement.
    */
#ifdef Q_WS_X11
    Window root = RootWindow(QX11Info::display(), QX11Info::appScreen());
    m_image = XGetImage(QX11Info::display(), root, m_grabRect.x(), m_grabRect.y(), m_grabRect.width(), m_grabRect.height(), -1, ZPixmap);
#else
    QWidget *desktop = QApplication::desktop();
    m_image = QPixmap::grabWindow(desktop->winId(), m_grabRect.x(), m_grabRect.y(), m_grabRect.width(), m_grabRect.height()).toImage();
#endif

    for (int x = 0; x < m_grabRect.width(); ++x) {
        for (int y = 0; y < m_grabRect.height(); ++y) {
            QColor color = grabColor(QPoint(x, y), false);
            sumR += color.red();
            sumG += color.green();
            sumB += color.blue();
        }

        // Warning: slows things down, so don't do it for every pixel (the inner for loop)
        if (numPixel > 40000)
            emit displayMessage(i18n("Requesting color information..."), (int)(x * m_grabRect.height() / (qreal)numPixel * 100));
    }

#ifdef Q_WS_X11
    XDestroyImage(m_image);
    m_image = NULL;
#endif

    if (numPixel > 40000)
        emit displayMessage(i18n("Calculated average color for rectangle."), -1);

    emit colorPicked(QColor(sumR / numPixel, sumG / numPixel, sumB / numPixel));
}

void ColorPickerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        closeEventFilter();
        event->accept();
        return;
    }

    if (m_filterActive) {
        m_grabRect = QRect(event->globalPos(), QSize(0, 0));
        m_grabRectFrame->setGeometry(m_grabRect);
        m_grabRectFrame->show();
    }

    QWidget::mousePressEvent(event);
}

void ColorPickerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_filterActive) {
        m_grabRectFrame->hide();

        closeEventFilter();

        m_grabRect.setWidth(event->globalX() - m_grabRect.x());
        m_grabRect.setHeight(event->globalY() - m_grabRect.y());

        if (m_grabRect.width() * m_grabRect.height() == 0) {
            emit colorPicked(grabColor(event->globalPos()));
        } else {
            // delay because m_grabRectFrame does not hide immediately
            QTimer::singleShot(50, this, SLOT(slotGetAverageColor()));
        }

        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ColorPickerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_filterActive) {
        m_grabRect.setWidth(event->globalX() - m_grabRect.x());
        m_grabRect.setHeight(event->globalY() - m_grabRect.y());
        m_grabRectFrame->setGeometry(m_grabRect.normalized());
    }
    QWidget::mouseMoveEvent(event);
}

void ColorPickerWidget::keyPressEvent(QKeyEvent *event)
{
    if (m_filterActive) {
        // "special keys" (non letter, numeral) do not work, so close for every key
        //if (event->key() == Qt::Key_Escape)
        closeEventFilter();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void ColorPickerWidget::slotSetupEventFilter()
{
    m_filterActive = true;
#ifdef Q_WS_X11
    m_filter = new KCDPickerFilter(this);
    kapp->installX11EventFilter(m_filter);
#endif
    grabMouse(QCursor(KIcon("color-picker").pixmap(22, 22), 0, 21));
    grabKeyboard();
}

void ColorPickerWidget::closeEventFilter()
{
    m_filterActive = false;
#ifdef Q_WS_X11
    kapp->removeX11EventFilter(m_filter);
    delete m_filter;
    m_filter = 0;
#endif
    releaseMouse();
    releaseKeyboard();
}

QColor ColorPickerWidget::grabColor(const QPoint &p, bool destroyImage)
{
#ifdef Q_WS_X11
    /*
     we use the X11 API directly in this case as we are not getting back a valid
     return from QPixmap::grabWindow in the case where the application is using
     an argb visual
    */
    if( !qApp->desktop()->geometry().contains( p ))
        return QColor();
    unsigned long xpixel;
    if (m_image == NULL) {
        Window root = RootWindow(QX11Info::display(), QX11Info::appScreen());
        m_image = XGetImage(QX11Info::display(), root, p.x(), p.y(), 1, 1, -1, ZPixmap);
        xpixel = XGetPixel(m_image, 0, 0);
    } else {
        xpixel = XGetPixel(m_image, p.x(), p.y());
    }
    if (destroyImage) {
        XDestroyImage(m_image);
        m_image = 0;
    }
    XColor xcol;
    xcol.pixel = xpixel;
    xcol.flags = DoRed | DoGreen | DoBlue;
    XQueryColor(QX11Info::display(),
                DefaultColormap(QX11Info::display(), QX11Info::appScreen()),
                &xcol);
    return QColor::fromRgbF(xcol.red / 65535.0, xcol.green / 65535.0, xcol.blue / 65535.0);
#else
    if (m_image.isNull()) {
        QWidget *desktop = QApplication::desktop();
        QPixmap pm = QPixmap::grabWindow(desktop->winId(), p.x(), p.y(), 1, 1);
        QImage i = pm.toImage();
        return i.pixel(0, 0);
    } else {
        return m_image.pixel(p.x(), p.y());
    }
#endif
}

#include "colorpickerwidget.moc"
