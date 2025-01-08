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

#ifndef _CONTROL_CORE_COMPONENT_H
#define _CONTROL_CORE_COMPONENT_H

#include "event.h"
#include "instruction.h"

#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include <sst/core/timeLord.h>
#include <sst/core/output.h>

#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <cmath> //std::ceil

namespace SST {
namespace CAMSHAP {

class control_core : public SST::Component {
    static const uint32_t INSTRUCTION_FIELD = 7;
public:
    SST_ELI_REGISTER_COMPONENT(
        control_core,
        "camshap",
        "control_core",
        SST_ELI_ELEMENT_VERSION(1,0,0),
        "Control Unit of core",
        COMPONENT_CATEGORY_UNCATEGORIZED
    )
    SST_ELI_DOCUMENT_PARAMS(
        {"verbose",             "(uint) Output verbosity. The higher verbosity, the more debug info", "0"},
        {"mask",                "(uint) Output mask", "0"},
        {"name",                "(string) Name of component"},
        {"freq",                "(UnitAlgebra) Clock frequency", "1GHz"},
        {"instructionTable",    "(vector<uint32_t>) Instruction memory", " "},
        {"dataMemoryTable",     "(vector<uint8_t>) Data memory", " "},
        {"outputDir",           "(string) Path of output files", " "},
    )
    SST_ELI_DOCUMENT_PORTS(
        {"toTilePort",          "Port to control_tile",             {"camshap.CAMSHAPEvent"}},
        {"fromTilePort",        "Port from control_tile",           {"camshap.CAMSHAPEvent"}},
        {"toCAMPort",           "Port to first acam precharge",     {"camshap.CAMSHAPCoreEvent"}},
        {"toCAMDataPort",       "Port to acam data",                {"camshap.CAMSHAPCoreEvent"}},
        {"toMPEPort",           "Port to MPE",                      {"camshap.CAMSHAPCoreEvent"}},
        {"fromMPEPort",         "Port from MPE",                    {"camshap.CAMSHAPCoreEvent"}},
        {"toMMRPort",           "Port to MMR",                      {"camshap.CAMSHAPCoreEvent"}},
        {"fromMMRPort",         "Port from MMR",                    {"camshap.CAMSHAPCoreEvent"}},
    )
    SST_ELI_DOCUMENT_STATISTICS(
        { "activeCycleF",       "Active cycles of Fetch stage", "cycles", 1},
        { "activeCycleD",       "Active cycles of Decode stage", "cycles", 1},
        { "activeCycleM",       "Active cycles of Memory stage", "cycles", 1},
        { "activeCycleALU",     "Active cycles of ALU stage", "cycles", 1},
        { "activeCycleFPU",     "Active cycles of FPU stage", "cycles", 1},
    )
    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    )

public:
    control_core(ComponentId_t id, Params& params);
    ~control_core() { }

    void init( uint32_t phase ) { }
	void setup();
    void finish(){ }

    void handleTile( SST::Event* ev );
    void handleMPE( SST::Event* ev );
    void handleMMR( SST::Event* ev );
    void resetPipeline( uint32_t newPC );
    bool performFetch();
    bool performDecode();
    bool performExecute();
    bool performMemory();
    bool performWriteBack();
    bool clockTick( Cycle_t cycle );

    float_t convertFP8toFP32(uint8_t fp8);
    float_t simdMul(float_t a, float_t b);

private:
    class InstructionEntry{
        public:
            InstructionEntry(uint32_t _opcode, uint32_t _rd, uint32_t _rs1, uint32_t _rs2, uint32_t _imm, uint32_t _funct3, uint32_t _funct7):
            opcode(_opcode), rd(_rd), rs1(_rs1), rs2(_rs2), imm(_imm), funct3(_funct3), funct7(_funct7) { }

            uint32_t getOpcode() const { return opcode;}
            uint32_t getRd() const { return rd;}
            uint32_t getRs1() const { return rs1;}
            uint32_t getRs2() const { return rs2;}
            uint32_t getImm() const { return imm;}
            uint32_t getFunct3() const { return funct3;}
            uint32_t getFunct7() const { return funct7;}
            Instrn getCmd();

