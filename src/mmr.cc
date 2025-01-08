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

#include "mmr.h"

using namespace SST;
using namespace SST::CAMSHAP;

/**
* @brief Main constructor for mmr
* @details Read parameters, configure output, register clock handler, and configure links.
*/
mmr::mmr(ComponentId_t id, Params &params) : Component(id) {
    /* Read parameters */
    uint32_t verbose        = params.find<uint32_t>("verbose", 0);
    uint32_t mask           = params.find<uint32_t>("mask", 0);
    std::string name        = params.find<std::string>("name");
    UnitAlgebra freq        = params.find<UnitAlgebra>("freq", "1GHz");
    latency                 = params.find<uint32_t>("latency", 1);
    numRow                  = params.find<uint32_t>("numRow", 256);

    /* Configure output (outStd: command prompt, outFile: txt file) */
    std::string outputDir   = params.find<std::string>("outputDir");
    std::string prefix = "@t ["+name+"]:";
    outStd.init(prefix, verbose, mask, Output::STDOUT);
    outFile.init("@t ", verbose, mask, Output::FILE, outputDir+name+".txt");

    /* Register clock handler */ 
    clockHandler    = new Clock::Handler<mmr>(this, &mmr::clockTick);
    clockPeriod     = registerClock(freq, clockHandler);

    /* Configure links */
    responseLink    = configureLink("responsePort");
    requestLink     = configureLink("requestPort", new Event::Handler<mmr>(this, &mmr::handleRequest));
    dataLink        = configureLink("dataPort", new Event::Handler<mmr>(this, &mmr::handleData));
    selfLink        = configureSelfLink("self", freq, new Event::Handler<mmr>(this, &mmr::handleSelf));

}

/**
 * @brief Handle request event.
 * @details Add request event to request queue to handle it in the next clock cycle.
 */
void
mmr::handleRequest(Event *ev) {
    requestQueue.push(getNextClockCycle(clockPeriod), 0, static_cast<CAMSHAPCoreEvent*>(ev));
}

/**
 * @brief Handle data event.
 * @details Extract data from data event.
 */
void
mmr::handleData(Event *ev) {
    CAMSHAPCoreEvent *dataEv = static_cast<CAMSHAPCoreEvent*>(ev);
    data = dataEv->getPayload();
    up.assign(data.begin(), data.begin()+numRow);
    un.assign(data.begin()+numRow, data.end());
    delete dataEv;
}

/**
 * @brief Handle self event.
 * @details Extract data from self event, extract index of 1 from data, and send it to control_core.
 */
void
mmr::handleSelf(Event *ev){
    std::vector<uint8_t>::iterator it_up;
    std::vector<uint8_t>::iterator it_un;
    std::vector<uint8_t> mmr_out(16, 255);

    for (uint32_t i = 0; i < 8; i++){
        it_up = std::find(up.begin(), up.end(), 1);
        uint32_t index = it_up-up.begin();
        if (index == 255){
            break;
        }
        else{
            mmr_out[i] = index;
            up[mmr_out[i]] = 0;
        }
    }

    for (uint32_t i = 0; i < 8; i++){
        it_un = std::find(un.begin(), un.end(), 1);
        uint32_t index = it_un-un.begin();
        if (index == 255){
            break;
        }
        else{
            mmr_out[i+8] = index;
            un[mmr_out[i+8]] = 0;
        }
    }
    CAMSHAPCoreEvent *mmrEv = new CAMSHAPCoreEvent(0, 0, 0, mmr_out);
    responseLink->send(mmrEv);
    outFile.verbose(CALL_INFO, 4, (1<<10), "%10s - Pos: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", "Response", mmr_out[0], mmr_out[1], mmr_out[2], mmr_out[3], mmr_out[4], mmr_out[5], mmr_out[6], mmr_out[7]);
    outFile.verbose(CALL_INFO, 4, (1<<10), "%10s - Neg: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", "Response", mmr_out[8], mmr_out[9], mmr_out[10], mmr_out[11], mmr_out[12], mmr_out[13], mmr_out[14], mmr_out[15]);
    busy = false;
    delete ev;
}

bool
mmr::clockTick(Cycle_t cycle) {
    if (!busy){
        auto requestEv = requestQueue.pop(cycle);
        if (requestEv){
            outFile.verbose(CALL_INFO, 4, (1<<10), "%10s - Opcode:%3" PRIu32 ", Dst:%8" PRIu32 ", Imm:%8" PRIu32 "\n", "In", static_cast<uint32_t>(requestEv->getOpcode()), requestEv->getDst(), requestEv->getImm());
            selfLink->send(latency - 1, requestEv);
            busy = true;
        }
    }
    return false; 
}
