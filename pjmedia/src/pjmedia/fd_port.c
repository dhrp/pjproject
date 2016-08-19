/* $Id$ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny at prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#include <pjmedia/fd_port.h>
#include <pjmedia/errno.h>
#include <pj/assert.h>
#include <pj/pool.h>
#include <pj/string.h>

#if PJ_WIN32
#	define WIN32_LEAN_AND_MEAN
#	include <Windows.h>
#	include <io.h>
#else
#	include <unistd.h>
#	include <fcntl.h>
#	include <errno.h>
#endif

#define SIGNATURE   PJMEDIA_SIG_PORT_FD

#ifdef O_NONBLOCK

/** Buffer size */
#define FD_BUF_SIZE 131072U  /* 128 KiB */

/** Audio data cyclic buffer */
typedef struct fd_buf_t {
	/**
	 * Position in the buffer to write next data to.
	 * Position to read data from is ((FD_BUF_SIZE + pos - len) % FD_BUF_SIZE).
	 */
	pj_size_t       pos;

	/** Bytes stored in the buffer. */
	pj_size_t       len;
	char            data[FD_BUF_SIZE];
} fd_buf_t;

#endif  /* O_NONBLOCK */

/** File descriptor port implementation. */
struct fd_port {
	pjmedia_port base;

	int          fd_in;
	int          fd_out;
#ifdef O_NONBLOCK
	fd_buf_t    *buf_in;
	fd_buf_t    *buf_out;
#endif  /* O_NONBLOCK */
	pj_timestamp timestamp;
};

/** Get frame (blocking mode) */
static pj_status_t fd_port_get_frame_b(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
/** Put frame (blocking mode) */
static pj_status_t fd_port_put_frame_b(pjmedia_port *this_port, 
				  pjmedia_frame *frame);

#ifdef O_NONBLOCK
/** Get frame (non-blocking mode) */
static pj_status_t fd_port_get_frame_nb(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
/** Put frame (non-blocking mode) */
static pj_status_t fd_port_put_frame_nb(pjmedia_port *this_port, 
				  pjmedia_frame *frame);
#endif  /* O_NONBLOCK */

/** Destroy and free buffers */
static pj_status_t fd_port_on_destroy(pjmedia_port *this_port);


PJ_DEF(pj_status_t) pjmedia_fd_port_create( pj_pool_t *pool,
							unsigned sampling_rate,
							unsigned channel_count,
							unsigned samples_per_frame,
							unsigned bits_per_sample,
							int fd_in,
							int fd_out,
							unsigned flags,
							pjmedia_port **p_port )
{
	struct fd_port *fdport;
	const pj_str_t name = pj_str("fd-port");

	PJ_ASSERT_RETURN(pool && sampling_rate && channel_count &&
			samples_per_frame && bits_per_sample && p_port &&
			((fd_in >= 0) || (fd_out >= 0)),
		PJ_EINVAL);
#ifndef O_NONBLOCK
	PJ_ASSERT_RETURN(!(flags & PJMEDIA_FD_NONBLOCK), PJ_ENOTSUP);
#endif
#if !PJ_WIN32
	PJ_ASSERT_RETURN(!(flags & PJMEDIA_FD_HANDLES), PJ_EINVAL);
#endif

	fdport = PJ_POOL_ZALLOC_T(pool, struct fd_port);
	PJ_ASSERT_RETURN(fdport != NULL, PJ_ENOMEM);

	/* Create the port */
	pjmedia_port_info_init(&fdport->base.info, &name, SIGNATURE, sampling_rate,
		channel_count, bits_per_sample, samples_per_frame);

#if PJ_WIN32
	if (!(flags & PJMEDIA_FD_HANDLES))
	{
		/* PJMEDIA_FD_UNUSED should be equal to INVALID_HANLDE_VALUE */
		if (fd_in >= 0) fdport->fd_in = (int)_get_osfhandle(fd_in);
		if (fd_out >= 0) fdport->fd_out = (int)_get_osfhandle(fd_out);
	}
	else
#endif  /* PJ_WIN32 */
	{
		fdport->fd_in = fd_in;
		fdport->fd_out = fd_out;
	}

	if (fdport->fd_in >= 0) {
#ifdef O_NONBLOCK
		if (flags & PJMEDIA_FD_NONBLOCK) {
			int fl = fcntl(fdport->fd_in, F_GETFL);
			if (fl < 0) return pj_get_os_error();
			fl |= O_NONBLOCK;
			fl = fcntl(fdport->fd_in, F_SETFL, fl);
			if (fl < 0) return pj_get_os_error();
			fdport->buf_in = PJ_POOL_ALLOC_T(pool, fd_buf_t);
			PJ_ASSERT_RETURN(fdport->buf_in, PJ_ENOMEM);
			fdport->buf_in->pos = 0;
			fdport->buf_in->len = 0;
			fdport->base.get_frame = &fd_port_get_frame_nb;
		}
		else
#endif  /* O_NONBLOCK */
		{
			fdport->base.get_frame = &fd_port_get_frame_b;
		}
	}

	if (fdport->fd_out >= 0) {
#ifdef O_NONBLOCK
		if (flags & PJMEDIA_FD_NONBLOCK) {
			int fl = fcntl(fdport->fd_out, F_GETFL);
			if (fl < 0) return pj_get_os_error();
			fl |= O_NONBLOCK;
			fl = fcntl(fdport->fd_out, F_SETFL, fl);
			if (fl < 0) return pj_get_os_error();
			fdport->buf_out = PJ_POOL_ALLOC_T(pool, fd_buf_t);
			PJ_ASSERT_RETURN(fdport->buf_out, PJ_ENOMEM);
			fdport->buf_out->pos = 0;
			fdport->buf_out->len = 0;
			fdport->base.put_frame = &fd_port_put_frame_nb;
		}
		else
#endif  /* O_NONBLOCK */
		{
			fdport->base.put_frame = &fd_port_put_frame_b;
		}
	}

	fdport->base.on_destroy = &fd_port_on_destroy;

	*p_port = &fdport->base;

	return PJ_SUCCESS;
}


static pj_status_t fd_port_put_frame_b(pjmedia_port *this_port,
							pjmedia_frame *frame)
{
	const struct fd_port *fdport;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		PJ_EINVALIDOP);

	fdport = (struct fd_port*) this_port;
	if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO) {
#if PJ_WIN32
		DWORD written;
		BOOL succ = WriteFile((HANDLE)(pj_size_t)fdport->fd_out, frame->buf, (DWORD)frame->size, &written, NULL);
		if (!succ) return pj_get_os_error();
		if (written != frame->size) return PJ_EUNKNOWN;
#else  /* PJ_WIN32 */
		int ret = write(fdport->fd_out, frame->buf, frame->size);
		if (ret < 0) return pj_get_os_error();
		if (ret != frame->size) return PJ_EUNKNOWN;
#endif  /* PJ_WIN32 else */
	}

	return PJ_SUCCESS;
}


