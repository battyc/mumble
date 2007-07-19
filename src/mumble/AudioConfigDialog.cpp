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

#include "AudioInput.h"
#include "AudioOutput.h"
#include "AudioConfigDialog.h"
#include "Global.h"

static ConfigWidget *AudioConfigDialogNew() {
	return new AudioConfigDialog();
}

static ConfigRegistrar registrar(10, AudioConfigDialogNew);

AudioConfigDialog::AudioConfigDialog(QWidget *p) : ConfigWidget(p) {
	QGroupBox *qgbInterface, *qgbTransmit, *qgbCompress, *qgbJitter, *qgbLoop;
	QLabel *l;
	QVBoxLayout *v;
	QGridLayout *grid;
	QList<QString> keys;
	QString key;
	int i;

	qgbInterface=new QGroupBox(tr("Interfaces"));
	qgbTransmit=new QGroupBox(tr("Transmission"));
	qgbCompress=new QGroupBox(tr("Compression"));
	qgbJitter=new QGroupBox(tr("Jitter Buffer"));
	qgbLoop=new QGroupBox(tr("Loopback Test"));

	grid = new QGridLayout();

	qcbInput = new QComboBox();
	keys=AudioInputRegistrar::qmNew->keys();
	foreach(key, keys) {
		qcbInput->addItem(key);
	}
	i=keys.indexOf(AudioInputRegistrar::current);
	if (i >= 0)
		qcbInput->setCurrentIndex(i);

	l = new QLabel(tr("Input"));
	l->setBuddy(qcbInput);

	qcbInput->setToolTip(tr("Input method for audio"));
	qcbInput->setWhatsThis(tr("<b>This is the input method to use for audio.</b><br />Most likely you want to use DirectSound."));

	grid->addWidget(l, 0, 0);
	grid->addWidget(qcbInput, 0, 1);

	qcbOutput = new QComboBox();
	keys=AudioOutputRegistrar::qmNew->keys();
	foreach(key, keys) {
		qcbOutput->addItem(key);
	}
	i=keys.indexOf(AudioOutputRegistrar::current);
	if (i >= 0)
		qcbOutput->setCurrentIndex(i);

	l = new QLabel(tr("Output"));
	l->setBuddy(qcbOutput);

	qcbOutput->setToolTip(tr("Output method for audio"));
	qcbOutput->setWhatsThis(tr("<b>This is the output method to use for audio.</b><br />Most likely you want to use DirectSound."));

	grid->addWidget(l, 1, 0);
	grid->addWidget(qcbOutput, 1, 1);

	qgbInterface->setLayout(grid);

	grid = new QGridLayout();

	qcbTransmit = new QComboBox();
	qcbTransmit->addItem(tr("Continuous"), Settings::Continous);
	qcbTransmit->addItem(tr("Voice Activity"), Settings::VAD);
	qcbTransmit->addItem(tr("Push To Talk"), Settings::PushToTalk);
	qcbTransmit->setCurrentIndex(g.s.atTransmit);
	qcbTransmit->setObjectName(QLatin1String("Transmit"));
	l = new QLabel(tr("Transmit"));
	l->setBuddy(qcbTransmit);

	qcbTransmit->setToolTip(tr("When to transmit your speech"));
	qcbTransmit->setWhatsThis(tr("<b>This sets when speech should be transmitted.</b><br />"
	                             "<i>Continuous</i> - All the time<br />"
	                             "<i>Voice Activity</i> - When you are speaking clearly.<br />"
	                             "<i>Push To Talk</i> - When you hold down the hotkey set under <i>Shortcuts</i>."));
	grid->addWidget(l, 0, 0);
	grid->addWidget(qcbTransmit, 0, 1, 1, 2);

	qsTransmitHold = new QSlider(Qt::Horizontal);
	qsTransmitHold->setRange(20, 250);
	qsTransmitHold->setSingleStep(1);
	qsTransmitHold->setPageStep(5);
	qsTransmitHold->setValue(g.s.iVoiceHold);
	qsTransmitHold->setObjectName(QLatin1String("TransmitHold"));

	l = new QLabel(tr("Voice Hold"));
	l->setBuddy(qsTransmitHold);

	qlTransmitHold=new QLabel();
	qlTransmitHold->setMinimumWidth(40);
	on_TransmitHold_valueChanged(qsTransmitHold->value());

	qsTransmitHold->setToolTip(tr("How long to keep transmitting after silence"));
	qsTransmitHold->setWhatsThis(tr("<b>This selects how long after a perceived stop in speech transmission should continue.</b><br />"
	                                "Set this higher if your voice breaks up when you speak (seen by a rapidly blinking voice icon next to your name).<br />"
	                                "Only makes sense when used with Voice Activity transmission."));

	grid->addWidget(l, 1, 0);
	grid->addWidget(qsTransmitHold, 1, 1);
	grid->addWidget(qlTransmitHold, 1, 2);

	qsFrames = new QSlider(Qt::Horizontal);
	qsFrames->setRange(1, 4);
	qsFrames->setSingleStep(1);
	qsFrames->setPageStep(1);
	qsFrames->setValue(g.s.iFramesPerPacket);
	qsFrames->setObjectName(QLatin1String("Frames"));

	l = new QLabel(tr("Audio per packet"));
	l->setBuddy(qsFrames);

	qlFrames=new QLabel();
	qlFrames->setMinimumWidth(40);
	qsQuality = NULL;
	qlBitrate = NULL;
	on_Frames_valueChanged(qsFrames->value());

	qsFrames->setToolTip(tr("How many audio frames to send per packet"));
	qsFrames->setWhatsThis(tr("<b>This selects how many audio frames should be put in one packet.</b><br />"
	                          "Increasing this will increase the "
	                          "latency of your voice, but will also reduce bandwidth requirements."));
	grid->addWidget(l, 2, 0);
	grid->addWidget(qsFrames, 2, 1);
	grid->addWidget(qlFrames, 2, 2);

	qcbPushClick = new QCheckBox(tr("PTT Audio cue"));
	qcbPushClick->setChecked(g.s.bPushClick);
	qcbPushClick->setToolTip(tr("Audible audio cue when push-to-talk pressed and released"));
	qcbPushClick->setWhatsThis(tr("<b>This enables the audio cues for push to talk.</b><br />"
	                              "Setting this will give you a short audio beep when push to talk is pressed and released."));

	qcbTCP = new QCheckBox(tr("Use TCP mode"));
	qcbTCP->setChecked(g.s.bTCPCompat);
	qcbTCP->setToolTip(tr("Use TCP Compatibility mode"));
	qcbTCP->setWhatsThis(tr("<b>Enable TCP Compatibility mode</b>.<br />"
	                        "This will make Mumble use only TCP when communicating with the server. "
	                        "This will increase overhead and cause lost packets to produce noticeable "
	                        "pauses in communication, so this should only be used if you are unable to "
	                        "use the default (which uses UDP for voice and TCP for control)."));

	qcbReconnect = new QCheckBox(tr("Automatic Reconnect"));
	qcbReconnect->setChecked(g.s.bReconnect);
	qcbReconnect->setToolTip(tr("Reconnect when disconnected"));
	qcbReconnect->setWhatsThis(tr("<b>Reconnect when disconnected</b>.<br />"
	                              "This will make Mumble try to automatically reconnect after 10 "
	                              "seconds if your server "
	                              "connection fails."));

	QHBoxLayout *qhbl=new QHBoxLayout();
	qhbl->addWidget(qcbPushClick);
	qhbl->addWidget(qcbTCP);
	qhbl->addWidget(qcbReconnect);
	grid->addLayout(qhbl,3,1,1,2);

	qlBitrate = new QLabel();
	qlBitrate->setToolTip(tr("Maximum bandwidth used for sent audio"));
	qlBitrate->setWhatsThis(tr("<b>This shows peak outgoing bandwidth used.</b><br />"
	                           "This shows the peak amount of bandwidth sent out from your machine. Audio bitrate "
	                           "is the maximum bitrate (as we use VBR) for the audio data alone. Position "
	                           "is the bitrate used for positional information. Overhead is our framing and the "
	                           "IP packet headers (IP and UDP is 90% of this overhead)."));

	l = new QLabel(tr("Outgoing Bitrate"));
	l->setBuddy(qlBitrate);

	grid->addWidget(l, 4, 0);
	grid->addWidget(qlBitrate, 4, 1, 1, 2);

	qgbTransmit->setLayout(grid);


	grid = new QGridLayout();

	qsQuality = new QSlider(Qt::Horizontal);
	qsQuality->setRange(2, 10);
	qsQuality->setSingleStep(1);
	qsQuality->setPageStep(2);
	qsQuality->setValue(g.s.iQuality);
	qsQuality->setObjectName(QLatin1String("Quality"));

	l = new QLabel(tr("Quality"));
	l->setBuddy(qsQuality);

	qlQuality=new QLabel();
	qlQuality->setMinimumWidth(30);
	on_Quality_valueChanged(qsQuality->value());

	qsQuality->setToolTip(tr("Quality of compression (peak bandwidth)"));
	qsQuality->setWhatsThis(tr("<b>This sets the quality of compression.</b><br />"
	                           "This determines how much bandwidth Mumble is allowed to use for outgoing audio."));

	grid->addWidget(l, 0, 0);
	grid->addWidget(qsQuality, 0, 1);
	grid->addWidget(qlQuality, 0, 2);

	qsComplexity = new QSlider(Qt::Horizontal);
	qsComplexity->setRange(4, 10);
	qsComplexity->setSingleStep(1);
	qsComplexity->setPageStep(2);
	qsComplexity->setValue(g.s.iComplexity);
	qsComplexity->setObjectName(QLatin1String("Complexity"));

	l = new QLabel(tr("Complexity"));
	l->setBuddy(qsComplexity);

	qlComplexity=new QLabel();
	on_Complexity_valueChanged(qsComplexity->value());

	qsComplexity->setToolTip(tr("Complexity of compression (CPU)"));
	qsComplexity->setWhatsThis(tr("<b>This sets the complexity of compression.</b><br />"
	                              "This determines how much CPU Mumble is allowed to use to increase transmitted "
	                              "voice quality. Settings above 5 give only marginal gain."));

	grid->addWidget(l, 1, 0);
	grid->addWidget(qsComplexity, 1, 1);
	grid->addWidget(qlComplexity, 1, 2);

	qsAmp = new QSlider(Qt::Horizontal);
	qsAmp->setRange(0, 18000);
	qsAmp->setSingleStep(500);
	qsAmp->setPageStep(2000);
	qsAmp->setValue(20000 - g.s.iMinLoudness);
	qsAmp->setObjectName(QLatin1String("Amp"));

	l = new QLabel(tr("Amp"));
	l->setBuddy(qsAmp);

	qlAmp=new QLabel();
	on_Amp_valueChanged(qsAmp->value());

	qsAmp->setToolTip(tr("Maximum amplification of input sound"));
	qsAmp->setWhatsThis(tr(
	                        "<b>Maximum amplification of input.</b><br />"
	                        "Mumble normalizes the input volume before compressing, and this sets how much it's allowed to amplify.<br />"
	                        "The actual level is continually updated based on your current speech pattern, but it will never go "
	                        "above the level specified here.<br />"
	                        "If the <i>Microphone loudness</i> level of the audio statistics hover around 100%, you probably want to set this to 2.0 "
	                        "or so, but if, like most people, you are unable to reach 100%, set this to something much higher.<br />"
	                        "Ideally, set it so <i>Microphone Loudness * Amplification Factor >= 100</i>, even when you're speaking really soft.<br /><br />"
	                        "Note that there is no harm in setting this to maximum, but Mumble will start picking up other conversations "
	                        "if you leave it to auto-tune to that level."
	                    ));

	grid->addWidget(l, 2, 0);
	grid->addWidget(qsAmp, 2, 1);
	grid->addWidget(qlAmp, 2, 2);

	qgbCompress->setLayout(grid);


	grid = new QGridLayout();

	qsJitter = new QSlider(Qt::Horizontal);
	qsJitter->setRange(1, 5);
	qsJitter->setSingleStep(1);
	qsJitter->setPageStep(5);
	qsJitter->setValue(g.s.iJitterBufferSize);
	qsJitter->setObjectName(QLatin1String("Jitter"));

	l = new QLabel(tr("Default Jitter Buffer"));
	l->setBuddy(qsJitter);

	qlJitter=new QLabel();
	qlJitter->setMinimumWidth(40);
	on_Jitter_valueChanged(qsJitter->value());

	qsJitter->setToolTip(tr("How long to prebuffer on first packet"));
	qsJitter->setWhatsThis(tr("<b>This sets the default buffer size for the jitter buffer</b>.<br />"
	                          "All incoming audio is buffered, and the jitter buffer continually tries to "
	                          "push the buffer to the minimum sustainable by your network, so latency can "
	                          "be as low as possible. This sets the default buffer size to use on the first "
	                          "packet to arrive from a new speaker, or when a speaker using Voice Activity or "
	                          "Push-To-Talk just started talking again. If the start of sentences you hear is "
	                          "very jittery, increase this value."));
	grid->addWidget(l, 0, 0);
	grid->addWidget(qsJitter, 0, 1);
	grid->addWidget(qlJitter, 0, 2);

	qgbJitter->setLayout(grid);


	grid = new QGridLayout();

	qcbLoopback = new QComboBox();
	qcbLoopback->addItem(tr("None"), Global::None);
	qcbLoopback->addItem(tr("Local"), Global::Local);
	qcbLoopback->addItem(tr("Server"), Global::Server);
	qcbLoopback->setCurrentIndex(g.lmLoopMode);
	qcbLoopback->setObjectName(QLatin1String("Loopback"));
	l = new QLabel(tr("Loopback"));
	l->setBuddy(qcbLoopback);

	qcbLoopback->setToolTip(tr("Desired loopback mode"));
	qcbLoopback->setWhatsThis(tr("<b>This enables one of the loopback testmodes.</b><br />"
	                             "<i>None</i> - Loopback disabled<br />"
	                             "<i>Local</i> - Emulate a local server.<br />"
	                             "<i>Server</i> - Request loopback from server.<br />"
	                             "Please note than when loopback is enabled, no other players will hear your voice. "
	                             "This setting is not saved on application exit."
	                            ));
	grid->addWidget(l, 0, 0);
	grid->addWidget(qcbLoopback, 0, 1, 1, 2);


	qsPacketDelay = new QSlider(Qt::Horizontal);
	qsPacketDelay->setRange(0, 100);
	qsPacketDelay->setSingleStep(1);
	qsPacketDelay->setPageStep(10);
	qsPacketDelay->setValue(static_cast<int>(g.dMaxPacketDelay));
	qsPacketDelay->setObjectName(QLatin1String("PacketDelay"));

	l = new QLabel(tr("Delay Variance"));
	l->setBuddy(qsPacketDelay);

	qlPacketDelay=new QLabel();
	qlPacketDelay->setMinimumWidth(40);
	on_PacketDelay_valueChanged(qsPacketDelay->value());

	qsPacketDelay->setToolTip(tr("Variance in packet latency"));
	qsPacketDelay->setWhatsThis(tr("<b>This sets the packet latency variance for loopback testing</b><br />"
	                               "Most audio paths contain some variable latency. This allows you set that variance for loopback "
	                               "mode testing. For example, if you set this to 15ms, this will emulate a network with 20-35ms ping latency "
	                               "or one with 80-95ms latency. Most domestic net connections have a variance of about 5ms"
	                              ));

	grid->addWidget(l, 1, 0);
	grid->addWidget(qsPacketDelay, 1, 1);
	grid->addWidget(qlPacketDelay, 1, 2);

	qsPacketLoss = new QSlider(Qt::Horizontal);
	qsPacketLoss->setRange(0, 50);
	qsPacketLoss->setSingleStep(5);
	qsPacketLoss->setPageStep(20);
	qsPacketLoss->setValue(static_cast<int>(g.dPacketLoss * 100.0));
	qsPacketLoss->setObjectName(QLatin1String("PacketLoss"));

	l = new QLabel(tr("Packet Loss"));
	l->setBuddy(qsPacketLoss);

	qlPacketLoss=new QLabel();
	qlPacketLoss->setMinimumWidth(40);
	on_PacketLoss_valueChanged(qsPacketLoss->value());

	qsPacketLoss->setToolTip(tr("Packet loss for loopback mode"));
	qsPacketLoss->setWhatsThis(tr("<b>This sets the packet loss for loopback mode.</b><br />"
	                              "This will be the ratio of packets lost. Unless your outgoing bandwidth is peaked or "
	                              "there's something wrong with your network connection, this will be 0%%"
	                             ));

	grid->addWidget(l, 2, 0);
	grid->addWidget(qsPacketLoss, 2, 1);
	grid->addWidget(qlPacketLoss, 2, 2);

	qgbLoop->setLayout(grid);

	v = new QVBoxLayout;
	v->addWidget(qgbInterface);
	v->addWidget(qgbTransmit);
	v->addWidget(qgbCompress);
	v->addWidget(qgbJitter);
	v->addWidget(qgbLoop);
	v->addStretch(1);
	setLayout(v);

	on_Transmit_currentIndexChanged(qcbTransmit->currentIndex());
	on_Loopback_currentIndexChanged(qcbLoopback->currentIndex());

	QMetaObject::connectSlotsByName(this);

}

