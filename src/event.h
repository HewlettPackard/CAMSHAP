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

#include <sst/core/event.h>

#include <cstdint>
#include <string>

#include "instruction.h"

namespace SST {
namespace CAMSHAP {

class CAMSHAPEvent : public Event {

public:
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & opcode;
        ser & dst;
        ser & src1;
        ser & src2;
        ser & imm;
        ser & payload;
    }    

    CAMSHAPEvent(uint32_t _opcode, uint32_t _dst, uint32_t _src1, uint32_t _src2, uint32_t _imm, std::vector<uint8_t> _payload) :
        Event(), opcode(_opcode), dst(_dst), src1(_src1), src2(_src2), imm(_imm), payload(_payload) { }

    CAMSHAPEvent *clone() override {
        return new CAMSHAPEvent(*this);
    }

    uint32_t getOpcode() const { return opcode;}
    uint32_t getDst() const { return dst;}
    uint32_t getSrc1() const { return src1;}
    uint32_t getSrc2() const { return src2;}
    uint32_t getImm() const { return imm;}
    std::vector<uint8_t>& getPayload() { return payload; }

private:
    CAMSHAPEvent()  {} // For Serialization only
    uint32_t opcode;
    uint32_t dst;
    uint32_t src1;
    uint32_t src2;
    uint32_t imm;
    std::vector<uint8_t> payload;

    ImplementSerializable(SST::CAMSHAP::CAMSHAPEvent);
};

class CAMSHAPCoreEvent : public Event {

public:
    void serialize_order(SST::Core::Serialization::serializer &ser)  override {
        Event::serialize_order(ser);
        ser & opcode;
        ser & dst;
        ser & imm;
        ser & payload;
    }    

    /**
    * @brief 
    */
    CAMSHAPCoreEvent(uint32_t _opcode, uint32_t _dst, uint32_t _imm, std::vector<uint8_t> _payload) :
        Event(), opcode(_opcode), dst(_dst), imm(_imm), payload(_payload){ }

    CAMSHAPCoreEvent *clone() override {
        return new CAMSHAPCoreEvent(*this);
    }

    uint32_t getOpcode() const { return opcode;}
    uint32_t getDst() const { return dst;}
    uint32_t getImm() const { return imm;}
    std::vector<uint8_t>& getPayload() { return payload; }

private:
    CAMSHAPCoreEvent()  {} // For Serialization only
    uint32_t opcode;
    uint32_t dst;
    uint32_t imm;
    std::vector<uint8_t> payload;

    ImplementSerializable(SST::CAMSHAP::CAMSHAPCoreEvent);
};


}
}
