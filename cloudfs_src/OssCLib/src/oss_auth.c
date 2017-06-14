/*
 * =============================================================================
 *
 *       Filename:  generate_authentication.c
 *
 *    Description:  generate authentication
 *
 *        Created:  09/06/2012 11:24:12 AM
 *
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _OSS_CONSTANTS_H
#include <oss_constants.h>
#undef _OSS_CONSTANTS_H

#include <oss_map.h>
#include <uthash.h>

#include <base64.h>
#include <hmac.h>
#include <memxor.h>
#include <sha1.h>
#include <pthread.h>
#define CANONICALIZED_HEADERS_BUFFER_SIZE 4096
#define HMAC_SHA1_OUT_LEN 21
#define SIGNED_VALUE_LEN 65


/* *
 * �û��Զ���ͷ����ͷ�����ְ��ֵ�������
 * */
typedef struct user_headers_s user_headers_t;

struct user_headers_s {
	char *key;
	char *value;
	UT_hash_handle hh;
};

/* *
 * С�ģ���Ĵ��벻Ҫ����ʹ�þ�̬����
 * */
static char canonicalized_headers[CANONICALIZED_HEADERS_BUFFER_SIZE];
static user_headers_t *oss_user_headers = NULL;

/* *
 * key_iter ����ָ��������ڱ��� user_headers ʱ��key_iter ָ��
 * canonicalized_headers �Ѵ����λ�ã��´�canonicalized_headers�ַ���
 * ����ɸ�λ����
 * */
static char *key_iter = NULL;

static pthread_mutex_t gen_auth_mutex;




static int oss_sort_headers_by_key(user_headers_t *a, user_headers_t *b)
{
	return strcmp(a->key, b->key);
}

/* *
 * ͷ������
 * */
static void oss_sort_headers()
{
	HASH_SORT(oss_user_headers, oss_sort_headers_by_key);
}

static void oss_add_headers(const char *key, const char *value)
{
	assert(key != NULL);
	assert(value != NULL);

	user_headers_t *s = (user_headers_t *)malloc(sizeof(user_headers_t));
	unsigned int key_len = strlen(key);
	unsigned int value_len = strlen(value);

	s->key = (char *)malloc(sizeof(char) * key_len + 1);
	s->value = (char *)malloc(sizeof(char) * value_len + 1);

	memset(s->key, '\0', key_len + 1);
	memset(s->value, '\0', value_len + 1);

	strncpy(s->key, key, key_len);
	strncpy(s->value, value, value_len);

	HASH_ADD_INT(oss_user_headers, key, s);
}
static void oss_delete_all_headers()
{
	user_headers_t *current_user_header, *tmp;
	HASH_ITER(hh, oss_user_headers, current_user_header, tmp) {
		free(current_user_header->key);
		free(current_user_header->value);
		HASH_DEL(oss_user_headers,current_user_header);  /* delete; users advances to next */
		free(current_user_header);            /* optional- if you want to free  */
	}
}
/* *
 * �������û��Զ���ͷ��������һ��
 * */
static void fill_canonicalized_headers()
{
	user_headers_t *s;
	for (s = oss_user_headers; s != NULL; s = s->hh.next) {
		unsigned int offset = 0;
		offset = sprintf(key_iter, "%s:%s\n", s->key, s->value);
		key_iter += offset;
	}
}

#if 0
static void iter_user_headers(const char *key, const char *value, const void *obj)
{
	unsigned int offset = 0;
	offset = sprintf(key_iter, "%s:%s\n", key, value);
	key_iter += offset;
}
#endif

/* *
 * �ص����������� user_headers ��ÿһ��ͷ��
 * */
static void iter_user_headers(const char *key, const char *value, const void *obj)
{
	oss_add_headers(key, value);
}

void oss_auth_init()
{
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&gen_auth_mutex,&mutexattr);

	pthread_mutexattr_destroy(&mutexattr);
}
void oss_auth_destroy()
{
	pthread_mutex_destroy(&gen_auth_mutex);
}


