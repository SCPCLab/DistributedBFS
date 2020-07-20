#ifndef CHECK_H_INCLUDED
#define CHECK_H_INCLUDED 1

#include "common.h"

#define CHECK(err, err_code) if(err) { return err_code; }
#define CHECK_ERROR(ret, err_code) CHECK(ret != 0, err_code)
#define CHECK_NULL(ret, err_code) CHECK(ret == NULL, err_code)

#ifdef __cplusplus
extern "C" {
#endif

    int check_answer(dist_graph_t* check_graph, dist_graph_t* graph, const char* filename, index_t s, \
        const index_t* pred);

    int get_te(const dist_graph_t* graph, const index_t* pred);

#ifdef __cplusplus
}
#endif

#endif
