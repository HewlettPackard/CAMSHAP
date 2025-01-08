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

#include "router.h"

using namespace SST;
using namespace SST::CAMSHAP;

/**
* @brief Main constructor for router
* @details Read parameters, configure output, register clock handler, and configure links.
*/
router::router(ComponentId_t id, Params &params) : Component(id) {
    
    /* Read parameters */
    uint32_t verbose        = params.find<uint32_t>("verbose", 0);
    uint32_t mask           = params.find<uint32_t>("mask", 0);
    std::string name        = params.find<std::string>("name");
    UnitAlgebra freq        = params.find<UnitAlgebra>("freq", "1GHz");
    latency                 = params.find<uint32_t>("latency", 1);
    numPort                 = params.find<uint32_t>("numPort", 1);

    /* Configure output (outStd: command prompt, outFile: txt file) */
    std::string outputDir   = params.find<std::string>("outputDir");
    std::string prefix = "@t ["+name+"]:";
    outStd.init(prefix, verbose, mask, Output::STDOUT);
    outFile.init("@t ", verbose, mask, Output::FILE, outputDir+name+".txt");

    /* Register clock handler */ 
    clockHandler    = new Clock::Handler<router>(this, &router::clockTick);
    clockPeriod     = registerClock(freq, clockHandler);

    /* Configure links */
    fromUpLink      = configureLink("fromUpPort", new Event::Handler<router>(this, &router::handleUp));
    toUpLink        = configureLink("toUpPort");

    for (uint32_t i = 0; i < numPort; ++i){
        fromDownPort.push_back(new Port(i, clockPeriod, this));
        fromDownLink.push_back(configureLink("fromDownPort"+std::to_string(i), new Event::Handler<Port>(fromDownPort.back(), &Port::handleDown)));

        toDownLink.push_back(configureLink("toDownPort"+std::to_string(i)));
    }
    dataLink        = configureSelfLink("data", freq, new Event::Handler<router>(this, &router::handleData));
    resultLink      = configureSelfLink("result", freq, new Event::Handler<router>(this, &router::handleResult));

    /* Register statistics */
    activeCycleFPU  = registerStatistic<uint32_t>("activeCycleFPU");
    activeCycleMUX  = registerStatistic<uint32_t>("activeCycleMUX");
}

void
router::handleUp(Event *ev) {
    CAMSHAPEvent *dataEv = static_cast<CAMSHAPEvent*>(ev);
    fromUpQueue.push(getNextClockCycle(clockPeriod), 0, dataEv);
}

void
router::Port::handleDown(Event *ev) {
    CAMSHAPEvent *resultEv = static_cast<CAMSHAPEvent*>(ev);
    portQueue.push(m_router->getNextClockCycle(clockPeriod), 0, resultEv);
}

void
router::handleResult(Event *ev) {
    CAMSHAPEvent *resultEv = static_cast<CAMSHAPEvent*>(ev);
    std::vector<uint8_t> result = resultEv->getPayload();
    uint32_t size = result.size();
    float_t sum = 0.0;
    for (uint32_t i = 0 ; i < size; i=i+4){
        outFile.verbose(CALL_INFO, 1, (1<<2), "%10s - %5s %5s %5s %5s = %.5f\n", "Input result", std::to_string(result[i+3]).c_str(), std::to_string(result[i+2]).c_str(), std::to_string(result[i+1]).c_str(), std::to_string(result[i]).c_str(), convertINTtoFP32(result[i], result[i+1], result[i+2], result[i+3]));
        sum += convertINTtoFP32(result[i], result[i+1], result[i+2], result[i+3]);
        activeCycleFPU->addData(1);
    }
    uint32_t sumFP32 = convertFP32toINT(sum);
    std::vector<uint8_t> sumResult(4,0);
    sumResult[3] = (sumFP32 >> 24) & 0xff;
    sumResult[2] = (sumFP32 >> 16) & 0xff;
    sumResult[1] = (sumFP32 >> 8) & 0xff;
    sumResult[0] = sumFP32 & 0xff;
    outFile.verbose(CALL_INFO, 1, (1<<2), "%10s - %5s %5s %5s %5s = %.5f\n", "Sum Result", std::to_string(sumResult[3]).c_str(), std::to_string(sumResult[2]).c_str(), std::to_string(sumResult[1]).c_str(), std::to_string(sumResult[0]).c_str(), sum);
    CAMSHAPEvent *sumEv = new CAMSHAPEvent(resultEv->getOpcode(), resultEv->getDst(), resultEv->getSrc1(), resultEv->getSrc2(), resultEv->getImm(), sumResult);
    toUpLink->send(sumEv);
    busy_result = false;
    delete ev;
}

