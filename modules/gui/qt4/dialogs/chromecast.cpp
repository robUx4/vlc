/*****************************************************************************
 * chromecast.cpp : Chromecast output dialog
 ****************************************************************************
 * Copyright (C) 2015 the VideoLAN team
 *
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <QListWidget>
#include <QListWidgetItem>
#include <sstream>

#include <vlc_common.h>
#include <vlc_access.h>
#include <vlc_input_item.h>
#include <vlc_services_discovery.h>

#include "dialogs/chromecast.hpp"

#define VAR_CHROMECAST_IP  "chromecast-ip"

class ChromecastReceiver : public QListWidgetItem
{
public:
    enum ChromecastType {
        HAS_VIDEO = UserType,
        AUDIO_ONLY
    };

    ChromecastReceiver(const char *psz_name, const char *psz_ip,
                       uint16_t i_port, ChromecastType type)
        : QListWidgetItem( type==AUDIO_ONLY ? QIcon( ":/sidebar/music" ) : QIcon( ":/sidebar/movie" ),
                           qfu( psz_name ).append(" (").append(psz_ip).append(')'))
        , ipAddress( qfu( psz_ip ) )
        , port(i_port)
        , type(type)
    {
    }

protected:
    QString ipAddress;
    uint16_t port;
    ChromecastType type;

    friend class ChromecastDialog;
};

extern "C" void discovery_event_received( const vlc_event_t * p_event, void * user_data )
{
    ChromecastDialog *p_this = reinterpret_cast<ChromecastDialog*>(user_data);
    p_this->discoveryEventReceived( p_event );
}

ChromecastDialog::ChromecastDialog( intf_thread_t *_p_intf)
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
               , p_sd( NULL )
               , b_sd_started( false )
{
    setWindowTitle( qtr( "Chromecast Output" ) );
    setWindowRole( "vlc-chromecast" );

    /* Build Ui */
    ui.setupUi( this );

    CONNECT( ui.buttonBox, accepted(), this, accept() );
    CONNECT( ui.buttonBox, rejected(), this, onReject() );
    CONNECT( ui.receiversListWidget, itemDoubleClicked(QListWidgetItem*), this, accept());

    //QVLCTools::restoreWidgetPosition( p_intf, "Chromecast", this, QSize( 400 , 440 ) );
}

ChromecastDialog::~ChromecastDialog()
{
    if ( p_sd != NULL )
        vlc_sd_Destroy( p_sd );
};

void ChromecastDialog::onReject()
{
    /* set the chromecast control */
    playlist_t *p_playlist = pl_Get( p_intf );
    if (p_playlist != NULL)
    {
        if( var_Type( p_playlist, VAR_CHROMECAST_IP ) )
            var_SetString( p_playlist, VAR_CHROMECAST_IP, NULL );
    }

    QVLCDialog::reject();
}

void ChromecastDialog::close()
{
    //QVLCTools::saveWidgetPosition( p_intf, "Chromecast", this );

    QVLCDialog::close();
}

void ChromecastDialog::setVisible(bool visible)
{
    QVLCDialog::setVisible(visible);

    if (visible)
    {
        if ( p_sd == NULL )
        {
            msg_Dbg( p_intf, "starting services_discovery chromecast..." );
            p_sd = vlc_sd_Create( VLC_OBJECT(p_intf), "chromecast" );
            if( !p_sd )
                msg_Err( p_intf, "Could not start chromecast discovery" );
        }

        if ( p_sd != NULL )
        {
            int row = -1;
            playlist_t *p_playlist = pl_Get( p_intf );
            if (p_playlist != NULL)
            {
                char *psz_current_ip = var_GetString( p_playlist, VAR_CHROMECAST_IP );
                if (psz_current_ip != NULL)
                {
                    for ( row = 0 ; row < ui.receiversListWidget->count(); row++ )
                    {
                        ChromecastReceiver *rowItem = reinterpret_cast<ChromecastReceiver*>( ui.receiversListWidget->item( row ) );
                        if (rowItem->ipAddress == psz_current_ip)
                            break;
                    }
                    free( psz_current_ip );
                    if ( row == ui.receiversListWidget->count() )
                        row = -1;
                }
            }
            ui.receiversListWidget->setCurrentRow( row );

            if ( !b_sd_started )
            {
                vlc_event_manager_t *em = services_discovery_EventManager( p_sd );
                vlc_event_attach( em, vlc_ServicesDiscoveryItemAdded, discovery_event_received, this );
                vlc_event_attach( em, vlc_ServicesDiscoveryItemRemoved, discovery_event_received, this );
                vlc_event_attach( em, vlc_ServicesDiscoveryItemRemoveAll, discovery_event_received, this );

                b_sd_started = vlc_sd_Start( p_sd );
                if ( !b_sd_started )
                {
                    vlc_event_detach( em, vlc_ServicesDiscoveryItemAdded, discovery_event_received, this);
                    vlc_event_detach( em, vlc_ServicesDiscoveryItemRemoved, discovery_event_received, this);
                    vlc_event_detach( em, vlc_ServicesDiscoveryItemRemoveAll, discovery_event_received, this);
                }
            }
        }
    }
    else
    {
        if ( p_sd != NULL )
        {
            if ( b_sd_started )
            {
                vlc_event_manager_t *em = services_discovery_EventManager( p_sd );
                vlc_event_detach( em, vlc_ServicesDiscoveryItemAdded, discovery_event_received, this);
                vlc_event_detach( em, vlc_ServicesDiscoveryItemRemoved, discovery_event_received, this);
                vlc_event_detach( em, vlc_ServicesDiscoveryItemRemoveAll, discovery_event_received, this);

                vlc_sd_Stop( p_sd );
                b_sd_started = false;
            }
        }
    }
}