static pj_status_t fd_port_get_frame_b(pjmedia_port *this_port, 
							pjmedia_frame *frame)
{
	struct fd_port *fdport;
	pj_size_t size;
#if PJ_WIN32
	BOOL succ;
	DWORD ret;
#else  /* PJ_WIN32 */
	int ret;
#endif  /* PJ_WIN32 else */

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		PJ_EINVALIDOP);

	fdport = (struct fd_port*) this_port;
	size = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
#if PJ_WIN32
	succ = ReadFile((HANDLE)(pj_size_t)fdport->fd_in, frame->buf, (DWORD)size, &ret, NULL);
	if (!succ) ret = 0;
	if (succ) {
#else  /* PJ_WIN32 */
	ret = read(fdport->fd_in, frame->buf, size);
	if (ret > 0) {
#endif  /* PJ_WIN32 else */
		frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame->size = ret;
		/* Is this the correct timestamp calculation? */
		frame->timestamp.u64 = fdport->timestamp.u64;
		fdport->timestamp.u64 += PJMEDIA_PIA_SPF(&this_port->info);
	} else {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
		return pj_get_os_error();
	}

	return PJ_SUCCESS;
}


#ifdef O_NONBLOCK

/** Assumes buf->len + len <= FD_BUF_SIZE */
static void fd_buf_append(fd_buf_t *buf, const char *data, pj_size_t len)
{
	pj_size_t sz = len;
	if (buf->pos + sz > FD_BUF_SIZE)
		sz = FD_BUF_SIZE - buf->pos;
	memcpy(buf->data + buf->pos, data, sz);
	buf->pos += sz;
	buf->len += len;
	len -= sz;
	if (buf->pos >= FD_BUF_SIZE)
		buf->pos = 0;
	memcpy(buf->data, data + sz, len);
}