char *
generate_authentication(const char *access_key, const char *method,
		oss_map_t *default_headers, oss_map_t *user_headers,
		const char *resource, unsigned int *sign_len)
{
	assert(access_key != NULL);
	assert(method != NULL);
	assert(default_headers != NULL);
	assert(resource != NULL);

	/* Ϊgenerate_authentication����ȫ�������� */
    pthread_mutex_lock(&gen_auth_mutex);


	char *content_md5 = NULL;
	char *content_type = NULL;
	char *date = NULL;

	/* *
	 * ��Ҫǩ�����ַ��������� canonicalized_headers �� 4 ��
	 * */
	char *string_to_sign = (char *)malloc(sizeof(char) * CANONICALIZED_HEADERS_BUFFER_SIZE *4);
	unsigned int access_key_len = strlen(access_key);

	/* *
	 * hmac-sha1 ������
	 * */
	char *hmac_sha1_out = (char *)malloc(sizeof(char) * HMAC_SHA1_OUT_LEN);

	/* *
	 * base64 ������ǩ������ֵ
	 * */
	char *signed_value = (char *)malloc(sizeof(char) * SIGNED_VALUE_LEN);

	memset(string_to_sign, '\0', CANONICALIZED_HEADERS_BUFFER_SIZE * 4);
	memset(canonicalized_headers, '\0', CANONICALIZED_HEADERS_BUFFER_SIZE);
	memset(hmac_sha1_out, '\0', HMAC_SHA1_OUT_LEN);
	memset(signed_value, '\0', SIGNED_VALUE_LEN);
	/* *
	 * �����ٴθ�ֵΪ NULL
	 * */
	key_iter = NULL;

	unsigned int result_len = oss_map_get(default_headers, OSS_CONTENT_MD5, NULL, 0);
	if (result_len != 0) {
		content_md5 = (char *)malloc(sizeof(char) * result_len);
		memset(content_md5, '\0', result_len);
		oss_map_get(default_headers, OSS_CONTENT_MD5, content_md5, result_len);
	}

	result_len = oss_map_get(default_headers, OSS_CONTENT_TYPE, NULL, 0);
	if (result_len != 0) {
		content_type = (char *)malloc(sizeof(char) * result_len);
		memset(content_type, '\0', result_len);
		oss_map_get(default_headers, OSS_CONTENT_TYPE, content_type, result_len);
	}

	result_len = oss_map_get(default_headers, OSS_DATE, NULL, 0);
	if (result_len != 0) {
		date = (char *)malloc(sizeof(char) * result_len);
		memset(date, '\0', result_len);
		oss_map_get(default_headers, OSS_DATE, date, result_len);
	}

	/* *
	 *  key_iter ָ��canonicalized_headers��ʼλ��
	 * */
	key_iter = canonicalized_headers;

	/* *
	 * ע��ص�����
	 * */
	if (user_headers != NULL)
		oss_map_enum(user_headers, iter_user_headers, NULL);

	/* *
	 * �û��Զ���ͷ������
	 * */
	oss_sort_headers();

	/* *
	 * �������� user_headers������� canonicalized_headers
	 * */
	fill_canonicalized_headers();
	key_iter = NULL;

	if (content_md5 != NULL && content_type != NULL) 
		sprintf(string_to_sign, "%s\n%s\n%s\n%s\n%s%s", method,
				content_md5, content_type, date, canonicalized_headers, resource);
	else if (content_md5 == NULL && content_type != NULL)
		sprintf(string_to_sign, "%s\n\n%s\n%s\n%s%s", method, content_type,
				date, canonicalized_headers, resource);
	else if (content_md5 == NULL && content_type == NULL)
		sprintf(string_to_sign, "%s\n\n\n%s\n%s%s", method,
				date, canonicalized_headers, resource);

	unsigned int string_to_sign_len = strlen(string_to_sign);

	hmac_sha1(access_key, access_key_len, string_to_sign, string_to_sign_len, hmac_sha1_out);

	base64_encode(hmac_sha1_out, HMAC_SHA1_OUT_LEN - 1, signed_value, SIGNED_VALUE_LEN);
	*sign_len = strlen(signed_value);

	/* *
	 * clean up.
	 * */
	free(string_to_sign);
	free(hmac_sha1_out);
	oss_delete_all_headers();
	if (content_md5 != NULL) free(content_md5);
	if (content_type != NULL) free(content_type);
	if (date != NULL) free(date);

	/* �����˳�, ���� */
    pthread_mutex_unlock(&gen_auth_mutex);

	return signed_value;
}