        private:
            uint32_t opcode;
            uint32_t rd;
            uint32_t rs1;
            uint32_t rs2;
            uint32_t imm;
            uint32_t funct3;
            uint32_t funct7;
    };

    class DataMemory{
        public:
            DataMemory(std::vector<uint8_t> _entry, uint32_t base_min, uint32_t base_max, control_core *_control_core):
            entry(_entry), m_control_core(_control_core) {
                index = base_min;
                base.first = base_min;
                base.second = base_max;
            }
            
            void sendVector(uint32_t dest, uint32_t addr, uint32_t size, uint32_t funct3);
            uint32_t getINT_4B(uint32_t addr);
            uint32_t getINT_1B_4(uint32_t addr, uint32_t offset);
            float_t getFP_4B(uint32_t addr);
            float_t getFP_1B_4(uint32_t addr, uint32_t offset);
            void putFP_4B(uint32_t addr, float_t fp);
            void putINT_4B(uint32_t addr, uint32_t value);
            void putINT_1B(uint32_t addr, uint8_t value){
                entry[addr] = value;
            }
            uint32_t getIndex();

        private:
            std::vector<uint8_t> entry;
            std::pair<uint32_t, uint32_t> base;
            uint32_t index;
            control_core *m_control_core;
    };

    /** Clock *****************************************************************/
    Clock::Handler<control_core>    *clockHandler;
    TimeConverter                   *clockPeriod;

    /** IO ********************************************************************/
    Output                          outStd;
    Output                          outFile;

    /** Link/Port *************************************************************/
    Link*                           toTileLink;
    Link*                           fromTileLink;
    Link*                           toCAMLink;
    Link*                           toCAMDataLink;
    Link*                           toMPELink;
    Link*                           fromMPELink;
    Link*                           toMMRLink;
    Link*                           fromMMRLink;

    /** Pipeline Registers ****************************************************/
    uint32_t                        pc;
    bool                            fetch_ready;
    bool                            fetch_update;

    bool                            decode_ready;
    bool                            decode_update;
    Instrn                          decode_cmd;
    uint32_t                        decode_pc;
    uint32_t                        decode_rd;
    uint32_t                        decode_rs1;
    uint32_t                        decode_rs2;
    float_t                         decode_frs1;
    float_t                         decode_frs2;
    uint32_t                        decode_imm;
    uint32_t                        decode_funct3;
    uint32_t                        decode_funct7;

    bool                            execute_ready;
    bool                            execute_update;
    Instrn                          execute_cmd;
    uint32_t                        execute_rd;
    uint32_t                        execute_rs1;
    uint32_t                        execute_rs2;
    float_t                         execute_frs1;
    float_t                         execute_frs2;
    uint32_t                        execute_imm;
    uint32_t                        execute_funct3;
    uint32_t                        execute_funct7;

    bool                            memory_ready;
    bool                            memory_update;
    Instrn                          memory_cmd;
    uint32_t                        memory_rd;
    uint32_t                        memory_rs1;
    float_t                         memory_frs1;

    bool                            write_ready;
    bool                            write_update;
    Instrn                          write_cmd;
    bool                            write_end;
    
    /** Parameters ************************************************************/
    std::vector<InstructionEntry*>  instructionMemory;
    DataMemory*                     dataMemory;
    std::vector<uint32_t>           RegINT;
    std::vector<float_t>            RegFP;

    /** Control signal ********************************************************/
    bool                            busy = false;

    /** Statistics ************************************************************/
    Statistic<uint32_t>*            activeCycleF;
    Statistic<uint32_t>*            activeCycleD;
    Statistic<uint32_t>*            activeCycleM;
    Statistic<uint32_t>*            activeCycleALU;
    Statistic<uint32_t>*            activeCycleFPU;
};

}
}

#endif