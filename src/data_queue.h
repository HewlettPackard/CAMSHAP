// MIT License
//
// Copyright (c) 2024 Hewlett Packard Enterprise Development LP
//		
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <sst/core/statapi/statbase.h>

#include <queue>
#include <cstdint>

namespace SST {
namespace CAMSHAP {

using namespace Statistics;

template<class T>
class Queue {
public:
    using Entry = std::pair<Cycle_t, T>;
    Queue()
    {
        nextAvailable = 0;
    }

    void push_out(Cycle_t current, Cycle_t delay, T obj) {

        if (delayQueue.empty()){
            if (nextAvailable <= current){
                delayQueue.push(std::make_pair(current + 0, obj));
                nextAvailable = current + delay;
            }
            else{
                delayQueue.push(std::make_pair(nextAvailable + 0, obj));
                nextAvailable += delay;
            }
        }
        else{
            auto back = delayQueue.back();
            delayQueue.push(std::make_pair(back.first + delay, obj));
        }
    }

    void push(Cycle_t current, Cycle_t delay, T obj) {
        if (delayQueue.empty()){
            delayQueue.push(std::make_pair(current + delay, obj));
        }
        else{
            auto back = delayQueue.back();
            delayQueue.push(std::make_pair(back.first + delay, obj));
        }
    }
    
    T pop(Cycle_t cycle) {
        T out = nullptr;
        if(!delayQueue.empty()) {
            auto entry = delayQueue.front();
            if(entry.first <= cycle) {
                delayQueue.pop();
                out = entry.second;
            }
        }
        return out;
    }
    Cycle_t nextAvailable;
    std::queue<Entry>   delayQueue;
};

}
}
