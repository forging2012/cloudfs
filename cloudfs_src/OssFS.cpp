

#include "OssFS.h"
#include "OssObject.h"
#include "OssDirObject.h"
#include "OssGroupObject.h"
#include "OssSubObject.h"
#include "OssSymObject.h"
#include "OssStats.h"
#include "Ali.h"
#include "AliUtil.h"
#include "AliConf.h"
#include "log.h"
#include "util.h"
#include <iostream>
#include <string>
#include <algorithm>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <curl/curl.h>
#include "cloudfs_sqlite.h"




//using namespace afs;
//using namespace ali;
using namespace std;
//the comparer for map with const char * as key
void *sync_thread(void *p) {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	OssFS *fs = (OssFS *) p;
	return fs->sync_routine();
}




void* upload_part_task(void* args)
{
	UploadPart* part = (UploadPart *)args;
	
	if (part->subObj->sync(false) == 0)
	{
	/*
		if (part->isDropCache == true)
			part->subObj->drop_cache(true);
	*/		
	}
	
	SAFE_DELETE(part);
	return NULL;
}

void* upload_part_copy_task(void* args)
{
	UploadPartCopy* part = (UploadPartCopy *)args;
	
	string ret = part->oss->upload_part_copy(
					part->bucket.c_str(),		//bucket����
					part->object.c_str(),		//���Ƶ�Դobject����
					part->s_object.c_str(),		//copy����ʱ��Դ����
					part->uploadid.c_str(),		//uploadid
					part->part_num,				//part��
					part->start_pos, 			//��part�Ŀ�ʼ�����ֽ�
					part->end_pos);				//��part�Ľ��������ֽ�
	if (ret == "") {
		log_error("upload part copy failed. source object[%s], destination object[%s], m_part_num: %d", 
					part->s_object.c_str(), part->object.c_str(), part->part_num);
	}
	else {
		log_debug("upload part copy OK. source object[%s], destination object[%s], m_part_num: %d", 
					part->s_object.c_str(), part->object.c_str(), part->part_num);

	}
		
	SAFE_DELETE(part);
	return NULL;
}







void convert_object_to_meta(oss_object_desc_t *pObject, OSS_FILE_META &meta)
{
	stringstream ss, oss_type;
	
	/* ��ȡ�����Size */
	ss << pObject->size;
	meta["size"] = ss.str();

	log_debug("meta size: %s", meta["size"].c_str());

	/* �ж϶�������� 
	�ļ��е�name����: a/b/c/
	�����ļ���name����: a.txt a/b/c.tar
	*/
	if (pObject->name[pObject->name.size()-1] == '/')
	{
		/* ���һ���ַ�Ϊ'/', ˵���ö���ΪĿ¼���� */
		oss_type << OSS_DIR;
		
	}
	else
	{
		//�����ļ��Ƿ���к�׺ΪAliConf::SYMLINK_TYPE_NAME, ����ǲ����������ļ�
		int object_name_len = pObject->name.length();
		int symlink_postfix_len = AliConf::SYMLINK_TYPE_NAME.length();
		if (object_name_len > (symlink_postfix_len+1))
		{
			if (pObject->name.substr(object_name_len-symlink_postfix_len) == AliConf::SYMLINK_TYPE_NAME)
			{
				oss_type << OSS_LINK;

				//�������ӵ�����ȥ��AliConf::SYMLINK_TYPE_NAME��׺, ������Ҫ��".1s2l3k"ȫ��ȥ��
				pObject->name = pObject->name.substr(0, (object_name_len-(symlink_postfix_len+1)));
			}
			else
			{
				//��ͨ�ļ�
				oss_type << OSS_REGULAR;				
			}
		}
		else
		{
			//��ͨ�ļ�
			oss_type << OSS_REGULAR;
		}
	}

	meta["type"] = oss_type.str();
	
	log_debug("meta type: %s", meta["type"].c_str());

	// ����modified time����, ���������Я����x-oss-meta-mtime
	stringstream oss_time;
	oss_time << time(0);
	meta["mtime"] = oss_time.str();
	
	
	log_debug("meta mtime: %s", meta["mtime"].c_str());


	stringstream oss_mode;
	oss_mode << AliConf::ACCESS_MODE;
	meta["mode"] = oss_mode.str();
	
	log_debug("meta mode: %s", meta["mode"].c_str());

}


void create_group_stats(size_t file_size, time_t file_time,std::vector<OssStats *>& vStats)
{
	int pos = 0;
	size_t partSize = file_size % AliConf::BLOCK_SIZE;
	int count = file_size / AliConf::BLOCK_SIZE;

	if (file_time == 0)
	{
		file_time = time(0);
	}
	
	for (pos = 0; pos < count; ++pos)
	{
		OssStats* subStats = new OssStats(AliConf::BLOCK_SIZE, file_time, OSS_REGULAR);
		vStats.push_back(subStats);
		log_debug("part:[%d],size:[%d]", pos, AliConf::BLOCK_SIZE);
	}

	if (partSize)
	{
		OssStats* subStats = new OssStats(partSize, file_time, OSS_REGULAR);
		vStats.push_back(subStats);
		log_debug("part:[%d],size:[%d]", pos, partSize);
	}	
}

void delete_group_stats(vector<OssStats *>& vStats)

