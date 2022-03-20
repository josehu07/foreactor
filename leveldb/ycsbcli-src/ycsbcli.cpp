#include <chrono>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>
#include <limits>
#include <algorithm>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "cxxopts.hpp"

#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/slice.h"


static void drop_caches(void) {
    system("sudo sync; sudo sh -c 'echo 3 > /proc/sys/vm/drop_caches'");
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
    db_options.create_if_missing = true;

    // Use unusually small mem buffer size and max file size.
    // Must comment out `ClipToRange()` sanitizers @ db_impl.cc:102.
    db_options.write_buffer_size = memtable_limit;
    db_options.max_file_size = filesize_limit;

    // When benchmarking Gets we want to turn off the automatic bg thread.
    db_options.bg_compact_off = bg_compact_off;

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
do_ycsb(leveldb::DB *db, const std::vector<ycsb_req_t>& reqs,
        const std::string value, bool do_drop_caches,
        std::vector<double>& microsecs)
{
    leveldb::Status status;
    uint cnt = 0;
    std::string read_buf;
    read_buf.reserve(value.length());

    for (auto& req : reqs) {
        auto time_start = std::chrono::high_resolution_clock::now();
        switch (req.op) {
            case INSERT:
            case UPDATE: {
                    status = db->Put(write_options, req.key, value);
                }
                break;

            case READ: {
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

        if (do_drop_caches)
            drop_caches();
    }

    return cnt;
}


int
main(int argc, char *argv[])
{
    std::string db_location, ycsb_filename;
    size_t value_size, memtable_limit, filesize_limit;
    bool do_drop_caches = false, bg_compact_off = false;

    cxxopts::Options cmd_args("leveldb ycsb trace exec client");
    cmd_args.add_options()
            ("h,help", "print help message", cxxopts::value<bool>()->default_value("false"))
            ("d,directory", "directory of db", cxxopts::value<std::string>(db_location)->default_value("./dbdir"))
            ("v,value_size", "size of value", cxxopts::value<size_t>(value_size)->default_value("64"))
            ("f,ycsb", "YCSB trace filename", cxxopts::value<std::string>(ycsb_filename)->default_value(""))
            ("mlim", "memtable size limit", cxxopts::value<size_t>(memtable_limit)->default_value("4194304"))
            ("flim", "sstable filesize limit", cxxopts::value<size_t>(filesize_limit)->default_value("2097152"))
            ("write_sync", "force write sync", cxxopts::value<bool>()->default_value("false"))
            ("bg_compact_off", "turn off background compaction", cxxopts::value<bool>()->default_value("false"))
            ("no_fill_cache", "no block cache for gets", cxxopts::value<bool>()->default_value("false"))
            ("drop_caches", "do drop_caches between ops", cxxopts::value<bool>()->default_value("false"));
    auto result = cmd_args.parse(argc, argv);

    if (result.count("help")) {
        printf("%s", cmd_args.help().c_str());
        exit(0);
    }

    if (result.count("sync"))
        write_options.sync = true;

    if (result.count("bg_compact_off"))
        bg_compact_off = true;

    if (result.count("no_fill_cache"))
        read_options.fill_cache = false;

    if (result.count("drop_caches"))
        do_drop_caches = true;

    // Read in YCSB workload trace.
    std::vector<ycsb_req_t> ycsb_reqs;
    if (!ycsb_filename.empty()) {
        std::ifstream input(ycsb_filename);
        std::string opcode;
        std::string key;
        while (input >> opcode >> key) {
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
        std::cerr << "Error: given YCSB trace file has not valid lines" << std::endl;
        exit(1);
    }

    // Generate value.
    std::string value(value_size, '0');

    leveldb::DB *db = leveldb_open(db_location, memtable_limit, filesize_limit,
                                   bg_compact_off);

    // Execute the actions of the YCSB trace.
    std::vector<double> microsecs;
    uint cnt = do_ycsb(db, ycsb_reqs, value, do_drop_caches, microsecs);
    std::cout << "Finished " << cnt << " requests." << std::endl << std::endl;

    // Print timing info.
    if (cnt > 0) {
        assert(microsecs.size() == cnt);
        std::sort(microsecs.begin(), microsecs.end());

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
                  << "  min  " << min_us << " us" << std::endl;

        if (microsecs.size() > 10) {
            std::cout << " removing top/bottom-5:" << std::endl;
            microsecs.erase(microsecs.begin(), microsecs.begin() + 5);
            microsecs.erase(microsecs.end() - 5, microsecs.end());

            sum_us = 0.;
            for (double& us : microsecs)
                sum_us += us;
            min_us = microsecs.front();
            max_us = microsecs.back();
            avg_us = sum_us / microsecs.size();

            std::cout << "  sum  " << sum_us << " us" << std::endl
                      << "  avg  " << avg_us << " us" << std::endl
                      << "  max  " << max_us << " us" << std::endl
                      << "  min  " << min_us << " us" << std::endl;
        }
    }

    // Force compaction of everything in memory.
    // leveldb_compact(db);

    leveldb_stats(db);
    leveldb_close(db);

    return 0;
}
