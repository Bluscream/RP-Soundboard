// src/UpdateChecker.cpp
//----------------------------------
// RP Soundboard Source Code
// Copyright (c) 2015 Marius Graefe
// All rights reserved
// Contact: rp_soundboard@mgraefe.de
//----------------------------------


// Parses XML Files from a server
// Example File:

// <?xml version="1.0" encoding="utf-8"?>
//
// <versionDescription>
//   <product descVersion="1" name="rp_soundboard">
//     <latestVersion>1101</latestVersion>
//	   <featureUrl>http://mgraefe.de/rpsb/version/features_1101.txt</featureUrl>
//   </product>
// </versionDescription>

#include <QAbstractButton>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>
#include <QDateTime>
#include <QTimer>

#include "buildinfo.h"

#include "UpdateChecker.h"
#include "ts3log.h"
#include "ConfigModel.h"

#ifdef _DEBUG
	#define CHECK_URL "https://mgraefe.de/rpsb/version/version_debug.xml"
#else
	#define CHECK_URL "https://mgraefe.de/rpsb/version/version.xml"
#endif
#define GITHUB_URL "https://github.com/MGraefe/RP-Soundboard/releases"


std::string toStdStringUtf8(const QString& str)
{
	QByteArray arr = str.toUtf8();
	std::string res(arr.constData(), arr.size());
	return res;
}

bool isValid(const QXmlStreamReader& xml)
{
	return !(xml.hasError() || xml.atEnd());
}


UpdateChecker::UpdateChecker(QObject* parent /*= nullptr*/) :
	QObject(parent),
	m_config(nullptr),
	m_explicitCheck(false)
{
}


void UpdateChecker::startCheck(bool explicitCheck, ConfigModel* config)
{
	m_explicitCheck = explicitCheck;
	m_config = config;

	uint currentTime = QDateTime::currentDateTime().toTime_t();
	if (!m_explicitCheck && m_config && currentTime < m_config->getNextUpdateCheck())
		return;

	m_mgr = new QNetworkAccessManager(this);
	connect(m_mgr, SIGNAL(finished(QNetworkReply*)), this, SLOT(onFinishDownload(QNetworkReply*)));

	QUrl url(CHECK_URL);
	QNetworkRequest request;
	request.setUrl(url);
	setUserAgent(request);
	loading = Loading::mainXml;
	m_mgr->get(request);
}


void UpdateChecker::onFinishDownload(QNetworkReply* reply)
{
	switch (loading)
	{
	case Loading::mainXml:
		onFinishDownloadXml(reply);
		break;
	case Loading::features:
		onFinishDownloadFeatures(reply);
		break;
	}
}


void UpdateChecker::onFinishDownloadXml(QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError)
	{
		std::string err = toStdStringUtf8(reply->errorString());
		logError("UpdateChecker: Error requesting version document %s.\nError-String: %s", CHECK_URL, err.c_str());
		if (m_explicitCheck)
		{
			QMessageBox::warning(
				nullptr,
				"Update Check",
				QString("Could not check for updates.\n\n%1").arg(reply->errorString())
			);
		}
		return;
	}

	parseXml(reply);
	if (m_verInfo.valid() && m_verInfo.build > buildinfo_getVersionNumber(3))
	{
		if (!m_verInfo.featuresUrl.isEmpty())
		{
			QNetworkRequest request;
			request.setUrl(QUrl(m_verInfo.featuresUrl));
			setUserAgent(request);
			loading = Loading::features;
			m_mgr->get(request);
		}
		else
			notifyUserOfUpdate();
	}
	else // no new version
	{
		if (m_config)
		{
			// No update found -> don't bother checking again for 3 days
			uint currentTime = QDateTime::currentDateTime().toTime_t();
			m_config->setNextUpdateCheck(currentTime + 60 * 60 * 24 * 3);
		}

		if (m_explicitCheck)
		{
			QMessageBox::information(nullptr, "Update Check", "Your version of RP Soundboard is up to date.");
		}
	}
}


