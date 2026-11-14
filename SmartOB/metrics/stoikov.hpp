#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <utility>
#include <span>
#include <map>
#include "../../features/data/ticks.hpp"
#include "hawkesProcess.hpp"

namespace AvellanedaStoikov{
	std::pair<double, double> generate(std::span<obTick> ob, std::span<Trade> trades, double q, double y){
		auto [lastAsk, _] = *ob.back().asks.begin();
		auto [lastBid, _] = *ob.back().bids.begin();

		double s = (lastBid + lastAsk) / 2; 
		double D = trades.back().ts - trades.front().ts;

		double avg{};
		double var{};
		for (int i{1}; i < trades.size(); i++){
			double delta = trades[i].price - trades[i-1].price;
			double old = delta - avg;
			avg += (delta - avg) / i; 
			var += old * (delta - avg);
		}
		
		double stdDev = (var / (trades.size())) * (trades.size() / (hawkesProcess::msToSeconds(trades.back().ts - trades.front().ts)));
		

		double r = s - q * y * stdDev * 60;
		
		std::map<double, int> deltas;
		std::map<double, double> Ys;
		for (double i{0.01}; i < 0.11; i+= 0.01){
			deltas[i] = 0;
			Ys[i] = 0;
		}

		int i{}, j{};

		while (i < trades.size()){
			if (trades[i].ts < ob.front().ts) {
				i++;
				continue;
			}

			while (j < ob.size()-1 && ob[j+1].ts < trades[i].ts) j++;
			
			auto [ask, _] = *ob[j].asks.begin();
			auto [bid, _] = *ob[j].bids.begin();

			double mid_i = (bid + ask) / 2;
			
			double d_i = std::abs(trades[i].price - mid_i);

			for (auto it = deltas.begin(); it != deltas.end(); it++)
				if (d_i >= it->first) it->second++;
			i++;
		}

		std::for_each(deltas.begin(), deltas.end(), [&](auto curr){
			if (curr.second >= 10) Ys[curr.first] = std::log(curr.second / D);
			else Ys.erase(curr.first);
		});

		std::size_t m = Ys.size();
		
		if (m < 5) return std::make_pair(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
		
		double xAvg = std::accumulate(Ys.begin(), Ys.end(), 0.0, [](double init, auto first){return init + first.first;}) / m;
		double yAvg = std::accumulate(Ys.begin(), Ys.end(), 0.0, [](double init, auto first){return init + first.second;}) / m;

		double sxy{}, sxx{};
		for (auto it = Ys.begin(); it != Ys.end(); it++){
			sxy += (it->first - xAvg) * (it->second - yAvg);
			sxx += std::pow(it->first - xAvg, 2);
		}
		
		
		double k = sxx != 0 ? -(sxy / sxx) : -1;
		
		
		if (k <= 0) return std::make_pair(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
		
		double L = 1/y * std::log(1 + y / k);
		double dBid = (s - r) + L;
		double dAsk = (r - s) + L;
		
		return std::make_pair(s - dBid, s + dAsk);
	}
}
