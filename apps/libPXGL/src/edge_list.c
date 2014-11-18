#define __STDC_FORMAT_MACROS

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>

#include "hpx/hpx.h"
#include "libsync/sync.h"
#include "libpxgl/edge_list.h"

static hpx_action_t _put_edge_edgelist;
static int _put_edge_edgelist_action(edge_list_edge_t *e)
{
   const hpx_addr_t target = hpx_thread_current_target();

   edge_list_edge_t *edge;
   if (!hpx_gas_try_pin(target, (void**)&edge))
     return HPX_RESEND;

   memcpy(edge, e, sizeof(*e));
   hpx_gas_unpin(target);
   return HPX_SUCCESS;
}

typedef struct {
  unsigned int edges_skip;
  unsigned int edges_no;
  hpx_addr_t edges_sync;
  edge_list_t el;
  char filename[];
} _edge_list_from_file_local_args_t;
hpx_action_t _edge_list_from_file_local = 0;
int _edge_list_from_file_local_action(const _edge_list_from_file_local_args_t *args) {
  // Read from the edge-list filename
  FILE *f = fopen(args->filename, "r");
  assert(f);

  edge_list_edge_t *edge = malloc(sizeof(*edge));
  assert(edge);

  sssp_uint_t skipped = 0;
  sssp_uint_t count = 0;

  char line[LINE_MAX];
  while (fgets(line, sizeof(line), f) != NULL && count < args->edges_no) {
    switch (line[0]) {
    case 'c': continue;
    case 'a':
      if(skipped < args->edges_skip) {
	skipped++;
	continue;
      }
      sscanf(&line[1], " %" SSSP_UINT_PRI " %" SSSP_UINT_PRI " %" SSSP_UINT_PRI, &edge->source, &edge->dest, &edge->weight);
      // printf("%s", &line[1]);
      const sssp_uint_t position = count++ + skipped;
      hpx_addr_t e = hpx_addr_add(args->el.edge_list, position * sizeof(edge_list_edge_t), args->el.edge_list_bsize);
      assert(args->edges_sync != HPX_NULL);
      hpx_call(e, _put_edge_edgelist, edge, sizeof(*edge), args->edges_sync);
      continue;
    case 'p': continue;
    default:
      fprintf(stderr, "invalid command specifier '%c' in graph file. skipping..\n", line[0]);
      continue;
    }
  }
  free(edge);
  fclose(f);

  return HPX_SUCCESS;
}

hpx_action_t edge_list_from_file = 0;
int edge_list_from_file_action(const edge_list_from_file_args_t * const args) {
  // Read from the edge-list filename
  FILE *f = fopen(args->filename, "r");
  assert(f);
  edge_list_t *el = malloc(sizeof(*el));
  assert(el);
  hpx_addr_t edges_sync = HPX_NULL;

  printf("Starting DIMACS file reading\n");
  hpx_time_t now = hpx_time_now();
  char line[LINE_MAX];
  while (fgets(line, sizeof(line), f) != NULL && edges_sync == HPX_NULL) {
    switch (line[0]) {
    case 'c': continue;
    case 'a': continue;
    case 'p':
      sscanf(&line[1], " sp %" SSSP_UINT_PRI " %" SSSP_UINT_PRI, &el->num_vertices, &el->num_edges);
      // Account for the  DIMACS graph format (.gr) where the node
      // ids range from 1..n
      el->num_vertices++;
      // Set an appropriate block size
      el->edge_list_bsize = ((el->num_edges + HPX_LOCALITIES - 1) / HPX_LOCALITIES) * sizeof(edge_list_edge_t);
      // Allocate an edge_list array in the global address space
      el->edge_list = hpx_gas_global_alloc(HPX_LOCALITIES, el->edge_list_bsize);
      // Create synchronization lco
      edges_sync = hpx_lco_and_new(el->num_edges);
      continue;
    default:
      fprintf(stderr, "invalid command specifier '%c' in graph file. skipping..\n", line[0]);
      continue;
    }
  }
  assert(edges_sync != HPX_NULL);

  size_t filename_len = strlen(args->filename);
  const size_t local_args_size = sizeof(_edge_list_from_file_local_args_t) + filename_len;
  _edge_list_from_file_local_args_t *local_args = malloc(local_args_size);
  local_args->el = *el;
  local_args->edges_sync = edges_sync;
  memcpy(local_args->filename, args->filename, filename_len);
  const sssp_uint_t thread_chunk = el->num_edges / (args->thread_readers * args->locality_readers) + 1;
  local_args->edges_no = thread_chunk;
  for(unsigned int locality_desc = 0; locality_desc < args->locality_readers; ++locality_desc) {
    for(unsigned int thread_desc = 0; thread_desc < args->thread_readers; ++thread_desc) {
      local_args->edges_skip = ((locality_desc * args->thread_readers) + thread_desc) * thread_chunk;
      hpx_call(HPX_THERE(locality_desc), _edge_list_from_file_local, local_args, local_args_size, HPX_NULL);
    }
  }

  double elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Waiting for completion LCO.  Time took to start local read loops: %fs\n", elapsed);
  now = hpx_time_now();
  hpx_lco_wait(edges_sync);
  elapsed = hpx_time_elapsed_ms(now)/1e3;
  printf("Fininshed waiting for edge list completion.  Time waiting: %fs\n", elapsed);
  hpx_lco_delete(edges_sync, HPX_NULL);
  fclose(f);

  hpx_thread_continue_cleanup(sizeof(*el), el, free, el);
  return HPX_SUCCESS;
}


static __attribute__((constructor)) void _edge_list_register_actions() {
  _put_edge_edgelist  = HPX_REGISTER_ACTION(_put_edge_edgelist_action);
  edge_list_from_file = HPX_REGISTER_ACTION(edge_list_from_file_action);
  _edge_list_from_file_local = HPX_REGISTER_ACTION(_edge_list_from_file_local_action);
}
