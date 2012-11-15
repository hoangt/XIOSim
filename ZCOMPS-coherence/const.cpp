/* const.cpp - Const-latency coherency */

#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(controller_opt_string,"const"))
    return new cache_controller_const_t(core, cache, controller_opt_string);
#else

class cache_controller_const_t : public cache_controller_t {
  public:
  cache_controller_const_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string);


  virtual controller_array_response_t check_array(struct cache_line_t * line);
  virtual controller_response_t check_MSHR(struct cache_action_t * MSHR_item);

  virtual bool can_schedule_upstream();
  virtual bool can_schedule_downstream(struct cache_t * const prev_cache);
  virtual bool send_request_upstream(int bank, int MSHR_index, struct cache_action_t * MSHR);
  virtual void send_response_downstream(struct cache_action_t * const MSHR);

  protected:
  unsigned int sharing_penalty;

  static const unsigned int PRODUCER_INVALID = (unsigned int)-1;

  static XIOSIM_LOCK lk_controller;
  static std::map<md_paddr_t, unsigned> writers;
};

struct const_coherence_data : public line_coherence_data_t {
  bool is_used() { return v > 0; }
};

cache_controller_const_t::cache_controller_const_t(struct core_t * const core, struct cache_t * const cache, const char * opt_string) :
  cache_controller_t(core, cache)
{
  char name[256];
  if(sscanf(opt_string, "%[^:]:%u", name, &sharing_penalty))
    fatal("couldn't parse const contrller options <name:penalty>");

  lk_init(&lk_controller);
}

controller_array_response_t cache_controller_const_t::check_array(struct cache_line_t * line)
{
  if (line == NULL)
    return ARRAY_MISS;

  return ARRAY_HIT;
}

controller_response_t cache_controller_const_t::check_MSHR(struct cache_action_t * MSHR_item)
{
  (void) MSHR_item;

  return MSHR_CHECK_ARRAY;
}

bool cache_controller_const_t::can_schedule_upstream()
{
  if (cache->next_level)
    return (!cache->next_bus || bus_free(cache->next_bus));
  return bus_free(uncore->fsb);
}

bool cache_controller_const_t::can_schedule_downstream(struct cache_t * const prev_cache)
{
  return (!prev_cache || bus_free(prev_cache->next_bus));
}

bool cache_controller_const_t::send_request_upstream(int bank, int MSHR_index, struct cache_action_t * MSHR)
{
  if(cache->next_level) /* enqueue the request to the next-level cache */
  {
    if(!cache_enqueuable(cache->next_level, DO_NOT_TRANSLATE, MSHR->paddr))
      return false;

    /* write miss means this is the last owner */
    if(MSHR->cmd == CACHE_WRITE)
    {
      lk_lock(&lk_controller, 1);
      writers[MSHR->cmd] = (unsigned)MSHR->core->id;
      lk_unlock(&lk_controller);
    }

    /* writeback means noone is the last owner */
    if(MSHR->cmd == CACHE_WRITEBACK)
    {
      lk_lock(&lk_controller, 1);
      writers.erase(MSHR->paddr);
      lk_unlock(&lk_controller);
    }

    cache_enqueue(MSHR->core, cache->next_level, cache,MSHR->cmd, DO_NOT_TRANSLATE, MSHR->PC, MSHR->paddr, MSHR->action_id, bank, MSHR_index, MSHR->op, MSHR->cb, MSHR->miss_cb, NULL, MSHR->get_action_id);

    bus_use(cache->next_bus, (MSHR->cmd == CACHE_WRITE || MSHR->cmd == CACHE_WRITEBACK) ? cache->linesize : 1, MSHR->cmd == CACHE_PREFETCH);
  }
  else /* or if there is no next level, enqueue to the memory controller */
  {
    if(!uncore->MC->enqueuable())
      return false;

    uncore->MC->enqueue(cache, MSHR->cmd, MSHR->paddr, cache->linesize, MSHR->action_id, bank, MSHR_index, MSHR->op, MSHR->cb, MSHR->get_action_id);

    bus_use(uncore->fsb, (MSHR->cmd == CACHE_WRITE || MSHR->cmd == CACHE_WRITEBACK) ? (cache->linesize>>uncore->fsb_DDR) : 1, MSHR->cmd == CACHE_PREFETCH);
  }

  MSHR->when_started = sim_cycle;
  return true;
}

void cache_controller_const_t::send_response_downstream(struct cache_action_t * const MSHR)
{
  if(MSHR->prev_cp)
  {
    unsigned int last_producer;
    lk_lock(&lk_controller, 1);
    if (writers.find(MSHR->paddr) == writers.end())
      last_producer = PRODUCER_INVALID;
    else
      last_producer = writers[MSHR->paddr];
    lk_unlock(&lk_controller);

    tick_t delay = 0;
    if(last_producer != PRODUCER_INVALID && last_producer != (unsigned)MSHR->core->id)
      delay = sharing_penalty;

    /* everyone but the response from a writeback transfers a full cache line */
    bus_use(MSHR->prev_cp->next_bus, (MSHR->cmd == CACHE_WRITEBACK) ? 1 : MSHR->prev_cp->linesize, MSHR->cmd == CACHE_PREFETCH);
    fill_arrived(MSHR->prev_cp, MSHR->MSHR_bank, MSHR->MSHR_index, delay);
  }
}

#endif /* ZESTO_PARSE_ARGS */