void UpdateChecker::onFinishDownloadFeatures(QNetworkReply* reply)
{
	if (reply->error() != QNetworkReply::NoError)
	{
		std::string err = toStdStringUtf8(reply->errorString());
		std::string url = toStdStringUtf8(reply->url().toString());
		logError("UpdateChecker: Error requesting features document %s.\nError-String: %s", url.c_str(), err.c_str());
	}
	else
	{
		m_verInfo.features = reply->readAll();
	}

	notifyUserOfUpdate();
}


void UpdateChecker::parseXml(QIODevice* device)
{
	m_verInfo.reset();

	QXmlStreamReader xml;
	xml.setDevice(device);

	while (isValid(xml))
	{
		QXmlStreamReader::TokenType token = xml.readNext();
		if (token == QXmlStreamReader::StartElement && xml.name() == "product")
			parseProduct(xml);
	}

	xml.clear();
}


void UpdateChecker::parseProduct(QXmlStreamReader& xml)
{
	if (xml.attributes().value("descVersion") == "1" && xml.attributes().value("name") == "rp_soundboard")
	{
		m_verInfo.productName = xml.attributes().value("name").toString();
		xml.readNext();
		while (isValid(xml) && !(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == "product"))
		{
			if (xml.tokenType() == QXmlStreamReader::StartElement)
				parseProductInner(xml);
			xml.readNext();
		}
	}
}


void UpdateChecker::parseProductInner(QXmlStreamReader& xml)
{
	if (xml.name() == "latestVersion")
	{
		xml.readNext();
		m_verInfo.build = xml.text().toInt();
	}
	else if (xml.name() == "latestVersionString")
	{
		xml.readNext();
		m_verInfo.version = xml.text().toString();
	}
	else if (xml.name() == "featuresUrl")
	{
		xml.readNext();
		m_verInfo.featuresUrl = xml.text().toString();
	}
}


void UpdateChecker::notifyUserOfUpdate()
{
	QString versionLabel = m_verInfo.version.isEmpty() ? QString("build %1").arg(m_verInfo.build) : m_verInfo.version;

	QMessageBox msgBox;
	msgBox.setTextFormat(Qt::RichText);
	msgBox.setText(QString(
						"A new version of RP Soundboard is available (%1).<br /><br />"
						"You can download it here: <a href=\"%2\">%2</a>"
	)
						.arg(versionLabel)
						.arg(GITHUB_URL));
	msgBox.setIcon(QMessageBox::Information);
	msgBox.setWindowTitle("New version of RP Soundboard!");
	msgBox.setStandardButtons(QMessageBox::Ok);
	msgBox.setDefaultButton(QMessageBox::Ok);
	if (m_verInfo.features.length() > 0)
	{
		msgBox.setDetailedText(m_verInfo.features);
		// QMessageBox hides the detailed text behind a "Show Details..." button. Click it
		// once the event loop runs so the details are visible by default.
		QTimer::singleShot(0, &msgBox, [&msgBox]() {
			for (QAbstractButton* btn : msgBox.buttons())
			{
				if (msgBox.buttonRole(btn) == QMessageBox::ActionRole)
				{
					btn->click();
					break;
				}
			}
		});
	}
	msgBox.exec();

	if (m_config)
	{
		// Don't bother user for 3 days
		uint currentTime = QDateTime::currentDateTime().toTime_t();
		m_config->setNextUpdateCheck(currentTime + 60 * 60 * 24 * 3);
	}
}


void UpdateChecker::version_info_t::reset()
{
	productName = QString();
	build = 0;
	version = QString();
	featuresUrl = QString();
	features = QString();
}


bool UpdateChecker::version_info_t::valid()
{
	return !productName.isNull() && !productName.isEmpty() && build > 0;
}


QByteArray UpdateChecker::getUserAgent() // static
{
	return QByteArray("RP Soundboard Update Checker, ") + buildinfo_getPluginVersion();
}


void UpdateChecker::setUserAgent(QNetworkRequest& request) // static
{
	request.setRawHeader("User-Agent", getUserAgent());
}
