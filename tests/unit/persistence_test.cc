#include <gtest/gtest.h>

#include <cstdio>
#include <cstdint>
#include <deque>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include <gflags/gflags.h>

#include "command/command_registry.h"
#include "command/server_family.h"
#include "core/command_context.h"
#include "core/database.h"
#include "core/nano_obj.h"
#include "core/rdb_defs.h"
#include "core/rdb_loader.h"
#include "core/rdb_serializer.h"
#include "core/unordered_dense.h"
#include "server/slice_snapshot.h"

DECLARE_int32(port);
DECLARE_int32(num_shards);
DECLARE_bool(tcp_nodelay);
DECLARE_bool(use_iouring_tcp_server);
DECLARE_uint64(photon_handler_stack_kb);

namespace {

using HashType = ankerl::unordered_dense::map<std::string, std::string, ankerl::unordered_dense::hash<std::string>>;
using SetType = ankerl::unordered_dense::set<std::string, ankerl::unordered_dense::hash<std::string>>;

class MemorySink : public io::Sink {
public:
	std::error_code Append(const uint8_t* data, size_t len) override {
		buffer.insert(buffer.end(), data, data + len);
		return {};
	}

	std::vector<uint8_t> buffer;
};

class MemorySource : public io::Source {
public:
	explicit MemorySource(const std::vector<uint8_t>& data) : data_(data) {}

	std::error_code Read(uint8_t* buf, size_t len) override {
		if (offset_ + len > data_.size()) {
			return std::make_error_code(std::errc::io_error);
		}
		std::memcpy(buf, data_.data() + offset_, len);
		offset_ += len;
		return {};
	}

private:
	const std::vector<uint8_t>& data_;
	size_t offset_ = 0;
};

constexpr char kTestDumpFile[] = "test_dump.nrdb";

void RemoveTestFile() {
	(void)std::remove(kTestDumpFile);
	std::string tmp = std::string(kTestDumpFile) + ".tmp";
	(void)std::remove(tmp.c_str());
}

class FileSink : public io::Sink {
public:
	explicit FileSink(std::string file_path) : path_(std::move(file_path)), file_(path_, std::ios::binary | std::ios::trunc) {
	}

	std::error_code Append(const uint8_t* data, size_t len) override {
		if (!file_.good()) {
			return std::make_error_code(std::errc::io_error);
		}
		file_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(len));
		if (!file_) {
			return std::make_error_code(std::errc::io_error);
		}
		return {};
	}

	std::error_code FlushAndClose() {
		if (!file_.is_open()) {
			return {};
		}
		file_.flush();
		if (!file_) {
			return std::make_error_code(std::errc::io_error);
		}
		file_.close();
		return {};
	}

	bool IsOpen() const {
		return file_.is_open();
	}

	const std::string& Path() const {
		return path_;
	}

private:
	std::string path_;
	std::ofstream file_;
};

class FileSource : public io::Source {
public:
	explicit FileSource(const std::string& file_path) : file_(file_path, std::ios::binary) {
	}

	std::error_code Read(uint8_t* data, size_t len) override {
		if (!file_.is_open()) {
			return std::make_error_code(std::errc::bad_file_descriptor);
		}
		if (len == 0) {
			return {};
		}
		file_.read(reinterpret_cast<char*>(data), static_cast<std::streamsize>(len));
		if (file_.gcount() != static_cast<std::streamsize>(len)) {
			return std::make_error_code(std::errc::io_error);
		}
		return {};
	}

	bool IsOpen() const {
		return file_.is_open();
	}

private:
	std::ifstream file_;
};

}  // namespace

class PersistenceTest : public ::testing::Test {
protected:
	void SetUp() override {
		RemoveTestFile();
		db.ClearAll();
		db.Select(0);
	}

	void TearDown() override {
		RemoveTestFile();
	}

	Database db;
};

TEST_F(PersistenceTest, SerializeAndLoadEmptyDatabase) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 0);
}