static pj_status_t fd_port_put_frame_nb(pjmedia_port *this_port,
							pjmedia_frame *frame)
{
	struct fd_port *fdport;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		PJ_EINVALIDOP);

	fdport = (struct fd_port*) this_port;
	/* Write out the buffer first */
	while (fdport->buf_out->len) {
		int ret;
		pj_size_t sz = fdport->buf_out->len;
		pj_size_t pos = (FD_BUF_SIZE + fdport->buf_out->pos - sz) % FD_BUF_SIZE;
		if (pos + sz > FD_BUF_SIZE) sz = FD_BUF_SIZE - pos;
		ret = write(fdport->fd_out, fdport->buf_out->data + pos, sz);
		if (ret <= 0) {
			if ((ret == 0) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
				break;
			return pj_get_os_error();
		}
		fdport->buf_out->len -= ret;
	}
	if (frame->type == PJMEDIA_FRAME_TYPE_AUDIO) {
		if (fdport->buf_out->len) {
			/* Append the frame to the buffer */
			if (fdport->buf_out->len + frame->size > FD_BUF_SIZE)
				return PJ_ETOOMANY;
			fd_buf_append(fdport->buf_out, (char*)frame->buf, frame->size);
		} else {
			/* Try to write the frame immediately */
			int ret = write(fdport->fd_out, frame->buf, frame->size);
			if ((ret < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK))
				return pj_get_os_error();
			if (ret < 0) ret = 0;
			if (ret < frame->size) {
				/* Buffer the rest */
				if (fdport->buf_out->len + frame->size - ret > FD_BUF_SIZE)
					return PJ_ETOOMANY;
				fd_buf_append(fdport->buf_out, (char*)frame->buf + ret, frame->size - ret);
			}
		}
	}

	return PJ_SUCCESS;
}


static pj_status_t fd_port_get_frame_nb(pjmedia_port *this_port, 
							pjmedia_frame *frame)
{
	struct fd_port *fdport;
	pj_size_t size;

	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		PJ_EINVALIDOP);

	fdport = (struct fd_port*) this_port;
	/* Buffer as much data first */
	while (fdport->buf_in->len < FD_BUF_SIZE) {
		int ret;
		pj_size_t sz = FD_BUF_SIZE - fdport->buf_in->len;
		if (fdport->buf_in->pos + sz > FD_BUF_SIZE)
			sz = FD_BUF_SIZE - fdport->buf_in->pos;
		ret = read(fdport->fd_in,
			fdport->buf_in->data + fdport->buf_in->pos, sz);
		if (ret <= 0) {
			if ((ret == 0) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
				break;
			return pj_get_os_error();
		}
		fdport->buf_in->pos += ret;
		fdport->buf_in->len -= ret;
		if (fdport->buf_in->pos >= FD_BUF_SIZE) fdport->buf_in->pos = 0;
	}
	size = PJMEDIA_PIA_AVG_FSZ(&this_port->info);
	if (fdport->buf_in->len >= size) {
		/* Enough data for whole frame */
		pj_size_t sz = size;
		pj_size_t pos = (FD_BUF_SIZE + fdport->buf_in->pos - fdport->buf_in->len) % FD_BUF_SIZE;
		if (pos + sz > FD_BUF_SIZE) sz = FD_BUF_SIZE - pos;
		memcpy(frame->buf, fdport->buf_in->data + pos, sz);
		if (sz < size)
			memcpy((char*)frame->buf + sz, fdport->buf_in->data, size - sz);
		fdport->buf_in->len -= size;
		frame->type = PJMEDIA_FRAME_TYPE_AUDIO;
		frame->size = size;
		/* Is this the correct timestamp calculation? */
		frame->timestamp.u64 = fdport->timestamp.u64;
		fdport->timestamp.u64 += PJMEDIA_PIA_SPF(&this_port->info);
	} else {
		frame->type = PJMEDIA_FRAME_TYPE_NONE;
		frame->size = 0;
	}

	return PJ_SUCCESS;
}

#endif  /* O_NONBLOCK */


/*
 * Destroy port.
 */
static pj_status_t fd_port_on_destroy(pjmedia_port *this_port)
{
	PJ_ASSERT_RETURN(this_port->info.signature == SIGNATURE,
		PJ_EINVALIDOP);

	/* Destroy signature */
	this_port->info.signature = 0;

	return PJ_SUCCESS;
}

