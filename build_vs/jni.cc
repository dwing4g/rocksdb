#include <stdio.h>
#include <sstream>
#include "../jni/jni.h"
#include "rocksdb/db.h"
#include "rocksdb/env.h"
#include "rocksdb/options.h"
#include "rocksdb/cache.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/filter_policy.h"
#include "rocksdb/iterator.h"
#include "port/port.h"
#include "db/db_impl.h"
#include "db/filename.h"
#include "db/version_set.h"
#include "table/format.h"
#include "util/coding.h"

using namespace rocksdb;

static const jint	WRITE_BUFSIZE_MIN	= 1 << 20;
static const jint	CACHE_SIZE_MIN		= 1 << 20;
static const int	BLOOM_FILTER_BITS	= 10;
static const size_t	FILE_BUF_SIZE		= 1 << 16;

static const ReadOptions	g_ro_cached;	// safe for global shared instance
static ReadOptions			g_ro_nocached;	// safe for global shared instance
static WriteOptions			g_wo_sync;		// safe for global shared instance
static const FilterPolicy*	g_fp = 0;		// safe for global shared instance

namespace rocksdb { port::Mutex g_mutex_backup; }

// public native static long rocksdb_open(String path, int write_bufsize, int cache_size, boolean use_snappy);
extern "C" JNIEXPORT jlong JNICALL Java_jane_core_StorageRocksDB_rocksdb_1open
	(JNIEnv* jenv, jclass jcls, jstring path, jint write_bufsize, jint cache_size, jboolean use_snappy)
{
	if(!path) return 0;
	const char* pathptr = jenv->GetStringUTFChars(path, 0);
	if(!pathptr) return 0;
	std::string pathstr(pathptr);
	jenv->ReleaseStringUTFChars(path, pathptr);
	Options opt;
	opt.create_if_missing = true;
	opt.write_buffer_size = (write_bufsize > WRITE_BUFSIZE_MIN ? write_bufsize : WRITE_BUFSIZE_MIN);
	// opt.block_cache = NewLRUCache(cache_size > CACHE_SIZE_MIN ? cache_size : CACHE_SIZE_MIN);
	opt.compression = (use_snappy ? kSnappyCompression : kNoCompression);
	// opt.filter_policy = (g_fp ? g_fp : (g_fp = NewBloomFilterPolicy(BLOOM_FILTER_BITS)));
	g_ro_nocached.fill_cache = false;
	g_wo_sync.sync = true;
	DB* db = 0;
	Status s = DB::Open(opt, pathstr, &db);
	return s.ok() ? (jlong)db : 0;
}

// public native static void rocksdb_close(long handle);
extern "C" JNIEXPORT void JNICALL Java_jane_core_StorageRocksDB_rocksdb_1close
	(JNIEnv* jenv, jclass jcls, jlong handle)
{
	if(handle)
	{
		DB* db = (DBImpl*)handle;
		// DBImpl* dbi = dynamic_cast<DBImpl*>(db);
		// Cache* cache = (dbi ? dbi->GetOptions().block_cache : 0);
		delete db;
		// if(cache) delete cache;
	}
}

// public native static byte[] rocksdb_get(long handle, byte[] key, int keylen); // return null for not found
extern "C" JNIEXPORT jbyteArray JNICALL Java_jane_core_StorageRocksDB_rocksdb_1get
	(JNIEnv* jenv, jclass jcls, jlong handle, jbyteArray key, jint keylen)
{
	DB* db = (DB*)handle;
	if(!db || !key) return 0;
	jsize m = jenv->GetArrayLength(key);
	if(keylen > m) keylen = m;
	if(keylen <= 0) return 0;
	jbyte* keyptr = jenv->GetByteArrayElements(key, 0);
	if(!keyptr) return 0;
	std::string valstr;
	Status s = db->Get(g_ro_cached, Slice((const char*)keyptr, (size_t)keylen), &valstr);
	jenv->ReleaseByteArrayElements(key, keyptr, JNI_ABORT);
	if(!s.ok()) return 0;
	jsize vallen = valstr.size();
	jbyteArray val = jenv->NewByteArray(vallen);
	jenv->SetByteArrayRegion(val, 0, vallen, (const jbyte*)valstr.data());
	return val;
}

