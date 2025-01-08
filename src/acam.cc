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

#include "acam.h"

using namespace SST;
using namespace SST::CAMSHAP;

/**
* @brief Main constructor for acam 
* @details Read parameters, program aCAM, configure output, register clock handler, and configure links.
*/
acam::acam(ComponentId_t id, Params &params) : Component(id) {
    
    /* Read parameters */
    uint32_t verbose        = params.find<uint32_t>("verbose", 0);
    uint32_t mask           = params.find<uint32_t>("mask", 0);
    std::string name        = params.find<std::string>("name");
    UnitAlgebra freq        = params.find<UnitAlgebra>("freq", "1GHz");
    latency                 = params.find<uint32_t>("latency", 4);

    numRow                  = params.find<uint32_t>("numRow", 256);
    numCol                  = params.find<uint32_t>("numCol", 128);

    dl.resize(numCol);
    dlX.resize(numCol);

    /* Power parameters*/
    double_t Cml            = params.find<double_t>("Cml", -1);
    double_t Cpre           = params.find<double_t>("Cpre", -1);
    double_t Cmlso          = params.find<double_t>("Cmlso", -1);
    double_t Cdl            = params.find<double_t>("Cdl", -1);
    double_t Rw             = params.find<double_t>("Rw", -1);
    double_t K1             = params.find<double_t>("K1", -1);
    double_t Vth            = params.find<double_t>("Vth", -1);
    double_t Vml            = params.find<double_t>("Vml", -1);
    double_t Vns            = params.find<double_t>("Vns", -1);
    double_t Vsl            = params.find<double_t>("Vsl", -1);
    double_t Vdd            = params.find<double_t>("Vdd", -1);
    double_t gHRS           = params.find<double_t>("gHRS", -1);
    double_t gLRS           = params.find<double_t>("gLRS", -1);

    double_t dynamicW_reg   = params.find<double_t>("dynamicW_reg", -1);
    staticW_reg             = params.find<double_t>("staticW_reg", -1);
    Junit                   = params.find<double_t>("Junit", 1e-15);

    double_t Tclk = 1/(freq.getDoubleValue());
    double_t Rout = Tclk/(numRow*Cdl) - 0.5*Rw*(numRow-1);
    energyDAC_col = (1/Junit)*2*Tclk*Vdd*Vdd/Rout;
    energySA_row = (1/Junit)*(Cmlso) * std::pow(Vml-Vns, 2);
    energyPC_row = (1/Junit)*(Cpre) * std::pow(Vml-Vns, 2);
    energyCAM_row = (1/Junit)*(Cml*numCol) * std::pow(Vml-Vns, 2);
    energySL_imax = 0.5 * K1 * std::pow(Vml - Vth, 2);
    energyRegDynamic = (1/Junit)*Tclk*dynamicW_reg;
    
    energyCAM = registerStatistic<double_t>("energyCAM");
    energyDAC = registerStatistic<double_t>("energyDAC");
    energySA = registerStatistic<double_t>("energySA");
    energyPC = registerStatistic<double_t>("energyPC");
    energyREG = registerStatistic<double_t>("energyREG");
    
    params.find_array("gList", gList);    

    /* Program aCAM */
    std::vector<uint8_t> acamThLow;
    std::vector<uint8_t> acamThHigh;
    std::vector<uint8_t> acamThXLow;
    std::vector<uint8_t> acamThXHigh;
    params.find_array("acamThLow", acamThLow);
    params.find_array("acamThHigh", acamThHigh);
    params.find_array("acamThXLow", acamThXLow);
    params.find_array("acamThXHigh", acamThXHigh);
    
    matchRows.resize(numRow);
    for (uint32_t row = 0; row < numRow; ++row){
        std::vector<uint8_t> acamThLowRow = {acamThLow.begin() + row*numCol, acamThLow.begin() + (row+1)*numCol};
        std::vector<uint8_t> acamThHighRow = {acamThHigh.begin() + row*numCol, acamThHigh.begin() + (row+1)*numCol};
        std::vector<uint8_t> acamThXLowRow = {acamThXLow.begin() + row*numCol, acamThXLow.begin() + (row+1)*numCol};
        std::vector<uint8_t> acamThXHighRow = {acamThXHigh.begin() + row*numCol, acamThXHigh.begin() + (row+1)*numCol};
        matchRows[row].init(numCol, Junit, gList, gHRS, gLRS, Tclk, Vsl, energySL_imax, energyCAM);
        matchRows[row].program(acamThLowRow, acamThHighRow, acamThXLowRow, acamThXHighRow);
    }

    /* Configure output (outStd: command prompt, outFile: txt file) */
    std::string outputDir   = params.find<std::string>("outputDir");
    std::string prefix = "@t ["+name+"]:";
    outStd.init(prefix, verbose, mask, Output::STDOUT);
    outFile.init("@t ", verbose, mask, Output::FILE, outputDir+name+".txt");

    /* Register clock handler */ 
    clockHandler    = new Clock::Handler<acam>(this, &acam::clockTick);
    clockPeriod     = registerClock(freq, clockHandler);

    /* Configure links */
    outputLink      = configureLink("outputPort");
    requestLink     = configureLink("requestPort", new Event::Handler<acam>(this, &acam::handleRequest));
    dataLink        = configureLink("dataPort", new Event::Handler<acam>(this, &acam::handleData));
    selfLink        = configureSelfLink("selfLink", freq, new Event::Handler<acam>(this, &acam::handleSelf));
}

