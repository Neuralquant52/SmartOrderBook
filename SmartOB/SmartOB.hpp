#include <format>
#include "../features/core/buffer.hpp"
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <set>
#include "../features/fetch/obFetcher.hpp"
#include "../features/fetch/tnsFetcher.hpp"
#include "metrics/hawkesProcess.hpp"
#include "metrics/kyleLambda.hpp"
#include "metrics/vpin.hpp"
#include "metrics/stoikov.hpp"
#include "metrics/almgrenChriss.hpp"
#include "../features/data/ticks.hpp"
#include "data/SmartTick.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <span>
#include <stop_token>
#include <utility>
#include "../features/core/mutex.hpp"

template <std::size_t size_, std::size_t lvl>
class SmartOB {
public:


	SmartOB(Orderbook& obStream, TimeAndSales& tradeStream){
		workers.reserve(8);

		workers.emplace_back([&, this](std::stop_token st){
				tradeStream.fetch(st, trades);
		});

		workers.emplace_back([&, this](std::stop_token st){
				obStream.fetch(st, ob, lvl);
		});

	}

	void fillSmartTick(
		double kyleLambda_threshold,
		double VPIN_threshold
	){
		workers.emplace_back([this](std::stop_token st){
			int last{};
			while (!st.stop_requested()){
			
				while (!st.stop_requested() && ob->size()  == last){}

				if (trades->empty() || ob->empty()){
					std::this_thread::yield();
					continue;
				}

				obTick curr = ob->back();
				Trade lt = trades->back();

				last = ob->size();
				
				{
					std::scoped_lock lock(mtx);
					
					tick.ts = curr.ts;
					tick.bestAsk = *curr.asks.begin();
					tick.bestBid = *curr.bids.begin();
					
					tick.lastTrade = trades->back();
				}

				std::replace(work.begin(), work.end(), false, true);
			}
		});

		workers.emplace_back([=, this](std::stop_token st){
		    std::size_t lastSeen{};
		
		    while (!st.stop_requested()){
				if (!work[0].load(std::memory_order_acquire)) {
				    std::this_thread::yield();
				    continue;
				}
			
				if (trades->totalPushed() == lastSeen) {
				    work[0].store(false, std::memory_order_release);
				    continue;
				}
			
				auto snapshot = trades->snapshot();
				lastSeen = trades->totalPushed();
			
				if (snapshot.size() < 2) {
				    work[0].store(false, std::memory_order_release);
				    continue;
				}
			
				std::vector<std::pair<std::size_t, std::size_t>> windows;
				std::size_t start = 0;
				double curr_ = 0.0;
			
				for (std::size_t i = 0; i < snapshot.size(); ++i){
				    curr_ += snapshot[i].quantity * snapshot[i].price;
				    if (curr_ >= kyleLambda_threshold){
					windows.emplace_back(start, i + 1);
					start = i + 1;
					curr_ = 0.0;
				    }
				}
			
				if (!windows.empty()){
				    double lambda = kyleLambda::generate(snapshot, windows, windows.size());
				    std::scoped_lock lock(mtx);
				    tick.kyleLambda = lambda;
				}
			
				work[0].store(false, std::memory_order_release);
		    }
		});

		workers.emplace_back([this](std::stop_token st){
						
			std::size_t last{};

			while (!st.stop_requested()){
				if (!work[1].load(std::memory_order_acquire)){
					std::this_thread::yield();
					continue;
				}
			
				if (trades->empty() || trades->size() == last) {
					work[1].store(false, std::memory_order_release);
					continue;
				}
				
				auto snapshot = trades->snapshot();
				auto [hawkes, branchingRatio] = hawkesProcess::generate(snapshot);
				
				{
					std::scoped_lock lock(mtx);
					tick.hawkesProcess = hawkes, tick.branchingRatio = branchingRatio;
				}
				
				work[1].store(false, std::memory_order_release);
				last = trades->size();
			}
		});


		
		workers.emplace_back([=, this](std::stop_token st){
			std::size_t last{};
			while (!st.stop_requested()){
				if (!work[2].load(std::memory_order_acquire)){
					std::this_thread::yield();
					continue;
				}

				if (trades->empty() || trades->size() == last) {
					work[2].store(false, std::memory_order_release);
					continue;
				}
				
				auto snapshot = trades->snapshot();
				double VPIN = VPIN::generate(snapshot, VPIN_threshold);	
				
				{
					std::scoped_lock lock(mtx);
					tick.VPIN = VPIN;
				}

				work[2].store(false, std::memory_order_release);
				last = trades->size();
			}
					
		});
	}

	

	void generateASBidAsk(double inventory, double riskAversion){
		auto AS = AvellanedaStoikov::generate(std::span{*ob}.first(ob->size()), std::span{*trades}.first(trades->size()), inventory, riskAversion);
		
		{
			std::scoped_lock lock(mtx);
			tick.AS = AS;
		}
	}

	template <typename Func>
	void smartFillCalculator(Func fillFunction, double timeToComplete, double nSteps, double riskAversion, double qtyToFill){
		workers.emplace_back([=, this](std::stop_token st){
			using Clock = std::chrono::steady_clock;

			double currStep{};
			double filled{};

			auto nextDecision = Clock::now();
			const auto interval = std::chrono::duration<double>(timeToComplete / nSteps);

			while (currStep < nSteps){
				auto now = Clock::now();
				if (now < nextDecision) continue;

				nextDecision += std::chrono::duration_cast<Clock::duration>(interval);
				
				while (work[0].load(std::memory_order_acquire))  {};
							
				
				double lambda = tick.kyleLambda;
				
				auto obSnapshot = ob->snapshot();
				auto tradesSnapshot = trades->snapshot();
				
				auto qty = AlmgrenChriss::generate(obSnapshot, tradesSnapshot, qtyToFill - filled, timeToComplete, nSteps, lambda,  riskAversion, currStep);
		
				if (qty.has_value() && std::isfinite(*qty)){ 
					filled +=  *qty,
					fillFunction(*qty);
					currStep++;
				}
			}
		});

	}
	
	SmartTick getTick()  {
		std::scoped_lock lock(mtx);
		return tick;
	}

private:
	std::shared_ptr<Buffer<Trade, size_>> trades = std::make_shared<Buffer<Trade, size_>>();
	std::shared_ptr<Buffer<obTick, size_>> ob    = std::make_shared<Buffer<obTick, size_>>();	
	mutable SpinMtx mtx;
	std::array<std::atomic<bool>, 3> work = {false, false, false};
	SmartTick tick;
	std::vector<std::jthread> workers;
};
