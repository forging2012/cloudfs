/*
 * =============================================================================
 *
 *       Filename:  oss_compression.c
 *
 *    Description:  oss compression utility.
 *
 *        Created:  09/21/2012 04:51:53 PM
 *
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#include <lz4.h>
#include <lz4hc.h>
#include <minilzo.h>
#include <oss_common.h>
#include <oss_compression.h>

static const int one = 1;

#define CPU_LITTLE_ENDIAN (*(char*)(&one))
#define CPU_BIG_ENDIAN (!CPU_LITTLE_ENDIAN)
#define LITTLE_ENDIAN32(i)   if (CPU_BIG_ENDIAN) { i = swap32(i); }

#ifndef LZO_compressBound
#define LZO_compressBound(len) ((len) + (len) / 16 + 64 + 3)
#endif

#define HEAP_ALLOC(var,size) \
    lzo_align_t __LZO_MMODEL var [ ((size) + (sizeof(lzo_align_t) - 1)) / sizeof(lzo_align_t) ]

static HEAP_ALLOC(wrkmem, LZO1X_1_MEM_COMPRESS);

static inline unsigned int swap32(unsigned int x)
{
	return ((x << 24) & 0xff000000 ) |
		((x <<  8) & 0x00ff0000 ) |
		((x >>  8) & 0x0000ff00 ) |
		((x >> 24) & 0x000000ff );
}

void
oss_write_compression_header_in_memory(char *buffer,
		char algorithm,
		char flag,
		char md5[])
{
	assert(buffer != 0);

	oss_compression_header_t *header =
		(oss_compression_header_t *)malloc(sizeof(oss_compression_header_t));
	if (header == NULL) {
		fprintf(stderr, "malloc failed.\n");
		return;
	}
	memset(header, 0, sizeof(oss_compression_header_t));

	strncpy(header->magic, OSS_COMPRESSION_MAGIC, OSS_COMPRESSION_MAGIC_LEN);
	header->version = (char)OSS_COMPRESSION_VERSION; 
	header->algorithm = algorithm;
	header->flag = flag;
	header->length = sizeof(oss_compression_header_t);

	if (md5 != NULL)
		memcpy(header->md5, md5, 16);
	else memset(header->md5, 0, 16);

	header->optional = NULL; /**< Ŀǰ�ײ�����չ������ֱ�ӽ�optional��ΪNULL */
	memcpy(buffer, header, sizeof(oss_compression_header_t));

	free(header);
	return;
}

void
oss_write_compression_header(FILE *fp,
		char algorithm,
		char flag,
		char md5[])
{
	assert(fp != NULL);
	unsigned int ret = 0;
	
	oss_compression_header_t *header =
		(oss_compression_header_t *)malloc(sizeof(oss_compression_header_t));
	if (header == NULL) {
		fprintf(stderr, "malloc failed.\n");
		return;
	}
	memset(header, 0, sizeof(oss_compression_header_t));

	strncpy(header->magic, OSS_COMPRESSION_MAGIC, OSS_COMPRESSION_MAGIC_LEN);
	header->version = (char)OSS_COMPRESSION_VERSION; 
	header->algorithm = algorithm;
	header->flag = flag;
	header->length = sizeof(oss_compression_header_t);

	if (md5 != NULL)
		memcpy(header->md5, md5, 16);
	else memset(header->md5, 0, 16);

	header->optional = NULL; /**< Ŀǰ�ײ�����չ������ֱ�ӽ�optional��ΪNULL */

	ret = fwrite(header, 1, header->length, fp);
	assert(ret == header->length);
	free(header);
}

/**
 * ѹ�������ڴ棬������ѹ���ļ���ͷ������
 * */
