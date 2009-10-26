/*
 * Copyright (c) 1996-2004 Barton P. Miller
 * 
 * We provide the Paradyn Parallel Performance Tools (below
 * described as "Paradyn") on an AS IS basis, and do not warrant its
 * validity or performance.  We reserve the right to update, modify,
 * or discontinue this software at any time.  We shall have no
 * obligation to supply such updates or modifications or any other
 * form of support to you.
 * 
 * This license is for research uses.  For such uses, there is no
 * charge. We define "research use" to mean you may freely use it
 * inside your organization for whatever purposes you see fit. But you
 * may not re-distribute Paradyn or parts of Paradyn, in any form
 * source or binary (including derivatives), electronic or otherwise,
 * to any other organization or entity without our permission.
 * 
 * (for other uses, please contact us at paradyn@cs.wisc.edu)
 * 
 * All warranties, including without limitation, any warranty of
 * merchantability or fitness for a particular purpose, are hereby
 * excluded.
 * 
 * By your use of Paradyn, you understand and agree that we (or any
 * other person or entity with proprietary rights in Paradyn) are
 * under no obligation to provide either maintenance services,
 * update services, notices of latent defects, or correction of
 * defects for Paradyn.
 * 
 * Even if advised of the possibility of such damages, under no
 * circumstances shall we (or any other person or entity with
 * proprietary rights in the software licensed hereunder) be liable
 * to you or any third party for direct, indirect, or consequential
 * damages of any character regardless of type of action, including,
 * without limitation, loss of profits, loss of use, loss of good
 * will, or computer failure or malfunction.  You agree to indemnify
 * us (and any other person or entity with proprietary rights in the
 * software licensed hereunder) for any and all liability it may
 * incur to third parties resulting from your use of Paradyn.
 */

/*
 * emit-x86.h - x86 & AMD64 code generators
 * $Id: emit-power.h,v 1.6 2008/03/25 19:24:26 bernat Exp $
 */

#ifndef _EMITTER_POWER_H
#define _EMITTER_POWER_H

#include "common/h/headers.h"
#include "dyninstAPI/src/instPoint.h"
#include "dyninstAPI/src/baseTramp.h"
#include "dyninstAPI/src/ast.h"

#include "dyninstAPI/src/emitter.h"

class codeGen;
class registerSpace;
class baseTramp;

// class for encapsulating
// platform dependent code generation functions
class EmitterPOWER : public Emitter {

 public:
    virtual ~EmitterPOWER() {};
    virtual codeBufIndex_t emitIf(Register, Register, RegControl, codeGen &) { assert(0); return 0; }
    virtual void emitOp(unsigned, Register, Register, Register, codeGen &) { assert(0); }
    virtual void emitOpImm(unsigned, unsigned, Register, Register, RegValue,
			   codeGen &) { assert(0); }
    virtual void emitRelOp(unsigned, Register, Register, Register, codeGen &) { assert(0); }
    virtual void emitRelOpImm(unsigned, Register, Register, RegValue, codeGen &) { assert(0); }
    virtual void emitDiv(Register, Register, Register, codeGen &) { assert(0); }
    virtual void emitTimesImm(Register, Register, RegValue, codeGen &) { assert(0); }
    virtual void emitDivImm(Register, Register, RegValue, codeGen &) { assert(0); }
    virtual void emitLoad(Register, Address, int, codeGen &) { assert(0); }
    virtual void emitLoadConst(Register, Address, codeGen &) { assert(0); }
    virtual void emitLoadIndir(Register, Register, int, codeGen &) { assert(0); }
    virtual bool emitLoadRelative(Register, Address, Register, codeGen &) { assert(0); return true;}
    virtual bool emitLoadRelative(registerSlot *dest, Address offset, registerSlot *base, codeGen &gen);
    // Not implemented yet
    virtual void emitLoadShared(opCode op, Register dest, const image_variable *var, bool is_local, int size, codeGen &gen, Address offset);
    virtual void emitLoadFrameAddr(Register, Address, codeGen &) { assert(0); }

