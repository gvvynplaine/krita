/*
 *  kis_paintop_box.cc - part of KImageShop/Krayon/Krita
 *
 *  Copyright (c) 2004 Boudewijn Rempt (boud@valdyas.org)
 *  Copyright (c) 2009-2011 Sven Langkamp (sven.langkamp@gmail.com)
 *  Copyright (c) 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  Copyright (C) 2011 Silvio Heinrich <plassy@web.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "kis_paintop_box.h"
#include <QWidget>
#include <QString>
#include <QPixmap>
#include <QLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QAction>

#include <kactioncollection.h>
#include <kis_debug.h>
#include <kglobal.h>
#include <klocale.h>
#include <kglobalsettings.h>
#include <kacceleratormanager.h>
#include <kconfig.h>
#include <kstandarddirs.h>
#include <kseparator.h>

#include <KoToolManager.h>
#include <KoColorSpace.h>
#include <KoResourceSelector.h>
#include <KoResourceServerAdapter.h>

#include <kis_paint_device.h>
#include <kis_cmb_composite.h>
#include <kis_paintop_registry.h>
#include <kis_canvas_resource_provider.h>
#include <kis_painter.h>
#include <kis_paintop.h>
#include <kis_layer.h>
#include <kis_resource_server_provider.h>
#include <kis_paintop_preset.h>
#include <kis_paintop_settings.h>
#include <kis_config_widget.h>
#include <kis_image.h>
#include <kis_node.h>

#include "kis_canvas2.h"
#include "kis_node_manager.h"
#include "kis_layer_manager.h"
#include "kis_view2.h"
#include "kis_factory2.h"
#include "widgets/kis_popup_button.h"
#include "widgets/kis_paintop_presets_popup.h"
#include "widgets/kis_paintop_presets_chooser_popup.h"
#include "widgets/kis_workspace_chooser.h"
#include "kis_paintop_settings_widget.h"
#include "ko_favorite_resource_manager.h"
#include <kis_cmb_paintop.h>
#include "kis_slider_spin_box.h"


KisPaintopBox::KisPaintopBox(KisView2 * view, QWidget *parent, const char * name)
        : QWidget(parent)
        , m_resourceProvider(view->resourceProvider())
        , m_optionWidget(0)
        , m_settingsWidget(0)
        , m_presetWidget(0)
        , m_brushChooser(0)
        , m_view(view)
        , m_activePreset(0)
        , m_compositeOp(0)
        , m_previousNode(0)
        , m_eraserUsed(false)
{
    Q_ASSERT(view != 0);

    KGlobal::mainComponent().dirs()->addResourceType("kis_defaultpresets", "data", "krita/defaultpresets/");

    setObjectName(name);

    KAcceleratorManager::setNoAccel(this);

    setWindowTitle(i18n("Painter's Toolchest"));

    m_settingsWidget = new KisPopupButton(this);
    m_settingsWidget->setIcon(KIcon("paintop_settings_02"));
    m_settingsWidget->setToolTip(i18n("Edit brush settings"));
    //m_settingsWidget->setText(i18n("Create Brush"));
    m_settingsWidget->setFixedSize(32, 32);

    m_presetWidget = new KisPopupButton(this);
    m_presetWidget->setIcon(KIcon("paintop_settings_01"));
    m_presetWidget->setToolTip(i18n("Choose brush preset"));
    //m_presetWidget->setText(i18n("Select Brush"));
    m_presetWidget->setFixedSize(32, 32);

    m_eraseModeButton = new QToolButton(this);
    m_eraseModeButton->setFixedSize(32, 32);
    m_eraseModeButton->setCheckable(true);
    KAction* eraseAction = new KAction(i18n("Set eraser mode"), m_eraseModeButton);
    eraseAction->setIcon(KIcon("draw-eraser"));
    eraseAction->setShortcut(Qt::Key_E);
    eraseAction->setCheckable(true);
    m_eraseModeButton->setDefaultAction(eraseAction);
    m_view->actionCollection()->addAction("erase_action", eraseAction);

    QToolButton* hMirrorButton = new QToolButton(this);
    hMirrorButton->setFixedSize(32, 32);
    hMirrorButton->setCheckable(true);
    KAction* hMirrorAction = new KAction(i18n("Set horizontal mirror mode"), hMirrorButton);
    hMirrorAction->setIcon(KIcon("object-flip-horizontal"));
//     hMirrorAction->setShortcut(Qt::Key_H);
    hMirrorAction->setCheckable(true);
    hMirrorButton->setDefaultAction(hMirrorAction);
    m_view->actionCollection()->addAction("hmirror_action", hMirrorAction);

    QToolButton* vMirrorButton = new QToolButton(this);
    vMirrorButton->setFixedSize(32, 32);
    vMirrorButton->setCheckable(true);
    KAction* vMirrorAction = new KAction(i18n("Set vertical mirror mode"), vMirrorButton);
    vMirrorAction->setIcon(KIcon("object-flip-vertical"));
//     vMirrorAction->setShortcut(Qt::Key_V);
    vMirrorAction->setCheckable(true);
    vMirrorButton->setDefaultAction(vMirrorAction);
    m_view->actionCollection()->addAction("vmirror_action", vMirrorAction);

    connect(eraseAction, SIGNAL(triggered(bool)), this, SLOT(eraseModeToggled(bool)));
    connect(hMirrorAction, SIGNAL(triggered(bool)), this, SLOT(slotHorizontalMirrorChanged(bool)));
    connect(vMirrorAction, SIGNAL(triggered(bool)), this, SLOT(slotVerticalMirrorChanged(bool)));

    QLabel* labelMode = new QLabel(i18n("Mode: "), this);
    labelMode->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_cmbComposite = new KisCmbComposite(this);
    nodeChanged(view->activeNode());
    connect(m_cmbComposite, SIGNAL(activated(const QString&)), this, SLOT(slotSetCompositeMode(const QString&)));

    
    QLabel* labelOpacity = new QLabel(i18n("Opacity: "), this);
    labelOpacity->setAlignment(Qt::AlignVCenter | Qt::AlignRight);
    m_sliderOpacity = new KisSliderSpinBox(this);
    m_sliderOpacity->setRange(0, 100);
    m_sliderOpacity->setValue(100);
    m_sliderOpacity->setMinimumWidth(150);
    connect(m_sliderOpacity, SIGNAL(valueChanged(int)), this, SLOT(slotOpacityChanged(int)));
//     m_brushChooser = new KisPopupButton(this);
//     //m_brushChooser->setIcon(KIcon("paintop_settings_01"));
//     m_brushChooser->setText(i18n("Brush Editor"));
//     m_brushChooser->setToolTip(i18n("Choose and edit brush"));

    m_paletteButton = new QPushButton(i18n("Save to Palette"));
    connect(m_paletteButton, SIGNAL(clicked()), this, SLOT(slotSaveToFavouriteBrushes()));
    //KAction *action  = new KAction(i18n("&Palette"), this);
    //view->actionCollection()->addAction("palette_manager", action);
    //action->setDefaultWidget(m_paletteButton);

    m_workspaceWidget = new KisPopupButton(this);
    m_workspaceWidget->setIcon(KIcon("document-multiple"));
    m_workspaceWidget->setToolTip(i18n("Choose workspace"));
    m_workspaceWidget->setFixedSize(32, 32);
    m_workspaceWidget->setPopupWidget(new KisWorkspaceChooser(view));
    
    QHBoxLayout* baseLayout = new QHBoxLayout(this);
    m_paintopWidget = new QWidget(this);
    baseLayout->addWidget(m_paintopWidget);

    m_layout = new QHBoxLayout(m_paintopWidget);
    m_layout->addWidget(m_settingsWidget);
    m_layout->addWidget(m_presetWidget);
    m_layout->addWidget(labelMode);
    m_layout->addWidget(m_cmbComposite);
    m_layout->addWidget(m_eraseModeButton);
    m_layout->addWidget(new KSeparator(Qt::Vertical, this));
    m_layout->addWidget(hMirrorButton);
    m_layout->addWidget(vMirrorButton);
    m_layout->addWidget(new KSeparator(Qt::Vertical, this));
    m_layout->addWidget(labelOpacity);
    m_layout->addWidget(m_sliderOpacity);
    m_layout->addWidget(m_paletteButton);
    m_layout->addSpacerItem(new QSpacerItem(10, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    m_layout->setContentsMargins(0, 0, 0, 0);

    baseLayout->addWidget(m_workspaceWidget);
    baseLayout->setContentsMargins(0, 0, 0, 0);

    m_presetsPopup = new KisPaintOpPresetsPopup(m_resourceProvider);
    m_settingsWidget->setPopupWidget(m_presetsPopup);
    m_presetsPopup->switchDetached();

    m_presetsChooserPopup = new KisPaintOpPresetsChooserPopup();
    m_presetWidget->setPopupWidget(m_presetsChooserPopup);

    m_colorspace = view->image()->colorSpace();

    updatePaintops();
    
    setCurrentPaintop(defaultPaintop(KoToolManager::instance()->currentInputDevice()));

    connect(m_presetsPopup, SIGNAL(paintopActivated(QString)), this, SLOT(slotSetPaintop(QString)));

    connect(m_presetsPopup, SIGNAL(savePresetClicked()), this, SLOT(slotSaveActivePreset()));

    connect(m_presetsPopup, SIGNAL(defaultPresetClicked()), this, SLOT(slotSetupDefaultPreset()));
    
    connect(m_presetsPopup, SIGNAL(presetNameLineEditChanged(QString)),
            this, SLOT(slotWatchPresetNameLineEdit(QString)));

    connect(m_presetsChooserPopup, SIGNAL(resourceSelected(KoResource*)),
            this, SLOT(resourceSelected(KoResource*)));

    connect(m_resourceProvider, SIGNAL(sigNodeChanged(const KisNodeSP)),
            this, SLOT(nodeChanged(const KisNodeSP)));
    
    connect(m_presetsChooserPopup, SIGNAL(resourceSelected(KoResource*)),
            m_presetsPopup, SLOT(resourceSelected(KoResource*)));

    //Needed to connect canvas to favoriate resource manager
    m_view->canvasBase()->createFavoriteResourceManager(this);
}

KisPaintopBox::~KisPaintopBox()
{
    // Do not delete the widget, since it it is global to the application, not owned by the view
    m_presetsPopup->setPaintOpSettingsWidget(0);
    qDeleteAll(m_paintopOptionWidgets);
}

KisPaintOpPresetSP KisPaintopBox::paintOpPresetSP(KoID* paintop)
{
    if (paintop == 0)
        return m_activePreset->clone();
    else if (!QString::compare(paintop->id(), "eraser", Qt::CaseInsensitive))
        return activePreset(*paintop, KoInputDevice::eraser());
    else
        return activePreset(*paintop, KoToolManager::instance()->currentInputDevice());
}

void KisPaintopBox::slotSetPaintop(const QString& paintOpId)
{
    if (KisPaintOpRegistry::instance()->get(paintOpId) != 0){
        setCurrentPaintop( KoID(paintOpId, KisPaintOpRegistry::instance()->get(paintOpId)->name()) );
    }
}


void KisPaintopBox::colorSpaceChanged(const KoColorSpace *cs)
{
    if (cs != m_colorspace) {
        m_colorspace = cs;
        updatePaintops();

        // TODO: not solved completly:
        // ensure the the right paintop is selected
        // It might happen that you must change the paintop as the current one is not supported
        // by the new colorspace.
        m_presetsPopup->setCurrentPaintOp( currentPaintop().id() );
        if (m_presetsPopup->currentPaintOp() != currentPaintop().id()){
            kWarning() << "PaintOp " << currentPaintop().name() << " was not selected, as it is not supported by colorspace " 
            << m_colorspace->name();
        }
    }
}

void KisPaintopBox::updatePaintops()
{
    /* get the list of the factories*/
    QList<QString> keys = KisPaintOpRegistry::instance()->keys();
    QList<KisPaintOpFactory*> factoryList;
    
    foreach(const QString & paintopId, keys) {
        KisPaintOpFactory * factory = KisPaintOpRegistry::instance()->get(paintopId);
        if (KisPaintOpRegistry::instance()->userVisible(KoID(factory->id(), factory->name()) ,m_colorspace)){
            factoryList.append(factory);
        }else{
            kWarning() << "Brush engine " << factory->name() << " is not visible for colorspace" << m_colorspace->name();
        }
    }
    
    m_presetsPopup->setPaintOpList(factoryList);
}

