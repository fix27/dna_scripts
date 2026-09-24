/**
 * @file fastq.c
 *
 * @date Jan 9, 2012
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
#include <ctype.h>

#include <bytemap.h>
#include <bytebuffer.h>
#include <fastqpack.h>

#include <fastq.h>

#ifdef _WIN32
static ssize_t getline(char **lineptr, size_t *n, FILE *stream)
{
	char *buf = *lineptr;
	size_t len = 0;
	size_t cap = *n;
	int c;

	if (!lineptr || !n || !stream) return -1;
	if (!buf)
	{
		cap = 128;
		buf = malloc(cap);
		if (!buf) return -1;
		*lineptr = buf;
		*n = cap;
	}
	while ((c = getc(stream)) != EOF)
	{
		if (len + 1 >= cap)
		{
			char *nb = realloc(buf, cap * 2);
			if (!nb) return -1;
			buf = nb;
			cap *= 2;
			*lineptr = buf;
			*n = cap;
		}
		buf[len++] = (char)c;
		if (c == '\n') break;
	}
	if (len == 0 && c == EOF) return -1;
	buf[len] = '\0';
	return (ssize_t)len;
}

static ssize_t getdelim(char **lineptr, size_t *n, int delim, FILE *stream)
{
	char *buf = *lineptr;
	size_t len = 0;
	size_t cap = *n;
	int c;

	if (!lineptr || !n || !stream) return -1;
	if (!buf)
	{
		cap = 128;
		buf = malloc(cap);
		if (!buf) return -1;
		*lineptr = buf;
		*n = cap;
	}
	while ((c = getc(stream)) != EOF)
	{
		if (len + 1 >= cap)
		{
			char *nb = realloc(buf, cap * 2);
			if (!nb) return -1;
			buf = nb;
			cap *= 2;
			*lineptr = buf;
			*n = cap;
		}
		buf[len++] = (char)c;
		if (c == delim) break;
	}
	if (len == 0 && c == EOF) return -1;
	buf[len] = '\0';
	return (ssize_t)len;
}
#endif

ssize_t fastq_read_sequence(char *m_line, size_t m_len, fqpack_stream_t *m_stream)
{
	size_t output = 0;
	size_t input = 0;
	bool ignore_case = !m_stream->header->sequence.case_sensitive;
	bool large_codex = false;
	int test_char;


	for (input = 0; input < m_len; input++)
	{
		if (ignore_case)
			test_char = toupper(m_line[input]);
		else
			test_char = m_line[input];

		if (test_char == '\r' || test_char =='\n') continue;
		if (test_char == '+') break;
		if (test_char == '.' || test_char == '~' ||
				(test_char >= 'A' && test_char <= 'Z') ||
				(test_char >= 'a' && test_char <= 'z'))
		{
			m_line[output++] = test_char;
			m_stream->seq_map.freq[test_char]++;

			/**
			 * The most common case is a sequence line with only upper-case ATGC.  In this case,
			 * we encode each with 2 bits but to allow for the possibility of alternate characters,
			 * we also mark lines that have larger codices, in which case ATCG are marked with
			 * 2-3 bits each (unary coding)
			 */
			if (!(test_char == 'A' || test_char == 'T' || test_char == 'G' || test_char == 'C'))
				large_codex = true;
		}
		else
		{
			/// Invalid character encountered in the sequence
			return -1;
		}
	}
	if (large_codex) m_stream->header->sequence.large_codex = true;
	bb_append(m_stream->large_codex, &large_codex, sizeof(bool));

	m_line[output] = '\0';
	return output;
}

static ssize_t fastq_read_quality(char *m_line, size_t m_len, bytemap_t *m_map, FILE *m_file)
{
	size_t output = 0;
	register int test_char = 0;
	register int prev_char;
	size_t run_count = 1;

	while (output < m_len)
	{

		if ((test_char = getc(m_file)) < 0) return -1;

		if (test_char == '\r' || test_char =='\n') continue;

		if (test_char >= '!' && test_char <= '~')
		{
			if (output && prev_char == test_char) run_count++;
			else { run_count = 1; prev_char = test_char;}

			m_line[output++] = test_char;
			m_map->freq[test_char] += run_count;
		}
		else
		{
			/// Invalid character encountered in the quality
			return -1;
		}
	}
	while (test_char != '\n' && !feof(m_file)) test_char = getc(m_file);

	/// NULL-terminate the string
	m_line[output] = '\0';
	return output;
}