static int
_compress_block_with_lz4(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		int level /**< ѹ���ȼ�*/)
{

	int (*compression)(const char*, char*, int);

	switch (level) {
		case 0 : compression = LZ4_compress; break;
		case 1 : compression = LZ4_compressHC; break;
		default : compression = LZ4_compress;
	}


	/**< compression output size. */
	int cout_size; 

	cout_size = compression(inbuf, outbuf + 4, inbuf_len);

	LITTLE_ENDIAN32(cout_size);
	* (unsigned int*) outbuf = cout_size;
	LITTLE_ENDIAN32(cout_size);

	return cout_size;
}

/**
 * ѹ�������ڴ棬������ѹ���ļ���ͷ������
 * */
static int
_compress_block_with_lzo(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		int level /**< ѹ���ȼ�*/)
{
	int r = -1;
	int (*compression)(const lzo_bytep, lzo_uint, lzo_bytep, lzo_uintp, lzo_voidp);

	switch (level) {
		case 0 : compression = lzo1x_1_compress; break;
		case 1 : compression = lzo1x_1_compress; break;
		default : compression = lzo1x_1_compress;
	}


	/**< compression output size. */
	int cout_size; 

	r = compression((const lzo_bytep)inbuf, (lzo_uint)inbuf_len, 
			(lzo_bytep)(outbuf + 4), (lzo_uintp)&cout_size, (lzo_voidp)wrkmem);
    if (r != LZO_E_OK) {
        /* this should NEVER happen */
        fprintf(stderr, "internal error - compression failed: %d\n", r);
        return -1;
    }

	LITTLE_ENDIAN32(cout_size);
	* (unsigned int*) outbuf = cout_size;
	LITTLE_ENDIAN32(cout_size);

	return cout_size;
}

/**
 * ѹ�������ڴ棬ѹ��������ѹ���ļ���ͷ������
 * */
static int
_compress_block_with_lz4_2nd(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		char flag, /**< ��־λ */
		int level /**< ѹ���ȼ�*/)
{
	char *md5 = NULL;
	int (*compression)(const char*, char*, int);

	switch (level) {
		case 0 : compression = LZ4_compress; break;
		case 1 : compression = LZ4_compressHC; break;
		default : compression = LZ4_compress;
	}

	if (flag == 0)
		oss_write_compression_header_in_memory(outbuf, OSS_LZ4, flag, NULL);
	if (flag == 1) {
		md5 = oss_get_buffer_md5_digest(inbuf, inbuf_len);
		oss_write_compression_header_in_memory(outbuf, OSS_LZ4, flag, md5);
		free(md5);
	}

	/**< compression output size. */
	int cout_size; 

	cout_size = compression(inbuf, outbuf + sizeof(oss_compression_header_t) + 4, inbuf_len);

	LITTLE_ENDIAN32(cout_size);
	* (unsigned int*) (outbuf + sizeof(oss_compression_header_t)) = cout_size;
	LITTLE_ENDIAN32(cout_size);

	return cout_size + sizeof(oss_compression_header_t) + 4;
}

/**
 * ѹ�������ڴ棬ѹ��������ѹ���ļ���ͷ������
 * */
static int
_compress_block_with_lzo_2nd(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		char flag, /**< ��־λ */
		int level /**< ѹ���ȼ�*/)
{
	int r = -1;
	char *md5 = NULL;
	int (*compression)(const lzo_bytep, lzo_uint, lzo_bytep, lzo_uintp, lzo_voidp);

	switch (level) {
		case 0 : compression = lzo1x_1_compress; break;
		case 1 : compression = lzo1x_1_compress; break;
		default : compression = lzo1x_1_compress;
	}

	if (flag == 0)
		oss_write_compression_header_in_memory(outbuf, OSS_LZO, flag, NULL);
	if (flag == 1) {
		md5 = oss_get_buffer_md5_digest(inbuf, inbuf_len);
		oss_write_compression_header_in_memory(outbuf, OSS_LZO, flag, md5);
		free(md5);
	}

	/**< compression output size. */
	int cout_size; 

	r = compression((const lzo_bytep)inbuf, (lzo_uint)inbuf_len, 
			(lzo_bytep)(outbuf + sizeof(oss_compression_header_t) + 4),
			(lzo_uintp)&cout_size, (lzo_voidp)wrkmem);
    if (r != LZO_E_OK) {
        /* this should NEVER happen */
        fprintf(stderr, "internal error - compression failed: %d\n", r);
        return -1;
    }

	LITTLE_ENDIAN32(cout_size);
	* (unsigned int*) (outbuf + sizeof(oss_compression_header_t)) = cout_size;
	LITTLE_ENDIAN32(cout_size);

	return cout_size + sizeof(oss_compression_header_t) + 4;
}