// public native static int rocksdb_write(long handle, Iterator<Entry<Octets, OctetsStream>> buf); // return 0 for ok
extern "C" JNIEXPORT jint JNICALL Java_jane_core_StorageRocksDB_rocksdb_1write
	(JNIEnv* jenv, jclass jcls, jlong handle, jobject it)
{
	DB* db = (DB*)handle;
	if(!db || !it) return 1;
	static jclass cls_it = 0;
	static jclass cls_entry = 0;
	static jclass cls_octets = 0;
	static jmethodID mid_hasNext = 0;
	static jmethodID mid_next = 0;
	static jmethodID mid_getKey = 0;
	static jmethodID mid_getValue = 0;
	static jfieldID fid_buffer = 0;
	static jfieldID fid_count = 0;
	static jint s_err = -1;
	if(s_err < 0)
	{
		cls_it = jenv->FindClass("java/util/Iterator");
		cls_entry = jenv->FindClass("java/util/Map$Entry");
		cls_octets = jenv->FindClass("jane/core/Octets");
		if(!cls_it || !cls_entry || !cls_octets) return s_err = 2;
		cls_it = (jclass)jenv->NewGlobalRef(cls_it);
		cls_entry = (jclass)jenv->NewGlobalRef(cls_entry);
		cls_octets = (jclass)jenv->NewGlobalRef(cls_octets);
		mid_hasNext = jenv->GetMethodID(cls_it, "hasNext", "()Z");
		mid_next = jenv->GetMethodID(cls_it, "next", "()Ljava/lang/Object;");
		mid_getKey = jenv->GetMethodID(cls_entry, "getKey", "()Ljava/lang/Object;");
		mid_getValue = jenv->GetMethodID(cls_entry, "getValue", "()Ljava/lang/Object;");
		fid_buffer = jenv->GetFieldID(cls_octets, "_buffer", "[B");
		fid_count = jenv->GetFieldID(cls_octets, "_count", "I");
		if(!mid_hasNext || !mid_next || !mid_getKey || !mid_getValue || !fid_buffer || !fid_count) return s_err = 3;
		s_err = 0;
	}
	else if(s_err > 0) return s_err;
	if(jenv->IsInstanceOf(it, cls_it) == JNI_FALSE) return 4;
	WriteBatch wb;
	while(jenv->CallBooleanMethod(it, mid_hasNext) == JNI_TRUE)
	{
		jobject entry = jenv->CallObjectMethod(it, mid_next);
		jobject key = jenv->CallObjectMethod(entry, mid_getKey);
		if(key)
		{
			jbyteArray keybuf = (jbyteArray)jenv->GetObjectField(key, fid_buffer); // check cast?
			jint keylen = jenv->GetIntField(key, fid_count);
			if(keybuf && keylen > 0)
			{
				jbyte* keyptr = jenv->GetByteArrayElements(keybuf, 0);
				if(keyptr)
				{
					jobject val = jenv->CallObjectMethod(entry, mid_getValue);
					if(val)
					{
						jbyteArray valbuf = (jbyteArray)jenv->GetObjectField(val, fid_buffer); // check cast?
						jint vallen = jenv->GetIntField(val, fid_count);
						if(valbuf && vallen > 0)
						{
							jbyte* valptr = jenv->GetByteArrayElements(valbuf, 0);
							if(valptr)
							{
								wb.Put(Slice((const char*)keyptr, (size_t)keylen), Slice((const char*)valptr, (size_t)vallen));
								jenv->ReleaseByteArrayElements(valbuf, valptr, JNI_ABORT);
							}
						}
						else
							wb.Delete(Slice((const char*)keyptr, (size_t)keylen));
					}
					else
						wb.Delete(Slice((const char*)keyptr, (size_t)keylen));
					jenv->ReleaseByteArrayElements(keybuf, keyptr, JNI_ABORT);
				}
			}
		}
	}
	return db->Write(g_wo_sync, &wb).ok() ? 0 : 5;
}

