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

#include "control_tile.h"

using namespace SST;
using namespace SST::CAMSHAP;

control_tile::control_tile(ComponentId_t id, Params &params) : Component(id) {
    
    /* Read parameters */
    uint32_t verbose        = params.find<uint32_t>("verbose", 0);
    uint32_t mask           = params.find<uint32_t>("mask", 0);
    std::string name        = params.find<std::string>("name");
    indexClass              = params.find<uint32_t>("indexClass");
    UnitAlgebra freq        = params.find<UnitAlgebra>("freq", "1GHz");
    numCore                 = params.find<uint32_t>("numCore", 1);

    std::vector<std::uint32_t> instructionTable;
    params.find_array<std::uint32_t>("instructionTable", instructionTable);
    assert(instructionTable.size() % INSTRUCTION_FIELD == 0);
    uint32_t numInstruction = static_cast<uint32_t>(static_cast<double>(instructionTable.size())/INSTRUCTION_FIELD);
    
    instructionMemory.resize(numInstruction);
    for (uint32_t i = 0, j = 0; i < numInstruction; ++i, j+=INSTRUCTION_FIELD){
        instructionMemory[i] = new InstructionEntry(instructionTable[j], instructionTable[j+1], instructionTable[j+2], instructionTable[j+3], instructionTable[j+4], instructionTable[j+5], instructionTable[j+6]);
    }
    std::vector<std::uint8_t> dataMemoryTable;
    params.find_array<std::uint8_t>("dataMemoryTable", dataMemoryTable);
    dataMemory = new DataMemory(dataMemoryTable, 0, 16528, this);
    RegINT.resize(32);
    RegFP.resize(32);

    /* Configure output (outStd: command prompt, outFile: txt file) */
    std::string outputDir   = params.find<std::string>("outputDir");
    std::string prefix = "@t ["+name+"]:";
    outStd.init(prefix, verbose, mask, Output::STDOUT);
    outFile.init("@t ", verbose, mask, Output::FILE, outputDir+name+".txt");
    
    /* Register clock handler */ 
    clockHandler = new Clock::Handler<control_tile>(this, &control_tile::clockTick);
    clockPeriod = registerClock(freq, clockHandler);

    /* Configure links */
    toRouterLink    = configureLink("toRouterPort");
    fromRouterLink  = configureLink("fromRouterPort", new Event::Handler<control_tile>(this, &control_tile::handleRouter));
    for (uint32_t i = 0; i < numCore; ++i){
        fromCorePort.push_back(new Port(i, this));
        fromCoreLink.push_back(configureLink("fromCorePort"+std::to_string(i), new Event::Handler<Port>(fromCorePort.back(), &Port::handleFromCore)));

        toCoreLink.push_back(configureLink("toCorePort"+std::to_string(i)));
    }

    /* Register statistics */
    activeCycleF    = registerStatistic<uint32_t>("activeCycleF");
    activeCycleD    = registerStatistic<uint32_t>("activeCycleD");
    activeCycleM    = registerStatistic<uint32_t>("activeCycleM");
    activeCycleALU  = registerStatistic<uint32_t>("activeCycleALU");
    activeCycleFPU  = registerStatistic<uint32_t>("activeCycleFPU");
}

void
control_tile::setup() {
    pc = 0;
    fetch_ready = true;
    fetch_update = false;

    decode_ready = true;
    decode_update = false;
    decode_cmd = Instrn::OP_IMM;
    decode_pc = 0;
    decode_rd = 0;
    decode_rs1 = 0;
    decode_rs2 = 0;
    decode_frs1 = 0.0;
    decode_frs2 = 0.0;
    decode_imm = 0;
    decode_funct3 = 0;
    decode_funct7 = 0;

    execute_ready = true;
    execute_update = false;
    execute_cmd = Instrn::OP_IMM;
    execute_rd = 0;
    execute_rs1 = 0;
    execute_rs2 = 0;
    execute_frs1 = 0.0;
    execute_frs2 = 0.0;
    execute_imm = 0;
    execute_funct3 = 0;
    execute_funct7 = 0;

    memory_ready = true;
    memory_update = false;
    memory_cmd = Instrn::OP_IMM;
    memory_rd = 0;
    memory_rs1 = 0;
    memory_frs1 = 0.0;

    write_ready = true;
    write_update = false;
    write_cmd = Instrn::OP_IMM;
    write_end = false;
}

