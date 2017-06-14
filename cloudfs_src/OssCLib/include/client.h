/*
 * =============================================================================
 *
 *       Filename:  client.h
 *
 *    Description:  client interface, you should always include this file if 
 *                  you want to use oss client.
 *
 *        Created:  09/04/2012 03:22:51 PM
 *
 *        Company:  ICT ( Institute Of Computing Technology, CAS )
 *
 * =============================================================================
 */
#ifndef _CLIENT_H_
#define _CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#define _OSS_ABORT_MULTIPART_UPLOAD_REQUEST_H
#include "oss_abort_multipart_upload_request.h"
#undef _OSS_ABORT_MULTIPART_UPLOAD_REQUEST_H

#define _OSS_ACCESS_CONTROL_LIST_H
#include "oss_access_control_list.h"
#undef _OSS_ACCESS_CONTROL_LIST_H

#define _OSS_BUCKET_H
#include "oss_bucket.h"
#undef _OSS_BUCKET_H

#define _OSS_CLIENT_H
#include "oss_client.h"
#undef _OSS_CLIENT_H

#define _OSS_COMPLETE_MULTIPART_UPLOAD_REQUEST_H
#include "oss_complete_multipart_upload_request.h"
#undef _OSS_COMPLETE_MULTIPART_UPLOAD_REQUEST_H

#define _OSS_COMPLETE_MULTIPART_UPLOAD_RESULT_H
#include "oss_complete_multipart_upload_result.h"
#undef _OSS_COMPLETE_MULTIPART_UPLOAD_RESULT_H

#define _OSS_COPY_OBJECT_REQUEST_H
#include "oss_copy_object_request.h"
#undef _OSS_COPY_OBJECT_REQUEST_H

#define _OSS_COPY_OBJECT_RESULT_H
#include "oss_copy_object_result.h"
#undef _OSS_COPY_OBJECT_RESULT_H

#define _OSS_DELETE_MULTIPLE_OBJECT_REQUEST_H
#include "oss_delete_multiple_object_request.h"
#undef _OSS_DELETE_MULTIPLE_OBJECT_REQUEST_H

#define _OSS_GENERATE_PRESIGNED_URL_REQUEST_H
#include "oss_generate_presigned_url_request.h"
#undef _OSS_GENERATE_PRESIGNED_URL_REQUEST_H

#define _OSS_GET_OBJECT_GROUP_REQUEST_H
#include"oss_get_object_group_request.h"
#undef _OSS_GET_OBJECT_GROUP_REQUEST_H

#define _OSS_GET_OBJECT_REQUEST_H
#include "oss_get_object_request.h"
#undef _OSS_GET_OBJECT_REQUEST_H

#define _OSS_GRANT_H
#include "oss_grant.h"
#undef _OSS_GRANT_H

#define _OSS_INITIATE_MULTIPART_UPLOAD_REQUEST_H
#include "oss_initiate_multipart_upload_request.h"
#undef _OSS_INITIATE_MULTIPART_UPLOAD_REQUEST_H

#define _OSS_INITIATE_MULTIPART_UPLOAD_RESULT_H
#include "oss_initiate_multipart_upload_result.h"
#undef _OSS_INITIATE_MULTIPART_UPLOAD_RESULT_H

#define _OSS_LIST_MULTIPART_UPLOADS_REQUEST_H
#include "oss_list_multipart_uploads_request.h"
#undef _OSS_LIST_MULTIPART_UPLOADS_REQUEST_H

#define _OSS_LIST_OBJECTS_REQUEST_H
#include "oss_list_objects_request.h"
#undef _OSS_LIST_OBJECTS_REQUEST_H

#define _OSS_LIST_PARTS_REQUEST_H
#include "oss_list_parts_request.h"
#undef _OSS_LIST_OBJECTS_REQUEST_H

#define _OSS_MULTIPART_UPLOAD_H
#include "oss_multipart_upload.h"
#undef _OSS_MULTIPART_UPLOAD_H

#define _OSS_MULTIPART_UPLOAD_LISTING_H
#include "oss_multipart_upload_listing.h"
#undef _OSS_MULTIPART_UPLOAD_LISTING_H

#define _OSS_OBJECT_H
#include "oss_object.h"
#define _OSS_OBJECT_H

#define _OSS_OBJECT_LISTING_H
#include "oss_object_listing.h"
#undef _OSS_OBJECT_LISTING_H

#define _OSS_OBJECT_METADATA_H
#include "oss_object_metadata.h"
#undef _OSS_OBJECT_METADATA_H

#define _OSS_OBJECT_SUMMARY_H
#include "oss_object_summary.h"
#undef _OSS_OBJECT_SUMMARY_H

#define _OSS_OWNER_H
#include "oss_owner.h"
#undef _OSS_OWNER_H

#define _OSS_PART_ETAG_H
#include "oss_part_etag.h"
#undef _OSS_PART_ETAG_H

#define _OSS_PART_LISTING_H
#include "oss_part_listing.h"
#undef _OSS_PART_LISTING_H

#define _OSS_PART_SUMMARY_H
#include "oss_part_summary.h"
#undef _OSS_PART_SUMMARY_H

#define _OSS_PUT_OBJECT_RESULT_H
#include "oss_put_object_result.h"
#undef _OSS_PUT_OBJECT_RESULT_H

#define _OSS_RESPONSE_HEADER_OVERRIDES_H
#include "oss_response_header_overrides.h"
#undef _OSS_RESPONSE_HEADER_OVERRIDES_H

#define _OSS_UPLOAD_PART_REQUEST_H
#include "oss_upload_part_request.h"
#undef _OSS_UPLOAD_PART_REQUEST_H

#define _OSS_UPLOAD_PART_RESULT_H
#include "oss_upload_part_result.h"
#undef _OSS_UPLOAD_PART_RESULT_H

/* *
 * get generate authentication
 * */
#include "oss_auth.h"

#include "oss_common.h"

/* *
 * get the proper time format
 * */
#include "oss_time.h"

/* 
 *
 * */
#include "oss_compression.h"
#include "oss_decompression.h"

/* *
 * constants
 * */
#define _OSS_CONSTANTS_H
#include "oss_constants.h"
#undef _OSS_CONSTANTS_H

#include "oss_helper.h"

#include "oss_ttxml.h"



#ifdef __cplusplus
}
#endif

#endif
