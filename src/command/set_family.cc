#include "command/set_family.h"
#include "core/command_context.h"
#include "protocol/resp_parser.h"
#include <cstdlib>
#include <sstream>
#include <vector>
#include <algorithm>

void SetFamily::Register(CommandRegistry* registry) {
	registry->register_command_with_context("SADD", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SAdd(args, ctx); });
	registry->register_command_with_context("SREM", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SRem(args, ctx); });
	registry->register_command_with_context("SPOP", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SPop(args, ctx); });
	registry->register_command_with_context("SMEMBERS", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SMembers(args, ctx); });
	registry->register_command_with_context("SCARD", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SCard(args, ctx); });
	registry->register_command_with_context("SISMEMBER", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SIsMember(args, ctx); });
	registry->register_command_with_context("SMISMEMBER", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SMIsMember(args, ctx); });
	registry->register_command_with_context("SINTER", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SInter(args, ctx); });
	registry->register_command_with_context("SUNION", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SUnion(args, ctx); });
	registry->register_command_with_context("SDIFF", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SDiff(args, ctx); });
	registry->register_command_with_context("SSCAN", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SScan(args, ctx); });
	registry->register_command_with_context("SRANDMEMBER", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SRandMember(args, ctx); });
	registry->register_command_with_context("SMOVE", [](const std::vector<CompactObj>& args, CommandContext* ctx) { return SMove(args, ctx); });
}

std::string SetFamily::SAdd(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for SADD");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		if (set_obj != nullptr && !set_obj->isSet()) {
			db->Del(key);
		}
		auto set = new SetType();
		CompactObj new_set = CompactObj::fromSet();
		new_set.setObj(set);
		db->Set(key, std::move(new_set));
		set_obj = db->Find(key);
	}

	auto set = set_obj->getObj<SetType>();
	int added = 0;
	for (size_t i = 1; i < args.size(); i++) {
		std::string member = args[i].toString();
		if (set->insert(member).second) {
			added++;
		}
	}

	return RESPParser::make_integer(added);
}

std::string SetFamily::SRem(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for SREM");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_integer(0);
	}

	auto set = set_obj->getObj<SetType>();
	int removed = 0;
	for (size_t i = 1; i < args.size(); i++) {
		std::string member = args[i].toString();
		if (set->erase(member)) {
			removed++;
		}
	}

	if (set->empty()) {
		db->Del(key);
	}

	return RESPParser::make_integer(removed);
}

std::string SetFamily::SPop(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 1 || args.size() > 2) {
		return RESPParser::make_error("wrong number of arguments for SPOP");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_null_bulk_string();
	}

	auto set = set_obj->getObj<SetType>();

	if (set->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	int64_t count = 1;
	if (args.size() == 2) {
		if (!ParseLongLong(args[1].toString(), &count) || count < 0) {
			return RESPParser::make_error("count is not a valid positive integer");
		}
	}

	std::string result = RESPParser::make_array(count);
	int64_t i = 0;
	for (auto it = set->begin(); i < count && it != set->end(); ++i) {
		result += RESPParser::make_bulk_string(*it);
		it = set->erase(it);
	}

	if (set->empty()) {
		db->Del(key);
	}

	return result;
}

std::string SetFamily::SMembers(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for SMEMBERS");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_array(0);
	}

	auto set = set_obj->getObj<SetType>();

	std::string result = RESPParser::make_array(static_cast<int64_t>(set->size()));
	for (const auto& member : *set) {
		result += RESPParser::make_bulk_string(member);
	}

	return result;
}

std::string SetFamily::SCard(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 1) {
		return RESPParser::make_error("wrong number of arguments for SCARD");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_integer(0);
	}

	auto set = set_obj->getObj<SetType>();
	return RESPParser::make_integer(static_cast<int64_t>(set->size()));
}

std::string SetFamily::SIsMember(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 2) {
		return RESPParser::make_error("wrong number of arguments for SISMEMBER");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_integer(0);
	}

	auto set = set_obj->getObj<SetType>();
	std::string member = args[1].toString();

	return RESPParser::make_integer(set->count(member) ? 1 : 0);
}

std::string SetFamily::SMIsMember(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for SMISMEMBER");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	std::string result = RESPParser::make_array(args.size() - 1);

	if (set_obj == nullptr || !set_obj->isSet()) {
		for (size_t i = 1; i < args.size(); i++) {
			result += RESPParser::make_integer(0);
		}
		return result;
	}

	auto set = set_obj->getObj<SetType>();
	for (size_t i = 1; i < args.size(); i++) {
		std::string member = args[i].toString();
		result += RESPParser::make_integer(set->count(member) ? 1 : 0);
	}

	return result;
}

std::string SetFamily::SInter(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 1) {
		return RESPParser::make_error("wrong number of arguments for SINTER");
	}

	if (args.size() == 1) {
		return RESPParser::make_array(0);
	}

	auto* db = ctx->GetDB();

	std::vector<SetType*> sets;
	for (size_t i = 0; i < args.size(); i++) {
		const CompactObj& key = args[i];
		auto* set_obj = db->Find(key);

		if (set_obj == nullptr || !set_obj->isSet()) {
			return RESPParser::make_array(0);
		}

		auto set = set_obj->getObj<SetType>();
		sets.push_back(set);
	}

	SetType intersection = *sets[0];
	for (size_t i = 1; i < sets.size(); i++) {
		SetType temp;
		for (const auto& elem : intersection) {
			if (sets[i]->count(elem)) {
				temp.insert(elem);
			}
		}
		intersection = std::move(temp);
	}

	std::string result = RESPParser::make_array(static_cast<int64_t>(intersection.size()));
	for (const auto& elem : intersection) {
		result += RESPParser::make_bulk_string(elem);
	}

	return result;
}

