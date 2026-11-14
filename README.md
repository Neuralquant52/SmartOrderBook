Before any further advancement, read this thread :
https://x.com/localminimaa/status/2034630639664652465

# SmartOrderBook

Real-time market microstructure engine in modern C++20. It streams live
Binance Futures trades and order book data, and computes microstructure
metrics and execution signals concurrently on every update.

Built by [Neuralquant52](https://github.com/Neuralquant52) and [dpintodebrito](https://github.com/dpintodebrito).

## Features

| Metric | What it gives you |
|--------|-------------------|
| **Kyle's lambda** | Price impact per unit of signed volume |
| **VPIN** | Volume-synchronized probability of informed trading (toxicity) |
| **Hawkes process** | Trade arrival intensity and branching ratio (self-excitation) |
| **Avellaneda-Stoikov** (`generateASBidAsk`) | Optimal bid/ask quotes given inventory and risk aversion |
| **Almgren-Chriss** (`smartFillCalculator`) | Optimal execution schedule for a parent order |

## Architecture

```
 Binance WS (trades)  ──► TimeAndSales ──► Buffer<Trade>  ──┐
                                                             ├─► SmartOB ──► SmartTick
 Binance WS (depth)   ──► Orderbook    ──► Buffer<obTick> ──┘      │
                                                                    ├─ worker: Kyle's lambda
                                                                    ├─ worker: Hawkes
                                                                    └─ worker: VPIN
```

Kyle's lambda, Hawkes and VPIN run as background workers.
Avellaneda-Stoikov and Almgren-Chriss are called on demand.

- One thread per stream and per metric, managed with `std::jthread` / `std::stop_token`
- Fixed-size ring buffer (`Buffer<T, N>`, power-of-two size, masked indexing)
- Spinlock-protected `SmartTick` snapshot, readable from any thread
- Fetchers are pluggable: bring your own exchange (see below)

## Quick start

**Requirements:** C++20 compiler, Boost (Asio, Beast, JSON), OpenSSL.

On Ubuntu/Debian:

```bash
sudo apt install build-essential libboost-all-dev libssl-dev
```

Build and run:

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

// template parameters: buffer size (must be a power of two), order book levels
SmartOB<2048, 10> sob(ob, tns);
sob.fillSmartTick(kyleLambdaThreshold, vpinThreshold);

SmartTick t = sob.getTick();             // thread-safe snapshot
```

## Custom streams

See [`features/fetch/README.md`](features/fetch/README.md).

## Questions

- **Why did we build it?** We have both been interested in finance since a young age, and building this kind of project is a real passion for us. As a developer, it also let me apply many things I had learned (ring buffer, concurrent programming, spin-wait mutex).
- **What mistakes did we make while building it?** (NeuralQuant) Not rereading my notes. I could have learned much more by applying more of what was in them, especially on the threading side.
- **What would I do differently if I rewrote this code?** Plan first and structure the architecture with a tool like Excalidraw, because structuring the code was the hardest part. I would also gather all the resources before writing anything: I had to learn many math concepts along the way, as well as software design (e.g. *C++ Software Design*), which I should have read before starting.

## Limitations / roadmap

- Binance Futures only (ETHUSDT hardcoded for now)
- Busy-wait workers (candidates for condition variables)
- No backtesting layer yet

## References

- Kyle (1985), *Continuous Auctions and Insider Trading*
- Easley, López de Prado, O'Hara (2012), *Flow Toxicity and Liquidity*
- Avellaneda & Stoikov (2008), *High-frequency trading in a limit order book*
- Almgren & Chriss (2000), *Optimal execution of portfolio transactions*
- Inspired by [this thread](https://x.com/localminimaa/status/2034630639664652465)

## Authors

- [Neuralquant52](https://github.com/Neuralquant52)
- [dpintodebrito](https://github.com/dpintodebrito)

## License

MIT
