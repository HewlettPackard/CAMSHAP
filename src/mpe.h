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

#ifndef _MPE_COMPONENT_H
#define _MPE_COMPONENT_H

#include "event.h"
#include "data_queue.h"

#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include <sst/core/timeLord.h>
#include <sst/core/output.h>
#include <sst/core/unitAlgebra.h>

#include <iostream>
#include <vector>
#include <numeric>
#include <cmath>

namespace SST {
namespace CAMSHAP {
/**
* @brief Match Processing Element (MPE)
* @details 
*/
class mpe : public SST::Component {
public:
    /**
    * @brief Register a component.
    * @details SST_ELI_REGISTER_COMPONENT(class, “library”, “name”, version, “description”, category).
    */
    SST_ELI_REGISTER_COMPONENT(
        mpe,
        "camshap",
        "mpe",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Match Processing Element (MPE)",
        COMPONENT_CATEGORY_UNCATEGORIZED
    );
    /**
    * @brief List of parameters
    * @details SST_ELI_DOCUMENT_PARAMS({ “name”, “description”, “default value” }).
    */
    SST_ELI_DOCUMENT_PARAMS(
        {"verbose",             "(uint) Output verbosity. The higher verbosity, the more debug info", "0"},
        {"mask",                "(uint) Output mask", "0"},
        {"name",                "(string) Name of component"},
        {"freq",                "(UnitAlgebra) Clock frequency", "1GHz"},
        {"latency",             "(uint) Latency of component operation (handleSelf)", "1"},
        {"outputDir",           "(string) Path of output files", " "},
        {"numRow",              "(uint) Number of acam rows", "256"},
    );
    /**
    * @brief List of ports
    * @details SST_ELI_DOCUMENT_PORTS({ “name”, “description”, vector of supported events }).
    */
    SST_ELI_DOCUMENT_PORTS(
        {"outputPort",          "Output port",                      {"camshap.CAMSHAPCoreEvent"}},
        {"responsePort",        "Response port to control_core",    {"camshap.CAMSHAPCoreEvent"}},
        {"requestPort",         "Request port from control_core",   {"camshap.CAMSHAPCoreEvent"}},
        {"dataPort",            "Data port",                        {"camshap.CAMSHAPCoreEvent"}},
    );
    /**
    * @brief List of statistics
    * @details SST_ELI_DOCUMENT_STATISTICS({ “name”, “description”, “units”, enable level }).
    */
    SST_ELI_DOCUMENT_STATISTICS(
        { "activeCycle",        "Active cycles", "cycles", 1},
    );
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    );

    mpe(ComponentId_t id, Params& params);
    ~mpe() { }

    void handleRequest( SST::Event* ev );
    void handleData( SST::Event* ev );
    void handleSelf( SST::Event* ev );
    bool clockTick( Cycle_t cycle );

    void init( uint32_t phase ) {}
	void setup() { }
    void finish() { }

private:
    /** Clock *****************************************************************/
    Clock::Handler<mpe>             *clockHandler;
    TimeConverter                   *clockPeriod;

    /** IO ********************************************************************/
    Output                          outStd;
    Output                          outFile;
    
    /** Link/Port *************************************************************/
    Link*                           outputLink;
    Link*                           responseLink;
    Link*                           requestLink;
    Link*                           dataLink;
    Link*                           selfLink;

    /* Temporary data/result *************************************************/
    Queue<CAMSHAPCoreEvent*>        requestQueue;
    std::vector<uint8_t>            t;
    std::vector<uint8_t>            b;
    std::vector<uint8_t>            n;
    std::vector<uint8_t>            s;
    std::vector<uint8_t>            p;
    std::vector<uint8_t>            up;
    std::vector<uint8_t>            un;
    std::vector<uint8_t>            data;

    /* Parameters ************************************************************/
    uint32_t                        latency;
    uint32_t                        numRow;
    
    /* Control signal ********************************************************/
    bool                            busy = false;

    /** Statistics ************************************************************/
    Statistic<uint32_t>*            activeCycle;
};

}
}

#endif
