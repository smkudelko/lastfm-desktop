/***************************************************************************
 *   Copyright 2005-2009 Last.fm Ltd.                                      *
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
 
#ifndef APP_H
#define APP_H

#include <lastfm/global.h>
#include "lib/unicorn/UnicornApplication.h"
namespace Phonon { class AudioOutput; }


class App : public unicorn::Application
{
    Q_OBJECT
    
public:
    App( int& argc, char* argv[] );
    ~App();

    void init( class MainWindow* ) throw( int /*exitcode*/ );

    void play( QStringList tags );
    
public slots:
    void play();
    /** returns false if user cancels the picker dialog */
    bool scan( bool force_ask_user = true );
    
private slots:
    void onOutputDeviceActionTriggered( QAction* );

    void onPreparing();
    void onStarted( const Track& );
    void onResumed();
    void onPaused();
    void onStopped();
    
    void onScanningFinished();
    
    void onPlaybackError( const QString& );
    
private:
    void cleanup();
    
    class LocalContentScannerThread* m_contentScannerThread;
    class LocalContentScanner* m_contentScanner;
    class TrackTagUpdater* m_trackTagUpdater;
    class MainWindow* m_mainwindow;
    class TagCloudView* m_cloud;
    class ScrobSocket* m_scrobsocket;
    class MediaPipeline* m_pipe;
    
    Phonon::AudioOutput* m_audioOutput;
};

#endif //APP_H