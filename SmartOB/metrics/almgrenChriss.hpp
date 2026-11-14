#include <vector>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <optional>
#include <span>
#include "hawkesProcess.hpp"
#include "../../features/data/ticks.hpp"

namespace AlmgrenChriss {
	std::optional<double> generate(std::span<obTick> ob, std::span<Trade> trades, double totalSize, double T, double N, double kyleLambda, double riskAversion, double currStep){
		double t = T / N;


		double avg{};
		double var{};
		for (int i{1}; i < trades.size(); i++){
			double delta = trades[i].price - trades[i-1].price;
			double old = delta - avg;
			avg += (delta - avg) / i; 
			var += old * (delta - avg);
		}
		
		double stdDev = (var / (trades.size())) * (trades.size() / (hawkesProcess::msToSeconds(trades.back().ts - trades.front().ts)));
		
		std::array<double, 8> Qs;
		std::vector<double> Cs;
		std::vector<double> n_s;
		n_s.reserve(ob.size());

		
		std::size_t currIdx{};
		
		double remainingSteps = N - currStep;
		if (remainingSteps <= 0) return totalSize;

		double expected = totalSize / remainingSteps;

		for (double i{0.5}; i < 2; i += 0.1875)
			Qs[currIdx++] = i * expected;

		auto regression = [&](int idx){
			double avgQ = std::accumulate(Qs.begin(), Qs.begin()+idx, 0.0) / idx;
			double avgC = std::accumulate(Cs.begin(), Cs.begin()+idx, 0.0) / idx;
			double sxy{}, sxx{};
			for (int i{}; i < idx; i++){
				sxy += ((Qs[i] - avgQ) * (Cs[i] - avgC));
				sxx += std::pow(Qs[i] - avgQ, 2);
			}
			return sxy / sxx;
		};

		for (const auto& tick : ob){
			auto [bestAsk, _] = *tick.asks.begin();		
			auto [bestBid, _] = *tick.bids.begin();
			double mid = (bestAsk + bestBid) / 2;
			
			for (int i{}; i < Qs.size(); i++){
				double rest = Qs[i], total = 0;

				for (auto [askPrice, askQty] : tick.asks){
					if (rest <= 0) break;
					double m = std::min(askQty, rest);
					total += askPrice * m; 
					rest -= m;
				}
				if (rest <= 0) Cs.push_back(total / Qs[i] - mid);
				
			}
			
			if (Cs.size() >= 3) n_s.push_back(regression(Cs.size()));	
			Cs.clear();
		}

		if (n_s.empty()) return std::nullopt;

		double n_  = [&](){
			auto mid = n_s.begin() + (n_s.size() / 2);
			std::nth_element(n_s.begin(), mid, n_s.end());

			if (n_s.size() % 2 != 0) return *mid;
			else {
				auto lower_mid = std::max_element(n_s.begin(), mid);
				return (*mid + *lower_mid) / 2.0;
			}
		}();
		
		double n = (n_ * t) - 1.0/2 * kyleLambda*t;
		
		if (!std::isfinite(n) || n <= 0.0 ||
		    !std::isfinite(stdDev) || stdDev < 0.0 ||
		    !std::isfinite(t) || t <= 0.0) {
		    return std::nullopt;
		}	
		
		double k_ = riskAversion * stdDev / n;
		double k = (1/t) * std::acosh(1 + 1.0/2 *k_ * std::pow(t, 2));
		
		if (totalSize <= 0.0) return 0.0;
		if (currStep >= N - 1) return totalSize;
		double remaining = totalSize * (1.0 - std::sinh(k * (T - (currStep+1)*t)) / std::sinh(k * (T - currStep*t)));
		return remaining;
	}
}
