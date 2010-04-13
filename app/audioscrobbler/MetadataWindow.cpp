/*
   Copyright 2005-2009 Last.fm Ltd. 
      - Primarily authored by Max Howell, Jono Cole and Doug Mansell

   This file is part of the Last.fm Desktop Application Suite.

   lastfm-desktop is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   lastfm-desktop is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with lastfm-desktop.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "MetadataWindow.h"

#include "ScrobbleStatus.h"
#include "ScrobbleControls.h"
#include "Application.h"
#include "RestWidget.h"
#include "lib/unicorn/widgets/DataListWidget.h"
#include "lib/unicorn/widgets/MessageBar.h"
#include "lib/unicorn/StylableWidget.h"
#include "lib/unicorn/widgets/HttpImageWidget.h"
#include "lib/unicorn/qtwin.h"
#include "lib/unicorn/layouts/SideBySideLayout.h"
#include "lib/listener/PlayerConnection.h"

#include <lastfm/Artist>
#include <lastfm/XmlQuery>
#include <lastfm/ws.h>

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTextBrowser>
#include <QNetworkReply>
#include <QTextFrame>
#include <QVBoxLayout>
#include <QStackedLayout>
#include <QScrollArea>
#include <QStatusBar>
#include <QSizeGrip>
#include <QDesktopServices>
#include <QAbstractTextDocumentLayout>


TitleBar::TitleBar( const QString& title )
{
    QHBoxLayout* layout = new QHBoxLayout( this );
    layout->setContentsMargins( 0, 0, 0, 0 );
    QPushButton* pb;
    layout->addWidget( pb = new QPushButton( "" ));
    connect( pb, SIGNAL(clicked()), SIGNAL( closeClicked()));
    pb->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
    pb->setFlat( true );
    QLabel* l;
    layout->addWidget( l = new QLabel( title ));
    l->setAlignment( Qt::AlignCenter );
}

MetadataWindow::MetadataWindow()
{
    setWindowTitle( "Audioscrobbler" );
    setAttribute( Qt::WA_TranslucentBackground );
    QPalette pal(palette());
    pal.setColor( QPalette::Window, QColor( 0, 0, 0, 0 ));
    setPalette( pal );
    setAutoFillBackground( true );
    
    setAttribute( Qt::WA_MacAlwaysShowToolWindow );
	
#ifdef Q_OS_MAC
	setWindowFlags( Qt::CustomizeWindowHint );
#else
	setWindowFlags( Qt::CustomizeWindowHint | Qt::WindowCloseButtonHint );
#endif	

    //Enable aero blurry window effect:
    QtWin::extendFrameIntoClientArea( this );
    
    setCentralWidget(new QWidget);

    new QVBoxLayout( centralWidget());
    centralWidget()->layout()->setSpacing( 0 );
    centralWidget()->layout()->setContentsMargins( 0, 0, 0, 0 );

#ifdef Q_OS_MAC   
    TitleBar* titleBar;
    qobject_cast<QBoxLayout*>(centralWidget()->layout())->addWidget( titleBar = new TitleBar("Audioscrobbler"), 0, Qt::AlignBottom );
    connect( titleBar, SIGNAL( closeClicked()), SLOT( close()));
#endif

    SideBySideLayout* stackLayout = new SideBySideLayout();
    QWidget* nav;
    centralWidget()->layout()->addWidget( nav = new StylableWidget());
    nav->setObjectName( "navigation" );
   
    centralWidget()->layout()->addWidget(ui.now_playing_source = new ScrobbleStatus());
    ui.now_playing_source->setObjectName("now_playing");
    ui.now_playing_source->setFixedHeight( 22 );
    qobject_cast<QBoxLayout*>(centralWidget()->layout())->addLayout( stackLayout );
    stackLayout->addWidget( stack.rest = new RestWidget());

    {
        QHBoxLayout* nl = new QHBoxLayout( nav );
        nl->addWidget( ui.nav.profile = new QPushButton( tr( "Profile" )), 0, Qt::AlignLeft );
        nl->addWidget( ui.nav.nowScrobbling = new QPushButton( tr( "Now Scrobbling" )),0, Qt::AlignRight);
        connect( ui.nav.profile, SIGNAL( clicked()), SLOT( showProfile()));
        connect( ui.nav.nowScrobbling, SIGNAL( clicked()), SLOT( showNowScrobbling()));
    }
    

    stack.nowScrobbling = new QWidget( centralWidget() );
    stack.nowScrobbling->setObjectName( "NowScrobbling" );
    QVBoxLayout* v = new QVBoxLayout( stack.nowScrobbling );
    stackLayout->addWidget( stack.nowScrobbling );
    setCurrentWidget( stack.rest );

    setMinimumWidth( 410 );

    QVBoxLayout* vs;
    {
        QWidget* scrollWidget;
        QScrollArea* sa = new QScrollArea();
        sa->setWidgetResizable( true );
        sa->setWidget( scrollWidget = new StylableWidget(sa));
        sa->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        vs = new QVBoxLayout( scrollWidget );
        v->addWidget( sa );
    }

    // listeners, scrobbles, tags:
    {
        QLabel* label;
        QWidget* gridWidget = new QWidget();
        QGridLayout* grid = new QGridLayout(gridWidget);
        grid->setSpacing( 0 );

        {
            QVBoxLayout* v2 = new QVBoxLayout();
            grid->addWidget(ui.artist_image = new HttpImageWidget, 0, 0, Qt::AlignTop | Qt::AlignLeft );
            ui.artist_image->setObjectName("artist_image");
            ui.artist_image->setCursor( Qt::PointingHandCursor );
            connect( ui.artist_image, SIGNAL(clicked()), SLOT(onArtistImageClicked()));
            v2->addWidget(ui.title = new QLabel);
            v2->addWidget(ui.album = new QLabel);
            v2->addStretch();
            ui.title->setOpenExternalLinks( true );
            ui.title->setObjectName("title1");
            ui.title->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse );
            ui.title->setWordWrap(true);
            ui.album->setObjectName("title2");
            ui.album->setTextInteractionFlags( Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse );
            ui.album->setOpenExternalLinks( true );
            grid->addLayout(v2, 0, 1, Qt::AlignTop );
        }

        // On tour
        ui.onTour = new QLabel(tr("On tour"));
        ui.onTour->setObjectName("name");
        ui.onTour->setProperty("alternate", QVariant(true));
        ui.onTour->setWordWrap(true);
        ui.onTour->hide();
        
        ui.onTourBlank = new QLabel();
        ui.onTourBlank->setObjectName("value");
        ui.onTourBlank->setProperty("alternate", QVariant(true));
        grid->addWidget(ui.onTour, 1, 0 );
        grid->addWidget(ui.onTourBlank, 1, 1 );

        // Similar artists
        label = new QLabel(tr("Similar artists"));
        label->setObjectName("name");
        label->setAlignment( Qt::AlignTop );
        grid->addWidget( label, 2, 0 );
        ui.similarArtists = new DataListWidget(this);
        ui.similarArtists->setObjectName("value");
        grid->addWidget(ui.similarArtists, 2, 1);

        // Top fans
        label = new QLabel(tr("Top fans"));
        label->setObjectName("name");
        label->setProperty("alternate", QVariant(true));
        label->setAlignment( Qt::AlignTop );
        grid->addWidget( label, 3, 0 );
        ui.topFans = new DataListWidget(this);
        ui.topFans->setObjectName("value");
        ui.topFans->setProperty("alternate", QVariant(true));
        grid->addWidget(ui.topFans, 3, 1);

        {
            // Scrobbles (artist, album, track)
            label = new QLabel(tr("Scrobbles"));
            label->setObjectName("name");
            label->setAlignment( Qt::AlignTop );
            grid->addWidget( label, 4, 0 );

            QVBoxLayout* vp = new QVBoxLayout();

            ui.artistScrobbles = new QLabel;
            ui.artistScrobbles->setObjectName("value");
            ui.artistScrobbles->hide();
            vp->addWidget(ui.artistScrobbles);
            ui.albumScrobbles = new QLabel;
            ui.albumScrobbles->setObjectName("value");
            ui.albumScrobbles->hide();
            vp->addWidget(ui.albumScrobbles);
            ui.trackScrobbles = new QLabel;
            ui.trackScrobbles->setObjectName("value");
            vp->addWidget(ui.trackScrobbles);

            grid->addLayout(vp, 4, 1);
        }

        // Top tags
        label = new QLabel(tr("Tagged as"));
        label->setObjectName("name");
        label->setProperty("alternate", QVariant(true));
        label->setAlignment( Qt::AlignTop );
        grid->addWidget( label, 5, 0 );
        ui.tags = new DataListWidget(this);
        ui.tags->setObjectName("value");
        ui.tags->setProperty("alternate", QVariant(true));
        grid->addWidget(ui.tags, 5, 1);

        // bio:
        label = new QLabel(tr("Biography"));
        label->setObjectName("name");
        label->setAlignment( Qt::AlignTop );
        grid->addWidget( label, 6, 0 );
        grid->addWidget(ui.bio = new QTextBrowser, 6, 1);
        ui.bio->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
        ui.bio->setOpenLinks( false );

        grid->setRowStretch( 6, 1 );
        gridWidget->setObjectName( "data_grid" );

        vs->addWidget(gridWidget, 1);
        vs->addStretch();
    }
    connect(ui.bio->document()->documentLayout(), SIGNAL( documentSizeChanged(QSizeF)), SLOT( onBioChanged(QSizeF)));
    connect(ui.bio, SIGNAL(anchorClicked(QUrl)), SLOT(onAnchorClicked(QUrl)));
    vs->setStretchFactor(ui.bio, 1);

    // status bar and scrobble controls
    QStatusBar* statusBar = new QStatusBar( this );
    {
        statusBar->setContentsMargins( 0, 0, 0, 0 );
        addDragHandleWidget( statusBar );

#ifdef Q_WS_WIN
        statusBar->setSizeGripEnabled( false );
#else
        //FIXME: this code is duplicated in the radio too
        //In order to compensate for the sizer grip on the bottom right
        //of the window, an empty QWidget is added as a spacer.
        QSizeGrip* sg = statusBar->findChild<QSizeGrip *>();
        if( sg ) {
            int gripWidth = sg->sizeHint().width();
            QWidget* w = new QWidget( statusBar );
            w->setFixedWidth( gripWidth );
            statusBar->addWidget( w );
        }
#endif
        //Seemingly the only way to get a central widget in a QStatusBar
        //is to add an empty widget either side with a stretch value.
        statusBar->addWidget( new QWidget( statusBar), 1 );
        statusBar->addWidget( ui.sc = new ScrobbleControls());
        statusBar->addWidget( new QWidget( statusBar), 1 );
        setStatusBar( statusBar );
    }
    addDragHandleWidget( ui.now_playing_source );
#ifdef Q_OS_MAC
    addDragHandleWidget( titleBar );
    addDragHandleWidget( nav );
#endif
    addDragHandleWidget( stack.rest );
    addDragHandleWidget( stack.nowScrobbling );
    addDragHandleWidget( statusBar );

    v->setSpacing(0);
    v->setMargin(0);
    vs->setSpacing(0);
    vs->setMargin(0);


    setWindowTitle(tr("Last.fm Audioscrobbler"));
    setUnifiedTitleAndToolBarOnMac( true );
    setMinimumHeight( 80 );
    resize(20, 500);

    ui.message_bar = new MessageBar( centralWidget());
    finishUi();
}

void
MetadataWindow::onAnchorClicked( const QUrl& link )
{
    QDesktopServices::openUrl( link );
}

void
MetadataWindow::onBioChanged( const QSizeF& size )
{
    ui.bio->setMinimumHeight( size.toSize().height() );
}

void
MetadataWindow::onTrackStarted(const Track& t, const Track& previous)
{
    setCurrentWidget( stack.nowScrobbling );
    const unsigned short em_dash = 0x2014;
    QString title = QString("<a class='title' href=\"%1\">%2</a> ") + QChar(em_dash) + " <a class='title' href=\"%3\">%4</a>";
    const unicorn::Application* uApp = qobject_cast<unicorn::Application*>(qApp);
    ui.title->setText( "<style>" + uApp->loadedStyleSheet() + "</style>" + title.arg(t.artist().www().toString())
                                                                                .arg(t.artist())
                                                                                .arg(t.www().toString())
                                                                                .arg(t.title()));
    if( !t.album().isNull() ) {
        QString album("from <a class='title' href=\"%1\">%2</a>");
        ui.album->setText("<style>" + uApp->loadedStyleSheet() + "</style>" + album.arg( t.album().www().toString())
                                                                               .arg( t.album().title()));
    } else {
        ui.album->clear();
    }

    ui.onTour->hide();
    ui.onTourBlank->hide();

    m_currentTrack = t;
    ui.now_playing_source->onTrackStarted(t, previous);
    
    if (t.artist() != previous.artist())
    {
        ui.artist_image->clear();
        ui.bio->clear();
        ui.onTour->setEnabled( false );
        ui.similarArtists->clear();
        ui.tags->clear();

        connect(t.artist().getInfo( lastfm::ws::Username , lastfm::ws::SessionKey ), SIGNAL(finished()), SLOT(onArtistGotInfo()));
        connect(t.artist().getEvents( 1 ), SIGNAL(finished()), SLOT(onArtistGotEvents()));
    }

    connect(t.album().getInfo( lastfm::ws::Username , lastfm::ws::SessionKey ), SIGNAL(finished()), SLOT(onAlbumGotInfo()));
    connect(t.getInfo( lastfm::ws::Username , lastfm::ws::SessionKey ), SIGNAL(finished()), SLOT(onTrackGotInfo()));

    ui.topFans->clear();
    connect(t.getTopFans(), SIGNAL(finished()), SLOT(onTrackGotTopFans()));
}

void
MetadataWindow::onArtistGotInfo()
{
    XmlQuery lfm = static_cast<QNetworkReply*>(sender())->readAll();

    int scrobbles = lfm["artist"]["stats"]["playcount"].text().toInt();
    int listeners = lfm["artist"]["stats"]["listeners"].text().toInt();
    int userListens = lfm["artist"]["stats"]["userplaycount"].text().toInt();

    foreach(const XmlQuery& e, lfm["artist"]["similar"].children("artist"))
    {
        ui.similarArtists->addItem( e["name"].text(), QUrl(e["url"].text()));
    }

    foreach(const XmlQuery& e, lfm["artist"]["tags"].children("tag"))
    {
        ui.tags->addItem( e["name"].text(), QUrl(e["url"].text()));
    }

    ui.artistScrobbles->setText(QString("%L1").arg(scrobbles) + " plays (" + QString("%L1").arg(listeners) + " listeners)" + "\n" + QString("%L1").arg(userListens) + " plays in your library");

    QString stylesheet = ((audioscrobbler::Application*)qApp)->loadedStyleSheet() + styleSheet();
    QString style = "<style>" + stylesheet + "</style>";
    
    //TODO if empty suggest they edit it
    QString bio;
    {
        QStringList bioList = lfm["artist"]["bio"]["summary"].text().trimmed().split( "\r" );
        foreach( const QString& p, bioList )
            bio += "<p>" + p + "</p>";
    }

    ui.bio->setHtml( style + bio );

    QTextFrame* root = ui.bio->document()->rootFrame();
    QTextFrameFormat f = root->frameFormat();
    f.setMargin(12);
    root->setFrameFormat(f);

    QUrl url = lfm["artist"]["image size=large"].text();
    ui.artist_image->loadUrl( url, true );
}


void
MetadataWindow::onArtistGotEvents()
{
    XmlQuery lfm = lastfm::ws::parse( static_cast<QNetworkReply*>(sender()) );

    if (lfm["events"].children("event").count() > 0)
    {
        // Display an on tour notification
        ui.onTour->show();
        ui.onTourBlank->show();
    }
}

void
MetadataWindow::onAlbumGotInfo()
{
    XmlQuery lfm = static_cast<QNetworkReply*>(sender())->readAll();

    int scrobbles = lfm["album"]["playcount"].text().toInt();
    int listeners = lfm["album"]["listeners"].text().toInt();
    int userListens = lfm["album"]["userplaycount"].text().toInt();

    ui.albumScrobbles->setText(QString("%L1").arg(scrobbles) + " plays (" + QString("%L1").arg(listeners) + " listeners)" + "\n" + QString("%L1").arg(userListens) + " plays in your library");;
}

void
MetadataWindow::onTrackGotInfo()
{
    XmlQuery lfm = static_cast<QNetworkReply*>(sender())->readAll();

    int scrobbles = lfm["track"]["playcount"].text().toInt();
    int listeners = lfm["track"]["listeners"].text().toInt();
    int userListens = lfm["track"]["userplaycount"].text().toInt();

    ui.trackScrobbles->setText(QString("%L1").arg(scrobbles) + " plays (" + QString("%L1").arg(listeners) + " listeners)" + "\n" + QString("%L1").arg(userListens) + " plays in your library");;
}

void
MetadataWindow::onTrackGotTopFans()
{
    XmlQuery lfm = static_cast<QNetworkReply*>(sender())->readAll();

    foreach(const XmlQuery& e, lfm["topfans"].children("user").mid(0, 5))
    {
        ui.topFans->addItem(e["name"].text(), QUrl(e["url"].text()));
    }
}

void
MetadataWindow::onStopped()
{
    setCurrentWidget( stack.rest );
    ui.bio->clear();
    ui.artist_image->clear();
    ui.title->clear();
    ui.tags->clear();
    ui.album->clear();
    ui.artistScrobbles->clear();
    ui.albumScrobbles->clear();
    ui.trackScrobbles->clear();
    m_currentTrack = Track();
    ui.now_playing_source->onTrackStopped();
}

void
MetadataWindow::onResumed()
{
}

void
MetadataWindow::onPaused()
{
}

void
MetadataWindow::setCurrentWidget( QWidget* w )
{
    if( w == stack.nowScrobbling ) {
        ui.nav.profile->setVisible( true );
        ui.nav.nowScrobbling->setVisible( false );
    } else if( w == stack.rest ) {
        ui.nav.profile->setVisible( false );
        PlayerConnection* connection = qobject_cast<audioscrobbler::Application*>(qApp)->currentConnection();
        if( connection && connection->state() != Stopped )
            ui.nav.nowScrobbling->setVisible( true );
        else
            ui.nav.nowScrobbling->setVisible( false );
    }
    centralWidget()->findChild<SideBySideLayout*>()->moveToWidget( w );
}

void
MetadataWindow::showProfile()
{
    setCurrentWidget( stack.rest );
}

void 
MetadataWindow::showNowScrobbling()
{
    setCurrentWidget( stack.nowScrobbling );
}

void 
MetadataWindow::onArtistImageClicked()
{
    QDesktopServices::openUrl( m_currentTrack.artist().www() );
}
