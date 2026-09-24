/**
 * @file filters.c
 *
 * @date Dec 19, 2011
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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>

#include <bytemap.h>
#include <util.h>

#include <fastqpack.h>


/**
 * Handles numeric conversion and delta-coding of number columns in the FASTQ name column.  It performs
 * the conversion in place. In case of an overflow during conversion, the stream's data will remain untouched
 * and the function will return fqpack_col_alphanum to indicate that the column remains represented by the
 * alphanumeric values
 * @param m_stream Pointer to the allocated and initialized fqpack stream
 * @param m_start Pointer to a data buffer containing the NULL-terminated input strings.  Will be overwritten.
 * @return e_fqpack_col_type value indicating smallest possible representation of the data
 */
static int fqpack_pack_numeric_stream(fqpack_stream_t *m_stream, uint8_t *m_start)
{
	intmax_t *numeric_stream;
	intmax_t max_val = 0;
	intmax_t min_val = 0;
	intmax_t last_val = 0;
	intmax_t temp_val = 0;

	char *nextval = (char*) m_start;
	int8_t *output8bit = (int8_t*) m_start;
	int16_t *output16bit = (int16_t*) m_start;
	int32_t *output32bit = (int32_t*) m_start;
	int64_t *output64bit = (int64_t*) m_start;
	int retval;

	numeric_stream = malloc(sizeof(intmax_t) * m_stream->header->chunk_record_count);

	numeric_stream[0] = strtoimax(nextval, &nextval, 10);
	if (*nextval && errno == ERANGE)
	{
		free(numeric_stream);
		return fqpack_col_alphanum;
	}
	last_val = numeric_stream[0];
	min_val = numeric_stream[0];
	max_val = numeric_stream[0];
	nextval++;

	for (size_t i = 1; i < m_stream->header->chunk_record_count; i++)
	{
		temp_val = strtoimax(nextval, &nextval, 10);
		/// If the number is too large to represent as imax, we leave the col as a string
		if (*nextval && errno == ERANGE)
		{
			free(numeric_stream);
			return fqpack_col_alphanum;
		}
		numeric_stream[i] = temp_val - last_val;
		last_val = temp_val;
		if (numeric_stream[i] > max_val) max_val = numeric_stream[i];
		if (numeric_stream[i] < min_val) min_val = numeric_stream[i];

		nextval++;
	}

	if (max_val <= INT8_MAX && min_val >= INT8_MIN)
	{
		retval = fqpack_col_8bit;
		for (size_t i = 0; i < m_stream->header->chunk_record_count; i++)
		{
			*output8bit++ = (int8_t)numeric_stream[i];
		}
	}
	else if (max_val <= INT16_MAX && min_val >= INT16_MIN)
	{
		retval = fqpack_col_16bit;
		for (size_t i = 0; i < m_stream->header->chunk_record_count; i++)
		{
			*output16bit++ = (int16_t)numeric_stream[i];
		}
	}
	else if (max_val <= INT32_MAX && min_val >= INT32_MIN)
	{
		retval = fqpack_col_32bit;
		for (size_t i = 0; i < m_stream->header->chunk_record_count; i++)
		{
			*output32bit++ = (int32_t)numeric_stream[i];
		}
	}
	else if (max_val <= INT64_MAX && min_val >= INT64_MIN)
	{
		retval = fqpack_col_64bit;
		for (size_t i = 0; i < m_stream->header->chunk_record_count; i++)
		{
			*output64bit++ = (int64_t)numeric_stream[i];
		}
	}
	else
	{
		retval = fqpack_col_alphanum;
	}

	free (numeric_stream);
	return retval;
}


static size_t fqpack_pack_text_stream(fqpack_stream_t *m_stream, uint8_t *m_start)
{
	size_t tmp_string_len = 0;
	size_t cur_string_len = 0;

	char *tmp_string;
	char *input_ptr;
	char *output_ptr;

	tmp_string_len = strlen((char*)m_start);
	tmp_string = strdup((char*)m_start);
	input_ptr = (char*)(m_start + tmp_string_len);
	output_ptr = (char*)input_ptr;
	*output_ptr++ = '.';

	for (size_t i = 1; i < m_stream->header->chunk_record_count; i++)
	{
		input_ptr++;
		cur_string_len = strlen(input_ptr);

		if (!strncmp(input_ptr, tmp_string, tmp_string_len))
		{
			*output_ptr++ = '\0';
		}
		else
		{
			if (cur_string_len > tmp_string_len)
			{
				tmp_string = realloc(tmp_string, cur_string_len + 1);
				tmp_string_len = cur_string_len;
			}
			memcpy(tmp_string, input_ptr, cur_string_len + 1);
			output_ptr += cur_string_len;
			*output_ptr++ = '.';
		}
		input_ptr += cur_string_len;
	}
	free(tmp_string);

	return ((uint8_t*)output_ptr - m_start);
}


