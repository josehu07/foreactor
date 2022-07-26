#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <limits>
#include <algorithm>
#include <thread>
#include <future>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "cxxopts.hpp"

#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/slice.h"


static void cmd_drop_caches(void) {
    int rc __attribute__((unused)) =
        system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
    assert(rc == 0);
}


typedef enum {
    READ,
    INSERT,
    UPDATE,
    SCAN,
    UNKNOWN
} ycsb_op_t;

typedef struct {
    ycsb_op_t op;
    std::string key;
    size_t scan_len;    // only valid for scan
} ycsb_req_t;


static leveldb::Options db_options = leveldb::Options();
static leveldb::ReadOptions read_options = leveldb::ReadOptions();
static leveldb::WriteOptions write_options = leveldb::WriteOptions();


/** Open/close leveldb instance, show stats, etc. */
static leveldb::DB *
leveldb_open(const std::string db_location, size_t memtable_limit,
             size_t filesize_limit, bool bg_compact_off)
{
    leveldb::DB *db;
    leveldb::Status status = leveldb::DB::Open(db_options, db_location, &db);
    if (!status.ok()) {
        std::cerr << status.ToString() << std::endl;
        exit(1);
    }
    return db;
}

static void
leveldb_close(leveldb::DB *db)
{
    delete db;
}

static void __attribute__((unused))
leveldb_compact(leveldb::DB *db)
{
    // Compact all keys.
    db->CompactRange(nullptr, nullptr);
}

static void
leveldb_stats(leveldb::DB *db)
{
    std::string result;
    bool valid = true;
    uint level = 0;

    std::cout << std::endl << "Num files at level --" << std::endl;
    do {
        std::string s = "leveldb.num-files-at-level";
        s += std::to_string(level);
        leveldb::Slice property = leveldb::Slice(s);

        valid = db->GetProperty(property, &result);
        if (valid)
            std::cout << "  " << level << ": " << result << std::endl;

        level++;
    } while (valid);

    std::cout << std::endl << "Stats --" << std::endl;
    valid = db->GetProperty(leveldb::Slice("leveldb.stats"), &result);
    if (valid)
        std::cout << result << std::endl;
    else
        std::cout << "  failed to get property" << std::endl;

    std::cout << "SSTables --" << std::endl;
    valid = db->GetProperty(leveldb::Slice("leveldb.sstables"), &result);
    if (valid)
        std::cout << result << std::endl;
    else
        std::cout << "  failed to get property" << std::endl;
}


/** Run/load YCSB workload on a leveldb instance. */
static uint
do_ycsb(leveldb::DB *db, const std::vector<ycsb_req_t> *reqs,
        const std::string value, bool drop_caches, bool print_block_info,
        std::vector<double>& microsecs)
{
    leveldb::Status status;
    uint cnt = 0;
    std::string read_buf;
    read_buf.reserve(value.length());

    for (auto& req : *reqs) {
        auto time_start = std::chrono::high_resolution_clock::now();
        switch (req.op) {
            case INSERT:
            case UPDATE: {
                    status = db->Put(write_options, req.key, value);
                }
                break;

            case READ: {
                    if (print_block_info)
                        std::cout << "req " << req.key << std::endl;
                    status = db->Get(read_options, req.key, &read_buf);
                }
                break;

            case SCAN: {
                    auto iter = db->NewIterator(read_options);
                    iter->Seek(req.key);
                    status = iter->status();
                    for (size_t i = 0; i < req.scan_len; ++i) {
                        if (iter->Valid()) {
                            read_buf = iter->value().ToString();
                            iter->Next();
                        } else
                            break;
                    }
                }
                break;

            case UNKNOWN:
            default: {
                    std::cerr << "Error: unrecognized opcode" << std::endl;
                    exit(1);
                }
        }
        auto time_end = std::chrono::high_resolution_clock::now();

        // Record all successful and failed requests.
        cnt++;
        microsecs.push_back(std::chrono::duration<double, std::micro>(
            time_end - time_start).count());

        if (!status.ok()) {
            std::cerr << "Error: req returned status: " << status.ToString()
                      << std::endl;
        }

        if (drop_caches)
            cmd_drop_caches();
    }

    return cnt;
}

/** Worker thread func for multithreaded exper. */
static void
worker_thread_func(leveldb::DB *db, const std::vector<ycsb_req_t> *reqs,
                   const std::string value, bool drop_caches,
                   bool print_block_info, size_t id,
                   std::vector<double> *ops_per_sec,
                   std::promise<void> init_promise,
                   std::shared_future<void> start_future) {
    // signal main thread for startup complete
    init_promise.set_value();

    // wait on signal to start the work
    start_future.wait();

    // call do_ycsb()
    std::vector<double> microsecs;
    uint cnt = do_ycsb(db, reqs, value, drop_caches, print_block_info,
                       microsecs);
    
    // calculate my throughput in ops/sec
    double sum_us = 0.;
    for (auto& us : microsecs)
        sum_us += us;
    double ops = static_cast<double>(cnt) * 1e6 / sum_us;
    ops_per_sec->at(id) = ops;
}

