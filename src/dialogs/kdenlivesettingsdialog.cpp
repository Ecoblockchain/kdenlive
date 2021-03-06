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

#include "kdenlivesettingsdialog.h"
#include "profilesdialog.h"
#include "encodingprofilesdialog.h"
#include "project/dialogs/profilewidget.h"
#include "utils/KoIconUtils.h"
#include "dialogs/profilesdialog.h"
#include "kdenlivesettings.h"
#include "renderer.h"

#ifdef USE_V4L
#include "capture/v4lcapture.h"
#endif

#include "klocalizedstring.h"
#include <KMessageBox>
#include <KLineEdit>
#include <KService>
#include <KRun>
#include <KOpenWithDialog>
#include <QDebug>
#include <QDir>
#include <QTimer>
#include <QThread>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef USE_JOGSHUTTLE
  #include "jogshuttle/jogaction.h"
  #include "jogshuttle/jogshuttleconfig.h"
  #include <linux/input.h>
#include <QStandardPaths>
#endif


KdenliveSettingsDialog::KdenliveSettingsDialog(const QMap<QString, QString>& mappable_actions, bool gpuAllowed, QWidget * parent) :
    KConfigDialog(parent, QStringLiteral("settings"), KdenliveSettings::self()),
    m_modified(false),
    m_shuttleModified(false),
    m_mappable_actions(mappable_actions)
{
    KdenliveSettings::setV4l_format(0);
    QWidget *p1 = new QWidget;
    m_configMisc.setupUi(p1);
    m_page1 = addPage(p1, i18n("Misc"));
    m_page1->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));

    // Hide avformat-novalidate trick, causes crash (bug #2205 and #2206)
    m_configMisc.kcfg_projectloading_avformatnovalidate->setVisible(false);

    m_configMisc.kcfg_use_exiftool->setEnabled(!QStandardPaths::findExecutable(QStringLiteral("exiftool")).isEmpty());

    QWidget *p8 = new QWidget;
    m_configProject.setupUi(p8);
    m_page8 = addPage(p8, i18n("Project Defaults"));
    QVBoxLayout *vbox = new QVBoxLayout;
    m_pw = new ProfileWidget(this);
    vbox->addWidget(m_pw);
    m_configProject.profile_box->setLayout(vbox);
    // Select profile
    m_pw->loadProfile(KdenliveSettings::default_profile().isEmpty() ? KdenliveSettings::current_profile() : KdenliveSettings::default_profile());
    m_page8->setIcon(KoIconUtils::themedIcon(QStringLiteral("project-defaults")));
    connect(m_configProject.kcfg_generateproxy, SIGNAL(toggled(bool)), m_configProject.kcfg_proxyminsize, SLOT(setEnabled(bool)));
    m_configProject.kcfg_proxyminsize->setEnabled(KdenliveSettings::generateproxy());
    connect(m_configProject.kcfg_generateimageproxy, SIGNAL(toggled(bool)), m_configProject.kcfg_proxyimageminsize, SLOT(setEnabled(bool)));
    m_configProject.kcfg_proxyimageminsize->setEnabled(KdenliveSettings::generateimageproxy());

    QWidget *p3 = new QWidget;
    m_configTimeline.setupUi(p3);
    m_page3 = addPage(p3, i18n("Timeline"));
    m_page3->setIcon(KoIconUtils::themedIcon(QStringLiteral("video-display")));

    QWidget *p2 = new QWidget;
    m_configEnv.setupUi(p2);
    m_configEnv.mltpathurl->setMode(KFile::Directory);
    m_configEnv.mltpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_mltpath"));
    m_configEnv.rendererpathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_rendererpath"));
    m_configEnv.ffmpegurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffmpegpath"));
    m_configEnv.ffplayurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffplaypath"));
    m_configEnv.ffprobeurl->lineEdit()->setObjectName(QStringLiteral("kcfg_ffprobepath"));
    int maxThreads = QThread::idealThreadCount();
    m_configEnv.kcfg_mltthreads->setMaximum(maxThreads > 2 ? maxThreads : 8);
    m_configEnv.tmppathurl->setMode(KFile::Directory);
    m_configEnv.tmppathurl->lineEdit()->setObjectName(QStringLiteral("kcfg_currenttmpfolder"));
    m_configEnv.projecturl->setMode(KFile::Directory);
    m_configEnv.projecturl->lineEdit()->setObjectName(QStringLiteral("kcfg_defaultprojectfolder"));
    m_configEnv.capturefolderurl->setMode(KFile::Directory);
    m_configEnv.capturefolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_capturefolder"));
    m_configEnv.capturefolderurl->setEnabled(!KdenliveSettings::capturetoprojectfolder());
    connect(m_configEnv.kcfg_capturetoprojectfolder, SIGNAL(clicked()), this, SLOT(slotEnableCaptureFolder()));
    // Library folder
    m_configEnv.libraryfolderurl->setMode(KFile::Directory);
    m_configEnv.libraryfolderurl->lineEdit()->setObjectName(QStringLiteral("kcfg_libraryfolder"));
    m_configEnv.libraryfolderurl->setEnabled(!KdenliveSettings::librarytodefaultfolder());
    m_configEnv.kcfg_librarytodefaultfolder->setToolTip(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + QStringLiteral("/library"));
    connect(m_configEnv.kcfg_librarytodefaultfolder, SIGNAL(clicked()), this, SLOT(slotEnableLibraryFolder()));

    m_page2 = addPage(p2, i18n("Environment"));
    m_page2->setIcon(KoIconUtils::themedIcon(QStringLiteral("application-x-executable-script")));

    QWidget *p4 = new QWidget;
    m_configCapture.setupUi(p4);
    m_configCapture.tabWidget->removeTab(0);
    m_configCapture.tabWidget->removeTab(2);
#ifdef USE_V4L

    // Video 4 Linux device detection
    for (int i = 0; i < 10; ++i) {
        QString path = QLatin1String("/dev/video") + QString::number(i);
        if (QFile::exists(path)) {
            QStringList deviceInfo = V4lCaptureHandler::getDeviceName(path);
            if (!deviceInfo.isEmpty()) {
                m_configCapture.kcfg_detectedv4ldevices->addItem(deviceInfo.at(0), path);
                m_configCapture.kcfg_detectedv4ldevices->setItemData(m_configCapture.kcfg_detectedv4ldevices->count() - 1, deviceInfo.at(1), Qt::UserRole + 1);
            }
        }
    }
    connect(m_configCapture.kcfg_detectedv4ldevices, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatev4lDevice()));
    connect(m_configCapture.kcfg_v4l_format, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatev4lCaptureProfile()));
    connect(m_configCapture.config_v4l, SIGNAL(clicked()), this, SLOT(slotEditVideo4LinuxProfile()));

    slotUpdatev4lDevice();
#endif

    m_page4 = addPage(p4, i18n("Capture"));
    m_page4->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-record")));
    m_configCapture.tabWidget->setCurrentIndex(KdenliveSettings::defaultcapture());
#ifdef Q_WS_MAC
    m_configCapture.tabWidget->setEnabled(false);
    m_configCapture.kcfg_defaultcapture->setEnabled(false);
    m_configCapture.label->setText(i18n("Capture is not yet available on Mac OS X."));
#endif

    QWidget *p5 = new QWidget;
    m_configShuttle.setupUi(p5);
