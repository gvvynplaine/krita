/*
 *  Copyright (c) 2011 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "channelmodel.h"
#include <QImage>
#include <KoColorSpace.h>
#include <KoChannelInfo.h>
#include <kis_painter.h>

#include <kis_group_layer.h>
#include <kis_paint_device.h>
#include <kis_iterator_ng.h>
#include <kis_default_bounds.h>

#include <kis_canvas2.h>

ChannelModel::ChannelModel(QObject* parent):
    QAbstractTableModel(parent),
    m_canvas(nullptr), m_oversampleRatio(2)
{
    setThumbnailSizeLimit(QSize(64, 64));
}

ChannelModel::~ChannelModel()
{
}

QVariant ChannelModel::data(const QModelIndex& index, int role) const
{
    if (m_canvas && index.isValid()) {
        auto rootLayer = m_canvas->image()->rootLayer();
        auto cs = rootLayer->colorSpace();
        auto channels = cs->channels();

        int channelIndex = KoChannelInfo::displayPositionToChannelIndex(index.row(), channels);

        switch (role) {
        case Qt::DisplayRole: {
            if (index.column() == 2) {
                return channels.at(channelIndex)->name();
            }
            return QVariant();
        }
        case Qt::DecorationRole: {
            if (index.column() == 1) {
                Q_ASSERT(m_thumbnails.count() > index.row());
                return QVariant(m_thumbnails.at(index.row()));
            }
            return QVariant();
        }
        case Qt::CheckStateRole: {
            Q_ASSERT(index.row() < rowCount());
            Q_ASSERT(index.column() < columnCount());

            if (index.column() == 0) {
                QBitArray flags = rootLayer->channelFlags();
                return (flags.isEmpty() || flags.testBit(channelIndex)) ? Qt::Checked : Qt::Unchecked;
            }
            return QVariant();
        }
        }
    }
    return QVariant();
}

QVariant ChannelModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section); Q_UNUSED(orientation); Q_UNUSED(role);
    return QVariant();
}


int ChannelModel::rowCount(const QModelIndex& /*parent*/) const
{
    if (!m_canvas) return 0;

    return m_canvas->image() ? (m_canvas->image()->colorSpace()->channelCount()) : 0;
}

int ChannelModel::columnCount(const QModelIndex& /*parent*/) const
{
    if (!m_canvas) return 0;

    //columns are: checkbox, thumbnail, channel name
    return 3;
}


bool ChannelModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (m_canvas && m_canvas->image()) {
        auto rootLayer = m_canvas->image()->rootLayer();
        auto cs = rootLayer->colorSpace();
        auto channels = cs->channels();
        Q_ASSERT(index.row() <= channels.count());

        int channelIndex = KoChannelInfo::displayPositionToChannelIndex(index.row(), channels);

        if (role == Qt::CheckStateRole) {
            QBitArray flags = cs->channelFlags(true, true);
            Q_ASSERT(!flags.isEmpty());
            if (flags.isEmpty())
                return false;

            //flags = flags.isEmpty() ? cs->channelFlags(true, true) : flags;
            flags.setBit(channelIndex, value.toInt() == Qt::Checked);
            rootLayer->setChannelFlags(flags);

            emit channelFlagsChanged();
            emit dataChanged(this->index(0, 0), this->index(channels.count(), 0));
            return true;
        }
    }
    return false;
}


//User double clicked on a row (but on channel checkbox)
//we select this channel, and deselect all other channels (except alpha, which we don't touch)
//this makes it fast to select single color channel
void ChannelModel::rowActivated(const QModelIndex &index)
{
    if (m_canvas && m_canvas->image()) {
        auto rootLayer = m_canvas->image()->rootLayer();
        auto cs = rootLayer->colorSpace();
        auto channels = cs->channels();
        Q_ASSERT(index.row() <= channels.count());

        int channelIndex = KoChannelInfo::displayPositionToChannelIndex(index.row(), channels);

        QBitArray flags = cs->channelFlags(true, true);
        Q_ASSERT(!flags.isEmpty());
        if (flags.isEmpty())
            return;

        for (int i = 0; i < channels.count(); ++i) {
            if (channels[i]->channelType() != KoChannelInfo::ALPHA) {
                flags.setBit(i, (i == channelIndex));
            }
        }

        rootLayer->setChannelFlags(flags);

        emit channelFlagsChanged();
        emit dataChanged(this->index(0, 0), this->index(channels.count(), 0));
    }
}


Qt::ItemFlags ChannelModel::flags(const QModelIndex& /*index*/) const
{
    Qt::ItemFlags flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
    return flags;
}

void ChannelModel::setThumbnailSizeLimit(QSize size)
{
    m_thumbnailSizeLimit = size;
    updateData(m_canvas);
}

void ChannelModel::slotSetCanvas(KisCanvas2 *canvas)
{
    if (m_canvas != canvas) {
        beginResetModel();
        m_canvas = canvas;
        if (m_canvas && m_canvas->image()) {
            updateThumbnails();
        }
        endResetModel();
    }
}

void ChannelModel::slotColorSpaceChanged(const KoColorSpace *colorSpace)
{
    Q_UNUSED(colorSpace);
    beginResetModel();
    updateThumbnails();
    endResetModel();
}

void ChannelModel::updateData(KisCanvas2 *canvas)
{
    beginResetModel();
    m_canvas = canvas;
    updateThumbnails();
    endResetModel();
}

//Create thumbnails from full image.
//Assumptions: thumbnail size is small compared to the original image and thumbnail quality
//doesn't need to be high, so we use fast but not very accurate algorithm.
void ChannelModel::updateThumbnails(void)
{

    if (m_canvas && m_canvas->image()) {
        KisImageSP canvas_image = m_canvas->image();
        const KoColorSpace* cs = canvas_image->colorSpace();
        quint32 channelCount = cs->channelCount();

        KisPaintDeviceSP dev = canvas_image->projection();

        //make sure thumbnail maintains aspect ratio of the original image
        QSize thumbnailSize(canvas_image->bounds().size());
        thumbnailSize.scale(m_thumbnailSizeLimit, Qt::KeepAspectRatio);

        KisPaintDeviceSP thumbnailDev = dev->createThumbnailDeviceOversampled(thumbnailSize.width(), thumbnailSize.height(),
                                                                              m_oversampleRatio, canvas_image->bounds());

        QRect thumbnailRect = thumbnailDev->exactBounds();
        QSize thumbnailSizeOversample = thumbnailRect.size();

        m_thumbnails.resize(channelCount);

        QVector<uchar*> image_cache = thumbnailDev->readPlanarBytes(0,0,thumbnailSize.width(), thumbnailSize.height());
        Q_ASSERT(image_cache.size() == channelCount);

        for (quint32 i = 0; i < channelCount; ++i) {
            m_thumbnails[i] = QImage(thumbnailSizeOversample, QImage::Format_Grayscale8);
            QImage &img = m_thumbnails[i];

            for (int y = 0; y < img.height(); y++)
            {
                memcpy(img.scanLine(y), image_cache[i]+y*thumbnailSize.width(), img.bytesPerLine());
            }
        }

    }
}

#include "moc_channelmodel.cpp"
