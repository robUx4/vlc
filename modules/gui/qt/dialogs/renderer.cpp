/*****************************************************************************
 * renderer.cpp : Renderer output dialog
 ****************************************************************************
 * Copyright (C) 2015-2016 the VideoLAN team
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
#include <vlc_renderer.h>
#include <vlc_services_discovery.h>
#include <vlc_url.h>

#include "dialogs/renderer.hpp"

class RendererItem : public QListWidgetItem
{
public:
    RendererItem(vlc_renderer_item *obj)
        : QListWidgetItem( vlc_renderer_item_flags(obj) & VLC_RENDERER_CAN_VIDEO ? QIcon( ":/sidebar/movie" ) : QIcon( ":/sidebar/music" ),
                           qfu( vlc_renderer_item_name(obj) ).append(" (").append(RendererItemIP(obj)).append(')'))
    {
        m_obj = vlc_renderer_item_hold(obj);
    }
    ~RendererItem()
    {
        vlc_renderer_item_release(m_obj);
    }

protected:
    vlc_renderer_item* m_obj;

    friend class RendererDialog;

private:
    static QString RendererItemIP( const vlc_renderer_item *obj )
    {
        vlc_url_t url;
        vlc_UrlParse(&url, vlc_renderer_item_uri(obj));
        QString result = QString(url.psz_host);
        vlc_UrlClean(&url);
        return result;
    }
};

extern "C" void discovery_event_received( const vlc_event_t * p_event, void * user_data )
{
    RendererDialog *p_this = reinterpret_cast<RendererDialog*>(user_data);
    p_this->discoveryEventReceived( p_event );
}

RendererDialog::RendererDialog( intf_thread_t *_p_intf )
               : QVLCDialog( (QWidget*)_p_intf->p_sys->p_mi, _p_intf )
               , p_sd( NULL )
               , b_sd_started( false )
{
    setWindowTitle( qtr( "Renderer Output" ) );
    setWindowRole( "vlc-renderer" );

    /* Build Ui */
    ui.setupUi( this );

    CONNECT( ui.buttonBox, accepted(), this, accept() );
    CONNECT( ui.buttonBox, rejected(), this, onReject() );
    CONNECT( ui.receiversListWidget, itemDoubleClicked(QListWidgetItem*), this, accept());

    QVLCTools::restoreWidgetPosition( p_intf, "Renderer", this, QSize( 400 , 440 ) );
}

RendererDialog::~RendererDialog()
{
    if ( p_sd != NULL )
        vlc_sd_Destroy( p_sd );
}

void RendererDialog::onReject()
{
    setRenderer( NULL );

    QVLCDialog::reject();
}

void RendererDialog::close()
{
    QVLCTools::saveWidgetPosition( p_intf, "Renderer", this );

    QVLCDialog::close();
}