static void _compress_file_with_lz4(
		const char *infile,
		const char *outfile,
		char flag,      /** 0: ��д��Դ�ļ���У��ֵ��1:д��Դ�ļ���У��ֵ */
		int level)
{

	int (*compression)(const char*, char*, int);
	char *inbuf = NULL;
	char *outbuf = NULL;
	char *md5 = NULL;
	FILE *fin = NULL;
	FILE *fout = NULL;

	switch (level) {
		case 0 : compression = LZ4_compress; break;
		case 1 : compression = LZ4_compressHC; break;
		default : compression = LZ4_compress;
	}

	fin = fopen(infile, "rb");
	if (fin == NULL) {
		fprintf(stderr, "error occured when opening file %s\n", infile);
		return;
	}
	
	fout = fopen(outfile, "wb");
	if (fout == NULL) {
		fprintf(stderr, "error occured when opening file %s\n", outfile);
		return;
	}

	if (flag == 0)
		oss_write_compression_header(fout, OSS_LZ4, flag, NULL);
	if (flag == 1) {
		md5 = oss_get_file_md5_digest(infile);
		oss_write_compression_header(fout, OSS_LZ4, flag, md5);
		free(md5);
	}

	inbuf = (char *)malloc(OSS_CHUNK_SIZE);
	outbuf = (char *)malloc(LZ4_compressBound(OSS_CHUNK_SIZE));
	if (!inbuf || !outbuf) {
		fprintf(stderr, "Allocation error : not enough memory\n");
		return;
	}

	while (1)
	{
		/**< compression output size. */
		int cout_size; 
		/** compression input size */
	    int cin_size = (int)fread(inbuf, (unsigned int)1, (unsigned int)OSS_CHUNK_SIZE, fin);
		if( cin_size <= 0 ) break;

		cout_size = compression(inbuf, outbuf+4, cin_size);

		LITTLE_ENDIAN32(cout_size);
		* (unsigned int*) outbuf = cout_size;
		LITTLE_ENDIAN32(cout_size);
	
		fwrite(outbuf, 1, cout_size + 4, fout);
	}

	free(inbuf);
	free(outbuf);
	fclose(fin);
	fclose(fout);
	return;
}

