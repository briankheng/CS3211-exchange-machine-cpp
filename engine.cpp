#include <iostream>
#include <thread>

#include "io.hpp"
#include "engine.hpp"

std::vector<uint32_t> OrderBook::handle_buy_order(ClientCommand& input)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::vector<uint32_t> output;

	while(input.count > 0 && !orders_sell.empty() && orders_sell.top().price <= input.price)
	{
		OrderBookType top = orders_sell.top();
		orders_sell.pop();

		if(top.count > input.count)
		{
			Output::OrderExecuted(top.order_id, input.order_id, ++top.exec_id, top.price, input.count, getCurrentTimestamp());
			top.count -= input.count;
			input.count = 0;
			orders_sell.push(top);
		}
		else
		{
			Output::OrderExecuted(top.order_id, input.order_id, ++top.exec_id, top.price, top.count, getCurrentTimestamp());
			input.count -= top.count;

			output.push_back(top.order_id);
		}
	}
	if(input.count > 0)
	{
		auto output_time = getCurrentTimestamp();
		orders_buy.push({input.order_id, 0, input.price, input.count, output_time});
		Output::OrderAdded(input.order_id, input.instrument, input.price, input.count, input.type == input_sell, output_time);
		
		output.push_back(input.order_id);
	}

	return output;
}

std::vector<uint32_t> OrderBook::handle_sell_order(ClientCommand& input)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::vector<uint32_t> output;
	
	while(input.count > 0 && !orders_buy.empty() && orders_buy.top().price >= input.price)
	{
		OrderBookType top = orders_buy.top();
		orders_buy.pop();

		if(top.count > input.count)
		{
			Output::OrderExecuted(top.order_id, input.order_id, ++top.exec_id, top.price, input.count, getCurrentTimestamp());
			top.count -= input.count;
			input.count = 0;
			orders_buy.push(top);
		}
		else
		{
			Output::OrderExecuted(top.order_id, input.order_id, ++top.exec_id, top.price, top.count, getCurrentTimestamp());
			input.count -= top.count;

			output.push_back(top.order_id);
		}
	}
	if(input.count > 0)
	{
		auto output_time = getCurrentTimestamp();
		orders_sell.push({input.order_id, 0, input.price, input.count, output_time});
		Output::OrderAdded(input.order_id, input.instrument, input.price, input.count, input.type == input_sell, output_time);
		
		output.push_back(input.order_id);
	}

	return output;
}

void OrderBook::handle_cancel_order(ClientCommand& input)
{
	std::lock_guard<std::mutex> lock(mtx);
	std::priority_queue<OrderBookType, std::vector<OrderBookType>, decltype(&cmp_buy)> new_orders_buy(cmp_buy);
	std::priority_queue<OrderBookType, std::vector<OrderBookType>, decltype(&cmp_sell)> new_orders_sell(cmp_sell);
	bool flag = false;

	while(!orders_buy.empty())
	{
		OrderBookType top = orders_buy.top();
		orders_buy.pop();
		if(top.order_id != input.order_id)
			new_orders_buy.push(top);
		else
		{
			flag = true;
			Output::OrderDeleted(input.order_id, true, getCurrentTimestamp());
		}
	}
	while(!orders_sell.empty())
	{
		OrderBookType top = orders_sell.top();
		orders_sell.pop();
		if(top.order_id != input.order_id)
			new_orders_sell.push(top);
		else
		{
			flag = true;
			Output::OrderDeleted(input.order_id, true, getCurrentTimestamp());
		}
	}
	orders_buy = new_orders_buy;
	orders_sell = new_orders_sell;

	if(!flag)
		Output::OrderDeleted(input.order_id, false, getCurrentTimestamp());
}

void Engine::accept(ClientConnection connection)
{
	auto thread = std::thread(&Engine::connection_thread, this, std::move(connection));
	thread.detach();
}

void Engine::connection_thread(ClientConnection connection)
{
	while(true)
	{
		ClientCommand input {};
		switch(connection.readInput(input))
		{
			case ReadResult::Error: SyncCerr {} << "Error reading input" << std::endl;
			case ReadResult::EndOfFile: return;
			case ReadResult::Success: break;
		}

		// Functions for printing output actions in the prescribed format are
		// provided in the Output class:
		switch(input.type)
		{
			case input_cancel: {
				SyncCerr {} << "Got cancel: ID: " << input.order_id << std::endl;
			
				id_to_orderbook_mtx.lock();
				if(id_to_orderbook.find(input.order_id) != id_to_orderbook.end())
				{
					auto orderbook = id_to_orderbook[input.order_id];
					id_to_orderbook.erase(input.order_id);
					id_to_orderbook_mtx.unlock();

					orderbook->handle_cancel_order(input);
				}
				else
				{
					id_to_orderbook_mtx.unlock();
					Output::OrderDeleted(input.order_id, false, getCurrentTimestamp());
				}
				break;
			}

			default: {
				SyncCerr {}
				    << "Got order: " << static_cast<char>(input.type) << " " << input.instrument << " x " << input.count << " @ "
				    << input.price << " ID: " << input.order_id << std::endl;

				if(input.type == input_buy)
				{
					instrument_to_orderbook_mtx.lock();
					if(instrument_to_orderbook.find(input.instrument) == instrument_to_orderbook.end())
					{
						instrument_to_orderbook[input.instrument] = new OrderBook();
					}
					auto orderbook = instrument_to_orderbook[input.instrument];
					instrument_to_orderbook_mtx.unlock();

					std::vector<uint32_t> result = orderbook->handle_buy_order(input);
					for(uint32_t id : result)
					{
						std::lock_guard<std::mutex> lock(id_to_orderbook_mtx);
						if(id == input.order_id)
							id_to_orderbook[input.order_id] = instrument_to_orderbook[input.instrument];
						else
							id_to_orderbook.erase(id);
					}
				}
				else
				{
					instrument_to_orderbook_mtx.lock();
					if(instrument_to_orderbook.find(input.instrument) == instrument_to_orderbook.end())
					{
						instrument_to_orderbook[input.instrument] = new OrderBook();
					}
					auto orderbook = instrument_to_orderbook[input.instrument];
					instrument_to_orderbook_mtx.unlock();

					std::vector<uint32_t> result = orderbook->handle_sell_order(input);
					for(uint32_t id : result)
					{
						std::lock_guard<std::mutex> lock(id_to_orderbook_mtx);
						if(id == input.order_id)
							id_to_orderbook[input.order_id] = instrument_to_orderbook[input.instrument];
						else
							id_to_orderbook.erase(id);
					}
				}	
				break;
			}
		}
	}
}