void RendererDialog::setVisible(bool visible)
{
    QVLCDialog::setVisible(visible);

    if (visible)
    {
        /* SD subnodes */
        char **ppsz_longnames;
        int *p_categories;
        char **ppsz_names = vlc_sd_GetNames( THEPL, &ppsz_longnames, &p_categories );
        if( !ppsz_names )
            return;

        char **ppsz_name = ppsz_names, **ppsz_longname = ppsz_longnames;
        int *p_category = p_categories;
        for( ; *ppsz_name; ppsz_name++, ppsz_longname++, p_category++ )
        {
            if( *p_category == SD_CAT_RENDERER )
            {
                /* TODO launch all discovery services for renderers */
                msg_Dbg( p_intf, "starting renderer discovery service %s", *ppsz_longname );
                if ( p_sd == NULL )
                {
                    p_sd = vlc_sd_Create( VLC_OBJECT(p_intf), *ppsz_name );
                    if( !p_sd )
                        msg_Err( p_intf, "Could not start renderer discovery services" );
                }
                break;
            }
        }
        free( ppsz_names );
        free( ppsz_longnames );
        free( p_categories );

        if ( p_sd != NULL )
        {
            int row = -1;
            char *psz_renderer = var_InheritString( THEPL, "renderer" );
            if ( psz_renderer != NULL )
            {
                for ( row = 0 ; row < ui.receiversListWidget->count(); row++ )
                {
                    RendererItem *rowItem = reinterpret_cast<RendererItem*>( ui.receiversListWidget->item( row ) );
                    if ( isItemUri( psz_renderer, rowItem->m_obj ) )
                        break;
                }
                if ( row == ui.receiversListWidget->count() )
                    row = -1;
                free( psz_renderer );
            }
            ui.receiversListWidget->setCurrentRow( row );

            if ( !b_sd_started )
            {
                vlc_event_manager_t *em = services_discovery_EventManager( p_sd );
                vlc_event_attach( em, vlc_ServicesDiscoveryRendererAdded, discovery_event_received, this );
                vlc_event_attach( em, vlc_ServicesDiscoveryRendererRemoved, discovery_event_received, this );

                b_sd_started = vlc_sd_Start( p_sd );
                if ( !b_sd_started )
                {
                    vlc_event_detach( em, vlc_ServicesDiscoveryRendererAdded, discovery_event_received, this);
                    vlc_event_detach( em, vlc_ServicesDiscoveryRendererRemoved, discovery_event_received, this);
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
                vlc_event_detach( em, vlc_ServicesDiscoveryRendererAdded, discovery_event_received, this);
                vlc_event_detach( em, vlc_ServicesDiscoveryRendererRemoved, discovery_event_received, this);

                vlc_sd_Stop( p_sd );
                b_sd_started = false;
            }
        }
    }
}

void RendererDialog::accept()
{
    /* get the selected one in the listview if any */
    QListWidgetItem *current = ui.receiversListWidget->currentItem();
    if (current != NULL)
    {
        RendererItem *rowItem = reinterpret_cast<RendererItem*>(current);
        msg_Dbg( p_intf, "selecting Renderer %s %s", vlc_renderer_item_name(rowItem->m_obj),
                 vlc_renderer_item_uri(rowItem->m_obj) );

        setRenderer( vlc_renderer_item_uri( rowItem->m_obj ) );
    }

    QVLCDialog::accept();
}

void RendererDialog::discoveryEventReceived( const vlc_event_t * p_event )
{
    if ( p_event->type == vlc_ServicesDiscoveryRendererAdded )
    {
        vlc_renderer_item *p_item =  p_event->u.services_discovery_renderer_added.p_new_item;

        int row = 0;
        for ( ; row < ui.receiversListWidget->count(); row++ )
        {
            RendererItem *rowItem = reinterpret_cast<RendererItem*>( ui.receiversListWidget->item( row ) );
            if ( !strcmp( vlc_renderer_item_uri( p_item ), vlc_renderer_item_uri( rowItem->m_obj ) ) )
                return;
        }

        RendererItem *newItem = new RendererItem(p_item);
        ui.receiversListWidget->addItem( newItem );

        char *psz_renderer = var_InheritString( THEPL, "renderer" );
        if ( psz_renderer != NULL )
        {
            if ( isItemUri( psz_renderer, p_item ) )
                ui.receiversListWidget->setCurrentItem( newItem );
            free( psz_renderer );
        }
    }
}

void RendererDialog::setRenderer( const char *psz_renderer )
{
    const char *psz_old_renderer = var_GetString( THEPL, "renderer" );
    if ( psz_renderer == NULL )
        psz_renderer = "";
    if ( !strcmp( psz_old_renderer, psz_renderer ) )
        return;

    var_SetString( THEPL, "renderer", psz_renderer );
}

bool RendererDialog::isItemUri( const char *psz_renderer_uri, const vlc_renderer_item *p_item )
{
    vlc_url_t uri, item_uri;
    vlc_UrlParse( &uri, psz_renderer_uri );
    vlc_UrlParse( &item_uri, vlc_renderer_item_uri( p_item ) );
    bool b_match = ( uri.i_port == item_uri.i_port || uri.i_port == 0 || item_uri.i_port == 0 ) &&
            ( uri.psz_protocol && item_uri.psz_protocol && !strcmp( uri.psz_protocol, item_uri.psz_protocol ) ) &&
            ( uri.psz_host && item_uri.psz_host && !strcmp( uri.psz_host, item_uri.psz_host ) );

    vlc_UrlClean( &uri );
    vlc_UrlClean( &item_uri );
    return b_match;
}