{
	//subobject������Ҫ����stats, �˴�ѭ���ͷ����е�stat
	std::vector<OssStats *>::iterator stat_it = vStats.begin();
	for (;stat_it != vStats.end(); stat_it++)
	{
		SAFE_DELETE(*stat_it);
	}	
}


int OssFS::AddUploadId(string uploadId, string filename)
{
	return m_uploadid_file.AddUploadId(uploadId, filename);
}
int OssFS::RemoveUploadId(string uploadId, string filename)
{
	return m_uploadid_file.RemoveUploadId(uploadId, filename);
}


void OssFS::uplodid_undone_proc()
{
	size_t pos;
	string uploadId;
	string filename;
	string result;

	list<string>::iterator iter;
	list<string> uploadIdList = m_uploadid_file.GetUploadList();
	
	for (iter = uploadIdList.begin(); iter != uploadIdList.end(); ++iter)
	{
		string value(*iter);
		pos = value.find(":");
		uploadId = value.substr(0, pos);
		filename = value.substr(pos + 1);
		
		result = m_oss->complete_multipart_upload(AliConf::BUCKET.c_str(), 
												  filename.c_str(), 
												  uploadId.c_str());
		if (result.empty()) {
			log_error("upload id[%s] failed.", value.c_str());
		}
		else {
			log_debug("upload id[%s] success.", value.c_str());
		}
	}

	/* �ύ��ɺ�, ���ԭ����uploadid list */
	m_uploadid_file.ClearUploadList();
	
}


OssFS::OssFS() 
{

	m_thread_running_flag = true;
	m_file_count = 0;
	m_cache_size = 0;

	// ����һ�� /var/log/cloudfsĿ¼�����ڴ����־�ļ�
	if (0 != create_dir("/var/log/cloudfs"))
	{
		printf("create dir[/var/log/cloudfs] failed.\n");
		exit(-1);
	}

	log_error("CloudFS Version:%s", CLOUDFS_VERSION_INFO);

	// ��ȡ ./conf/AliFS.conf �����ļ����ڴ�
	AliConf::INIT();

	// ��ʼ��sqlite ���ݿ�
	int rc = cloudfs_sqlite_init(AliConf::SQLITE_FILE_PATH.c_str());
	if (0 != rc)
	{
		cout << "sqlite init failed" << endl;
		exit(-1);
	}
	

	// ������־��ӡ����
	log_init(AliConf::LOG_LEVEL);
	
	// ��ʼ�������Ʒ�����������Ϣ������ʹ��
	m_oss = new Oss();
	m_oss->init(AliConf::HOST.c_str(), AliConf::ID.c_str(),
			AliConf::KEY.c_str());

	// ��ʼ�� CRULȫ�ֻ���
	curl_global_init(CURL_GLOBAL_ALL);
	
	pthread_mutexattr_t mutexattr;
	pthread_mutexattr_init(&mutexattr);
	pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_RECURSIVE);

	pthread_mutex_init(&m_online_sync_mutex, &mutexattr);

	pthread_mutexattr_destroy(&mutexattr);
	
	// ���ұ���uploadID�ļ������Ƿ���multipart �ļ�δ�ύ
	// �������һ��Ҫ����List Bucket����֮ǰ
	this->uplodid_undone_proc();

	// ʵ����һ�� Bucket ��
	// ������ռ������Bucket�������ļ���meta��Ϣ���ǳ���Ҫ
	m_root = new OssDirObject(this, m_oss, AliConf::BUCKET.c_str(), "/", "/",
			new OssStats(4096, 0, OSS_DIR));
	this->init_file();

	printf("cloudfs has successfully started!!!\n");
}


OssObject * OssFS::add_file(const char* path, 
							std::vector<OssStats *> &stats, 
							oss_obj_type_t type) 
{

	string filename(path);
	if (filename.at(0) == '/')
		filename = filename.substr(1);

	std::vector < string > paths;
	split(filename, paths, "/");
	
    log_debug("path:%s, paths.size:%d", path, paths.size());
	OssDirObject *root = (OssDirObject *)m_root;
	OssObject *obj = NULL;
	oss_obj_type_t tmpType;
	string tmpFilename;

	for (size_t i = 0; i < paths.size(); i++) 
	{
		log_debug("paths[%d]:%s", i, paths[i].c_str());
		OssObject * pTmp = root->get_record(paths[i].c_str());
		if (i < (paths.size() -1))
		{
			if ((pTmp != NULL) )
			{
				root = (OssDirObject *)pTmp;
				continue;
			}
			else 
			{
				// ��һ��·���е�ĳ����·����ϵͳ�л�û�б�����ʱ 
				tmpType = OSS_DIR;
				tmpFilename.clear();
				for (size_t j = 0; j <= i; j++)
				{
					tmpFilename += paths[j] + "/";
				}
			}
		}
		else //i == paths.size() -1
		{
			tmpType = type;
			tmpFilename.assign(filename);
		}

		log_debug("type:%d,filename:%s", tmpType, tmpFilename.c_str());
		
		if (tmpType == OSS_DIR) 
		{
			OssStats * o_stats = NULL;
	 		o_stats = new OssStats(4096, (time(0) - AliConf::ONLINE_SYNC_CYCLE - 2), tmpType);
			obj = new OssDirObject(this, m_oss, AliConf::BUCKET.c_str(),
								   tmpFilename.c_str(), paths[i].c_str(), o_stats);
			root->add_record(obj->get_file_name(), obj);
			root = (OssDirObject *)obj;
		} 
		else if (tmpType == OSS_REGULAR) 
		{
			obj = new OssGroupObject(this, m_oss, AliConf::BUCKET.c_str(),
									 tmpFilename.c_str(), paths[i].c_str(), stats);
			root->add_record(obj->get_file_name(), obj);						 
		} 
		else if (tmpType == OSS_LINK) 
		{
			if (stats.size() != 1)
			{
				log_error("symlink stats size error: %d", stats.size());
			}
			OssStats * o_stats = stats[0];
			obj = new OssSymObject(this, m_oss, AliConf::BUCKET.c_str(),
					tmpFilename.c_str(), paths[i].c_str(), o_stats);
			root->add_record(obj->get_file_name(), obj);		
		}
		
	}
	
	return obj;
}




