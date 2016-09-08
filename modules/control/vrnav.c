 /*
 *  VR/360 navigation hooks.
 *
 *  Copyright (C) 2016 the VideoLAN team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License version 2.1 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation Inc. 59 Temple Place, Suite 330, Boston MA 02111-1307 USA
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <math.h>

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_input.h>
#include <vlc_keys.h>
#include <vlc_playlist.h>
#include <vlc_vout.h>

/*****************************************************************************
 * intf_sys_t: description and status of interface
 *****************************************************************************/
struct intf_sys_t
{
    vlc_mutex_t         lock;
    input_thread_t     *p_input;
    vout_thread_t      *p_vout;
    bool                b_button_pressed;
    int                 i_last_x, i_last_y;
    vlc_viewpoint_t     last_viewpoint;
    unsigned int        i_pattern;
};


static int MovedEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_data;
    intf_sys_t    *p_sys = p_intf->p_sys;

    (void) p_this; (void) psz_var; (void) oldval;

    vlc_mutex_lock( &p_sys->lock );
    if( p_sys->b_button_pressed )
    {
        vlc_viewpoint_t viewpoint = p_sys->last_viewpoint;
        int i_horizontal = newval.coords.x - p_sys->i_last_x;
        int i_vertical = newval.coords.y - p_sys->i_last_y;

        viewpoint.f_yaw   += (float)i_horizontal / 200;
        viewpoint.f_pitch += (float)i_vertical / 200;

        vout_SetViewpoint( p_sys->p_vout, &viewpoint);
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int ButtonEvent( vlc_object_t *p_this, char const *psz_var,
                        vlc_value_t oldval, vlc_value_t newval, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var; (void) oldval;

    vlc_mutex_lock( &p_sys->lock );
    if( newval.i_int & 0x01)
    {
        if( !p_sys->b_button_pressed )
        {
            p_sys->b_button_pressed = true;
            playlist_GetViewpoint( pl_Get(p_intf), &p_sys->last_viewpoint );
            var_GetCoords( p_this, "mouse-moved",
                           &p_sys->i_last_x, &p_sys->i_last_y );
        }
    }
    else
    {
        if( p_sys->b_button_pressed )
            p_sys->b_button_pressed = false;
    }
    vlc_mutex_unlock( &p_sys->lock );

    return VLC_SUCCESS;
}

static int InputEvent( vlc_object_t *p_this, char const *psz_var,
                       vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    input_thread_t *p_input = (input_thread_t *)p_this;
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;

    (void) psz_var; (void) oldval;

    switch( val.i_int )
    {
      case INPUT_EVENT_VOUT:
        /* intf-event is serialized against itself and is the sole user of
         * p_sys->p_vout. So there is no need to acquire the lock currently. */
        if( p_sys->p_vout != NULL )
        {   /* /!\ Beware of lock inversion with var_DelCallback() /!\ */
            var_DelCallback( p_sys->p_vout, "mouse-moved", MovedEvent,
                             p_intf );
            var_DelCallback( p_sys->p_vout, "mouse-button-down", ButtonEvent,
                             p_intf );
            vlc_object_release( p_sys->p_vout );
        }

        p_sys->p_vout = input_GetVout( p_input );
        if( p_sys->p_vout != NULL )
        {
            var_AddCallback( p_sys->p_vout, "mouse-moved", MovedEvent,
                             p_intf );
            var_AddCallback( p_sys->p_vout, "mouse-button-down", ButtonEvent,
                             p_intf );
        }
        break;
    }
    return VLC_SUCCESS;
}

static int PlaylistEvent( vlc_object_t *p_this, char const *psz_var,
                          vlc_value_t oldval, vlc_value_t val, void *p_data )
{
    intf_thread_t *p_intf = p_data;
    intf_sys_t *p_sys = p_intf->p_sys;
    input_thread_t *p_input = val.p_address;

    (void) p_this; (void) psz_var;

    if( p_sys->p_input != NULL )
    {
        assert( p_sys->p_input == oldval.p_address );
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );
    }

    p_sys->p_input = p_input;

    if( p_input != NULL )
        var_AddCallback( p_input, "intf-event", InputEvent, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * OpenIntf: initialize interface
 *****************************************************************************/
static int Open ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    /* Allocate instance and initialize some members */
    intf_sys_t *p_sys = p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( unlikely(p_sys == NULL) )
        return VLC_ENOMEM;

    // Configure the module
    vlc_mutex_init( &p_sys->lock );
    p_sys->p_input = NULL;
    p_sys->p_vout = NULL;
    p_sys->b_button_pressed = false;

    var_AddCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * CloseIntf: destroy dummy interface
 *****************************************************************************/
static void Close ( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* Destroy the callbacks (the order matters!) */
    var_DelCallback( pl_Get(p_intf), "input-current", PlaylistEvent, p_intf );

    if( p_sys->p_input != NULL )
        var_DelCallback( p_sys->p_input, "intf-event", InputEvent, p_intf );

    if( p_sys->p_vout )
    {
        var_DelCallback( p_sys->p_vout, "mouse-moved", MovedEvent, p_intf );
        var_DelCallback( p_sys->p_vout, "mouse-button-down",
                         ButtonEvent, p_intf );
        vlc_object_release( p_sys->p_vout );
    }

    /* Destroy structure */
    vlc_mutex_destroy( &p_sys->lock );
    free( p_sys );
}

vlc_module_begin ()
    set_shortname( N_("vrnav"))
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_CONTROL )
    set_description( N_("VR/360° mouse navigation interface") )
    set_capability( "interface", 0 )
    set_callbacks( Open, Close )
    add_shortcut( "vrnav" )
vlc_module_end ()
