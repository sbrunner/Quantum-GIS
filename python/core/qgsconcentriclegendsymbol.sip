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

/**
 * \ingroup core
 * \class QgsConcentricLegendSymbol
 * \brief Symbol used to display the diagram size legend.
 * \note Added in version 3.0
 */
class QgsConcentricLegendSymbol : public QgsMarkerSymbol
{
%TypeHeaderCode
#include <qgsconcentriclegendsymbol.h>
%End

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
      const QList< QPair< QString, double > > values,
      const QgsUnitTypes::RenderUnit types,
      const double maxSize,
      const QgsDiagramSettings::SizeLegendType type );
    ~QgsConcentricLegendSymbol();
    virtual QgsConcentricLegendSymbol* clone() const /Factory/;
    void drawPreviewIcon( QPainter* painter, QSize size, QgsRenderContext* customContext = nullptr );
};