int OssFS::del_file(const char *path) 
{
	if (strcmp(path, "/") == 0) 
	{
		return -ENOENT;
	} 

	OssObject *obj = find_certain_path(path);
	if (obj == NULL)
	{
		return -ENOENT;
	}
	
	//��ɾ��OSS����
	obj->delete_oss_object();

	//ɾ��meta_db�еļ�¼
	string tmpPath = obj->get_path_name();
	cloudfs_sqlite_remove_file_meta(tmpPath);

	//ɾ����Ŀ¼�еļ�¼
	OssDirObject *parent_dir = (OssDirObject *)get_parent(path);
	if (parent_dir != NULL)
	{		
		parent_dir->remove_record(obj->get_file_name());
	}

	return 0;

}





int OssFS::init_file() 
{
	//OSS_FILE_MAP objects;
	vector<oss_object_desc_t> objects;	//GET BUCKET��ȡ��������object��Ϣ
	OSS_OBJS_MAP bucket_files;

	int ret_code = 0;

	printf("Start to load object from oss: \n");
	time_t t_start = time(0);
	// ͨ�� Get Bucket �ӿڴӷ�������ȡ�����Bucket�¸�Ŀ¼��meta��Ϣ
	ret_code = m_oss->get_bucket(AliConf::BUCKET.c_str(), "", "/", objects);
	if (0 != ret_code)
	{
		printf("Load File basic info from [%s] failed\n", AliConf::BUCKET.c_str());
		exit(-1);
	}

	time_t t_get_bucket_end = time(0);
	printf("get_bucket finished: time cost %ld second\n", (t_get_bucket_end-t_start));
	m_files.insert(pair<const char*, OSS_OBJS_MAP *>(AliConf::BUCKET.c_str(), m_root->get_subs()));

	time_t t_end_insert = time(0);
	printf("insert root node: time cost %ld second\n", (t_end_insert-t_get_bucket_end));
	// �����ļ�meta��Ϣ���ڴ�
	load_files(objects, true, 0);
	time_t t_end_load = time(0);
	printf("load_files: time cost %ld second\n", (t_end_load-t_end_insert));
		
	return 0;
}


OssObject* OssFS::get_parent(const char* path)
{

	string strPath = path;

	if (0 == strcmp(path, "/"))
	{
		return NULL;
	}

	//������ǰ���'/'
	if (strPath.at(0) != '/')
	{
		strPath = "/" + strPath;
	}

	//ȥ��������'/'
	if (strPath.at(strPath.length()-1) == '/')
	{
		strPath = strPath.substr(0, strPath.length()-1);
	}

	size_t last_pos = strPath.rfind('/');
	if (last_pos == string::npos)
	{
		//path����û���ҵ�'/', ֱ�ӷ���ʧ��
		log_error("find last / failed");
		return NULL;
	}

	//���츸Ŀ¼��path
	string parent_dir_path = strPath.substr(0, (last_pos+1));

	return get_file(parent_dir_path.c_str());
	
}