#ifdef USE_JOGSHUTTLE
    m_configShuttle.toolBtnReload->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-refresh")));
    connect(m_configShuttle.kcfg_enableshuttle, SIGNAL(stateChanged(int)), this, SLOT(slotCheckShuttle(int)));
    connect(m_configShuttle.shuttledevicelist, SIGNAL(activated(int)), this, SLOT(slotUpdateShuttleDevice(int)));
    connect(m_configShuttle.toolBtnReload, SIGNAL(clicked(bool)), this, SLOT(slotReloadShuttleDevices()));

    slotCheckShuttle(KdenliveSettings::enableshuttle());
    m_configShuttle.shuttledisabled->hide();

    // Store the button pointers into an array for easier handling them in the other functions.
    // TODO: impl enumerator or live with cut and paste :-)))
    setupJogshuttleBtns(KdenliveSettings::shuttledevice());
#if 0
    m_shuttle_buttons.push_back(m_configShuttle.shuttle1);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle2);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle3);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle4);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle5);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle6);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle7);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle8);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle9);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle10);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle11);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle12);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle13);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle14);
    m_shuttle_buttons.push_back(m_configShuttle.shuttle15);
#endif

#else /* ! USE_JOGSHUTTLE */
    m_configShuttle.kcfg_enableshuttle->hide();
    m_configShuttle.kcfg_enableshuttle->setDisabled(true);
#endif /* USE_JOGSHUTTLE */
    m_page5 = addPage(p5, i18n("JogShuttle"));
    m_page5->setIcon(KoIconUtils::themedIcon(QStringLiteral("jog-dial")));

    QWidget *p6 = new QWidget;
    m_configSdl.setupUi(p6);
    m_configSdl.reload_blackmagic->setIcon(KoIconUtils::themedIcon(QStringLiteral("view-refresh")));
    connect(m_configSdl.reload_blackmagic, SIGNAL(clicked(bool)), this, SLOT(slotReloadBlackMagic()));

    //m_configSdl.kcfg_openglmonitors->setHidden(true);


    m_page6 = addPage(p6, i18n("Playback"));
    m_page6->setIcon(KoIconUtils::themedIcon(QStringLiteral("media-playback-start")));

    QWidget *p7 = new QWidget;
    m_configTranscode.setupUi(p7);
    m_page7 = addPage(p7, i18n("Transcode"));
    m_page7->setIcon(KoIconUtils::themedIcon(QStringLiteral("edit-copy")));

    connect(m_configTranscode.button_add, SIGNAL(clicked()), this, SLOT(slotAddTranscode()));
    connect(m_configTranscode.button_delete, SIGNAL(clicked()), this, SLOT(slotDeleteTranscode()));
    connect(m_configTranscode.profiles_list, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(slotDialogModified()));
    connect(m_configTranscode.profiles_list, SIGNAL(currentRowChanged(int)), this, SLOT(slotSetTranscodeProfile()));
    
    connect(m_configTranscode.profile_name, SIGNAL(textChanged(QString)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_description, SIGNAL(textChanged(QString)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_extension, SIGNAL(textChanged(QString)), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_parameters, SIGNAL(textChanged()), this, SLOT(slotEnableTranscodeUpdate()));
    connect(m_configTranscode.profile_audioonly, SIGNAL(stateChanged(int)), this, SLOT(slotEnableTranscodeUpdate()));
    
    connect(m_configTranscode.button_update, SIGNAL(pressed()), this, SLOT(slotUpdateTranscodingProfile()));
    
    m_configTranscode.profile_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 5);

    connect(m_configEnv.kp_image, SIGNAL(clicked()), this, SLOT(slotEditImageApplication()));
    connect(m_configEnv.kp_audio, SIGNAL(clicked()), this, SLOT(slotEditAudioApplication()));

    loadEncodingProfiles();

    connect(m_configSdl.kcfg_audio_driver, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCheckAlsaDriver()));
    connect(m_configSdl.kcfg_audio_backend, SIGNAL(currentIndexChanged(int)), this, SLOT(slotCheckAudioBackend()));
    initDevices();
    connect(m_configCapture.kcfg_grab_capture_type, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateGrabRegionStatus()));

    slotUpdateGrabRegionStatus();
    loadTranscodeProfiles();


    //HACK: check dvgrab version, because only dvgrab >= 3.3 supports
    //   --timestamp option without bug

    if (KdenliveSettings::dvgrab_path().isEmpty() || !QFile::exists(KdenliveSettings::dvgrab_path())) {
        QString dvgrabpath = QStandardPaths::findExecutable(QStringLiteral("dvgrab"));
        KdenliveSettings::setDvgrab_path(dvgrabpath);
    }

    // decklink profile
    QAction *act = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(4);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(slotManageEncodingProfile()));
    m_configCapture.decklink_manageprofile->setDefaultAction(act);
    m_configCapture.decklink_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    m_configCapture.decklink_parameters->setVisible(false);
    m_configCapture.decklink_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.decklink_parameters->setPlainText(KdenliveSettings::decklink_parameters());
    connect(m_configCapture.kcfg_decklink_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateDecklinkProfile()));
    connect(m_configCapture.decklink_showprofileinfo, SIGNAL(clicked(bool)), m_configCapture.decklink_parameters, SLOT(setVisible(bool)));

    // ffmpeg profile
    m_configCapture.v4l_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    m_configCapture.v4l_parameters->setVisible(false);
    m_configCapture.v4l_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.v4l_parameters->setPlainText(KdenliveSettings::v4l_parameters());

    act = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(2);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(slotManageEncodingProfile()));
    m_configCapture.v4l_manageprofile->setDefaultAction(act);
    connect(m_configCapture.kcfg_v4l_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateV4lProfile()));
    connect(m_configCapture.v4l_showprofileinfo, SIGNAL(clicked(bool)), m_configCapture.v4l_parameters, SLOT(setVisible(bool)));

    // screen grab profile
    m_configCapture.grab_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    m_configCapture.grab_parameters->setVisible(false);
    m_configCapture.grab_parameters->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 4);
    m_configCapture.grab_parameters->setPlainText(KdenliveSettings::grab_parameters());
    act = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(3);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(slotManageEncodingProfile()));
    m_configCapture.grab_manageprofile->setDefaultAction(act);
    connect(m_configCapture.kcfg_grab_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateGrabProfile()));
    connect(m_configCapture.grab_showprofileinfo, SIGNAL(clicked(bool)), m_configCapture.grab_parameters, SLOT(setVisible(bool)));

    // Timeline preview
    act = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(1);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(slotManageEncodingProfile()));
    m_configProject.preview_manageprofile->setDefaultAction(act);
    connect(m_configProject.kcfg_preview_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdatePreviewProfile()));
    connect(m_configProject.preview_showprofileinfo, SIGNAL(clicked(bool)), m_configProject.previewparams, SLOT(setVisible(bool)));
    m_configProject.previewparams->setVisible(false);
    m_configProject.previewparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 3);
    m_configProject.previewparams->setPlainText(KdenliveSettings::previewparams());
    m_configProject.preview_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    m_configProject.preview_showprofileinfo->setToolTip(i18n("Show default timeline preview parameters"));
    m_configProject.preview_manageprofile->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    m_configProject.preview_manageprofile->setToolTip(i18n("Manage timeline preview profiles"));
    m_configProject.kcfg_preview_profile->setToolTip(i18n("Select default timeline preview profile"));

    // proxy profile stuff
    m_configProject.proxy_showprofileinfo->setIcon(KoIconUtils::themedIcon(QStringLiteral("help-about")));
    m_configProject.proxy_showprofileinfo->setToolTip(i18n("Show default profile parameters"));
    m_configProject.proxy_manageprofile->setIcon(KoIconUtils::themedIcon(QStringLiteral("configure")));
    m_configProject.proxy_manageprofile->setToolTip(i18n("Manage proxy profiles"));
    m_configProject.kcfg_proxy_profile->setToolTip(i18n("Select default proxy profile"));
    m_configProject.proxyparams->setVisible(false);
    m_configProject.proxyparams->setMaximumHeight(QFontMetrics(font()).lineSpacing() * 3);
    m_configProject.proxyparams->setPlainText(KdenliveSettings::proxyparams());

    act = new QAction(KoIconUtils::themedIcon(QStringLiteral("configure")), i18n("Configure profiles"), this);
    act->setData(0);
    connect(act, SIGNAL(triggered(bool)), this, SLOT(slotManageEncodingProfile()));
    m_configProject.proxy_manageprofile->setDefaultAction(act);

    connect(m_configProject.proxy_showprofileinfo, SIGNAL(clicked(bool)), m_configProject.proxyparams, SLOT(setVisible(bool)));
    connect(m_configProject.kcfg_proxy_profile, SIGNAL(currentIndexChanged(int)), this, SLOT(slotUpdateProxyProfile()));


    slotUpdateProxyProfile(-1);
    slotUpdateV4lProfile(-1);
    slotUpdateGrabProfile(-1);
    slotUpdateDecklinkProfile(-1);

    // enable GPU accel only if Movit is found
    m_configSdl.kcfg_gpu_accel->setEnabled(gpuAllowed);
    m_configSdl.kcfg_gpu_accel->setToolTip(i18n("GPU processing needs MLT compiled with Movit and Rtaudio modules"));

    Render::getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice);
    if (!Render::getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device)) {
        // No blackmagic card found
        m_configSdl.kcfg_external_display->setEnabled(false);
    }

    if (!KdenliveSettings::dvgrab_path().isEmpty()) {
        double dvgrabVersion = 0;
        QProcess *versionCheck = new QProcess;
        versionCheck->setProcessChannelMode(QProcess::MergedChannels);
        versionCheck->start(QStringLiteral("dvgrab"), QStringList() << QStringLiteral("--version"));
        if (versionCheck->waitForFinished()) {
            QString version = QString(versionCheck->readAll()).simplified();
            if (version.contains(' ')) version = version.section(' ', -1);
            dvgrabVersion = version.toDouble();

            //qDebug() << "// FOUND DVGRAB VERSION: " << dvgrabVersion;
        }
        delete versionCheck;
        if (dvgrabVersion < 3.3) {
            KdenliveSettings::setFirewiretimestamp(false);
            m_configCapture.kcfg_firewiretimestamp->setEnabled(false);
        }
        m_configCapture.dvgrab_info->setText(i18n("dvgrab version %1 at %2", dvgrabVersion, KdenliveSettings::dvgrab_path()));
    } else m_configCapture.dvgrab_info->setText(i18n("<strong><em>dvgrab</em> utility not found, please install it for firewire capture</strong>"));
}