    // These implicitly use the stored original/non-inst value
    virtual void emitLoadOrigFrameRelative(Register, Address, codeGen &) { assert(0); }
    virtual void emitLoadOrigRegRelative(Register, Address, Register, codeGen &, bool) { assert(0); }
    virtual void emitLoadOrigRegister(Address, Register, codeGen &) { assert(0); }

    virtual void emitStore(Address, Register, int, codeGen &) { assert(0); }
    virtual void emitStoreIndir(Register, Register, int, codeGen &) { assert(0); }
    virtual void emitStoreFrameRelative(Address, Register, Register, int, codeGen &) { assert(0); }
    virtual void emitStoreRelative(Register, Address, Register, codeGen &) { assert(0); }
    virtual void emitStoreRelative(registerSlot *source, Address offset, registerSlot *base, codeGen &gen);
    // Not implemented yet
    virtual void emitStoreShared(Register source, const image_variable *var, bool is_local, int size, codeGen &gen);


    virtual void emitStoreOrigRegister(Address, Register, codeGen &) { assert(0); }

    virtual bool emitMoveRegToReg(Register, Register, codeGen &) { assert(0); return 0;}
    virtual bool emitMoveRegToReg(registerSlot *src, registerSlot *dest, codeGen &gen);
    // This one we actually use now.
    virtual Register emitCall(opCode, codeGen &, const pdvector<AstNodePtr> &,
			      bool, int_function *) = 0;

    virtual void emitGetRetVal(Register, bool, codeGen &) { assert(0); }
    virtual void emitGetParam(Register, Register, instPointType_t, bool, codeGen &) { assert(0); }
    virtual void emitFuncJump(int_function*, instPointType_t, codeGen &) { assert(0); }
    virtual void emitASload(int, int, int, long, Register, codeGen &) { assert(0); }
    virtual void emitCSload(int, int, int, long, Register, codeGen &) { assert(0); }
    virtual void emitPushFlags(codeGen &) { assert(0); }
    virtual void emitRestoreFlags(codeGen &, unsigned) { assert(0); }
    // Built-in offset...
    virtual void emitRestoreFlagsFromStackSlot(codeGen &) { assert(0); }
    virtual bool emitBTSaves(baseTramp*, baseTrampInstance*, codeGen &) { assert(0); return true;}
    virtual bool emitBTRestores(baseTramp*, baseTrampInstance *, codeGen &) { assert(0); return true; }
    virtual void emitStoreImm(Address, int, codeGen &, bool) { assert(0); }
    virtual void emitAddSignedImm(Address, int, codeGen &, bool) { assert(0); }
    virtual int Register_DWARFtoMachineEnc(int) { assert(0); return 0;}
    virtual bool emitPush(codeGen &, Register) { assert(0); return true;}
    virtual bool emitPop(codeGen &, Register) { assert(0); return true;}
    virtual bool emitAdjustStackPointer(int, codeGen &) { assert(0); return true;}
    
    virtual bool clobberAllFuncCall(registerSpace *rs,int_function *callee);
};

class EmitterPOWERDyn : public EmitterPOWER {
 public:
    virtual ~EmitterPOWERDyn() {};

    Register emitCall(opCode op, codeGen &gen,
                      const pdvector<AstNodePtr> &operands,
                      bool noCost, int_function *callee);
};

class EmitterPOWERStat : public EmitterPOWER {
 public:
    virtual ~EmitterPOWERStat() {};
};

class EmitterPOWER32Dyn : public EmitterPOWERDyn {
 public:
    virtual ~EmitterPOWER32Dyn() {}
};

class EmitterPOWER32Stat : public EmitterPOWERStat {
 public:
    virtual ~EmitterPOWER32Stat() {}

    Register emitCall(opCode op, codeGen &gen,
                      const pdvector<AstNodePtr> &operands,
                      bool noCost, int_function *callee);
};

class EmitterPOWER64Dyn : public EmitterPOWERDyn {
 public:
    virtual ~EmitterPOWER64Dyn() {}

};

class EmitterPOWER64Stat : public EmitterPOWERStat {
 public:
    virtual ~EmitterPOWER64Stat() {}

    Register emitCall(opCode op, codeGen &gen,
                      const pdvector<AstNodePtr> &operands,
                      bool noCost, int_function *callee);
};

#endif