/**
 * Reads data from a file and standardizes the FASTQ reads into a context-free grammar for processing.
 * The output will be 3-lines per record (NAME, SEQ, QUAL).  We also populate the stream data structure
 * with information about the number of records and total uncompressed bytes read into the chunk
 * @param m_stream
 * @param m_file
 * @return
 */
size_t fastq_read_record(fqpack_stream_t *m_stream, FILE *m_file)
{
	char 		*line[4] = {NULL};
	size_t 		prev_linelen = 0;
	size_t 		linelen[4] = {0};
	ssize_t 	retval[4];
	off_t		starting_pos = ftello(m_file);

	byte_buffer_t *length_vec = m_stream->length_vector;

	m_stream->header->chunk_record_count = 0;

	while (ftello(m_file) - starting_pos < m_stream->header->uncompressed_size)
	{
		/// The first line _must_ begin with \@ to be a valid record start
		if ((retval[0] = getline(&line[0], &linelen[0], m_file)) < 0)
			break;
		if (line[0][0] != '@')
			continue;
		/// Trim the CR and LF characters from the end of the string (we standardize in compression)
		while(line[0][retval[0] - 1] == '\n' || line[0][retval[0] - 1] == '\r')
		{
			line[0][--retval[0]] = '\0';
		}
		line[0][retval[0]++] = '\0';

		/// Next, we expect the sequence; possibly spread over newlines
		if ((retval[1] = getdelim(&line[1], &linelen[1], '+', m_file)) < 0)
			break;

		if ((retval[1] = fastq_read_sequence(line[1], retval[1], m_stream)) < 0)
		{
			/// Negative returns here indicate an invalid sequence character, so we skip this read
			fprintf(stderr, "Invalid character encountered in sequence for read ID %s\n", line[0]);
			continue;
		}

		/// Calculate the line length deltas for each sequence line (assume that quality lines are consistent)
		if (!prev_linelen)
		{
			prev_linelen = retval[1];
			bb_append_word(length_vec, retval[1]);
		}
		else
		{
			bb_append_word(length_vec, retval[1] - prev_linelen);
		}


		/// This reads the repeated name line that precedes the quality segment
		if ((retval[2] = getline(&line[2], &linelen[2], m_file)) < 0)
			break;

		/**
		 * In the quality section, we must be careful as the FASTQ grammar is context-sensitive.  That is
		 * to say, the quality can include both '@' and newline characters that we would otherwise use to
		 * determine the end of our quality section read.  Thus our only cue is when we have read the same
		 * number of valid characters as existed in the sequence section.
		 *
		 * The upshot is that we cannot perform a character-delimited read on our file, nor can we read a
		 * specific number of bytes as we don't know, a priori, how many line superfluous characters might
		 * exist in the quality sequence string (CR, CRLF, etc)
		 *
		 */
		if (linelen[3] < linelen[1])
		{
			if (linelen[3])
			{
				line[3] = realloc(line[3], linelen[1]);
			}
			else line[3] = malloc(linelen[1]);
			linelen[3] = linelen[1];
		}
		if ((retval[3] = fastq_read_quality(line[3], retval[1], &m_stream->qual_map, m_file)) < 0)
		{
			fprintf(stderr, "Invalid character encountered in quality for read ID %s\n", line[0]);
			continue;
		}

		/**
		 * Here we skip the duplicate name line and the starting '@' character for our first name line.
		 * This creates independent buffers that can be used for parallel compression
		 */


		bb_append(m_stream->bb_names, &line[0][1], retval[0] - 1);
		bb_append(m_stream->bb_seq, line[1], retval[1]);
		bb_append(m_stream->bb_qual, line[3], retval[3]);

		/// Update the variable length flag if we have a changing sequence length
		if (prev_linelen != retval[1])
		{
			m_stream->header->variable_length = true;
			prev_linelen = retval[1];
		}

		m_stream->header->chunk_record_count++;
	}

	FQPACK_SAFE_FREE(line[0]);
	FQPACK_SAFE_FREE(line[1]);
	FQPACK_SAFE_FREE(line[2]);
	FQPACK_SAFE_FREE(line[3]);

	return ftello(m_file) - starting_pos;
}
