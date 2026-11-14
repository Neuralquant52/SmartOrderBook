#include <array>
#include <atomic>
#include <boost/asio/buffer.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/context_base.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/json.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <cstddef>
#include <iostream>
#include "../data/ticks.hpp"
#include <openssl/tls1.h>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#pragma once



namespace net = boost::asio; namespace ssl = net::ssl; namespace ws = boost::beast::websocket; namespace json = boost::json;



class Orderbook {
public:

	auto create_ws_connection(const std::string& host, const std::string& path, const std::string& msg){
		ctx.set_default_verify_paths();
		ctx.set_verify_mode(ssl::verify_peer);
		ws::stream<ssl::stream<net::ip::tcp::socket>> s{ioc, ctx};
		SSL_set_tlsext_host_name(s.next_layer().native_handle(), host.c_str());
		net::ip::tcp::resolver r{ioc};
		net::connect(boost::beast::get_lowest_layer(s), r.resolve(host, "443"));
		s.next_layer().handshake(ssl::stream_base::client);
		s.handshake(host, path);
		s.write(net::buffer(msg));return s;
	}

	void fetch(std::stop_token st, auto& Ticks, std::size_t lvl){
		std::string host = "fstream.binance.com";
		std::string path = "/stream";
		std::string msg = R"({"method" : "SUBSCRIBE", "params":["ethusdt@depth20"], "id": 1})";
		
		auto stream = create_ws_connection(host, path, msg);	
		try {
			while (!st.stop_requested()){
				obTick curr;

				boost::beast::flat_buffer buf;
				stream.read(buf);
				std::string raw = boost::beast::buffers_to_string(buf.data());
				json::value json_ = json::parse(raw);
				json::object const& obj = json_.as_object();
				if (obj.contains("data")){
					json::object data = obj.at("data").as_object();
					if (data.contains("b") && data.contains("a")){
						json::array const& bids = data.at("b").as_array();
						json::array const& asks = data.at("a").as_array();
						for (size_t i{}; i < lvl; i++){
							curr.asks[std::stod(asks[i].as_array()[0].as_string().c_str())] = std::stod(asks[i].as_array()[1].as_string().c_str());

							curr.bids[std::stod(bids[i].as_array()[0].as_string().c_str())] = std::stod(bids[i].as_array()[1].as_string().c_str());
						}
					}
					curr.ts = data.at("T").as_int64(); 
					Ticks->push(curr);
				}
			}
		} catch(boost::beast::system_error& e){
			std::cout << "WS Orderbook Fetcher error \n"; 	
		}
	}

private:
	net::io_context ioc;
	ssl::context ctx{ssl::context::tlsv12_client};	
	std::size_t size;

};