void KisPaintopBox::resourceSelected(KoResource* resource)
{
    KisPaintOpPreset* preset = static_cast<KisPaintOpPreset*>(resource);
    dbgUI << "preset " << preset->name() << "selected";
    if (!preset->settings()->isValid()) {
        return;
    }

    if(preset->paintOp() != currentPaintop()) {
        setCurrentPaintop(preset->paintOp());
    }

    m_optionWidget->setConfiguration(preset->settings());
    slotUpdatePreset();
}

QPixmap KisPaintopBox::paintopPixmap(const KoID & paintop)
{
    QString pixmapName = KisPaintOpRegistry::instance()->pixmap(paintop);

    if (pixmapName.isEmpty()) {
        return QPixmap();
    }

    QString fname = KisFactory2::componentData().dirs()->findResource("kis_images", pixmapName);

    return QPixmap(fname);
}

void KisPaintopBox::slotInputDeviceChanged(const KoInputDevice & inputDevice)
{
    KoID paintop;
    InputDevicePaintopMap::iterator it = m_currentID.find(inputDevice);

    if (it == m_currentID.end()) {
        paintop = defaultPaintop(inputDevice);
    } else {
        paintop = (*it);
    }

    m_presetsPopup->setCurrentPaintOp(paintop.id());
    if (m_presetsPopup->currentPaintOp() != paintop.id()){
        // Must change the paintop as the current one is not supported
        // by the new colorspace.
        paintop = KoID(m_presetsPopup->currentPaintOp(), KisPaintOpRegistry::instance()->get(m_presetsPopup->currentPaintOp())->name());
    }
    
    setCurrentPaintop(paintop);

    if(inputDevice.device() == QTabletEvent::Stylus && inputDevice.pointer() == QTabletEvent::Eraser && !m_eraserUsed) {
        m_inputDeviceEraseModes[inputDevice] = true;
        m_eraserUsed = true;
    }

    m_eraseModeButton->setChecked(m_inputDeviceEraseModes[KoToolManager::instance()->currentInputDevice()]);
    setCompositeOpInternal(m_inputDeviceCompositeModes[KoToolManager::instance()->currentInputDevice()]);
    updateCompositeOpComboBox();
}

