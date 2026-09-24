/**
 * @file main.c
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

/**
 * Globals for tunable parameters
 */
uint32_t option_chunk_size = 50 *1024 * 1024; /// 50MB chunk by default

int option_verbose = false;
int option_force = false;
int action_compress = true;
int option_stdout = false;
int option_stdin = false;
int option_case = false;
int option_newline = false;
int option_continue = false;

uint16_t option_seq_order = 8;
uint16_t option_qual_order = 3;

uint8_t fqpack_magic_num[4] = { 'F', 'Q', 'P', 'K' };

static char infilename[FILENAME_MAX];
static char outfilename[FILENAME_MAX];
static char *progname;
static char compress_suffix[16] = {'f','q','p','\0'};
static char decompress_suffix[16] = {'f','a','s','t','q','\0'};

static double start_time;
static char progress_msg[80];
static double progress_end;
static double progress_current;

static void print_nice_filesize( off_t m_size);

static double wall_time()
{
	struct timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec * 0.000001;
}

static inline void fqpack_start_timer()
{
	start_time = wall_time();
}

static inline double fqpack_stop_timer()
{
	return wall_time() - start_time;
}

void fqpack_init_progress(const char *m_msg, double m_complete)
{
	if (m_msg)
	{
		strcpy(progress_msg, m_msg);
	}
	progress_end = m_complete;
	progress_current = 0.0;
}

void fqpack_update_progress (double m_delta)
{
	progress_current += m_delta;
	fprintf(stdout, "\r%s . . . %02d%%", progress_msg, (int) (100.0 * progress_current/progress_end));
	fflush(stdout);
}


int compress_data(void)
{
	fqpack_stream_t *data_stream;

	FILE *input_file;
	FILE *output_file;
	off_t input_file_size = 0;

	ssize_t compressed_size = 0;
	size_t names_size = 0;
	size_t seq_size = 0;
	size_t qual_size = 0;

	size_t bytes_read = 0;
	size_t bytes_written = 0;
	size_t total_bytes_out = 0;

	int retval = fqpack_no_error;

	if (option_stdin)
	{
		input_file = stdin;
	}
	else
	{
		input_file = fopen(infilename, "rb");
	}
	if (input_file == NULL)
	{
		fprintf(stderr, "Can't open input file: %s!", infilename);
		perror(NULL);
		exit(1);
	}

	if (option_stdout)
	{
		output_file = stdout;
	}
	else
	{
		output_file = fopen(outfilename, "wb");
	}
	if (output_file == NULL)
	{
		fprintf(stderr, "Can't open output file: %s!\n", outfilename);
		perror(NULL);
		exit(1);
	}


	if (!option_stdin)
	{
		if (fseeko(input_file, 0, SEEK_END)
				|| (input_file_size = ftello(input_file)) == (off_t) -1)
		{
			fprintf(stderr, "Error getting file size for %s!\n", infilename);
			perror(NULL);
			exit(1);
		}
		if (fseeko(input_file, 0, SEEK_SET))
		{
			fprintf(stderr, "Error seeking to file %s beginning!\n", infilename);
			perror(NULL);
			exit(1);
		}
		if (input_file_size == 0)
		{
			fprintf(stderr, "File %s has no content\n", infilename);
			exit(1);
		}

		if (option_chunk_size > input_file_size)
		{
			option_chunk_size = (input_file_size + 4095) & ~((size_t)4095);
		}

	}

	data_stream = fqpack_compress_init(option_chunk_size);
	memcpy(data_stream->header->magic, fqpack_magic_num, 4);
	data_stream->header->sequence.case_sensitive = option_case;
	data_stream->header->version = FQPACK_VERSION;

	if (option_verbose)
	{
		if (!option_stdin)
			fqpack_init_progress("Compressing file", input_file_size);
		fqpack_start_timer();
	}

	while ((bytes_read = fastq_read_record(data_stream, input_file)) > 0)
	{

		if (option_verbose && !option_stdin)
			fqpack_update_progress(bytes_read);

		compressed_size = fqpack_compress(data_stream, bytes_read);

		data_stream->header_crc =
				fqpack_crc32((uint8_t*)data_stream->header->name.column,
						sizeof(fqpack_col_t) * data_stream->header->name.num_col,
				fqpack_crc32((uint8_t*)data_stream->header,
						sizeof(fqpack_chunk_header_t), FQPACK_CRC32_SEED) );
		data_stream->data_crc = fqpack_crc32(data_stream->buffer, compressed_size, FQPACK_CRC32_SEED);
		bytes_written = fwrite(data_stream->header, 1, sizeof(fqpack_chunk_header_t) + data_stream->header->name.num_col * sizeof(fqpack_col_t),
				output_file);
		bytes_written += fwrite(&data_stream->header_crc, 1, sizeof(data_stream->header_crc), output_file);
		bytes_written += fwrite(&data_stream->data_crc, 1, sizeof(data_stream->data_crc), output_file);
		if (bytes_written != sizeof(fqpack_chunk_header_t) +
				data_stream->header->name.num_col * sizeof(fqpack_col_t) +
				sizeof(data_stream->header_crc) + sizeof(data_stream->data_crc))
		{
			perror("\nError writing header to file");
			retval = fqpack_io_error;
			break;
		}

		total_bytes_out += bytes_written;
		names_size += data_stream->header->name.compressed_len;
		seq_size += data_stream->header->sequence.compressed_len;
		qual_size += data_stream->header->quality.compressed_len;

		bytes_written = fwrite(data_stream->buffer, 1, compressed_size, output_file);
		if ( bytes_written != compressed_size)
		{
			perror("\nError writing data to file");
			retval = fqpack_io_error;
			break;
		}

		total_bytes_out += bytes_written;

        if (option_stdout) fflush(output_file);
        data_stream->header->continuing = option_continue;
	}

	fqpack_compress_deinit(data_stream);

	if(option_verbose)
	{
		fprintf(stderr, "\r%.40s compressed %.40s from ", progname, infilename);
		print_nice_filesize(input_file_size);
		fprintf(stderr, " to ");
		print_nice_filesize(ftello(output_file));
		fprintf(stderr, " in %.2f seconds.\n", fqpack_stop_timer());
		fprintf(stderr, "Names Size: %zu\nSequence Size: %zu\nQuality Size: %zu\n",
				names_size, seq_size, qual_size);
	}

	fclose(input_file);
	fclose(output_file);

	return retval;
}

