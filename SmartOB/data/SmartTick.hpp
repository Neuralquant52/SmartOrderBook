#include <cstddef>
#include <utility>
#include "../../features/data/ticks.hpp"
#include <atomic>


constexpr std::size_t cacheLineSize = 64;
struct SmartTick {
	alignas(cacheLineSize) std::pair<double, double> bestBid;
	std::pair<double, double> bestAsk;
	Trade lastTrade;
	std::uint64_t ts;

	alignas(cacheLineSize) std::pair<double, double> AS;
	alignas(cacheLineSize) double VPIN;
	alignas(cacheLineSize) double kyleLambda ;
	
	alignas(cacheLineSize) double hawkesProcess;
	double branchingRatio;
};
