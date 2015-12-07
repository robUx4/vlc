/*****************************************************************************
 * chromecast.hpp : Chromecast output dialog
 ****************************************************************************
 * Copyright ( C ) 2015 the VideoLAN team
 * $Id$
 *
 * Authors: Steve Lhomme <robux4@videolabs.io>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef QVLC_CHROMECAST_DIALOG_H_
#define QVLC_CHROMECAST_DIALOG_H_ 1

#include "util/qvlcframe.hpp"
#include "util/singleton.hpp"
#include "ui/chromecast.h"
#include <stdarg.h>
#include <QMutex>
#include <QAtomicInt>

class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class MsgEvent;

class ChromecastDialog : public QVLCDialog, public Singleton<ChromecastDialog>
{
    Q_OBJECT

public:
    void discoveryEventReceived( const vlc_event_t * p_event );
    void setVisible(bool visible);

private:
    ChromecastDialog( intf_thread_t * );
    virtual ~ChromecastDialog();

    Ui::chromecastWidget ui;
    void sinkMessage( const MsgEvent * );

private slots:
    void accept();
    void onReject();
    void close();

private:

    friend class    Singleton<ChromecastDialog>;
    services_discovery_t *p_sd;
    bool                  b_sd_started;
    bool                  b_interface_loaded;
};


#endif
