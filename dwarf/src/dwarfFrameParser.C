/*
 * See the dyninst/COPYRIGHT file for copyright information.
 *
 * We provide the Paradyn Tools (below described as "Paradyn")
 * on an AS IS basis, and do not warrant its validity or performance.
 * We reserve the right to update, modify, or discontinue this
 * software at any time.  We shall have no obligation to supply such
 * updates or modifications or any other form of support to you.
 *
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#include "dwarfFrameParser.h"
#include "dwarfExprParser.h"
#include "dwarfResult.h"
#include "VariableLocation.h"
#include "Types.h"
#include "elfutils/libdw.h"
#include <stdio.h>
#include <iostream>
#include "debug_common.h" // dwarf_printf
#include <libelf.h>

//#define DW_FRAME_CFA_COL3 ((Dwarf_Half) -1)
#define DW_FRAME_CFA_COL3               1036
using namespace Dyninst;
using namespace DwarfDyninst;
using namespace std;


std::map<DwarfFrameParser::frameParser_key, DwarfFrameParser::Ptr> DwarfFrameParser::frameParsers;

DwarfFrameParser::Ptr DwarfFrameParser::create(Dwarf * dbg, Elf * eh_frame, Architecture arch)
{
    if(!dbg && !eh_frame) return NULL;

    frameParser_key k(dbg, eh_frame, arch);

    auto iter = frameParsers.find(k);
    if (iter == frameParsers.end()) {
        Ptr newParser = Ptr(new DwarfFrameParser(dbg, eh_frame, arch));
        frameParsers[k] = newParser;
        return newParser;
    }
    else {
        return iter->second;
    }
}


DwarfFrameParser::DwarfFrameParser(Dwarf * dbg_, Elf * eh_frame, Architecture arch_) :
    dbg(dbg_),
    dbg_eh_frame(eh_frame),
    arch(arch_),
    fde_dwarf_status(dwarf_status_uninitialized)
{
}

DwarfFrameParser::~DwarfFrameParser()
{
    if (fde_dwarf_status != dwarf_status_ok)
        return;
    for (unsigned i=0; i<cfi_data.size(); i++)
    {
        // FIXME only do this for dwarf_getcfi_elf!
        // // dwarf_cfi_end(cfi_data[i]);
        //
        // previous code
        //dwarf_fde_cie_list_dealloc(dbg,
        //        cfi_data[i].cfi_data, cfi_data[i].cie_count,
        //        cfi_data[i].cfi_data, cfi_data[i].fde_count);
    }
}

bool DwarfFrameParser::hasFrameDebugInfo()
{
    setupFdeData();
    return fde_dwarf_status == dwarf_status_ok;
}

bool DwarfFrameParser::getRegValueAtFrame(
        Address pc,
        Dyninst::MachRegister reg,
        Dyninst::MachRegisterVal &reg_result,
        ProcessReader *reader,
        FrameErrors_t &err_result)
{
    ConcreteDwarfResult cons(reader, arch, pc, dbg);

    dwarf_printf("Getting concrete value for %s at 0x%lx\n",
            reg.name().c_str(), pc);
    if (!getRegAtFrame(pc, reg, cons, err_result)) {
        assert(err_result != FE_No_Error);
        dwarf_printf("\t Returning error from getRegValueAtFrame: %d\n", err_result);
        return false;
    }
    if (cons.err()) {
        dwarf_printf("\t Computed dwarf result to an error\n");
        err_result = FE_Frame_Eval_Error;
        return false;
    }

    reg_result = cons.val();
    dwarf_printf("Returning result 0x%lx for reg %s at 0x%lx\n",
            reg_result, reg.name().c_str(), pc);
    return true;
}

bool DwarfFrameParser::getRegRepAtFrame(
        Address pc,
        Dyninst::MachRegister reg,
        VariableLocation &loc,
        FrameErrors_t &err_result) 
{
    SymbolicDwarfResult cons(loc, arch);

    dwarf_printf("Getting symbolic value for %s at 0x%lx\n",
            reg.name().c_str(), pc);
    if (!getRegAtFrame(pc, reg, cons, err_result)) {
        dwarf_printf("\t Returning error from getRegRepAtFrame\n");
        assert(err_result != FE_No_Error);
        return false;
    }

    if (cons.err()) {
        dwarf_printf("\t Computed dwarf result to an error\n");
        err_result = FE_Frame_Eval_Error;
        return false;
    }

    loc = cons.val();

    dwarf_printf("Returning symbolic result for reg %s at 0x%lx\n",
            reg.name().c_str(), pc);

    return true;
}

bool DwarfFrameParser::getRegsForFunction(
        std::pair<Address, Address> range,
        Dyninst::MachRegister reg,
        std::vector<VariableLocation> &locs,
        FrameErrors_t &err_result) 
{
    locs.clear();
    dwarf_printf("Entry to getRegsForFunction at 0x%lx, range end 0x%lx, reg %s\n", range.first, range.second, reg.name().c_str());
    err_result = FE_No_Error;

    /**
     * Initialize the FDE and CIE data.  This is only really done once,
     * after which setupFdeData will immediately return.
     **/
    setupFdeData();
    if (!cfi_data.size()) {
        dwarf_printf("\t No FDE data, ret false\n");
        err_result = FE_Bad_Frame_Data;
        return false;
    }
    
    for(size_t i=0; i<cfi_data.size(); i++)
    {
        auto next_pc = range.first;
        while(next_pc < range.second)
        {
            Dwarf_Frame * frame = NULL;
            int result = dwarf_cfi_addrframe(cfi_data[i], next_pc, &frame);
            if(result==-1) break;

            Dwarf_Addr start_pc, end_pc;
            dwarf_frame_info(frame, &start_pc, &end_pc, NULL);

            Dwarf_Op * ops;
            size_t nops;
            result = dwarf_frame_cfa(frame, &ops, &nops);
            if (result != 0) break;

            VariableLocation loc2;
            DwarfDyninst::SymbolicDwarfResult cons(loc2, arch);
            if (!DwarfDyninst::decodeDwarfExpression(ops, nops, NULL, cons, arch)) break;
            loc2.lowPC = next_pc;
            loc2.hiPC = end_pc;

            locs.push_back(cons.val());
            next_pc = end_pc;
        }
    }

    return !locs.empty();
}

