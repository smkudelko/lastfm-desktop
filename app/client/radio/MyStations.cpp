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

#include "MyStations.h"
#include "the/radio.h"
#include "the/settings.h"
#include "the/mainWindow.h"
#include <QListWidgetItem>
#include <QDebug>
#include "lib/types/Tag.h"
#include "SearchResultsTuner.h"
#include "lib/unicorn/widgets/SpinnerLabel.h"
#include "MyStationsDelegate.h"


RadioStation helper( RadioStation s, const QString& title )
{
	s.setTitle( title );
	return s;
}


MyStations::MyStations()
		   :m_searchResults( 0 )
{
	User user( The::settings().username() );
	
	m_myStationList << helper( RadioStation::library( user ), "My Library" )
                    << helper( RadioStation::recommendations( user ), "Recommended" )
	                << helper( RadioStation::lovedTracks( user ), "My Loved Tracks" )
	                << helper( RadioStation::neighbourhood( user ), "My Neighbourhood" );
	
	ui.setupUi( this );
	
	ui.list->setItemDelegate( new MyStationsDelegate );
	
	//It would be nice to set the backgroundRole to Window but this screws up
	//the painting when scrolling / hovering items :(
	//
	//ui.list->viewport()->setBackgroundRole( QPalette::Window );
	
	ui.list->setSpacing( 0 );
	
	connect( ui.list, SIGNAL( itemEntered( QListWidgetItem* )), SLOT( onItemHover( QListWidgetItem* )));
	connect( ui.list, SIGNAL( itemClicked( QListWidgetItem* )), SLOT( onItemClicked( QListWidgetItem* )));
	
	ui.spinner->hide();
	connect( ui.searchBox, SIGNAL( returnPressed()), SLOT( onSearch()));
	connect( ui.searchButton, SIGNAL( clicked()), SLOT( onSearch()));
	
	ui.list->setMouseTracking( true );
	setMouseTracking( true );
	
	foreach( const RadioStation& r, m_myStationList )
	{
		QListWidgetItem* i = new QListWidgetItem( QIcon( ":/station.png" ), r.title() );

		i->setData( Qt::UserRole, QVariant::fromValue<RadioStation>( r ));
		ui.list->addItem( i );
	}
	
}

void
MyStations::onItemHover( QListWidgetItem* item )
{
	item->setSelected( true );
}


void
MyStations::mouseMoveEvent( QMouseEvent* /* e */ )
{
	//clear the current selection when then mouse moves out of the list view
	//and onto the widget - this needs a better solution me thinks..
	ui.list->setCurrentItem( 0 );
}


void
MyStations::onItemClicked( QListWidgetItem* i )
{
	RadioStation r = i->data( Qt::UserRole ).value< RadioStation >();
	The::radio().play( r );
}


void
MyStations::onSearch()
{
	const QString& searchTerm = ui.searchBox->text();
	
	if( searchTerm.startsWith( "lastfm://", Qt::CaseInsensitive ))
	{
		The::radio().play( searchTerm );
		return;
	}
	
	ui.spinner->show();
	delete m_searchResults;
	m_searchResults = new SearchResultsTuner;
	m_searchResults->setObjectName( searchTerm + " - Search Results" );
	
	Artist a( searchTerm);
	Tag t( searchTerm);
	
	connect( a.search(), SIGNAL(finished( WsReply*)), SLOT(onArtistSearchResults( WsReply*)) );
	connect( t.search(), SIGNAL(finished( WsReply*)), SLOT(onTagSearchResults( WsReply*)) );
	
}


void 
MyStations::onArtistSearchResults( WsReply* r )
{
	QStringList results = Artist::search( r );
	m_searchResults->addArtists( results );
	emit searchResultComplete( m_searchResults, m_searchResults->objectName() );
	ui.spinner->hide();
}


void
MyStations::onTagSearchResults( WsReply* r )
{
	QStringList results = Tag::search( r );
	m_searchResults->addTags( results );
}