void KdenliveSettingsDialog::setupJogshuttleBtns(QString device)
{
    QList<KComboBox*> list;
    QList<QLabel*> list1;

    list << m_configShuttle.shuttle1;
    list << m_configShuttle.shuttle2;
    list << m_configShuttle.shuttle3;
    list << m_configShuttle.shuttle4;
    list << m_configShuttle.shuttle5;
    list << m_configShuttle.shuttle6;
    list << m_configShuttle.shuttle7;
    list << m_configShuttle.shuttle8;
    list << m_configShuttle.shuttle9;
    list << m_configShuttle.shuttle10;
    list << m_configShuttle.shuttle11;
    list << m_configShuttle.shuttle12;
    list << m_configShuttle.shuttle13;
    list << m_configShuttle.shuttle14;
    list << m_configShuttle.shuttle15;

    list1 << m_configShuttle.label_2; // #1
    list1 << m_configShuttle.label_4; // #2
    list1 << m_configShuttle.label_3; // #3
    list1 << m_configShuttle.label_7; // #4
    list1 << m_configShuttle.label_5; // #5
    list1 << m_configShuttle.label_6; // #6
    list1 << m_configShuttle.label_8; // #7
    list1 << m_configShuttle.label_9; // #8
    list1 << m_configShuttle.label_10; // #9
    list1 << m_configShuttle.label_11; // #10
    list1 << m_configShuttle.label_12; // #11
    list1 << m_configShuttle.label_13; // #12
    list1 << m_configShuttle.label_14; // #13
    list1 << m_configShuttle.label_15; // #14
    list1 << m_configShuttle.label_16; // #15


    for (int i = 0; i < list.count(); ++i) {
        list[i]->hide();
        list1[i]->hide();
    }
#ifdef USE_JOGSHUTTLE
    if (!m_configShuttle.kcfg_enableshuttle->isChecked()) return;
    int keysCount = JogShuttle::keysCount(device);

    for (int i = 0; i < keysCount; ++i) {
        m_shuttle_buttons.push_back(list[i]);
        list[i]->show();
        list1[i]->show();
    }

    // populate the buttons with the current configuration. The items are sorted
    // according to the user-selected language, so they do not appear in random order.
    QMap<QString, QString> mappable_actions(m_mappable_actions);
    QList<QString> action_names = mappable_actions.keys();
    QList<QString>::Iterator iter = action_names.begin();
    //qDebug() << "::::::::::::::::";
    while (iter != action_names.end()) {
        //qDebug() << *iter;
        ++iter;
    }

    //qDebug() << "::::::::::::::::";

    qSort(action_names);
    iter = action_names.begin();
    while (iter != action_names.end()) {
        //qDebug() << *iter;
        ++iter;
    }
    //qDebug() << "::::::::::::::::";

    // Here we need to compute the action_id -> index-in-action_names. We iterate over the
    // action_names, as the sorting may depend on the user-language.
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    foreach (const QString& action_id, actions_map) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (mappable_actions[action_names.at(i)] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }

    int i = 0;
    foreach (KComboBox* button, m_shuttle_buttons) {
        button->addItems(action_names);
        connect(button, SIGNAL(activated(int)), this, SLOT(slotShuttleModified()));
        ++i;
        if (i < actions_map.size())
            button->setCurrentIndex(action_pos[actions_map[i]]);
    }
#endif
}

KdenliveSettingsDialog::~KdenliveSettingsDialog() {}

void KdenliveSettingsDialog::slotUpdateGrabRegionStatus()
{
    m_configCapture.region_group->setHidden(m_configCapture.kcfg_grab_capture_type->currentIndex() != 1);
}

void KdenliveSettingsDialog::slotEnableCaptureFolder()
{
    m_configEnv.capturefolderurl->setEnabled(!m_configEnv.kcfg_capturetoprojectfolder->isChecked());
}

void KdenliveSettingsDialog::slotEnableLibraryFolder()
{
    m_configEnv.libraryfolderurl->setEnabled(!m_configEnv.kcfg_librarytodefaultfolder->isChecked());
}

void KdenliveSettingsDialog::initDevices()
{
    // Fill audio drivers
    m_configSdl.kcfg_audio_driver->addItem(i18n("Automatic"), QString());
#ifndef Q_WS_MAC
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS"), "dsp");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ALSA"), "alsa");
    m_configSdl.kcfg_audio_driver->addItem(i18n("PulseAudio"), "pulse");
    m_configSdl.kcfg_audio_driver->addItem(i18n("OSS with DMA access"), "dma");
    m_configSdl.kcfg_audio_driver->addItem(i18n("Esound daemon"), "esd");
    m_configSdl.kcfg_audio_driver->addItem(i18n("ARTS daemon"), "artsc");