/**
 * Process the NAME lines from a FASTQ file.  The NAME will be broken into alphanumeric characters, separated by
 * a non-alphanumeric character.  Each segment is then concatenated and computed for minimum delta values.
 * @param m_stream Pointer to an allocated and populated stream
 * @return Number of bytes in written into names buffer.  -1 on failure
 */
ssize_t fqpack_process_names(fqpack_stream_t *m_stream)
{
	uint8_t *input_str = m_stream->bb_names->byte_content;
	uint8_t *end_input = m_stream->bb_names->byte_content + m_stream->bb_names->length;
	uint8_t *output_str = m_stream->buffer;
	uint8_t **col_input_start = NULL;
	uint8_t *col_output_start = NULL;
	uint_fast16_t output_index = 0;

	uint32_t total_reads = m_stream->header->chunk_record_count;
	uint32_t current_read = 0;
	int column_type = -1;

	/**
	 * This creates a map of current pointers into the individual reads.  This will allow us to rapidly
	 * stream each name segment into an sorted output without repeating multiple searches
	 */
	col_input_start = fqpack_aligned_malloc(m_stream->header->chunk_record_count * sizeof(uint8_t*), false);
	for (current_read = 0; current_read < total_reads && input_str < end_input; current_read++)
	{
		col_input_start[current_read] = input_str;
		input_str += strlen((char*)input_str) + 1; /// End of the name read
	}
	input_str = m_stream->bb_names->byte_content;

	/**
	 * We create a map of the chunk's name segments based on the first entry.  While this needn't map
	 * to following entries, it generally does.  In the case where the entries are all different, we
	 * get a series of text columns to compress.  This is sub-optimal but still very close, given the
	 * low chance for shared tokens in disparate read names.
	 */
	while (input_str < end_input && *input_str++ && output_index < FQPACK_MAX_NAME_COL)
	{
		while(isalnum(*input_str)) input_str++;
		m_stream->header->name.column[output_index++].tailing_char = *input_str;
	}
	m_stream->header->name.num_col = output_index;

	/**
	 * We pass over the entire input buffer multiple times, sorting the name tokens into a sequential stream.
	 * For each token stream, we determine what the smallest possible representation of its content can be.  All token
	 * streams begin as alpha numeric.  Then, if we can represent it as an integer, we find the smallest possible integer
	 * representation in powers of 2 (8-bit, 16-bit, 32-bit, 64-bit).
	 *
	 * Alpha numeric streams have individual records separated by '.' with repeated records denoted by '\0' and no '.'
	 * between them. The run-length sequence is terminated by any character other than '\0' which then begins a new record.
	 *
	 * Numeric streams are packed by their data type with no separating terminators
	 */
	col_output_start = output_str;
	for (output_index = 0; output_index < m_stream->header->name.num_col; output_index++)
	{
		output_str = col_output_start;

		for (current_read = 0; current_read < total_reads; current_read++)
		{
			/// Skip to the current read's name input segment
			input_str = col_input_start[current_read];

			while (*input_str != m_stream->header->name.column[output_index].tailing_char)
			{
				if (column_type && !isdigit(*input_str))
					column_type = fqpack_col_alphanum;
				*output_str++ = *input_str++;
			}
			*output_str++ = '\0';

			/// Store the current read pointer to the next segment of the current name
			col_input_start[current_read] = ++input_str;

			if (m_stream->header->name.column[output_index].tailing_char != '\0')
				while (*input_str++ != '\0'); /// End of the name read
		}

		///Handle the column type.  Numeric columns are packed by their largest delta value.  Alphanumeric columns are encoded
		if (column_type != fqpack_col_alphanum)
		{
			m_stream->header->name.column[output_index].type = (e_fqpack_col_type) fqpack_pack_numeric_stream(m_stream, col_output_start);
			if (m_stream->header->name.column[output_index].type != fqpack_col_alphanum)
				col_output_start += ( (1 << (m_stream->header->name.column[output_index].type - 1)) * m_stream->header->chunk_record_count);
			else
				col_output_start += fqpack_pack_text_stream(m_stream, col_output_start);
		}
		else
		{
			m_stream->header->name.column[output_index].type = fqpack_col_alphanum;
			col_output_start += fqpack_pack_text_stream(m_stream, col_output_start);
		}
		column_type = -1;
	}

	free(col_input_start);

	bb_reset(m_stream->bb_names);
	if (!bb_append(m_stream->bb_names, m_stream->buffer, col_output_start - m_stream->buffer)) return -1;

	return m_stream->bb_names->length;
}

