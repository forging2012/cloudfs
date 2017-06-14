/*
 * =============================================================================
 *
 *       Filename:  oss_abort_multipart_upload_request.c
 *
 *    Description:  abort multipart upload request implementation.
 *
 *        Created:  09/04/2012 08:59:20 PM
 *
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */

#define _OSS_ABORT_MULTIPART_UPLOAD_REQUEST_H
#include <oss_abort_multipart_upload_request.h>
#undef _OSS_ABORT_MULTIPART_UPLOAD_REQUEST_H

/**
 * ����Bucket����
 */
static inline const char *
_abort_multipart_upload_request_get_bucket_name(
		oss_abort_multipart_upload_request_t *request)
{
	return request->bucket_name;
}

static inline void
__abort_multipart_upload_request_set_bucket_name(
		oss_abort_multipart_upload_request_t *request,
		const char *bucket_name, unsigned int bucket_name_len)
{
	assert(bucket_name != NULL);
	assert(bucket_name_len > 0);

	if (request->bucket_name != NULL) {
		free(request->bucket_name);
		request->bucket_name = NULL;
	}

	request->bucket_name = (char *)malloc(sizeof(char) * bucket_name_len + 1);
	strncpy(request->bucket_name, bucket_name, bucket_name_len);
	(request->bucket_name)[bucket_name_len] = '\0';
}

/* *
 * ����Bucket����
 * */
static inline void
_abort_multipart_upload_request_set_bucket_name(
		oss_abort_multipart_upload_request_t *request,
		const char *bucket_name)
{
	unsigned int bucket_name_len = strlen(bucket_name);

	__abort_multipart_upload_request_set_bucket_name(request,
			bucket_name, bucket_name_len);
}

/**
 * ����OSSObject key
 */
static inline const char *
_abort_multipart_upload_request_get_key(
		oss_abort_multipart_upload_request_t *request)
{
	return request->key;
}

static inline void
__abort_multipart_upload_request_set_key(
		oss_abort_multipart_upload_request_t *request,
		const char *key, unsigned int key_len)
{
	assert(key != NULL);
	assert(key_len > 0);

	if (request->key != NULL) {
		free(request->key);
		request->key = NULL;
	}

	request->key= (char *)malloc(sizeof(char) * key_len + 1);
	strncpy(request->key, key, key_len);
	(request->key)[key_len] = '\0';
}

/**
 * ����OSSObject key
 */
static inline void
_abort_multipart_upload_request_set_key(
		oss_abort_multipart_upload_request_t *request,
		const char *key)
{
	unsigned int key_len = strlen(key);

	__abort_multipart_upload_request_set_key(request,
			key, key_len);
}

/**
 * ���ر�ʶMultipart�ϴ��¼���Upload ID
 */
static inline const char *
_abort_multipart_upload_request_get_upload_id(
		oss_abort_multipart_upload_request_t *request)
{
	return request->upload_id;
}

static inline void
__abort_multipart_upload_request_set_upload_id(
		oss_abort_multipart_upload_request_t *request,
		const char *upload_id, unsigned int upload_id_len)
{
	assert(upload_id != NULL);
	assert(upload_id_len > 0);

	if (request->upload_id!= NULL) {
		free(request->upload_id);
		request->upload_id = NULL;
	}

	request->upload_id= (char *)malloc(sizeof(char) * upload_id_len+ 1);
	strncpy(request->upload_id, upload_id, upload_id_len);
	(request->upload_id)[upload_id_len] = '\0';
}

/**
 * ���ñ�ʶMultipart�ϴ��¼���Upload ID
 */
static inline void
_abort_multipart_upload_request_set_upload_id(
		oss_abort_multipart_upload_request_t *request,
		const char *upload_id)
{
	unsigned int upload_id_len = strlen(upload_id);

	__abort_multipart_upload_request_set_upload_id(request,
			upload_id, upload_id_len);
}

/**
 * ��ʼ��
 */
static inline oss_abort_multipart_upload_request_t *
_abort_multipart_upload_request_initialize(
		const char *bucket_name, unsigned int bucket_name_len,
		const char *key, unsigned int key_len,
		const char *upload_id, unsigned int upload_id_len)
{
	unsigned int byte_of_char = sizeof(char);
	oss_abort_multipart_upload_request_t * request = NULL;

	request = (oss_abort_multipart_upload_request_t *)malloc(
			sizeof(oss_abort_multipart_upload_request_t));

	request->bucket_name = (char *)malloc(byte_of_char * bucket_name_len + 1);
	request->key = (char *)malloc(byte_of_char * key_len + 1);
	request->upload_id = (char *)malloc(byte_of_char * upload_id_len + 1);

	strncpy(request->bucket_name, bucket_name, bucket_name_len);
	strncpy(request->key, key, key_len);
	strncpy(request->upload_id, upload_id, upload_id_len);

	(request->bucket_name)[bucket_name_len] = '\0';
	(request->key)[key_len] = '\0';
	(request->upload_id)[upload_id_len] = '\0';

	request->get_bucket_name = _abort_multipart_upload_request_get_bucket_name;
	request->get_key = _abort_multipart_upload_request_get_key;
	request->get_upload_id = _abort_multipart_upload_request_get_upload_id;
	request->set_bucket_name = _abort_multipart_upload_request_set_bucket_name;
	request->set_key = _abort_multipart_upload_request_set_key;
	request->set_upload_id = _abort_multipart_upload_request_set_upload_id;

	return request;
}

/**
 * ��ʼ��
 */
oss_abort_multipart_upload_request_t *
abort_multipart_upload_request_initialize(const char *bucket_name,
		const char *key,
		const char *upload_id)
{
	assert(bucket_name != NULL);
	assert(key != NULL);
	assert(upload_id != NULL);

	unsigned int bucket_name_len = strlen(bucket_name);
	unsigned int key_len = strlen(key);
	unsigned int upload_id_len = strlen(upload_id);

	return _abort_multipart_upload_request_initialize(
			bucket_name, bucket_name_len,
			key, key_len,
			upload_id, upload_id_len);

}

/**
 * ��������
 */
void
abort_multipart_upload_request_finalize(
		oss_abort_multipart_upload_request_t *request)
{
	assert(request != NULL);

	if (request->bucket_name != NULL) {
		free(request->bucket_name);
		request->bucket_name = NULL;
	}

	if (request->key!= NULL) {
		free(request->key);
		request->key = NULL;
	}

	if (request->upload_id != NULL) {
		free(request->upload_id);
		request->upload_id = NULL;
	}

	free(request);
}