// �������Ĺ�������Ϊÿ���ļ�ʵ����һ����Ӧ����
// Ŀ¼�ļ�ʵ����Ϊ OssDirObject 
int OssFS::load_files(vector<oss_object_desc_t>& objects, bool initial_load, size_t sync_flag)
{
	std::vector<OssStats *> group_stats;
	vector<oss_object_desc_t>::iterator iter;
	OSS_FILE_META meta;
	OssObject *tmpObj = NULL;	

	for (iter = objects.begin(); iter != objects.end(); iter++) 
	{
		meta.clear();
		convert_object_to_meta(&(*iter), meta);

		//�ȼ�鱾���ļ��Ƿ��Ѿ�����, ����Ѿ���������Ҫ����
		//��һ�ε�ȫ�����ز����б�����ļ��, ��߼����ٶ�
		if (!initial_load)
		{
			tmpObj = get_file(iter->name.c_str());
			
			if (tmpObj != NULL)
			{

				//���жϸ��ļ��Ƿ��Ѿ����򿪶�д, ����Ѿ�����, �򱣳ָ��ļ�״̬���仯
				if (tmpObj->is_open())
				{
					log_error("file [%s] is opened, ignore it", iter->name.c_str());
					if (sync_flag != 0)
					{
						tmpObj->set_sync_flag(sync_flag);
					}					
					continue;
				}
			
				/* obj������Ŀ¼, ��ͨ�ļ�, �����ļ�������һ��
				   Ŀ¼: ����ͬ����ʶ, ֱ������
				   ��ͨ�ļ��������ļ�: ����ļ��ļ���С��һ��, ֱ��ɾ��cloudfs�еĶ���, ���´���һ���µĶ���
				*/				
				OssStats *pTmpStat = const_cast<OssStats *>(tmpObj->get_stats());
				if (pTmpStat->type == OSS_DIR)
				{		
						if (sync_flag != 0)
						{
							tmpObj->set_sync_flag(sync_flag);
						}	
						continue;
					
				}				
				else if ((pTmpStat->type == OSS_REGULAR) || (pTmpStat->type == OSS_LINK))
				{														
					//��̨�ļ���ǰ̨�ļ�һ��, ֱ��continue					
					if (pTmpStat->get_size() == iter->size) 
					{
						if (sync_flag != 0)
						{
							tmpObj->set_sync_flag(sync_flag);
						}	
						continue;
					}

					log_error("file [%s] in OSS size[%zd] is different from cloudfs size[%zd], updating ...", 
									iter->name.c_str(), iter->size, pTmpStat->get_size());
					
					//��̨�ļ���ǰ̨�ļ���һ��, ɾ��ǰ̨����, ��������´����ö���
					//ɾ���ϲ�Ŀ¼���иö������Ϣ
					OssDirObject * parent_dir = (OssDirObject *)get_parent(iter->name.c_str());
					if (parent_dir != NULL)
					{
						parent_dir->remove_record(tmpObj->get_file_name());
					}
					else
					{
						log_error("file [%s] get parent directory failed", iter->name.c_str());
					}

				}
				else
				{
					log_error("file [%s] invalid object type %zd", iter->name.c_str(), pTmpStat->type);
					continue;
				}

				
			}
		}
		
		log_debug("file:[%s] need online sync", iter->name.c_str());

		group_stats.clear();
		OssStats stats = OssStats(meta);
		if (stats.type == OSS_REGULAR)
		{
			log_debug("stats.size:[%d]", stats.size);
			create_group_stats(stats.size, 0, group_stats);
			tmpObj = this->add_file(iter->name.c_str(), group_stats, stats.type);
			//subobject������Ҫ����stats, �˴�ѭ���ͷ����е�stat
			delete_group_stats(group_stats);
			
		}
		//���Ӷ�LINK�ļ��ĳ�ʼ������
		else if (stats.type == OSS_DIR)
		{
			tmpObj = this->add_file(iter->name.c_str(), group_stats, stats.type);
		}
		else if (stats.type == OSS_LINK)
		{
			OssStats *pTmpStat = new OssStats(meta);
			group_stats.push_back(pTmpStat);
			tmpObj = this->add_file(iter->name.c_str(), group_stats, stats.type);
		}
		else
		{
			log_error("invalid stats.type %d", stats.type);
		}

		/* ���add_file�Ľ��, �����ö�Ӧ��sync_flag */
		if (tmpObj != NULL)
		{
			if (sync_flag != 0)
				tmpObj->set_sync_flag(sync_flag);
		}
	}

	return 0;
}


OssObject *OssFS::get_file(const char *path) 
{
	log_debug("path: %s", path);
	 
	if (strcmp(path, "/") == 0) 
	{ 
		return m_root;
	} 

	string filename(path);
	if (filename.at(0) == '/')
		filename = filename.substr(1);
	std::vector < string > paths;
	split(filename, paths, "/");
	OssDirObject *root = (OssDirObject *)m_root;
	OssObject *obj = NULL;
	for (size_t i = 0; i < paths.size(); i++) 
	{
		obj = root->get_record(paths[i].c_str());	
		if (obj != NULL) 
		{
			root = (OssDirObject *)obj;
		} 
		else 
		{ 
			return NULL;
		}				
	}
	
	return obj;	
}

OssObject* OssFS::find_certain_path(const char* path)
{

	log_debug("path:[%s]", path);
	
	OssObject* obj = get_file(path);
	if (obj != NULL)
	{
		return obj;
	}

	if (0 == AliConf::IMMEDIATE_SYNC)
	{
		return NULL;
	}
	
	int res = 0;	
	vector<oss_object_desc_t> object_list;
	oss_object_desc_t out_put;

	res = m_oss->get_object_meta(AliConf::BUCKET.c_str(), 
						  		path,
						  		out_put);
	if (0 != res)
	{
		//head_object failed, object is not exist
		return NULL;							  		
	}
						  		
	//construct load_file functon input
	object_list.push_back(out_put);
	size_t sync_flag = get_parent(path)->get_sync_flag();
	load_files(object_list, false, sync_flag);

	obj = get_file(path);

	return obj;
}


