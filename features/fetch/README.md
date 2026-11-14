# Custom streams

To plug in your own data source, write a fetcher with one of these signatures.

**Trades**

```cpp
void fetch(std::stop_token st, auto& Ticks) {
    // fill the ticks
}
```

**Order book**

```cpp
void fetch(std::stop_token st, auto& Ticks, std::size_t lvls) {
    // lvls = number of order book levels you want
    // fill the ticks
}
```

`Ticks` is a pointer to the buffer: call `Ticks->push(tick)` for each new
`Trade` (or `obTick`), and loop until `st.stop_requested()`.

See `tnsFetcher.hpp` and `obFetcher.hpp` for working Binance examples.
