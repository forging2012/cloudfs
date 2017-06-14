/*
 * =============================================================================
 *
 *       Filename:  oss_extra.h
 *
 *    Description:  multithreaded upload and download operation.
 *
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef OSS_EXTRA_H
#define OSS_EXTRA_H
#include <assert.h>
#include <limits.h>		/* for CHAR_BIT */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <client.h>
#include <oss_helper.h>
#include <oss_curl_callback.h>
#include <oss_auth.h>
#include <oss_common.h>
#include <oss_ttxml.h>
#include <oss_time.h>

#define _OSS_CLIENT_H
#include <oss_client.h>
#undef _OSS_CLIENT_H

#define BITMASK(b) (1 << ((b) % CHAR_BIT))
#define BITSLOT(b) ((b) / CHAR_BIT)
#define BITSET(a, b) ((a)[BITSLOT(b)] |= BITMASK(b))
#define BITCLEAR(a, b) ((a)[BITSLOT(b)] &= ~BITMASK(b))
#define BITTEST(a, b) ((a)[BITSLOT(b)] & BITMASK(b))
#define BITNSLOTS(nb) ((nb + CHAR_BIT - 1) / CHAR_BIT)
#define NUM_THREADS 4
#define PART_SIZE (8 * 1024 * 1024)

typedef struct extra_buffer_s extra_buffer_t;
typedef struct extra_request_param_s extra_request_param_t;
typedef struct extra_worker_monitor_param_s extra_worker_monitor_param_t;
typedef struct extra_curl_request_param_s extra_curl_request_param_t;

struct extra_buffer_s {
	char *unmovable_buffer_ptr; /**<记录主线程中分配的缓冲区首地址，用于在子线程中释放该空间 */
	char *ptr; /**< 缓冲区首指针，发送数据时该指针需要向后移动 */
	unsigned int left; /** 缓冲区剩余大小 */
	unsigned int allocated; /** 缓冲区总大小 */
	unsigned short code; /**返回码 */
};

struct extra_request_param_s {
	oss_client_t *client;
	char *upload_id;
	char *bucket_name;
	char *key;
	unsigned int part_number;
	char *orig_md5;
	char *echo_md5;
	char *metadir;
	extra_buffer_t *send_buffer;
	extra_buffer_t *header_buffer;
};

struct extra_curl_request_param_s {
	extra_buffer_t *send_buffer;
	extra_buffer_t *recv_buffer;
	extra_buffer_t *header_buffer;
};

struct extra_worker_monitor_param_s {
	char *metadir; /* 上传信息存放目录 */
	int parts; /* 上传文件块数 */
};

extern void
client_extra_put_object(oss_client_t *client,
		const char *bucket_name,
		const char *key,
		const char *local_file,
		unsigned short *retcode);

extern int
oss_sync_upload(oss_client_t * client,
		const char * dir, 
		const char *bucket_name);

extern int 
oss_sync_download(oss_client_t * client,
		const char *dir, 
		const char *bucket_name);

#endif // OSS_EXTRA_H
