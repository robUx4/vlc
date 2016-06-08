/*****************************************************************************
 * block.c: Data blocks management functions
 *****************************************************************************
 * Copyright (C) 2003-2004 VLC authors and VideoLAN
 * Copyright (C) 2007-2009 Rémi Denis-Courmont
 *
 * Authors: Laurent Aimar <fenrir@videolan.org>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <vlc_common.h>
#include <vlc_block.h>
#include <vlc_fs.h>

/**
 * @section Block handling functions.
 */

#ifndef NDEBUG
static void BlockNoRelease( block_t *b )
{
    fprintf( stderr, "block %p has no release callback! This is a bug!\n",
             (void *) b );
    abort();
}

static void block_Check (block_t *block)
{
    while (block != NULL)
    {
        unsigned char *start = block->p_start;
        unsigned char *end = block->p_start + block->i_size;
        unsigned char *bufstart = block->p_buffer;
        unsigned char *bufend = block->p_buffer + block->i_buffer;

        assert (block->pf_release != BlockNoRelease);
        assert (start <= end);
        assert (bufstart <= bufend);
        assert (bufstart >= start);
        assert (bufend <= end);

        block = block->p_next;
    }
}

static void block_Invalidate (block_t *block)
{
    block->p_next = NULL;
    block_Check (block);
    block->pf_release = BlockNoRelease;
}
#else
# define block_Check(b) ((void)(b))
# define block_Invalidate(b) ((void)(b))
#endif

void block_Init( block_t *restrict b, void *buf, size_t size )
{
    /* Fill all fields to their default */
    b->p_next = NULL;
    b->p_buffer = buf;
    b->i_buffer = size;
    b->p_start = buf;
    b->i_size = size;
    b->i_flags = 0;
    b->i_nb_samples = 0;
    b->i_pts =
    b->i_dts = VLC_TS_INVALID;
    b->i_length = 0;
#ifndef NDEBUG
    b->pf_release = BlockNoRelease;
#endif
}

static void block_generic_Release (block_t *block)
{
    /* That is always true for blocks allocated with block_Alloc(). */
    assert (block->p_start == (unsigned char *)(block + 1));
    block_Invalidate (block);
    free (block);
}

static void BlockMetaCopy( block_t *restrict out, const block_t *in )
{
    out->p_next    = in->p_next;
    out->i_nb_samples = in->i_nb_samples;
    out->i_dts     = in->i_dts;
    out->i_pts     = in->i_pts;
    out->i_flags   = in->i_flags;
    out->i_length  = in->i_length;
}

/** Initial memory alignment of data block.
 * @note This must be a multiple of sizeof(void*) and a power of two.
 * libavcodec AVX optimizations require at least 32-bytes. */
#define BLOCK_ALIGN        32

/** Initial reserved header and footer size. */
#define BLOCK_PADDING      32

block_t *block_Alloc (size_t size)
{
    /* 2 * BLOCK_PADDING: pre + post padding */
    const size_t alloc = sizeof (block_t) + BLOCK_ALIGN + (2 * BLOCK_PADDING)
                       + size;
    if (unlikely(alloc <= size))
        return NULL;

    block_t *b = malloc (alloc);
    if (unlikely(b == NULL))
        return NULL;

    block_Init (b, b + 1, alloc - sizeof (*b));
    static_assert ((BLOCK_PADDING % BLOCK_ALIGN) == 0,
                   "BLOCK_PADDING must be a multiple of BLOCK_ALIGN");
    b->p_buffer += BLOCK_PADDING + BLOCK_ALIGN - 1;
    b->p_buffer = (void *)(((uintptr_t)b->p_buffer) & ~(BLOCK_ALIGN - 1));
    b->i_buffer = size;
    b->pf_release = block_generic_Release;
    return b;
}

