#define _GNU_SOURCE
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <mpi.h>
#include "common.h"
#include "utils.h"


int my_file_read_at_all(MPI_File fh, MPI_Offset offset, void *buf,\
                        int count, MPI_Datatype datatype);
int read_graph_impl(dist_graph_t *graph, MPI_File file, \
                  int global_v, int local_v, int offset_v);

/*
typedef struct {
    int global_v, local_v, offset_v;
    const int* v_count;
    const int* v_displs;
} tree_info_t;*/



int read_graph_default(dist_graph_t *graph, const char* filename) {
    MPI_File file;
    int global_v, global_e;
    int local_v, offset_v, r, q;
    index_t* v_pos;
    index_t *e_dst = NULL;
    int ret = MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    CHECK_ERROR(ret, ret)
    ret = my_file_read_at_all(file, 0, &global_v, 1, MPI_INT);
    CHECK_ERROR(ret, ret)

    q = global_v / graph->p_num;
    r = global_v % graph->p_num;
    local_v = q + ((graph->p_id < r) ? 1 : 0);
    offset_v = graph->p_id * q + ((graph->p_id < r) ? graph->p_id : r);

    ret = read_graph_impl(graph, file, global_v, local_v, offset_v);
    MPI_File_close(&file);
    return ret;
}

int read_graph_with_distribution(dist_graph_t *graph, const char* filename, \
                    int local_v, int offset_v) {
    MPI_File file;
    int global_v, global_e;
    index_t* v_pos;
    index_t *e_dst = NULL;
    int ret = MPI_File_open(MPI_COMM_WORLD, filename, MPI_MODE_RDONLY, MPI_INFO_NULL, &file);
    CHECK_ERROR(ret, ret)
    ret = my_file_read_at_all(file, 0, &global_v, 1, MPI_INT);
    CHECK_ERROR(ret, ret)

    ret = read_graph_impl(graph, file, global_v, local_v, offset_v);
    MPI_File_close(&file);
    return ret;
}

int read_graph_impl(dist_graph_t *graph, MPI_File file, \
                  int global_v, int local_v, int offset_v) {
    MPI_Offset file_offset;
    int global_e, local_e;
    index_t* v_pos = NULL;
    index_t *e_dst = NULL;
    int ret;

    v_pos = (index_t*)malloc(sizeof(index_t) * (local_v + 1));
    CHECK(v_pos == NULL, MPI_ERR_NO_MEM)

    file_offset = (offset_v + 1) * sizeof(int);
    ret = my_file_read_at_all(file, file_offset, v_pos, local_v + 1, MPI_INT);
    CHECK_ERROR(ret, ret)
    global_e = v_pos[local_v];
    MPI_Bcast(&global_e, 1, MPI_INT, graph->p_num-1, MPI_COMM_WORLD);
    
    local_e = v_pos[local_v] - v_pos[0];
    if(local_e > 0) {
        e_dst = (index_t*) malloc(sizeof(index_t) * local_e);
        CHECK_NULL(e_dst, MPI_ERR_NO_MEM)
    }

    file_offset = (global_v + 2 + v_pos[0]) * sizeof(int);
    ret = my_file_read_at_all(file, file_offset, e_dst, local_e, MPI_INT);
    CHECK_ERROR(ret, ret)

    graph->global_v = global_v;
    graph->global_e = global_e;
    graph->local_v = local_v;
    graph->offset_v = offset_v;
    graph->local_e = local_e;
    graph->v_pos = v_pos;
    graph->e_dst = e_dst;
    graph->additional_info = NULL;
    graph->is_null = false;
    return MPI_SUCCESS;
}

int my_file_read_at_all(MPI_File fh, MPI_Offset offset, void *buf,\
                        int count, MPI_Datatype datatype) {
    int actual_count;
    MPI_Status status;
    int ret = MPI_File_read_at_all(fh, offset, buf, count, datatype, &status);
    CHECK_ERROR(ret, ret)
    ret = MPI_Get_count(&status, datatype, &actual_count);
    CHECK(count != actual_count, MPI_ERR_IO)
    return ret;
}


int fatal_error(int code) {
    return MPI_Abort(MPI_COMM_WORLD, code);
}

void destroy_dist_graph(dist_graph_t *graph) {
    if(graph->additional_info != NULL){
        destroy_additional_info(graph->additional_info);
        graph->additional_info = NULL;
    }
    if(graph->v_pos != NULL){
        free(graph->v_pos);
        graph->v_pos = NULL;
    }
    if(graph->e_dst != NULL){
        free(graph->e_dst);
        graph->e_dst = NULL;
    }
}

