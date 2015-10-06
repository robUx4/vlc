/*****************************************************************************
 * vlc_codec.h: Definition of the decoder and encoder structures
 *****************************************************************************
 * Copyright (C) 1999-2003 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Gildas Bazin <gbazin@netcourrier.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef VLC_CODEC_H
#define VLC_CODEC_H 1

#include <vlc_block.h>
#include <vlc_es.h>
#include <vlc_picture.h>
#include <vlc_subpicture.h>

/**
 * \defgroup codec Codec
 * Decoders and encoders
 * @{
 * \file
 * Decoder and encoder modules interface
 *
 * \defgroup decoder Decoder
 * Audio, video and text decoders
 * @{
 */

typedef struct decoder_owner_sys_t decoder_owner_sys_t;

/*
 * BIG FAT WARNING : the code relies in the first 4 members of filter_t
 * and decoder_t to be the same, so if you have anything to add, do it
 * at the end of the structure.
 */
struct decoder_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    decoder_sys_t *     p_sys;

    /* Input format ie from demuxer (XXX: a lot of field could be invalid) */
    es_format_t         fmt_in;

    /* Output format of decoder/packetizer */
    es_format_t         fmt_out;

    /* Tell the decoder if it is allowed to drop frames */
    bool                b_frame_drop_allowed;

    /* */
    picture_t *         ( * pf_decode_video )( decoder_t *, block_t ** );
    block_t *           ( * pf_decode_audio )( decoder_t *, block_t ** );
    subpicture_t *      ( * pf_decode_sub)   ( decoder_t *, block_t ** );
    block_t *           ( * pf_packetize )   ( decoder_t *, block_t ** );

    /* Closed Caption (CEA 608/708) extraction.
     * If set, it *may* be called after pf_decode_video/pf_packetize
     * returned data. It should return CC for the pictures returned by the
     * last pf_packetize/pf_decode_video call only,
     * pb_present will be used to known which cc channel are present (but
     * globaly, not necessary for the current packet */
    block_t *           ( * pf_get_cc )      ( decoder_t *, bool pb_present[4] );

    /* Meta data at codec level
     *  The decoder owner set it back to NULL once it has retreived what it needs.
     *  The decoder owner is responsible of its release except when you overwrite it.
     */
    vlc_meta_t          *p_description;

    /*
     * Owner fields
     * XXX You MUST not use them directly.
     */

    /* Video output callbacks
     * XXX use decoder_NewPicture */
    int             (*pf_vout_format_update)( decoder_t * );
    picture_t      *(*pf_vout_buffer_new)( decoder_t * );

    /**
     * Number of extra (ie in addition to the DPB) picture buffers
     * needed for decoding.
     */
    int             i_extra_picture_buffers;

    /* Audio output callbacks */
    int             (*pf_aout_format_update)( decoder_t * );

    /* SPU output callbacks
     * XXX use decoder_NewSubpicture */
    subpicture_t   *(*pf_spu_buffer_new)( decoder_t *, const subpicture_updater_t * );

    /* Input attachments
     * XXX use decoder_GetInputAttachments */
    int             (*pf_get_attachments)( decoder_t *p_dec, input_attachment_t ***ppp_attachment, int *pi_attachment );

    /* Display date
     * XXX use decoder_GetDisplayDate */
    mtime_t         (*pf_get_display_date)( decoder_t *, mtime_t );

    /* Display rate
     * XXX use decoder_GetDisplayRate */
    int             (*pf_get_display_rate)( decoder_t * );

    /* Private structure for the owner of the decoder */
    decoder_owner_sys_t *p_owner;

    bool                b_error;
};

/**
 * @}
 */

/**
 * \defgroup encoder Encoder
 * Audio, video and text encoders
 * @{
 */

struct encoder_t
{
    VLC_COMMON_MEMBERS

    /* Module properties */
    module_t *          p_module;
    encoder_sys_t *     p_sys;

    /* Properties of the input data fed to the encoder */
    es_format_t         fmt_in;

    /* Properties of the output of the encoder */
    es_format_t         fmt_out;

    block_t *           ( * pf_encode_video )( encoder_t *, picture_t * );
    block_t *           ( * pf_encode_audio )( encoder_t *, block_t * );
    block_t *           ( * pf_encode_sub )( encoder_t *, subpicture_t * );

