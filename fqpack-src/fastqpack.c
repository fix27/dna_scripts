/**
 * @file fastqpack.c
 *
 * @date Dec 15, 2011
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
#include <string.h>
#include <math.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <sys/time.h>
#include <unistd.h>
#include <inttypes.h>

#include <crc.h>
#include <util.h>
#include <filters.h>
#include <model.h>
#include <fastq.h>

#include <bytebuffer.h>
#include <bytemap.h>

#include <fastqpack.h>

extern uint16_t option_qual_order;
extern uint16_t option_seq_order;

/**
 * Initializes a compression stream for use.
 * @param m_chunk_size
 * @return
 */
fqpack_stream_t *fqpack_compress_init(uint32_t m_chunk_size)
{
	fqpack_stream_t *temp_stream = NULL;
	uint32_t half_size = m_chunk_size >> 1;

	if (!(temp_stream = (fqpack_stream_t*)fqpack_aligned_malloc(sizeof(fqpack_stream_t), true)))
	{
		fprintf(stderr,"Insufficient memory for compression stream.  Tried allocating %lu\n", (unsigned long)sizeof(fqpack_stream_t));
		return NULL;
	}

	temp_stream->buffer_len = m_chunk_size + FQPACK_BUFFER_OVERHEAD;
	temp_stream->raw_buffer = fqpack_aligned_malloc(temp_stream->buffer_len, true);
	temp_stream->buffer = (uint8_t*)temp_stream->raw_buffer;

	temp_stream->bb_names = bb_new(half_size);
	temp_stream->bb_qual = bb_new(half_size);
	temp_stream->bb_seq = bb_new(half_size);

	temp_stream->length_vector = bb_new(1024 * sizeof(uint32_t));
	temp_stream->invalid_read = bb_new(1024 * sizeof(bool));
	temp_stream->large_codex = bb_new(1024 * sizeof(bool));

	temp_stream->header = fqpack_aligned_malloc(sizeof(fqpack_chunk_header_t) + FQPACK_MAX_NAME_COL * sizeof(fqpack_col_t), true);

	if (!temp_stream->raw_buffer
			|| !temp_stream->bb_names || !temp_stream->bb_qual || !temp_stream->bb_seq
			|| !temp_stream->length_vector || !temp_stream->large_codex || !temp_stream->invalid_read
			|| !temp_stream->header)
	{
		fprintf(stderr,"Insufficient memory chunk.  Aborting.\n");
		FQPACK_SAFE_FREE(temp_stream->header);
		bb_free(temp_stream->bb_seq);
		bb_free(temp_stream->bb_qual);
		bb_free(temp_stream->bb_names);

		bb_free(temp_stream->length_vector);
		bb_free(temp_stream->large_codex);
		bb_free(temp_stream->invalid_read);

		FQPACK_SAFE_FREE(temp_stream->raw_buffer);
		FQPACK_SAFE_FREE(temp_stream);

		return NULL;
	}

	bytemap_init(&temp_stream->seq_map);
	bytemap_init(&temp_stream->qual_map);

	temp_stream->header->uncompressed_size = m_chunk_size;

	return temp_stream;
}

void fqpack_compress_deinit(fqpack_stream_t *m_stream)
{
	if (m_stream)
	{
		FQPACK_SAFE_FREE(m_stream->header);
		FQPACK_SAFE_FREE(m_stream->raw_buffer);

		bb_free(m_stream->bb_seq);
		bb_free(m_stream->bb_qual);
		bb_free(m_stream->bb_names);
		bb_free(m_stream->length_vector);
		bb_free(m_stream->large_codex);
		bb_free(m_stream->invalid_read);

		if (m_stream->names_model) model_free_rle(m_stream->names_model);
		if (m_stream->general_model) model_free_rle(m_stream->general_model);
		if (m_stream->seq_model) model_free_sequence(m_stream->seq_model);
		if (m_stream->qual_model) model_free_quality(m_stream->qual_model);

		free(m_stream);
	}
}

