#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include "common.h"
#include "utils.h"
#include <athread.h>
#include <time.h>

const char* version_name = "A reference version of edge-based load balancing";
extern void SLAVE_FUN(prepare)(); 
extern void SLAVE_FUN(search)();
int *data_info; // 每个从核每轮迭代处理结点个数  // 每个从核每轮迭代处理数据的起始地址 
index_t* buffer_s;
index_t* buffer_e; //  发送给从核的边缓冲�?

index_t *mypred; // 前缀
index_t *bound; // 边界顶点
index_t *prefix; // 边界顶点的前�?
index_t *buffer;
index_t *nextLayer; // 下一层顶�?
index_t *prefix_nx;

int r_id, c_id;
int col_num;
MPI_Comm row_comm;
MPI_Comm col_comm;

index_t *v_pos;
index_t *e_dst;
int offset_v;
// 同行进程数据偏移
int row_v_count[8];
int row_v_displs[8];

index_t *row_v_pos; // 存储一行的数据
index_t *row_e_dst;

// 合并以后边数和点�?
int row_local_e;
int row_local_v;

int vpos_count[9]; // 每一行结点数量的前缀�?
int vpos_count_second[8];
int size_b, size_n;
int recvcounts[8]; // 列通信域进行gather时的参数
int displs[8];

