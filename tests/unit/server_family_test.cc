#include <gtest/gtest.h>

#include <charconv>
#include <chrono>
#include <optional>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <gflags/gflags.h>

#include "command/command_registry.h"
#include "command/server_family.h"
#include "core/command_context.h"
#include "core/database.h"
#include "core/nano_obj.h"
#include "server/connection.h"
#include "server/proactor_pool.h"

DEFINE_int32(port, 9527, "Server listen port");
DEFINE_int32(num_shards, 8, "Number of shards");
DEFINE_bool(tcp_nodelay, true, "Enable TCP_NODELAY");
DEFINE_bool(use_iouring_tcp_server, true, "Use io_uring tcp server");
DEFINE_uint64(photon_handler_stack_kb, 256, "Photon stack size KB");

class ServerFamilyTest : public ::testing::Test {
protected:
	class ScopedRegisteredConnection {
	public:
		explicit ScopedRegisteredConnection(Connection* connection) : connection(connection) {
			if (this->connection != nullptr) {
				ProactorPool::RegisterLocalConnection(this->connection);
			}
		}

		~ScopedRegisteredConnection() {
			if (connection != nullptr) {
				ProactorPool::UnregisterLocalConnection(connection->GetClientId());
			}
		}

	private:
		Connection* connection = nullptr;
	};

	void SetUp() override {
		registry = &CommandRegistry::Instance();
		ServerFamily::Register(registry);
		db.ClearAll();
		db.Select(0);
	}

	std::string Execute(const std::string& cmd, const std::vector<std::string>& args) {
		std::vector<NanoObj> full_args;
		full_args.reserve(1 + args.size());
		full_args.emplace_back(NanoObj::FromKey(cmd));
		for (const auto& arg : args) {
			full_args.emplace_back(NanoObj::FromKey(arg));
		}
		CommandContext ctx(&db, db.CurrentDB());
		return registry->Execute(full_args, &ctx);
	}

	std::string ExecuteWithConnection(const std::string& cmd, const std::vector<std::string>& args, Connection* conn) {
		std::vector<NanoObj> full_args;
		full_args.reserve(1 + args.size());
		full_args.emplace_back(NanoObj::FromKey(cmd));
		for (const auto& arg : args) {
			full_args.emplace_back(NanoObj::FromKey(arg));
		}
		CommandContext ctx(&db, db.CurrentDB(), conn);
		return registry->Execute(full_args, &ctx);
	}

	std::optional<int64_t> ParseIntegerResponse(const std::string& response) {
		if (response.size() < 4 || response[0] != ':' || response[response.size() - 2] != '\r' ||
		    response[response.size() - 1] != '\n') {
			return std::nullopt;
		}
		int64_t value = 0;
		const char* start = response.data() + 1;
		const char* end = response.data() + response.size() - 2;
		auto [ptr, ec] = std::from_chars(start, end, value, 10);
		if (ec != std::errc() || ptr != end) {
			return std::nullopt;
		}
		return value;
	}

	CommandRegistry* registry = nullptr;
	Database db;
};

TEST_F(ServerFamilyTest, InfoContainsServerAndKeyspaceSections) {
	db.Set(NanoObj::FromKey("k1"), NanoObj::FromKey("v1"));

	std::string response = Execute("INFO", {});
	EXPECT_TRUE(response.find("# Server\r\n") != std::string::npos);
	EXPECT_TRUE(response.find("# Keyspace\r\n") != std::string::npos);
	EXPECT_TRUE(response.find("db0:keys=1\r\n") != std::string::npos);
}

TEST_F(ServerFamilyTest, ConfigGetPort) {
	std::string response = Execute("CONFIG", {"GET", "port"});
	EXPECT_TRUE(response.find("port") != std::string::npos);
	EXPECT_TRUE(response.find("9527") != std::string::npos);
}

TEST_F(ServerFamilyTest, ConfigSetAndGetBooleanOption) {
	const bool old_value = FLAGS_tcp_nodelay;

	EXPECT_EQ(Execute("CONFIG", {"SET", "tcp_nodelay", "no"}), "+OK\r\n");
	std::string response = Execute("CONFIG", {"GET", "tcp_nodelay"});
	EXPECT_TRUE(response.find("tcp_nodelay") != std::string::npos);
	EXPECT_TRUE(response.find("no") != std::string::npos);

	EXPECT_EQ(Execute("CONFIG", {"SET", "tcp_nodelay", old_value ? "yes" : "no"}), "+OK\r\n");
}