OssObject* OssFS::find_file(const char* path)
{
	log_debug("path:[%s]", path);
	int res = 0;
	size_t sync_flag = 0;

	//�����ڴ��в���, ���ҵ�ֱ�ӷ��ض���
	OssObject* obj = get_file(path);
	if (obj != NULL)
	{
		return obj;
	}	

	//����ڴ����Ҳ���, ���ж��Ƿ���Ҫ��OSS�������Ƿ����
	if (0 == AliConf::IMMEDIATE_SYNC)
	{
		return NULL;
	}

	/*��OSS��ȡ����, ����������getattr/chown/chmod�Ȳ�ȷ�����ļ������ļ��е�·��
	  ���������·����Ҫ�������� */
	vector<oss_object_desc_t> object_list;
	oss_object_desc_t out_put;

	res = m_oss->get_object_meta(AliConf::BUCKET.c_str(), 
						  		path,
						  		out_put);
	//�˴���ѯʧ��, ˵����Ӧpath���ļ����󲻴���
	if (0 != res)
	{
		//������Ŀ¼�������ʽ�ٲ�ѯһ��
		string tmp1 = path;
		string tmp2 = "/";
		string dir_path = tmp1 + tmp2;
		res = m_oss->get_object_meta(AliConf::BUCKET.c_str(), dir_path.c_str(), out_put);
		if (0 != res)
		{
			return NULL;
		}							  		
	}
						  		
	//construct load_file functon input
	object_list.push_back(out_put);

	load_files(object_list, false, sync_flag);	
	obj = get_file(path);

	return obj;
}






OssFS::~OssFS() 
{
	// TODO Auto-generated destructor stub
	SAFE_DELETE(m_root);
	SAFE_DELETE(m_oss);
	pthread_mutex_destroy(&m_online_sync_mutex);

}


int OssFS::getattr(const char *path, struct stat *statbuf) 
{
	log_debug("path:%s", path);	
	int res = 0;
	
	OssObject *obj = this->find_file(path);

	log_debug("immediate sync:%zd, obj:%p", AliConf::IMMEDIATE_SYNC, obj);
	if (obj != NULL) 
	{
		memset(statbuf, 0, sizeof(struct stat));
		if (obj->get_stats()->type == OSS_DIR)
			statbuf->st_mode = S_IFDIR | obj->get_stats()->mode;
		else if (obj->get_stats()->type == OSS_REGULAR)
			statbuf->st_mode = S_IFREG | obj->get_stats()->mode;
		else if (obj->get_stats()->type == OSS_LINK)
			statbuf->st_mode = S_IFLNK | obj->get_stats()->mode;
		statbuf->st_nlink = 2;
		statbuf->st_size = obj->get_stats()->size;
		statbuf->st_atim.tv_sec = obj->get_stats()->mtime;
		statbuf->st_atim.tv_nsec = 0;
		statbuf->st_mtim.tv_sec = obj->get_stats()->mtime;
		statbuf->st_mtim.tv_nsec = 0;
		statbuf->st_ctim.tv_sec = obj->get_stats()->mtime;
		statbuf->st_ctim.tv_nsec = 0;
		statbuf->st_uid = getuid();
		statbuf->st_gid = getgid();
	} else
		res = -ENOENT;
	return res;
}
int OssFS::readlink(const char *path, char *link, size_t size) {
	log_debug("path:%s, link:%s", path, link);
	OssObject *obj = find_certain_path(path);	
	if (obj == NULL || obj->get_stats()->type != OSS_LINK) {
		return -ENOENT;
	}
	OssSymObject *sym = (OssSymObject *) obj;
	sym->get_link_obj(link, size);
	return 0;
}


int OssFS::mkdir(const char *path, mode_t mode) 
{
	log_debug("path:%s", path);
	std::vector<OssStats *> dir_stats;
	string tmp_str = path;
    if (tmp_str.at(tmp_str.length() - 1) != '/') {
        tmp_str.append("/");
    }

	OssDirObject* parentObj = (OssDirObject *)get_parent(path);
	if (NULL == parentObj)
	{
		log_error("path [%s]", path);
		return -ENOENT;
	}
	OssObject *obj = this->add_file(tmp_str.c_str(), dir_stats, OSS_DIR);
	if (NULL == obj)
	{
		return -ENOENT;
	}

	OSS_FILE_META meta;
	obj->get_stats()->to_meta(meta);
    m_oss->put_object_data(AliConf::BUCKET.c_str(), obj->get_path_name(), meta);


	return 0;
}


int OssFS::unlink(const char *path) 
{
	log_debug("path:%s", path);
	
	OssDirObject* parentObj = (OssDirObject *)get_parent(path);
	if (NULL == parentObj)
	{
		log_error("path [%s]", path);
		return -ENOENT;
	}
	this->del_file(path);
	
	return 0;
}


int OssFS::rmdir(const char *path) 
{
	log_debug("path:%s", path);
	string tmp_str = path;
    if (tmp_str.at(tmp_str.length() - 1) != '/')
        tmp_str.append("/");

	OssDirObject* parentObj = (OssDirObject *)get_parent(path);
	if (NULL == parentObj)
	{
		log_error("path [%s]", path);
		return -ENOENT;
	}

	this->del_file(tmp_str.c_str());
	
	return 0;
}


int OssFS::symlink(const char *path, const char *link) 
{
	log_debug("path:%s", path);
	std::vector<OssStats *> sym_stats;

	//�ڴ˴�����link_stats��Ŀ����Ϊ������link stats��ʵ�ʳ���, ����OssSymObject
	//��ʼ��ʱ������Ӧ���ȵ��ڴ�
	int path_len = strlen(path);
	

	OssDirObject* parentObj = (OssDirObject *)get_parent(path);
	if (NULL == parentObj)
	{
		log_error("path [%s], link [%s]", path, link);
		return -ENOENT;
	}
	OssStats *link_stats = new OssStats(path_len, time(0), OSS_LINK);
	sym_stats.push_back(link_stats);

	OssSymObject *sym = (OssSymObject *)(this->add_file(link, sym_stats, OSS_LINK));	
	
	sym->set_link_obj(path, path_len);
	
	return 0;
}