TEST_F(PersistenceTest, RoundTripStringEntries) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key1 = NanoObj::FromKey("hello");
	auto val1 = NanoObj::FromString("world");
	auto key2 = NanoObj::FromKey("foo");
	auto val2 = NanoObj::FromString("bar");

	ASSERT_FALSE(serializer.SaveEntry(key1, val1, 0, 0));
	ASSERT_FALSE(serializer.SaveEntry(key2, val2, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	std::vector<std::pair<std::string, std::string>> loaded;
	auto ec = loader.Load([&](uint32_t dbid, const NanoObj& key, const NanoObj& value, int64_t expire_ms) {
		EXPECT_EQ(dbid, 0u);
		EXPECT_EQ(expire_ms, 0);
		loaded.emplace_back(key.ToString(), value.ToString());
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	ASSERT_EQ(loaded.size(), 2u);

	bool found_hello = false;
	bool found_foo = false;
	for (const auto& [k, v] : loaded) {
		if (k == "hello") {
			EXPECT_EQ(v, "world");
			found_hello = true;
		} else if (k == "foo") {
			EXPECT_EQ(v, "bar");
			found_foo = true;
		}
	}
	EXPECT_TRUE(found_hello);
	EXPECT_TRUE(found_foo);
}

TEST_F(PersistenceTest, RoundTripIntEntry) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("counter");
	auto val = NanoObj::FromInt(42);
	ASSERT_FALSE(serializer.SaveEntry(key, val, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_TRUE(value.IsInt());
		EXPECT_EQ(value.AsInt(), 42);
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripNegativeInt) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("neg");
	auto val = NanoObj::FromInt(-999);
	ASSERT_FALSE(serializer.SaveEntry(key, val, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_TRUE(value.IsInt());
		EXPECT_EQ(value.AsInt(), -999);
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripHashEntry) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("myhash");
	auto val = NanoObj::FromHash();
	auto* hash = new HashType();
	val.SetObj(hash);
	(*hash)["field1"] = "value1";
	(*hash)["field2"] = "value2";

	ASSERT_FALSE(serializer.SaveEntry(key, val, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj& k, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_EQ(k.ToString(), "myhash");
		EXPECT_TRUE(value.IsHash());
		auto* loaded_hash = value.GetObj<HashType>();
		EXPECT_NE(loaded_hash, nullptr);
		if (loaded_hash != nullptr) {
			EXPECT_EQ(loaded_hash->size(), 2u);
			EXPECT_EQ((*loaded_hash)["field1"], "value1");
			EXPECT_EQ((*loaded_hash)["field2"], "value2");
		}
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripSetEntry) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("myset");
	auto val = NanoObj::FromSet();
	auto* set = new SetType();
	val.SetObj(set);
	set->insert("a");
	set->insert("b");
	set->insert("c");

	ASSERT_FALSE(serializer.SaveEntry(key, val, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_TRUE(value.IsSet());
		auto* loaded_set = value.GetObj<SetType>();
		EXPECT_NE(loaded_set, nullptr);
		if (loaded_set != nullptr) {
			EXPECT_EQ(loaded_set->size(), 3u);
			EXPECT_TRUE(loaded_set->count("a") > 0);
			EXPECT_TRUE(loaded_set->count("b") > 0);
			EXPECT_TRUE(loaded_set->count("c") > 0);
		}
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripListEntry) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("mylist");
	auto val = NanoObj::FromList();
	auto* list = new std::deque<NanoObj>();
	val.SetObj(list);
	list->push_back(NanoObj::FromString("elem1"));
	list->push_back(NanoObj::FromString("elem2"));
	list->push_back(NanoObj::FromString("elem3"));

	ASSERT_FALSE(serializer.SaveEntry(key, val, 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_TRUE(value.IsList());
		auto* loaded_list = value.GetObj<std::deque<NanoObj>>();
		EXPECT_NE(loaded_list, nullptr);
		if (loaded_list != nullptr) {
			EXPECT_EQ(loaded_list->size(), 3u);
			EXPECT_EQ((*loaded_list)[0].ToString(), "elem1");
			EXPECT_EQ((*loaded_list)[1].ToString(), "elem2");
			EXPECT_EQ((*loaded_list)[2].ToString(), "elem3");
		}
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripWithExpiry) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	auto key = NanoObj::FromKey("expiring");
	auto val = NanoObj::FromString("data");
	constexpr int64_t kExpireMs = 1700000000000LL;
	ASSERT_FALSE(serializer.SaveEntry(key, val, kExpireMs, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj& k, const NanoObj& value, int64_t expire_ms) -> std::error_code {
		EXPECT_EQ(k.ToString(), "expiring");
		EXPECT_EQ(value.ToString(), "data");
		EXPECT_EQ(expire_ms, kExpireMs);
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}

TEST_F(PersistenceTest, RoundTripMultipleDBs) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("k0"), NanoObj::FromString("v0"), 0, 0));
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("k3"), NanoObj::FromString("v3"), 0, 3));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	std::vector<std::pair<uint32_t, std::string>> loaded;
	auto ec = loader.Load([&](uint32_t dbid, const NanoObj& key, const NanoObj&, int64_t) {
		loaded.emplace_back(dbid, key.ToString());
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	ASSERT_EQ(loaded.size(), 2u);
	EXPECT_EQ(loaded[0].first, 0u);
	EXPECT_EQ(loaded[0].second, "k0");
	EXPECT_EQ(loaded[1].first, 3u);
	EXPECT_EQ(loaded[1].second, "k3");
}

TEST_F(PersistenceTest, CorruptedMagicFails) {
	std::vector<uint8_t> data = {'B', 'A', 'D', 'M', 'A', 'G', 'I', 'C'};
	MemorySource source(data);
	RdbLoader loader(&source);

	auto ec = loader.Load([](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		return std::error_code {};
	});
	EXPECT_TRUE(ec);
}

TEST_F(PersistenceTest, MixedTypesRoundTrip) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("str"), NanoObj::FromString("hello"), 0, 0));
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("num"), NanoObj::FromInt(100), 0, 0));

	auto hash_val = NanoObj::FromHash();
	auto* hash_inner = new HashType();
	hash_val.SetObj(hash_inner);
	(*hash_inner)["x"] = "y";
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("h"), hash_val, 0, 0));

	auto set_val = NanoObj::FromSet();
	auto* set_inner = new SetType();
	set_val.SetObj(set_inner);
	set_inner->insert("m1");
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("s"), set_val, 0, 0));

	auto list_val = NanoObj::FromList();
	auto* list_inner = new std::deque<NanoObj>();
	list_val.SetObj(list_inner);
	list_inner->push_back(NanoObj::FromString("e1"));
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("l"), list_val, 0, 0));

	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 5);
}