static uint32_t fqpack_get_next_chunk(FILE *m_input, fqpack_stream_t **m_stream)
{
	uint8_t					*raw_chunk;
	fqpack_chunk_header_t 	chunk_header;
	fqpack_col_t 			columns[255];
	uint32_t				header_crc;

	uint32_t				total_bytes_read = 0;

	raw_chunk = (uint8_t*)&chunk_header;

	while(1)
	{
		if (!fread(chunk_header.magic, sizeof(fqpack_magic_num), 1, m_input))
		{
			return 0;
		}
		while (memcmp(chunk_header.magic, fqpack_magic_num, sizeof(fqpack_magic_num)))
		{
			chunk_header.magic[0] = chunk_header.magic[1];
			chunk_header.magic[1] = chunk_header.magic[2];
			chunk_header.magic[2] = chunk_header.magic[3];
			if(fread(&chunk_header.magic[3], 1, 1, m_input) != 1)
			{
				return 0;
			}
		}

		if ((fread(raw_chunk + sizeof(fqpack_magic_num), sizeof(fqpack_chunk_header_t) - sizeof(fqpack_magic_num), 1, m_input) != 1)
				|| (fread(columns, sizeof(fqpack_col_t) * chunk_header.name.num_col, 1, m_input) != 1)
				|| (fread(&header_crc, sizeof(header_crc), 1, m_input) != 1))
		{
			perror("\nCould not read header");
			exit(2);
		}
		total_bytes_read += sizeof(fqpack_chunk_header_t) + sizeof(fqpack_col_t) * chunk_header.name.num_col + sizeof(header_crc);

		if (header_crc != fqpack_crc32((uint8_t*)columns, sizeof(fqpack_col_t) * chunk_header.name.num_col,
				fqpack_crc32(raw_chunk, sizeof(fqpack_chunk_header_t), FQPACK_CRC32_SEED) ))
		{
			fprintf(stderr, "\nCRC mismatch on chunk!  Skipping\n");
			continue;
		}

		if (chunk_header.version > FQPACK_VERSION)
		{
			fprintf(stderr, "\nThis file has been compressed by fqpack version %ud.  "
					"I can only handle version %d\n", chunk_header.version, FQPACK_VERSION);
			exit(2);
		}

		if (*m_stream && (*m_stream)->header->uncompressed_size < chunk_header.uncompressed_size)
		{
			fqpack_compress_deinit(*m_stream);
			*m_stream = NULL;
		}

		if (!(*m_stream) && !(*m_stream = fqpack_compress_init(chunk_header.uncompressed_size))) return 0;

		memcpy((*m_stream)->header, &chunk_header, sizeof(chunk_header));
		memcpy((*m_stream)->header->name.column, columns, sizeof(fqpack_col_t) * chunk_header.name.num_col);
		(*m_stream)->header_crc = header_crc;

		if ((fread(&(*m_stream)->data_crc, sizeof(uint32_t), 1, m_input) != 1))
		{
			fprintf(stderr, "\nError reading Data\n");
			return 0;
		}
		total_bytes_read += sizeof(uint32_t);

		if (fread((*m_stream)->buffer, chunk_header.compressed_size, 1, m_input) != 1)
		{
			fprintf(stderr, "\nCould not read data from file!\n");
			return 0;
		}
		total_bytes_read += chunk_header.compressed_size;
		break;
	}

	return total_bytes_read;
}