void KisPaintopBox::slotCurrentNodeChanged(KisNodeSP node)
{
    for (InputDevicePresetsMap::iterator it = m_inputDevicePresets.begin();
            it != m_inputDevicePresets.end();
            ++it) {
        foreach(const KisPaintOpPresetSP & preset, it.value()) {
            if (preset && preset->settings()) {
                preset->settings()->setNode(node);
            }
        }
    }
}


const KoID& KisPaintopBox::currentPaintop()
{
    KoID id = m_currentID[KoToolManager::instance()->currentInputDevice()];
    return m_currentID[KoToolManager::instance()->currentInputDevice()];
}


void KisPaintopBox::setCurrentPaintop(const KoID & paintop)
{
    if (m_activePreset && m_optionWidget) {
        m_optionWidget->writeConfiguration(const_cast<KisPaintOpSettings*>(m_activePreset->settings().data()));
        m_optionWidget->disconnect(m_presetWidget);
        m_presetsPopup->setPaintOpSettingsWidget(0);
        m_optionWidget->hide();
    }

    m_currentID[KoToolManager::instance()->currentInputDevice()] = paintop;

    KisPaintOpPresetSP preset =
        activePreset(currentPaintop(), KoToolManager::instance()->currentInputDevice());

    if (preset != 0 && preset->settings()) {
        if (!m_paintopOptionWidgets.contains(paintop)) {
            m_paintopOptionWidgets[paintop] = KisPaintOpRegistry::instance()->get(paintop.id())->createSettingsWidget(this);
        }
        m_optionWidget = m_paintopOptionWidgets[paintop];
        m_optionWidget->setImage(m_view->image());
        m_optionWidget->writeConfiguration(const_cast<KisPaintOpSettings*>(preset->settings().data()));
        preset->settings()->setOptionsWidget(m_optionWidget);

        if (!preset->settings()->getProperties().isEmpty()) {
            m_optionWidget->setConfiguration(preset->settings());
        }
        m_presetsPopup->setPaintOpSettingsWidget(m_optionWidget);
        m_presetsChooserPopup->setPresetFilter(paintop);
        Q_ASSERT(m_optionWidget);
        Q_ASSERT(m_presetWidget);
        connect(m_optionWidget, SIGNAL(sigConfigurationUpdated()), this, SLOT(slotUpdatePreset()));
        m_presetsPopup->setPreset(preset);
        KisPaintOpFactory* paintOp = KisPaintOpRegistry::instance()->get(paintop.id());
        QString pixFilename = KisFactory2::componentData().dirs()->findResource("kis_images", paintOp->pixmap());
        m_settingsWidget->setIcon(QIcon(pixFilename));
    } else {
        m_presetsPopup->setPaintOpSettingsWidget(0);
    }

    preset->settings()->setNode(m_resourceProvider->currentNode());
    m_resourceProvider->slotPaintOpPresetActivated(preset);

    m_presetsPopup->setCurrentPaintOp(paintop.id());
    if (m_presetsPopup->currentPaintOp() != paintop.id()){
        // Must change the paintop as the current one is not supported
        // by the new colorspace.
        kWarning() << "current paintop " << paintop.name() << " was not set, not supported by colorspace";
    }
    
    m_activePreset = preset;
    updateCompositeOpComboBox();
    emit signalPaintopChanged(preset);
}

