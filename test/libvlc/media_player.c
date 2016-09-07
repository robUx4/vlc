/*
 * media_player.c - libvlc smoke test
 *
 * $Id$
 */

/**********************************************************************
 *  Copyright (C) 2007 Rémi Denis-Courmont.                           *
 *  This program is free software; you can redistribute and/or modify *
 *  it under the terms of the GNU General Public License as published *
 *  by the Free Software Foundation; version 2 of the license, or (at *
 *  your option) any later version.                                   *
 *                                                                    *
 *  This program is distributed in the hope that it will be useful,   *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of    *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *  See the GNU General Public License for more details.              *
 *                                                                    *
 *  You should have received a copy of the GNU General Public License *
 *  along with this program; if not, you can get it from:             *
 *  http://www.gnu.org/copyleft/gpl.html                              *
 **********************************************************************/

#include "test.h"

#include <vlc_common.h>
#include <vlc_threads.h> /* for msleep */

static void wait_playing(libvlc_media_player_t *mp)
{
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mp);
    } while(state != libvlc_Playing &&
            state != libvlc_Error &&
            state != libvlc_Ended );

    state = libvlc_media_player_get_state (mp);
    assert(state == libvlc_Playing || state == libvlc_Ended);
}

static void wait_paused(libvlc_media_player_t *mp)
{
    libvlc_state_t state;
    do {
        state = libvlc_media_player_get_state (mp);
    } while(state != libvlc_Paused &&
            state != libvlc_Ended );

    state = libvlc_media_player_get_state (mp);
    assert(state == libvlc_Paused || state == libvlc_Ended);
}

/* Test a bunch of A/V properties. This most does nothing since the current
 * test file contains a dummy audio track. This is a smoke test. */
static void test_audio_video(libvlc_media_player_t *mp)
{
    bool fs = libvlc_get_fullscreen(mp);
    libvlc_set_fullscreen(mp, true);
    assert(libvlc_get_fullscreen(mp));
    libvlc_set_fullscreen(mp, false);
    assert(!libvlc_get_fullscreen(mp));
    libvlc_toggle_fullscreen(mp);
    assert(libvlc_get_fullscreen(mp));
    libvlc_toggle_fullscreen(mp);
    assert(!libvlc_get_fullscreen(mp));
    libvlc_set_fullscreen(mp, fs);
    assert(libvlc_get_fullscreen(mp) == fs);

    assert(libvlc_video_get_scale(mp) == 0.); /* default */
    libvlc_video_set_scale(mp, 0.); /* no-op */
    libvlc_video_set_scale(mp, 2.5);
    assert(libvlc_video_get_scale(mp) == 2.5);
    libvlc_video_set_scale(mp, 0.);
    libvlc_video_set_scale(mp, 0.); /* no-op */
    assert(libvlc_video_get_scale(mp) == 0.);

    libvlc_audio_output_device_t *aouts = libvlc_audio_output_device_enum(mp);
    for (libvlc_audio_output_device_t *e = aouts; e != NULL; e = e->p_next)
    {
        libvlc_audio_output_device_set( mp, NULL, e->psz_device );
    }
    libvlc_audio_output_device_list_release( aouts );
}

static void test_role(libvlc_media_player_t *mp)
{
    int role;

    /* Test default value */
    assert(libvlc_media_player_get_role(mp) == libvlc_role_Video);

    for (role = 0; libvlc_media_player_set_role(mp, role) == 0; role++)
        assert(libvlc_media_player_get_role(mp) == role);

    assert(role > libvlc_role_Last);
}

static void test_media_player_set_media(const char** argv, int argc)
{
    const char * file = test_default_sample;

    log ("Testing set_media\n");

    libvlc_instance_t *vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    libvlc_media_t *md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    libvlc_media_player_t *mp = libvlc_media_player_new (vlc);
    assert (mp != NULL);

    libvlc_media_player_set_media (mp, md);

    libvlc_media_release (md);

    libvlc_media_player_play (mp);

    wait_playing (mp);

    libvlc_media_player_stop (mp);
    libvlc_media_player_release (mp);
    libvlc_release (vlc);
}

