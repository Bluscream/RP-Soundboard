// src/UpdateChecker.h
//----------------------------------
// RP Soundboard Source Code
// Copyright (c) 2015 Marius Graefe
// All rights reserved
// Contact: rp_soundboard@mgraefe.de
//----------------------------------

#pragma once

#include <QObject>
#include <QXmlStreamReader>
#include <QNetworkRequest>

class QNetworkReply;
class QNetworkAccessManager;
class ConfigModel;

class UpdateChecker : public QObject
{
	Q_OBJECT

  public:
	struct version_info_t
	{
		QString productName;
		int build;
		QString version;

		void reset();
		bool valid();
	};

  public:
	explicit UpdateChecker(QObject* parent = nullptr);
	void startCheck(bool explicitCheck = true, ConfigModel* config = nullptr);
	static QByteArray getUserAgent();
	static void setUserAgent(QNetworkRequest& request);

  public slots:
	void onFinishDownload(QNetworkReply* reply);

  private:
	void parseXml(QIODevice* device);
	void parseProduct(QXmlStreamReader& xml);
	void parseProductInner(QXmlStreamReader& xml);
	void notifyUserOfUpdate();

  private:
	QNetworkAccessManager* m_mgr;
	version_info_t m_verInfo;
	ConfigModel* m_config;
	bool m_explicitCheck;
};