/*
int OssFS::rename(const char *path, const char *newpath) {
	log_debug("path:%s, newpath:%s", path, newpath);
    int ret = 0;

	if (strcmp(path, "/") == 0 || strcmp(newpath, "/") == 0) {
		return -ENOENT;
	} else {
		string filename(path);
		if (filename.at(0) == '/')
			filename = filename.substr(1);
		std::vector < string > paths;
		split(filename, paths, "/");
		OSS_OBJS_MAP *root = &(m_files[AliConf::BUCKET.c_str()]);
        OssObject *old_obj = NULL;
        const OssStats *old_stats = NULL;
        string str_newpath(newpath);
		for (size_t i = 0; i < paths.size(); i++) {
			if (i == paths.size() - 1) {
                old_obj = (*root)[paths[i].c_str()];
	            std::vector<OssStats *> stats;
                old_stats = old_obj->get_stats();
                OSS_FILE_META meta;
                old_stats->to_meta(meta);
                OssStats *new_stats = new OssStats(meta);
                if (old_stats->type != OSS_REGULAR) {
                    stats.push_back(new_stats);
                }
                if (old_stats->type == OSS_DIR) {
                    str_newpath += "/";
                }
                this->add_file(str_newpath, stats, old_stats->type);
			}
			root = (*root)[paths[i].c_str()]->get_subs();
		}


        if (old_stats && old_stats->type == OSS_DIR) {
            map<const char *, OssObject *>::iterator it;
            string str_new_sub_path = str_newpath;
            for (it = root->begin(); it != root->end(); it++) {
                str_new_sub_path = str_newpath + "/" + it->second->get_file_name();    
                ret = this->rename(it->second->get_path_name(), str_new_sub_path.c_str());
            }
        } else if (old_stats && old_stats->type == OSS_REGULAR) {
            struct fuse_file_info fi_old;
            struct fuse_file_info fi_new;
            off_t offset = 0;
            off_t nbytes = 0;
            size_t size = 1 * 1024 * 1024;
            char buf[1 * 1024 * 1024];
            
            this->open(path, &fi_old);
            this->open(newpath, &fi_new);

            while((nbytes = this->read(path, buf, size, offset, &fi_old)) > 0) {
                nbytes = this->write(newpath, buf, nbytes, offset, &fi_new);
                offset += nbytes;
            }
            this->release(newpath, &fi_new);
            this->release(path, &fi_old);
        }

        this->del_file(path);
	}
	return ret;
}*/


int OssFS::rename(const char *path, const char *newpath) 
{
	int ret = link(path, newpath);
	if (ret != 0)
	{
		return ret;
	}

	//ɾ�����ļ�
	this->del_file(path);	
	return ret;
}


/*
 * Make hard link just same with symbol link
 */
int OssFS::link(const char *path, const char *newpath) 
{
	log_debug("path:%s, newpath:%s", path, newpath);
    int ret = 0;

	if (strcmp(path, "/") == 0 || strcmp(newpath, "/") == 0) 
	{
		return -ENOENT;
	} 

	OssObject *old_obj = find_file(path);
	if (NULL == old_obj)
	{
		return -ENOENT;
	}

	const OssStats *old_stats = old_obj->get_stats();
	std::vector<OssStats *> stats;

	//��ǰ��֧���޸��ļ�������, ��Ϊ�ļ��и���Ӱ��̫��, ��Ҫ��OSS�ϸ��ļ����µĶ���ȫ������һ��
    if (old_stats->type == OSS_DIR) 
	{
       log_debug("Now not support rename a directory.");
	   return -ENOENT;
    }
    else if (old_stats->type == OSS_REGULAR)
	{
		create_group_stats(old_stats->size, old_stats->mtime, stats);
		add_file(newpath, stats, old_stats->type);
		delete_group_stats(stats);
	}
	else if (old_stats->type == OSS_LINK)
	{	
		//OSS_LINK
		OSS_FILE_META meta;
		old_stats->to_meta(meta);				
		OssStats *new_stats = new OssStats(meta);
		stats.push_back(new_stats);
		add_file(newpath, stats, old_stats->type);
	}

	string newname(newpath);
	if (newname.at(0) == '/')
	{
		newname = newname.substr(1);	
	}
	
	// ͬ��OSS��̨����, ���ļ�С�ڻ����һ����ʱ, ʹ��copy_object; ����1����ʹ��multi_part�ӿڼӿ��ϴ��ٶ�
	if (old_stats->size <= AliConf::BLOCK_SIZE) {
    	old_obj->copy(newname.c_str());
	}
	else
	{		
		((OssGroupObject *)old_obj)->multi_copy(newname.c_str());
	}
	
	return ret;

}
/*
 * THe mode information should be stored in the OSS server
 */
int OssFS::chmod(const char *path, mode_t mode) {
	log_debug("path:%s", path);
	OssObject *obj = this->find_file(path);
		
	if (obj != NULL) {
		obj->set_mode(mode);
	}
	return 0;
}
/*
 * OWN is not support yet, all the
 */