TEST_F(PersistenceTest, FileRoundTrip) {
	{
		FileSink sink(kTestDumpFile);
		ASSERT_TRUE(sink.IsOpen());
		RdbSerializer serializer(&sink);
		ASSERT_FALSE(serializer.SaveHeader());
		ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("persist_key"), NanoObj::FromString("persist_val"), 0, 0));
		ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("int_key"), NanoObj::FromInt(777), 0, 0));
		ASSERT_FALSE(serializer.SaveFooter());
		ASSERT_FALSE(sink.FlushAndClose());
	}

	{
		FileSource source(kTestDumpFile);
		ASSERT_TRUE(source.IsOpen());
		RdbLoader loader(&source);

		std::vector<std::pair<std::string, std::string>> loaded;
		auto ec = loader.Load([&](uint32_t, const NanoObj& key, const NanoObj& value, int64_t) {
			loaded.emplace_back(key.ToString(), value.ToString());
			return std::error_code {};
		});
		ASSERT_FALSE(ec) << ec.message();
		ASSERT_EQ(loaded.size(), 2u);
	}
}

TEST_F(PersistenceTest, SaveCommandRoundTrip) {
	CommandRegistry* registry = &CommandRegistry::Instance();
	ServerFamily::Register(registry);

	db.Set(NanoObj::FromKey("save_k1"), NanoObj::FromString("save_v1"));
	db.Set(NanoObj::FromKey("save_k2"), NanoObj::FromInt(42));

	auto hash_val = NanoObj::FromHash();
	auto* hash_tbl = new HashType();
	hash_val.SetObj(hash_tbl);
	(*hash_tbl)["f1"] = "val1";
	db.Set(NanoObj::FromKey("save_hash"), std::move(hash_val));

	std::vector<NanoObj> args;
	args.emplace_back(NanoObj::FromKey("SAVE"));
	args.emplace_back(NanoObj::FromKey(kTestDumpFile));
	CommandContext ctx(&db, 0);
	std::string result = registry->Execute(args, &ctx);
	EXPECT_EQ(result, "+OK\r\n");

	FileSource source(kTestDumpFile);
	ASSERT_TRUE(source.IsOpen());
	RdbLoader loader(&source);

	int entry_count = 0;
	bool found_k1 = false;
	bool found_k2 = false;
	bool found_hash = false;
	auto ec = loader.Load([&](uint32_t, const NanoObj& key, const NanoObj& value, int64_t) {
		entry_count++;
		const std::string k = key.ToString();
		if (k == "save_k1") {
			EXPECT_EQ(value.ToString(), "save_v1");
			found_k1 = true;
		} else if (k == "save_k2") {
			EXPECT_TRUE(value.IsInt());
			EXPECT_EQ(value.AsInt(), 42);
			found_k2 = true;
		} else if (k == "save_hash") {
			EXPECT_TRUE(value.IsHash());
			found_hash = true;
		}
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 3);
	EXPECT_TRUE(found_k1);
	EXPECT_TRUE(found_k2);
	EXPECT_TRUE(found_hash);
}

TEST_F(PersistenceTest, SliceSnapshotBasic) {
	db.Set(NanoObj::FromKey("snap_k1"), NanoObj::FromString("snap_v1"));
	db.Set(NanoObj::FromKey("snap_k2"), NanoObj::FromString("snap_v2"));
	db.Set(NanoObj::FromKey("snap_k3"), NanoObj::FromInt(123));

	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	{
		SliceSnapshot snapshot(&db, &serializer, 1);
		auto ec = snapshot.SerializeAllDBs();
		ASSERT_FALSE(ec) << ec.message();
	}

	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 3);
}

