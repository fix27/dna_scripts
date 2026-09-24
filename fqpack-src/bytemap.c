/**
 * @file bytemap.c
 *
 * @date Dec 20, 2011
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
#include <string.h>

#include "bytemap.h"

/**
 * Initializes the bytemap to the default values (ordered map, zero freq)
 * @param m_map Pointer to the map to initialize
 */
void bytemap_init(bytemap_t *m_map)
{
	for (uint_fast16_t i = 0; i < 256; i++)
	{
		m_map->map[i] = i;
		m_map->freq[i] = 0;
	}
}

/**
 * Simple insertion sort to sort the bytemap symbols by frequency in
 * descending order, i.e. the most frequent symbol will be at index 0
 * @param m_map Pointer to the bytemap
 */
void bytemap_sort_symbols(bytemap_t *m_map)
{
	uint_fast16_t i;
	int_fast16_t j;
	for (i = 1; i < 256; i++)
	{
		uint32_t tmp_freq = m_map->freq[i];
		uint8_t tmp_char = m_map->map[i];
		for (j = i - 1; j >= 0 && tmp_freq > m_map->freq[j]; j--)
		{
			m_map->freq[j + 1] = m_map->freq[j];
			m_map->map[j + 1] = m_map->map[j];
		}

		m_map->freq[j + 1] = tmp_freq;
		m_map->map[j + 1] = tmp_char;
	}
}

/**
 * Inverts the bytemap to get a read map from an unordered source to
 * a frequency-ordered codex.  This should be used after #bytemap_sort_symbols
 * @param m_map Pointer to the map to invert
 */
void bytemap_invert(bytemap_t *m_dest, bytemap_t *m_src)
{
	bytemap_t temp_map;
	bytemap_t *out_map;

	if (m_dest == m_src) out_map = &temp_map;
	else out_map = m_dest;

	for (uint_fast16_t i = 0; i < 256; i++)
	{
		out_map->map[m_src->map[i]] = i;
		out_map->freq[m_src->map[i]] = m_src->freq[i];
	}
	if (m_dest == m_src)
		memcpy(m_dest, &temp_map, sizeof(bytemap_t));
}
