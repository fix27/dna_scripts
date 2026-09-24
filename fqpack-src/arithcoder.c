/**
 * @file arithcoder.c
 *
 * @date Dec 22, 2011
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

#include <stdint.h>
#include <stdbool.h>

#include <util.h>
#include <arithcoder.h>

void ac_initialize_encoder(arithcoder_t *m_encoder, uint8_t *m_output, size_t m_len)
{
	m_encoder->out_start = m_output;
	m_encoder->out_buf = m_output;
	m_encoder->out_end = m_output + m_len - 0x10;

	m_encoder->ac_low = 0;
	m_encoder->ac_ffnum = 0;
	m_encoder->ac_cache = 0;
	m_encoder->ac_range = UINT32_MAX;
}

size_t ac_finialize_encoder(arithcoder_t *m_coder)
{
	 m_coder->ac_low += (m_coder->ac_range >> 1);

	ac_adjust_low(m_coder);
	ac_adjust_low(m_coder);
	ac_adjust_low(m_coder);
	ac_adjust_low(m_coder);
	ac_adjust_low(m_coder);

	return (size_t) (m_coder->out_buf - m_coder->out_start);
}


void ac_initialize_decoder(arithcoder_t *m_coder, uint8_t *m_input)
{
	 uint32_t ac_code = 0;
	 m_coder->in_buf = m_input;

	m_coder->ac_ffnum = 0;
	m_coder->ac_cache = 0;
	m_coder->ac_range = UINT32_MAX;

	ac_code = (ac_code << 8) | AC_READ_BYTE(m_coder);
	ac_code = (ac_code << 8) | AC_READ_BYTE(m_coder);
	ac_code = (ac_code << 8) | AC_READ_BYTE(m_coder);
	ac_code = (ac_code << 8) | AC_READ_BYTE(m_coder);
	ac_code = (ac_code << 8) | AC_READ_BYTE(m_coder);

	m_coder->ac_code = ac_code;
}