KoID KisPaintopBox::defaultPaintop(const KoInputDevice & inputDevice)
{
    if (inputDevice == KoInputDevice::eraser()) {
        return KoID("eraser", "");
    } else {
        return KoID("paintbrush", "");
    }
}

KisPaintOpPresetSP KisPaintopBox::activePreset(const KoID & paintop, const KoInputDevice & inputDevice)
{
    QHash<QString, KisPaintOpPresetSP> settingsArray;

    if (!m_inputDevicePresets.contains(inputDevice)) {
        foreach(const KoID& paintop, KisPaintOpRegistry::instance()->listKeys()) {
            settingsArray[paintop.id()] =
                KisPaintOpRegistry::instance()->defaultPreset(paintop, m_view->image());
        }
        m_inputDevicePresets[inputDevice] = settingsArray;
    } else {
        settingsArray = m_inputDevicePresets[inputDevice];
    }

    if (settingsArray.contains(paintop.id())) {
        KisPaintOpPresetSP preset = settingsArray[paintop.id()];
        return preset;
    } else {
        warnKrita << "Could not get paintop preset for paintop " << paintop.name() << ", return default";
        return KisPaintOpRegistry::instance()->defaultPreset(paintop, m_view->image());
    }
}

void KisPaintopBox::slotSaveActivePreset()
{
    KisPaintOpPresetSP curPreset = m_resourceProvider->currentPreset();
    
    if (!curPreset)
        return;

    KisPaintOpPreset* newPreset = curPreset->clone();
    KoResourceServer<KisPaintOpPreset>* rServer = KisResourceServerProvider::instance()->paintOpPresetServer();
    QString saveLocation = rServer->saveLocation();
    QString name = m_presetsPopup->getPresetName();
    QFileInfo fileInfo(saveLocation + name + newPreset->defaultFileExtension());

    if (fileInfo.exists()) {
        rServer->removeResource(rServer->getResourceByFilename(fileInfo.fileName()));
    }
    
//     int i = 1;
//     while (fileInfo.exists()) {
//         fileInfo.setFile(saveLocation + name + QString("%1").arg(i) + newPreset->defaultFileExtension());
//         i++;
//     }

    newPreset->setImage(m_presetsPopup->cutOutOverlay());
    newPreset->setFilename(fileInfo.filePath());
    newPreset->setName(name);

    m_presetsPopup->changeSavePresetButtonText(true);
    
    rServer->addResource(newPreset);
}

