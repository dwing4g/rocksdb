@echo off
setlocal
pushd %~dp0

rem install mingw-gcc 4.8+ and append PATH with mingw/bin
rem install jdk6+ and set JAVA_HOME
if "%JAVA_HOME%" equ "" set JAVA_HOME=C:/Program Files/Java/jdk1.7.0_80

set CORE_FILES= ^
    db/builder.cc ^
    db/c.cc ^
    db/column_family.cc ^
    db/compacted_db_impl.cc ^
    db/compaction.cc ^
    db/compaction_job.cc ^
    db/compaction_picker.cc ^
    db/convenience.cc ^
    db/dbformat.cc ^
    db/db_filesnapshot.cc ^
    db/db_impl.cc ^
    db/db_impl_debug.cc ^
    db/db_impl_experimental.cc ^
    db/db_impl_readonly.cc ^
    db/db_iter.cc ^
    db/event_helpers.cc ^
    db/experimental.cc ^
    db/filename.cc ^
    db/file_indexer.cc ^
    db/flush_job.cc ^
    db/flush_scheduler.cc ^
    db/forward_iterator.cc ^
    db/internal_stats.cc ^
    db/log_reader.cc ^
    db/log_writer.cc ^
    db/managed_iterator.cc ^
    db/memtable.cc ^
    db/memtable_allocator.cc ^
    db/memtable_list.cc ^
    db/merge_helper.cc ^
    db/merge_operator.cc ^
    db/repair.cc ^
    db/slice.cc ^
    db/snapshot_impl.cc ^
    db/table_cache.cc ^
    db/table_properties_collector.cc ^
    db/transaction_log_impl.cc ^
    db/version_builder.cc ^
    db/version_edit.cc ^
    db/version_set.cc ^
    db/wal_manager.cc ^
    db/write_batch.cc ^
    db/write_batch_base.cc ^
    db/write_controller.cc ^
    db/write_thread.cc ^
    port/stack_trace.cc ^
    port/win/env_win.cc ^
    port/win/port_win.cc ^
    port/win/win_logger.cc ^
    table/adaptive_table_factory.cc ^
    table/block.cc ^
    table/block_based_filter_block.cc ^
    table/block_based_table_builder.cc ^
    table/block_based_table_factory.cc ^
    table/block_based_table_reader.cc ^
    table/block_builder.cc ^
    table/block_hash_index.cc ^
    table/block_prefix_index.cc ^
    table/bloom_block.cc ^
    table/cuckoo_table_builder.cc ^
    table/cuckoo_table_factory.cc ^
    table/cuckoo_table_reader.cc ^
    table/flush_block_policy.cc ^
    table/format.cc ^
    table/full_filter_block.cc ^
    table/get_context.cc ^
    table/iterator.cc ^
    table/merger.cc ^
    table/meta_blocks.cc ^
    table/mock_table.cc ^
    table/plain_table_builder.cc ^
    table/plain_table_factory.cc ^
    table/plain_table_index.cc ^
    table/plain_table_key_coding.cc ^
    table/plain_table_reader.cc ^
    table/table_properties.cc ^
    table/two_level_iterator.cc ^
    util/arena.cc ^
    util/auto_roll_logger.cc ^
    util/bloom.cc ^
    util/build_version.cc ^
    util/cache.cc ^
    util/coding.cc ^
    util/compaction_job_stats_impl.cc ^
    util/comparator.cc ^
    util/crc32c.cc ^
    util/db_info_dumper.cc ^
    util/delete_scheduler_impl.cc ^
    util/db_test_util.cc ^
    util/dynamic_bloom.cc ^
    util/env.cc ^
    util/env_hdfs.cc ^
    util/event_logger.cc ^
    util/file_util.cc ^
    util/file_reader_writer.cc ^
    util/filter_policy.cc ^
    util/hash.cc ^
    util/hash_cuckoo_rep.cc ^
    util/hash_linklist_rep.cc ^
    util/hash_skiplist_rep.cc ^
    util/histogram.cc ^
    util/instrumented_mutex.cc ^
    util/iostats_context.cc ^
    util/ldb_cmd.cc ^
    util/ldb_tool.cc ^
    util/logging.cc ^
    util/log_buffer.cc ^
    util/memenv.cc ^
    util/mock_env.cc ^
    util/murmurhash.cc ^
    util/mutable_cf_options.cc ^
    util/options.cc ^
    util/options_builder.cc ^
    util/options_helper.cc ^
    util/perf_context.cc ^
    util/perf_level.cc ^
    util/rate_limiter.cc ^
    util/skiplistrep.cc ^
    util/slice.cc ^
    util/sst_dump_tool.cc ^
    util/statistics.cc ^
    util/status.cc ^
    util/status_message.cc ^
    util/string_util.cc ^
    util/sync_point.cc ^
    util/testharness.cc ^
    util/testutil.cc ^
    util/thread_local.cc ^
    util/thread_status_impl.cc ^
    util/thread_status_updater.cc ^
    util/thread_status_updater_debug.cc ^
    util/thread_status_util.cc ^
    util/thread_status_util_debug.cc ^
    util/vectorrep.cc ^
    util/xfunc.cc ^
    util/xxhash.cc ^
    utilities/backupable/backupable_db.cc ^
    utilities/checkpoint/checkpoint.cc ^
    utilities/document/document_db.cc ^
    utilities/document/json_document.cc ^
    utilities/document/json_document_builder.cc ^
    utilities/flashcache/flashcache.cc ^
    utilities/geodb/geodb_impl.cc ^
    utilities/leveldb_options/leveldb_options.cc ^
    utilities/merge_operators/string_append/stringappend.cc ^
    utilities/merge_operators/string_append/stringappend2.cc ^
    utilities/merge_operators/put.cc ^
    utilities/merge_operators/uint64add.cc ^
    utilities/redis/redis_lists.cc ^
    utilities/spatialdb/spatial_db.cc ^
    utilities/table_properties_collectors/compact_on_deletion_collector.cc ^
    utilities/transactions/optimistic_transaction_impl.cc ^
    utilities/transactions/optimistic_transaction_db_impl.cc ^
    utilities/transactions/transaction_base.cc ^
    utilities/transactions/transaction_impl.cc ^
    utilities/transactions/transaction_db_impl.cc ^
    utilities/transactions/transaction_lock_mgr.cc ^
    utilities/transactions/transaction_util.cc ^
    utilities/ttl/db_ttl_impl.cc ^
    utilities/write_batch_with_index/write_batch_with_index.cc ^
    utilities/write_batch_with_index/write_batch_with_index_internal.cc ^
    lz4/lz4.c

