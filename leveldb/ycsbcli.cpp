#include <cassert>
#include <chrono>
#include <cstring>
#include <cmath>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <random>
#include <chrono>

#include "cxxopts.hpp"

#include "leveldb/db.h"
#include "leveldb/iterator.h"
#include "leveldb/slice.h"


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
             size_t filesize_limit)
{
    leveldb::DB *db;
    db_options.create_if_missing = true;

    // Use unusually small mem buffer size and max file size.
    // Must comment out `ClipToRange()` sanitizers @ db_impl.cc:102.
    db_options.write_buffer_size = memtable_limit;
    db_options.max_file_size = filesize_limit;

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
        const std::string value, double& microsecs)
{
    leveldb::Status status;
    uint cnt = 0;
    std::string read_buf;
    read_buf.reserve(value.length());

    // Prepare for timing.
    auto time_start = std::chrono::high_resolution_clock::now();
    microsecs = 0;

    for (auto& req : reqs) {
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

        assert(status.ok());
        cnt++;
    }

    // Calculate time elapsed.
    auto time_end = std::chrono::high_resolution_clock::now();
    microsecs = std::chrono::duration<double, std::milli>(
        time_end - time_start).count();

    return cnt;
}


int
main(int argc, char *argv[])
{
    std::string db_location, ycsb_filename;
    size_t value_size, memtable_limit, filesize_limit;

    cxxopts::Options cmd_args("leveldb ycsb trace exec client");
    cmd_args.add_options()
            ("h,help", "print help message", cxxopts::value<bool>()->default_value("false"))
            ("d,directory", "directory of db", cxxopts::value<std::string>(db_location)->default_value("./dbdir"))
            ("v,value_size", "size of value", cxxopts::value<size_t>(value_size)->default_value("64"))
            ("f,ycsb", "YCSB trace filename", cxxopts::value<std::string>(ycsb_filename)->default_value(""))
            ("s,sync", "force write sync", cxxopts::value<bool>()->default_value("false"))
            ("mlim", "memtable size limit", cxxopts::value<size_t>(memtable_limit)->default_value("4194304"))
            ("flim", "sstable filesize limit", cxxopts::value<size_t>(filesize_limit)->default_value("2097152"));
    auto result = cmd_args.parse(argc, argv);

    if (result.count("help")) {
        printf("%s", cmd_args.help().c_str());
        exit(0);
    }

    if (result.count("sync"))
        write_options.sync = true;

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

    leveldb::DB *db = leveldb_open(db_location, memtable_limit, filesize_limit);

    // Execute the actions of the YCSB trace.
    double microsecs;
    uint cnt = do_ycsb(db, ycsb_reqs, value, microsecs);
    std::cout << "Finished " << cnt << " requests." << std::endl;
    if (microsecs > 0)
        std::cout << "Time elapsed: " << microsecs << " us" << std::endl;

    // Force compaction of everything in memory.
    // leveldb_compact(db);

    leveldb_stats(db);
    leveldb_close(db);

    return 0;
}