void
control_tile::Port::handleFromCore(Event *ev) {
    CAMSHAPEvent *event = static_cast<CAMSHAPEvent*>(ev);
    float_t temp = m_control_tile->dataMemory->getFP_4B(16528);
    if (event->getOpcode() == static_cast<uint32_t>(Instrn::COPY)){
        uint32_t value = (event->getPayload()[3] << 24) + (event->getPayload()[2] << 16) + (event->getPayload()[1] << 8) + (event->getPayload()[0]);
        float_t* fp = reinterpret_cast<float_t*>(&value);
        temp += *fp;
        m_control_tile->dataMemory->putFP_4B(16528, temp);
        m_control_tile->dataMemory->putINT_1B(16540, 1);
        m_control_tile->outFile.verbose(CALL_INFO, 1, (1<<3), "%10s - %5s %5s %5s %5s = %.5f\n", "Input result", std::to_string(event->getPayload()[3]).c_str(), std::to_string(event->getPayload()[2]).c_str(), std::to_string(event->getPayload()[1]).c_str(), std::to_string(event->getPayload()[0]).c_str(), m_control_tile->convertINTtoFP32(event->getPayload()[0], event->getPayload()[1], event->getPayload()[2], event->getPayload()[3]));
        m_control_tile->activeCycleM->addData(2);
    }
    else{
        m_control_tile->outFile.fatal(CALL_INFO, -1, "Unknown opcode:%5" PRIu32 "\n", event->getOpcode());
    }
    m_control_tile->outFile.verbose(CALL_INFO, 1, (1<<3), "%10s - %.5f\n", "Sum Result", temp);

    delete ev;
}

void
control_tile::handleRouter(Event *ev) {
    CAMSHAPEvent *event = static_cast<CAMSHAPEvent*>(ev);
    if (event->getOpcode() == static_cast<uint32_t>(Instrn::COPY)){
        uint32_t size = event->getPayload().size();
        for (uint32_t i = 0; i < size; i++){
            dataMemory->putINT_1B(i, event->getPayload()[i]);
            outFile.verbose(CALL_INFO, 1, (1<<4), "%10s - Opcode:%3" PRIu32 ", Data: %3" PRIu32 " at %8" PRIu32 "\n", "Node->", event->getOpcode(), event->getPayload()[i], i);
        }
        dataMemory->putINT_1B(16536, 1);
    }
    else{
        outFile.fatal(CALL_INFO, -1, "Unknown opcode:%5" PRIu32 "\n", event->getOpcode());
    }
    delete ev;
}

void 
control_tile::resetPipeline(uint32_t newPc){
    pc = newPc;
    fetch_ready = true;
    fetch_update = false;

    decode_ready = true;
    decode_update = false;
    decode_cmd = Instrn::OP_IMM;
    decode_pc = 0;
    decode_rd = 0;
    decode_rs1 = 0;
    decode_rs2 = 0;
    decode_frs1 = 0.0;
    decode_frs2 = 0.0;
    decode_imm = 0;
    decode_funct3 = 0;
    decode_funct7 = 0;

    execute_ready = true;
    execute_update = false;
    execute_cmd = Instrn::OP_IMM;
    execute_rd = 0;
    execute_rs1 = 0;
    execute_rs2 = 0;
    execute_frs1 = 0.0;
    execute_frs2 = 0.0;
    execute_imm = 0;
    execute_funct3 = 0;
    execute_funct7 = 0;

    memory_ready = true;
    memory_update = false;
    memory_cmd = Instrn::OP_IMM;
    memory_rd = 0;
    memory_rs1 = 0;
    memory_frs1 = 0.0;

    write_ready = true;
    write_update = false;
    write_cmd = Instrn::OP_IMM;
    write_end = false;
}


bool
control_tile::performFetch(){
    activeCycleF->addData(1);
    if (pc > instructionMemory.size()){
        outFile.fatal(CALL_INFO, -1, "PC is out of range: PC %5" PRIu32 " > Instruction memory size %5" PRIu64 "\n", pc, instructionMemory.size());
        return false;
    }
    else{
        decode_pc = pc;
        pc++;
        outFile.verbose(CALL_INFO, 2, (1<<6), "%10s - PC:%3" PRIu32 "\n", "Fetch", decode_pc);
        return true;
    }
}

