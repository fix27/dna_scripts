/**
 * @file fastqpack.h
 *
 * @date Dec 16, 2011
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

#ifndef FASTQPACK_H_
#define FASTQPACK_H_


#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#include <model.h>
#include <util.h>
#include <bytemap.h>
#include <bytebuffer.h>

#define FQPACK_VERSION				1
#define FQPACK_BUFFER_OVERHEAD		4096
#define FQPACK_MAX_NAME_COL			UINT16_MAX

typedef enum
{
	fqpack_no_error 		=  0,
	fqpack_io_error 		= -1,
	fqpack_mem_error 		= -2,
	fqpack_crc_error 		= -3,
	fqpack_internal_error 	= -4,
	fqpack_not_compressible	= -5
} e_fqpack_error;

typedef enum
{
	fqpack_chunk_type_data	= 0,
	fqpack_chunk_type_index	= 1
} e_fqpack_chunk_type;
typedef enum
{
	fqpack_col_alphanum 	= 0,
	fqpack_col_8bit 		= 1,
	fqpack_col_16bit 		= 2,
	fqpack_col_32bit 		= 3,
	fqpack_col_64bit 		= 4
} e_fqpack_col_type;

typedef struct
{
	uint32_t			compressed_len;
	uint32_t			uncompressed_len;
	uint32_t			context_order:4;
	bool				large_codex:1;
	bool				case_sensitive:1;
	uint32_t			__reserved:2;
}__attribute__((packed)) fqpack_seq_qual_options_t;

typedef struct
{
	uint8_t				tailing_char;
	e_fqpack_col_type	type:8;
}__attribute__((packed)) fqpack_col_t;
typedef struct
{
	uint32_t		compressed_len;
	uint32_t		unprocessed_len;
	uint32_t		uncompressed_len;
	uint16_t		num_col;
	fqpack_col_t	column[0];
}__attribute__((packed)) fqpack_name_t;

typedef struct
{
	uint8_t						magic[4];
	uint8_t						version;
	e_fqpack_chunk_type			chunk_type:4;
	bool						variable_length:1;
	bool						continuing:1;
	uint32_t					__reserved:2;
	uint32_t					chunk_record_count;
	uint32_t					uncompressed_size;
	uint32_t					compressed_size;
	fqpack_seq_qual_options_t	sequence;
	fqpack_seq_qual_options_t	quality;
	fqpack_name_t				name;
}__attribute__((packed)) fqpack_chunk_header_t;

struct fqpack_stream
{
	fqpack_chunk_header_t	*header;
	uint32_t				header_crc;
	uint32_t				data_crc;

	uint8_t					*seq_segment;
	uint8_t					*qual_segment;

	byte_buffer_t			*length_vector;
	byte_buffer_t			*invalid_read;
	byte_buffer_t			*large_codex;

	uint8_t 				*buffer;
	void 					*raw_buffer;
	size_t					buffer_len;

	byte_buffer_t			*bb_names;
	byte_buffer_t			*bb_seq;
	byte_buffer_t			*bb_qual;

	bytemap_t 				seq_map;
	bytemap_t 				qual_map;

	rle_model_t				*general_model;
	rle_model_t 			*names_model;
	sequence_model_t 		*seq_model;
	quality_model_t 		*qual_model;

};

fqpack_stream_t *fqpack_compress_init(uint32_t m_chunk_size);
void fqpack_compress_deinit(fqpack_stream_t *m_stream);
ssize_t fqpack_decompress(fqpack_stream_t *m_stream);
ssize_t fqpack_compress(fqpack_stream_t *m_stream, size_t m_num);

#endif /* FASTQPACK_H_ */
