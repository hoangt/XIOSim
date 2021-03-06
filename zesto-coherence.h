#ifndef ZESTO_COHERENCE_INCLUDED
#define ZESTO_COHERENCE_INCLUDED

enum controller_response_t { MSHR_CHECK_ARRAY, MSHR_STALL };
enum controller_array_response_t { ARRAY_HIT, ARRAY_MISS };

class cache_controller_t {
  public:
  cache_controller_t (
    struct core_t * const core,
    struct cache_t * const cache);

  virtual controller_array_response_t check_array(struct cache_line_t * line) = 0;
  virtual controller_response_t check_MSHR(struct cache_action_t * MSHR_item) = 0;

  virtual bool can_schedule_upstream() = 0;
  virtual bool can_schedule_downstream(struct cache_t * const prev_cache) = 0;
  virtual bool send_request_upstream(int bank, int MSHR_index, struct cache_action_t * MSHR) = 0;
  virtual void send_response_downstream(struct cache_action_t * const MSHR) = 0;

  virtual void reg_stats(struct stat_sdb_t * const sdb) { };

  protected:
  struct cache_t * const cache;
  struct core_t * const core;

};

class LLC_controller_t {

};

class cache_controller_t * controller_create(const char * controller_opt_string, struct core_t * core, struct cache_t * cache);

#endif /*ZESTO_COHERENCE*/