void
router::handleData(Event *ev) {
    CAMSHAPEvent *dataEv = static_cast<CAMSHAPEvent*>(ev);
    for (uint32_t i = 0; i < numPort; ++i){
        CAMSHAPEvent *dataEvClone = dataEv->clone();
        toDownLink[i]->send(dataEvClone);
        activeCycleMUX->addData(1);
    }
    outFile.verbose(CALL_INFO, 1, (1<<2), "%10s - %5s %5s %5s %5s\n", "Data", std::to_string(dataEv->getPayload()[3]).c_str(), std::to_string(dataEv->getPayload()[2]).c_str(), std::to_string(dataEv->getPayload()[1]).c_str(), std::to_string(dataEv->getPayload()[0]).c_str());
    busy_data = false;
    delete ev;
}

bool
router::clockTick(Cycle_t cycle) {

    if (!busy_result){
        uint32_t opcode, dst, src1, src2;
        uint32_t imm = UINT32_MAX;
        bool anyIn = false;
        std::vector<uint8_t> resultMat;
        for (uint32_t i = 0; i < 2*numPort; ++i){
            CAMSHAPEvent *fromEv = static_cast<CAMSHAPEvent*>(fromDownPort[i/2]->getEvent(cycle));
            if (fromEv){
                busy_result = true;
                anyIn = true;
                if (imm == fromEv->getImm()){
                    resultMat.push_back(fromEv->getPayload()[0]);
                    resultMat.push_back(fromEv->getPayload()[1]);
                    resultMat.push_back(fromEv->getPayload()[2]);
                    resultMat.push_back(fromEv->getPayload()[3]);
                }
                else if (imm == UINT32_MAX){
                    opcode = fromEv->getOpcode();
                    src1 = fromEv->getSrc1();
                    src2 = fromEv->getSrc2();
                    imm = fromEv->getImm();
                    dst = fromEv->getDst();
                    resultMat.push_back(fromEv->getPayload()[0]);
                    resultMat.push_back(fromEv->getPayload()[1]);
                    resultMat.push_back(fromEv->getPayload()[2]);
                    resultMat.push_back(fromEv->getPayload()[3]);
                }
                else{
                    CAMSHAPEvent *resultMatEv = new CAMSHAPEvent(opcode, dst, src1, src2, imm, resultMat);
                    resultLink->send(latency-1, resultMatEv);
                    opcode = fromEv->getOpcode();
                    src1 = fromEv->getSrc1();
                    src2 = fromEv->getSrc2();
                    imm = fromEv->getImm();
                    dst = fromEv->getDst();
                    resultMat.clear();
                    resultMat.push_back(fromEv->getPayload()[0]);
                    resultMat.push_back(fromEv->getPayload()[1]);
                    resultMat.push_back(fromEv->getPayload()[2]);
                    resultMat.push_back(fromEv->getPayload()[3]);
                }
                delete fromEv;
            }
        }
        if (anyIn){
            CAMSHAPEvent *resultMatEv = new CAMSHAPEvent(opcode, dst, src1, src2, imm, resultMat);
            resultLink->send(latency-1, resultMatEv);
        }
    }

    if (!busy_data){
        CAMSHAPEvent *dataEv = static_cast<CAMSHAPEvent*>(fromUpQueue.pop(cycle));
        if (dataEv){
            busy_data = true;
            dataLink->send(latency-1, dataEv);
        }
    }
    return false; 
}

float_t
router::convertINTtoFP32(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth){
    uint32_t value = (fourth << 24) + (third << 16) + (second << 8) + (first);
    float_t* fp = reinterpret_cast<float_t*>(&value);
    return *fp;
}

uint32_t
router::convertFP32toINT(float_t value){
    uint32_t* fp32 = reinterpret_cast<uint32_t*>(&value);
    return *fp32;
}