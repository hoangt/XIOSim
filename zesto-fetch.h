#ifndef ZESTO_FETCH_INCLUDED
#define ZESTO_FETCH_INCLUDED

/* zesto-fetch.h - Zesto fetch stage class
 * 
 * Copyright � 2009 by Gabriel H. Loh and the Georgia Tech Research Corporation
 * Atlanta, GA  30332-0415
 * All Rights Reserved.
 * 
 * THIS IS A LEGAL DOCUMENT BY DOWNLOADING ZESTO, YOU ARE AGREEING TO THESE
 * TERMS AND CONDITIONS.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNERS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * NOTE: Portions of this release are directly derived from the SimpleScalar
 * Toolset (property of SimpleScalar LLC), and as such, those portions are
 * bound by the corresponding legal terms and conditions.  All source files
 * derived directly or in part from the SimpleScalar Toolset bear the original
 * user agreement.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Georgia Tech Research Corporation nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * 4. Zesto is distributed freely for commercial and non-commercial use.  Note,
 * however, that the portions derived from the SimpleScalar Toolset are bound
 * by the terms and agreements set forth by SimpleScalar, LLC.  In particular:
 * 
 *   "Nonprofit and noncommercial use is encouraged. SimpleScalar may be
 *   downloaded, compiled, executed, copied, and modified solely for nonprofit,
 *   educational, noncommercial research, and noncommercial scholarship
 *   purposes provided that this notice in its entirety accompanies all copies.
 *   Copies of the modified software can be delivered to persons who use it
 *   solely for nonprofit, educational, noncommercial research, and
 *   noncommercial scholarship purposes provided that this notice in its
 *   entirety accompanies all copies."
 * 
 * User is responsible for reading and adhering to the terms set forth by
 * SimpleScalar, LLC where appropriate.
 * 
 * 5. No nonprofit user may place any restrictions on the use of this software,
 * including as modified by the user, by any other authorized user.
 * 
 * 6. Noncommercial and nonprofit users may distribute copies of Zesto in
 * compiled or executable form as set forth in Section 2, provided that either:
 * (A) it is accompanied by the corresponding machine-readable source code, or
 * (B) it is accompanied by a written offer, with no time limit, to give anyone
 * a machine-readable copy of the corresponding source code in return for
 * reimbursement of the cost of distribution. This written offer must permit
 * verbatim duplication by anyone, or (C) it is distributed by someone who
 * received only the executable form, and is accompanied by a copy of the
 * written offer of source code.
 * 
 * 7. Zesto was developed by Gabriel H. Loh, Ph.D.  US Mail: 266 Ferst Drive,
 * Georgia Institute of Technology, Atlanta, GA 30332-0765
 */

class core_fetch_t {

  public:

  md_addr_t PC;
  md_addr_t feeder_NPC; // What the instruction feeder sends us as next pc
  md_addr_t feeder_PC;  // Same for current pc
  md_addr_t feeder_ftPC;// Same for fallthough pc
  bool fake_insn;       // Instruction that we artificially injected
  bool prev_insn_fake;  // Same for previously fetched instruction
  bool taken_branch;    // Is this a taken branch, regardless of npc
  bool bogus; /* TRUE if oracle is on wrong path and encountered an invalid inst */
  bool invalid; /* TRUE if oracle encounters an instruction it doesn't know (which is fine if we are running under an instruction feeder */
  class bpred_t * bpred;

  /* constructor, stats registration */
  core_fetch_t(void);
  virtual ~core_fetch_t();
  virtual void reg_stats(struct stat_sdb_t * const sdb) = 0;
  virtual void update_occupancy(void) = 0;

  //Handles events before the actual fetch (cache requests, jeclears, etc.)
  virtual void pre_fetch(void) = 0;
  //Fetch a Mop from the current PC, returns true if more Mops can be fetched this cycle
  //possibly called multiple times a cycle
  virtual bool do_fetch(void) = 0;
  //Handles events after the actual fetch (predecode pipe, insert into decode, etc.)
  virtual void post_fetch(void) = 0;

  /* simulate one cycle */
  virtual void step(void)
  {
     post_fetch();
     while(do_fetch());
     pre_fetch();
  }

  /* interface to decode stage:
     Mop_available() returns true if front-end has a Mop ready
     Mop_peek() returns a pointer to that Mop without removing it from fetch
     Mop_consume() removes the Mop from the front-end */
  virtual bool Mop_available(void) = 0;
  virtual struct Mop_t * Mop_peek(void) = 0;
  virtual void Mop_consume(void) = 0;

  /* enqueue a jeclear event into the jeclear_pipe */
  virtual void jeclear_enqueue(struct Mop_t * const Mop, const md_addr_t New_PC) = 0;
  /* recover the front-end after the recover request actually
     reaches the front end. Blows everything away if new_PC==0 */
  virtual void recover(const md_addr_t new_PC) = 0;

  protected:

  struct core_t * core;
};


class core_fetch_t * fetch_create(const char * fetch_opt_string, struct core_t * core);

#endif /* ZESTO_FETCH_INCLUDED */
