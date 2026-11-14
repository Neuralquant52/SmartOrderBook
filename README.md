Before any further advancement, read this thread : 
https://x.com/localminimaa/status/2034630639664652465

# SmartOrderBook

Real-time market microstructure engine in modern C++20. It streams live
Binance Futures trades and order book data, and computes microstructure
metrics and execution signals concurrently on every update.

## Features

| Metric | What it gives you |
|--------|-------------------|
| **Kyle's lambda** | Price impact per unit of signed volume |
| **VPIN** | Volume-synchronized probability of informed trading (toxicity) |
| **Hawkes process** | Trade arrival intensity and branching ratio (self-excitation) |
| **Avellaneda-Stoikov (generateASBidAsk)** | Optimal bid/ask quotes given inventory and risk aversion |
| **Almgren-Chriss (smartFillCalculator)** | Optimal execution schedule for a parent order|

## Architecture

```
 Binance WS (trades)  ──► TimeAndSales ──► Buffer<Trade>  ──┐
                                                             ├─► SmartOB ──► SmartTick
 Binance WS (depth)   ──► Orderbook    ──► Buffer<obTick> ──┘      │
                                                                    ├─ worker: Kyle's lambda
                                                                    ├─ worker: Hawkes
                                                                    ├─ worker: VPIN
                                                                    └─ worker: execution (A-C)
```

- One thread per stream and per metric, managed with `std::jthread` / `std::stop_token`
- Lock-free ring buffer (`Buffer<T, N>`, power-of-two size, masked indexing)
- Spinlock-protected `SmartTick` snapshot, readable from any thread
- Fetchers are pluggable: bring your own exchange (see below)

## Quick start

**Requirements:** C++20 compiler, Boost (Asio, Beast, JSON), OpenSSL.

```bash
git clone https://github.com/Neuralquant52/SmartOrderBook.git
cd SmartOrderBook
g++ -std=c++20 -O2 -I. main.cpp -o main -lssl -lcrypto -lpthread
./main
```

## Usage

```cpp
Orderbook ob;
TimeAndSales tns;

SmartOB<2048, 10> sob(ob, tns);          // buffer size, order book levels
sob.fillSmartTick(kyleLambdaThreshold, vpinThreshold);

SmartTick t = sob.getTick();             // thread-safe snapshot
```

## Custom streams

See [`features/fetch/README.md`](features/fetch/README.md).

## Design choices

- **Why a ring buffer?** Fixed memory, no allocation on the hot path
- **Why one thread per metric?** Metrics have very different costs
  (Hawkes fitting vs. VPIN), so a slow one never delays the others
- *(add 1 or 2 of your own, this is what recruiters read)*

## Limitations / roadmap

- Binance Futures only (ETHUSDT hardcoded for now)
- Busy-wait workers (candidates for condition variables)
- No backtesting layer yet

## References

- Kyle (1985), *Continuous Auctions and Insider Trading*
- Easley, López de Prado, O'Hara (2012), *Flow Toxicity and Liquidity*
- Avellaneda & Stoikov (2008), *High-frequency trading in a limit order book*
- Almgren & Chriss (2000), *Optimal execution of portfolio transactions*
- Inspired by [this post](https://x.com/localminimaa/status/2034630639664652465)

## License

MIT
