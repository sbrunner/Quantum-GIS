/***************************************************************************
  qgsdiagramproperties.cpp
  Adjust the properties for diagrams
  -------------------
         begin                : August 2012
         copyright            : (C) Matthias Kuhn
         email                : matthias at opengis dot ch

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "diagram/qgshistogramdiagram.h"
#include "diagram/qgspiediagram.h"
#include "diagram/qgstextdiagram.h"

#include "qgsproject.h"
#include "qgsapplication.h"
#include "qgsdiagramproperties.h"
#include "qgsdiagramrenderer.h"
#include "qgslabelengineconfigdialog.h"
#include "qgsmessagebar.h"
#include "qgsvectorlayerproperties.h"
#include "qgsvectordataprovider.h"
#include "qgsfeatureiterator.h"
#include "qgscolordialog.h"
#include "qgisgui.h"
#include "qgssymbolselectordialog.h"
#include "qgsstyle.h"
#include "qgsmapcanvas.h"
#include "qgsexpressionbuilderdialog.h"
#include "qgslogger.h"
#include "qgisapp.h"
#include "qgssettings.h"

#include <QList>
#include <QMessageBox>

#include <QSettings>
#include <QVariant>


QgsExpressionContext QgsDiagramProperties::createExpressionContext() const
{
  QgsExpressionContext expContext;
  expContext << QgsExpressionContextUtils::globalScope()
             << QgsExpressionContextUtils::projectScope( QgsProject::instance() )
             << QgsExpressionContextUtils::atlasScope( nullptr )
             << QgsExpressionContextUtils::mapSettingsScope( mMapCanvas->mapSettings() )
             << QgsExpressionContextUtils::layerScope( mLayer );

  return expContext;
}

QgsDiagramProperties::QgsDiagramProperties( QgsVectorLayer *layer, QWidget *parent, QgsMapCanvas *canvas )
  : QWidget( parent )
  , mMapCanvas( canvas )
{

  mLayer = layer;
  if ( !layer )
  {
    return;
  }

  setupUi( this );

  mSizeRulesTable->setItemDelegate( new SizeRuleItemDelegate( this ) );

  // get rid of annoying outer focus rect on Mac
  mDiagramOptionsListWidget->setAttribute( Qt::WA_MacShowFocusRect, false );

  mDiagramTypeComboBox->blockSignals( true );
  QPixmap pix = QgsApplication::getThemePixmap( QStringLiteral( "diagramNone" ) );
  mDiagramTypeComboBox->addItem( pix, tr( "No diagrams" ), "None" );
  pix = QgsApplication::getThemePixmap( QStringLiteral( "pie-chart" ) );
  mDiagramTypeComboBox->addItem( pix, tr( "Pie chart" ), DIAGRAM_NAME_PIE );
  pix = QgsApplication::getThemePixmap( QStringLiteral( "text" ) );
  mDiagramTypeComboBox->addItem( pix, tr( "Text diagram" ), DIAGRAM_NAME_TEXT );
  pix = QgsApplication::getThemePixmap( QStringLiteral( "histogram" ) );
  mDiagramTypeComboBox->addItem( pix, tr( "Histogram" ), DIAGRAM_NAME_HISTOGRAM );
  mDiagramTypeComboBox->blockSignals( false );

  mLegendType->addItem( tr( "Concentric symbols" ), QVariant(( int ) QgsDiagramSettings::ConcentricBottom ) );
  mLegendType->addItem( tr( "Concentric symbols middle" ), QVariant(( int ) QgsDiagramSettings::ConcentricCenter ) );
  mLegendType->addItem( tr( "Concentric symbols top" ), QVariant(( int ) QgsDiagramSettings::ConcentricTop ) );
  mLegendType->addItem( tr( "Multipple symbols" ), QVariant(( int ) QgsDiagramSettings::Multiple ) );

  mScaleRangeWidget->setMapCanvas( mMapCanvas );
  mSizeFieldExpressionWidget->registerExpressionContextGenerator( this );

  mBackgroundColorButton->setColorDialogTitle( tr( "Select background color" ) );
  mBackgroundColorButton->setAllowAlpha( true );
  mBackgroundColorButton->setContext( QStringLiteral( "symbology" ) );
  mBackgroundColorButton->setShowNoColor( true );
  mBackgroundColorButton->setNoColorString( tr( "Transparent background" ) );
  mDiagramPenColorButton->setColorDialogTitle( tr( "Select pen color" ) );
  mDiagramPenColorButton->setAllowAlpha( true );
  mDiagramPenColorButton->setContext( QStringLiteral( "symbology" ) );
  mDiagramPenColorButton->setShowNoColor( true );
  mDiagramPenColorButton->setNoColorString( tr( "Transparent stroke" ) );

  mMaxValueSpinBox->setShowClearButton( false );

  mDiagramAttributesTreeWidget->setItemDelegateForColumn( ColumnAttributeExpression, new EditBlockerDelegate( this ) );
  mDiagramAttributesTreeWidget->setItemDelegateForColumn( ColumnColor, new EditBlockerDelegate( this ) );

  connect( mFixedSizeRadio, &QRadioButton::toggled, this, &QgsDiagramProperties::scalingTypeChanged );
  connect( mAttributeBasedScalingRadio, &QRadioButton::toggled, this, &QgsDiagramProperties::scalingTypeChanged );

  mDiagramUnitComboBox->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                  << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mDiagramLineUnitComboBox->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                      << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  QgsWkbTypes::GeometryType layerType = layer->geometryType();
  if ( layerType == QgsWkbTypes::UnknownGeometry || layerType == QgsWkbTypes::NullGeometry )
  {
    mDiagramTypeComboBox->setEnabled( false );
    mDiagramFrame->setEnabled( false );
  }

  //insert placement options
  mPlacementComboBox->blockSignals( true );
  switch ( layerType )
  {
    case QgsWkbTypes::PointGeometry:
      mPlacementComboBox->addItem( tr( "Around Point" ), QgsDiagramLayerSettings::AroundPoint );
      mPlacementComboBox->addItem( tr( "Over Point" ), QgsDiagramLayerSettings::OverPoint );
      mLinePlacementFrame->setVisible( false );
      break;
    case QgsWkbTypes::LineGeometry:
      mPlacementComboBox->addItem( tr( "Around Line" ), QgsDiagramLayerSettings::Line );
      mPlacementComboBox->addItem( tr( "Over Line" ), QgsDiagramLayerSettings::Horizontal );
      mLinePlacementFrame->setVisible( true );
      break;
    case QgsWkbTypes::PolygonGeometry:
      mPlacementComboBox->addItem( tr( "Around Centroid" ), QgsDiagramLayerSettings::AroundPoint );
      mPlacementComboBox->addItem( tr( "Over Centroid" ), QgsDiagramLayerSettings::OverPoint );
      mPlacementComboBox->addItem( tr( "Perimeter" ), QgsDiagramLayerSettings::Line );
      mPlacementComboBox->addItem( tr( "Inside Polygon" ), QgsDiagramLayerSettings::Horizontal );
      mLinePlacementFrame->setVisible( false );
      break;
    default:
      break;
  }
  mPlacementComboBox->blockSignals( false );

  mLabelPlacementComboBox->addItem( tr( "Height" ), QgsDiagramSettings::Height );
  mLabelPlacementComboBox->addItem( tr( "x-height" ), QgsDiagramSettings::XHeight );

  mScaleDependencyComboBox->addItem( tr( "Area" ), true );
  mScaleDependencyComboBox->addItem( tr( "Diameter" ), false );

  mAngleOffsetComboBox->addItem( tr( "Top" ), 90 * 16 );
  mAngleOffsetComboBox->addItem( tr( "Right" ), 0 );
  mAngleOffsetComboBox->addItem( tr( "Bottom" ), 270 * 16 );
  mAngleOffsetComboBox->addItem( tr( "Left" ), 180 * 16 );

  QgsSettings settings;

  // reset horiz stretch of left side of options splitter (set to 1 for previewing in Qt Designer)
  QSizePolicy policy( mDiagramOptionsListFrame->sizePolicy() );
  policy.setHorizontalStretch( 0 );
  mDiagramOptionsListFrame->setSizePolicy( policy );
  if ( !settings.contains( QStringLiteral( "/Windows/Diagrams/OptionsSplitState" ) ) )
  {
    // set left list widget width on initial showing
    QList<int> splitsizes;
    splitsizes << 115;
    mDiagramOptionsSplitter->setSizes( splitsizes );
  }

  // restore dialog, splitters and current tab
  mDiagramOptionsSplitter->restoreState( settings.value( QStringLiteral( "/Windows/Diagrams/OptionsSplitState" ) ).toByteArray() );
  mDiagramOptionsListWidget->setCurrentRow( settings.value( QStringLiteral( "/Windows/Diagrams/Tab" ), 0 ).toInt() );

  // field combo and expression button
  mSizeFieldExpressionWidget->setLayer( mLayer );
  QgsDistanceArea myDa;
  myDa.setSourceCrs( mLayer->crs() );
  myDa.setEllipsoidalMode( true );
  myDa.setEllipsoid( QgsProject::instance()->ellipsoid() );
  mSizeFieldExpressionWidget->setGeomCalculator( myDa );

  //insert all attributes into the combo boxes
  const QgsFields &layerFields = layer->fields();
  for ( int idx = 0; idx < layerFields.count(); ++idx )
  {
    QTreeWidgetItem *newItem = new QTreeWidgetItem( mAttributesTreeWidget );
    QString name = QStringLiteral( "\"%1\"" ).arg( layerFields.at( idx ).name() );
    newItem->setText( 0, name );
    newItem->setData( 0, RoleAttributeExpression, name );
    newItem->setFlags( newItem->flags() & ~Qt::ItemIsDropEnabled );
  }

  const QgsDiagramRenderer *dr = layer->diagramRenderer();
  if ( !dr ) //no diagram renderer yet, insert reasonable default
  {
    mDiagramTypeComboBox->blockSignals( true );
    mDiagramTypeComboBox->setCurrentIndex( 0 );
    mDiagramTypeComboBox->blockSignals( false );
    mFixedSizeRadio->setChecked( true );
    mDiagramUnitComboBox->setUnit( QgsUnitTypes::RenderMillimeters );
    mDiagramLineUnitComboBox->setUnit( QgsUnitTypes::RenderMillimeters );
    mLabelPlacementComboBox->setCurrentIndex( mLabelPlacementComboBox->findText( tr( "x-height" ) ) );
    mDiagramSizeSpinBox->setEnabled( true );
    mDiagramSizeSpinBox->setValue( 15 );
    mLinearScaleFrame->setEnabled( false );
    mIncreaseMinimumSizeSpinBox->setEnabled( false );
    mIncreaseMinimumSizeLabel->setEnabled( false );
    mBarWidthSpinBox->setValue( 5 );
    mScaleVisibilityGroupBox->setChecked( layer->hasScaleBasedVisibility() );
    mScaleRangeWidget->setScaleRange( 1.0 / layer->maximumScale(), 1.0 / layer->minimumScale() ); // caution: layer uses scale denoms, widget uses true scales
    mShowAllCheckBox->setChecked( true );
    mCheckBoxAttributeLegend->setChecked( true );
    mCheckBoxSizeLegend->setChecked( false );
    mSizeLegendSymbol.reset( QgsMarkerSymbol::createSimple( QgsStringMap() ) );

    switch ( layerType )
    {
      case QgsWkbTypes::PointGeometry:
        mPlacementComboBox->setCurrentIndex( mPlacementComboBox->findData( QgsDiagramLayerSettings::AroundPoint ) );
        break;
      case QgsWkbTypes::LineGeometry:
        mPlacementComboBox->setCurrentIndex( mPlacementComboBox->findData( QgsDiagramLayerSettings::Line ) );
        chkLineAbove->setChecked( true );
        chkLineBelow->setChecked( false );
        chkLineOn->setChecked( false );
        chkLineOrientationDependent->setChecked( false );
        break;
      case QgsWkbTypes::PolygonGeometry:
        mPlacementComboBox->setCurrentIndex( mPlacementComboBox->findData( QgsDiagramLayerSettings::AroundPoint ) );
        break;
      case QgsWkbTypes::UnknownGeometry:
      case QgsWkbTypes::NullGeometry:
        break;
    }
    mBackgroundColorButton->setColor( QColor( 255, 255, 255, 255 ) );
    //force a refresh of widget status to match diagram type
    on_mDiagramTypeComboBox_currentIndexChanged( mDiagramTypeComboBox->currentIndex() );
  }
  else // already a diagram renderer present
  {
    //single category renderer or interpolated one?
    if ( dr->rendererName() == QLatin1String( "SingleCategory" ) )
    {
      mFixedSizeRadio->setChecked( true );
    }
    else
    {
      mAttributeBasedScalingRadio->setChecked( true );
    }
    mDiagramSizeSpinBox->setEnabled( mFixedSizeRadio->isChecked() );
    mLinearScaleFrame->setEnabled( mAttributeBasedScalingRadio->isChecked() );
    mCheckBoxAttributeLegend->setChecked( dr->attributeLegend() );
    mCheckBoxSizeLegend->setChecked( dr->sizeLegend() );
    mSizeLegendSymbol.reset( dr->sizeLegendSymbol() ? dr->sizeLegendSymbol()->clone() : QgsMarkerSymbol::createSimple( QgsStringMap() ) );
    QIcon icon = QgsSymbolLayerUtils::symbolPreviewIcon( mSizeLegendSymbol.get(), mButtonSizeLegendSymbol->iconSize() );
    mButtonSizeLegendSymbol->setIcon( icon );
    mSizeLabel->setText( dr->sizeLabel() );
    mSizeRulesTable->clear();


    //assume single category or linearly interpolated diagram renderer for now
    QList<QgsDiagramSettings> settingList = dr->diagramSettings();
    if ( !settingList.isEmpty() )
    {
      mDiagramFrame->setEnabled( settingList.at( 0 ).enabled );
      mDiagramFont = settingList.at( 0 ).font;
      QSizeF size = settingList.at( 0 ).size;
      mBackgroundColorButton->setColor( settingList.at( 0 ).backgroundColor );
      mTransparencySpinBox->setValue( settingList.at( 0 ).transparency * 100.0 / 255.0 );
      mTransparencySlider->setValue( mTransparencySpinBox->value() );
      mDiagramPenColorButton->setColor( settingList.at( 0 ).penColor );
      mPenWidthSpinBox->setValue( settingList.at( 0 ).penWidth );
      mDiagramSizeSpinBox->setValue( ( size.width() + size.height() ) / 2.0 );
      // caution: layer uses scale denoms, widget uses true scales
      mScaleRangeWidget->setScaleRange( 1.0 / ( settingList.at( 0 ).maxScaleDenominator > 0 ? settingList.at( 0 ).maxScaleDenominator : layer->maximumScale() ),
                                        1.0 / ( settingList.at( 0 ).minScaleDenominator > 0 ? settingList.at( 0 ).minScaleDenominator : layer->minimumScale() ) );
      mScaleVisibilityGroupBox->setChecked( settingList.at( 0 ).scaleBasedVisibility );
      mDiagramUnitComboBox->setUnit( settingList.at( 0 ).sizeType );
      mDiagramUnitComboBox->setMapUnitScale( settingList.at( 0 ).sizeScale );
      mDiagramLineUnitComboBox->setUnit( settingList.at( 0 ).lineSizeUnit );
      mDiagramLineUnitComboBox->setMapUnitScale( settingList.at( 0 ).lineSizeScale );
      mSizeAttributeLegend->setText( settingList.at( 0 ).sizeAttributeLabel );
      mLegendType->setCurrentIndex( mLegendType->findData( QVariant( settingList.at( 0 ).sizeDiagramLegendType ) ) );
      QLocale locale;
      foreach ( double rule, settingList.at( 0 ).sizeRules )
      {
        QListWidgetItem* item = new QListWidgetItem( locale.toString( rule ), mSizeRulesTable );
        item->setFlags( item->flags() | Qt::ItemIsEditable );
      }


      if ( settingList.at( 0 ).labelPlacementMethod == QgsDiagramSettings::Height )
      {
        mLabelPlacementComboBox->setCurrentIndex( 0 );
      }
      else
      {
        mLabelPlacementComboBox->setCurrentIndex( 1 );
      }

      mAngleOffsetComboBox->setCurrentIndex( mAngleOffsetComboBox->findData( settingList.at( 0 ).angleOffset ) );

      mOrientationLeftButton->setProperty( "direction", QgsDiagramSettings::Left );
      mOrientationRightButton->setProperty( "direction", QgsDiagramSettings::Right );
      mOrientationUpButton->setProperty( "direction", QgsDiagramSettings::Up );
      mOrientationDownButton->setProperty( "direction", QgsDiagramSettings::Down );
      switch ( settingList.at( 0 ).diagramOrientation )
      {
        case QgsDiagramSettings::Left:
          mOrientationLeftButton->setChecked( true );
          break;

        case QgsDiagramSettings::Right:
          mOrientationRightButton->setChecked( true );
          break;

        case QgsDiagramSettings::Up:
          mOrientationUpButton->setChecked( true );
          break;

        case QgsDiagramSettings::Down:
          mOrientationDownButton->setChecked( true );
          break;
      }

      mBarWidthSpinBox->setValue( settingList.at( 0 ).barWidth );

      mIncreaseSmallDiagramsCheck->setChecked( settingList.at( 0 ).minimumSize != 0 );
      mIncreaseMinimumSizeSpinBox->setEnabled( mIncreaseSmallDiagramsCheck->isChecked() );
      mIncreaseMinimumSizeLabel->setEnabled( mIncreaseSmallDiagramsCheck->isChecked() );

      mIncreaseMinimumSizeSpinBox->setValue( settingList.at( 0 ).minimumSize );

      if ( settingList.at( 0 ).scaleByArea )
      {
        mScaleDependencyComboBox->setCurrentIndex( 0 );
      }
      else
      {
        mScaleDependencyComboBox->setCurrentIndex( 1 );
      }

      QList< QColor > categoryColors = settingList.at( 0 ).categoryColors;
      QList< QString > categoryAttributes = settingList.at( 0 ).categoryAttributes;
      QList< QString > categoryLabels = settingList.at( 0 ).categoryLabels;
      QList< QString >::const_iterator catIt = categoryAttributes.constBegin();
      QList< QColor >::const_iterator coIt = categoryColors.constBegin();
      QList< QString >::const_iterator labIt = categoryLabels.constBegin();
      for ( ; catIt != categoryAttributes.constEnd(); ++catIt, ++coIt, ++labIt )
      {
        QTreeWidgetItem *newItem = new QTreeWidgetItem( mDiagramAttributesTreeWidget );
        newItem->setText( 0, *catIt );
        newItem->setData( 0, RoleAttributeExpression, *catIt );
        newItem->setFlags( newItem->flags() & ~Qt::ItemIsDropEnabled );
        QColor col( *coIt );
        col.setAlpha( 255 );
        newItem->setBackground( 1, QBrush( col ) );
        newItem->setText( 2, *labIt );
        newItem->setFlags( newItem->flags() | Qt::ItemIsEditable );
      }
    }

    if ( dr->rendererName() == QLatin1String( "LinearlyInterpolated" ) )
    {
      const QgsLinearlyInterpolatedDiagramRenderer *lidr = dynamic_cast<const QgsLinearlyInterpolatedDiagramRenderer *>( dr );
      if ( lidr )
      {
        mDiagramSizeSpinBox->setEnabled( false );
        mLinearScaleFrame->setEnabled( true );
        mMaxValueSpinBox->setValue( lidr->upperValue() );
        mSizeSpinBox->setValue( ( lidr->upperSize().width() + lidr->upperSize().height() ) / 2 );
        if ( lidr->classificationAttributeIsExpression() )
        {
          mSizeFieldExpressionWidget->setField( lidr->classificationAttributeExpression() );
        }
        else
        {
          mSizeFieldExpressionWidget->setField( lidr->classificationField() );
        }
      }
    }

    const QgsDiagramLayerSettings *dls = layer->diagramLayerSettings();
    if ( dls )
    {
      mDiagramDistanceSpinBox->setValue( dls->distance() );
      mPrioritySlider->setValue( dls->priority() );
      mZIndexSpinBox->setValue( dls->zIndex() );
      mPlacementComboBox->setCurrentIndex( mPlacementComboBox->findData( dls->placement() ) );

      chkLineAbove->setChecked( dls->linePlacementFlags() & QgsDiagramLayerSettings::AboveLine );
      chkLineBelow->setChecked( dls->linePlacementFlags() & QgsDiagramLayerSettings::BelowLine );
      chkLineOn->setChecked( dls->linePlacementFlags() & QgsDiagramLayerSettings::OnLine );
      if ( !( dls->linePlacementFlags() & QgsDiagramLayerSettings::MapOrientation ) )
        chkLineOrientationDependent->setChecked( true );

      mShowAllCheckBox->setChecked( dls->showAllDiagrams() );

      mDataDefinedProperties = dls->dataDefinedProperties();
    }

    if ( dr->diagram() )
    {
      mDiagramType = dr->diagram()->diagramName();

      mDiagramTypeComboBox->blockSignals( true );
      mDiagramTypeComboBox->setCurrentIndex( settingList.at( 0 ).enabled ? mDiagramTypeComboBox->findData( mDiagramType ) : 0 );
      mDiagramTypeComboBox->blockSignals( false );
      //force a refresh of widget status to match diagram type
      on_mDiagramTypeComboBox_currentIndexChanged( mDiagramTypeComboBox->currentIndex() );
      if ( mDiagramTypeComboBox->currentIndex() == -1 )
      {
        QMessageBox::warning( this, tr( "Unknown diagram type." ),
                              tr( "The diagram type '%1' is unknown. A default type is selected for you." ).arg( mDiagramType ), QMessageBox::Ok );
        mDiagramTypeComboBox->setCurrentIndex( mDiagramTypeComboBox->findData( DIAGRAM_NAME_PIE ) );
      }
    }
  }

  connect( mAddAttributeExpression, &QPushButton::clicked, this, &QgsDiagramProperties::showAddAttributeExpressionDialog );
  connect( mTransparencySlider, &QSlider::valueChanged, mTransparencySpinBox, &QgsSpinBox::setValue );
  connect( mTransparencySpinBox, static_cast < void ( QgsSpinBox::* )( int ) > ( &QSpinBox::valueChanged ), mTransparencySlider, &QSlider::setValue );

  registerDataDefinedButton( mBackgroundColorDDBtn, QgsDiagramLayerSettings::BackgroundColor );
  registerDataDefinedButton( mLineColorDDBtn, QgsDiagramLayerSettings::StrokeColor );
  registerDataDefinedButton( mLineWidthDDBtn, QgsDiagramLayerSettings::StrokeWidth );
  registerDataDefinedButton( mCoordXDDBtn, QgsDiagramLayerSettings::PositionX );
  registerDataDefinedButton( mCoordYDDBtn, QgsDiagramLayerSettings::PositionY );
  registerDataDefinedButton( mDistanceDDBtn, QgsDiagramLayerSettings::Distance );
  registerDataDefinedButton( mPriorityDDBtn, QgsDiagramLayerSettings::Priority );
  registerDataDefinedButton( mZOrderDDBtn, QgsDiagramLayerSettings::ZIndex );
  registerDataDefinedButton( mShowDiagramDDBtn, QgsDiagramLayerSettings::Show );
  registerDataDefinedButton( mAlwaysShowDDBtn, QgsDiagramLayerSettings::AlwaysShow );
  registerDataDefinedButton( mIsObstacleDDBtn, QgsDiagramLayerSettings::IsObstacle );
  registerDataDefinedButton( mStartAngleDDBtn, QgsDiagramLayerSettings::StartAngle );
}

QgsDiagramProperties::~QgsDiagramProperties()
{
  QgsSettings settings;
  settings.setValue( QStringLiteral( "/Windows/Diagrams/OptionsSplitState" ), mDiagramOptionsSplitter->saveState() );
  settings.setValue( QStringLiteral( "/Windows/Diagrams/Tab" ), mDiagramOptionsListWidget->currentRow() );
}

void QgsDiagramProperties::registerDataDefinedButton( QgsPropertyOverrideButton *button, QgsDiagramLayerSettings::Property key )
{
  button->init( key, mDataDefinedProperties, QgsDiagramLayerSettings::propertyDefinitions(), mLayer );
  connect( button, &QgsPropertyOverrideButton::changed, this, &QgsDiagramProperties::updateProperty );
  button->registerExpressionContextGenerator( this );
}

void QgsDiagramProperties::updateProperty()
{
  QgsPropertyOverrideButton *button = qobject_cast<QgsPropertyOverrideButton *>( sender() );
  QgsDiagramLayerSettings::Property key = static_cast<  QgsDiagramLayerSettings::Property >( button->propertyKey() );
  mDataDefinedProperties.setProperty( key, button->toProperty() );
}

void QgsDiagramProperties::on_mDiagramTypeComboBox_currentIndexChanged( int index )
{
  if ( index == 0 )
  {
    mDiagramFrame->setEnabled( false );
  }
  else
  {
    mDiagramFrame->setEnabled( true );

    mDiagramType = mDiagramTypeComboBox->itemData( index ).toString();

    if ( DIAGRAM_NAME_TEXT == mDiagramType )
    {
      mTextOptionsFrame->show();
      mBackgroundColorLabel->show();
      mBackgroundColorButton->show();
      mBackgroundColorDDBtn->show();
      mDiagramFontButton->show();
    }
    else
    {
      mTextOptionsFrame->hide();
      mBackgroundColorLabel->hide();
      mBackgroundColorButton->hide();
      mBackgroundColorDDBtn->hide();
      mDiagramFontButton->hide();
    }

    if ( DIAGRAM_NAME_HISTOGRAM == mDiagramType )
    {
      mBarWidthLabel->show();
      mBarWidthSpinBox->show();
      mBarOptionsFrame->show();
      mAttributeBasedScalingRadio->setChecked( true );
      mFixedSizeRadio->setEnabled( false );
      mDiagramSizeSpinBox->setEnabled( false );
      mLinearlyScalingLabel->setText( tr( "Bar length: Scale linearly, so that the following value matches the specified bar length:" ) );
      mSizeLabel->setText( tr( "Bar length" ) );
      mFrameIncreaseSize->setVisible( false );
    }
    else
    {
      mBarWidthLabel->hide();
      mBarWidthSpinBox->hide();
      mBarOptionsFrame->hide();
      mLinearlyScalingLabel->setText( tr( "Scale linearly between 0 and the following attribute value / diagram size:" ) );
      mSizeLabel->setText( tr( "Size" ) );
      mAttributeBasedScalingRadio->setEnabled( true );
      mFixedSizeRadio->setEnabled( true );
      mDiagramSizeSpinBox->setEnabled( mFixedSizeRadio->isChecked() );
      mFrameIncreaseSize->setVisible( true );
    }

    if ( DIAGRAM_NAME_TEXT == mDiagramType || DIAGRAM_NAME_PIE == mDiagramType )
    {
      mScaleDependencyComboBox->show();
      mScaleDependencyLabel->show();
    }
    else
    {
      mScaleDependencyComboBox->hide();
      mScaleDependencyLabel->hide();
    }

    if ( DIAGRAM_NAME_PIE == mDiagramType )
    {
      mAngleOffsetComboBox->show();
      mAngleOffsetLabel->show();
      mStartAngleDDBtn->show();
    }
    else
    {
      mAngleOffsetComboBox->hide();
      mAngleOffsetLabel->hide();
      mStartAngleDDBtn->hide();
    }
  }
}
QString QgsDiagramProperties::guessLegendText( const QString &expression )
{
  //trim unwanted characters from expression text for legend
  QString text = expression.mid( expression.startsWith( '\"' ) ? 1 : 0 );
  if ( text.endsWith( '\"' ) )
    text.chop( 1 );
  return text;
}

void QgsDiagramProperties::addAttribute( QTreeWidgetItem *item )
{
  QTreeWidgetItem *newItem = new QTreeWidgetItem( mDiagramAttributesTreeWidget );

  newItem->setText( 0, item->text( 0 ) );
  newItem->setText( 2, guessLegendText( item->text( 0 ) ) );
  newItem->setData( 0, RoleAttributeExpression, item->data( 0, RoleAttributeExpression ) );
  newItem->setFlags( ( newItem->flags() | Qt::ItemIsEditable ) & ~Qt::ItemIsDropEnabled );

  //set initial color for diagram category
  int red = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
  int green = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
  int blue = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
  QColor randomColor( red, green, blue );
  newItem->setBackground( 1, QBrush( randomColor ) );
  mDiagramAttributesTreeWidget->addTopLevelItem( newItem );
}

void QgsDiagramProperties::on_mAddCategoryPushButton_clicked()
{
  Q_FOREACH ( QTreeWidgetItem *attributeItem, mAttributesTreeWidget->selectedItems() )
  {
    addAttribute( attributeItem );
  }
}

void QgsDiagramProperties::on_mAttributesTreeWidget_itemDoubleClicked( QTreeWidgetItem *item, int column )
{
  Q_UNUSED( column );
  addAttribute( item );
}

void QgsDiagramProperties::on_mRemoveCategoryPushButton_clicked()
{
  Q_FOREACH ( QTreeWidgetItem *attributeItem, mDiagramAttributesTreeWidget->selectedItems() )
  {
    delete attributeItem;
  }
}

void QgsDiagramProperties::on_mFindMaximumValueButton_clicked()
{
  if ( !mLayer )
    return;

  float maxValue = 0.0;

  bool isExpression;
  QString sizeFieldNameOrExp = mSizeFieldExpressionWidget->currentField( &isExpression );
  if ( isExpression )
  {
    QgsExpression exp( sizeFieldNameOrExp );
    QgsExpressionContext context;
    context << QgsExpressionContextUtils::globalScope()
            << QgsExpressionContextUtils::projectScope( QgsProject::instance() )
            << QgsExpressionContextUtils::mapSettingsScope( mMapCanvas->mapSettings() )
            << QgsExpressionContextUtils::layerScope( mLayer );

    exp.prepare( &context );
    if ( !exp.hasEvalError() )
    {
      QgsFeature feature;
      QgsFeatureIterator features = mLayer->getFeatures();
      while ( features.nextFeature( *&feature ) )
      {
        context.setFeature( feature );
        maxValue = qMax( maxValue, exp.evaluate( &context ).toFloat() );
      }
    }
    else
    {
      QgsDebugMsgLevel( "Prepare error:" + exp.evalErrorString(), 4 );
    }
  }
  else
  {
    int attributeNumber = mLayer->fields().lookupField( sizeFieldNameOrExp );
    maxValue = mLayer->maximumValue( attributeNumber ).toFloat();
  }

  mMaxValueSpinBox->setValue( maxValue );
}

void QgsDiagramProperties::on_mDiagramFontButton_clicked()
{
  bool ok;
  mDiagramFont = QgisGui::getFont( ok, mDiagramFont );
}


void QgsDiagramProperties::on_mDiagramAttributesTreeWidget_itemDoubleClicked( QTreeWidgetItem *item, int column )
{
  switch ( column )
  {
    case ColumnAttributeExpression:
    {
      QString currentExpression = item->data( 0, RoleAttributeExpression ).toString();

      QString newExpression = showExpressionBuilder( currentExpression );
      if ( !newExpression.isEmpty() )
      {
        item->setData( 0, Qt::DisplayRole, newExpression );
        item->setData( 0, RoleAttributeExpression, newExpression );
      }
      break;
    }

    case ColumnColor:
    {
      QColor newColor = QgsColorDialog::getColor( item->background( 1 ).color(), nullptr );
      if ( newColor.isValid() )
      {
        item->setBackground( 1, QBrush( newColor ) );
      }
      break;
    }

    case ColumnLegendText:
      break;
  }
}

void QgsDiagramProperties::on_mEngineSettingsButton_clicked()
{
  QgsLabelEngineConfigDialog dlg( this );
  dlg.exec();
}

void QgsDiagramProperties::apply()
{
  int index = mDiagramTypeComboBox->currentIndex();
  bool diagramsEnabled = ( index != 0 );

  QgsDiagram *diagram = nullptr;

  if ( diagramsEnabled && 0 == mDiagramAttributesTreeWidget->topLevelItemCount() )
  {
    QgisApp::instance()->messageBar()->pushMessage(
      tr( "Diagrams: No attributes added." ),
      tr( "You did not add any attributes to this diagram layer. Please specify the attributes to visualize on the diagrams or disable diagrams." ),
      QgsMessageBar::WARNING );
  }

#if 0
  bool scaleAttributeValueOk = false;
  // Check if a (usable) scale attribute value is inserted
  mValueLineEdit->text().toDouble( &scaleAttributeValueOk );

  if ( !mFixedSizeRadio->isChecked() && !scaleAttributeValueOk )
  {
    double maxVal = DBL_MIN;
    QgsVectorDataProvider *provider = mLayer->dataProvider();

    if ( provider )
    {
      if ( diagramType == DIAGRAM_NAME_HISTOGRAM )
      {
        // Find maximum value
        for ( int i = 0; i < mDiagramAttributesTreeWidget->topLevelItemCount(); ++i )
        {
          QString fldName = mDiagramAttributesTreeWidget->topLevelItem( i )->data( 0, Qt::UserRole ).toString();
          if ( fldName.count() >= 2 && fldName.at( 0 ) == '"' && fldName.at( fldName.count() - 1 ) == '"' )
            fldName = fldName.mid( 1, fldName.count() - 2 ); // remove enclosing double quotes
          int fld = provider->fieldNameIndex( fldName );
          if ( fld != -1 )
          {
            bool ok = false;
            double val = provider->maximumValue( fld ).toDouble( &ok );
            if ( ok )
              maxVal = qMax( maxVal, val );
          }
        }
      }
      else
      {
        maxVal = provider->maximumValue( mSizeAttributeComboBox->currentData().toInt() ).toDouble();
      }
    }

    if ( diagramsEnabled && maxVal != DBL_MIN )
    {
      QgisApp::instance()->messageBar()->pushMessage(
        tr( "Interpolation value" ),
        tr( "You did not specify an interpolation value. A default value of %1 has been set." ).arg( QString::number( maxVal ) ),
        QgsMessageBar::INFO,
        5 );

      mMaxValueSpinBox->setValue( maxVal );
    }
  }
#endif

  if ( mDiagramType == DIAGRAM_NAME_TEXT )
  {
    diagram = new QgsTextDiagram();
  }
  else if ( mDiagramType == DIAGRAM_NAME_PIE )
  {
    diagram = new QgsPieDiagram();
  }
  else // if ( diagramType == DIAGRAM_NAME_HISTOGRAM )
  {
    diagram = new QgsHistogramDiagram();
  }

  QgsDiagramSettings ds;
  ds.enabled = ( mDiagramTypeComboBox->currentIndex() != 0 );
  ds.font = mDiagramFont;
  ds.transparency = mTransparencySpinBox->value() * 255.0 / 100.0;

  QList<QColor> categoryColors;
  QList<QString> categoryAttributes;
  QList<QString> categoryLabels;
  categoryColors.reserve( mDiagramAttributesTreeWidget->topLevelItemCount() );
  categoryAttributes.reserve( mDiagramAttributesTreeWidget->topLevelItemCount() );
  categoryLabels.reserve( mDiagramAttributesTreeWidget->topLevelItemCount() );
  for ( int i = 0; i < mDiagramAttributesTreeWidget->topLevelItemCount(); ++i )
  {
    QColor color = mDiagramAttributesTreeWidget->topLevelItem( i )->background( 1 ).color();
    color.setAlpha( 255 - ds.transparency );
    categoryColors.append( color );
    categoryAttributes.append( mDiagramAttributesTreeWidget->topLevelItem( i )->data( 0, RoleAttributeExpression ).toString() );
    categoryLabels.append( mDiagramAttributesTreeWidget->topLevelItem( i )->text( 2 ) );
  }
  ds.categoryColors = categoryColors;
  ds.categoryAttributes = categoryAttributes;
  ds.categoryLabels = categoryLabels;
  ds.size = QSizeF( mDiagramSizeSpinBox->value(), mDiagramSizeSpinBox->value() );
  ds.sizeType = mDiagramUnitComboBox->unit();
  ds.sizeScale = mDiagramUnitComboBox->getMapUnitScale();
  ds.lineSizeUnit = mDiagramLineUnitComboBox->unit();
  ds.lineSizeScale = mDiagramLineUnitComboBox->getMapUnitScale();
  ds.labelPlacementMethod = static_cast<QgsDiagramSettings::LabelPlacementMethod>( mLabelPlacementComboBox->currentData().toInt() );
  ds.scaleByArea = mScaleDependencyComboBox->currentData().toBool();
  ds.sizeAttributeLabel = mSizeAttributeLegend->text();
  ds.sizeDiagramLegendType = static_cast<QgsDiagramSettings::SizeLegendType>( mLegendType->currentData().toInt() );

  ds.sizeRules.clear();
  QList<double> sizeRules;
  QLocale locale;
  for ( int i = 0; i < mSizeRulesTable->count(); ++i )
  {
    double sizeRule = locale.toDouble( mSizeRulesTable->item( i )->text() );
    ds.sizeRules.append( sizeRule );
    sizeRules.append( sizeRule );
  }
  qSort( ds.sizeRules );
  qSort( sizeRules );

  if ( mIncreaseSmallDiagramsCheck->isChecked() )
  {
    ds.minimumSize = mIncreaseMinimumSizeSpinBox->value();
  }
  else
  {
    ds.minimumSize = 0;
  }

  ds.backgroundColor = mBackgroundColorButton->color();
  ds.penColor = mDiagramPenColorButton->color();
  ds.penWidth = mPenWidthSpinBox->value();
  // caution: layer uses scale denoms, widget uses true scales
  ds.maxScaleDenominator = 1.0 / mScaleRangeWidget->minimumScale();
  ds.minScaleDenominator = 1.0 / mScaleRangeWidget->maximumScale();
  ds.scaleBasedVisibility = mScaleVisibilityGroupBox->isChecked();

  // Diagram angle offset (pie)
  ds.angleOffset = mAngleOffsetComboBox->currentData().toInt();

  // Diagram orientation (histogram)
  ds.diagramOrientation = static_cast<QgsDiagramSettings::DiagramOrientation>( mOrientationButtonGroup->checkedButton()->property( "direction" ).toInt() );

  ds.barWidth = mBarWidthSpinBox->value();

  QgsDiagramRenderer *renderer = nullptr;
  if ( mFixedSizeRadio->isChecked() )
  {
    QgsSingleCategoryDiagramRenderer *dr = new QgsSingleCategoryDiagramRenderer();
    dr->setDiagramSettings( ds );
    renderer = dr;
  }
  else
  {
    QgsLinearlyInterpolatedDiagramRenderer *dr = new QgsLinearlyInterpolatedDiagramRenderer();
    dr->setLowerValue( 0.0 );
    dr->setLowerSize( QSizeF( 0.0, 0.0 ) );
    dr->setUpperValue( mMaxValueSpinBox->value() );
    dr->setUpperSize( QSizeF( mSizeSpinBox->value(), mSizeSpinBox->value() ) );

    bool isExpression;
    QString sizeFieldNameOrExp = mSizeFieldExpressionWidget->currentField( &isExpression );
    dr->setClassificationAttributeIsExpression( isExpression );
    if ( isExpression )
    {
      dr->setClassificationAttributeExpression( sizeFieldNameOrExp );
    }
    else
    {
      dr->setClassificationField( sizeFieldNameOrExp );
    }
    dr->setDiagramSettings( ds );
    renderer = dr;
  }
  renderer->setDiagram( diagram );
  renderer->setAttributeLegend( mCheckBoxAttributeLegend->isChecked() );
  renderer->setSizeLegend( mCheckBoxSizeLegend->isChecked() );
  renderer->setSizeLegendSymbol( mSizeLegendSymbol->clone() );
  renderer->setSizeLabel( mSizeLabel->text() );
  renderer->setSizeRules( sizeRules );
  mLayer->setDiagramRenderer( renderer );

  QgsDiagramLayerSettings dls;
  dls.setDataDefinedProperties( mDataDefinedProperties );
  dls.setDistance( mDiagramDistanceSpinBox->value() );
  dls.setPriority( mPrioritySlider->value() );
  dls.setZIndex( mZIndexSpinBox->value() );
  dls.setShowAllDiagrams( mShowAllCheckBox->isChecked() );
  dls.setPlacement( ( QgsDiagramLayerSettings::Placement )mPlacementComboBox->currentData().toInt() );

  QgsDiagramLayerSettings::LinePlacementFlags flags = 0;
  if ( chkLineAbove->isChecked() )
    flags |= QgsDiagramLayerSettings::AboveLine;
  if ( chkLineBelow->isChecked() )
    flags |= QgsDiagramLayerSettings::BelowLine;
  if ( chkLineOn->isChecked() )
    flags |= QgsDiagramLayerSettings::OnLine;
  if ( ! chkLineOrientationDependent->isChecked() )
    flags |= QgsDiagramLayerSettings::MapOrientation;
  dls.setLinePlacementFlags( flags );

  mLayer->setDiagramLayerSettings( dls );

  // refresh
  QgsProject::instance()->setDirty( true );
  mLayer->triggerRepaint();
}

QString QgsDiagramProperties::showExpressionBuilder( const QString &initialExpression )
{
  QgsExpressionContext context;
  context << QgsExpressionContextUtils::globalScope()
          << QgsExpressionContextUtils::projectScope( QgsProject::instance() )
          << QgsExpressionContextUtils::atlasScope( nullptr )
          << QgsExpressionContextUtils::mapSettingsScope( mMapCanvas->mapSettings() )
          << QgsExpressionContextUtils::layerScope( mLayer );

  QgsExpressionBuilderDialog dlg( mLayer, initialExpression, this, QStringLiteral( "generic" ), context );
  dlg.setWindowTitle( tr( "Expression based attribute" ) );

  QgsDistanceArea myDa;
  myDa.setSourceCrs( mLayer->crs() );
  myDa.setEllipsoidalMode( true );
  myDa.setEllipsoid( QgsProject::instance()->ellipsoid() );
  dlg.setGeomCalculator( myDa );

  if ( dlg.exec() == QDialog::Accepted )
  {
    return dlg.expressionText();
  }
  else
  {
    return QString();
  }
}

void QgsDiagramProperties::showAddAttributeExpressionDialog()
{
  QString expression;
  QList<QTreeWidgetItem *> selections = mAttributesTreeWidget->selectedItems();
  if ( !selections.empty() )
  {
    expression = selections[0]->text( 0 );
  }

  QString newExpression = showExpressionBuilder( expression );

  //Only add the expression if the user has entered some text.
  if ( !newExpression.isEmpty() )
  {
    QTreeWidgetItem *newItem = new QTreeWidgetItem( mDiagramAttributesTreeWidget );

    newItem->setText( 0, newExpression );
    newItem->setText( 2, newExpression );
    newItem->setData( 0, RoleAttributeExpression, newExpression );
    newItem->setFlags( ( newItem->flags() | Qt::ItemIsEditable ) & ~Qt::ItemIsDropEnabled );

    //set initial color for diagram category
    int red = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
    int green = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
    int blue = 1 + ( int )( 255.0 * qrand() / ( RAND_MAX + 1.0 ) );
    QColor randomColor( red, green, blue );
    newItem->setBackground( 1, QBrush( randomColor ) );
    mDiagramAttributesTreeWidget->addTopLevelItem( newItem );
  }
  activateWindow(); // set focus back parent
}

void QgsDiagramProperties::on_mDiagramStackedWidget_currentChanged( int index )
{
  mDiagramOptionsListWidget->blockSignals( true );
  mDiagramOptionsListWidget->setCurrentRow( index );
  mDiagramOptionsListWidget->blockSignals( false );
}

void QgsDiagramProperties::on_mPlacementComboBox_currentIndexChanged( int index )
{
  QgsDiagramLayerSettings::Placement currentPlacement = ( QgsDiagramLayerSettings::Placement )mPlacementComboBox->itemData( index ).toInt();
  if ( currentPlacement == QgsDiagramLayerSettings::AroundPoint ||
       ( currentPlacement == QgsDiagramLayerSettings::Line && mLayer->geometryType() == QgsWkbTypes::LineGeometry ) )
  {
    mDiagramDistanceLabel->setEnabled( true );
    mDiagramDistanceSpinBox->setEnabled( true );
  }
  else
  {
    mDiagramDistanceLabel->setEnabled( false );
    mDiagramDistanceSpinBox->setEnabled( false );
  }

  bool linePlacementEnabled = mLayer->geometryType() == QgsWkbTypes::LineGeometry && currentPlacement == QgsDiagramLayerSettings::Line;
  chkLineAbove->setEnabled( linePlacementEnabled );
  chkLineBelow->setEnabled( linePlacementEnabled );
  chkLineOn->setEnabled( linePlacementEnabled );
  chkLineOrientationDependent->setEnabled( linePlacementEnabled );
}

void QgsDiagramProperties::on_mButtonSizeLegendSymbol_clicked()
{
  QgsMarkerSymbol *newSymbol = mSizeLegendSymbol->clone();
  QgsSymbolWidgetContext context;
  context.setMapCanvas( mMapCanvas );
  QgsExpressionContext ec = createExpressionContext();
  context.setExpressionContext( &ec );

  QgsSymbolSelectorDialog d( newSymbol, QgsStyle::defaultStyle(), mLayer, this );
  d.setContext( context );

  if ( d.exec() == QDialog::Accepted )
  {
    mSizeLegendSymbol.reset( newSymbol );
    QIcon icon = QgsSymbolLayerUtils::symbolPreviewIcon( mSizeLegendSymbol.get(), mButtonSizeLegendSymbol->iconSize() );
    mButtonSizeLegendSymbol->setIcon( icon );
  }
  else
  {
    delete newSymbol;
  }
}

void QgsDiagramProperties::scalingTypeChanged()
{
  if ( !mAttributeBasedScalingRadio->isChecked() )
  {
    mCheckBoxSizeLegend->setChecked( false );
    mCheckBoxSizeLegend->setEnabled( false );
  }
  else
  {
    mCheckBoxSizeLegend->setEnabled( true );
  }
}


void QgsDiagramProperties::on_mAddButton_clicked()
{
  QListWidgetItem* item = new QListWidgetItem( "1", mSizeRulesTable );
  item->setFlags( item->flags() | Qt::ItemIsEditable );
}

void QgsDiagramProperties::on_mRemoveButton_clicked()
{
  qDeleteAll( mSizeRulesTable->selectedItems() );
}