QString AudioConfigDialog::title() const {
	return tr("Basic Audio");
}

QIcon AudioConfigDialog::icon() const {
	return QIcon(QLatin1String("skin:config_basic.png"));
}

void AudioConfigDialog::accept() {
	g.s.iQuality = qsQuality->value();
	g.s.iComplexity = qsComplexity->value();
	g.s.iMinLoudness = 18000 - qsAmp->value() + 2000;
	g.s.iVoiceHold = qsTransmitHold->value();
	g.s.iFramesPerPacket = qsFrames->value();
	g.s.bPushClick = qcbPushClick->isChecked();
	g.s.bTCPCompat = qcbTCP->isChecked();
	g.s.bReconnect = qcbReconnect->isChecked();
	g.s.iJitterBufferSize = qsJitter->value();
	g.s.atTransmit = static_cast<Settings::AudioTransmit>(qcbTransmit->currentIndex());
	g.s.qsAudioInput = qcbInput->currentText();
	g.s.qsAudioOutput = qcbOutput->currentText();
	g.lmLoopMode = static_cast<Global::LoopMode>(qcbLoopback->currentIndex());
	g.dMaxPacketDelay = qsPacketDelay->value();
	g.dPacketLoss = qsPacketLoss->value() / 100.0;
}

void AudioConfigDialog::on_Frames_valueChanged(int v) {
	qlFrames->setText(tr("%1 ms").arg(v*20));
	updateBitrate();
}