void
acam::finish(){
    energyREG->addData((1/Junit)*getElapsedSimTime().getDoubleValue()*staticW_reg*(numRow + 8*numCol));
}

/**
 * @brief Handle request event.
 * @details Add request event to request queue to handle it in the next clock cycle either reset or search.
 */
void
acam::handleRequest(Event *ev) {
    requestQueue.push(getNextClockCycle(clockPeriod), 0, static_cast<CAMSHAPCoreEvent*>(ev));
}

/**
 * @brief Handle data event.
 * @details Update data line/dont'care line with the data event.
 */
void
acam::handleData(Event *ev) {
    CAMSHAPCoreEvent *dataEv = static_cast<CAMSHAPCoreEvent*>(ev);
    uint32_t size = dataEv->getPayload().size();
    uint32_t dst = dataEv->getDst();
    for (uint32_t i = 0; i < size; ++i){
        dl[dst+i] = dataEv->getPayload()[i];
        dlX[dst+i] = 1;
    }
    delete dataEv;
}
/**
 * @brief Handle self event.
 * @details Handle self event to calculate energy consumption and match result.
 */
void
acam::handleSelf(Event *ev){
    outFile.verbose(CALL_INFO, 4, (1<<8), "%10s - Data: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", "Search", dl[0], dl[1], dl[2], dl[3]);
    outFile.verbose(CALL_INFO, 4, (1<<8), "%10s - DataX: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", "Search", dlX[0], dlX[1], dlX[2], dl[3]);
    CAMSHAPCoreEvent *selfEv = static_cast<CAMSHAPCoreEvent*>(ev);
    std::vector<uint8_t> match(numRow, 0);

    for (uint32_t col = 0; col < numCol; ++col){
        if (dlX[col]){
            energyDAC->addData(energyDAC_col);
            energyREG->addData(energyRegDynamic*8);
        }
    }
    for (uint32_t row = 0; row < numRow; ++row){
        energyCAM->addData(energyCAM_row);
        energySA->addData(energySA_row);
        energyPC->addData(energyPC_row);
        energyREG->addData(energyRegDynamic);
        if (matchRows[row].isMatch(dl, dlX)){
            match[row] = 1;
        }
    }

    CAMSHAPCoreEvent *matchEv = new CAMSHAPCoreEvent(0, 0, 0, match);
    outputLink->send(matchEv);
    outFile.verbose(CALL_INFO, 4, (1<<8), "%10s - Data: %1" PRIu32 "%1" PRIu32 "%1" PRIu32 "%1" PRIu32 "\n", "Match", match[0], match[1], match[2], match[3]);
    busy = false;
    delete selfEv;
}