static ssize_t fqpack_gather_reads(fqpack_stream_t *m_stream)
{
	uint8_t *name = m_stream->bb_names->byte_content;
	uint8_t *seq = m_stream->bb_seq->byte_content;
	uint8_t *qual = m_stream->bb_qual->byte_content;

	uint32_t *len_vec = m_stream->length_vector->word_content;
	uint32_t length = 0;

	uint8_t *output_str = m_stream->buffer;
	uint32_t total_reads = m_stream->header->chunk_record_count;
	uint32_t current_read = 0;


	for (current_read = 0; current_read < total_reads; current_read++)
	{
		length += *len_vec++;

		while (*name)
		{
			*output_str++ = *name++;
		}
		*output_str++ = '\n';
		name++;

		memcpy(output_str, seq, length);
		output_str += length;
		seq += length;

		*output_str++ = '\n';
		*output_str++ = '+';
		*output_str++ = '\n';

		memcpy(output_str, qual, length);
		output_str += length;
		qual += length;
		*output_str++ = '\n';
	}

	return output_str - m_stream->buffer;
}


ssize_t fqpack_decompress(fqpack_stream_t *m_stream)
{
	ssize_t retval;

	m_stream->buffer = (uint8_t*)m_stream->raw_buffer;

	m_stream->general_model = model_initialize_rle(m_stream->general_model, 1, m_stream->header->continuing);
	m_stream->names_model = model_initialize_rle(m_stream->names_model, 1, m_stream->header->continuing);
	if (!m_stream->names_model) return fqpack_internal_error;

	/**
	 * We always ensure these vectors are long enough and cleared.  For the large codex, this
	 * sets all reads to 'false' or GCTA codex.  For the variable length, this ensures that
	 * the delta coding will maintain the initial value if the reads are not variable length
	 */
	bb_ensure_len(m_stream->length_vector, m_stream->header->chunk_record_count * sizeof(uint32_t));
	bb_ensure_len(m_stream->large_codex, m_stream->header->chunk_record_count * sizeof(bool));
	bb_clear(m_stream->length_vector);
	bb_clear(m_stream->large_codex);

	if (m_stream->header->variable_length)
	{
		retval = model_uncompress_rle(m_stream->buffer, m_stream->length_vector->byte_content,
				m_stream->general_model, m_stream->header->chunk_record_count * sizeof(uint32_t));
		m_stream->buffer += retval;
	}
	else
	{
		*m_stream->length_vector->word_content = *(uint32_t*)m_stream->buffer;
		m_stream->buffer += sizeof(uint32_t);
	}
	if (m_stream->header->sequence.large_codex)
	{
		retval = model_uncompress_rle(m_stream->buffer, m_stream->large_codex->byte_content,
				m_stream->general_model, m_stream->header->chunk_record_count * sizeof(bool));
		m_stream->buffer += retval;
	}

    bb_reset(m_stream->bb_names);
    bb_ensure_len(m_stream->bb_names, m_stream->header->name.unprocessed_len);
	model_uncompress_rle(m_stream->buffer, m_stream->bb_names->byte_content,
			m_stream->names_model, m_stream->header->name.uncompressed_len);
	m_stream->buffer += m_stream->header->name.compressed_len;

	model_uncompress_quality(m_stream);
	m_stream->buffer += m_stream->header->quality.compressed_len;
	model_uncompress_sequence(m_stream);
	m_stream->buffer += m_stream->header->sequence.compressed_len;

	for (size_t i = 0; i < m_stream->header->quality.uncompressed_len; i++)
		m_stream->bb_qual->byte_content[i] = m_stream->qual_map.map[m_stream->bb_qual->byte_content[i]];

	fqpack_unprocess_names(m_stream);
	m_stream->buffer = (uint8_t*)m_stream->raw_buffer;
	retval = fqpack_gather_reads(m_stream);

	return retval;
}