static void _compress_file_with_lzo(
		const char *infile,
		const char *outfile,
		char flag,      /** 0: ��д��Դ�ļ���У��ֵ��1:д��Դ�ļ���У��ֵ */
		int level)
{
	int r = -1;
	int (*compression)(const lzo_bytep, lzo_uint, lzo_bytep, lzo_uintp, lzo_voidp);
	char *inbuf = NULL;
	char *outbuf = NULL;
	char *md5 = NULL;
	FILE *fin = NULL;
	FILE *fout = NULL;

	switch (level) {
		case 0 : compression = lzo1x_1_compress; break;
		case 1 : compression = lzo1x_1_compress; break;
		default : compression = lzo1x_1_compress;
	}

	fin = fopen(infile, "rb");
	if (fin == NULL) {
		fprintf(stderr, "error occured when opening file %s\n", infile);
		return;
	}
	
	fout = fopen(outfile, "wb");
	if (fout == NULL) {
		fprintf(stderr, "error occured when opening file %s\n", outfile);
		return;
	}

	if (flag == 0)
		oss_write_compression_header(fout, OSS_LZO, flag, NULL);
	if (flag == 1) {
		md5 = oss_get_file_md5_digest(infile);
		oss_write_compression_header(fout, OSS_LZO, flag, md5);
		free(md5);
	}

	inbuf = (char *)malloc(OSS_CHUNK_SIZE);
	outbuf = (char *)malloc(LZO_compressBound(OSS_CHUNK_SIZE));
	if (!inbuf || !outbuf) {
		fprintf(stderr, "Allocation error : not enough memory\n");
		return;
	}

	while (1)
	{
		/**< compression output size. */
		int cout_size; 
		/** compression input size */
	    int cin_size = (int)fread(inbuf, (unsigned int)1, (unsigned int)OSS_CHUNK_SIZE, fin);
		if( cin_size <= 0 ) break;

		r = compression((const lzo_bytep)inbuf, (lzo_uint)cin_size, (lzo_bytep)(outbuf+4),
				(lzo_uintp)&cout_size, (lzo_voidp)wrkmem);
		if (r != LZO_E_OK) {
			/* this should NEVER happen */
			fprintf(stderr, "internal error - compression failed: %d\n", r);
			return;
		}
		LITTLE_ENDIAN32(cout_size);
		* (unsigned int*) outbuf = cout_size;
		LITTLE_ENDIAN32(cout_size);
	
		fwrite(outbuf, 1, cout_size + 4, fout);
	}

	free(inbuf);
	free(outbuf);
	fclose(fin);
	fclose(fout);
	return;
}

/**
 * ѹ���ڴ��
 * */
int oss_compress_block(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		char algorithm, /**< ѹ���㷨  */
		int level /**< ��ѹ���㷨��ѹ���ȼ�*/)
{

	assert(inbuf != NULL);
	assert(outbuf != NULL);
	int retsize = 0;

	if (algorithm == OSS_LZ4) {
		retsize = _compress_block_with_lz4(inbuf, inbuf_len,
				outbuf, outbuf_len, level);
	} else if (algorithm == OSS_LZO) {
		retsize = _compress_block_with_lzo(inbuf, inbuf_len,
				outbuf, outbuf_len, level);
	} else {
		fprintf(stderr, "compression algorithm not supported right now.\n");
	}

	return retsize;
}

/**
 * ѹ���ڴ�飬ͬʱ����ѹ���ļ���ͷ������
 * */
int oss_compress_block_2nd(
		char *inbuf, unsigned int inbuf_len, /** �������������Ԥ�ȷ���ռ� */
		char *outbuf, unsigned int outbuf_len,/** �������������Ԥ�ȷ���ռ� */
		char algorithm, /**< ѹ���㷨  */
		char flag, /**< ��ʶλ��0 �����ԭ�ļ�MD5��1 ���ԭ�ļ�MD5ֵ */
		int level /**< ��ѹ���㷨��ѹ���ȼ�*/)
{

	assert(inbuf!= NULL);
	assert(outbuf != NULL);
	int retsize = 0;

	if (algorithm == OSS_LZ4) {
		retsize = _compress_block_with_lz4_2nd(inbuf, inbuf_len,
				outbuf, outbuf_len, flag, level);
	} else if (algorithm == OSS_LZO) {
		retsize = _compress_block_with_lzo_2nd(inbuf, inbuf_len,
				outbuf, outbuf_len, flag, level);
	} else {
		fprintf(stderr, "compression algorithm not supported right now.\n");
	}

	return retsize;
}

/**
 * ѹ���ļ�
 * */
void oss_compress_file(
		const char *infile,
		const char *outfile,
		char algorithm,
		char flag,      /** 0: ��д��Դ�ļ���У��ֵ��1:д��Դ�ļ���У��ֵ */
		int level)
{
	assert(infile != NULL);
	assert(outfile != NULL);

	if (algorithm == OSS_LZ4) {
		_compress_file_with_lz4(infile, outfile, flag, level);
	} else if (algorithm == OSS_LZO) {
		_compress_file_with_lzo(infile, outfile, flag, level);
	} else {
		fprintf(stderr, "compression algorithm not supported right now.\n");
	}
	return;
}

#undef LZO_compressBound