    /* Common encoder options */
    int i_threads;               /* Number of threads to use during encoding */
    int i_iframes;               /* One I frame per i_iframes */
    int i_bframes;               /* One B frame per i_bframes */
    int i_tolerance;             /* Bitrate tolerance */

    /* Encoder config */
    config_chain_t *p_cfg;
};

/**
 * @}
 *
 * \ingroup decoder
 * @{
 */

/**
 * Updates the video output format.
 *
 * This function notifies the video output pipeline of a new video output
 * format (fmt_out.video). If there was no video output from the decoder so far
 * or if the video output format has changed, a new video output will be set
 * up. decoder_GetPicture() can then be used to allocate picture buffers.
 *
 * If the format is unchanged, this function has no effects and returns zero.
 *
 * \note
 * This function is not reentrant.
 *
 * @return 0 if the video output was set up succesfully, -1 otherwise.
 */
static inline int decoder_UpdateVideoFormat( decoder_t *dec )
{
    if( dec->pf_vout_format_update != NULL )
        return dec->pf_vout_format_update( dec );
    else
        return -1;
}

/**
 * Allocates an output picture buffer.
 *
 * This function pulls an output picture buffer for the decoder from the
 * buffer pool of the video output. The picture must be released with
 * picture_Release() when it is no longer referenced by the decoder.
 *
 * \note
 * This function is reentrant. However, decoder_UpdateVideoFormat() cannot be
 * used concurrently; the caller is responsible for serialization.
 *
 * \warning
 * The behaviour is undefined if decoder_UpdateVideoFormat() was not called or
 * if the last call returned an error.
 *
 * \return a picture buffer on success, NULL on error
 */
VLC_USED
static inline picture_t *decoder_GetPicture( decoder_t *dec )
{
    return dec->pf_vout_buffer_new( dec );
}

/**
 * Checks the format and allocates a picture buffer.
 *
 * This common helper function sets the output video output format and
 * allocates a picture buffer in that format. The picture must be released with
 * picture_Release() when it is no longer referenced by the decoder.
 *
 * \note
 * Lile decoder_UpdateVideoFormat(), this function is not reentrant.
 *
 * \return a picture buffer on success, NULL on error
 */
VLC_USED
static inline picture_t *decoder_NewPicture( decoder_t *dec )
{
    if( decoder_UpdateVideoFormat(dec) )
        return NULL;
    return decoder_GetPicture( dec );
}

/**
 * This function notifies the audio output pipeline of a new audio output
 * format (fmt_out.audio). If there is currently no audio output or if the
 * audio output format has changed, a new audio output will be set up.
 * @return 0 if the audio output is working, -1 if not. */
static inline int decoder_UpdateAudioFormat( decoder_t *dec )
{
    if( dec->pf_aout_format_update != NULL )
        return dec->pf_aout_format_update( dec );
    else
        return -1;
}

/**
 * This function will return a new audio buffer usable by a decoder as an
 * output buffer. It must be released with block_Release() or returned it to
 * the caller as a pf_decode_audio return value.
 */
VLC_API block_t * decoder_NewAudioBuffer( decoder_t *, int i_size ) VLC_USED;

/**
 * This function will return a new subpicture usable by a decoder as an output
 * buffer. You have to release it using subpicture_Delete() or by returning
 * it to the caller as a pf_decode_sub return value.
 */
VLC_API subpicture_t * decoder_NewSubpicture( decoder_t *, const subpicture_updater_t * ) VLC_USED;

/**
 * This function gives all input attachments at once.
 *
 * You MUST release the returned values
 */
VLC_API int decoder_GetInputAttachments( decoder_t *, input_attachment_t ***ppp_attachment, int *pi_attachment );

/**
 * This function converts a decoder timestamp into a display date comparable
 * to mdate().
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_API mtime_t decoder_GetDisplayDate( decoder_t *, mtime_t ) VLC_USED;

/**
 * This function returns the current input rate.
 * You MUST use it *only* for gathering statistics about speed.
 */
VLC_API int decoder_GetDisplayRate( decoder_t * ) VLC_USED;

/** @} */
/** @} */
#endif /* _VLC_CODEC_H */