bool
control_tile::performDecode(){
    activeCycleD->addData(1);
    bool update = true;
    InstructionEntry *instrn = instructionMemory[decode_pc];
    decode_cmd = instrn->getCmd();
    switch (decode_cmd){
        case Instrn::COPY:
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_rs2      = RegINT[instrn->getRs2()];
            decode_funct3   = instrn->getFunct3();
            break;
        case Instrn::LOAD:
        case Instrn::LOAD_FP:
        case Instrn::OP_IMM:
            decode_rd       = instrn->getRd();
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_imm      = instrn->getImm();
            decode_funct3   = instrn->getFunct3();
            break;
        case Instrn::STORE:
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_rs2      = RegINT[instrn->getRs2()];
            decode_imm      = instrn->getImm();
            decode_funct3   = instrn->getFunct3();
            break;
        case Instrn::STORE_FP:
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_frs2     = RegFP[instrn->getRs2()];
            decode_imm      = instrn->getImm();
            decode_funct3   = instrn->getFunct3();
            break;
        case Instrn::OP:
            decode_rd       = instrn->getRd();
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_rs2      = RegINT[instrn->getRs2()];
            decode_funct3   = instrn->getFunct3();
            decode_funct7   = instrn->getFunct7();
            break;
        case Instrn::OP_FP:
            decode_rd       = instrn->getRd();
            decode_frs1     = RegFP[instrn->getRs1()];
            decode_frs2     = RegFP[instrn->getRs2()];
            decode_funct3   = instrn->getFunct3();
            decode_funct7   = instrn->getFunct7();
            break;
        case Instrn::LUI:
            decode_rd       = instrn->getRd();
            decode_imm      = instrn->getImm();
            break;
        case Instrn::BRANCH:
            decode_rs1      = RegINT[instrn->getRs1()];
            decode_rs2      = RegINT[instrn->getRs2()];
            decode_imm      = instrn->getImm();
            decode_funct3   = instrn->getFunct3();
            break;
        case Instrn::RET:
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Decode: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(decode_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<6), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", Rs2:%8" PRIu32 ", Imm:%8" PRIu32 ", FRs1:%8.3f, FRs2:%8.3f, Funct3:%8" PRIu32", Funct7:%8" PRIu32"\n", "Decode", static_cast<uint32_t>(decode_cmd), decode_rd, decode_rs1, decode_rs2, decode_imm, decode_frs1, decode_frs2, decode_funct3, decode_funct7);
    return update;
}