set TEST_FILES=util/testutil.cc util/testharness.cc

set COMPILE=-DOS_WIN -DLZ4 -DENABLE_JNI -D__USE_MINGW_ANSI_STDIO=1 -DNDEBUG -D_GLIBCXX_USE_C99_STDINT_TR1 -D_GLIBCXX_BEGIN_NAMESPACE_VERSION -D_GLIBCXX_HAS_GTHREADS -I. -Iinclude -Ilz4 -Iport -I"%JAVA_HOME%/include" -I"%JAVA_HOME%/include/win64" -I"%JAVA_HOME%/include/win32" -Ofast -ffast-math -fweb -fomit-frame-pointer -fmerge-all-constants -fno-builtin-memcmp -pipe -pthread -static -lpthread

set COMPILE32=i686-w64-mingw32-g++.exe -std=c++11 -m32 -march=i686 -flto -fwhole-program %COMPILE%
set COMPILE64=x86_64-w64-mingw32-g++.exe -std=c++11 -m64 %COMPILE%

echo building rocksdbjni32.dll ...
%COMPILE32% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,rocksdbjni32.dll -o rocksdbjni32.dll %CORE_FILES%

echo building rocksdbjni64.dll ...
rem %COMPILE64% -shared -Wl,--image-base,0x10000000 -Wl,--kill-at -Wl,-soname -Wl,rocksdbjni64.dll -o rocksdbjni64.dll %CORE_FILES%

echo building db-tools ...
rem %COMPILE32% -o db_bench32.exe     db/db_bench.cc     %CORE_FILES% %TEST_FILES%
rem %COMPILE64% -o db_bench64.exe     db/db_bench.cc     %CORE_FILES% %TEST_FILES%
rem %COMPILE32% -o db_test32.exe      db/db_test.cc      %CORE_FILES% %TEST_FILES%
rem %COMPILE64% -o db_test64.exe      db/db_test.cc      %CORE_FILES% %TEST_FILES%
rem %COMPILE32% -o rocksdb_main32.exe db/rocksdb_main.cc %CORE_FILES% %TEST_FILES%
rem %COMPILE64% -o rocksdb_main64.exe db/rocksdb_main.cc %CORE_FILES% %TEST_FILES%

pause
