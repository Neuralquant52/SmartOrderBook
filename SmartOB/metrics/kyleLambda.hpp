#include "../../features/data/ticks.hpp"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <span>
#include <utility>
#include <vector>
#pragma once

namespace kyleLambda {
	double generate(std::span<Trade> ticks, std::span<std::pair<std::size_t, std::size_t>> windows, std::size_t winSize){
		std::for_each(ticks.begin(), ticks.end(), [](Trade& tick){tick.signedQty = tick.isAsk ? -tick.signedQty : tick.signedQty;});
		
		double productSum{};
		double squaredGaps{};
		
		std::vector<double> qtys(winSize);
		std::vector<double> spreads(winSize);
		
		std::size_t idx{};
		for (const auto& [begin, end] : windows){
			double winQty = std::accumulate(ticks.begin() + begin, ticks.begin() + end, 0.0, [](double init, const Trade& tick){
						return init + tick.signedQty;
					});

			double winSpread = ticks[end-1].price - ticks[begin].price;

			qtys[idx] = winQty;
			spreads[idx] = winSpread;
			++idx;
		}
	
		if (winSize <= 0) return std::numeric_limits<double>::quiet_NaN();

		double avgQty = std::accumulate(qtys.begin(), qtys.end(), 0.0) / winSize;
		double avgSpread = std::accumulate(spreads.begin(), spreads.end(), 0.0) / winSize;
		
		for (std::size_t i{}; i < winSize; i++){
			productSum += (qtys[i] - avgQty) * (spreads[i] - avgSpread);
			squaredGaps += std::pow(qtys[i] - avgQty, 2);
		}

		double cov = productSum / winSize;
		double var = squaredGaps / winSize;
		
		return cov / var;
	}
}
