/* zesto-exec.cpp - Zesto execution stage class
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

#include <cstddef>

#include "thread.h"
#include "synchronization.h"

#include "zesto-cache.h"
#include "zesto-noc.h"
#include "zesto-repeater.h"
#include "zesto-core.h"
#include "zesto-oracle.h"
#include "zesto-alloc.h"
#include "zesto-fetch.h"
#include "zesto-exec.h"
#include "zesto-commit.h"
#include "zesto-memdep.h"
#include "zesto-prefetch.h"
#include "zesto-uncore.h"
#include "zesto-coherence.h"
#include "helix.h"

/* default constructor */
core_exec_t::core_exec_t(void):
  last_completed(0)
{
}

/* default destructor */
core_exec_t::~core_exec_t()
{
}

/* update deadlock watchdog timestamp */
void core_exec_t::update_last_completed(tick_t now)
{
  last_completed = now;
  core->commit->deadlocked = false;
}

enum cache_command core_exec_t::get_STQ_request_type(const struct uop_t * const uop)
{
  if(!uop->oracle.is_sync_op)
    return CACHE_WRITE;

  return is_addr_helix_signal(uop->oracle.virt_addr) ? CACHE_SIGNAL : CACHE_WAIT;
}

extern int min_coreID;

/* load in all definitions */
#include "ZPIPE-exec.list"


class core_exec_t * exec_create(const char * exec_opt_string, struct core_t * core)
{
#define ZESTO_PARSE_ARGS
#include "ZPIPE-exec.list"

  fatal("unknown exec type \"%s\"",exec_opt_string);
#undef ZESTO_PARSE_ARGS
}
