#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED 1

#include<stdint.h>
#include<stdbool.h>

#define UNREACHABLE (-1)
typedef int32_t index_t;

/* 
 * global_v: number of vertices in the whole input graph
 * global_e: number of edges in the whole input graph
 * local_v: number of vertices in the current process
 * offset_v: number of vertices in previous processes
 * local_e: number of edges in the current process
 */
typedef struct {
    int p_id, p_num;                 /* do not modify */
    int global_v, global_e;          /* do not modify */
    int local_v, offset_v, local_e;  /* do not modify after preprocess */
    
    index_t* v_pos;
    index_t* e_dst;

    bool is_null;           /* if 2D partitioned */
    int local_v_src, local_v_dst;    /* may be used for 2D partitioning */
    int offset_v_src, offset_v_dst;  /* may be used for 2D partitioning */

    void *additional_info;           /* any information you want to attach */
} dist_graph_t;

#ifdef __cplusplus
extern "C" {
#endif

void preprocess(dist_graph_t *graph);
void destroy_additional_info(void *additional_info);
void bfs(dist_graph_t *graph, index_t s, index_t* pred);

#ifdef __cplusplus
}
#endif

#endif