#endif

    if (!KdenliveSettings::audiodrivername().isEmpty())
        for (int i = 1; i < m_configSdl.kcfg_audio_driver->count(); ++i) {
            if (m_configSdl.kcfg_audio_driver->itemData(i).toString() == KdenliveSettings::audiodrivername()) {
                m_configSdl.kcfg_audio_driver->setCurrentIndex(i);
                KdenliveSettings::setAudio_driver((uint) i);
            }
        }

    // Fill the list of audio playback / recording devices
    m_configSdl.kcfg_audio_device->addItem(i18n("Default"), QString());
    m_configCapture.kcfg_v4l_alsadevice->addItem(i18n("Default"), "default");
    if (!QStandardPaths::findExecutable(QStringLiteral("aplay")).isEmpty()) {
        m_readProcess.setOutputChannelMode(KProcess::OnlyStdoutChannel);
        m_readProcess.setProgram(QStringLiteral("aplay"), QStringList() << QStringLiteral("-l"));
        connect(&m_readProcess, &KProcess::readyReadStandardOutput, this, &KdenliveSettingsDialog::slotReadAudioDevices);
        m_readProcess.execute(5000);
    } else {
        // If aplay is not installed on the system, parse the /proc/asound/pcm file
        QFile file(QStringLiteral("/proc/asound/pcm"));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            QString line = stream.readLine();
            QString deviceId;
            while (!line.isNull()) {
                if (line.contains(QStringLiteral("playback"))) {
                    deviceId = line.section(':', 0, 0);
                    m_configSdl.kcfg_audio_device->addItem(line.section(':', 1, 1), "plughw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                }
                if (line.contains(QStringLiteral("capture"))) {
                    deviceId = line.section(':', 0, 0);
                    m_configCapture.kcfg_v4l_alsadevice->addItem(line.section(':', 1, 1).simplified(), "hw:" + QString::number(deviceId.section('-', 0, 0).toInt()) + ',' + QString::number(deviceId.section('-', 1, 1).toInt()));
                }
                line = stream.readLine();
            }
            file.close();
        } else qDebug()<<" / / / /CANNOT READ PCM";
    }

    // Add pulseaudio capture option
    m_configCapture.kcfg_v4l_alsadevice->addItem(i18n("PulseAudio"), "pulse");

    if (!KdenliveSettings::audiodevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configSdl.kcfg_audio_device->findData(KdenliveSettings::audiodevicename());
        m_configSdl.kcfg_audio_device->setCurrentIndex(ix);
        KdenliveSettings::setAudio_device(ix);
    }

    if (!KdenliveSettings::v4l_alsadevicename().isEmpty()) {
        // Select correct alsa device
        int ix = m_configCapture.kcfg_v4l_alsadevice->findData(KdenliveSettings::v4l_alsadevicename());
        m_configCapture.kcfg_v4l_alsadevice->setCurrentIndex(ix);
        KdenliveSettings::setV4l_alsadevice(ix);
    }

    m_configSdl.kcfg_audio_backend->addItem(i18n("SDL"), "sdl_audio");
    m_configSdl.kcfg_audio_backend->addItem(i18n("RtAudio"), "rtaudio");

    if (!KdenliveSettings::audiobackend().isEmpty()) {
        int ix = m_configSdl.kcfg_audio_backend->findData(KdenliveSettings::audiobackend());
        m_configSdl.kcfg_audio_backend->setCurrentIndex(ix);
        KdenliveSettings::setAudio_backend(ix);
    }
    m_configSdl.group_sdl->setEnabled(KdenliveSettings::audiobackend() == QLatin1String("sdl_audio"));

    loadCurrentV4lProfileInfo();
}

void KdenliveSettingsDialog::slotReadAudioDevices()
{
    QString result = QString(m_readProcess.readAllStandardOutput());
    //qDebug() << "// / / / / / READING APLAY: ";
    //qDebug() << result;
    QStringList lines = result.split('\n');
    foreach(const QString & data, lines) {
        ////qDebug() << "// READING LINE: " << data;
        if (!data.startsWith(' ') && data.count(':') > 1) {
            QString card = data.section(':', 0, 0).section(' ', -1);
            QString device = data.section(':', 1, 1).section(' ', -1);
            m_configSdl.kcfg_audio_device->addItem(data.section(':', -1).simplified(), "plughw:" + card + ',' + device);
            m_configCapture.kcfg_v4l_alsadevice->addItem(data.section(':', -1).simplified(), "hw:" + card + ',' + device);
        }
    }
}

void KdenliveSettingsDialog::showPage(int page, int option)
{
    switch (page) {
    case 1:
        setCurrentPage(m_page1);
        break;
    case 2:
        setCurrentPage(m_page2);
        break;
    case 3:
        setCurrentPage(m_page3);
        break;
    case 4:
        setCurrentPage(m_page4);
        m_configCapture.tabWidget->setCurrentIndex(option);
        break;
    case 5:
        setCurrentPage(m_page5);
        break;
    case 6:
        setCurrentPage(m_page6);
        break;
    case 7:
        setCurrentPage(m_page7);
        break;
    default:
        setCurrentPage(m_page1);
    }
}

void KdenliveSettingsDialog::slotEditAudioApplication()
{
    KService::Ptr service;
    QPointer<KOpenWithDialog> dlg = new KOpenWithDialog(QList<QUrl>(), i18n("Select default audio editor"), m_configEnv.kcfg_defaultaudioapp->text(), this);
    if (dlg->exec() == QDialog::Accepted) {
        service = dlg->service();
        m_configEnv.kcfg_defaultaudioapp->setText(KRun::binaryName(service->exec(), false));
    }

    delete dlg;
}

void KdenliveSettingsDialog::slotEditImageApplication()
{
    KService::Ptr service;
    QPointer<KOpenWithDialog> dlg = new KOpenWithDialog(QList<QUrl>(), i18n("Select default image editor"), m_configEnv.kcfg_defaultimageapp->text(), this);
    if (dlg->exec() == QDialog::Accepted) {
        service = dlg->service();
        m_configEnv.kcfg_defaultimageapp->setText(KRun::binaryName(service->exec(), false));
    }
    delete dlg;
}

