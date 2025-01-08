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

#ifndef _ACAM_COMPONENT_H
#define _ACAM_COMPONENT_H

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
* @brief Analog Content-Addressable Memory (aCAM)
* @details 
*/
class acam : public SST::Component {
public:
    /**
    * @brief Register a component.
    * @details SST_ELI_REGISTER_COMPONENT(class, “library”, “name”, version, “description”, category).
    */
    SST_ELI_REGISTER_COMPONENT(
        acam,
        "camshap",
        "acam",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Analog Content-Addressable Memory (aCAM)",
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
        {"numCol",              "(uint) Number of acam column", "32"},
        {"numRow",              "(uint) Number of acam row", "256"},

        {"acamThLow",           "(vector<uint8_t>) Low threshold", " "},
        {"acamThHigh",          "(vector<uint8_t>) High threshold", " "},
        {"acamThXLow",          "(vector<uint8_t>) Low 'don't care' threshold. 0='Don't care'", " "},
        {"acamThXHigh",         "(vector<uint8_t>) High 'dont' care' threshold. 0='Don't care'", " "},
    );
    /**
    * @brief List of ports
    * @details SST_ELI_DOCUMENT_PORTS({ “name”, “description”, vector of supported events }).
    */
    SST_ELI_DOCUMENT_PORTS(
        {"outputPort",          "Output port",      {"camshap.CAMSHAPCoreEvent"}},
        {"requestPort",         "Request port",     {"camshap.CAMSHAPCoreEvent"}},
        {"dataPort",            "Data port",        {"camshap.CAMSHAPCoreEvent"}},
    );
    /**
    * @brief List of statistics
    * @details SST_ELI_DOCUMENT_STATISTICS({ “name”, “description”, “units”, enable level }).
    */
    SST_ELI_DOCUMENT_STATISTICS(
        { "energyCAM",           "Energy consumption of CAM", "J", 1},
        { "energyDAC",           "Energy consumption of DAC", "J", 1},
        { "energySA",            "Energy consumption of SA", "J", 1},
        { "energyPC",            "Energy consumption of PC", "J", 1},
        { "energyREG",           "Energy consumption of REG", "J", 1},
    );
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    );

    acam(ComponentId_t id, Params& params);
    ~acam() { }

    void handleRequest( SST::Event* ev );
    void handleData( SST::Event* ev );
    void handleSelf( SST::Event* ev );
    bool clockTick( Cycle_t cycle );

    void init( uint32_t phase ) {}
	void setup() { }
    void finish();

private:
    class MatchRow {
    public:
        MatchRow() {}
        void init(uint32_t _size, double_t &_Junit, std::vector<double_t> &_gList, double_t &_gHRS, double_t &_gLRS, double_t &_Tclk, double_t &_Vsl, double_t &_energySL_imax, Statistic<double_t> *_energyCAM);
        void program(std::vector<uint8_t> _low, std::vector<uint8_t> _high, std::vector<uint8_t> _lowX, std::vector<uint8_t> _highX);
        bool isMatch(std::vector<uint8_t> _data, std::vector<uint8_t> _dataX);
        void calcPowerSL(uint8_t _lowX, uint8_t _highX, uint8_t _indexLow, uint8_t _indexHigh);
    private:
        std::vector<uint8_t> low;
        std::vector<uint8_t> high;
        std::vector<uint8_t> lowX;
        std::vector<uint8_t> highX;
        double_t Junit;
        std::vector<double_t> gList;
        double_t gHRS;
        double_t gLRS;
        double_t Tclk;
        double_t Vsl;
        double_t energySL_imax;
        Statistic<double_t>* energyCAM = nullptr;
    };
    
    /** Clock *****************************************************************/
    Clock::Handler<acam>            *clockHandler;
    TimeConverter                   *clockPeriod;
    
    /** IO ********************************************************************/
    Output                          outStd;
    Output                          outFile;

    /** Link/Port *************************************************************/
    Link*                           outputLink;
    Link*                           requestLink;
    Link*                           dataLink;
    Link*                           selfLink;

    /** Temporary data/result *************************************************/
    Queue<CAMSHAPCoreEvent*>        requestQueue;
    std::vector<MatchRow>           matchRows;
    std::vector<uint8_t>            dl;
    std::vector<uint8_t>            dlX;

    /** Parameters ************************************************************/
    uint32_t                        latency;
    uint32_t                        numRow;
    uint32_t                        numCol;

    /** Power parameters ******************************************************/
    std::vector<double_t>           gList;

    double_t                        energyDAC_col;
    double_t                        energySA_row;
    double_t                        energyPC_row;
    double_t                        energyCAM_row;
    double_t                        energySL_imax;
    double_t                        energyRegDynamic;
    double_t                        staticW_reg;
    double_t                        Junit;
    Statistic<double_t>*            energyCAM;
    Statistic<double_t>*            energyDAC;
    Statistic<double_t>*            energySA;
    Statistic<double_t>*            energyPC;
    Statistic<double_t>*            energyREG;

    /** Control signal ********************************************************/
    bool                            busy = false;
};

}
}

#endif
