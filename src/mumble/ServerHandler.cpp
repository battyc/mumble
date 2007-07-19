/* Copyright (C) 2005-2007, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "ServerHandler.h"
#include "MainWindow.h"
#include "AudioInput.h"
#include "AudioOutput.h"
#include "Message.h"
#include "Player.h"
#include "Connection.h"
#include "Global.h"
#include "Database.h"
#include "PacketDataStream.h"

ServerHandlerMessageEvent::ServerHandlerMessageEvent(QByteArray &msg, bool flush) : QEvent(static_cast<QEvent::Type>(SERVERSEND_EVENT)) {
	qbaMsg = msg;
	bFlush = flush;
}

ServerHandler::ServerHandler() {
	cConnection = NULL;
	qusUdp = NULL;

	uiTCPPing = uiUDPPing = 0LL;

	// For some strange reason, on Win32, we have to call supportsSsl before the cipher list is ready.
	qWarning("OpenSSL Support: %d", QSslSocket::supportsSsl());

	QList<QSslCipher> pref;
	foreach(QSslCipher c, QSslSocket::defaultCiphers()) {
		if (c.usedBits() < 128)
			continue;
		pref << c;
	}
	if (pref.isEmpty())
		qFatal("No ciphers of at least 128 bit found");
	QSslSocket::setDefaultCiphers(pref);
}

ServerHandler::~ServerHandler() {
	wait();
}

void ServerHandler::customEvent(QEvent *evt) {
	if (evt->type() != SERVERSEND_EVENT)
		return;

	ServerHandlerMessageEvent *shme=static_cast<ServerHandlerMessageEvent *>(evt);

	if (cConnection) {
		if (shme->qbaMsg.size() > 0) {
			cConnection->sendMessage(shme->qbaMsg);
			if (shme->bFlush)
				cConnection->forceFlush();
		} else {
			cConnection->disconnect();
		}
	}
}

void ServerHandler::udpReady() {
	while (qusUdp->hasPendingDatagrams()) {
		char buffer[65536];
		quint32 buflen = qusUdp->pendingDatagramSize();
		QHostAddress senderAddr;
		quint16 senderPort;
		qusUdp->readDatagram(buffer, qMin(65536U, buflen), &senderAddr, &senderPort);

		if (!(senderAddr == qhaRemote) || (senderPort != iPort))
			continue;

		PacketDataStream pds(buffer, buflen);

		quint32 msgType, uiSession;
		pds >> msgType >> uiSession;

		if (msgType == Message::Ping) {
			quint64 t;
			pds >> t;
			uiUDPPing = tTimestamp.elapsed() - t;
		} else if (msgType == Message::Speex) {
			Player *p = Player::get(uiSession);
			AudioOutputPtr ao = g.ao;
			if (ao) {
				if (p) {
					if (! p->bLocalMute) {
						unsigned int iSeq;
						pds >> iSeq;
						QByteArray qbaSpeexPacket(pds.dataBlock(pds.left()));
						ao->addFrameToBuffer(p, qbaSpeexPacket, iSeq);
					}
				} else {
					ao->removeBuffer(p);
				}
			}
		}
	}
}

void ServerHandler::sendMessage(Message *mMsg, bool forceTCP) {
	bool mayUdp = !forceTCP && ((mMsg->messageType() == Message::Speex) || (mMsg->messageType() == Message::Ping));
	mMsg->uiSession = g.uiSession;

	if (mayUdp && ! g.s.bTCPCompat) {
		QMutexLocker qml(&qmUdp);
		if (! qusUdp)
			return;
		char buffer[65536];
		PacketDataStream pds(buffer, 65536);
		mMsg->messageToNetwork(pds);
		qusUdp->writeDatagram(buffer, pds.size(), qhaRemote, iPort);
	} else {
		QByteArray qbaBuffer;
		mMsg->messageToNetwork(qbaBuffer);

		ServerHandlerMessageEvent *shme=new ServerHandlerMessageEvent(qbaBuffer, mayUdp);
		QApplication::postEvent(this, shme);
	}
}

void ServerHandler::run() {
	QSslSocket *qtsSock = new QSslSocket(this);
	cConnection = new Connection(this, qtsSock);

	qlErrors.clear();
	qscCert.clear();

	uiUDPPing = uiTCPPing = 0LL;

	connect(qtsSock, SIGNAL(encrypted()), this, SLOT(serverConnectionConnected()));
	connect(cConnection, SIGNAL(connectionClosed(QString)), this, SLOT(serverConnectionClosed(QString)));
	connect(cConnection, SIGNAL(message(QByteArray &)), this, SLOT(message(QByteArray &)));
	connect(cConnection, SIGNAL(handleSslErrors(const QList<QSslError> &)), this, SLOT(setSslErrors(const QList<QSslError> &)));
	qtsSock->connectToHostEncrypted(qsHostName, iPort);

	QTimer *ticker = new QTimer(this);
	connect(ticker, SIGNAL(timeout()), this, SLOT(sendPing()));
	ticker->start(5000);

	g.mw->rtLast = MessageServerReject::None;

	exec();

	ticker->stop();
	cConnection->disconnect();
	delete cConnection;
	cConnection = NULL;

	if (qusUdp) {
		QMutexLocker qml(&qmUdp);

		delete qusUdp;
		qusUdp = NULL;
	}
}

void ServerHandler::setSslErrors(const QList<QSslError> &errors) {
	qscCert = cConnection->peerCertificateChain();
	if ((qscCert.size() > 0)  && (QString::fromLatin1(qscCert.at(0).digest(QCryptographicHash::Sha1).toHex()) == Database::getDigest(qsHostName, iPort)))
		cConnection->proceedAnyway();
	else
		qlErrors = errors;
}

void ServerHandler::sendPing() {
	MessagePing mp;
	mp.uiTimestamp = tTimestamp.elapsed();
	sendMessage(&mp, true);
	sendMessage(&mp, false);
}

void ServerHandler::message(QByteArray &qbaMsg) {
	Message *mMsg = Message::networkToMessage(qbaMsg);
	if (! mMsg)
		return;

	Player *p = Player::get(mMsg->uiSession);
	AudioOutputPtr ao = g.ao;

	if (mMsg->messageType() == Message::Speex) {
		if (ao) {
			if (p) {
				MessageSpeex *msMsg=static_cast<MessageSpeex *>(mMsg);
				if (! p->bLocalMute)
					ao->addFrameToBuffer(p, msMsg->qbaSpeexPacket, msMsg->iSeq);
			} else {
				// Eek, we just got a late packet for a player already removed. Remove
				// the buffer and pretend this never happened.
				// If ~AudioOutputPlayer or decendants uses the Player object now,
				// Bad Things happen.
				ao->removeBuffer(p);
			}
		}
	} else if (mMsg->messageType() == Message::Ping) {
		MessagePing *mpMsg = static_cast<MessagePing *>(mMsg);
		uiTCPPing = tTimestamp.elapsed() - mpMsg->uiTimestamp;
	} else {
		if (mMsg->messageType() == Message::ServerLeave) {
			if (ao)
				ao->removeBuffer(p);
		}
		ServerHandlerMessageEvent *shme=new ServerHandlerMessageEvent(qbaMsg, false);
		QApplication::postEvent(g.mw, shme);
	}

	delete mMsg;
}

void ServerHandler::disconnect() {
	// Actual TCP object is in a different thread, so signal it
	QByteArray qbaBuffer;
	ServerHandlerMessageEvent *shme=new ServerHandlerMessageEvent(qbaBuffer, false);
	QApplication::postEvent(this, shme);
}

void ServerHandler::serverConnectionClosed(QString reason) {
	AudioOutputPtr ao = g.ao;
	if (ao)
		ao->wipe();
	emit disconnected(reason);
	exit(0);
}

void ServerHandler::serverConnectionConnected() {
	qscCert = cConnection->peerCertificateChain();
	qscCipher = cConnection->sessionCipher();

	AudioInputPtr ai = g.ai;
	MessageServerAuthenticate msaMsg;
	msaMsg.qsUsername = qsUserName;
	msaMsg.qsPassword = qsPassword;
	if (ai)
		msaMsg.iMaxBandwidth = ai->getMaxBandwidth();
	else
		msaMsg.iMaxBandwidth = 0;

	cConnection->sendMessage(&msaMsg);

	{
		QMutexLocker qml(&qmUdp);

		qusUdp = new QUdpSocket(this);
		qusUdp->bind();
		connect(qusUdp, SIGNAL(readyRead()), this, SLOT(udpReady()));
#ifdef Q_OS_WIN
		int tos = 0xb8;
		if (setsockopt(qusUdp->socketDescriptor(), IPPROTO_IP, 3, reinterpret_cast<char *>(&tos), sizeof(tos)) != 0) {
			tos = 0x98;
			setsockopt(qusUdp->socketDescriptor(), IPPROTO_IP, 3, reinterpret_cast<char *>(&tos), sizeof(tos));
		}
#endif
		qhaRemote = cConnection->peerAddress();
	}

	emit connected();
}

void ServerHandler::setConnectionInfo(const QString &host, int port, const QString &username, const QString &pw) {
	qsHostName = host;
	iPort = port;
	qsUserName = username;
	qsPassword = pw;
}

void ServerHandler::getConnectionInfo(QString &host, int &port, QString &username, QString &pw) {
	host = qsHostName;
	port = iPort;
	username = qsUserName;
	pw = qsPassword;
}