block_t *block_TryRealloc (block_t *p_block, ssize_t i_prebody, size_t i_body)
{
    block_Check( p_block );

    /* Corner case: empty block requested */
    if( i_prebody <= 0 && i_body <= (size_t)(-i_prebody) )
        i_prebody = i_body = 0;

    assert( p_block->p_start <= p_block->p_buffer );
    assert( p_block->p_start + p_block->i_size
                                    >= p_block->p_buffer + p_block->i_buffer );

    /* First, shrink payload */

    /* Pull payload start */
    if( i_prebody < 0 )
    {
        if( p_block->i_buffer >= (size_t)-i_prebody )
        {
            p_block->p_buffer -= i_prebody;
            p_block->i_buffer += i_prebody;
        }
        else /* Discard current payload entirely */
            p_block->i_buffer = 0;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Trim payload end */
    if( p_block->i_buffer > i_body )
        p_block->i_buffer = i_body;

    size_t requested = i_prebody + i_body;

    if( p_block->i_buffer == 0 )
    {   /* Corner case: nothing to preserve */
        if( requested <= p_block->i_size )
        {   /* Enough room: recycle buffer */
            size_t extra = p_block->i_size - requested;

            p_block->p_buffer = p_block->p_start + (extra / 2);
            p_block->i_buffer = requested;
            return p_block;
        }

        /* Not enough room: allocate a new buffer */
        block_t *p_rea = block_Alloc( requested );
        if( p_rea == NULL )
            return NULL;

        BlockMetaCopy( p_rea, p_block );
        block_Release( p_block );
        return p_rea;
    }

    uint8_t *p_start = p_block->p_start;
    uint8_t *p_end = p_start + p_block->i_size;

    /* Second, reallocate the buffer if we lack space. */
    assert( i_prebody >= 0 );
    if( (size_t)(p_block->p_buffer - p_start) < (size_t)i_prebody
     || (size_t)(p_end - p_block->p_buffer) < i_body )
    {
        block_t *p_rea = block_Alloc( requested );
        if( p_rea == NULL )
            return NULL;

        memcpy( p_rea->p_buffer + i_prebody, p_block->p_buffer,
                p_block->i_buffer );
        BlockMetaCopy( p_rea, p_block );
        block_Release( p_block );
        return p_rea;
    }

    /* Third, expand payload */

    /* Push payload start */
    if( i_prebody > 0 )
    {
        p_block->p_buffer -= i_prebody;
        p_block->i_buffer += i_prebody;
        i_body += i_prebody;
        i_prebody = 0;
    }

    /* Expand payload to requested size */
    p_block->i_buffer = i_body;

    return p_block;
}

block_t *block_Realloc (block_t *block, ssize_t prebody, size_t body)
{
    block_t *rea = block_TryRealloc (block, prebody, body);
    if (rea == NULL)
        block_Release(block);
    return rea;
}

static void block_heap_Release (block_t *block)
{
    block_Invalidate (block);
    free (block->p_start);
    free (block);
}

/**
 * Creates a block from a heap allocation.
 * This is provided by LibVLC so that manually heap-allocated blocks can safely
 * be deallocated even after the origin plugin has been unloaded from memory.
 *
 * When block_Release() is called, VLC will free() the specified pointer.
 *
 * @param addr base address of the heap allocation (will be free()'d)
 * @param length bytes length of the heap allocation
 * @return NULL in case of error (ptr free()'d in that case), or a valid
 * block_t pointer.
 */
block_t *block_heap_Alloc (void *addr, size_t length)
{
    block_t *block = malloc (sizeof (*block));
    if (block == NULL)
    {
        free (addr);
        return NULL;
    }

    block_Init (block, addr, length);
    block->pf_release = block_heap_Release;
    return block;
}

#ifdef HAVE_MMAP
# include <sys/mman.h>

static void block_mmap_Release (block_t *block)
{
    block_Invalidate (block);
    munmap (block->p_start, block->i_size);
    free (block);
}

/**
 * Creates a block from a virtual address memory mapping (mmap).
 * This is provided by LibVLC so that mmap blocks can safely be deallocated
 * even after the allocating plugin has been unloaded from memory.
 *
 * @param addr base address of the mapping (as returned by mmap)
 * @param length length (bytes) of the mapping (as passed to mmap)
 * @return NULL if addr is MAP_FAILED, or an error occurred (in the later
 * case, munmap(addr, length) is invoked before returning).
 */
block_t *block_mmap_Alloc (void *addr, size_t length)
{
    if (addr == MAP_FAILED)
        return NULL;

    long page_mask = sysconf(_SC_PAGESIZE) - 1;
    size_t left = ((uintptr_t)addr) & page_mask;
    size_t right = (-length) & page_mask;

    block_t *block = malloc (sizeof (*block));
    if (block == NULL)
    {
        munmap (addr, length);
        return NULL;
    }

    block_Init (block, ((char *)addr) - left, left + length + right);
    block->p_buffer = addr;
    block->i_buffer = length;
    block->pf_release = block_mmap_Release;
    return block;
}
#else
block_t *block_mmap_Alloc (void *addr, size_t length)
{
    (void)addr; (void)length; return NULL;
}
#endif

#ifdef HAVE_SYS_SHM_H
# include <sys/shm.h>

typedef struct block_shm_t
{
    block_t     self;
    void       *base_addr;
} block_shm_t;

static void block_shm_Release (block_t *block)
{
    block_shm_t *p_sys = (block_shm_t *)block;

    shmdt (p_sys->base_addr);
    free (p_sys);
}

/**
 * Creates a block from a System V shared memory segment (shmget()).
 * This is provided by LibVLC so that segments can safely be deallocated
 * even after the allocating plugin has been unloaded from memory.
 *
 * @param addr base address of the segment (as returned by shmat())
 * @param length length (bytes) of the segment (as passed to shmget())
 * @return NULL if an error occurred (in that case, shmdt(addr) is invoked
 * before returning NULL).
 */
block_t *block_shm_Alloc (void *addr, size_t length)
{
    block_shm_t *block = malloc (sizeof (*block));
    if (unlikely(block == NULL))
    {
        shmdt (addr);
        return NULL;
    }

    block_Init (&block->self, (uint8_t *)addr, length);
    block->self.pf_release = block_shm_Release;
    block->base_addr = addr;
    return &block->self;
}
#else
block_t *block_shm_Alloc (void *addr, size_t length)
{
    (void) addr; (void) length;
    abort ();
}
#endif


#ifdef _WIN32
ssize_t pread (int fd, void *buf, size_t count, off_t offset)
{
    if ( lseek (fd, offset, SEEK_SET) != offset ) {
        return -1;
    }
    return read( fd, buf, count );
}
#endif

/**
 * Loads a file into a block of memory through a file descriptor.
 * If possible a private file mapping is created. Otherwise, the file is read
 * normally. This function is a cancellation point.
 *
 * @note On 32-bits platforms,
 * this function will not work for very large files,
 * due to memory space constraints.
 *
 * @param fd file descriptor to load from
 * @return a new block with the file content at p_buffer, and file length at
 * i_buffer (release it with block_Release()), or NULL upon error (see errno).
 */
block_t *block_File (int fd)
{
    size_t length;
    struct stat st;

    /* First, get the file size */
    if (fstat (fd, &st))
        return NULL;

    /* st_size is meaningful for regular files, shared memory and typed memory.
     * It's also meaning for symlinks, but that's not possible with fstat().
     * In other cases, it's undefined, and we should really not go further. */
#ifndef S_TYPEISSHM
# define S_TYPEISSHM( buf ) (0)
#endif
    if (S_ISDIR (st.st_mode))
    {
        errno = EISDIR;
        return NULL;
    }
    if (!S_ISREG (st.st_mode) && !S_TYPEISSHM (&st))
    {
        errno = ESPIPE;
        return NULL;
    }

    /* Prevent an integer overflow in mmap() and malloc() */
    if ((uintmax_t)st.st_size >= SIZE_MAX)
    {
        errno = ENOMEM;
        return NULL;
    }
    length = (size_t)st.st_size;

#ifdef HAVE_MMAP
    if (length > 0)
    {
        void *addr;

        addr = mmap (NULL, length, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
        if (addr != MAP_FAILED)
            return block_mmap_Alloc (addr, length);
    }
#endif

    /* If mmap() is not implemented by the OS _or_ the filesystem... */
    block_t *block = block_Alloc (length);
    if (block == NULL)
        return NULL;
    block_cleanup_push (block);

    for (size_t i = 0; i < length;)
    {
        ssize_t len = pread (fd, block->p_buffer + i, length - i, i);
        if (len == -1)
        {
            block_Release (block);
            block = NULL;
            break;
        }
        i += len;
    }
    vlc_cleanup_pop ();
    return block;
}

/**
 * Loads a file into a block of memory from the file path.
 * See also block_File().
 */
block_t *block_FilePath (const char *path)
{
    int fd = vlc_open (path, O_RDONLY);
    if (fd == -1)
        return NULL;

    block_t *block = block_File (fd);
    vlc_close (fd);
    return block;
}
