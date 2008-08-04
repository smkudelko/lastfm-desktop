/***************************************************************************
 *   Copyright 2005-2008 Last.fm Ltd.                                      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Steet, Fifth Floor, Boston, MA  02110-1301, USA.          *
 ***************************************************************************/

#include "ScrobbleProgressBar.h"
#include "PlayerEvent.h"
#include <QtGui>


ScrobbleProgressBar::ScrobbleProgressBar()
                   : m_scrobbleProgressTick( 0 ),
                     m_scrobblePoint( 0 )
{
    QHBoxLayout* h = new QHBoxLayout;
    h->addWidget( ui.time = new QLabel );
    h->addStretch();
    h->addWidget( ui.timeToScrobblePoint = new QLabel );
    h->setContentsMargins( 0, 0, 0, 11 );
    setLayout( h );

    ui.time->setDisabled( true ); //aesthetics
    ui.timeToScrobblePoint->setDisabled( true ); //aesthetics
    ui.time->setAttribute( Qt::WA_MacMiniSize );
    ui.timeToScrobblePoint->setAttribute( Qt::WA_MacMiniSize );

    setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Fixed );

    m_progressPaintTimer = new QTimer( this );
    connect( m_progressPaintTimer, SIGNAL(timeout()), SLOT(onProgressDisplayTick()) );

    connect( qApp, SIGNAL(event( int, QVariant )), SLOT(onAppEvent( int, QVariant )) );
}


void
ScrobbleProgressBar::paintEvent( QPaintEvent* )
{
    uint const h = height();

    if (ui.time->text().isEmpty())
        return;
    
    QPainter p( this );
    p.setPen( QColor( 0x37, 0x37, 0x37 ) );
    p.setBrush( QColor( 0xff, 0xff, 0xff, 20 ) );
    p.drawRect( 0, h-10, width()-1, 9 );
    
    p.setPen( Qt::transparent );
    p.setBrush( QColor( 255, 255, 255, 70 ) );
    p.drawRect( 1, h-9, m_scrobbleProgressTick / 2, 8 );
    
    p.setPen( Qt::white );
    p.setBrush( Qt::transparent );
    for (uint x = 3; x < m_scrobbleProgressTick; x+=2)
        p.drawLine( x, h-7, x, h-4 );
}


void
ScrobbleProgressBar::determineProgressDisplayGranularity( const ScrobblePoint& g )
{
    m_progressPaintTimer->setInterval( 1000 * g / width() );
}


void
ScrobbleProgressBar::onProgressDisplayTick()
{
    m_scrobbleProgressTick++;
    update();
}


void
ScrobbleProgressBar::onPlaybackTick( int s )
{
    if ((uint)s < scrobblePoint())
    {
        QTime t( 0, 0 );
        t = t.addSecs( scrobblePoint() );
        t = t.addSecs( -s );
        ui.timeToScrobblePoint->setText( t.toString( "-mm:ss" ) );
    }
    else
        ui.timeToScrobblePoint->setText( tr( "Scrobbled" ) );

    QTime t( 0, 0 );
    t = t.addSecs( s );
    ui.time->setText( t.toString( "mm:ss" ) );
}


void
ScrobbleProgressBar::resizeEvent( QResizeEvent* e )
{
    if (!scrobblePoint() || e->oldSize().width() == e->size().width())
        return;

    // this is as exact as we can get it in milliseconds
    uint exactElapsedScrobbleTime = m_scrobbleProgressTick * m_progressPaintTimer->interval();

    determineProgressDisplayGranularity( scrobblePoint() );

    if (e->oldSize().width() == 0)
    {
        m_scrobbleProgressTick = 0;
    }
    else
    {
        double f = exactElapsedScrobbleTime;
        f /= scrobblePoint() * 1000;
        f *= e->size().width();
        m_scrobbleProgressTick = ceil( f );
    }

    update();
}


void
ScrobbleProgressBar::onAppEvent( int e, const QVariant& v )
{
    switch (e)
    {
    case PlayerEvent::PlaybackStarted:
    case PlayerEvent::TrackChanged:
        {
            onPlaybackTick( 0 );
            m_scrobbleProgressTick = 0;
            ObservedTrack t = v.value<ObservedTrack>();
            determineProgressDisplayGranularity( t.scrobblePoint() );
            m_scrobblePoint = t.scrobblePoint();
            connect( t.watch(), SIGNAL(tick( int )), SLOT(onPlaybackTick( int )) );
            connect( t.watch(), SIGNAL(destroyed()), m_progressPaintTimer, SLOT(stop()) );
            update();
            break;
        }
    }

    switch (e)
    {
        case PlayerEvent::PlaybackStarted:
        case PlayerEvent::TrackChanged:
        case PlayerEvent::PlaybackUnstalled:
        case PlayerEvent::PlaybackUnpaused:
            m_progressPaintTimer->start();
            break;

        case PlayerEvent::PlaybackStalled:
        case PlayerEvent::PlaybackPaused:
            m_progressPaintTimer->stop();
            break;
        
        case PlayerEvent::PlaybackEnded:
            resetUI();
            break;
    }
}


void
ScrobbleProgressBar::resetUI()
{
    m_scrobbleProgressTick = 0;
    ui.time->clear();
    ui.timeToScrobblePoint->clear();
    update();
}