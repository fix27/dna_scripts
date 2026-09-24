/**
 * @file bytebuffer.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include <util.h>
#include <bytebuffer.h>

#define BB_DEFAULT_SIZE 1024 * 1024 /// 1MB default size

byte_buffer_t *bb_new(const size_t m_len)
{
	byte_buffer_t *ret_buf = (byte_buffer_t*) fqpack_aligned_malloc(sizeof(byte_buffer_t), true);
	if (ret_buf == NULL)
		return NULL;
	ret_buf->alloc = 0;
	ret_buf->length = 0;
	ret_buf->content = NULL;
	if (m_len) bb_ensure_len(ret_buf, m_len);

	return ret_buf;
}

bool bb_ensure_len(byte_buffer_t *m_buf, const size_t m_len)
{
	size_t length = m_len;
	char* fresh_data;
	if (!m_buf) return false;
	if (m_buf->alloc >= m_len) return true;

	if (!length)
	{
		if (m_buf->alloc) length = m_buf->alloc << 1;
		else length = BB_DEFAULT_SIZE;
	}

	fresh_data = fqpack_aligned_malloc(length, true);
	if (m_buf->alloc && m_buf->length && m_buf->content)
	{
		memcpy(fresh_data, m_buf->content,	m_buf->length);
	}

	if (m_buf->content) free(m_buf->content);

	m_buf->content = fresh_data;
	m_buf->alloc = length;
	return true;
}

void bb_free(byte_buffer_t *m_buf)
{
	if (!m_buf) return;

	if (m_buf->content != NULL)
		free(m_buf->content);
	free(m_buf);
}

bool bb_append_char(byte_buffer_t *m_buf, const char m_char)
{
	while (!bb_sufficient(m_buf, sizeof(char)))
		if (!bb_expand(m_buf, m_buf->alloc)) return false;

	m_buf->content[m_buf->length] = m_char;
	m_buf->length++;
	m_buf->content[m_buf->length] = '\0';
	return true;
}

bool bb_append_bool(byte_buffer_t *m_buf, const bool m_bool)
{
	while (!bb_sufficient(m_buf, sizeof(bool)))
		if (!bb_expand(m_buf, m_buf->alloc)) return false;

	*(bool*)(m_buf->content + m_buf->length) = m_bool;
	m_buf->length += sizeof(bool);
	return true;
}

bool bb_clear(byte_buffer_t *m_buf)
{
	if (!m_buf) return false;
	memset(m_buf->byte_content, 0, m_buf->alloc);
	m_buf->length = 0;
	return true;
}

bool bb_append_word(byte_buffer_t *m_buf, const uint32_t m_word)
{
	while (!bb_sufficient(m_buf, sizeof(uint32_t)))
		if (!bb_expand(m_buf, m_buf->alloc)) return false;

	*(uint32_t*)(m_buf->content + m_buf->length) = m_word;
	m_buf->length += sizeof(m_word);
	return true;
}

bool bb_append(byte_buffer_t *m_buf, const void *m_data, const size_t m_len)
{
	size_t length = m_len;

	if (!m_buf || !m_data) return false;

	if (!length) length = strlen((char*) m_data);

	while (!bb_sufficient(m_buf,(length+1)))
	{
		if (!bb_expand(m_buf, m_buf->alloc)) return false;
	}

	memcpy((void*) &m_buf->content[m_buf->length], (void*) m_data, length);
	m_buf->length += length;
	m_buf->content[m_buf->length] = '\0';
	return true;
}
