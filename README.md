# Bi-directional Coroutine Library for C++

Has Boost.Context as a dependency.

You should be able to build the example program with a command like this (adjusted for your compiler of choice + library locations):
`clang++ -std=c++11 -Iinclude/ -I/opt/local/include/ -L/opt/local/lib -lboost-context_mt main.cpp -o bidirectional-coroutines`

This library provides asymmetric coroutines which provide for bidirectional communication between the parent and child routines. 
