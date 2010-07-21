/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** No Commercial Usage
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights.  These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "messageserver.h"
#include <QCoreApplication>
#include <QDebug>
#include <qmailnamespace.h>
#include <qmaillog.h>

#if !defined(NO_SHUTDOWN_SIGNAL_HANDLING) && defined(Q_OS_UNIX)
#include <signal.h>

static void shutdown(int n)
{
    qMailLog(Messaging) << "Received signal" << n << ", shutting down.";
    QCoreApplication::exit();
}
#endif

int main(int argc, char** argv)
{

    if(QMail::fileLock("messageserver-instance.lock") == -1)
        qFatal("Could not get messageserver lock. Messageserver might already be running!");

    QCoreApplication app(argc, argv);

    MessageServer server;

#if !defined(NO_SHUTDOWN_SIGNAL_HANDLING) && defined(Q_OS_UNIX)
    signal(SIGINT, shutdown);
    signal(SIGTERM, shutdown);
#endif

    int exitCode = app.exec();

    return exitCode;
}