/** Main coordinator thread if in multithreaded exper. */
static std::vector<double>
do_ycsb_multithreaded(leveldb::DB *db, const std::vector<ycsb_req_t> *reqs,
                      const std::string value, bool drop_caches,
                      bool print_block_info, size_t num_threads) {
    // take a read-only snapshot shared across threads
    read_options.snapshot = db->GetSnapshot();

    // spawn the desired number of worker threads, each applying the whole
    // trace once
    std::vector<double> ops_per_sec;
    std::vector<std::thread> workers;
    std::vector<std::future<void>> init_futures;
    std::promise<void> start_promise;
    std::shared_future<void> start_future(start_promise.get_future());
    for (size_t id = 0; id < num_threads; ++id) {
        std::promise<void> init_promise;
        init_futures.push_back(init_promise.get_future());
        ops_per_sec.push_back(0.);
        workers.emplace_back(worker_thread_func, db, reqs, value, drop_caches,
                             print_block_info, id, &ops_per_sec,
                             std::move(init_promise), start_future);
    }

    // wait for everyone to finish starting up
    for (auto&& init_future : init_futures)
        init_future.wait();

    // signal everyone to start the work
    start_promise.set_value();

    // wait for all workers to complete their job
    for (auto&& worker : workers)
        worker.join();

    db->ReleaseSnapshot(read_options.snapshot);
    read_options.snapshot = nullptr;

    return ops_per_sec;
}


