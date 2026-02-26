#pragma once

#include <array>
#include <cstdint>

constexpr size_t kNrdbMagicSize = 8;
constexpr std::array<uint8_t, kNrdbMagicSize> kNrdbMagic = {
	'N', 'R', 'D', 'B', '0', '0', '0', '1'};

constexpr uint8_t NRDB_OPCODE_DB_SELECT = 0xF0;
constexpr uint8_t NRDB_OPCODE_DB_SIZE = 0xF1;
constexpr uint8_t NRDB_OPCODE_EXPIRE_MS = 0xFD;
constexpr uint8_t NRDB_OPCODE_EOF = 0xFF;

constexpr uint8_t NRDB_OBJ_STRING = 0x00;
constexpr uint8_t NRDB_OBJ_INT = 0x01;
constexpr uint8_t NRDB_OBJ_HASH = 0x02;
constexpr uint8_t NRDB_OBJ_SET = 0x03;
constexpr uint8_t NRDB_OBJ_LIST = 0x04;
constexpr uint8_t NRDB_OBJ_ZSET = 0x05;
