/****************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** This file is part of the Qt Messaging Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef ATTACHMENTOPTIONS_H
#define ATTACHMENTOPTIONS_H

#include <qmailmessage.h>
#include <QByteArray>
#include <QDialog>
#include <QList>
#include <QSize>
#include <QString>

QT_BEGIN_NAMESPACE

class QByteArray;
class QLabel;
class QPushButton;
class QString;

QT_END_NAMESPACE

class AttachmentOptions : public QDialog
{
    Q_OBJECT

public:
    enum ContentClass
    {
        Text,
        Image,
        Media,
        Multipart,
        Other
    };

    AttachmentOptions(QWidget* parent);
    ~AttachmentOptions();

    QSize sizeHint() const;

signals:
    void retrieve(const QMailMessagePart& part);
    void retrievePortion(const QMailMessagePart& part, uint bytes);
    void respondToPart(const QMailMessagePart::Location& partLocation, QMailMessage::ResponseType);

public slots:
    void setAttachment(const QMailMessagePart& part);

    void viewAttachment();
    void saveAttachment();
    void retrieveAttachment();
    void forwardAttachment();

private:
    QSize _parentSize;
    QLabel* _name;
    QLabel* _type;
    //QLabel* _comment;
    QLabel* _sizeLabel;
    QLabel* _size;
    QPushButton* _view;
    QLabel* _viewer;
    QPushButton* _save;
    QLabel* _document;
    QPushButton* _retrieve;
    QPushButton* _forward;
    const QMailMessagePart* _part;
    ContentClass _class;
    QString _decodedText;
    QByteArray _decodedData;
    QStringList _temporaries;
};

#endif

