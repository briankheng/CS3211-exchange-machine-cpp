// This file contains declarations for the main Engine class. You will
// need to add declarations to this file as you develop your Engine.

#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <chrono>
#include <map>
#include <queue>
#include <mutex>

#include "io.hpp"

struct OrderBookType
{
	uint32_t order_id;
	uint32_t exec_id;
	uint32_t price;
	uint32_t count;
	intmax_t timestamp;
};

struct OrderBook
{
public:
	std::vector<uint32_t> handle_buy_order(ClientCommand& input);
	std::vector<uint32_t> handle_sell_order(ClientCommand& input);
	void handle_cancel_order(ClientCommand& input);

private:
	static bool cmp_buy(const OrderBookType& a, const OrderBookType& b)
	{
		if(a.price == b.price)
			return a.timestamp > b.timestamp;
		return a.price < b.price;
	}
	static bool cmp_sell(const OrderBookType& a, const OrderBookType& b)
	{
		if(a.price == b.price)
			return a.timestamp > b.timestamp;
		return a.price > b.price;
	}
	std::mutex mtx;
	std::priority_queue<OrderBookType, std::vector<OrderBookType>, decltype(&cmp_buy)> orders_buy{cmp_buy};
	std::priority_queue<OrderBookType, std::vector<OrderBookType>, decltype(&cmp_sell)> orders_sell{cmp_sell};
};

struct Engine
{
public:
	void accept(ClientConnection conn);

private:
	std::mutex instrument_to_orderbook_mtx;
	std::mutex id_to_orderbook_mtx;
	std::map<std::string, OrderBook*> instrument_to_orderbook;
	std::map<uint32_t, OrderBook*> id_to_orderbook;
	void connection_thread(ClientConnection conn);
};

inline std::chrono::microseconds::rep getCurrentTimestamp() noexcept
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#endif
