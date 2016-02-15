
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_keystore.h>

static int  Open(vlc_renderer *p_renderer, const vlc_renderer_item *p_item);
static void Close(vlc_renderer *p_renderer);

struct vlc_renderer_sys
{
};

vlc_module_begin()
    set_shortname(N_("sample name"))
    set_description(N_("sample desc"))
    set_category(CAT_ADVANCED)
    set_subcategory(SUBCAT_ADVANCED_NETWORK)
    set_capability("renderer", 100)
    set_callbacks(Open, Close)
vlc_module_end ()

static int
Start(vlc_renderer *p_renderer, input_thread_t *p_input)
{
    return VLC_SUCCESS;
}

void
Stop(vlc_renderer *p_renderer)
{
}

static int
VolumeChange(vlc_renderer *p_renderer, int i_volume)
{
    return VLC_SUCCESS;
}

int
VolumeMute(vlc_renderer *p_renderer, bool b_mute)
{
    return VLC_SUCCESS;
}

static int
Open(vlc_renderer *p_renderer, const vlc_renderer_item *p_item)
{
    p_renderer->p_sys = malloc(sizeof(vlc_renderer_sys));
    if (p_renderer->p_sys == NULL)
        return VLC_ENOMEM;

    p_renderer->pf_start = Start;
    p_renderer->pf_stop = Stop;
    p_renderer->pf_volume_change = VolumeChange;
    p_renderer->pf_volume_mute = VolumeMute;

    return VLC_SUCCESS;
}

static void
Close(vlc_renderer *p_renderer)
{
    free(p_renderer->p_sys);
}
