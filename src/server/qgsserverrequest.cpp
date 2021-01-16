/***************************************************************************
                          qgsserverrequest.cpp

  Define ruquest class for getting request contents
  -------------------
  begin                : 2016-12-05
  copyright            : (C) 2016 by David Marteau
  email                : david dot marteau at 3liz dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsserverrequest.h"
#include <QUrlQuery>

QgsServerRequest::QgsServerRequest( const QString &url, Method method, const Headers &headers )
  : QgsServerRequest( QUrl( url ), method, headers )
{
  mHeaderFromEnum[QgsServerRequest::HOST] = QStringLiteral( "Host" );
  mHeaderFromEnum[QgsServerRequest::FORWARDED] = QStringLiteral( "Forwarded" );
  mHeaderFromEnum[QgsServerRequest::X_FORWARDED_HOST] = QStringLiteral( "X-Forwarded-Host" );
  mHeaderFromEnum[QgsServerRequest::X_FORWARDED_PROTO] = QStringLiteral( "X-Forwarded-Proto" );
  mHeaderFromEnum[QgsServerRequest::X_QGIS_SERVICE_URL] = QStringLiteral( "X-Qgis-Service-Url" );
  mHeaderFromEnum[QgsServerRequest::X_QGIS_WMS_SERVICE_URL] = QStringLiteral( "X-Qgis-Wms-Service-Url" );
  mHeaderFromEnum[QgsServerRequest::X_QGIS_WFS_SERVICE_URL] = QStringLiteral( "X-Qgis-Wfs-Service-Url" );
  mHeaderFromEnum[QgsServerRequest::X_QGIS_WCS_SERVICE_URL] = QStringLiteral( "X-Qgis-Wcs-Service-Url" );
  mHeaderFromEnum[QgsServerRequest::X_QGIS_WMTS_SERVICE_URL] = QStringLiteral( "X-Qgis-Wmts-Service-Url" );
}

QgsServerRequest::QgsServerRequest( const QUrl &url, Method method, const Headers &headers )
  : mUrl( url )
  , mOriginalUrl( url )
  , mMethod( method )
  , mHeaders( headers )
{
  mParams.load( QUrlQuery( url ) );
}

QString QgsServerRequest::methodToString( const QgsServerRequest::Method &method )
{
  static QMetaEnum metaEnum = QMetaEnum::fromType<QgsServerRequest::Method>();
  return QString( metaEnum.valueToKey( method ) ).remove( QStringLiteral( "Method" ) ).toUpper( );
}

QString QgsServerRequest::header( const QString &name ) const
{
  return mHeaders.value( name );
}

QString QgsServerRequest::headerEnum( const QgsServerRequest::Header header ) const
{
  QString headerName = mHeaderFromEnum[header];

  // Get from internal dictionary
  QString result = this->header( headerName );

  // Or from standard environmant variable
  // https://tools.ietf.org/html/rfc3875#section-4.1.18
  if ( result.isEmpty() )
  {
    result = qgetenv( QStringLiteral( "HTTP_%1" ).arg(
                        headerName.toUpper().replace( QLatin1Char( '-' ), QLatin1Char( '_' ) ) ).toStdString().c_str() );
  }
  return result;
}

void QgsServerRequest::setHeader( const QString &name, const QString &value )
{
  mHeaders.insert( name, value );
}

QMap<QString, QString> QgsServerRequest::headers() const
{
  return mHeaders;
}

void QgsServerRequest::removeHeader( const QString &name )
{
  mHeaders.remove( name );
}

QUrl QgsServerRequest::url() const
{
  return mUrl;
}

QUrl QgsServerRequest::originalUrl() const
{
  return mOriginalUrl;
}

void QgsServerRequest::setOriginalUrl( const QUrl &url )
{
  mOriginalUrl = url;
}

QgsServerRequest::Method QgsServerRequest::method() const
{
  return mMethod;
}

QMap<QString, QString> QgsServerRequest::parameters() const
{
  return mParams.toMap();
}

QgsServerParameters QgsServerRequest::serverParameters() const
{
  return mParams;
}

QByteArray QgsServerRequest::data() const
{
  return QByteArray();
}

void QgsServerRequest::setParameter( const QString &key, const QString &value )
{
  mParams.add( key, value );
  mUrl.setQuery( mParams.urlQuery() );
}

QString QgsServerRequest::parameter( const QString &key, const QString &defaultValue ) const
{
  const auto value { mParams.value( key ) };
  if ( value.isEmpty() )
  {
    return defaultValue;
  }
  return value;
}

void QgsServerRequest::removeParameter( const QString &key )
{
  mParams.remove( key );
  mUrl.setQuery( mParams.urlQuery() );
}

void QgsServerRequest::setUrl( const QUrl &url )
{
  mUrl = url;
  mParams.clear();
  mParams.load( QUrlQuery( mUrl ) );
}

void QgsServerRequest::setMethod( Method method )
{
  mMethod = method;
}

const QString QgsServerRequest::queryParameter( const QString &name, const QString &defaultValue ) const
{
  if ( !QUrlQuery( mUrl ).hasQueryItem( name ) )
  {
    return defaultValue;
  }
  return QUrl::fromPercentEncoding( QUrlQuery( mUrl ).queryItemValue( name ).toUtf8() );
}

const QString QgsServerRequest::scriptName() const
{
  return qgetenv( "SCRIPT_NAME" );
}
