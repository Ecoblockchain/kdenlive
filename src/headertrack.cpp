/***************************************************************************
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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


#include "headertrack.h"
#include "kdenlivesettings.h"

#include <KIcon>
#include <KLocale>
#include <KDebug>

#include <QMouseEvent>
#include <QWidget>
#include <QPainter>
#include <QAction>

HeaderTrack::HeaderTrack(int index, TrackInfo info, QWidget *parent)
        : QWidget(parent), m_index(index), m_type(info.type) {
    setFixedHeight(KdenliveSettings::trackheight());
    view.setupUi(this);
    view.track_number->setText(QString::number(m_index));
    view.buttonVideo->setChecked(!info.isBlind);
    view.buttonVideo->setToolTip(i18n("Hide track"));
    view.buttonAudio->setChecked(!info.isMute);
    view.buttonAudio->setToolTip(i18n("Mute track"));
    view.buttonLock->setChecked(info.isLocked);
    view.buttonLock->setToolTip(i18n("Lock track"));

    if (m_type == VIDEOTRACK) {
        view.frame->setBackgroundRole(QPalette::AlternateBase);
        view.frame->setAutoFillBackground(true);
        if (!info.isBlind) view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
        else view.buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    } else {
        view.buttonVideo->setHidden(true);
    }
    if (!info.isMute) view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    else view.buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));

    if (!info.isLocked) view.buttonLock->setIcon(KIcon("kdenlive-unlock"));
    else view.buttonLock->setIcon(KIcon("kdenlive-lock"));

    connect(view.buttonVideo, SIGNAL(clicked()), this, SLOT(switchVideo()));
    connect(view.buttonAudio, SIGNAL(clicked()), this, SLOT(switchAudio()));
    connect(view.buttonLock, SIGNAL(clicked()), this, SLOT(switchLock()));

    m_contextMenu = new QMenu(this);

    QAction *insertAction = new QAction(i18n("Insert Track"), this);
    m_contextMenu->addAction(insertAction);
    connect(insertAction, SIGNAL(triggered()), this, SLOT(slotAddTrack()));

    QAction *removeAction = new QAction(KIcon("edit-delete"), i18n("Delete Track"), this);
    m_contextMenu->addAction(removeAction);
    connect(removeAction, SIGNAL(triggered()), this, SLOT(slotDeleteTrack()));

    QAction *changeAction = new QAction(i18n("Change Track Type"), this);
    m_contextMenu->addAction(changeAction);
    connect(changeAction, SIGNAL(triggered()), this, SLOT(slotChangeTrack()));
}

HeaderTrack::~HeaderTrack() {
    if (m_contextMenu) delete m_contextMenu;
}

void HeaderTrack::switchVideo() {
    if (view.buttonVideo->isChecked()) {
        view.buttonVideo->setIcon(KIcon("kdenlive-show-video"));
    } else {
        view.buttonVideo->setIcon(KIcon("kdenlive-hide-video"));
    }
    emit switchTrackVideo(m_index);
}

void HeaderTrack::switchAudio() {
    if (view.buttonAudio->isChecked()) {
        view.buttonAudio->setIcon(KIcon("kdenlive-show-audio"));
    } else {
        view.buttonAudio->setIcon(KIcon("kdenlive-hide-audio"));
    }
    emit switchTrackAudio(m_index);
}

void HeaderTrack::switchLock(bool emitSignal) {
    if (view.buttonLock->isChecked()) {
        view.buttonLock->setIcon(KIcon("kdenlive-lock"));
    } else {
        view.buttonLock->setIcon(KIcon("kdenlive-unlock"));
    }
    if (emitSignal) emit switchTrackLock(m_index);
}


void HeaderTrack::setLock(bool lock) {
    view.buttonLock->setChecked(lock);
    switchLock(false);
}

void HeaderTrack::slotDeleteTrack() {
    emit deleteTrack(m_index);
}

void HeaderTrack::slotAddTrack() {
    emit insertTrack(m_index);
}

void HeaderTrack::slotChangeTrack() {
    emit changeTrack(m_index);
}

// virtual
void HeaderTrack::contextMenuEvent(QContextMenuEvent * event) {
    m_contextMenu->popup(event->globalPos());
}

// virtual
/*void HeaderTrack::paintEvent(QPaintEvent *e) {
    QRect region = e->rect();
    region.setTopLeft(QPoint(region.left() + 1, region.top() + 1));
    region.setBottomRight(QPoint(region.right() - 1, region.bottom() - 1));
    QPainter painter(this);
    if (m_type == AUDIOTRACK) painter.fillRect(region, QBrush(QColor(240, 240, 255)));
    else painter.fillRect(region, QBrush(QColor(255, 255, 255)));
    painter.drawText(region, Qt::AlignCenter, m_label);
}*/


#include "headertrack.moc"
