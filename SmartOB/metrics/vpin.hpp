#include "../../features/data/ticks.hpp"
#include <cmath>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <iostream>
#include <vector>
#include <span>
#include <algorithm>


namespace VPIN{
	double generate(std::span<Trade> ticks, double V){
		if (ticks.size() < 3) return 0.0;

		double avg{};
		double var{};
		for (int i{1}; i < ticks.size(); i++){
			double delta = ticks[i].price - ticks[i-1].price;
			double old = delta - avg;
			avg += (delta - avg) / i; 
			var += old * (delta - avg);
		}


		double stdDev = std::sqrt(var / (ticks.size()-2));
		if (stdDev <= 1e-12 || !std::isfinite(stdDev))
			return std::numeric_limits<double>::quiet_NaN();
		
		auto Z = [](double x){
			return 0.5 * (1 + std::erf(x / std::sqrt(2.0)));
		};

		std::vector<double> S(ticks.size()), B(ticks.size());
		B[0] = S[0] = ticks[0].quantity / 2;
		

		for (int i{1}; i < ticks.size(); i++){
			B[i] = ticks[i].quantity * Z((ticks[i].price - ticks[i-1].price) / stdDev);
			S[i] = ticks[i].quantity * (1 - Z((ticks[i].price - ticks[i-1].price) / stdDev));
		}
		
		double fills{}, globalB{}, globalS{}, res{};
		std::vector<double> buckets;

		for (int i{}; i < ticks.size(); i++){
			fills += ticks[i].quantity;
			globalB += B[i]; globalS += S[i];
			
			if (fills >= V){
				buckets.push_back(std::abs(globalS - globalB) / fills);
				fills = globalB = globalS = 0;
			}
		}

		if (buckets.empty() || V <= 0.0)
		    return std::numeric_limits<double>::quiet_NaN();

		const size_t n = 50;
		size_t start = buckets.size() > n ? buckets.size() - n : 0;
		return std::accumulate(buckets.begin() + start, buckets.end(), 0.0) / (buckets.size() - start);
	}
}