void KisPaintopBox::slotUpdatePreset()
{
    m_optionWidget->writeConfiguration(const_cast<KisPaintOpSettings*>(m_activePreset->settings().data()));
}

void KisPaintopBox::slotSetupDefaultPreset(){
    QString defaultName = m_activePreset->paintOp().id() + ".kpp";
    QString path = KGlobal::mainComponent().dirs()->findResource("kis_defaultpresets", defaultName);
    KisPaintOpPresetSP preset = new KisPaintOpPreset(path);

    if ( !preset->load() ){
        kWarning() << preset->filename() << "could not be found.";
        kWarning() << "I was looking for " << defaultName;
        return;
    }

    preset->settings()->setNode( m_activePreset->settings()->node() );
    preset->settings()->setOptionsWidget(m_optionWidget);
    m_optionWidget->setConfiguration(preset->settings());
    m_optionWidget->writeConfiguration(const_cast<KisPaintOpSettings*>( preset->settings().data() ));
}

void KisPaintopBox::nodeChanged(const KisNodeSP node)
{
    // Deconnect colorspace change of previous node
    if (m_previousNode) {
        if (m_previousNode->paintDevice()) {
            disconnect(m_previousNode->paintDevice().data(), SIGNAL(colorSpaceChanged(const KoColorSpace*)), this, SLOT(updateCompositeOpComboBox()));
        }
    }
    // Reconnect colorspace change of node
    m_previousNode = node;
    if (m_previousNode && m_previousNode->paintDevice()) {
        connect(m_previousNode->paintDevice().data(), SIGNAL(colorSpaceChanged(const KoColorSpace*)), SLOT(updateCompositeOpComboBox()));
    }
    updateCompositeOpComboBox();
}