bool DwarfFrameParser::getRegAtFrame(
        Address pc,
        Dyninst::MachRegister reg,
        DwarfResult &cons,
        FrameErrors_t &err_result) 
{

    err_result = FE_No_Error;

    dwarf_printf("getRegAtFrame for 0x%lx, %s\n", pc, reg.name().c_str());
    /**
     * Initialize the FDE and CIE data.  This is only really done once,
     * after which setupFdeData will immediately return.
     **/
    setupFdeData();
    if (!cfi_data.size()) {
        dwarf_printf("\t No FDE data, ret false\n");
        err_result = FE_Bad_Frame_Data;
        return false;
    }

    int not_found = 0; // if not found FDE covering PC, increment
    for(size_t i=0; i<cfi_data.size(); i++)
    {
        Dwarf_Frame * frame = NULL;
        int result = dwarf_cfi_addrframe(cfi_data[i], pc, &frame);
        if (result != 0) // 0 is success, not found FDE covering PC is returned -1
        {
            not_found++;
            continue;
        }
        // FDE found so make not_found=0
        not_found=0;

        Dwarf_Addr start_pc, end_pc;
        dwarf_frame_info(frame, &start_pc, &end_pc, NULL); 

        Dwarf_Op * ops;
        size_t nops;
        result = dwarf_frame_cfa(frame, &ops, &nops);
        if (result != 0)
            return false;

        if (!DwarfDyninst::decodeDwarfExpression(ops, nops, NULL, cons, arch)) {
            //dwarf_printf("\t Failed to decode dwarf expr, ret false\n");
            return false;
        }
    }

    if(not_found){
        err_result = FE_No_Frame_Entry;
        return false;
    }

    return true;

    /**
     * Get the FDE at this PC.  The FDE contains the rules for getting
     * registers at the given PC in this frame.
     **/
    /*Dwarf_Frame * frame;
    Address u1, u2;
    if (!getFDE(pc, frame, u1, u2, err_result)) {
        dwarf_printf("\t No FDE at 0x%lx, ret false\n", pc);
        assert(err_result != FE_No_Error);
        return false;
    }

    //unique_ptr<Dwarf_Frame, decltype(std::free)*> frame_ptr(frame, &std::free);

    Dwarf_Half dwarf_reg;
    if (!getDwarfReg(reg, frame, dwarf_reg, err_result)) {
        dwarf_printf("\t Failed to convert %s to dwarf reg, ret false\n",
                reg.name().c_str());
        assert(err_result != FE_No_Error);
        return false;
    }

    Address ignored;
    return getRegAtFrame_aux(pc, frame, dwarf_reg, reg, cons, 
            ignored, err_result);
            */
}