void KdenliveSettingsDialog::slotCheckShuttle(int state)
{
#ifdef USE_JOGSHUTTLE
    m_configShuttle.config_group->setEnabled(state);
    m_configShuttle.shuttledevicelist->clear();

    QStringList devNames = KdenliveSettings::shuttledevicenames();
    QStringList devPaths = KdenliveSettings::shuttledevicepaths();

    if (devNames.count() != devPaths.count()) {
        return;
    }
    for (int i = 0; i < devNames.count(); ++i) {
        m_configShuttle.shuttledevicelist->addItem(
                devNames.at(i), devPaths.at(i));
    }
    if (state) setupJogshuttleBtns(m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString());
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::slotUpdateShuttleDevice(int ix)
{
#ifdef USE_JOGSHUTTLE
    QString device = m_configShuttle.shuttledevicelist->itemData(ix).toString();
    //KdenliveSettings::setShuttledevice(device);
    setupJogshuttleBtns(device);
    m_configShuttle.kcfg_shuttledevice->setText(device);
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::updateWidgets()
{
    // Revert widgets to last saved state (for example when user pressed "Cancel")
    // //qDebug() << "// // // KCONFIG Revert called";
#ifdef USE_JOGSHUTTLE
    // revert jog shuttle device
    if (m_configShuttle.shuttledevicelist->count() > 0) {
        for (int i = 0; i < m_configShuttle.shuttledevicelist->count(); ++i) {
            if (m_configShuttle.shuttledevicelist->itemData(i) == KdenliveSettings::shuttledevice()) {
                m_configShuttle.shuttledevicelist->setCurrentIndex(i);
                break;
            }
        }
    }

    // Revert jog shuttle buttons
    QList<QString> action_names = m_mappable_actions.keys();
    qSort(action_names);
    QStringList actions_map = JogShuttleConfig::actionMap(KdenliveSettings::shuttlebuttons());
    QMap<QString, int> action_pos;
    foreach (const QString& action_id, actions_map) {
        // This loop find out at what index is the string that would map to the action_id.
        for (int i = 0; i < action_names.size(); ++i) {
            if (m_mappable_actions[action_names[i]] == action_id) {
                action_pos[action_id] = i;
                break;
            }
        }
    }
    int i = 0;
    foreach (KComboBox* button, m_shuttle_buttons) {
        ++i;
        if (i < actions_map.size())
            button->setCurrentIndex(action_pos[actions_map[i]]);
    }
#endif /* USE_JOGSHUTTLE */
}

void KdenliveSettingsDialog::updateSettings()
{
    // Save changes to settings (for example when user pressed "Apply" or "Ok")
    // //qDebug() << "// // // KCONFIG UPDATE called";
    KdenliveSettings::setDefault_profile(m_pw->selectedProfile());

    bool resetProfile = false;
    bool updateCapturePath = false;
    bool updateLibrary = false;

    /*if (m_configShuttle.shuttledevicelist->count() > 0) {
    QString device = m_configShuttle.shuttledevicelist->itemData(m_configShuttle.shuttledevicelist->currentIndex()).toString();
    if (device != KdenliveSettings::shuttledevice()) KdenliveSettings::setShuttledevice(device);
    }*/

    // Capture default folder
    if (m_configEnv.kcfg_capturetoprojectfolder->isChecked() != KdenliveSettings::capturetoprojectfolder()) {
        KdenliveSettings::setCapturetoprojectfolder(m_configEnv.kcfg_capturetoprojectfolder->isChecked());
        updateCapturePath = true;
    }

    if (m_configEnv.capturefolderurl->url().path() != KdenliveSettings::capturefolder()) {
        KdenliveSettings::setCapturefolder(m_configEnv.capturefolderurl->url().path());
        updateCapturePath = true;
    }

    // Library default folder
    if (m_configEnv.kcfg_librarytodefaultfolder->isChecked() != KdenliveSettings::librarytodefaultfolder()) {
        KdenliveSettings::setLibrarytodefaultfolder(m_configEnv.kcfg_librarytodefaultfolder->isChecked());
        updateLibrary = true;
    }

    if (m_configEnv.libraryfolderurl->url().path() != KdenliveSettings::libraryfolder()) {
        KdenliveSettings::setLibraryfolder(m_configEnv.libraryfolderurl->url().path());
        if (!KdenliveSettings::librarytodefaultfolder()) {
            updateLibrary = true;
        }
    }

    if (m_configCapture.kcfg_dvgrabfilename->text() != KdenliveSettings::dvgrabfilename()) {
        KdenliveSettings::setDvgrabfilename(m_configCapture.kcfg_dvgrabfilename->text());
        updateCapturePath = true;
    }

    if ((uint) m_configCapture.kcfg_firewireformat->currentIndex() != KdenliveSettings::firewireformat()) {
        KdenliveSettings::setFirewireformat(m_configCapture.kcfg_firewireformat->currentIndex());
        updateCapturePath = true;
    }

    if ((uint) m_configCapture.kcfg_v4l_format->currentIndex() != KdenliveSettings::v4l_format()) {
        saveCurrentV4lProfile();
        KdenliveSettings::setV4l_format(0);
    }

    // Check if screengrab is fullscreen
    if ((uint) m_configCapture.kcfg_grab_capture_type->currentIndex() != KdenliveSettings::grab_capture_type()) {
        KdenliveSettings::setGrab_capture_type(m_configCapture.kcfg_grab_capture_type->currentIndex());
        emit updateFullScreenGrab();
    }

    // Check encoding profiles
    // FFmpeg
    QString data = m_configCapture.kcfg_v4l_profile->itemData(m_configCapture.kcfg_v4l_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(';', 0, 0) != KdenliveSettings::v4l_parameters() || data.section(';', 1, 1) != KdenliveSettings::v4l_extension())) {
        KdenliveSettings::setV4l_parameters(data.section(';', 0, 0));
        KdenliveSettings::setV4l_extension(data.section(';', 1, 1));
    }
    // screengrab
    data = m_configCapture.kcfg_grab_profile->itemData(m_configCapture.kcfg_grab_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(';', 0, 0) != KdenliveSettings::grab_parameters() || data.section(';', 1, 1) != KdenliveSettings::grab_extension())) {
        KdenliveSettings::setGrab_parameters(data.section(';', 0, 0));
        KdenliveSettings::setGrab_extension(data.section(';', 1, 1));
    }

    // decklink
    data = m_configCapture.kcfg_decklink_profile->itemData(m_configCapture.kcfg_decklink_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(';', 0, 0) != KdenliveSettings::decklink_parameters() || data.section(';', 1, 1) != KdenliveSettings::decklink_extension())) {
        KdenliveSettings::setDecklink_parameters(data.section(';', 0, 0));
        KdenliveSettings::setDecklink_extension(data.section(';', 1, 1));
    }
    // proxies
    data = m_configProject.kcfg_proxy_profile->itemData(m_configProject.kcfg_proxy_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(';', 0, 0) != KdenliveSettings::proxyparams() || data.section(';', 1, 1) != KdenliveSettings::proxyextension())) {
        KdenliveSettings::setProxyparams(data.section(';', 0, 0));
        KdenliveSettings::setProxyextension(data.section(';', 1, 1));
    }

    // timeline preview
    data = m_configProject.kcfg_preview_profile->itemData(m_configProject.kcfg_preview_profile->currentIndex()).toString();
    if (!data.isEmpty() && (data.section(';', 0, 0) != KdenliveSettings::previewparams() || data.section(';', 1, 1) != KdenliveSettings::previewextension())) {
        KdenliveSettings::setPreviewparams(data.section(';', 0, 0));
        KdenliveSettings::setPreviewextension(data.section(';', 1, 1));
    }

    if (updateCapturePath) emit updateCaptureFolder();
    if (updateLibrary) emit updateLibraryFolder();

    QString value = m_configCapture.kcfg_v4l_alsadevice->itemData(m_configCapture.kcfg_v4l_alsadevice->currentIndex()).toString();
    if (value != KdenliveSettings::v4l_alsadevicename()) {
        KdenliveSettings::setV4l_alsadevicename(value);
    }

    if (m_configSdl.kcfg_external_display->isChecked() != KdenliveSettings::external_display()) {
        KdenliveSettings::setExternal_display(m_configSdl.kcfg_external_display->isChecked());
        resetProfile = true;
    }

    value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    if (value != KdenliveSettings::audiodrivername()) {
        KdenliveSettings::setAudiodrivername(value);
        resetProfile = true;
    }

    if (value == QLatin1String("alsa")) {
        // Audio device setting is only valid for alsa driver
        value = m_configSdl.kcfg_audio_device->itemData(m_configSdl.kcfg_audio_device->currentIndex()).toString();
        if (value != KdenliveSettings::audiodevicename()) {
            KdenliveSettings::setAudiodevicename(value);
            resetProfile = true;
        }
    } else if (KdenliveSettings::audiodevicename().isEmpty() == false) {
        KdenliveSettings::setAudiodevicename(QString());
        resetProfile = true;
    }

    value = m_configSdl.kcfg_audio_backend->itemData(m_configSdl.kcfg_audio_backend->currentIndex()).toString();
    if (value != KdenliveSettings::audiobackend()) {
        KdenliveSettings::setAudiobackend(value);
        resetProfile = true;
    }

    if (m_configSdl.kcfg_window_background->color() != KdenliveSettings::window_background()) {
        KdenliveSettings::setWindow_background(m_configSdl.kcfg_window_background->color());
        resetProfile = true;
    }

    if (m_configSdl.kcfg_volume->value() != KdenliveSettings::volume()) {
        KdenliveSettings::setVolume(m_configSdl.kcfg_volume->value());
        resetProfile = true;
    }

    if (m_configMisc.kcfg_tabposition->currentIndex() != KdenliveSettings::tabposition()) {
        KdenliveSettings::setTabposition(m_configMisc.kcfg_tabposition->currentIndex());
    }

    if (m_modified) {
        // The transcoding profiles were modified, save.
        m_modified = false;
        saveTranscodeProfiles();
    }