void KisPaintopBox::eraseModeToggled(bool checked)
{
    m_cmbComposite->setEnabled(!checked);
    m_inputDeviceEraseModes[KoToolManager::instance()->currentInputDevice()] = checked;
    compositeOpChanged();
}

void KisPaintopBox::updateCompositeOpComboBox()
{
    KisNodeSP node = m_resourceProvider->currentNode();
    if (m_cmbComposite && node) {
        KisPaintDeviceSP device = node->paintDevice();

        if (device) {
            QList<KoCompositeOp*> compositeOps = device->colorSpace()->compositeOps();
            QList<KoCompositeOp*> whiteList;
            if (m_activePreset) {
                QStringList whiteListIDs =  KisPaintOpRegistry::instance()->get(m_activePreset->paintOp().id())->whiteListedCompositeOps();
                foreach(QString id, whiteListIDs) {
                    foreach(KoCompositeOp* op, compositeOps) {
                        if (op->id() == id) {
                            whiteList << op;
                        }
                    }
                }
            }

            m_cmbComposite->setCompositeOpList(compositeOps, whiteList);

            if(m_cmbComposite->currentItem().isEmpty()){

            }
            
            if (m_compositeOp == 0 || !compositeOps.contains(const_cast<KoCompositeOp*>(m_compositeOp)) ||
               (!m_compositeOp->userVisible() && !whiteList.contains(const_cast<KoCompositeOp*>(m_compositeOp)))) {
                    m_compositeOp = device->colorSpace()->compositeOp(COMPOSITE_OVER);
            }
            m_cmbComposite->setCurrent(m_compositeOp);
            
            if(!m_inputDeviceEraseModes[KoToolManager::instance()->currentInputDevice()]){
                m_cmbComposite->setEnabled(true);
            }
            setEnabledInternal(true);
            compositeOpChanged();
        } else {
            setEnabledInternal(false);
        }
    }
}

void KisPaintopBox::compositeOpChanged()
{
    if(m_inputDeviceEraseModes[KoToolManager::instance()->currentInputDevice()]) {
        m_resourceProvider->setCurrentCompositeOp(COMPOSITE_ERASE);
    } else {
        m_resourceProvider->setCurrentCompositeOp(m_compositeOp->id());
    }
}

void KisPaintopBox::setCompositeOpInternal(const QString& id)
{
    QString compositeID = id;
    if(compositeID.isEmpty()) {
        compositeID = COMPOSITE_OVER;
    }
    KisNodeSP node = m_resourceProvider->currentNode();
    if(node) {
        KisPaintDeviceSP device = node->paintDevice();
        if (device) {
            m_compositeOp = device->colorSpace()->compositeOp(compositeID);
        }
    }
}

void KisPaintopBox::setEnabledInternal(bool value)
{
    m_paintopWidget->setEnabled(value);
    if(value) {
        m_presetWidget->setIcon(KIcon("paintop_settings_01"));
    } else {
        m_presetWidget->setIcon(KIcon("paintop_presets_disabled"));
    }
}

void KisPaintopBox::slotSetCompositeMode(const QString& compositeOp)
{
    m_inputDeviceCompositeModes[KoToolManager::instance()->currentInputDevice()] = compositeOp;
    setCompositeOpInternal(compositeOp);
    m_resourceProvider->setCurrentCompositeOp(compositeOp);
}


void KisPaintopBox::slotSaveToFavouriteBrushes()
{
    if(! m_view->canvasBase()->favoriteResourceManager())
    {
        m_view->canvasBase()->createFavoriteResourceManager(this);
    }
    else {
        m_view->canvasBase()->favoriteResourceManager()->showPaletteManager();
    }
}

void KisPaintopBox::slotWatchPresetNameLineEdit(const QString& text)
{
    KoResourceServer<KisPaintOpPreset>* rServer = KisResourceServerProvider::instance()->paintOpPresetServer();
    m_presetsPopup->changeSavePresetButtonText(rServer->getResourceByName(text) != 0);
}

void KisPaintopBox::slotHorizontalMirrorChanged(bool value)
{
    m_resourceProvider->setMirrorHorizontal(value);
}

void KisPaintopBox::slotVerticalMirrorChanged(bool value)
{
    m_resourceProvider->setMirrorVertical(value);
}

void KisPaintopBox::slotOpacityChanged(int value)
{
    m_resourceProvider->setOpacity(value);
}

#include "kis_paintop_box.moc"