int OssFS::chown(const char *path, uid_t uid, gid_t gid) {
	log_debug("path:%s", path);
	return 0;
}


int OssFS::truncate(const char *path, off_t newSize) {
	log_debug("path:%s", path);

	OssObject *obj = this->find_certain_path(path);

	if (obj == NULL) {
		return -ENOENT;	
	}

	//�������������ж�����ļ�����, cloudfsĿǰֻ֧����ͨ�ļ���truncate
	if (obj->get_stats()->type != OSS_REGULAR)
	{
		log_debug("Error Truncate type");
		return -1;
	}

	OssGroupObject *pTmpObj = (OssGroupObject *)obj;
	return pTmpObj->truncate(newSize);

}
/*
 * TODO: not sure if must needed.
 */
int OssFS::utime(const char *path, struct utimbuf *ubuf) 
{
	log_debug("path:[%s]", path);
	OssObject *obj = this->find_file(path);

	if (obj == NULL)
	{
		return -ENOENT;
	}

	if (NULL == ubuf)
	{
		//input time is NULL, use current system time
		time_t tmpTime = time(0);
		return obj->set_ftime(tmpTime, tmpTime);
	}
	else
	{
		log_debug("ubuf->actime %Zd, ubuf->modtime %Zd", ubuf->actime, ubuf->modtime);
		obj->set_ftime(ubuf->actime, ubuf->modtime);		
	}
	
	return 0;
	
}


int OssFS::access(const char *path, int mode) 
{
	log_debug("path:[%s]", path);
	OssObject *obj = this->find_file(path);

	if (obj == NULL)
	{
		return -ENOENT;
	}
				
	return 0;
}


int OssFS::open(const char *path, struct fuse_file_info *fileInfo) 
{
	log_debug("path:[%s]", path);
	OssObject *obj = this->find_certain_path(path);
	
	if (obj != NULL) {
		return obj->fopen(fileInfo);
	} else
		return -ENOENT;
}


int OssFS::read(const char *path, char *buf, size_t size, 
				off_t offset, struct fuse_file_info *fileInfo) 
{
	log_debug("path:[%s]", path);
	OssObject *obj = this->find_certain_path(path);;
	
	if (obj != NULL) {
		return obj->read_data(offset, size, buf);
	}
	return 0;
}


int OssFS::write(const char *path, const char *buf, size_t size, 
				 off_t offset, struct fuse_file_info *fileInfo) 
{
	log_debug("path:%s", path);
	OssObject *obj = this->find_certain_path(path);;
	
	if (obj != NULL) {
		return obj->write_data(offset, size, buf);
	}
	return 0;
}


int OssFS::statfs(const char *path, struct statvfs *statInfo) {
	log_debug("path:%s", path);

	unsigned long a = 1024*1024;
	unsigned long b = 1024*1024;
	
	statInfo->f_bsize = 4096;
	statInfo->f_frsize = 4096;
	statInfo->f_blocks = a * b;
	statInfo->f_bfree = a * b;
	statInfo->f_bavail = a * b;
	statInfo->f_files = a * b;
	statInfo->f_ffree = a * b;
	statInfo->f_favail = a * b;
	statInfo->f_fsid = 123456789;
	statInfo->f_namemax = 256;
	
	return 0;
}
int OssFS::flush(const char *path, struct fuse_file_info *fileInfo) {
	log_debug("path:%s", path);
	/*OssObject *obj = this->get_file(path);
	 if (obj != NULL) {
	 return obj->sync(false);
	 }*/
	return 0;
}


int OssFS::release(const char *path, struct fuse_file_info *fileInfo) 
{
	log_debug("path:%s", path);
	OssObject *obj = this->find_certain_path(path);;

	log_debug("obj:%p", obj);
	if (obj == NULL) 
	{
		return -ENOENT;
	}
		
	int res = obj->fclose(fileInfo);
	return res;

}
int OssFS::fsync(const char *path, int datasync, struct fuse_file_info *fi) {
	log_debug("path:%s", path);
	/*OssObject *obj = this->get_file(path);
	 if (obj != NULL) {
	 return obj->sync(false);
	 }*/
	return 0;
}