void preprocess(dist_graph_t *graph) { 
	int p_id = graph->p_id;
	int p_num = graph->p_num;
	int local_e = graph->local_e;
	int local_v = graph->local_v;
	
//*****************通信域划�?*******************************/
	col_num = sqrt(p_num); // 划分为二�? col_num 为列�?
	MPI_Comm_split(MPI_COMM_WORLD, p_id / col_num, p_id % col_num, &row_comm); // 创建行通信�?
	MPI_Comm_split(MPI_COMM_WORLD, p_id % col_num, p_id / col_num, &col_comm); // 创建列通信�?
	MPI_Comm_rank(row_comm, &c_id); // 进程在行通信域中的id
	MPI_Comm_rank(col_comm, &r_id); // 列通信域中的id
//***************************************************************/
	int row_e_count[8];
	int row_e_displs[8];
//*****************构�?行内gatherv 参数 *******************************/
	MPI_Allgather(&local_v, 1, MPI_INT, row_v_count, 1, MPI_INT, row_comm);
	MPI_Allgather(&local_e, 1, MPI_INT, row_e_count, 1, MPI_INT, row_comm);

	int v_pre = 0;
	int e_pre = 0; 
	for(int i = 0; i < col_num; i++){
		row_v_displs[i] = v_pre;
		row_e_displs[i] = e_pre;
		v_pre += row_v_count[i];
		e_pre += row_e_count[i];
	}

//***********************************************************************
	row_local_v = v_pre; // 合并以后的点数量
	row_local_e = e_pre; // 合并以后的边数量
	row_v_pos = (index_t*)malloc(sizeof(index_t) * (row_local_v + 1)); // 行缓冲区
	row_e_dst = (index_t*)malloc(sizeof(index_t) * row_local_e); 
	
	// 行内gather数据
 	MPI_Allgatherv(graph->v_pos, local_v, MPI_INT, row_v_pos, row_v_count, row_v_displs, MPI_INT, row_comm);
	MPI_Allgatherv(graph->e_dst, local_e, MPI_INT, row_e_dst, row_e_count, row_e_displs, MPI_INT, row_comm);
	row_v_pos[row_local_v] = row_v_pos[0] + row_local_e; //哨兵

	// 列通信，得到每一行结点的数量		
	MPI_Allgather(&row_local_v, 1, MPI_INT, vpos_count, 1, MPI_INT, col_comm);
	// 求前面行的结点数�?
	offset_v = 0;
	for(int i = 0; i < col_num; i++){
		int temp = vpos_count[i];
		vpos_count[i] = offset_v;
		offset_v += temp;
		vpos_count_second[i] = offset_v;
	}  
	vpos_count[col_num] = offset_v;
	offset_v = vpos_count[r_id];

	// 存储进程本地数据
	v_pos =  (index_t*)malloc(sizeof(index_t) * (row_local_v + 1));
	e_dst =  (index_t*)malloc(sizeof(index_t) * row_local_e); //max size

	mypred = (index_t*)malloc(sizeof(index_t) * row_local_v);
	buffer = (index_t*)malloc(sizeof(index_t) * row_local_e); 
	bound =  (index_t*)malloc(sizeof(index_t) * graph->global_e);
	prefix = (index_t*)malloc(sizeof(index_t) * graph->global_e);
	nextLayer = (index_t*)malloc(sizeof(index_t) * row_local_e); 
	prefix_nx = (index_t*)malloc(sizeof(index_t) * row_local_e);

	//从核部分所要用到的函数
	buffer_s = (index_t*)malloc(sizeof(index_t) * row_local_e); //
	buffer_e = (index_t*)malloc(sizeof(index_t) * row_local_e); 
	data_info = (int*)malloc(sizeof(int) * 129);
	data_info[128] = offset_v;
	
	athread_init();
//****************构�?属于本进程的数据  *********************** ********
	int p = 0; // �ߵ��±�   
	for(int i = 0; i < row_local_v; i++){   
		v_pos[i] = p; // �������У�ǰ��ĵ��ж��ٱ�
		int begin = row_v_pos[i] - row_v_pos[0];
		int end = row_v_pos[i + 1] - row_v_pos[0];
		for(int e = begin; e < end; e++){
			int v = row_e_dst[e];
			if (vpos_count[c_id] <= v && v < vpos_count_second[c_id]){
				e_dst[p++] = v;
			}
		}
	}
	v_pos[row_local_v] = p;
	free(row_v_pos);
	free(row_e_dst);
//**************************************************************
}
void bfs(dist_graph_t *graph, index_t s, index_t* pred){
	memset(mypred, UNREACHABLE, sizeof(int) * row_local_v);
	size_b = 0;
	if(vpos_count[c_id] <= s && s < vpos_count_second[c_id]){
		bound[0] = s;
		prefix[0] = s;
		size_b = 1;
	}
	int newnode;
	do{ 
		if(size_b < 64){
			size_n = 0;
			for(int i = 0; i < size_b; i++){
				int u = bound[i]; 
				if(mypred[u - offset_v] == UNREACHABLE){ 
					int begin = v_pos[u - offset_v];  
					int end = v_pos[u + 1 - offset_v];
					mypred[u - offset_v] = prefix[i];
					for(int j = begin; j < end; j++){
						nextLayer[size_n] = e_dst[j];
						prefix_nx[size_n] = u; 
						size_n++;
					}	
				}
			} 
		}else{  
			int q = size_b / 64;
			int r = size_b % 64;
			for(int i = 0; i < 64; i++){
				data_info[i] = q + ((i < r) ? 1 : 0);
				data_info[i + 64] = i * q + ((i < r) ? i : r);
			}
			athread_spawn(search, 0);
			athread_join();
		}
		MPI_Barrier(MPI_COMM_WORLD);
		MPI_Allgather(&size_n, 1, MPI_INT, recvcounts, 1, MPI_INT, col_comm); // 选择对角线进程作为根节点，便于之后行广播	
	
		int pre = 0;
		for(int i = 0; i < col_num; i++){
			displs[i] = pre;
			pre += recvcounts[i];
		}
		size_b = pre; 
		
		MPI_Bcast(&size_b, 1, MPI_INT, r_id, row_comm);

		MPI_Gatherv(nextLayer, size_n, MPI_INT, bound, recvcounts, displs, MPI_INT, c_id, col_comm);
		MPI_Gatherv(prefix_nx, size_n, MPI_INT, prefix, recvcounts, displs, MPI_INT, c_id, col_comm);

		MPI_Bcast(bound, size_b, MPI_INT, r_id, row_comm);
		MPI_Bcast(prefix, size_b, MPI_INT, r_id, row_comm); 
				
		MPI_Allreduce(&size_b, &newnode, 1, MPI_INT, MPI_SUM, col_comm);
	}while(newnode > 0);
	MPI_Scatterv(mypred, row_v_count, row_v_displs, MPI_INT, pred, graph->local_v, MPI_INT, r_id, row_comm);	 
}

void destroy_additional_info(void *additional_info) {
	free(additional_info);
}