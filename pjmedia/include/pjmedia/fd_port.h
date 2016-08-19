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
#ifndef __PJMEDIA_FD_PORT_H__
#define __PJMEDIA_FD_PORT_H__

/**
 * @file fd_port.h
 * @brief File descriptor media port.
 */
#include <pjmedia/port.h>



/**
 * @defgroup PJMEDIA_FD_PORT File Descriptor Port
 * @ingroup PJMEDIA_PORT
 * @brief Reads from or writes audio frames to a file descriptor.
 * @{
 *
 * File descriptor port profides a simple way to pass audio frames
 * to/from an open file descriptor (a file, socket, pipe etc.)
 * which can be the same or a different process
 * (e.g. a speech recognizer or synthesizer).
 *
 * It can operate either in blocking or non-blocking mode. In blocking
 * mode the user must make sure the I/O operations complete quicky,
 * in non-blocking mode the data is buffered automatically.
 */


PJ_BEGIN_DECL

/**
 * File descriptor media port options.
 */
enum pjmedia_file_descriptor_option
{
	/**
	 * Use file descriptors in blocking mode.
	 */
	PJMEDIA_FD_BLOCK = 0,

	/**
	 * Use file-descriptors in non-blocking mode and buffer data.
	 */
	PJMEDIA_FD_NONBLOCK = 1,

	/**
	 * Parameters fd_in and fd_out are Windows file handles
	 * instead of integer file descriptors (Windows only).
	 */
	PJMEDIA_FD_HANDLES = 2
};


/**
 * Create file descriptor media port. 
 *
 * @param pool			Pool to allocate memory.
 * @param sampling_rate		Sampling rate of the port.
 * @param channel_count		Number of channels.
 * @param samples_per_frame	Number of samples per frame.
 * @param bits_per_sample	Number of bits per sample.
 * @param fd_in		Descriptor open for reading (i.e. audio source)
 *             		or -1 to disable audio input.
 * @param fd_out		Descriptor open for writing (i.e. audio sink)
 *             		or -1 to disable audio output.
 * @param flags		0 or PJMEDIA_FD_BLOCK for default, i.e. blocking mode,
 *             		or ORed pjmedia_file_descriptor_option flags:
 *             		- PJMEDIA_FD_NONBLOCK (1) for non-blocking mode
 *             		  (O_NONBLOCK is fcntl'ed on the descriptors if specified).
 *             		- PJMEDIA_FD_HANDLES (2): Parameters fd_in and fd_out are Windows
 *             		  file handles instead of integer file descriptors (Windows only).
 * @param p_port		Pointer to receive the port instance.
 *
 * @return			PJ_SUCCESS on success.
 */
PJ_DECL(pj_status_t) pjmedia_fd_port_create( pj_pool_t *pool,
							unsigned sampling_rate,
							unsigned channel_count,
							unsigned samples_per_frame,
							unsigned bits_per_sample,
							int fd_in,
							int fd_out,
							unsigned flags,
							pjmedia_port **p_port );


PJ_END_DECL

/**
 * @}
 */


#endif	/* __PJMEDIA_FD_PORT_H__ */