int OssFS::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
				   off_t offset, struct fuse_file_info *fileInfo) 
{
	log_debug("path:%s", path);
	(void) offset;
	(void) fileInfo;

	//��׼��������Ŀ¼, ��ǰ��Ŀ¼���ϼ�Ŀ¼
	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	OssObject *pTargetDir = NULL;
	OSS_OBJS_MAP *target_dir = NULL;
	string Prefix;	
	const char *pDelimiter = "/";	
		
	if (strcmp(path, "/") == 0) 
	{
		//��Ŀ¼����Ҫ����Prefix
		Prefix = "";					
	} 
	else
	{
		//�Ǹ�Ŀ¼, prefix��װ��ʽΪfun/ģʽ
		Prefix = (path+1);
		Prefix = Prefix + "/";		
	}

	pTargetDir = get_file(path);
	target_dir = pTargetDir->get_subs();


	int is_syncd = 0;
	
	//���ͬ��ʱ������, ���ʱ������Ϊ0, �������ý����ļ��з���ʱ��ˢ��
	if (AliConf::ONLINE_SYNC_CYCLE != 0)
	{
		//pTargetDir�ǵ�ǰҪ��ȡ��Ŀ¼����, �жϵ�ǰ��������һ�β�����ʱ����
		OssStats *pStat = (OssStats *)pTargetDir->get_stats();
		time_t current_time = time(0);
		vector<oss_object_desc_t> object_list;
		if ((size_t)(current_time - pStat->mtime) > AliConf::ONLINE_SYNC_CYCLE)
		{								
			size_t sync_flag = pTargetDir->get_sync_flag() + 1;
			pTargetDir->set_sync_flag(sync_flag);
			
			int ret_code = m_oss->get_bucket(AliConf::BUCKET.c_str(),
							  Prefix.c_str(),
							  pDelimiter,
							  object_list);
			if (ret_code != 0)
			{
				//�����OSS�������Ŀ¼���ļ�����Ŀ¼ʧ��, ��¼error��־, �������еĻ������ݼ�������
				log_error("get_bucket failed");
			}
			else
			{
				//���¶���ķ���ʱ��
				pStat->mtime = current_time;
				
				//����load�������ļ�
				load_files(object_list, false, pTargetDir->get_sync_flag());

				//�������溯���ж��Ƿ������һ��ͬ��
				is_syncd = 1;
			}			
		}
	}

	OssObject* tmpObj;
	map<const char *, OssObject *>::iterator it;
	vector<const char *> vecDel;
	vector<const char *>::iterator vec_iter;

	//����Ŀ¼��map�ṹ, ����Ŀ¼�Ķ���
	((OssDirObject *)pTargetDir)->rdlock();
	for (it = target_dir->begin(); it != target_dir->end(); it++) 
	{
		tmpObj = it->second;

		if (1 == is_syncd)
		{
			// ����readdir���й�����ͬ��
			if (tmpObj->get_sync_flag() == pTargetDir->get_sync_flag()) 
			{
				filler(buf, it->first, NULL, 0);
			}
			else
			{
				log_debug("file:[%s] need delete.", it->first);
				vecDel.push_back(it->first);
			}			
		}
		else
		{
			// ����readdirû�н��й�����ͬ��, ����Ҫ�Ƚ�ͬ��flag
			filler(buf, it->first, NULL, 0);
		}
		
	}

	//��������, �ر���
	((OssDirObject *)pTargetDir)->unlock();

	for (vec_iter = vecDel.begin(); vec_iter != vecDel.end(); vec_iter++) {
		del_file(*vec_iter);
	}

	return 0;
}
int OssFS::create(const char *path, mode_t mode, struct fuse_file_info *fi) 
{
	log_debug("path:%s", path);
	std::vector<OssStats *> stats;
	
	OssObject * obj = this->add_file(path, stats, OSS_REGULAR);
	if (NULL == obj)
	{
		return -ENOENT;
	}

    obj->set_write();
	
	return obj->fopen(fi);
}
/*
 * TODO: sync all files under this dir
 */
int OssFS::fsyncdir(const char *path, int datasync,
		struct fuse_file_info *fileInfo) {
	log_debug("path:%s", path);
	return 0;
}


void *OssFS::sync_routine() {
	while (m_thread_running_flag) {
		sleep(1);
		cout << m_cache_size << endl;
	}
	return 0;
}



int OssFS::threadpool_add_task(UploadPart* part)
{
	if (thpool_add_work(m_pool, upload_part_task, part) != 0)
	{
		log_error("add upload_part task failed.");
		return -1;
	}
	return 0;
}


bool OssFS::threadpool_is_busy()
{
	int res = thpool_thread_workload_full(m_pool);
	if (1 == res)
	{
		return true;
	}
	else if (0 == res)
	{
		return false;
	}
	else
	{
		log_error("unexpected return value %d", res);
		return true;
	}
}


int OssFS::threadpool_add_upload_part_copy_task(UploadPartCopy* part)
{
	if (thpool_add_work(m_pool, upload_part_copy_task, part) != 0)
	{
		log_error("add upload_part task failed.");
		return -1;
	}
	return 0;
}



/*
 * Init the sync thread, this thread will check the cache and sync them to the OSS
 * server each seconds
 */
void OssFS::init(struct fuse_conn_info *conn) {
	int s;
	pthread_attr_t attr;
	s = pthread_attr_init(&attr);
	if (s != 0)
		handle_error_en(s, "pthread_attr_init");
	s = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (s != 0)
		handle_error_en(s, "pthread_attr_setdetachstate");
	s = pthread_create(&m_sync_thread, &attr, &sync_thread, this);
	if (s != 0)
		handle_error_en(s, "pthread_create");

	// ��ʼ���̳߳أ����� upload_part �¼�
	m_pool = thpool_init(AliConf::MAX_UPLOAD_THREADS);
	if (m_pool == NULL)
	{
		log_error("thread pool init failed.");
		exit(-1);
	}

	s = pthread_attr_destroy(&attr);
	if (s != 0)
		handle_error_en(s, "pthread_attr_destory");
	
}
//TODO: should stop the sync thread here, but did not find a good way yet...
void OssFS::destroy() {
	//int s;
	//s = pthread_detach(m_sync_thread);
	//if (s != 0)
	//    handle_error_en(s, "pthread_detach");
	//s = pthread_cancel(m_sync_thread);
	//if (s != 0)
	//    handle_error_en(s, "pthread_cancel");
	m_thread_running_flag = false;
	cout << "des" << endl;
}
