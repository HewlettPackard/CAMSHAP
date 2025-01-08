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

#include "control_core.h"

using namespace SST;
using namespace SST::CAMSHAP;

control_core::control_core(ComponentId_t id, Params &params) : Component(id) {
    
    /* Read parameters */
    uint32_t verbose        = params.find<uint32_t>("verbose", 0);
    uint32_t mask           = params.find<uint32_t>("mask", 0);
    std::string name        = params.find<std::string>("name");
    UnitAlgebra freq        = params.find<UnitAlgebra>("freq", "1GHz");

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
    dataMemory = new DataMemory(dataMemoryTable, 128, 256, this);
    RegINT.resize(32);
    RegFP.resize(32);

    /* Configure output (outStd: command prompt, outFile: txt file) */
    std::string outputDir   = params.find<std::string>("outputDir");
    std::string prefix = "@t ["+name+"]:";
    outStd.init(prefix, verbose, mask, Output::STDOUT);
    outFile.init("@t ", verbose, mask, Output::FILE, outputDir+name+".txt");
    
    /* Register clock handler */ 
    clockHandler = new Clock::Handler<control_core>(this, &control_core::clockTick);
    clockPeriod = registerClock(freq, clockHandler);

    /* Configure links */
    toTileLink      = configureLink("toTilePort");
    fromTileLink    = configureLink("fromTilePort", new Event::Handler<control_core>(this, &control_core::handleTile));
    toCAMLink       = configureLink("toCAMPort");
    toCAMDataLink   = configureLink("toCAMDataPort");
    toMPELink       = configureLink("toMPEPort");
    fromMPELink     = configureLink("fromMPEPort", new Event::Handler<control_core>(this, &control_core::handleMPE));
    toMMRLink       = configureLink("toMMRPort");
    fromMMRLink     = configureLink("fromMMRPort", new Event::Handler<control_core>(this, &control_core::handleMMR));

    /* Register statistics */
    activeCycleF    = registerStatistic<uint32_t>("activeCycleF");
    activeCycleD    = registerStatistic<uint32_t>("activeCycleD");
    activeCycleM    = registerStatistic<uint32_t>("activeCycleM");
    activeCycleALU  = registerStatistic<uint32_t>("activeCycleALU");
    activeCycleFPU  = registerStatistic<uint32_t>("activeCycleFPU");
}