bool DwarfFrameParser::getRegAtFrame_aux(Address pc,
        Dwarf_Frame * frame,
        Dwarf_Half dwarf_reg,
        MachRegister /*orig_reg*/,
        DwarfResult &cons,
        Address & lowpc,
        FrameErrors_t &err_result) 
{
    int result;
    //Dwarf_Error err;

    int width = getArchAddressWidth(arch);
    dwarf_printf("getRegAtFrame_aux for 0x%lx, addr width %d\n", pc, width);

    //Dwarf_Small value_type;
    //Dwarf_Sword offset_relevant, register_num, offset_or_block_len;
    //Dwarf_Ptr block_ptr;
    Dwarf_Addr row_pc;

    Dwarf_Op ops_mem[3];
    Dwarf_Op * ops;
    size_t nops;

    /**
     * Decode the rule that describes how to get dwarf_reg at pc.
     **/
    if (dwarf_reg != DW_FRAME_CFA_COL3) {
        dwarf_printf("\tNot col3 reg, using default\n");
        result = dwarf_frame_register(frame, dwarf_reg, ops_mem, &ops, &nops);
    }
    else {
        dwarf_printf("\tcol3 reg, using CFA\n");
        result = dwarf_frame_cfa(frame, &ops, &nops);
    }

    if (result != 0) {
        err_result = FE_Bad_Frame_Data;
        return false;
    }

    dwarf_frame_info(frame, NULL, &row_pc, NULL); 
    lowpc = (Address) row_pc;

    if (!decodeDwarfExpression(ops, nops, NULL, cons, arch)) {
        dwarf_printf("\t Failed to decode dwarf expr, ret false\n");
        err_result = FE_Frame_Eval_Error;
        return false;
    }

/*    switch(value_type) 
    {
        // For a val offset, the value of the register is (other_reg + const)
        case DW_EXPR_VAL_OFFSET:
        case DW_EXPR_OFFSET:
            dwarf_printf("\tHandling val_offset or offset\n");
            if (offset_relevant) {
                dwarf_printf("\t Offset relevant, adding %d\n", offset_or_block_len);
                cons.pushSignedVal(offset_or_block_len);
                cons.pushOp(DwarfResult::Add);
            }

            if (offset_relevant && dwarf_reg != DW_FRAME_CFA_COL3) {
                dwarf_printf("\t Reg not CFA and offset relevant: indirect\n");
                indirect = true;
            }
            dwarf_printf("\t Done handling val_offset or offset\n");
            break;
        case DW_EXPR_VAL_EXPRESSION:
        case DW_EXPR_EXPRESSION: 
            {
                dwarf_printf("\t Handling val_expression or expression\n");
                Dwarf_Op* llbuf = NULL;
                size_t listlen  = 0; 
                //result = dwarf_loclist_from_expr(dbg, block_ptr, offset_or_block_len, &llbuf, &listlen, &err);
                int result = dwarf_getlocation(&attr, &llbuf, &listlen);
                if (result != 0) {
                    dwarf_printf("\t Failed to get loclist, ret false\n");
                    err_result = FE_Frame_Read_Error;
                    return false;
                }

    if (!decodeDwarfExpression(llbuf, listlen, NULL, cons, arch)) {
        dwarf_printf("\t Failed to decode dwarf expr, ret false\n");
        err_result = FE_Frame_Eval_Error;
        return false;
    }

                if (value_type == DW_EXPR_EXPRESSION) {
                    dwarf_printf("\t Handling expression, adding indirect\n");
                    indirect = true;
                }
                break;
            }
        default:
            err_result = FE_Bad_Frame_Data;
            return false;
    }

    if (indirect) {
        dwarf_printf("\t Adding a dereference to handle \"address of\" operator\n");
        cons.pushOp(DwarfResult::Deref, width);
    }*/
    return true;
}