static int64_t AppendFile(Env& env, const std::string& srcfile, const std::string& dstfile, bool checkmagic)
{
	uint64_t srcsize = 0, dstsize = 0;
	std::unique_ptr<SequentialFile> sf = 0;
	Slice slice;
	const EnvOptions envOpt;
	char buf[FILE_BUF_SIZE];
	env.GetFileSize(srcfile, &srcsize);
	env.GetFileSize(dstfile, &dstsize);
	if(checkmagic)
	{
		if(srcsize < 8) return -11;
		std::unique_ptr<RandomAccessFile> raf = 0;
		if(!env.NewRandomAccessFile(srcfile, &raf, envOpt).ok() || !raf) return -12;
		if(!raf->Read(srcsize - 8, 8, &slice, buf).ok() || slice.size() != 8) { /*delete raf;*/ return -13; }
		uint32_t magic_lo = DecodeFixed32(slice.data());
		uint32_t magic_hi = DecodeFixed32(slice.data() + 4);
		// delete raf;
		// if(((uint64_t)magic_hi << 32) + magic_lo != kTableMagicNumber) return -14;
	}
	if(srcsize < dstsize) dstsize = 0; // overwrite
	else if(dstsize > 0) // compare file head for more security
	{
		size_t checksize = (size_t)(dstsize < FILE_BUF_SIZE ? dstsize : FILE_BUF_SIZE);
		Slice dstslice;
		char dstbuf[FILE_BUF_SIZE];
		if(!env.NewSequentialFile(srcfile, &sf, envOpt).ok() || !sf) return -15;
		if(!sf->Read(checksize, &slice, buf).ok() || slice.size() != checksize) { /*delete sf;*/ return -16; }
		/*delete sf;*/ sf = 0;
		if(!env.NewSequentialFile(dstfile, &sf, envOpt).ok() || !sf) return -17;
		if(!sf->Read(checksize, &dstslice, dstbuf).ok() || dstslice.size() != checksize) { /*delete sf;*/ return -18; }
		/*delete sf;*/ sf = 0;
		if(memcmp(slice.data(), dstslice.data(), checksize)) dstsize = 0; // overwrite
		else if(srcsize == dstsize) return 0;
	}
	if(!env.NewSequentialFile(srcfile, &sf, envOpt).ok() || !sf) return -19;
	if(dstsize > 0 && !sf->Skip(dstsize).ok()) { /*delete sf;*/ return -20; }
	FILE* fp = fopen(dstfile.c_str(), (dstsize == 0 ? "wb" : "rb+"));
	if(!fp) { /*delete sf;*/ return -21; }
	if(dstsize > 0) fseek(fp, dstsize, SEEK_SET);
	Status s; size_t size; int64_t res = 0;
	do
	{
		s = sf->Read(FILE_BUF_SIZE, &slice, buf);
		size = slice.size();
		if(size <= 0) break;
		if(fwrite(slice.data(), 1, size, fp) != size) { fclose(fp); /*delete sf;*/ return -22; }
		res += size;
	}
	while(s.ok());
	fclose(fp);
	// delete sf;
	return res;
}

