#include <cstdint>
#include <functional>
#include <map>
#pragma once

struct obTick{
	std::map<double, double, std::less<double>> asks;
	std::map<double, double, std::greater<double>> bids;

	std::int64_t ts;
};

struct Trade{
	
	std::int64_t ID;
	double price ;
	double quantity ;
	double signedQty;
	
	std::int64_t ts;
	bool isAsk;
};
