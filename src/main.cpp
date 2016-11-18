/*
 * Copyright © 2015-2016 Antti Lamminsalo
 *
 * This file is part of Orion.
 *
 * Orion is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with Orion.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QQmlApplicationEngine>
#include <QtQml>
#include <QQmlComponent>
#include <QQuickView>
#include <QScreen>
#include <QMainWindow>
#include <QQmlContext>
#include <QString>
#include <QtSvg/QGraphicsSvgItem>
#include <QFontDatabase>
#include <QResource>
#include "model/channelmanager.h"
#include "network/networkmanager.h"
#include "power/power.h"
#include "systray.h"
#include "customapp.h"
#include "model/vodmanager.h"
#include "model/ircchat.h"

#ifndef Q_OS_ANDROID
#include <QtWebEngine>
#include "notification/notificationmanager.h"
#include "util/runguard.h"
#endif

#ifdef MPV_PLAYER
    #include "player/mpvrenderer.h"
#endif

int main(int argc, char *argv[])
{
    //Force using "auto" value for QT_DEVICE_PIXEL_RATIO env var
    //NOTE apparently this causes application to crash on moving to another screen
    //qputenv("QT_DEVICE_PIXEL_RATIO",QByteArray("auto"));

    QGuiApplication app(argc, argv);

    //Init engine
    QQmlApplicationEngine engine;

#ifndef Q_OS_ANDROID
    //Init webengine
    QtWebEngine::initialize();

    //Single application solution
    RunGuard guard("wz0dPKqHv3vX0BBsUFZt");
    if ( !guard.tryToRun() ){
        guard.sendWakeup();
        return -1;
    }

    SysTray *tray = new SysTray();
    tray->setIcon(appIcon);
    QObject::connect(tray, SIGNAL(closeEventTriggered()), &app, SLOT(quit()));
#endif

    QIcon appIcon = QIcon(":/icon/orion.ico");
    app.setFont(QFont("qrc:/fonts/NotoSans-Regular.ttf"));

    app.setWindowIcon(appIcon);

    //Prime network manager
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    NetworkManager *netman = new NetworkManager(engine.networkAccessManager());

    //Create channels manager
    ChannelManager *cman = new ChannelManager(netman);
    cman->checkResources();

    //Screensaver mngr
    Power *power = new Power(static_cast<QApplication *>(&app));

    //Create vods manager
    VodManager *vod = new VodManager(netman);
//-------------------------------------------------------------------------------------------------------------------//

    qreal dpiMultiplier = QGuiApplication::primaryScreen()->logicalDotsPerInch();

#ifdef Q_OS_WIN
    dpiMultiplier /= 96;

#elif defined(Q_OS_LINUX)
    dpiMultiplier /= 96;

#elif defined(Q_OS_MAC)
    dpiMultiplier /= 72;

#endif

    //Small adjustment to sizing overall
    dpiMultiplier *= .8;

    qDebug() << "Pixel ratio " << QGuiApplication::primaryScreen()->devicePixelRatio();
    qDebug() <<"DPI mult: "<< dpiMultiplier;

    QQmlContext *rootContext = engine.rootContext();
    rootContext->setContextProperty("dpiMultiplier", dpiMultiplier);
    rootContext->setContextProperty("netman", netman);
    rootContext->setContextProperty("g_cman", cman);
    rootContext->setContextProperty("g_powerman", power);
    rootContext->setContextProperty("g_favourites", cman->getFavouritesProxy());
    rootContext->setContextProperty("g_results", cman->getResultsModel());
    rootContext->setContextProperty("g_featured", cman->getFeaturedProxy());
    rootContext->setContextProperty("g_games", cman->getGamesModel());
    rootContext->setContextProperty("g_vodmgr", vod);
    rootContext->setContextProperty("vodsModel", vod->getModel());

#ifdef MPV_PLAYER
    rootContext->setContextProperty("player_backend", "mpv");
    qmlRegisterType<MpvObject>("mpv", 1, 0, "MpvObject");

#elif defined (QTAV_PLAYER)
    rootContext->setContextProperty("player_backend", "qtav");

#elif defined (MULTIMEDIA_PLAYER)
    rootContext->setContextProperty("player_backend", "multimedia");
#endif

    qmlRegisterType<IrcChat>("aldrog.twitchtube.ircchat", 1, 0, "IrcChat");

    engine.load(QUrl("qrc:/main.qml"));

#ifndef Q_OS_ANDROID
    //Set up notifications
    NotificationManager *notificationManager = new NotificationManager(&engine, netman->getManager());
    QObject::connect(cman, SIGNAL(pushNotification(QString,QString,QString)), notificationManager, SLOT(pushNotification(QString,QString,QString)));

    rootContext->setContextProperty("g_guard", &guard);
    rootContext->setContextProperty("g_tray", tray);
    tray->show();
#endif
    qDebug() << "Starting window...";

    app.exec();

//-------------------------------------------------------------------------------------------------------------------//

    //Cleanup
    delete vod;
    delete netman;
    delete cman;

#ifndef Q_OS_ANDROID
    delete tray;
    delete notificationManager;
#endif

    qDebug() << "Closing application...";
    return 0;
}