bool
control_tile::performExecute(){
    bool update = true;
    execute_cmd = decode_cmd;
    switch (execute_cmd){
        case Instrn::COPY:
            execute_rs1     = decode_rs1;
            execute_rs2     = decode_rs2;
            execute_funct3  = decode_funct3;
            break;
        case Instrn::LOAD:
            activeCycleALU->addData(1);
            execute_rd      = decode_rd;
            execute_rs1     = decode_rs1 + decode_imm;
            break;
        case Instrn::LOAD_FP:
            activeCycleALU->addData(1);
            execute_rd      = decode_rd;
            execute_rs1     = decode_rs1 + decode_imm;
            break;
        case Instrn::STORE:
            activeCycleALU->addData(1);
            execute_rs1     = decode_rs1 + decode_imm;
            execute_rs2     = decode_rs2;
            break;
        case Instrn::STORE_FP:
            activeCycleALU->addData(1);
            execute_rs1     = decode_rs1 + decode_imm;
            execute_frs2    = decode_frs2;
            break;
        case Instrn::OP_IMM:{
            activeCycleALU->addData(1);
            execute_rd      = decode_rd;
            switch (decode_funct3){
                case 0:
                    execute_rs1 = decode_rs1 + decode_imm;
                    break;
                case 1:
                    execute_rs1 = decode_rs1 << (decode_imm & 0x1f);
                    break;
            }
            break;
        }
        case Instrn::OP:{
            activeCycleALU->addData(1);
            execute_rd      = decode_rd;
            switch (decode_funct3){
                case 0:{
                    switch (decode_funct7){
                        case 0:
                            execute_rs1 = decode_rs1 + decode_rs2;
                            break;
                        case 1:
                            execute_rs1 = decode_rs1 * decode_rs2;
                            break;
                    }
                    break;
                }
            }
            break;
        }
        case Instrn::LUI:
            activeCycleALU->addData(1);
            execute_rd      = decode_rd;
            execute_rs1     = decode_imm << 12;
            break;
        case Instrn::OP_FP:{
            activeCycleFPU->addData(1);
            execute_rd      = decode_rd;
            switch (decode_funct7){
                case 0:
                    execute_frs1 = decode_frs1 + decode_frs2;
                    break;
                case 12:
                    execute_frs1 = decode_frs1 / decode_frs2;
                    break;
            }
            break;
        }
        case Instrn::BRANCH:{
            activeCycleALU->addData(1);
            int32_t offset = std::pow(-1, decode_imm >> 11) * (decode_imm & 0xff);
            switch (decode_funct3){
                case 0:{
                    if (decode_rs1 == decode_rs2){
                        resetPipeline(pc + offset);
                        update = false;
                    }
                    break;
                }
                case 1:{
                    if (decode_rs1 != decode_rs2){
                        resetPipeline(pc + offset);
                        update = false;
                    }
                    break;
                }
            }
            break;
        }
        case Instrn::RET:
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Execute: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(execute_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<6), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", Rs2:%8" PRIu32 ", Imm:%8" PRIu32 ", FRs1:%8.3f, FRs2:%8.3f, Funct3:%8" PRIu32", Funct7:%8" PRIu32"\n", "Execute", static_cast<uint32_t>(execute_cmd), execute_rd, execute_rs1, execute_rs2, execute_imm, execute_frs1, execute_frs2, execute_funct3, execute_funct7);
    return update;
}

/**
 * @brief Process an event.
 */
bool
control_tile::performMemory(){
    bool update = true;
    memory_cmd      = execute_cmd;
    memory_rd       = execute_rd;
    memory_rs1      = execute_rs1;
    memory_frs1     = execute_frs1;
    switch (memory_cmd){
        case Instrn::COPY:
            activeCycleM->addData(1);
            dataMemory->sendVector(execute_rs1, execute_rs2, execute_funct3);
            break;
        case Instrn::LOAD:
            activeCycleM->addData(1);
            memory_rs1 = dataMemory->getINT_4B(execute_rs1);
            break;
        case Instrn::LOAD_FP:
            activeCycleM->addData(1);
            memory_frs1 = dataMemory->getFP_4B(execute_rs1);
            break;
        case Instrn::STORE:
            activeCycleM->addData(1);
            dataMemory->putINT_4B(execute_rs1, execute_rs2);
            break;
        case Instrn::STORE_FP:
            activeCycleM->addData(1);
            dataMemory->putFP_4B(execute_rs1, execute_frs2);
            break;
        case Instrn::OP_IMM:
        case Instrn::OP:
        case Instrn::LUI:
        case Instrn::OP_FP:
        case Instrn::BRANCH:
        case Instrn::RET:
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Memory: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(memory_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<6), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", FRs1:%8.3f\n", "Memory", static_cast<uint32_t>(memory_cmd), memory_rd, memory_rs1, memory_frs1);
    return update;
}

bool
control_tile::performWriteBack(){
    bool update = true;
    write_cmd       = memory_cmd;
    switch (write_cmd){
        case Instrn::LOAD:
        case Instrn::OP_IMM:
        case Instrn::OP:
        case Instrn::LUI:
            RegINT[memory_rd] = memory_rs1;
            break;
        case Instrn::OP_FP:
        case Instrn::LOAD_FP:
            RegFP[memory_rd] = memory_frs1;
            break;
        case Instrn::STORE:
        case Instrn::STORE_FP:
        case Instrn::BRANCH:
        case Instrn::COPY:
            break;
        case Instrn::RET:
            write_end = true;
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Memory: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(write_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<6), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", FRs1:%8.3f\n", "WriteBack", static_cast<uint32_t>(write_cmd), memory_rd, memory_rs1, memory_frs1);
    return update;
}

