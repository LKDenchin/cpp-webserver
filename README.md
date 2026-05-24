# LKWebServer

A high-concurrency HTTP server implemented from scratch in C++.

## Overview

This project is a teaching-oriented HTTP server built entirely in C++ without relying on heavy frameworks. It demonstrates core network programming concepts including:

- **Event-driven I/O multiplexing** using `epoll` for handling thousands of concurrent connections efficiently
- **Non-blocking sockets** with asynchronous I/O patterns
- **HTTP protocol parsing** and response generation
- **Coroutines / callback-based async design** for readable asynchronous code

## Features

- Lightweight and minimal — no third-party dependencies (except optional `fmt` for logging)
- Compatible with C++17 and above
- Built from the ground up to help developers understand the fundamentals of network programming and concurrent server design

## Getting Started

### Requirements

- Linux (epoll-based)
- C++17 or newer compiler
- CMake 3.10+

### Build

```bash
mkdir build && cd build
cmake ..
make
```

### Run

```bash
./server
```

## Project Structure

| File | Description |
|------|-------------|
| `server.cpp` | Main HTTP server implementation |
| `remake.cpp` | Network programming basics / experiments |
| `CMakeLists.txt` | Build configuration |

## Learning Goals

This project was created to help students who find coroutine libraries (like co_async) difficult to understand. By starting from scratch with C++17 callback-based async patterns, learners can focus on networking fundamentals without being blocked by complex framework abstractions or compiler version limitations.

