/***************************************************************************
    qgsconcentriclegendsymbol.h
    ---------------------------
    begin                : December 2016
    copyright            : (C) 2016 by Stéhane Brunner
    email                : stephane dot brunner at camptocamp dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSCONCENTRICLEGENDSYMBOL_H
#define QGSCONCENTRICLEGENDSYMBOL_H


#include <QPair>
#include <QString>

#include "qgssymbol.h"
#include "qgsdiagramrenderer.h"


typedef QPair< QString, double > LabelValue;

/**
 * \ingroup core
 * \class QgsConcentricLegendSymbol
 * \brief Symbol used to display the diagram size legend.
 * \note Added in version 3.0
 */
class CORE_EXPORT QgsConcentricLegendSymbol : public QgsMarkerSymbol
{
  public:

    /**
       * Constructor
       * @param symbol The symbol used at different size
       * @param values The label, size pairs
       * @param types The render unit types
       * @param maxSize The maximum size
       * @param type The type of legend
       */
    QgsConcentricLegendSymbol(
      const QgsMarkerSymbol* symbol,
      const QList< LabelValue > values,
      const QgsUnitTypes::RenderUnit types,
      const double maxSize,
      const QgsDiagramSettings::SizeLegendType type );
    ~QgsConcentricLegendSymbol();
    QgsConcentricLegendSymbol* clone() const override;
    void drawPreviewIcon( QPainter* painter, QSize size, QgsRenderContext* customContext = nullptr ) override;

  private:
    QgsConcentricLegendSymbol( const QgsConcentricLegendSymbol* const original );

    QList< QgsMarkerSymbol* > mSymbols;
    const QList< LabelValue > mValues;
    const double mMaxSize;
    const QgsDiagramSettings::SizeLegendType mType;
};

#endif // QGSCONCENTRICLEGENDSYMBOL_H