#ifdef USE_JOGSHUTTLE
    m_shuttleModified = false;

    QStringList actions;
    actions << QStringLiteral("monitor_pause");  // the Job rest position action.
    foreach (KComboBox* button, m_shuttle_buttons) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    //fprintf(stderr, "Shuttle config: %s\n", JogShuttleConfig::actionMap(actions).toLatin1().constData());
    if (KdenliveSettings::shuttlebuttons() != maps)
        KdenliveSettings::setShuttlebuttons(maps);
#endif

    bool restart = false;
    if (m_configSdl.kcfg_gpu_accel->isChecked() != KdenliveSettings::gpu_accel()) {
	// GPU setting was changed, we need to restart Kdenlive or everything will be corrupted
	if (KMessageBox::warningContinueCancel(this, i18n("Kdenlive must be restarted to change this setting")) == KMessageBox::Continue) {
	    restart = true;
	}
	else {
	    m_configSdl.kcfg_gpu_accel->setChecked(KdenliveSettings::gpu_accel());
	}
    }

    KConfigDialog::settingsChangedSlot();
    //KConfigDialog::updateSettings();
    if (resetProfile) emit doResetProfile();
    if (restart) emit restartKdenlive();
    emit checkTabPosition();
}

void KdenliveSettingsDialog::slotCheckAlsaDriver()
{
    QString value = m_configSdl.kcfg_audio_driver->itemData(m_configSdl.kcfg_audio_driver->currentIndex()).toString();
    m_configSdl.kcfg_audio_device->setEnabled(value == QLatin1String("alsa"));
}

void KdenliveSettingsDialog::slotCheckAudioBackend()
{
    QString value = m_configSdl.kcfg_audio_backend->itemData(m_configSdl.kcfg_audio_backend->currentIndex()).toString();
    m_configSdl.group_sdl->setEnabled(value == QLatin1String("sdl_audio"));
}

void KdenliveSettingsDialog::loadTranscodeProfiles()
{
    KSharedConfigPtr config = KSharedConfig::openConfig(QStandardPaths::locate(QStandardPaths::DataLocation, QStringLiteral("kdenlivetranscodingrc")), KConfig::CascadeConfig);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    m_configTranscode.profiles_list->blockSignals(true);
    m_configTranscode.profiles_list->clear();
    QMap< QString, QString > profiles = transConfig.entryMap();
    QMapIterator<QString, QString> i(profiles);
    while (i.hasNext()) {
        i.next();
        QListWidgetItem *item = new QListWidgetItem(i.key());
        QString data = i.value();
        if (data.contains(';')) item->setToolTip(data.section(';', 1, 1));
        item->setData(Qt::UserRole, data);
        m_configTranscode.profiles_list->addItem(item);
        //item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
    }
    m_configTranscode.profiles_list->blockSignals(false);
    m_configTranscode.profiles_list->setCurrentRow(0);
}

void KdenliveSettingsDialog::saveTranscodeProfiles()
{
    QString transcodeFile = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/kdenlivetranscodingrc";
    KSharedConfigPtr config = KSharedConfig::openConfig(transcodeFile);
    KConfigGroup transConfig(config, "Transcoding");
    // read the entries
    transConfig.deleteGroup();
    int max = m_configTranscode.profiles_list->count();
    for (int i = 0; i < max; ++i) {
        QListWidgetItem *item = m_configTranscode.profiles_list->item(i);
        transConfig.writeEntry(item->text(), item->data(Qt::UserRole).toString());
    }
    config->sync();
}

void KdenliveSettingsDialog::slotAddTranscode()
{
    if (!m_configTranscode.profiles_list->findItems(m_configTranscode.profile_name->text(), Qt::MatchExactly).isEmpty()) {
        KMessageBox::sorry(this, i18n("A profile with that name already exists"));
        return;
    }
    QListWidgetItem *item = new QListWidgetItem(m_configTranscode.profile_name->text());
    QString data = m_configTranscode.profile_parameters->toPlainText();
    data.append(" %1." + m_configTranscode.profile_extension->text());
    data.append(';');
    if (!m_configTranscode.profile_description->text().isEmpty())
        data.append(m_configTranscode.profile_description->text());
    if (m_configTranscode.profile_audioonly->isChecked()) data.append(";audio");
    item->setData(Qt::UserRole, data);
    m_configTranscode.profiles_list->addItem(item);
    m_configTranscode.profiles_list->setCurrentItem(item);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotUpdateTranscodingProfile()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) return;
    m_configTranscode.button_update->setEnabled(false);
    item->setText(m_configTranscode.profile_name->text());
    QString data = m_configTranscode.profile_parameters->toPlainText();
    data.append(" %1." + m_configTranscode.profile_extension->text());
    data.append(';');
    if (!m_configTranscode.profile_description->text().isEmpty())
        data.append(m_configTranscode.profile_description->text());
    if (m_configTranscode.profile_audioonly->isChecked()) data.append(";audio");
    item->setData(Qt::UserRole, data);
    slotDialogModified();
}

void KdenliveSettingsDialog::slotDeleteTranscode()
{
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (item == NULL) return;
    delete item;
    slotDialogModified();
}

void KdenliveSettingsDialog::slotEnableTranscodeUpdate()
{
    if (!m_configTranscode.profile_box->isEnabled()) return;
    bool allow = true;
    if (m_configTranscode.profile_name->text().isEmpty() || m_configTranscode.profile_extension->text().isEmpty()) allow = false;
    m_configTranscode.button_update->setEnabled(allow);
}

void KdenliveSettingsDialog::slotSetTranscodeProfile()
{
    m_configTranscode.profile_box->setEnabled(false);
    m_configTranscode.button_update->setEnabled(false);
    m_configTranscode.profile_name->clear();
    m_configTranscode.profile_description->clear();
    m_configTranscode.profile_extension->clear();
    m_configTranscode.profile_parameters->clear();
    m_configTranscode.profile_audioonly->setChecked(false);
    QListWidgetItem *item = m_configTranscode.profiles_list->currentItem();
    if (!item) {
        return;
    }
    m_configTranscode.profile_name->setText(item->text());
    QString data = item->data(Qt::UserRole).toString();
    if (data.contains(';')) {
        m_configTranscode.profile_description->setText(data.section(';', 1, 1));
        if (data.section(';', 2, 2) == QLatin1String("audio")) m_configTranscode.profile_audioonly->setChecked(true);
        data = data.section(';', 0, 0).simplified();
    }
    m_configTranscode.profile_extension->setText(data.section('.', -1));
    m_configTranscode.profile_parameters->setPlainText(data.section(' ', 0, -2));
    m_configTranscode.profile_box->setEnabled(true);
}

void KdenliveSettingsDialog::slotShuttleModified()
{
#ifdef USE_JOGSHUTTLE
    QStringList actions;
    actions << QStringLiteral("monitor_pause");  // the Job rest position action.
    foreach (KComboBox* button, m_shuttle_buttons) {
        actions << m_mappable_actions[button->currentText()];
    }
    QString maps = JogShuttleConfig::actionMap(actions);
    m_shuttleModified = KdenliveSettings::shuttlebuttons() != maps;
#endif
    KConfigDialog::updateButtons();
}

