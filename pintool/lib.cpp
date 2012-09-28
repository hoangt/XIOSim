/*
 * Pin-specific declarations, so that we can build Zesto as
 * a library and not link it with pin.
 * Copyright, Svilen Kanev, 2012
 */

#include <map>
#include <queue>

#include "../synchronization.h"

XIOSIM_LOCK memory_lock;
XIOSIM_LOCK cache_lock;
XIOSIM_LOCK cycle_lock;
XIOSIM_LOCK core_pools_lock;
XIOSIM_LOCK oracle_pools_lock;

XIOSIM_LOCK printing_lock;
XIOSIM_LOCK repeater_lock;
