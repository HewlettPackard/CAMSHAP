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

#ifndef _ROUTER_COMPONENT_H
#define _ROUTER_COMPONENT_H

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
#include <cmath>

namespace SST {
namespace CAMSHAP {
/**
* @brief Router
* @details 
*/
class router : public SST::Component {
public:
    /**
    * @brief Register a component.
    * @details SST_ELI_REGISTER_COMPONENT(class, “library”, “name”, version, “description”, category).
    */
    SST_ELI_REGISTER_COMPONENT(
        router,
        "camshap",
        "router",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Router",
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
        {"numPort",             "(uint) Number of ports", "1"},
        {"outputDir",           "(string) Path of output files", " "},
    );
    /**
    * @brief List of ports
    * @details SST_ELI_DOCUMENT_PORTS({ “name”, “description”, vector of supported events }).
    */
    SST_ELI_DOCUMENT_PORTS(
        {"fromUpPort",          "Port from upper level",    {"camshap.CAMSHAPEvent"}},
        {"toUpPort",            "Port to upper level",      {"camshap.CAMSHAPEvent"}},
        {"fromDownPort%d",      "Port from lower level",    {"camshap.CAMSHAPEvent"}},
        {"toDownPort%d",        "Port to lower level",      {"camshap.CAMSHAPEvent"}},
    );
    /**
    * @brief List of statistics
    * @details SST_ELI_DOCUMENT_STATISTICS({ “name”, “description”, “units”, enable level }).
    */
    SST_ELI_DOCUMENT_STATISTICS(
        { "activeCycleFPU",     "Active cycles of FPU", "cycles", 1},
        { "activeCycleMUX",     "Active cycles of MUX", "cycles", 1},
    );
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    );
    router(ComponentId_t id, Params& params);
    ~router() { }

    void handleUp( SST::Event* ev );
    void handleResult( SST::Event* ev );
    void handleData( SST::Event* ev );
    bool clockTick( Cycle_t cycle );
    float_t convertINTtoFP32(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth);
    uint32_t convertFP32toINT(float_t value);

    void init( unsigned int phase ) {}
	void setup() { }
    void finish() { }

private:
    class Port {
    public:
        Port(uint32_t portID, TimeConverter *clockPeriod, router *router):
            portID(portID),
            clockPeriod(clockPeriod),
            m_router(router)
        {}

        void handleDown( SST::Event* ev );
        Event* getEvent(Cycle_t curCycle){ return portQueue.pop(curCycle); }
    private:
        uint32_t                    portID;
        Queue<Event*>               portQueue;
        TimeConverter               *clockPeriod;
        router                      *m_router;
    };

    /** Clock *****************************************************************/
    Clock::Handler<router>          *clockHandler;
    TimeConverter                   *clockPeriod;

    /** IO ********************************************************************/
    Output                          outStd;
    Output                          outFile;

    /** Link/Port *************************************************************/
    Link*                           fromUpLink;
    Link*                           toUpLink;
    Link*                           dataLink;
    Link*                           resultLink;
    std::vector<Link*>              fromDownLink;
    std::vector<Link*>              toDownLink;
    std::vector<Port*>              fromDownPort;

    /** Temporary data/result *************************************************/
    Queue<Event*>                   fromUpQueue;

    /** Parameters ************************************************************/
    uint32_t                        latency;
    uint32_t                        numPort;


    /** Control signal ********************************************************/
    bool                            busy_data = false;
    bool                            busy_result = false;
    
    /** Statistics ************************************************************/
    Statistic<uint32_t>*            activeCycleFPU;
    Statistic<uint32_t>*            activeCycleMUX;
};

}
}

#endif