void expand_data()
{

	fqpack_stream_t 	*stream = NULL;
	FILE 				*input_file = stdin;
	FILE 				*output_file = stdout;
	off_t 				input_file_size = 0;
	off_t 				total_bytes_read = 0;
	off_t 				total_bytes_out = 0;

	uint32_t			delta_size;
	ssize_t				result;

	if (!option_stdin)
	{
		input_file = fopen(infilename, "rb");
	}
	if (input_file == NULL)
	{
		fprintf(stderr, "Can't open input file: %s!\n", infilename);
		exit(1);
	}

	if (!option_stdout)
	{
		output_file = fopen(outfilename, "wb");
	}

	if (output_file == NULL)
	{
		fprintf(stderr, "Can't create output file: %s!\n", outfilename);
		exit(1);
	}

	if (!fseeko(input_file, 0, SEEK_END))
	{
		input_file_size = ftello(input_file);
		if (input_file_size < 0)
		{
			fprintf(stderr, "Error getting size for file %s!\n", infilename);
			exit(1);
		}

		if (fseeko(input_file, 0, SEEK_SET))
		{
			fprintf(stderr, "Error setting offset position to 0 in file %s!\n", infilename);
			exit(1);
		}
	}

	if (!(total_bytes_read = fqpack_get_next_chunk(input_file, &stream))) exit(2);

	if (option_verbose)
	{
		if (!option_stdout)
			fqpack_init_progress("Expanding file", input_file_size);
		fqpack_start_timer();
	}

	while(1)
	{

		result = fqpack_decompress(stream);

		if (result < fqpack_no_error)
		{
			switch (result)
			{
				case fqpack_crc_error:
					fprintf(stderr, "\nFile %s failed checksum verification!\n", infilename);
					break;
				case fqpack_mem_error:
					fprintf(stderr, "\nOut of memory\n");
					break;
				default:
					fprintf(stderr, "\nInternal error on decompression: %ld\n", (long int)result);
					perror(NULL);
					break;
			}
		}

		if (fwrite(stream->buffer, 1, result, output_file) != result)
		{
			fprintf(stderr, "\nError writing to file %s!\n", outfilename);
			exit(1);
		}
		total_bytes_out += result;

		if (option_stdout) fflush(output_file);
		if (!(delta_size = fqpack_get_next_chunk(input_file, &stream))) break;
		total_bytes_read += delta_size;

	}


	if (option_verbose)
	{
		fprintf(stderr, "\r%.40s expanded %.40s from ", progname, infilename);
		print_nice_filesize(total_bytes_read);
		fprintf(stderr, " to ");
		print_nice_filesize(total_bytes_out);
		fprintf(stderr, " in %.2f seconds.\n", fqpack_stop_timer());
	}

	fclose(input_file);
	fclose(output_file);
}

static void print_nice_filesize( off_t m_size)
{
	static const char suffix[5][4] = { "B", "KiB", "MiB", "GiB", "TiB" };
	int unit = 0;
	double val = (double)m_size;
	while (val > 9999.9)
	{
		val = val / 1024.;
		unit++;
	}
	fprintf(stderr, "%.1f %s", val, suffix[unit]);

}
void show_usage(void)
{
	printf("Usage: %s [OPTION]... [FILE [OUTFILE]]\n", progname);
	printf("Compress or uncompress FILE (by default, compress FILES to a new file with extension '.fqp').\n\n");
	printf("Mandatory arguments to long options are mandatory for short options too.\n\n");

	printf(
		"\t-d, --decompress       decompress\n"
		"\t-c, --stdout           write on standard output, keep original files unchanged\n"
		"\t-f, --force            force fqpack to output to the terminal screen\n"
		"\t-s, --size             set the input chunk size (compression only)\n"
		"\t-h, --help             give this help\n"
		"\t-o, --output           use suffix SUF on compressed files (.fqp by default)\n"
		"\t-v, --verbose          verbose mode\n"
		"\t    --order            set the context order of the sequence modeling (1-12, default=8)\n"
		"\t    --case             maintain input case in sequence lines\n"
		"\t    --solid_context    merge compatible contexts between chunks\n"
		"\t    --newline          maintain newline characters in sequence and quality reads\n"
		"\t-V  --version          display version number\n\n"
		"With no FILE, or when FILE is -, read standard input.\n"
		"(Implies -c if OUTFILE is not specified.)\n\n");

	exit(0);
}