int
main(int argc, char *argv[])
{
    std::string db_location, ycsb_filename;
    size_t value_size, memtable_limit, filesize_limit, num_threads;
    bool help, write_sync, bg_compact_off, no_fill_cache, drop_caches, print_block_info, print_stat_exit, wait_before_close;

    cxxopts::Options cmd_args("leveldb ycsb trace exec client");
    cmd_args.add_options()
            ("h,help", "print help message", cxxopts::value<bool>(help)->default_value("false"))
            ("d,directory", "directory of db", cxxopts::value<std::string>(db_location)->default_value("./dbdir"))
            ("v,value_size", "size of value", cxxopts::value<size_t>(value_size)->default_value("64"))
            ("f,ycsb", "YCSB trace filename", cxxopts::value<std::string>(ycsb_filename)->default_value(""))
            ("t,multithread", "if > 1, do multithreading on read-only snapshot", cxxopts::value<size_t>(num_threads)->default_value("1"))
            ("mlim", "memtable size limit", cxxopts::value<size_t>(memtable_limit)->default_value("4194304"))
            ("flim", "sstable filesize limit", cxxopts::value<size_t>(filesize_limit)->default_value("2097152"))
            ("write_sync", "force write sync", cxxopts::value<bool>(write_sync)->default_value("false"))
            ("bg_compact_off", "turn off background compaction", cxxopts::value<bool>(bg_compact_off)->default_value("false"))
            ("no_fill_cache", "no block cache for gets", cxxopts::value<bool>(no_fill_cache)->default_value("false"))
            ("drop_caches", "do drop_caches between ops", cxxopts::value<bool>(drop_caches)->default_value("false"))
            ("print_block_info", "for distribution accounting", cxxopts::value<bool>(print_block_info)->default_value("false"))
            ("print_stat_exit", "only print stats and exit", cxxopts::value<bool>(print_stat_exit)->default_value("false"))
            ("wait_before_close", "ensure memtable compacted", cxxopts::value<bool>(wait_before_close)->default_value("false"));
    auto result = cmd_args.parse(argc, argv);

    if (help) {
        printf("%s", cmd_args.help().c_str());
        exit(0);
    }

    // Use unusually small mem buffer size and max file size.
    // Must comment out `ClipToRange()` sanitizers @ db_impl.cc:102.
    db_options.write_buffer_size = memtable_limit;
    db_options.max_file_size = filesize_limit;
    // When benchmarking Gets we want to turn off the automatic bg thread.
    db_options.bg_compact_off = bg_compact_off;
    db_options.create_if_missing = true;

    write_options.sync = write_sync;
    read_options.fill_cache = !no_fill_cache;
    read_options.print_block_info = print_block_info;

    if (num_threads == 0) {
        std::cerr << "Error: number of threads given is zero" << std::endl;
        exit(1);
    }

    // Read in YCSB workload trace.
    std::vector<ycsb_req_t> ycsb_reqs;
    if (!print_stat_exit) {
        if (!ycsb_filename.empty()) {
            std::ifstream input(ycsb_filename);
            std::string opcode;
            std::string key;
            while (input >> opcode >> key) {
                if (num_threads > 1 && opcode != "READ" && opcode != "SCAN") {
                    std::cerr << "Error: can only do read-only ops when"
                              << " multithreaded" << std::endl;
                    exit(1);
                }
                ycsb_op_t op = opcode == "READ"   ? READ
                             : opcode == "INSERT" ? INSERT
                             : opcode == "UPDATE" ? UPDATE
                             : opcode == "SCAN"   ? SCAN : UNKNOWN;
                size_t scan_len = 0;
                if (op == SCAN)
                    input >> scan_len;
                ycsb_reqs.push_back(ycsb_req_t { .op=op, .key=key, .scan_len=scan_len });
            }
        } else {
            std::cerr << "Error: must give YCSB trace filename" << std::endl;
            printf("%s", cmd_args.help().c_str());
            exit(1);
        }
        if (ycsb_reqs.size() == 0) {
            std::cerr << "Error: given YCSB trace file has no valid lines" << std::endl;
            exit(1);
        }
    }

    // Generate value.
    std::string value(value_size, '0');

    // Open database instance.
    leveldb::DB *db = leveldb_open(db_location, memtable_limit, filesize_limit,
                                   bg_compact_off);

    if (print_stat_exit) {
        leveldb_stats(db);
        leveldb_close(db);
        return 0;
    }

    // Execute the actions of the YCSB trace.
    if (num_threads == 1) {
        // no multithread flag, apply trace directly on open DB object
        std::vector<double> microsecs;
        uint cnt = do_ycsb(db, &ycsb_reqs, value, drop_caches, print_block_info, microsecs);
        std::cout << "Finished " << cnt << " requests." << std::endl << std::endl;

        // print timing info
        if (microsecs.size() > 0) {
            std::sort(microsecs.begin(), microsecs.end());

            std::cout << "Sorted time elapsed:";
            for (double& us : microsecs)
                std::cout << " " << us;
            std::cout << std::endl << std::endl;

            double sum_us = 0.;
            for (double& us : microsecs)
                sum_us += us;
            double min_us = microsecs.front();
            double max_us = microsecs.back();
            double avg_us = sum_us / microsecs.size();

            std::cout << "Time elapsed stats:" << std::endl
                      << "  sum  " << sum_us << " us" << std::endl
                      << "  avg  " << avg_us << " us" << std::endl
                      << "  max  " << max_us << " us" << std::endl
                      << "  min  " << min_us << " us" << std::endl << std::endl;

            if (microsecs.size() > 1) {
                std::cout << "Removing top-1 outlier:" << std::endl;
                microsecs.erase(microsecs.end() - 1, microsecs.end());

                sum_us = 0.;
                for (double& us : microsecs)
                    sum_us += us;
                min_us = microsecs.front();
                max_us = microsecs.back();
                avg_us = sum_us / microsecs.size();
                size_t p999_idx = microsecs.size() * 999 / 1000;
                if (p999_idx == microsecs.size() - 1)
                    p999_idx -= 1;
                double p999_us = microsecs.at(p999_idx);

                std::cout << "  sum  "  << sum_us  << " us" << std::endl
                          << "  avg  "  << avg_us  << " us" << std::endl
                          << "  p999  " << p999_us << " us" << std::endl
                          << "  max  "  << max_us  << " us" << std::endl
                          << "  min  "  << min_us  << " us" << std::endl;
            }
        }

    } else {
        // multithreaded case, make a read-only snapshot and spawn child threads
        // to execute the trace on the snapshot
        assert(num_threads > 1);
        std::vector<double> ops_per_sec = do_ycsb_multithreaded(db, &ycsb_reqs, value,
                                                                drop_caches,
                                                                print_block_info,
                                                                num_threads);
        assert(ops_per_sec.size() == num_threads);
        std::cout << "Finished multithreaded." << std::endl << std::endl;

        // print the overall elapsed microseconds
        std::sort(ops_per_sec.begin(), ops_per_sec.end());

        double sum_ops_per_sec = 0.;
        for (double& ops : ops_per_sec)
            sum_ops_per_sec += ops;
        double min_ops_per_sec = ops_per_sec.front();
        double max_ops_per_sec = ops_per_sec.back();
        double avg_ops_per_sec = sum_ops_per_sec / num_threads;

        std::cout << "Throughput stats:" << std::endl
                  << "  sum  " << sum_ops_per_sec << " ops/sec" << std::endl
                  << "  avg  " << avg_ops_per_sec << " ops/sec" << std::endl
                  << "  max  " << max_ops_per_sec << " ops/sec" << std::endl
                  << "  min  " << min_ops_per_sec << " ops/sec" << std::endl << std::endl;
    }

    // Force compaction of everything in memory.
    // leveldb_compact(db);
    if (wait_before_close)
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    leveldb_stats(db);
    leveldb_close(db);

    return 0;
}
