/*
    Copyright 2020 The Silkrpc Authors

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "eth_api.hpp"

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>

#include <evmc/evmc.hpp>

#include <silkworm/common/util.hpp>
#include <silkworm/types/receipt.hpp>
#include <silkworm/db/tables.hpp>
#include <silkrpc/common/constants.hpp>
#include <silkrpc/common/log.hpp>
#include <silkrpc/common/util.hpp>
#include <silkrpc/core/blocks.hpp>
#include <silkrpc/core/rawdb/chain.hpp>
#include <silkrpc/ethdb/bitmap/database.hpp>
#include <silkrpc/json/types.hpp>
#include <silkrpc/types/block.hpp>
#include <silkrpc/types/filter.hpp>

namespace silkrpc::commands {

// https://eth.wiki/json-rpc/API#eth_blocknumber
asio::awaitable<void> EthereumRpcApi::handle_eth_block_number(const nlohmann::json& request, nlohmann::json& reply) {
    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};
        const auto block_height = co_await core::get_current_block_number(tx_database);
        reply = make_json_content(request["id"], "0x" + to_hex_no_leading_zeros(block_height));
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

// https://eth.wiki/json-rpc/API#eth_chainId
asio::awaitable<void> EthereumRpcApi::handle_eth_chain_id(const nlohmann::json& request, nlohmann::json& reply) {
    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};
        const auto chain_id = co_await core::rawdb::read_chain_config(tx_database);
        reply = make_json_content(request["id"], "0x" + to_hex_no_leading_zeros(chain_id));
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

// https://eth.wiki/json-rpc/API#eth_protocolVersion
asio::awaitable<void> EthereumRpcApi::handle_eth_protocol_version(const nlohmann::json& request, nlohmann::json& reply) {
    reply = make_json_content(request["id"], "0x" + to_hex_no_leading_zeros(common::kETH66));
    co_return;
}

// https://eth.wiki/json-rpc/API#eth_syncing
asio::awaitable<void> EthereumRpcApi::handle_eth_syncing(const nlohmann::json& request, nlohmann::json& reply) {
    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};
        const auto current_block_height = co_await core::get_current_block_number(tx_database);
        const auto highest_block_height = co_await core::get_highest_block_number(tx_database);
        if (current_block_height >= highest_block_height) {
            reply = make_json_content(request["id"], false);
        } else {
            reply = make_json_content(request["id"], {
                {"currentBlock", "0x" + to_hex_no_leading_zeros(current_block_height)},
                {"highestBlock", "0x" + to_hex_no_leading_zeros(highest_block_height)},
            });
        }
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

// https://eth.wiki/json-rpc/API#eth_getblockbyhash
asio::awaitable<void> EthereumRpcApi::handle_eth_get_block_by_hash(const nlohmann::json& request, nlohmann::json& reply) {
    auto params = request["params"];
    if (params.size() != 2) {
        auto error_msg = "invalid eth_getLogs params: " + params.dump();
        SILKRPC_ERROR << error_msg << "\n";
        reply = make_json_error(request["id"], 100, error_msg);
        co_return;
    }
    auto block_hash = params[0].get<evmc::bytes32>();
    auto full_tx = params[1].get<bool>();
    SILKRPC_DEBUG << "block_hash: " << block_hash << " full_tx: " << std::boolalpha << full_tx << "\n";

    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};

        const auto block_with_hash = co_await core::rawdb::read_block_by_hash(tx_database, block_hash);
        const auto block_number = block_with_hash.block.header.number;
        const auto total_difficulty = co_await core::rawdb::read_total_difficulty(tx_database, block_hash, block_number);
        const Block extended_block{block_with_hash, total_difficulty, full_tx};

        reply = make_json_content(request["id"], extended_block);
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

// https://eth.wiki/json-rpc/API#eth_getblockbynumber
asio::awaitable<void> EthereumRpcApi::handle_eth_get_block_by_number(const nlohmann::json& request, nlohmann::json& reply) {
    auto params = request["params"];
    if (params.size() != 2) {
        auto error_msg = "invalid eth_getLogs params: " + params.dump();
        SILKRPC_ERROR << error_msg << "\n";
        reply = make_json_error(request["id"], 100, error_msg);
        co_return;
    }
    const auto block_number_or_name = params[0].get<std::string>();
    auto full_tx = params[1].get<bool>();
    SILKRPC_DEBUG << "block_number_or_name: " << block_number_or_name << " full_tx: " << std::boolalpha << full_tx << "\n";

    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};

        uint64_t block_number_extended;
        if (block_number_or_name == "earliest") {
            block_number_extended = core::kEarliestBlockNumber;
        } else if (block_number_or_name == "latest") {
            block_number_extended = core::kLatestBlockNumber;
        } else if (block_number_or_name == "pending") {
            block_number_extended = core::kPendingBlockNumber;
        } else {
            block_number_extended = std::stol(block_number_or_name, 0, 16);
        }
        const auto block_number = co_await core::get_block_number(block_number_extended, tx_database);
        const auto block_with_hash = co_await core::rawdb::read_block_by_number(tx_database, block_number);
        const auto total_difficulty = co_await core::rawdb::read_total_difficulty(tx_database, block_with_hash.hash, block_number);
        const Block extended_block{block_with_hash, total_difficulty, full_tx};

        reply = make_json_content(request["id"], extended_block);
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

// https://github.com/ethereum/wiki/wiki/JSON-RPC#eth_getLogs
asio::awaitable<void> EthereumRpcApi::handle_eth_get_logs(const nlohmann::json& request, nlohmann::json& reply) {
    auto params = request["params"];
    if (params.size() != 1) {
        auto error_msg = "invalid eth_getLogs params: " + params.dump();
        SILKRPC_ERROR << error_msg << "\n";
        reply = make_json_error(request["id"], 100, error_msg);
        co_return;
    }
    auto filter = params[0].get<Filter>();
    SILKRPC_DEBUG << "filter: " << filter << "\n";

    std::vector<Log> logs;

    auto tx = co_await database_->begin();

    try {
        ethdb::kv::TransactionDatabase tx_database{*tx};

        uint64_t start{}, end{};
        if (filter.block_hash.has_value()) {
            auto block_hash_bytes = silkworm::from_hex(filter.block_hash.value());
            if (!block_hash_bytes.has_value()) {
                auto error_msg = "invalid eth_getLogs filter block_hash: " + filter.block_hash.value();
                SILKRPC_ERROR << error_msg << "\n";
                reply = make_json_error(request["id"], 100, error_msg);
                co_await tx->close(); // RAII not (yet) available with coroutines
                co_return;
            }
            auto block_hash = silkworm::to_bytes32(block_hash_bytes.value());
            auto block_number = co_await core::rawdb::read_header_number(tx_database, block_hash);
            start = end = block_number;
        } else {
            auto latest_block_number = co_await core::get_latest_block_number(tx_database);
            start = filter.from_block.value_or(latest_block_number);
            end = filter.to_block.value_or(latest_block_number);
        }
        SILKRPC_DEBUG << "start block: " << start << " end block: " << end << "\n";

        Roaring block_numbers;
        block_numbers.addRange(start, end + 1);

        SILKRPC_DEBUG << "block_numbers.cardinality(): " << block_numbers.cardinality() << "\n";

        if (filter.topics.has_value()) {
            auto topics_bitmap = co_await get_topics_bitmap(tx_database, filter.topics.value(), start, end);
            SILKRPC_TRACE << "topics_bitmap: " << topics_bitmap.toString() << "\n";
            if (topics_bitmap.isEmpty()) {
                block_numbers = topics_bitmap;
            } else {
                block_numbers &= topics_bitmap;
            }
        }
        SILKRPC_DEBUG << "block_numbers.cardinality(): " << block_numbers.cardinality() << "\n";
        SILKRPC_TRACE << "block_numbers: " << block_numbers.toString() << "\n";

        if (filter.addresses.has_value()) {
            auto addresses_bitmap = co_await get_addresses_bitmap(tx_database, filter.addresses.value(), start, end);
            if (addresses_bitmap.isEmpty()) {
                block_numbers = addresses_bitmap;
            } else {
                block_numbers &= addresses_bitmap;
            }
        }
        SILKRPC_DEBUG << "block_numbers.cardinality(): " << block_numbers.cardinality() << "\n";
        SILKRPC_TRACE << "block_numbers: " << block_numbers.toString() << "\n";

        if (block_numbers.cardinality() == 0) {
            reply = make_json_content(request["id"], logs);
            co_await tx->close(); // RAII not (yet) available with coroutines
            co_return;
        }

        std::vector<uint32_t> block_number_vector{};
        block_number_vector.reserve(uint32_t(block_numbers.cardinality()));
        SILKRPC_DEBUG << "block_number_vector vector size: " << block_number_vector.size() << "\n";
        block_numbers.toUint32Array(block_number_vector.data());
        for (auto block_to_match : block_numbers) {
            SILKRPC_DEBUG << "block_to_match: " << block_to_match << "\n";
            auto block_hash = co_await core::rawdb::read_canonical_block_hash(tx_database, uint64_t(block_to_match));
            SILKRPC_DEBUG << "block_hash: " << silkworm::to_hex(block_hash) << "\n";
            if (block_hash == evmc::bytes32{}) {
                reply = make_json_content(request["id"], logs);
                co_await tx->close(); // RAII not (yet) available with coroutines
                co_return;
            }

            auto receipts = co_await get_receipts(tx_database, uint64_t(block_to_match), block_hash);
            SILKRPC_DEBUG << "receipts.size(): " << receipts.size() << "\n";
            std::vector<Log> unfiltered_logs{};
            unfiltered_logs.reserve(receipts.size());
            for (auto receipt : receipts) {
                SILKRPC_DEBUG << "receipt.logs.size(): " << receipt.logs.size() << "\n";
                unfiltered_logs.insert(unfiltered_logs.end(), receipt.logs.begin(), receipt.logs.end());
            }
            SILKRPC_DEBUG << "unfiltered_logs.size(): " << unfiltered_logs.size() << "\n";
            auto filtered_logs = filter_logs(unfiltered_logs, filter);
            SILKRPC_DEBUG << "filtered_logs.size(): " << filtered_logs.size() << "\n";
            logs.insert(logs.end(), filtered_logs.begin(), filtered_logs.end());
        }
        SILKRPC_INFO << "logs.size(): " << logs.size() << "\n";

        reply = make_json_content(request["id"], logs);
    } catch (const std::exception& e) {
        SILKRPC_ERROR << "exception: " << e.what() << " processing request: " << request.dump() << "\n";
        reply = make_json_error(request["id"], 100, e.what());
    } catch (...) {
        SILKRPC_ERROR << "unexpected exception processing request: " << request.dump() << "\n";
        reply = make_json_error(request["id"], 100, "unexpected exception");
    }

    co_await tx->close(); // RAII not (yet) available with coroutines
    co_return;
}

asio::awaitable<Roaring> EthereumRpcApi::get_topics_bitmap(core::rawdb::DatabaseReader& db_reader, FilterTopics& topics, uint64_t start, uint64_t end) {
    SILKRPC_DEBUG << "#topics: " << topics.size() << " start: " << start << " end: " << end << "\n";
    Roaring result_bitmap;
    for (auto subtopics : topics) {
        SILKRPC_DEBUG << "#subtopics: " << subtopics.size() << "\n";
        Roaring subtopic_bitmap;
        for (auto topic : subtopics) {
            silkworm::Bytes topic_key{std::begin(topic.bytes), std::end(topic.bytes)};
            SILKRPC_TRACE << "topic: " << topic << " topic_key: " << silkworm::to_hex(topic) <<"\n";
            auto bitmap = co_await ethdb::bitmap::get(db_reader, silkworm::db::table::kLogTopicIndex.name, topic_key, start, end);
            SILKRPC_TRACE << "bitmap: " << bitmap.toString() << "\n";
            subtopic_bitmap |= bitmap;
            SILKRPC_TRACE << "subtopic_bitmap: " << subtopic_bitmap.toString() << "\n";
        }
        if (!subtopic_bitmap.isEmpty()) {
            if (result_bitmap.isEmpty()) {
                result_bitmap = subtopic_bitmap;
            } else {
                result_bitmap &= subtopic_bitmap;
            }
        }
        SILKRPC_DEBUG << "result_bitmap: " << result_bitmap.toString() << "\n";
    }
    co_return result_bitmap;
}

asio::awaitable<Roaring> EthereumRpcApi::get_addresses_bitmap(core::rawdb::DatabaseReader& db_reader, FilterAddresses& addresses, uint64_t start, uint64_t end) {
    SILKRPC_TRACE << "#addresses: " << addresses.size() << " start: " << start << " end: " << end << "\n";
    Roaring result_bitmap;
    for (auto address : addresses) {
        silkworm::Bytes address_key{std::begin(address.bytes), std::end(address.bytes)};
        auto bitmap = co_await ethdb::bitmap::get(db_reader, silkworm::db::table::kLogAddressIndex.name, address_key, start, end);
        SILKRPC_TRACE << "bitmap: " << bitmap.toString() << "\n";
        result_bitmap |= bitmap;
    }
    SILKRPC_TRACE << "result_bitmap: " << result_bitmap.toString() << "\n";
    co_return result_bitmap;
}

asio::awaitable<Receipts> EthereumRpcApi::get_receipts(core::rawdb::DatabaseReader& db_reader, uint64_t number, evmc::bytes32 hash) {
    auto cached_receipts = co_await core::rawdb::read_receipts(db_reader, hash, number);
    if (!cached_receipts.empty()) {
        co_return cached_receipts;
    }

    // If not already present, retrieve receipts by executing transactions
    //auto block = co_await core::rawdb::read_block(db_reader, hash, number);
    // TODO(canepat): implement
    SILKRPC_WARN << "retrieve receipts by executing transactions NOT YET IMPLEMENTED\n";
    co_return Receipts{};
}

std::vector<Log> EthereumRpcApi::filter_logs(std::vector<Log>& logs, const Filter& filter) {
    std::vector<Log> filtered_logs;

    auto addresses = filter.addresses;
    auto topics = filter.topics;
    SILKRPC_DEBUG << "filter.addresses: " << filter.addresses << "\n";
    for (auto log : logs) {
        SILKRPC_DEBUG << "log: " << log << "\n";
        if (addresses.has_value() && std::find(addresses.value().begin(), addresses.value().end(), log.address) == addresses.value().end()) {
            SILKRPC_DEBUG << "skipped log for address: 0x" << silkworm::to_hex(log.address) << "\n";
            continue;
        }
        auto matches = true;
        if (topics.has_value()) {
            if (topics.value().size() > log.topics.size()) {
                SILKRPC_DEBUG << "#topics: " << topics.value().size() << " #log.topics: " << log.topics.size() << "\n";
                continue;
            }
            for (size_t i{0}; i < topics.value().size(); i++) {
                SILKRPC_DEBUG << "log.topics[i]: " << log.topics[i] << "\n";
                auto subtopics = topics.value()[i];
                auto matches_subtopics = subtopics.empty(); // empty rule set == wildcard
                SILKRPC_TRACE << "matches_subtopics: " << std::boolalpha << matches_subtopics << "\n";
                for (auto topic : subtopics) {
                    SILKRPC_DEBUG << "topic: " << topic << "\n";
                    if (log.topics[i] == topic) {
                        matches_subtopics = true;
                        SILKRPC_TRACE << "matches_subtopics: " << matches_subtopics << "\n";
                        break;
                    }
                }
                if (!matches_subtopics) {
                    SILKRPC_TRACE << "No subtopic matches\n";
                    matches = false;
                    break;
                }
            }
        }
        SILKRPC_DEBUG << "matches: " << matches << "\n";
        if (matches) {
            filtered_logs.push_back(log);
        }
    }
    return filtered_logs;
}

} // namespace silkrpc::commands
