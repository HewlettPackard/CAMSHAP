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

#include "mpe.h"

using namespace SST;
using namespace SST::CAMSHAP;

/**
* @brief Main constructor for mpe
* @details Read parameters, configure output, register clock handler, and configure links.
*/
mpe::mpe(ComponentId_t id, Params &params) : Component(id) {
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
    clockHandler    = new Clock::Handler<mpe>(this, &mpe::clockTick);
    clockPeriod     = registerClock(freq, clockHandler);

    /* Configure links */
    outputLink      = configureLink("outputPort");
    responseLink    = configureLink("responsePort");
    requestLink     = configureLink("requestPort", new Event::Handler<mpe>(this, &mpe::handleRequest));
    dataLink        = configureLink("dataPort", new Event::Handler<mpe>(this, &mpe::handleData));
    selfLink        = configureSelfLink("selfLink", freq, new Event::Handler<mpe>(this, &mpe::handleSelf));

    /* Register statistics */
    activeCycle     = registerStatistic<uint32_t>("activeCycle");
}

/**
 * @brief Handle request event.
 * @details Add request event to request queue to handle it in the next clock cycle.
 */
void
mpe::handleRequest(Event *ev) {
    requestQueue.push(getNextClockCycle(clockPeriod), 0, static_cast<CAMSHAPCoreEvent*>(ev));
}

/**
 * @brief Handle data event.
 * @details Extract data from data event.
 */
void
mpe::handleData(Event *ev) {
    CAMSHAPCoreEvent *dataEv = static_cast<CAMSHAPCoreEvent*>(ev);
    data = dataEv->getPayload();
    delete dataEv;
}

/**
 * @brief Handle self event.
 * @details Extract data from self event and perform the corresponding operation.
 */
void
mpe::handleSelf(Event *ev){
    CAMSHAPCoreEvent *selfEv = static_cast<CAMSHAPCoreEvent*>(ev);
    switch (selfEv->getImm()){
        case 0:{ // Reset
            t.assign(numRow, 0);
            b.assign(numRow, 0);
            n.assign(numRow, 0);
            s.assign(numRow, 0);
            p.assign(numRow, 1);
            up.assign(numRow, 1);
            un.assign(numRow, 1);
            outFile.verbose(CALL_INFO, 4, (1<<9), "Reset\n");
            break;
        }
        case 1:{ // Update search results of test sample
            t = data;
            outFile.verbose(CALL_INFO, 4, (1<<9), "Test\n");
            break;
        }
        case 2:{ // Update search results of base sample
            b = data;
            outFile.verbose(CALL_INFO, 4, (1<<9), "Base\n");
            break;
        }
        case 3:{ // Update n, s, p based on t, b
            for (uint32_t l = 0; l < numRow; ++l){
                n[l] += ((t[l] ^ b[l]) & 0x1);
                s[l] += ((t[l] & ~b[l]) & 0x1);
                p[l] *= ((t[l] | b[l]) & 0x1);
            }
            outFile.verbose(CALL_INFO, 4, (1<<9), "NS\n");
            outFile.verbose(CALL_INFO, 4, (1<<9), "T: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", t[0], t[1], t[2], t[3]);
            outFile.verbose(CALL_INFO, 4, (1<<9), "B: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", b[0], b[1], b[2], b[3]);
            outFile.verbose(CALL_INFO, 4, (1<<9), "N: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", n[0], n[1], n[2], n[3]);
            outFile.verbose(CALL_INFO, 4, (1<<9), "S: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", s[0], s[1], s[2], s[3]);
            outFile.verbose(CALL_INFO, 4, (1<<9), "P: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", p[0], p[1], p[2], p[3]);
            break;
        }
        case 4:{ // Read NS and send NS to response port
            std::vector<uint8_t> ns(numRow, 0);
            for (uint32_t l = 0; l < numRow; ++l){
                ns[l] = ((n[l] & 0x7) << 3) | (s[l] & 0x7); 
            }
            CAMSHAPCoreEvent *nsEv = new CAMSHAPCoreEvent(0, 0, 0, ns); 
            responseLink->send(nsEv);
            outFile.verbose(CALL_INFO, 4, (1<<9), "rNS: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", ns[0], ns[1], ns[2], ns[3]);
            break;
        }
        case 5: { // Read UP and UN and send UP UN to output port
            for (uint32_t l = 0; l < numRow-1; ++l){
                up[l] = t[l] & ~b[l] & p[l];
                un[l] = ~t[l] & b[l] & p[l];
            }
            up[numRow-1] = 1;
            un[numRow-1] = 1;

            // Concatenate up un
            std::vector<uint8_t> upn;
            upn.reserve(up.size() + un.size());
            upn.insert(upn.end(), up.begin(), up.end());
            upn.insert(upn.end(), un.begin(), un.end());
            CAMSHAPCoreEvent *uEv = new CAMSHAPCoreEvent(0, 0, 0, upn); 
            outputLink->send(uEv);
            outFile.verbose(CALL_INFO, 4, (1<<9), "Sum(up): %3" PRIu32 ", Sum(un): %3" PRIu32 "\n", std::accumulate(up.begin(),up.end(),0), std::accumulate(un.begin(),un.end(),0));
            break;
        }
    }
    busy = false;
    activeCycle->addData(1);
    delete selfEv;
}

bool
mpe::clockTick(Cycle_t cycle) {
    if (!busy){
        auto requestEv = requestQueue.pop(cycle);
        if (requestEv){
            outFile.verbose(CALL_INFO, 4, (1<<9), "%10s - Opcode:%3" PRIu32 ", Dst:%8" PRIu32 ", Imm:%8" PRIu32 "\n", "In", static_cast<uint32_t>(requestEv->getOpcode()), requestEv->getDst(), requestEv->getImm());
            selfLink->send(latency - 1, requestEv);
            busy = true;
        }
    }
    return false; 
}