void ChromecastDialog::accept()
{
    /* get the selected one in the listview if any */
    QListWidgetItem *current = ui.receiversListWidget->currentItem();
    if (current != NULL)
    {
        ChromecastReceiver *item = reinterpret_cast<ChromecastReceiver*>(current);
        std::string psz_ip   = item->ipAddress.toUtf8().constData();
        std::string psz_name = item->text().toUtf8().constData();
        msg_Dbg( p_intf, "selecting Chromecast %s %s:%u", psz_name.c_str(), psz_ip.c_str(), item->port );

        /* set the chromecast control */
        playlist_t *p_playlist = pl_Get(p_intf);
        if( !var_Type( p_playlist, VAR_CHROMECAST_IP ) )
            /* Don't recreate the same variable over and over and over... */
            var_Create( p_playlist, VAR_CHROMECAST_IP, VLC_VAR_STRING );
        var_SetString( p_playlist, VAR_CHROMECAST_IP, psz_ip.c_str() );

        bool chromecast_loaded = false;
        vlc_list_t *l = vlc_list_children( p_playlist );
        for( int i=0; i < l->i_count; i++ )
        {
            vlc_object_t *p_obj = (vlc_object_t *)l->p_values[i].p_address;
            if ( p_obj->psz_object_type == std::string("interface") )
            {
                char *psz_name = vlc_object_get_name( p_obj );
                if ( psz_name && psz_name == std::string("ctrl_chromecast") )
                {
                    chromecast_loaded = true;
                    free(psz_name);
                    break;
                }
                free(psz_name);
            }
        }
        vlc_list_release( l );

        if ( !chromecast_loaded )
            intf_Create( p_playlist, "chromecast");
    }

    QVLCDialog::accept();
}

void ChromecastDialog::discoveryEventReceived( const vlc_event_t * p_event )
{
    if ( p_event->type == vlc_ServicesDiscoveryItemAdded )
    {
        int row = 0;
        for ( ; row < ui.receiversListWidget->count(); row++ )
        {
            ChromecastReceiver *rowItem = reinterpret_cast<ChromecastReceiver*>( ui.receiversListWidget->item( row ) );
            if (rowItem->ipAddress == p_event->u.services_discovery_item_added.p_new_item->psz_uri)
                return;
        }

        /* determine if it's audio-only by checking the YouTube app */
        std::stringstream s_video_test;
        s_video_test << "http://"
                     << p_event->u.services_discovery_item_added.p_new_item->psz_uri
                     << ":8008/apps/YouTube";
        access_t *p_test_app = vlc_access_NewMRL( VLC_OBJECT(p_intf), s_video_test.str().c_str() );

        ChromecastReceiver *item = new ChromecastReceiver( p_event->u.services_discovery_item_added.p_new_item->psz_name,
                                                           p_event->u.services_discovery_item_added.p_new_item->psz_uri,
                                                           8009,
                                                           p_test_app ? ChromecastReceiver::HAS_VIDEO : ChromecastReceiver::AUDIO_ONLY);
        if ( p_test_app )
            vlc_access_Delete( p_test_app );
        ui.receiversListWidget->addItem( item );

        playlist_t *p_playlist = pl_Get( p_intf );
        if (p_playlist != NULL)
        {
            char *psz_current_ip = var_GetString( p_playlist, VAR_CHROMECAST_IP );
            if (item->ipAddress == psz_current_ip)
                ui.receiversListWidget->setCurrentItem( item );
            free( psz_current_ip );
        }
    }
}