// public native static long rocksdb_backup(long handle, String srcpath, String dstpath, String datetime); // return byte-size of copied data
extern "C" JNIEXPORT jlong JNICALL Java_jane_core_StorageRocksDB_rocksdb_1backup
	(JNIEnv* jenv, jclass jcls, jlong handle, jstring srcpath, jstring dstpath, jstring datetime)
{
	if(!srcpath || !dstpath || !datetime) return -1;
	Env* env = Env::Default();
	if(!env) return -2;
	const char* srcpathptr = jenv->GetStringUTFChars(srcpath, 0);
	if(!srcpathptr) return -3;
	std::string srcpathstr(srcpathptr);
	jenv->ReleaseStringUTFChars(srcpath, srcpathptr);
	const char* dstpathptr = jenv->GetStringUTFChars(dstpath, 0);
	if(!dstpathptr) return -4;
	std::string dstpathstr(dstpathptr);
	jenv->ReleaseStringUTFChars(dstpath, dstpathptr);
	const char* datetimeptr = jenv->GetStringUTFChars(datetime, 0);
	if(!datetimeptr) return -5;
	std::string datetimestr(datetimeptr);
	jenv->ReleaseStringUTFChars(datetime, datetimeptr);

	jlong n = 0;
	std::vector<std::string> files;
	DBImpl* dbi = 0;
	std::set<uint64_t>* liveset = 0;
	std::stringstream files_saved;
	env->CreateDir(dstpathstr);
	g_mutex_backup.Lock();
	if(!env->GetChildren(srcpathstr, &files).ok()) { g_mutex_backup.Unlock(); return -6; }
	if(handle)
	{
		dbi = dynamic_cast<DBImpl*>((DB*)handle);
//		VersionSet* vs = dbi->GetVersionSet(); // maybe not thread safe
//		if(vs) vs->AddLiveFiles(liveset = new std::set<uint64_t>);
//		for(std::set<uint64_t>::const_iterator it = liveset->begin(); it != liveset->end(); ++it)
//			printf("**** %06u\n", (int)*it);
	}
	for(std::vector<std::string>::const_iterator it = files.begin(), ie = files.end(); it != ie; ++it)
	{
		uint64_t num;
		FileType ft;
		int64_t r = 0;
		if(!ParseFileName(*it, &num, &ft)) continue;
		if(ft == kTableFile || ft == kLogFile || ft == kDescriptorFile)
		{
			if(ft == kTableFile && liveset && liveset->find(num) == liveset->end()) continue;
			r = AppendFile(*env, srcpathstr + '/' + *it, dstpathstr + '/' + *it, ft == kTableFile);
			if(r > 0) n += r;
			files_saved << *it;
			// if(ft == kDescriptorFile)
			{
				env->GetFileSize(dstpathstr + '/' + *it, &num);
				files_saved << ' ' << num;
			}
			files_saved << '\n';
		}
		else if(ft == kCurrentFile)
		{
			r = AppendFile(*env, srcpathstr + '/' + *it, dstpathstr + '/' + *it + '-' + datetimestr, false);
			if(r > 0) n += r;
			files_saved << *it << '-' << datetimestr << '\n';
		}
		// [optional] copy LOG file to backup dir and rename to LOG-[datetime]
		if(r < 0 && dbi) Log(dbi->GetOptions().info_log, "rocksdb_backup copy/append failed: r=%d,ft=%d,file='%s'", (int)r, (int)ft, it->c_str());
	}
	g_mutex_backup.Unlock();
	if(liveset) delete liveset;
	std::unique_ptr<WritableFile> wf = 0;
	const EnvOptions envOpt;
	if(!env->NewWritableFile(dstpathstr + '/' + "BACKUP" + '-' + datetimestr, &wf, envOpt).ok() || !wf) return -7;
	if(!wf->Append(Slice(files_saved.str())).ok()) { /*delete wf;*/ return -8; }
	if(!wf->Sync().ok()) { /*delete wf;*/ return -9; }
	wf->Close();
	/*delete wf;*/
	return n;
}

// public native static long rocksdb_iter_new(long handle, byte[] key, int keylen, int type); // type=0|1|2|3: <|<=|>=|>key
extern "C" JNIEXPORT jlong JNICALL Java_jane_core_StorageRocksDB_rocksdb_1iter_1new
	(JNIEnv* jenv, jclass jcls, jlong handle, jbyteArray key, jint keylen, jint type)
{
	DB* db = (DB*)handle;
	if(!db || type < 0 || type > 3) return 0;
	Iterator* it = db->NewIterator(g_ro_nocached);
	if(it)
	{
		if(!key || keylen <= 0)
		{
			if(type >= 2)
				it->SeekToFirst();
			else
				it->SeekToLast();
		}
		else
		{
			jbyte* keyptr = jenv->GetByteArrayElements(key, 0);
			if(!keyptr) { delete it; return 0; }
			Slice slice((const char*)keyptr, (size_t)keylen);
			it->Seek(slice);
			if(it->Valid())
			{
				int comp = it->key().compare(slice);
				if(comp == 0)
				{
					if(type == 0) it->Prev();
					else if(type == 3) it->Next();
				}
				else // must be comp > 0
					if(type <= 1) it->Prev();
			}
			else
			{
				if(type >= 2)
					it->SeekToFirst();
				else
					it->SeekToLast();
			}
			jenv->ReleaseByteArrayElements(key, keyptr, JNI_ABORT);
		}
	}
	return (jlong)it;
}