bool fqpack_unprocess_names(fqpack_stream_t *m_stream)
{
	uint8_t *input_str = m_stream->bb_names->byte_content;
	uint8_t *output_str = m_stream->buffer;
	uint_fast16_t column_index = 0;

	uint32_t total_reads = m_stream->header->chunk_record_count;
	uint32_t current_read = 0;

	uint8_t  *prev_input;
	uint8_t  **column_offset;
	intptr_t *last_val;

	column_offset = alloca(sizeof(uint8_t*) * m_stream->header->name.num_col);
	last_val = alloca(sizeof(intptr_t) * m_stream->header->name.num_col);

	column_offset[0] = m_stream->bb_names->byte_content;
	last_val[0] = (intptr_t)input_str;


	for (column_index = 1; column_index < m_stream->header->name.num_col; column_index++)
	{
		if (m_stream->header->name.column[column_index].type == fqpack_col_alphanum)
		{
			last_val[column_index] = (intptr_t)input_str;
		}
		else
		{
			last_val[column_index] = 0;
		}

		switch (m_stream->header->name.column[column_index - 1].type)
		{
			case fqpack_col_alphanum:
				current_read = 0;
				for (current_read = 0; current_read < total_reads; current_read++)
				{
					while (*input_str != '\0' && *input_str != '.') input_str++;
					input_str++;
				}
				column_offset[column_index] = input_str;
				break;
			case fqpack_col_8bit:
				column_offset[column_index] = column_offset[column_index - 1] + total_reads * sizeof(int8_t);
				input_str = column_offset[column_index];
				break;
			case fqpack_col_16bit:
				column_offset[column_index] = column_offset[column_index - 1] + total_reads * sizeof(int16_t);
				input_str = column_offset[column_index];
				break;
			case fqpack_col_32bit:
				column_offset[column_index] = column_offset[column_index - 1] + total_reads * sizeof(int32_t);
				input_str = column_offset[column_index];
				break;
			case fqpack_col_64bit:
				column_offset[column_index] = column_offset[column_index - 1] + total_reads * sizeof(int64_t);
				input_str = column_offset[column_index];
				break;
		}
	}

	for (current_read = 0; current_read < total_reads; current_read++)
	{
		*output_str++ = '@';

		for (column_index = 0; column_index < m_stream->header->name.num_col; column_index++)
		{
			input_str = column_offset[column_index];
			switch (m_stream->header->name.column[column_index].type)
			{
				case fqpack_col_alphanum:
					prev_input = (uint8_t*) last_val[column_index];
					if (*input_str == '\0')
					{
						while (*prev_input != '.')
							*output_str++ = *prev_input++;
					}
					else
					{
						last_val[column_index] = (intptr_t)input_str;
						while (*input_str != '.')
							*output_str++ = *input_str++;
					}

					input_str++;
					*output_str++ = m_stream->header->name.column[column_index].tailing_char;
					break;
				case fqpack_col_8bit:
					last_val[column_index] += *(int8_t*)input_str;
					output_str += sprintf((char*)output_str, "%" PRIu64 "%c", (uint64_t)(last_val[column_index]), m_stream->header->name.column[column_index].tailing_char);
					input_str += (sizeof(uint8_t));
					break;
				case fqpack_col_16bit:
					last_val[column_index] += *(int16_t*)input_str;
					output_str += sprintf((char*)output_str, "%" PRIu64 "%c", (uint64_t)(last_val[column_index]), m_stream->header->name.column[column_index].tailing_char);
					input_str += (sizeof(uint16_t));
					break;
				case fqpack_col_32bit:
					last_val[column_index] += *(int32_t*)input_str;
					output_str += sprintf((char*)output_str, "%" PRIu64 "%c", (uint64_t)(last_val[column_index]), m_stream->header->name.column[column_index].tailing_char);
					input_str += (sizeof(uint32_t));
					break;
				case fqpack_col_64bit:
					last_val[column_index] += *(int64_t*)input_str;
					output_str += sprintf((char*)output_str, "%" PRIu64 "%c", (uint64_t)(last_val[column_index]), m_stream->header->name.column[column_index].tailing_char);
					input_str += (sizeof(uint64_t));
					break;
			}
			column_offset[column_index] = input_str;
		}
	}

	bb_reset(m_stream->bb_names);
	bb_append(m_stream->bb_names, m_stream->buffer, output_str - m_stream->buffer);
	return true;

}
