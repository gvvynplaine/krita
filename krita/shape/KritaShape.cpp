/* This file is part of the KDE project
   Copyright 2007 Boudewijn Rempt <boud@valdyas.org>
   Copyright 2007 Thomas Zander <zander@kde.org>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/
#include "KritaShape.h"

#include <QPainter>
#include <QFrame>
#include <QVBoxLayout>
#include <QCoreApplication>

#include <kdebug.h>

#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>
#include <KoImageData.h>

#include "kis_image.h"
#include "kis_doc2.h"

class KritaShape::Private
{
public:
    KUrl url;
    KoColorProfile * displayProfile;
    KisDoc2 * doc;
};

KritaShape::KritaShape(const KUrl& url, const QString & profileName)
    : QObject()
    , KoShape()
{
    m_d = new Private();
    m_d->url = url;
    m_d->doc = 0;
    if ( !url.isEmpty() ) {
        importImage( url );
    }
    m_d->displayProfile = KoColorSpaceRegistry::instance()->profileByName(profileName);
    setKeepAspectRatio(true);
    moveToThread(QCoreApplication::instance()->thread()); // its a QObject; lets me sure it always has a proper thread.
}

KritaShape::~KritaShape()
{
    delete m_d;
}

void KritaShape::importImage(const KUrl & url )
{
    delete m_d->doc;
    m_d->doc = new KisDoc2(0, 0, false);
    connect(m_d->doc, SIGNAL(sigLoadingFinished()), this, SLOT(slotLoadingFinished()));
    m_d->doc->openURL(url);
}

void KritaShape::slotLoadingFinished()
{
    m_mutex.lock();
    if ( m_d && m_d->doc && m_d->doc->image() ) {
        m_waiter.wakeAll();
        repaint();
    }
    m_mutex.unlock();

}

void KritaShape::paint( QPainter& painter, const KoViewConverter& converter )
{
    if ( m_d && m_d->doc && m_d->doc->image() ) {
        // XXX: Only convert the bit the painter needs for painting?
        //      Or should we keep a converted qimage in readiness,
        //      just as with KisCanvas2?
        KisImageSP kimage= m_d->doc->image();

        QImage qimg = kimage->convertToQImage(0, 0, kimage->width(), kimage->height(),
                                              m_d->displayProfile); // XXX: How about exposure?

        const QRectF paintRect = QRectF( QPointF( 0.0, 0.0 ), size() );
        applyConversion( painter, converter );
        painter.drawImage(paintRect.toRect(), qimg);

    }
}

void KritaShape::setDisplayProfile( const QString & profileName ) {
    m_d->displayProfile = KoColorSpaceRegistry::instance()->profileByName( profileName );
    repaint();
}

void KritaShape::saveOdf( KoShapeSavingContext & context ) const {
    // TODO
}
bool KritaShape::loadOdf( const KoXmlElement & element, KoShapeLoadingContext &context ) {
    return false; // TODO
}

void KritaShape::waitUntilReady() const {
    if ( m_d && m_d->doc && m_d->doc->image() ) // all done
        return;

    KoImageData *data = dynamic_cast<KoImageData*> (KoShape::userData());
    if(data == 0 || !data->imageLocation().isValid())
        return; // no data available at all.

    KritaShape *me = const_cast<KritaShape*> (this);

    m_mutex.lock();
    me->importImage(data->imageLocation());
    m_waiter.wait(&m_mutex);
    m_mutex.unlock();
}

#include "KritaShape.moc"