// public native static void rocksdb_iter_delete(long iter);
extern "C" JNIEXPORT void JNICALL Java_jane_core_StorageRocksDB_rocksdb_1iter_1delete
	(JNIEnv* jenv, jclass jcls, jlong iter)
{
	delete (Iterator*)iter;
}

// public native static byte[] rocksdb_iter_next(long iter); // return cur-key (maybe null) and do next
extern "C" JNIEXPORT jbyteArray JNICALL Java_jane_core_StorageRocksDB_rocksdb_1iter_1next
	(JNIEnv* jenv, jclass jcls, jlong iter)
{
	Iterator* it = (Iterator*)iter;
	if(!it || !it->Valid()) return 0;
	const Slice& slice = it->key();
	jbyteArray key = jenv->NewByteArray(slice.size());
	jenv->SetByteArrayRegion(key, 0, slice.size(), (const jbyte*)slice.data());
	it->Next();
	return key;
}

// public native static byte[] rocksdb_iter_prev(long iter); // return cur-key (maybe null) and do prev
extern "C" JNIEXPORT jbyteArray JNICALL Java_jane_core_StorageRocksDB_rocksdb_1iter_1prev
	(JNIEnv* jenv, jclass jcls, jlong iter)
{
	Iterator* it = (Iterator*)iter;
	if(!it || !it->Valid()) return 0;
	const Slice& slice = it->key();
	jbyteArray key = jenv->NewByteArray(slice.size());
	jenv->SetByteArrayRegion(key, 0, slice.size(), (const jbyte*)slice.data());
	it->Prev();
	return key;
}

// public native static byte[] rocksdb_iter_value(long iter); // return cur-value(maybe null)
extern "C" JNIEXPORT jbyteArray JNICALL Java_jane_core_StorageRocksDB_rocksdb_1iter_1value
	(JNIEnv* jenv, jclass jcls, jlong iter)
{
	Iterator* it = (Iterator*)iter;
	if(!it || !it->Valid()) return 0;
	const Slice& slice = it->value();
	jbyteArray key = jenv->NewByteArray(slice.size());
	jenv->SetByteArrayRegion(key, 0, slice.size(), (const jbyte*)slice.data());
	return key;
}

// public native static boolean rocksdb_compact(long handle, byte[] key_from, int key_from_len, byte[] key_to, int key_to_len);
extern "C" JNIEXPORT jboolean JNICALL Java_jane_core_StorageRocksDB_rocksdb_1compact
	(JNIEnv* jenv, jclass jcls, jlong handle, jbyteArray key_from, jint key_from_len, jbyteArray key_to, jint key_to_len)
{
	DB* db = (DB*)handle;
	if(!db) return JNI_FALSE;
	std::string fromstr, tostr;
	Slice from, to;
	if(key_from)
	{
		jsize m = jenv->GetArrayLength(key_from);
		if(key_from_len > m) key_from_len = m;
		if(key_from_len > 0)
		{
			jbyte* keyptr = jenv->GetByteArrayElements(key_from, 0);
			if(keyptr)
			{
				fromstr.assign((const char*)keyptr, key_from_len);
				from = Slice(fromstr);
				jenv->ReleaseByteArrayElements(key_from, keyptr, JNI_ABORT);
			}
		}
	}
	if(key_to)
	{
		jsize m = jenv->GetArrayLength(key_to);
		if(key_to_len > m) key_to_len = m;
		if(key_to_len > 0)
		{
			jbyte* keyptr = jenv->GetByteArrayElements(key_to, 0);
			if(keyptr)
			{
				tostr.assign((const char*)keyptr, key_to_len);
				from = Slice(tostr);
				jenv->ReleaseByteArrayElements(key_to, keyptr, JNI_ABORT);
			}
		}
	}
	db->CompactRange((from.size() > 0 ? &from : 0), (to.size() > 0 ? &to : 0));
	return JNI_TRUE;
}