void KdenliveSettingsDialog::slotDialogModified()
{
    m_modified = true;
    KConfigDialog::updateButtons();
}

//virtual
bool KdenliveSettingsDialog::hasChanged()
{
    if (m_modified || m_shuttleModified) return true;
    return KConfigDialog::hasChanged();
}

void KdenliveSettingsDialog::slotUpdatev4lDevice()
{
    QString device = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex()).toString();
    if (!device.isEmpty()) m_configCapture.kcfg_video4vdevice->setText(device);
    QString info = m_configCapture.kcfg_detectedv4ldevices->itemData(m_configCapture.kcfg_detectedv4ldevices->currentIndex(), Qt::UserRole + 1).toString();

    m_configCapture.kcfg_v4l_format->blockSignals(true);
    m_configCapture.kcfg_v4l_format->clear();

    QString vl4ProfilePath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/video4linux";
    if (QFile::exists(vl4ProfilePath)) {
        m_configCapture.kcfg_v4l_format->addItem(i18n("Current settings"));
    }

    QStringList pixelformats = info.split('>', QString::SkipEmptyParts);
    QString itemSize;
    QString pixelFormat;
    QStringList itemRates;
    for (int i = 0; i < pixelformats.count(); ++i) {
        QString format = pixelformats.at(i).section(':', 0, 0);
        QStringList sizes = pixelformats.at(i).split(':', QString::SkipEmptyParts);
        pixelFormat = sizes.takeFirst();
        for (int j = 0; j < sizes.count(); ++j) {
            itemSize = sizes.at(j).section('=', 0, 0);
            itemRates = sizes.at(j).section('=', 1, 1).split(',', QString::SkipEmptyParts);
            for (int k = 0; k < itemRates.count(); ++k) {
                m_configCapture.kcfg_v4l_format->addItem('[' + format + "] " + itemSize + " (" + itemRates.at(k) + ')', QStringList() << format << itemSize.section('x', 0, 0) << itemSize.section('x', 1, 1) << itemRates.at(k).section('/', 0, 0) << itemRates.at(k).section('/', 1, 1));
            }
        }
    }
    m_configCapture.kcfg_v4l_format->blockSignals(false);
    slotUpdatev4lCaptureProfile();
}

void KdenliveSettingsDialog::slotUpdatev4lCaptureProfile()
{
    QStringList info = m_configCapture.kcfg_v4l_format->itemData(m_configCapture.kcfg_v4l_format->currentIndex(), Qt::UserRole).toStringList();
    if (info.isEmpty()) {
        // No auto info, display the current ones
        loadCurrentV4lProfileInfo();
        return;
    }
    m_configCapture.p_size->setText(info.at(1) + 'x' + info.at(2));
    m_configCapture.p_fps->setText(info.at(3) + '/' + info.at(4));
    m_configCapture.p_aspect->setText(QStringLiteral("1/1"));
    m_configCapture.p_display->setText(info.at(1) + '/' + info.at(2));
    m_configCapture.p_colorspace->setText(ProfilesDialog::getColorspaceDescription(601));
    m_configCapture.p_progressive->setText(i18n("Progressive"));

    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/");
    if (!dir.exists() || !dir.exists(QStringLiteral("video4linux"))) saveCurrentV4lProfile();
}

void KdenliveSettingsDialog::loadCurrentV4lProfileInfo()
{
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/");
    if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
    }
    MltVideoProfile prof;
    if (!dir.exists(QStringLiteral("video4linux"))) {
        // No default formats found, build one
        prof.width = 320;
        prof.height = 200;
        prof.frame_rate_num = 15;
        prof.frame_rate_den = 1;
        prof.display_aspect_num = 4;
        prof.display_aspect_den = 3;
        prof.sample_aspect_num = 1;
        prof.sample_aspect_den = 1;
        prof.progressive = 1;
        prof.colorspace = 601;
        ProfilesDialog::saveProfile(prof, dir.absoluteFilePath(QStringLiteral("video4linux")));
    }
    else prof = ProfilesDialog::getVideoProfile(dir.absoluteFilePath(QStringLiteral("video4linux")));
    m_configCapture.p_size->setText(QString::number(prof.width) + 'x' + QString::number(prof.height));
    m_configCapture.p_fps->setText(QString::number(prof.frame_rate_num) + '/' + QString::number(prof.frame_rate_den));
    m_configCapture.p_aspect->setText(QString::number(prof.sample_aspect_num) + '/' + QString::number(prof.sample_aspect_den));
    m_configCapture.p_display->setText(QString::number(prof.display_aspect_num) + '/' + QString::number(prof.display_aspect_den));
    m_configCapture.p_colorspace->setText(ProfilesDialog::getColorspaceDescription(prof.colorspace));
    if (prof.progressive) m_configCapture.p_progressive->setText(i18n("Progressive"));
}

void KdenliveSettingsDialog::saveCurrentV4lProfile()
{
    MltVideoProfile profile;
    profile.description = QStringLiteral("Video4Linux capture");
    profile.colorspace = ProfilesDialog::getColorspaceFromDescription(m_configCapture.p_colorspace->text());
    profile.width = m_configCapture.p_size->text().section('x', 0, 0).toInt();
    profile.height = m_configCapture.p_size->text().section('x', 1, 1).toInt();
    profile.sample_aspect_num = m_configCapture.p_aspect->text().section('/', 0, 0).toInt();
    profile.sample_aspect_den = m_configCapture.p_aspect->text().section('/', 1, 1).toInt();
    profile.display_aspect_num = m_configCapture.p_display->text().section('/', 0, 0).toInt();
    profile.display_aspect_den = m_configCapture.p_display->text().section('/', 1, 1).toInt();
    profile.frame_rate_num = m_configCapture.p_fps->text().section('/', 0, 0).toInt();
    profile.frame_rate_den = m_configCapture.p_fps->text().section('/', 1, 1).toInt();
    profile.progressive = m_configCapture.p_progressive->text() == i18n("Progressive");
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/");
    if (!dir.exists()) {
            dir.mkpath(QStringLiteral("."));
    }
    ProfilesDialog::saveProfile(profile, dir.absoluteFilePath(QStringLiteral("video4linux")));
}

void KdenliveSettingsDialog::slotManageEncodingProfile()
{
    QAction *act = qobject_cast<QAction *>(sender());
    int type = 0;
    if (act) {
        type = act->data().toInt();
    }
    QPointer<EncodingProfilesDialog> d = new EncodingProfilesDialog(type);
    d->exec();
    delete d;
    loadEncodingProfiles();
}