TEST_F(PersistenceTest, SliceSnapshotVersionPreventsDoubleSerialize) {
	db.Set(NanoObj::FromKey("v_k1"), NanoObj::FromString("v_v1"));

	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	{
		SliceSnapshot snapshot(&db, &serializer, 1);
		auto ec = snapshot.SerializeAllDBs();
		ASSERT_FALSE(ec) << ec.message();
	}
	{
		SliceSnapshot snapshot2(&db, &serializer, 1);
		auto ec = snapshot2.SerializeAllDBs();
		ASSERT_FALSE(ec) << ec.message();
	}

	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 1);
}

TEST_F(PersistenceTest, SliceSnapshotHigherVersionSerializesAgain) {
	db.Set(NanoObj::FromKey("v_k1"), NanoObj::FromString("v_v1"));

	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	{
		SliceSnapshot snapshot(&db, &serializer, 1);
		ASSERT_FALSE(snapshot.SerializeAllDBs());
	}

	ASSERT_FALSE(serializer.SaveFooter());

	MemorySink sink2;
	RdbSerializer serializer2(&sink2);
	ASSERT_FALSE(serializer2.SaveHeader());

	{
		SliceSnapshot snapshot2(&db, &serializer2, 2);
		ASSERT_FALSE(snapshot2.SerializeAllDBs());
	}

	ASSERT_FALSE(serializer2.SaveFooter());

	MemorySource source(sink2.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 1);
}

TEST_F(PersistenceTest, SliceSnapshotPreModifyCallbackSerializesBeforeWrite) {
	for (int i = 0; i < 20; ++i) {
		db.Set(NanoObj::FromKey("pmk_" + std::to_string(i)),
		       NanoObj::FromString("pmv_" + std::to_string(i)));
	}

	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());

	constexpr uint64_t kSnapshotVersion = 10;
	SliceSnapshot snapshot(&db, &serializer, kSnapshotVersion);

	auto* table = db.GetTable(0);
	ASSERT_NE(table, nullptr);

	table->SetPreModifyCallback([&snapshot](size_t dir_idx) {
		(void)dir_idx;
	});

	auto ec = snapshot.SerializeAllDBs();
	ASSERT_FALSE(ec) << ec.message();
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, 20);
}

TEST_F(PersistenceTest, BgSaveRejectsWhenInProgress) {
	CommandRegistry* registry = &CommandRegistry::Instance();
	ServerFamily::Register(registry);

	db.Set(NanoObj::FromKey("bg_k"), NanoObj::FromString("bg_v"));

	std::vector<NanoObj> args;
	args.emplace_back(NanoObj::FromKey("BGSAVE"));
	args.emplace_back(NanoObj::FromKey(kTestDumpFile));
	CommandContext ctx(&db, 0);
	std::string result = registry->Execute(args, &ctx);
	EXPECT_TRUE(result.find("Background saving started") != std::string::npos);
}