void AudioConfigDialog::on_TransmitHold_valueChanged(int v) {
	double val = v * 20;
	val = val / 1000.0;
	qlTransmitHold->setText(tr("%1 s").arg(val, 0, 'f', 2));
}

void AudioConfigDialog::on_Quality_valueChanged(int v) {
	qlQuality->setText(QString::number(v));
	updateBitrate();
}

void AudioConfigDialog::on_Complexity_valueChanged(int v) {
	qlComplexity->setText(QString::number(v));
}

void AudioConfigDialog::on_Amp_valueChanged(int v) {
	v = 18000 - v + 2000;
	double d = 20000.0/v;
	qlAmp->setText(QString::fromLatin1("%1").arg(d, 0, 'f', 2));
}

void AudioConfigDialog::on_Jitter_valueChanged(int v) {
	qlJitter->setText(tr("%1 ms").arg(v*20));
}

void AudioConfigDialog::on_PacketDelay_valueChanged(int v) {
	qlPacketDelay->setText(tr("%1 ms").arg(v));
}

void AudioConfigDialog::on_PacketLoss_valueChanged(int v) {
	qlPacketLoss->setText(tr("%1%").arg(v));
}


void AudioConfigDialog::updateBitrate() {
	if (! qsQuality || ! qsFrames || ! qlBitrate)
		return;
	int q = qsQuality->value();
	int p = qsFrames->value();

	int audiorate, overhead, posrate;
	float f = q;
	void *es;

	es = speex_encoder_init(&speex_wb_mode);
	speex_encoder_ctl(es,SPEEX_SET_VBR_QUALITY, &f);
	speex_encoder_ctl(es,SPEEX_GET_BITRATE,&audiorate);
	speex_encoder_destroy(es);

	// 50 packets, in bits, IP + UDP + type/id (Message header) + flags + seq
	overhead = 50 * 8 * (20 + 8 + 3 + 1 + 2);

	// TCP is 12 more bytes than UDP
	if (qcbTCP->isChecked())
		overhead += 50 * 8 * 12;

	// Individual packet sizes
	if (p > 1)
		overhead += p * 8 * 50;

	if (g.s.bTransmitPosition)
		posrate = 12;
	else
		posrate = 0;

	posrate = posrate * 50 * 8;

	overhead = overhead / p;
	posrate = posrate / p;

	int total = audiorate + overhead + posrate;

	QPalette pal;

	if ((total / 8 > g.iMaxBandwidth) && g.uiSession) {
		pal.setColor(qlBitrate->foregroundRole(), Qt::red);
	}

	qlBitrate->setPalette(pal);

	QString v = tr("%1kbit/s (Audio %2, Position %4, Overhead %3)").arg(total / 1000.0, 0, 'f', 1).arg(audiorate / 1000.0, 0, 'f', 1).arg(overhead / 1000.0, 0, 'f', 1).arg(posrate / 1000.0, 0, 'f', 1);
	qlBitrate->setText(v);
}

void AudioConfigDialog::on_Transmit_currentIndexChanged(int v) {
	bool cue = false;
	bool hold = false;

	switch (v) {
		case 1:
			hold = true;
			break;
		case 2:
			cue = true;
			break;
	}

	qcbPushClick->setEnabled(cue);
	qsTransmitHold->setEnabled(hold);
	qlTransmitHold->setEnabled(hold);
}

void AudioConfigDialog::on_Loopback_currentIndexChanged(int v) {
	bool ena = false;
	if (v == 1)
		ena = true;

	qsPacketDelay->setEnabled(ena);
	qlPacketDelay->setEnabled(ena);
	qsPacketLoss->setEnabled(ena);
	qlPacketLoss->setEnabled(ena);
}