static void test_media_player_play_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    log ("Testing play and pause of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    libvlc_media_player_play (mi);

    wait_playing (mi);

    libvlc_media_player_stop (mi);
    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

static void test_media_player_pause_stop(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_sample;

    log ("Testing pause and stop of %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    test_audio_video(mi);
    test_role(mi);

    libvlc_media_player_play (mi);
    log ("Waiting for playing\n");
    wait_playing (mi);
    test_audio_video(mi);

    libvlc_media_player_set_pause (mi, true);
    log ("Waiting for pause\n");
    wait_paused (mi);
    test_audio_video(mi);

    libvlc_media_player_stop (mi);
    test_audio_video(mi);

    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

static void
player_has_es(const struct libvlc_event_t *p_ev, void *p_data)
{
    (void) p_ev;
    vlc_sem_t *p_sem = p_data;
    vlc_sem_post(p_sem);
}

static void wait_done(void *p_data)
{
    vlc_sem_t *p_sem = p_data;
    vlc_sem_post(p_sem);
}

static void test_media_player_viewpoint(const char** argv, int argc)
{
    libvlc_instance_t *vlc;
    libvlc_media_t *md;
    libvlc_media_player_t *mi;
    const char * file = test_default_video;

    log ("Testing viewpoint for %s\n", file);

    vlc = libvlc_new (argc, argv);
    assert (vlc != NULL);

    md = libvlc_media_new_path (vlc, file);
    assert (md != NULL);

    mi = libvlc_media_player_new_from_media (md);
    assert (mi != NULL);

    libvlc_media_release (md);

    libvlc_video_viewpoint_t *p_viewpoint = libvlc_video_new_viewpoint();
    assert(p_viewpoint != NULL);

    /* test without the file loaded */
    assert(libvlc_video_get_viewpoint(mi, p_viewpoint));

    p_viewpoint->f_yaw = 1.57f;
    assert(!libvlc_video_set_viewpoint(mi, p_viewpoint));
    assert(p_viewpoint->f_yaw == 1.57f);

    libvlc_free(p_viewpoint);
    p_viewpoint = libvlc_video_new_viewpoint();
    assert(p_viewpoint != NULL);

    assert(libvlc_video_get_viewpoint(mi, p_viewpoint));
    assert(p_viewpoint->f_yaw == 1.57f);

    libvlc_media_player_play (mi);

    vlc_sem_t es_selected;
    vlc_sem_init(&es_selected, 0);

    libvlc_event_manager_t * em = libvlc_media_player_event_manager(mi);
    int val = libvlc_event_attach(em, libvlc_MediaPlayerVout, player_has_es, &es_selected);
    assert(val == 0);

    log ("Waiting for Vout\n");
    vlc_sem_wait(&es_selected);
    log ("Vout found\n");

    libvlc_event_detach(em, libvlc_MediaPlayerVout, player_has_es, &es_selected);

    assert(libvlc_video_get_viewpoint(mi, p_viewpoint));
    assert(p_viewpoint->f_yaw == 1.57f);

    p_viewpoint->f_yaw = 0.57f;
    assert(!libvlc_video_set_viewpoint(mi, p_viewpoint));

    /* let the value propagate */
    vlc_timer_t done_timer;
    int ret = vlc_timer_create( &done_timer, wait_done, &es_selected );
    assert(!ret);
    vlc_timer_schedule( done_timer, false, 1, CLOCK_FREQ / 5 );
    vlc_sem_wait(&es_selected);
    vlc_timer_destroy( done_timer );
    vlc_sem_destroy(&es_selected);

    assert(libvlc_video_get_viewpoint(mi, p_viewpoint));
    assert(p_viewpoint->f_yaw == 0.57f);

    libvlc_free(p_viewpoint);

    libvlc_media_player_stop (mi);
    libvlc_media_player_release (mi);
    libvlc_release (vlc);
}

int main (void)
{
    test_init();

    test_media_player_set_media (test_defaults_args, test_defaults_nargs);
    test_media_player_play_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_pause_stop (test_defaults_args, test_defaults_nargs);
    test_media_player_viewpoint (test_defaults_args, test_defaults_nargs);

    return 0;
}