TEST_F(ServerFamilyTest, ConfigSetInvalidValueReturnsError) {
	std::string response = Execute("CONFIG", {"SET", "tcp_nodelay", "bad"});
	EXPECT_TRUE(response.find("Invalid argument for CONFIG SET 'tcp_nodelay'") != std::string::npos);
}

TEST_F(ServerFamilyTest, ClientSetNameGetNameAndId) {
	Connection conn(nullptr);

	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"GETNAME"}, &conn), "$-1\r\n");
	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"SETNAME", "client-a"}, &conn), "+OK\r\n");
	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"GETNAME"}, &conn), "$8\r\nclient-a\r\n");

	auto client_id = ParseIntegerResponse(ExecuteWithConnection("CLIENT", {"ID"}, &conn));
	ASSERT_TRUE(client_id.has_value());
	EXPECT_NE(*client_id, 0);
}

TEST_F(ServerFamilyTest, ClientFailsWithoutConnectionContext) {
	std::string response = Execute("CLIENT", {"GETNAME"});
	EXPECT_TRUE(response.find("CLIENT command not available in this context") != std::string::npos);
}

TEST_F(ServerFamilyTest, ClientInfoReturnsCurrentClientLine) {
	Connection conn(nullptr);
	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"SETNAME", "cli-info"}, &conn), "+OK\r\n");

	std::string response = ExecuteWithConnection("CLIENT", {"INFO"}, &conn);
	EXPECT_TRUE(response.find("id=" + std::to_string(conn.GetClientId())) != std::string::npos);
	EXPECT_TRUE(response.find("name=cli-info") != std::string::npos);
	EXPECT_TRUE(response.find("cmd=unknown") != std::string::npos);
}

TEST_F(ServerFamilyTest, ClientListShowsRegisteredClients) {
	Connection conn_a(nullptr);
	Connection conn_b(nullptr);
	conn_a.SetClientName("a");
	conn_b.SetClientName("b");
	ScopedRegisteredConnection reg_a(&conn_a);
	ScopedRegisteredConnection reg_b(&conn_b);

	std::string response = ExecuteWithConnection("CLIENT", {"LIST"}, &conn_a);
	EXPECT_TRUE(response.find("id=" + std::to_string(conn_a.GetClientId())) != std::string::npos);
	EXPECT_TRUE(response.find("id=" + std::to_string(conn_b.GetClientId())) != std::string::npos);
	EXPECT_TRUE(response.find("name=a") != std::string::npos);
	EXPECT_TRUE(response.find("name=b") != std::string::npos);
}

TEST_F(ServerFamilyTest, ClientKillByIdRequestsClose) {
	Connection conn_a(nullptr);
	Connection conn_b(nullptr);
	ScopedRegisteredConnection reg_a(&conn_a);
	ScopedRegisteredConnection reg_b(&conn_b);

	const std::string target_id = std::to_string(conn_b.GetClientId());
	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"KILL", "ID", target_id}, &conn_a), ":1\r\n");
	EXPECT_TRUE(conn_b.IsCloseRequested());

	EXPECT_EQ(ExecuteWithConnection("CLIENT", {"KILL", "ID", "99999999"}, &conn_a), ":0\r\n");
}

TEST_F(ServerFamilyTest, ClientPauseAcceptsModes) {
	EXPECT_EQ(Execute("CLIENT", {"PAUSE", "1"}), "+OK\r\n");
	EXPECT_TRUE(ProactorPool::PauseUntilMs() > 0);
	EXPECT_EQ(Execute("CLIENT", {"PAUSE", "1", "WRITE"}), "+OK\r\n");
	EXPECT_TRUE(Execute("CLIENT", {"PAUSE", "1", "BAD"}).find("unsupported CLIENT PAUSE mode") != std::string::npos);
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
}

TEST_F(ServerFamilyTest, TimeReturnsTwoBulkFields) {
	std::string response = Execute("TIME", {});

	EXPECT_TRUE(response.find("*2\r\n") == 0);
	EXPECT_TRUE(response.find("\r\n$") != std::string::npos);
}

TEST_F(ServerFamilyTest, RandomKeyReturnsNullWhenEmpty) {
	EXPECT_EQ(Execute("RANDOMKEY", {}), "$-1\r\n");
}

TEST_F(ServerFamilyTest, RandomKeyReturnsExistingKey) {
	db.Set(NanoObj::FromKey("alpha"), NanoObj::FromKey("1"));
	db.Set(NanoObj::FromKey("beta"), NanoObj::FromKey("2"));

	std::string response = Execute("RANDOMKEY", {});
	const bool is_alpha = response == "$5\r\nalpha\r\n";
	const bool is_beta = response == "$4\r\nbeta\r\n";
	EXPECT_TRUE(is_alpha || is_beta);
}