void KdenliveSettingsDialog::loadEncodingProfiles()
{
    KConfig conf(QStringLiteral("encodingprofiles.rc"), KConfig::CascadeConfig, QStandardPaths::DataLocation);

    // Load v4l profiles
    m_configCapture.kcfg_v4l_profile->blockSignals(true);
    QString currentItem = m_configCapture.kcfg_v4l_profile->currentText();
    m_configCapture.kcfg_v4l_profile->clear();
    KConfigGroup group(&conf, "video4linux");
    QMap< QString, QString > values = group.entryMap();
    QMapIterator<QString, QString> i(values);
    while (i.hasNext()) {
        i.next();
        if (!i.key().isEmpty()) m_configCapture.kcfg_v4l_profile->addItem(i.key(), i.value());
    }
    m_configCapture.kcfg_v4l_profile->blockSignals(false);
    if (!currentItem.isEmpty()) m_configCapture.kcfg_v4l_profile->setCurrentIndex(m_configCapture.kcfg_v4l_profile->findText(currentItem));

    // Load Screen Grab profiles
    m_configCapture.kcfg_grab_profile->blockSignals(true);
    currentItem = m_configCapture.kcfg_grab_profile->currentText();
    m_configCapture.kcfg_grab_profile->clear();
    KConfigGroup group2(&conf, "screengrab");
    values = group2.entryMap();
    QMapIterator<QString, QString> j(values);
    while (j.hasNext()) {
        j.next();
        if (!j.key().isEmpty()) m_configCapture.kcfg_grab_profile->addItem(j.key(), j.value());
    }
    m_configCapture.kcfg_grab_profile->blockSignals(false);
    if (!currentItem.isEmpty()) m_configCapture.kcfg_grab_profile->setCurrentIndex(m_configCapture.kcfg_grab_profile->findText(currentItem));

    // Load Decklink profiles
    m_configCapture.kcfg_decklink_profile->blockSignals(true);
    currentItem = m_configCapture.kcfg_decklink_profile->currentText();
    m_configCapture.kcfg_decklink_profile->clear();
    KConfigGroup group3(&conf, "decklink");
    values = group3.entryMap();
    QMapIterator<QString, QString> k(values);
    while (k.hasNext()) {
        k.next();
        if (!k.key().isEmpty()) m_configCapture.kcfg_decklink_profile->addItem(k.key(), k.value());
    }
    m_configCapture.kcfg_decklink_profile->blockSignals(false);
    if (!currentItem.isEmpty()) m_configCapture.kcfg_decklink_profile->setCurrentIndex(m_configCapture.kcfg_decklink_profile->findText(currentItem));
    
    // Load Timeline Preview profiles
    m_configProject.kcfg_preview_profile->blockSignals(true);
    currentItem = m_configProject.kcfg_preview_profile->currentText();
    m_configProject.kcfg_preview_profile->clear();
    KConfigGroup group5(&conf, "timelinepreview");
    values = group5.entryMap();
    m_configProject.kcfg_preview_profile->addItem(i18n("Automatic"));
    QMapIterator<QString, QString> l(values);
    while (l.hasNext()) {
        l.next();
        if (!l.key().isEmpty()) m_configProject.kcfg_preview_profile->addItem(l.key(), l.value());
    }
    if (!currentItem.isEmpty()) m_configProject.kcfg_preview_profile->setCurrentIndex(m_configProject.kcfg_preview_profile->findText(currentItem));
    m_configProject.kcfg_preview_profile->blockSignals(false);
    QString data = m_configProject.kcfg_preview_profile->itemData(m_configProject.kcfg_preview_profile->currentIndex()).toString();
    if (data.isEmpty()) {
        m_configProject.previewparams->clear();
    } else {
        m_configProject.previewparams->setPlainText(data.section(';', 0, 0));
    }

    // Load Proxy profiles
    m_configProject.kcfg_proxy_profile->blockSignals(true);
    currentItem = m_configProject.kcfg_proxy_profile->currentText();
    m_configProject.kcfg_proxy_profile->clear();
    KConfigGroup group4(&conf, "proxy");
    values = group4.entryMap();
    QMapIterator<QString, QString> m(values);
    while (m.hasNext()) {
        m.next();
        if (!m.key().isEmpty()) m_configProject.kcfg_proxy_profile->addItem(m.key(), m.value());
    }
    if (!currentItem.isEmpty()) m_configProject.kcfg_proxy_profile->setCurrentIndex(m_configProject.kcfg_proxy_profile->findText(currentItem));
    m_configProject.kcfg_proxy_profile->blockSignals(false);
    data = m_configProject.kcfg_proxy_profile->itemData(m_configProject.kcfg_proxy_profile->currentIndex()).toString();
    if (data.isEmpty()) {
        m_configProject.proxyparams->clear();
    } else {
        m_configProject.proxyparams->setPlainText(data.section(';', 0, 0));
    }
}

void KdenliveSettingsDialog::slotUpdateDecklinkProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::decklink_profile();
    else ix = m_configCapture.kcfg_decklink_profile->currentIndex();
    QString data = m_configCapture.kcfg_decklink_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configCapture.decklink_parameters->setPlainText(data.section(';', 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateV4lProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::v4l_profile();
    else ix = m_configCapture.kcfg_v4l_profile->currentIndex();
    QString data = m_configCapture.kcfg_v4l_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configCapture.v4l_parameters->setPlainText(data.section(';', 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateGrabProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::grab_profile();
    else ix = m_configCapture.kcfg_grab_profile->currentIndex();
    QString data = m_configCapture.kcfg_grab_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configCapture.grab_parameters->setPlainText(data.section(';', 0, 0));
    //
}

void KdenliveSettingsDialog::slotUpdateProxyProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::proxy_profile();
    else ix = m_configProject.kcfg_proxy_profile->currentIndex();
    QString data = m_configProject.kcfg_proxy_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configProject.proxyparams->setPlainText(data.section(';', 0, 0));
}

void KdenliveSettingsDialog::slotUpdatePreviewProfile(int ix)
{
    if (ix == -1) ix = KdenliveSettings::preview_profile();
    else ix = m_configProject.kcfg_preview_profile->currentIndex();
    QString data = m_configProject.kcfg_preview_profile->itemData(ix).toString();
    if (data.isEmpty()) return;
    m_configProject.previewparams->setPlainText(data.section(';', 0, 0));
}

void KdenliveSettingsDialog::slotEditVideo4LinuxProfile()
{
    QString vl4ProfilePath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/profiles/video4linux";
    QPointer<ProfilesDialog> w = new ProfilesDialog(vl4ProfilePath, true);
    if (w->exec() == QDialog::Accepted) {
        // save and update profile
        loadCurrentV4lProfileInfo();
    }
    delete w;
}

void KdenliveSettingsDialog::slotReloadBlackMagic()
{
    Render::getBlackMagicDeviceList(m_configCapture.kcfg_decklink_capturedevice, true);
    if (!Render::getBlackMagicOutputDeviceList(m_configSdl.kcfg_blackmagic_output_device, true)) {
        // No blackmagic card found
        m_configSdl.kcfg_external_display->setEnabled(false);
    }
    m_configSdl.kcfg_external_display->setEnabled(KdenliveSettings::decklink_device_found());
}

void KdenliveSettingsDialog::checkProfile()
{
    m_pw->loadProfile(KdenliveSettings::default_profile().isEmpty() ? KdenliveSettings::current_profile() : KdenliveSettings::default_profile());
}

void KdenliveSettingsDialog::slotReloadShuttleDevices()
{
#ifdef USE_JOGSHUTTLE
    QString devDirStr = QStringLiteral("/dev/input/by-id");
    QDir devDir(devDirStr);
    if (!devDir.exists()) {
        devDirStr = QStringLiteral("/dev/input");
    }

    QStringList devNamesList;
    QStringList devPathList;
    m_configShuttle.shuttledevicelist->clear();

    DeviceMap devMap = JogShuttle::enumerateDevices(devDirStr);
    DeviceMapIter iter = devMap.begin();
    while (iter != devMap.end()) {
        m_configShuttle.shuttledevicelist->addItem(iter.key(), iter.value());
        devNamesList << iter.key();
        devPathList << iter.value();
        ++iter;
    }
    KdenliveSettings::setShuttledevicenames(devNamesList);
    KdenliveSettings::setShuttledevicepaths(devPathList);
    QTimer::singleShot(200, this, SLOT(slotUpdateShuttleDevice()));
#endif //USE_JOGSHUTTLE
}

