#include <slave.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
 
#define NODE_SIZE 2048
#define  EDGE_SIZE 4096
// 鍚戝悓琛岀洰鏍囦粠鏍搁€佹暟
#define LONG_PUTR(var,dest) \
asm volatile ("putr %0,%1\n"::"r"(var),"r"(dest):"memory")
// 璇昏 岄€氫俊缂撳啿
#define LONG_GETR(var) \
asm volatile ("getr %0\n":"=r"(var)::"memory")
// 鍚戝悓鍒楃洰鏍囦粠鏍搁€佹暟
#define LONG_PUTC(var,dest) \
asm volatile ("putc %0,%1\n"::"r"(var),"r"(dest):"memory")
// 璇诲垪閫氫俊缂撳啿
#define LONG_GETC(var) \
asm volatile ("getc %0\n":"=r"(var)::"memory")

extern int *data_info; // 姣忎釜浠庢牳姣忚疆杩 浠ｅ 勭悊缁撶偣涓 鏁  
extern index_t *bound;
extern index_t *prefix;
extern index_t *mypred;
extern index_t *v_pos;
extern index_t *e_dst;
extern index_t *nextLayer;
extern index_t *prefix_nx;
extern int size_n;


__thread_local volatile index_t  get_reply, put_reply;
__thread_local int t_id; // 绾跨▼鍙?
__thread_local int cnt; // 鏈 浠庢牳鏁版嵁澶у ?
__thread_local int tdispls;  // 浠庢牳涔嬮棿鐨勫亸绉婚噺
void myReduce_prefix();
void wait_reply(volatile  index_t *reply, int m){
	while(*reply != m) {};
}
void slave_prepare()
{
}
void slave_search()
{
	index_t nodes[NODE_SIZE];
	index_t pre[NODE_SIZE];
	index_t tbuf_node[EDGE_SIZE];
	index_t tbuf_prefix[EDGE_SIZE];

	t_id = athread_get_id(-1);

	int sdata_info[129];
	get_reply = 0;
	athread_get(PE_MODE, data_info, sdata_info, sizeof(int) * 129, &get_reply, 0, 0, 0);
	wait_reply(&get_reply, 1);

	int len_v = sdata_info[t_id];
	int st_pos = sdata_info[64 + t_id]; // 璧峰 嬪湴鍧€
	int offset_v = sdata_info[128];

	int posation[2];

	get_reply = 0;
	athread_get(PE_MODE, &(bound[st_pos]), nodes, sizeof(index_t) * len_v, &get_reply, 0, 0, 0);
	wait_reply(&get_reply, 1);
	get_reply = 0;
	athread_get(PE_MODE, &(prefix[st_pos]), pre, sizeof(index_t) * len_v, &get_reply, 0, 0, 0);
	wait_reply(&get_reply, 1);

	int i, j, u, v;	
	int edges[256];
	cnt = 0;
	for(i = 0; i < len_v; i++){ // 椤剁偣
		int u = nodes[i];
		if(mypred[u - offset_v] == UNREACHABLE){
			mypred[u - offset_v] = pre[i];

			int begin = v_pos[u - offset_v]; int end = v_pos[u + 1 - offset_v];
			int len_e = end - begin;

			if(len_e > 0){
				get_reply = 0; 
				athread_get(PE_MODE, &(e_dst[begin]), edges, sizeof(index_t) * len_e, &get_reply, 0, 0, 0);
				wait_reply(&get_reply, 1);
			}
			for(j = 0; j < len_e; j++){ // 杈?
				v = edges[j];
				tbuf_node[cnt] = v;
				tbuf_prefix[cnt] = u;
				cnt++;
			}
		}
	}
	myReduce_prefix(); // 姹傚嚭姣忎釜绾跨▼鍋忕Щ閲忕殑鍓嶇紑鍜?tdispls 
	if(cnt > 0){
		put_reply = 0;
		athread_put(	PE_MODE, 
				tbuf_node, 
				&(nextLayer[tdispls]),
				sizeof(index_t) * cnt,
				&put_reply,
				0, 0);
		wait_reply(&put_reply, 1);
		put_reply = 0;
		athread_put(	PE_MODE, 
				tbuf_prefix, 
				&(prefix_nx[tdispls]),
				sizeof(index_t) * cnt,
				&put_reply,
				0, 0);
		wait_reply(&put_reply, 1);
	}
	if(t_id == 63){
		int size = tdispls + cnt;
		put_reply = 0;
		athread_put(PE_MODE, &size, &size_n, sizeof(int), &put_reply, 0, 0);
		wait_reply(&put_reply, 1);
	}
}

void myReduce_prefix()
{
	int i, k, temp, t, rsum, col_id, row_id;

	col_id = t_id % 8;
	row_id = t_id / 8;

	t = cnt;
	tdispls = 0;	
	for(i = 0; i < 7; i++){
		if(col_id == i) LONG_PUTR(t, i + 1); // 鍚戜笅涓€鍒楀彂閫?
		if(col_id == i + 1){
			LONG_GETR(tdispls);
			t += tdispls;
		}	
	}
	rsum = t;
	if(col_id == 7) LONG_PUTR(rsum, 8); // 琛屽箍鎾?
	if(col_id < 7) LONG_GETR(rsum);

	for(i = 0; i < 7; i++){
		if(row_id == i) LONG_PUTC(rsum, i + 1); //鍚戜笅涓€琛屽彂閫?
		if(row_id == i + 1){
			LONG_GETC(temp);
			tdispls += temp;
			rsum += temp;
		}
	}
}