std::string SetFamily::SUnion(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 1) {
		return RESPParser::make_error("wrong number of arguments for SUNION");
	}

	auto* db = ctx->GetDB();

	SetType union_set;
	for (size_t i = 0; i < args.size(); i++) {
		const CompactObj& key = args[i];
		auto* set_obj = db->Find(key);

		if (set_obj != nullptr && set_obj->isSet()) {
			auto set = set_obj->getObj<SetType>();
			union_set.insert(set->begin(), set->end());
		}
	}

	std::string result = RESPParser::make_array(static_cast<int64_t>(union_set.size()));
	for (const auto& elem : union_set) {
		result += RESPParser::make_bulk_string(elem);
	}

	return result;
}

std::string SetFamily::SDiff(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 1) {
		return RESPParser::make_error("wrong number of arguments for SDIFF");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_array(0);
	}

	auto diff_set = *set_obj->getObj<SetType>();

	for (size_t i = 1; i < args.size(); i++) {
		const CompactObj& key = args[i];
		auto* set_obj = db->Find(key);

		if (set_obj != nullptr && set_obj->isSet()) {
			auto set = set_obj->getObj<SetType>();
			for (const auto& elem : *set) {
				diff_set.erase(elem);
			}
		}
	}

	std::string result = RESPParser::make_array(static_cast<int64_t>(diff_set.size()));
	for (const auto& elem : diff_set) {
		result += RESPParser::make_bulk_string(elem);
	}

	return result;
}

std::string SetFamily::SScan(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 2) {
		return RESPParser::make_error("wrong number of arguments for SSCAN");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_error("WRONGTYPE Operation against a key holding the wrong kind of value");
	}

	auto set = set_obj->getObj<SetType>();

	uint64_t cursor = 0;
	if (args.size() >= 3) {
		try {
			cursor = std::stoull(args[2].toString());
		} catch (...) {
			return RESPParser::make_error("invalid cursor");
		}
	}

	if (cursor != 0) {
		std::string result = RESPParser::make_array(2);
		result += RESPParser::make_bulk_string("0");
		result += RESPParser::make_array(0);
		return result;
	}

	std::string result = RESPParser::make_array(2);
	result += RESPParser::make_bulk_string("0");
	result += RESPParser::make_array(static_cast<int64_t>(set->size()));
	for (const auto& elem : *set) {
		result += RESPParser::make_bulk_string(elem);
	}

	return result;
}

std::string SetFamily::SRandMember(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() < 1 || args.size() > 2) {
		return RESPParser::make_error("wrong number of arguments for SRANDMEMBER");
	}

	auto* db = ctx->GetDB();
	const CompactObj& key = args[0];
	auto* set_obj = db->Find(key);

	if (set_obj == nullptr || !set_obj->isSet()) {
		return RESPParser::make_null_bulk_string();
	}

	auto set = set_obj->getObj<SetType>();

	if (set->empty()) {
		return RESPParser::make_null_bulk_string();
	}

	if (args.size() == 1) {
		auto it = set->begin();
		size_t offset = std::rand() % set->size();
		std::advance(it, offset);
		return RESPParser::make_bulk_string(*it);
	}

	int64_t count = 1;
	if (!ParseLongLong(args[1].toString(), &count)) {
		return RESPParser::make_error("count is not a valid integer");
	}

	std::vector<std::string> members;
	for (const auto& member : *set) {
		members.push_back(member);
	}

	std::string result = RESPParser::make_array(count);
	for (int64_t i = 0; i < count && !members.empty(); i++) {
		size_t offset = std::rand() % members.size();
		result += RESPParser::make_bulk_string(members[offset]);
		members.erase(members.begin() + offset);
	}

	return result;
}

std::string SetFamily::SMove(const std::vector<CompactObj>& args, CommandContext* ctx) {
	if (args.size() != 3) {
		return RESPParser::make_error("wrong number of arguments for SMOVE");
	}

	auto* db = ctx->GetDB();
	const CompactObj& src_key = args[0];
	const CompactObj& dest_key = args[1];
	const std::string member = args[2].toString();

	auto* src_obj = db->Find(src_key);

	if (src_obj == nullptr || !src_obj->isSet()) {
		return RESPParser::make_integer(0);
	}

	auto src_set = src_obj->getObj<SetType>();

	auto it = src_set->find(member);
	if (it == src_set->end()) {
		return RESPParser::make_integer(0);
	}

	src_set->erase(it);

	if (src_set->empty()) {
		db->Del(src_key);
	}

	auto* dest_obj = db->Find(dest_key);
	if (dest_obj == nullptr || !dest_obj->isSet()) {
		if (dest_obj != nullptr && !dest_obj->isSet()) {
			db->Del(dest_key);
		}
		auto dest_set = new SetType();
		CompactObj new_set = CompactObj::fromSet();
		new_set.setObj(dest_set);
		db->Set(dest_key, std::move(new_set));
		dest_obj = db->Find(dest_key);
	}

	auto dest_set = dest_obj->getObj<SetType>();
	dest_set->insert(member);

	return RESPParser::make_integer(1);
}

bool SetFamily::ParseLongLong(const std::string& s, int64_t* out) {
	char* end;
	*out = std::strtoll(s.c_str(), &end, 10);

	if (end == s.c_str() || *end != '\0') {
		return false;
	}

	if (*out == LLONG_MAX || *out == LLONG_MIN) {
		return false;
	}

	return true;
}