bool
acam::clockTick(Cycle_t cycle) {
    if (!busy){
        auto requestEv = requestQueue.pop(cycle);
        if (requestEv){
            outFile.verbose(CALL_INFO, 4, (1<<8), "%10s - Opcode:%3" PRIu32 ", Dst:%8" PRIu32 ", Imm:%8" PRIu32 "\n", "In", static_cast<uint32_t>(requestEv->getOpcode()), requestEv->getDst(), requestEv->getImm());
            if (requestEv->getImm()){
                selfLink->send(latency - 1, requestEv);
                busy = true;
            }
            else{
                dlX.assign(numCol, 0);
                outFile.verbose(CALL_INFO, 4, (1<<8), "%10s - DataX: %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 ", %3" PRIu32 "\n", "Reset", dlX[0], dlX[1], dlX[2], dl[3]);
            }
        }
    }
    return false; 
}

/**
 * @brief Initialize aCAM row with threshold map (low, high, lowX, highX)
 */
void
acam::MatchRow::init(uint32_t _size, double_t &_Junit, std::vector<double_t> &_gList, double_t &_gHRS, double_t &_gLRS, double_t &_Tclk, double_t &_Vsl, double_t &_energySL_imax, Statistic<double_t> *_energyCAM){
    low.resize(_size);
    high.resize(_size);
    lowX.resize(_size);
    highX.resize(_size);
    Junit = _Junit;
    gList = _gList;
    gHRS = _gHRS;
    gLRS = _gLRS;
    Tclk = _Tclk;
    Vsl = _Vsl;
    energySL_imax = _energySL_imax;
    energyCAM = _energyCAM;
}

/**
 * @brief Program aCAM row with threshold map (low, high, lowX, highX)
 */
void
acam::MatchRow::program(std::vector<uint8_t> _low, std::vector<uint8_t> _high, std::vector<uint8_t> _lowX, std::vector<uint8_t> _highX){
    low = _low;
    high = _high;
    lowX = _lowX;
    highX = _highX;
}

/**
 * @brief Check if the data is matched with the aCAM row.
 * @return
 * - true: matched
 * - false: not matched
 */
bool
acam::MatchRow::isMatch(std::vector<uint8_t> _data, std::vector<uint8_t> _dataX){
    bool match = true;
    uint8_t LSBLo, LSBHi, HSBLo, HSBHi;
    for (uint32_t col = 0; col < _data.size(); ++col){
        if (_dataX[col]){
            if (!lowX[col] && !highX[col]){
                match = match && true;
            }
            else if (!lowX[col]){
                if (!(_data[col] <= high[col])){
                    match = match && false;
                }
            }
            else if (!highX[col]){
                if (!(low[col] < _data[col])){
                    match = match && false;
                }
            }
            else{
                if (!((low[col] < _data[col]) && (high[col] >= _data[col]))){
                    match = match && false;
                }
            }
        }
        LSBLo = static_cast<uint8_t>(low[col])%16;
        LSBHi = static_cast<uint8_t>(low[col]/16);
        HSBLo = static_cast<uint8_t>(high[col])%16;
        HSBHi = static_cast<uint8_t>(high[col]/16);
        calcPowerSL(lowX[col], highX[col], LSBLo, LSBHi);
        calcPowerSL(lowX[col], highX[col], HSBLo, HSBHi);
    }
    return match;
}

/**
 * @brief Calculate static energy consumption in SL.
 */
void
acam::MatchRow::calcPowerSL(uint8_t _lowX, uint8_t _highX, uint8_t _indexLow, uint8_t _indexHigh){
    double_t iTotLo = Vsl * gList[_indexLow];
    double_t iTotHi = Vsl * gList[_indexHigh];
    iTotLo = (_lowX)? iTotLo : Vsl * gHRS;
    iTotHi = (_highX)? iTotHi : Vsl * gLRS;
    iTotLo = (iTotLo > energySL_imax)? energySL_imax : iTotLo;
    iTotHi = (iTotHi > energySL_imax)? energySL_imax : iTotHi;
    energyCAM->addData((1/Junit) * Tclk * Vsl * (iTotLo+iTotHi));
}