void parse_cmdline(int argc, char *argv[])
{
	int c;
	char *file_extension = NULL;
	uintmax_t temp_size;
	char *suffix = NULL;
	char *size_suffix = NULL;
	int  size_char;

	while (1)
	{
		static struct option long_options[] = {
			{ "verbose", no_argument, 0, 'v' },
			{ "help", no_argument, 0, 'h' },
			{ "stdout", no_argument, 0, 'c' },
			{ "decompress", no_argument, 0, 'd' },
			{ "force", no_argument, 0, 'f'},
			{ "size", required_argument, 0, 's' },
			{ "output", required_argument, 0, 'o' },
			{ "case", no_argument, 0, 10 },
			{ "newline", no_argument, 0, 11 },
			{ "order", required_argument, 0, 12 },
			{ "solid_context", no_argument, 0, 13 },
			{ "version", no_argument, 0, 'V' },
			{ 0, 0, 0, 0 } };

		int option_index = 0;

		c = getopt_long(argc, argv, "vhcdfs:o:V", long_options, &option_index);

		/* Detect the end of the options. */
		if (c == -1) break;

		switch (c)
		{
			case 'c':
				option_stdout = true;
				break;
			case 'd':
				action_compress = false;
				break;
			case 'f':
				option_force = true;
				break;
			case 's':
				temp_size = strtoumax(optarg, &size_suffix, 10);

				size_char = toupper(*size_suffix);

				switch(size_char)
				{
					case 'K':
						temp_size *= UINTMAX_C(1024);
						break;
					case 'M':
						temp_size *= UINTMAX_C(1024 * 1024);
						break;
					case 'G':
						temp_size *= UINTMAX_C(1024 * 1024 * 1024);
						break;
					default:
						break;
				}

				if (temp_size > UINT32_MAX)
				{
					fprintf(stderr, "Limiting chunk size to 32 bit maximum value of %lu\n", (unsigned long)UINT32_MAX);
					option_chunk_size = UINT32_MAX;
				}
				else
					option_chunk_size = (uint32_t)temp_size;
				break;

			case 'o':
				suffix = optarg;
				break;

			case 'v' :
				option_verbose = true;
				break;

			case 10 :
				option_case = true;
				break;

			case 11 :
				option_newline = true;
				break;

			case 12 :
				temp_size = strtoumax(optarg, NULL,10);

				if (temp_size > 12)
				{
					fprintf(stderr, "Limiting sequence context order to maximum of 12");
					option_seq_order = 12;
				}
				else if (temp_size < 1)
				{
					fprintf(stderr, "Limiting sequence context order to minimum of 1");
					option_seq_order = 1;
				}
				else
					option_seq_order = (uint16_t)temp_size;

				break;

			case 13 :
				option_continue = true;
				break;

			case 'h':
			default:
				show_usage();
				break;
		}
	}

	if (optind >= argc || argv[optind][0] == '-')
	{
		option_stdin = true;
		option_stdout = true;
	}
	else
	{
		snprintf(infilename, FILENAME_MAX, "%s", argv[optind]);
	}

	if (option_stdout)
	{
		if (!option_force && isatty(fileno(stdout)))
		{
			fprintf(stderr, "Not printing data to console terminal by default.  Use -f to override\n");
			show_usage();
		}
	}
	else
	{
		if (++optind < argc)
		{
			snprintf(outfilename, FILENAME_MAX, "%s", argv[optind]);
			return;
		}

		strcpy(outfilename, infilename);
		file_extension = strrchr(outfilename, '.');
		if (!file_extension) file_extension = outfilename + strlen(outfilename);
		*file_extension++ = '.';

		if (!suffix)
		{
			if (!action_compress)
			{
				suffix = decompress_suffix;
			}
			else
			{
				suffix = compress_suffix;
			}
		}
		strncpy(file_extension, suffix, 16);
	}
}

int main(int argc, char *argv[])
{
	int retval = fqpack_no_error;
	char *slash = strrchr(argv[0], '/');
	char *bslash = strrchr(argv[0], '\\');

	progname = argv[0];
	if (slash) progname = slash + 1;
	if (bslash && bslash > slash) progname = bslash + 1;
	parse_cmdline(argc, argv);

	if (!option_force && !option_stdout && !access(outfilename, F_OK))
	{
		fprintf(stderr, "File %s exists!  Specify alternate name or '-f' to overwrite\n", outfilename);
		return -1;
	}
	if (action_compress)
		retval = compress_data();
	else
		expand_data();

	if (retval != fqpack_no_error) return -retval;
	return 0;
}