bool
control_tile::clockTick(Cycle_t cycle) {
    

    if (write_ready){
        if (memory_update){
            write_update = performWriteBack();
        }
        if (memory_ready){
            if (execute_update){
                memory_update = performMemory();
            }
            if (execute_ready){
                if (decode_update){
                    execute_update = performExecute();
                }
                if (decode_ready){
                    if (fetch_update){
                        decode_update = performDecode();
                    }
                    if (fetch_ready){
                        fetch_update = performFetch();
                    }
                }
            }
        }
    }

    outFile.verbose(CALL_INFO, 2, (1<<6), "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "\n", RegINT[10], RegINT[11], RegINT[12], RegINT[13], RegINT[14], RegINT[15], RegINT[16], RegINT[17], RegINT[18], RegINT[19]);
    outFile.verbose(CALL_INFO, 2, (1<<6), "%8.3f %8.3f %8.3f %8.3f %8.3f\n\n", RegFP[0], RegFP[1], RegFP[2], RegFP[3], RegFP[4]);

    return false;
}

Instrn
control_tile::InstructionEntry::getCmd(){
    switch(opcode){
        case(0):
            return Instrn::RET;
        case(11):
            return Instrn::COPY;
        case(3):
            return Instrn::LOAD;
        case(7):
            return Instrn::LOAD_FP;
        case(19):
            return Instrn::OP_IMM;
        case(35):
            return Instrn::STORE;
        case(39):
            return Instrn::STORE_FP;
        case(51):
            return Instrn::OP;
        case(55):
            return Instrn::LUI;
        case(83):
            return Instrn::OP_FP;
        case(99):
            return Instrn::BRANCH;
        default:
            return Instrn::OP_IMM;
    }
}

void
control_tile::DataMemory::sendVector(uint32_t addr, uint32_t size, uint32_t funct3){
    std::vector<uint8_t> vec;
    for (uint32_t i=0; i < size; i++){
        vec.push_back(entry[addr+i]);
    }
    switch (funct3){
        case (0):{
            CAMSHAPEvent *ev_send = new CAMSHAPEvent(static_cast<uint32_t>(Instrn::COPY), 0, 0, 0, m_control_tile->indexClass, vec);
            m_control_tile->toRouterLink->send(ev_send);
            putINT_4B(16528, 0);
            break;
        }
        case (1):{
            for (uint32_t i = 0; i < m_control_tile->numCore; i++){
                CAMSHAPEvent *ev_send = new CAMSHAPEvent(static_cast<uint32_t>(Instrn::COPY), 0, 0, 0, 0, vec);
                m_control_tile->toCoreLink[i]->send(ev_send);
            }
            break;
        }
    }
}

uint32_t
control_tile::DataMemory::getINT_4B(uint32_t addr){
    uint32_t value = (entry[addr+3] << 24) + (entry[addr+2] << 16) + (entry[addr+1] << 8) + (entry[addr]);
    return value;
}

float_t
control_tile::DataMemory::getFP_4B(uint32_t addr){
    uint32_t value = (entry[addr+3] << 24) + (entry[addr+2] << 16) + (entry[addr+1] << 8) + (entry[addr]);
    float_t* fp = reinterpret_cast<float_t*>(&value);
    return *fp;
}

void
control_tile::DataMemory::putFP_4B(uint32_t addr, float_t fp){
    uint32_t* fp32 = reinterpret_cast<uint32_t*>(&fp);
    // Little-endian
    entry[addr] = *fp32 & 0xff;
    entry[addr+1] = (*fp32>>8) & 0xff;
    entry[addr+2] = (*fp32>>16) & 0xff;
    entry[addr+3] = (*fp32>>24) & 0xff;
}

void
control_tile::DataMemory::putINT_4B(uint32_t addr, uint32_t value){
    // Little-endian
    entry[addr] = value & 0xff;
    entry[addr+1] = (value>>8) & 0xff;
    entry[addr+2] = (value>>16) & 0xff;
    entry[addr+3] = (value>>24) & 0xff;
}

uint32_t
control_tile::DataMemory::getIndex(){
    return base.second;
}

float_t
control_tile::convertINTtoFP32(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth){
    uint32_t value = (fourth << 24) + (third << 16) + (second << 8) + (first);
    float_t* fp = reinterpret_cast<float_t*>(&value);
    return *fp;
}