/**
 * @file arithcoder.h
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

#ifndef ARITHCODER_H_
#define ARITHCODER_H_

#include <stdint.h>
#include <stdbool.h>

#define AC_OVERFLOW_MASK 	UINT64_C(0xFF000000)
#define AC_OVERFLOW_THRESH 	UINT64_C(0x00FFFFFF)
#define AC_RANGE_THRESH 	UINT32_C(0x00FFFFFF)

typedef struct
{
    uint64_t		ac_low;
    uint32_t		ac_code;
    uint32_t		ac_ffnum;
    uint32_t		ac_cache;
    uint32_t		ac_range;

    const uint8_t 	*in_buf;
    uint8_t 		*out_buf;

    uint8_t			*out_end;
    uint8_t 		*out_start;
} arithcoder_t;

arithcoder_t *arithcoder_new();
void ac_initialize_encoder(arithcoder_t *m_encoder, uint8_t *m_output, size_t m_len);
size_t ac_finialize_encoder(arithcoder_t *m_coder);
void ac_initialize_decoder(arithcoder_t *m_coder, uint8_t *m_input);

#define AC_WRITE_BYTE(m_coder, m_byte) *((m_coder)->out_buf)++ = (m_byte)
#define AC_READ_BYTE(m_coder) *((m_coder)->in_buf)++
#define AC_VALID(m_coder) ((m_coder)->output < (m_coder)->output_end)


static inline void ac_adjust_low(arithcoder_t *m_coder)
{
	if ((m_coder->ac_low ^ AC_OVERFLOW_MASK) > AC_OVERFLOW_THRESH)
	{
		uint8_t char_out;
		uint32_t upper_low = (m_coder->ac_low >> 32);
		AC_WRITE_BYTE(m_coder, (uint8_t)(m_coder->ac_cache + upper_low));
		char_out = (uint8_t) (UINT32_C(0xff) + upper_low);
		while (m_coder->ac_ffnum)
		{
			AC_WRITE_BYTE(m_coder, char_out);
			m_coder->ac_ffnum--;
		}

		m_coder->ac_cache = (uint32_t) (m_coder->ac_low) >> 24;
	}
	else
		m_coder->ac_ffnum++;
	m_coder->ac_low = (uint32_t) (m_coder->ac_low << 8);
}

static inline void ac_encode_bit0(arithcoder_t *m_coder, uint32_t m_prob)
{
	m_coder->ac_range = (m_coder->ac_range >> 12) * m_prob;
	while (m_coder->ac_range <= AC_RANGE_THRESH)
	{
		ac_adjust_low(m_coder);
		m_coder->ac_range <<= 8;
	}
}

static inline void ac_encode_bit1(arithcoder_t *m_coder, uint32_t m_prob)
{
	uint32_t range = (m_coder->ac_range >> 12) * m_prob;
	m_coder->ac_low += range;
	m_coder->ac_range -= range;
	while (m_coder->ac_range <= AC_RANGE_THRESH)
	{
		ac_adjust_low(m_coder);
		m_coder->ac_range <<= 8;
	}
}

static inline void ac_store_bit(arithcoder_t *m_coder, uint32_t m_bit)
{
	if (m_bit)
		ac_encode_bit1(m_coder, 2048);
	else
		ac_encode_bit0(m_coder, 2048);
}

static inline void ac_store_byte(arithcoder_t *m_coder, uint8_t m_byte)
{
	for (int8_t bit = 7; bit >= 0; --bit)
	{
		ac_store_bit(m_coder, m_byte & (1 << bit));
	}
}

static inline void ac_store_halfword(arithcoder_t *m_coder, uint16_t m_halfword)
{
	for (int8_t bit = 15; bit >= 0; --bit)
	{
		ac_store_bit(m_coder, m_halfword & (1 << bit));
	}
}

static inline void ac_store_word(arithcoder_t *m_coder, uint32_t m_word)
{
	for (int8_t bit = 31; bit >= 0; --bit)
	{
		ac_store_bit(m_coder, m_word & (1 << bit));
	}
}


static inline uint8_t ac_decode_bit(arithcoder_t *m_coder, uint32_t m_prob)
{
	uint32_t range = (m_coder->ac_range >> 12) * m_prob;
	if (m_coder->ac_code >= range)
	{
		m_coder->ac_code -= range;
		m_coder->ac_range -= range;
		while (m_coder->ac_range <= AC_RANGE_THRESH)
		{
			m_coder->ac_code = (m_coder->ac_code << 8) | AC_READ_BYTE(m_coder);
			m_coder->ac_range <<= 8;
		}
		return 1;
	}
	m_coder->ac_range = range;
	while (m_coder->ac_range <= AC_RANGE_THRESH)
	{
		m_coder->ac_code = (m_coder->ac_code << 8) | AC_READ_BYTE(m_coder);
		m_coder->ac_range <<= 8;
	}
	return 0;
}

static inline uint8_t ac_unstore_bit(arithcoder_t *m_coder)
{
	return ac_decode_bit(m_coder, 2048);
}

static inline uint8_t ac_unstore_byte(arithcoder_t *m_coder)
{
	uint8_t byte = 0;
	for (int8_t bit = 7; bit >= 0; --bit)
	{
		byte |= ac_unstore_bit(m_coder) << bit;
	}
	return byte;
}

static inline uint16_t ac_unstore_halfword(arithcoder_t *m_coder)
{
	uint16_t halfword = 0;
	for (int8_t bit = 15; bit >= 0; --bit)
	{
		halfword |= ac_unstore_bit(m_coder) << bit;
	}
	return halfword;
}

static inline uint32_t ac_unstore_word(arithcoder_t *m_coder)
{
	uint32_t word = 0;
	for (int8_t bit = 31; bit >= 0; --bit)
	{
		word |= ac_unstore_bit(m_coder) << bit;
	}
	return word;
}
#endif /* ARITHCODER_H_ */
