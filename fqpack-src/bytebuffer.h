/**
 * @file bytebuffer.h
 *
 * @date Dec 29, 2011
 * @author seth
 *
 * Copyright (c) 2011, Seth Hillbrand
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright notice, this list of 
 * conditions and the following disclaimer.  Redistributions in binary form must reproduce 
 * the above copyright notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY 
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */

#ifndef BYTEBUFFER_H_
#define BYTEBUFFER_H_

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct
{
	size_t 	alloc;
	size_t 	length;
	union
	{
		char		*content;
		bool		*bool_content;
		uint8_t		*byte_content;
		uint16_t 	*halfword_content;
		uint32_t	*word_content;
	};
} byte_buffer_t;

byte_buffer_t* bb_new(const size_t m_len);
void bb_free(byte_buffer_t*);
bool bb_ensure_len(byte_buffer_t*, const size_t);
bool bb_clear(byte_buffer_t*);
bool bb_append(byte_buffer_t*, const void*, size_t);
bool bb_append_char(byte_buffer_t *m_buf, const char m_char);
bool bb_append_bool(byte_buffer_t *m_buf, const bool m_bool);
bool bb_append_word(byte_buffer_t *m_buf, const uint32_t m_word);


#define bb_expand(bb,len) bb_ensure_len((bb),(len)+(bb->alloc))
#define bb_reset(bb) ((void)((bb)?(bb)->length=0:0U))
#define bb_sufficient(bb,n) ((bb)?((bb)->alloc - (bb)->length) > (n):0U)


#endif /* BYTEBUFFER_H_ */