void DwarfFrameParser::setupFdeData()
{
    if (fde_dwarf_status == dwarf_status_ok ||
        fde_dwarf_status == dwarf_status_error)
        return;

    if (!dbg && !dbg_eh_frame) {
        fde_dwarf_status = dwarf_status_error;
        return;
    }

#if defined(dwarf_has_setframe)
    dwarf_set_frame_cfa_value(dbg, DW_FRAME_CFA_COL3);
#endif

    Dwarf_CFI * cfi = nullptr;

    // Try to get dwarf data from .debug_frame
    cfi = dwarf_getcfi(dbg);
    if (dbg && cfi)
    {
        cfi_data.push_back(cfi);
    }
    
    // Try to get dwarf data from .eh_frame
    cfi = nullptr;
    cfi = dwarf_getcfi_elf(dbg_eh_frame);
    if (dbg_eh_frame && cfi)
    {
        cfi_data.push_back(cfi);
    }
    
    // Verify if it got any dwarf data
    if (!cfi_data.size()) {
        fde_dwarf_status = dwarf_status_error;
    }
    else{
        fde_dwarf_status = dwarf_status_ok;
    }
}


bool DwarfFrameParser::getFDE(Address pc, Dwarf_Frame* &frame,
        Address &low, Address &high, FrameErrors_t &err_result) 
{
    /*
    dwarf_printf("Getting Frame for 0x%lx\n", pc);

    Dwarf_Addr lowpc = 0, hipc = 0;
    bool found = false;

    for (size_t cur_cfi=0; cur_cfi < cfi_data.size(); cur_cfi++)
    {
        for (size_t cur_entry =0; cur_entry < cfi_data[cur_cfi].cfi_entries.size(); cur_entry++)
        {
            auto& cfi_entry = cfi_data[cur_cfi].cfi_entries[cur_entry];
            
            // if this entry is a CIE, skip
            if(dwarf_cfi_cie_p(&cfi_entry))
            {
                continue;
            }

            lowpc = *cfi_entry.fde.start;
            hipc = *cfi_entry.fde.end;
            if (pc < lowpc || pc > hipc)
            {
                continue;
            }

            dwarf_printf("\t Got range 0x%lx..0x%lx\n", lowpc, hipc);

            low = (Address) lowpc;
            high = (Address) hipc;
            if(!dwarf_cfi_addrframe(cfi_data[cur_cfi].cfi, pc, &frame))
            {
                continue;
            }
            found = true;
            break;
        }

        if (found)
        {
            break;
        }
    }

    if (!found)
    {
        dwarf_printf("\tEntry not found, ret false\n");
        err_result = FE_No_Frame_Entry;
        return false;
    }
    return true;
    */
}

bool DwarfFrameParser::getDwarfReg(
        Dyninst::MachRegister reg, Dwarf_Frame* frame, 
        Dwarf_Half &dwarf_reg, FrameErrors_t & /*err_result*/)
{
    if (reg == Dyninst::ReturnAddr) {
        dwarf_reg = dwarf_frame_info(frame, NULL, NULL, NULL); 
    }
    else if (reg == Dyninst::FrameBase || reg == Dyninst::CFA) {
        dwarf_reg = DW_FRAME_CFA_COL3;
    }
    else {
        dwarf_reg = reg.getDwarfEnc();
    }
    return true;
}

/*bool DwarfFrameParser::handleExpression(Address pc,
        Dwarf_Sword registerNum,
        Dyninst::MachRegister origReg,
        Dyninst::Architecture arch,
        DwarfResult &cons,
        bool &done,
        FrameErrors_t &err_result)
{
    dwarf_printf("HandleExpression\n");

    done = false;
    dwarf_printf("register num %d\n", registerNum);
    switch (registerNum) {
        case DW_FRAME_CFA_COL3:
            dwarf_printf("\t Getting frame base\n");

            err_result = FE_No_Error;
            if (!getRegAtFrame(pc, Dyninst::FrameBase,
                        cons, err_result)) {
                assert(err_result != FE_No_Error);
                return false;
            }
            break;
        case DW_FRAME_SAME_VAL:
            dwarf_printf("\t Getting %s\n", origReg.name().c_str());
#if defined(arch_aarch64)
            //if(origReg == Dyninst::ReturnAddr)
            //    origReg = Dyninst::aarch64::x30;
            origReg = MachRegister::getArchRegFromAbstractReg(origReg, arch);
#endif
            cons.readReg(origReg);
            done = true;
            break;
        case DW_FRAME_UNDEFINED_VAL:
            dwarf_printf("\t Value not available for %s\n", origReg.name().c_str());
            err_result = FE_No_Frame_Entry;
            return false;

        default:
            {
                Dyninst::MachRegister dyn_register = MachRegister::DwarfEncToReg(registerNum, arch);
                dwarf_printf("\t Getting %s\n", dyn_register.name().c_str());

                cons.readReg(dyn_register);
                break;
            }
    }
    return true;
}*/