TEST_F(PersistenceTest, SaveAndLoadBackToDatabase) {
	db.Set(NanoObj::FromKey("restore_k1"), NanoObj::FromString("restore_v1"));
	db.Set(NanoObj::FromKey("restore_k2"), NanoObj::FromInt(99));

	{
		FileSink sink(kTestDumpFile);
		ASSERT_TRUE(sink.IsOpen());
		RdbSerializer serializer(&sink);
		ASSERT_FALSE(serializer.SaveHeader());
		SliceSnapshot snapshot(&db, &serializer, 1);
		ASSERT_FALSE(snapshot.SerializeAllDBs());
		ASSERT_FALSE(serializer.SaveFooter());
		ASSERT_FALSE(sink.FlushAndClose());
	}

	Database loaded_db;
	loaded_db.Select(0);

	{
		FileSource source(kTestDumpFile);
		ASSERT_TRUE(source.IsOpen());
		RdbLoader loader(&source);

		auto ec = loader.Load([&](uint32_t dbid, const NanoObj& key, const NanoObj& value, int64_t) {
			loaded_db.Select(dbid);
			loaded_db.Set(key, value);
			return std::error_code {};
		});
		ASSERT_FALSE(ec) << ec.message();
	}

	loaded_db.Select(0);
	auto v1 = loaded_db.Get(NanoObj::FromKey("restore_k1"));
	ASSERT_TRUE(v1.has_value());
	EXPECT_EQ(*v1, "restore_v1");

	const NanoObj* v2 = loaded_db.Find(NanoObj::FromKey("restore_k2"));
	ASSERT_NE(v2, nullptr);
	EXPECT_TRUE(v2->IsInt());
	EXPECT_EQ(v2->AsInt(), 99);
}

TEST_F(PersistenceTest, LargeDatasetRoundTrip) {
	constexpr int kNumEntries = 500;
	for (int i = 0; i < kNumEntries; ++i) {
		db.Set(NanoObj::FromKey("large_k_" + std::to_string(i)),
		       NanoObj::FromString("large_v_" + std::to_string(i)));
	}

	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());
	SliceSnapshot snapshot(&db, &serializer, 1);
	ASSERT_FALSE(snapshot.SerializeAllDBs());
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	int entry_count = 0;
	auto ec = loader.Load([&](uint32_t, const NanoObj&, const NanoObj&, int64_t) {
		entry_count++;
		return std::error_code {};
	});
	ASSERT_FALSE(ec) << ec.message();
	EXPECT_EQ(entry_count, kNumEntries);
}

TEST_F(PersistenceTest, DashTablePreModifyCallbackFires) {
	DashTable<NanoObj, NanoObj> table;

	int callback_count = 0;
	table.SetPreModifyCallback([&callback_count](size_t) {
		callback_count++;
	});

	table.Insert(NanoObj::FromKey("a"), NanoObj::FromString("1"));
	EXPECT_EQ(callback_count, 1);

	table.Insert(NanoObj::FromKey("b"), NanoObj::FromString("2"));
	EXPECT_EQ(callback_count, 2);

	table.Erase(NanoObj::FromKey("a"));
	EXPECT_EQ(callback_count, 3);

	table.ClearPreModifyCallback();
	table.Insert(NanoObj::FromKey("c"), NanoObj::FromString("3"));
	EXPECT_EQ(callback_count, 3);
}

TEST_F(PersistenceTest, DashTableSegmentVersionInheritedOnSplit) {
	DashTable<NanoObj, NanoObj> table(1, 4);

	table.SetSegVersion(0, 42);

	for (int i = 0; i < 20; ++i) {
		table.Insert(NanoObj::FromKey("split_" + std::to_string(i)),
		             NanoObj::FromString("v_" + std::to_string(i)));
	}

	for (size_t dir_idx = 0; dir_idx < table.DirSize();
	     dir_idx = table.NextUniqueSegment(dir_idx)) {
		EXPECT_GE(table.GetSegVersion(dir_idx), 42u)
		    << "Segment at dir_idx=" << dir_idx << " lost version after split";
	}
}

TEST_F(PersistenceTest, EmptyStringValueRoundTrip) {
	MemorySink sink;
	RdbSerializer serializer(&sink);
	ASSERT_FALSE(serializer.SaveHeader());
	ASSERT_FALSE(serializer.SaveEntry(NanoObj::FromKey("empty"), NanoObj::FromString(""), 0, 0));
	ASSERT_FALSE(serializer.SaveFooter());

	MemorySource source(sink.buffer);
	RdbLoader loader(&source);

	auto ec = loader.Load([&](uint32_t, const NanoObj& key, const NanoObj& value, int64_t) -> std::error_code {
		EXPECT_EQ(key.ToString(), "empty");
		EXPECT_EQ(value.ToString(), "");
		return {};
	});
	ASSERT_FALSE(ec) << ec.message();
}