ssize_t fqpack_compress(fqpack_stream_t *m_stream, size_t m_num)
{
	bytemap_t 				map;
	ssize_t 				retval;
	size_t					seq_codex_size;
	size_t					qual_codex_size;
	fqpack_chunk_header_t	*header = m_stream->header;

	m_stream->buffer = (uint8_t*)m_stream->raw_buffer;

	header->sequence.context_order = option_seq_order;
	header->quality.context_order = option_qual_order;

	bytemap_sort_symbols(&m_stream->seq_map);
	for (seq_codex_size = 0; seq_codex_size < 256 && m_stream->seq_map.freq[seq_codex_size]; seq_codex_size++);
	bytemap_sort_symbols(&m_stream->qual_map);
	for (qual_codex_size = 0; qual_codex_size < 256 && m_stream->qual_map.freq[qual_codex_size]; qual_codex_size++);

	if (header->continuing &&
			(seq_codex_size > m_stream->seq_model->codex_size || qual_codex_size > m_stream->qual_model->codex_size))
		header->continuing = false;


	m_stream->general_model = model_initialize_rle(m_stream->general_model, 1, header->continuing);
	m_stream->names_model = model_initialize_rle(m_stream->names_model, 1, header->continuing);
	m_stream->seq_model = model_initialize_sequence(m_stream->seq_model, seq_codex_size, header->sequence.context_order, header->continuing);
	m_stream->qual_model = model_initialize_quality(m_stream->qual_model, qual_codex_size, header->quality.context_order, header->continuing);

	if (!m_stream->names_model || !m_stream->seq_model || !m_stream->qual_model) return fqpack_internal_error;

	header->name.unprocessed_len = m_stream->bb_names->length;
	header->name.uncompressed_len = fqpack_process_names(m_stream);

	header->sequence.uncompressed_len = m_stream->bb_seq->length;
	header->quality.uncompressed_len = m_stream->bb_qual->length;

	if (header->variable_length)
	{
		retval = model_compress_rle(m_stream->length_vector->byte_content, m_stream->buffer,
				m_stream->general_model, m_stream->length_vector->length);
		m_stream->buffer += retval;
	}
	else
	{
		*(uint32_t*)m_stream->buffer = *m_stream->length_vector->word_content;
		m_stream->buffer += sizeof(uint32_t);
	}
	if (header->sequence.large_codex)
	{
		retval = model_compress_rle(m_stream->large_codex->byte_content, m_stream->buffer,
				m_stream->general_model, m_stream->large_codex->length);
		m_stream->buffer += retval;
	}

	header->name.compressed_len = model_compress_rle(m_stream->bb_names->byte_content,
			m_stream->buffer, m_stream->names_model, header->name.uncompressed_len);
	m_stream->buffer += header->name.compressed_len;

	bytemap_invert(&map, &m_stream->qual_map);
	for (size_t i = 0; i < header->quality.uncompressed_len; i++)
		m_stream->bb_qual->byte_content[i] = map.map[m_stream->bb_qual->byte_content[i]];
	header->quality.compressed_len = model_compress_quality(m_stream, header->quality.uncompressed_len);
	m_stream->buffer += header->quality.compressed_len;

	header->sequence.compressed_len = model_compress_sequence(m_stream, header->sequence.uncompressed_len);
	m_stream->buffer += header->sequence.compressed_len;

	bb_reset(m_stream->bb_names);
	bb_reset(m_stream->bb_qual);
	bb_reset(m_stream->bb_seq);
	bb_reset(m_stream->length_vector);
	bb_reset(m_stream->large_codex);

	bytemap_init(&m_stream->qual_map);
	bytemap_init(&m_stream->seq_map);

	header->compressed_size = m_stream->buffer - (uint8_t*)m_stream->raw_buffer;

	m_stream->buffer = (uint8_t *)m_stream->raw_buffer;
	return header->compressed_size;
}