void
control_core::setup() {
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
control_core::handleTile(Event *ev) {
    CAMSHAPEvent *event = static_cast<CAMSHAPEvent*>(ev);
    if (event->getOpcode() == static_cast<uint32_t>(Instrn::COPY)){
        uint32_t size = event->getPayload().size();
        for (uint32_t i = 0; i < size; i++){
            dataMemory->putINT_1B(dataMemory->getIndex() + i, event->getPayload()[i]);
            outFile.verbose(CALL_INFO, 1, (1<<4), "%10s - Opcode:%3" PRIu32 ", Data: %3" PRIu32 " at %8" PRIu32 "\n", "Tile->", event->getOpcode(), event->getPayload()[i], dataMemory->getIndex()+i);
        }
        dataMemory->putINT_1B(1192, (dataMemory->getINT_4B(1192) & 0xff)+ 1);
    }
    else{
        outFile.fatal(CALL_INFO, -1, "Unknown opcode:%5" PRIu32 "\n", event->getOpcode());
    }
    delete ev;
}

void control_core::handleMPE(Event *ev){
    CAMSHAPCoreEvent *event = static_cast<CAMSHAPCoreEvent*>(ev);
    uint32_t size = event->getPayload().size();
    for (uint32_t i = 0; i < size; i++){
        dataMemory->putINT_1B(384 + i, event->getPayload()[i]);
    }
    execute_ready = true;
    execute_update = true;
    delete ev;
}

void control_core::handleMMR(Event *ev){
    CAMSHAPCoreEvent *event = static_cast<CAMSHAPCoreEvent*>(ev);
    uint32_t size = event->getPayload().size();
    for (uint32_t i = 0; i < size; i++){
        dataMemory->putINT_1B(1152 + i, event->getPayload()[i]);
    }
    execute_ready = true;
    execute_update = true;
    delete ev;
}

void control_core::resetPipeline(uint32_t newPc){
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
control_core::performFetch(){
    activeCycleF->addData(1);
    if (pc > instructionMemory.size()){
        outFile.fatal(CALL_INFO, -1, "PC is out of range: PC %5" PRIu32 " > Instruction memory size %5" PRIu64 "\n", pc, instructionMemory.size());
        return false;
    }
    else{
        decode_pc = pc;
        pc++;
        outFile.verbose(CALL_INFO, 2, (1<<7), "%10s - PC:%3" PRIu32 "\n", "Fetch", decode_pc);
        return true;
    }
}

bool
control_core::performDecode(){
    activeCycleD->addData(1);
    bool update = true;
    InstructionEntry *instrn = instructionMemory[decode_pc];
    decode_cmd = instrn->getCmd();
    switch (decode_cmd){
        case Instrn::COPY:
            decode_rd       = RegINT[instrn->getRd()];
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
        case Instrn::CUSTOM:{
            decode_funct3   = instrn->getFunct3();
            decode_imm      = instrn->getImm();
            CAMSHAPCoreEvent *ev = new CAMSHAPCoreEvent(static_cast<uint32_t>(decode_cmd), 0, decode_imm, std::vector<uint8_t>(1, 0));
            switch(decode_funct3){
                // CAM
                case 0:{
                    // decode_imm: RESET(0)/SEARCH(1)
                    toCAMLink->send(ev);
                    break;
                }
                // MPE
                case 1:{
                    // decode_imm: RESET(0)/F(1)/B(2)/NS(3)/rNS(4)/U(5)
                    toMPELink->send(ev);
                    break;
                }
                // MMR
                case 2:{
                    toMMRLink->send(ev);
                    break;
                }
            }
            break;
        }
        default:
            outFile.fatal(CALL_INFO, -1, "Decode: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(decode_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<7), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", Rs2:%8" PRIu32 ", Imm:%8" PRIu32 ", FRs1:%8.3f, FRs2:%8.3f, Funct3:%8" PRIu32", Funct7:%8" PRIu32"\n", "Decode", static_cast<uint32_t>(decode_cmd), decode_rd, decode_rs1, decode_rs2, decode_imm, decode_frs1, decode_frs2, decode_funct3, decode_funct7);
    return update;
}

bool
control_core::performExecute(){
    bool update = true;
    execute_cmd = decode_cmd;
    switch (execute_cmd){
        case Instrn::COPY:
            execute_rd      = decode_rd;
            execute_rs1     = decode_rs1;
            execute_rs2     = decode_rs2;
            execute_funct3  = decode_funct3;
            break;
        case Instrn::LOAD:
        case Instrn::LOAD_FP:{
            switch (decode_funct3){
                case 0:
                    execute_rd      = decode_rd;
                    execute_rs1     = decode_rs1;
                    execute_imm     = decode_imm;
                    execute_funct3  = decode_funct3;
                    break;
                case 2:
                    activeCycleALU->addData(1);
                    execute_rd      = decode_rd;
                    execute_rs1     = decode_rs1 + decode_imm;
                    execute_funct3  = decode_funct3;
                    break;
            }
            break;
        }
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
                case 4:
                    execute_frs1 = decode_frs1 - decode_frs2;
                    break;
                case 8:
                    execute_frs1 = simdMul(decode_frs1, decode_frs2);
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
        case Instrn::CUSTOM:
        case Instrn::RET:
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Execute: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(execute_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<7), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", Rs2:%8" PRIu32 ", Imm:%8" PRIu32 ", FRs1:%8.3f, FRs2:%8.3f, Funct3:%8" PRIu32", Funct7:%8" PRIu32"\n", "Execute", static_cast<uint32_t>(execute_cmd), execute_rd, execute_rs1, execute_rs2, execute_imm, execute_frs1, execute_frs2, execute_funct3, execute_funct7);
    return update;
}

bool
control_core::performMemory(){
    bool update = true;
    memory_cmd      = execute_cmd;
    memory_rd       = execute_rd;
    memory_rs1      = execute_rs1;
    memory_frs1     = execute_frs1;
    switch (memory_cmd){
        case Instrn::COPY:
            activeCycleM->addData(1);
            dataMemory->sendVector(execute_rd, execute_rs1, execute_rs2, execute_funct3);
            break;
        case Instrn::LOAD:{
            activeCycleM->addData(1);
            switch (execute_funct3){
                case 0:
                    memory_rs1 = dataMemory->getINT_1B_4(execute_rs1, execute_imm);
                    break;
                case 2:
                    memory_rs1 = dataMemory->getINT_4B(execute_rs1);
                    break;
            }
            break;
        }
        case Instrn::LOAD_FP:{
            activeCycleM->addData(1);
            switch (execute_funct3){
                case 0:
                    memory_frs1 = dataMemory->getFP_1B_4(execute_rs1, execute_imm);
                    break;
                case 2:
                    memory_frs1 = dataMemory->getFP_4B(execute_rs1);
                    break;
            }
            break;
        }
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
        case Instrn::CUSTOM:
        case Instrn::RET:
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "Memory: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(memory_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<7), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", FRs1:%8.3f\n", "Memory", static_cast<uint32_t>(memory_cmd), memory_rd, memory_rs1, memory_frs1);
    return update;
}

bool
control_core::performWriteBack(){
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
        case Instrn::CUSTOM:
            break;
        case Instrn::RET:
            write_end = true;
            break;
        default:
            outFile.fatal(CALL_INFO, -1, "WriteBack: Unknown opcode:%5" PRIu32 "\n", static_cast<uint32_t>(write_cmd));
            break;
    }
    outFile.verbose(CALL_INFO, 2, (1<<7), "%10s - Opcode:%3" PRIu32 ", Rd:%8" PRIu32 ", Rs1:%8" PRIu32 ", FRs1:%8.3f\n", "WriteBack", static_cast<uint32_t>(write_cmd), memory_rd, memory_rs1, memory_frs1);
    return update;
}


bool
control_core::clockTick(Cycle_t cycle) {
    

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

    outFile.verbose(CALL_INFO, 2, (1<<7), "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "%8" PRIu32 "\n", RegINT[10], RegINT[11], RegINT[12], RegINT[13], RegINT[14], RegINT[15], RegINT[16], RegINT[17], RegINT[18], RegINT[19]);
    outFile.verbose(CALL_INFO, 2, (1<<7), "%8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f\n\n", RegFP[0], RegFP[1], RegFP[2], RegFP[3], RegFP[4], RegFP[5], RegFP[6], RegFP[7], RegFP[8], RegFP[9]);

    return false;
}

float_t
control_core::convertFP8toFP32(uint8_t fp8){
    uint8_t fp8_sign = fp8 >> 7;
    uint8_t fp8_exp = (fp8 >> 3) & 0x0f;
    uint8_t fp8_man = fp8 & 0x07;
    float_t fp8_decimal;
    if (fp8_exp == 0){
        fp8_decimal = float_t(std::pow(-1, fp8_sign) * std::pow(2, -6) * (fp8_man*std::pow(2, -3)));
    }
    else{
        fp8_decimal = float_t(std::pow(-1, fp8_sign) * std::pow(2, fp8_exp-7) * (1 + fp8_man*std::pow(2, -3)));
    }
    return fp8_decimal;
}

float_t
control_core::simdMul(float_t a, float_t b){
    float_t result = 0.0;
    uint32_t* a32 = reinterpret_cast<uint32_t*>(&a);
    uint32_t* b32 = reinterpret_cast<uint32_t*>(&b);
    for (uint32_t i = 0; i < 32; i+=8){
        uint8_t a8 = (*a32 >> i) & 0xff;
        uint8_t b8 = (*b32 >> i) & 0xff;
        float_t a8_fp = convertFP8toFP32(a8);
        float_t b8_fp = convertFP8toFP32(b8);
        result += a8_fp * b8_fp;
    }
    return result;
}

Instrn
control_core::InstructionEntry::getCmd(){
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
        case(43):
            return Instrn::CUSTOM;
        default:
            return Instrn::OP_IMM;
    }
}

void
control_core::DataMemory::sendVector(uint32_t dest, uint32_t addr, uint32_t size, uint32_t funct3){
    std::vector<uint8_t> vec;
    for (uint32_t i=0; i < size; i++){
        vec.push_back(entry[addr+i]);
    }

    switch (funct3){
        case (0):{
            CAMSHAPEvent *ev_send = new CAMSHAPEvent(static_cast<uint32_t>(Instrn::COPY), 0, 0, 0, 0, vec);
            m_control_core->toTileLink->send(ev_send);
            break;
        }
        case (2):{
            CAMSHAPCoreEvent *ev_send = new CAMSHAPCoreEvent(static_cast<uint32_t>(Instrn::COPY), dest, 0, vec);
            m_control_core->toCAMDataLink->send(ev_send);
            break;
        }
    }
}

uint32_t
control_core::DataMemory::getINT_4B(uint32_t addr){
    uint32_t value = (entry[addr+3] << 24) + (entry[addr+2] << 16) + (entry[addr+1] << 8) + (entry[addr]);
    return value;
}

float_t
control_core::DataMemory::getFP_4B(uint32_t addr){
    uint32_t value = (entry[addr+3] << 24) + (entry[addr+2] << 16) + (entry[addr+1] << 8) + (entry[addr]);
    float_t* fp = reinterpret_cast<float_t*>(&value);
    return *fp;
}

void
control_core::DataMemory::putFP_4B(uint32_t addr, float_t fp){
    uint32_t* fp32 = reinterpret_cast<uint32_t*>(&fp);
    // Little-endian
    entry[addr] = *fp32 & 0xff;
    entry[addr+1] = (*fp32>>8) & 0xff;
    entry[addr+2] = (*fp32>>16) & 0xff;
    entry[addr+3] = (*fp32>>24) & 0xff;
}

void
control_core::DataMemory::putINT_4B(uint32_t addr, uint32_t value){
    // Little-endian
    entry[addr] = value & 0xff;
    entry[addr+1] = (value>>8) & 0xff;
    entry[addr+2] = (value>>16) & 0xff;
    entry[addr+3] = (value>>24) & 0xff;
}

uint32_t
control_core::DataMemory::getINT_1B_4(uint32_t addr, uint32_t offset){
    uint32_t value = (entry[((addr >> 24) & 0xff)+offset] << 24) + (entry[((addr >> 16) & 0xff)+offset] << 16) + (entry[((addr >> 8) & 0xff)+offset] << 8) + (entry[(addr & 0xff)+offset]);
    return value;
}

float_t
control_core::DataMemory::getFP_1B_4(uint32_t addr, uint32_t offset){
    uint32_t value = (entry[((addr >> 24) & 0xff)+offset] << 24) + (entry[((addr >> 16) & 0xff)+offset] << 16) + (entry[((addr >> 8) & 0xff)+offset] << 8) + (entry[(addr & 0xff)+offset]);
    float_t* fp = reinterpret_cast<float_t*>(&value);
    return *fp;
}

uint32_t
control_core::DataMemory::getIndex(){
    uint32_t index = 0;
    if (getINT_4B(1192) & 0xff){
        index = base.second;
    }
    else{
        index = base.first;
    }
    return index;
}