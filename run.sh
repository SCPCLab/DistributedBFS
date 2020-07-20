# !/bin/bash

#if [ "$#" -ne 3 ]; then
#  echo "Usage: $0 <executable> <data name> <process per node> " >&2
#  exit 1
#fi
echo "---num  1  as graph-sequential---  or  ---num  2  as  graph-load-balance---  "
read excutable 
if [ $excutable -eq 1 ];then 
	EXECUTABLE="graph-sequential"
	echo  $EXECUTABLE
elif [ $excutable -eq 2 ];then 
	EXECUTABLE="graph-load-balance"
	echo  $EXECUTABLE
else 
	echo "input error"
	exit 1
fi

datanum=(Freescale1  cage15  road_usa  europe_osm)
for((i=0;i<4;i++))
do
	index=$(($i+1));
	#echo $index
	echo "----input  ${index} to operate ${datanum[$i]}----"
done
read  mychoose
if  [  $mychoose -eq 1  ]; then 
	DATAFILE=${datanum[0]};
	echo $DATAFILE
elif  [  $mychoose -eq 2  ]; then 
	DATAFILE=${datanum[1]};
	echo $DATAFILE
elif  [  $mychoose -eq 3  ]; then 
	DATAFILE=${datanum[2]};
	echo $DATAFILE
elif  [  $mychoose -eq 4  ]; then
	DATAFILE=${datanum[3]};
	echo $DATAFILE
else 
	echo "input error"
	exit 1 
fi

echo "-----input your proccess number-----"
read PROC_NUM 
echo $PROC_NUM 

function select_queue() {
	node=$((${1}/4))
	load=$(mqload -w | grep -P "q_sw_${2}[a-z0-9_]* " -o)
	if [ "$load" = "" ]; then
		load=$(mqload -w | grep -P "q_sw_[a-z0-9_]* " -o)
	fi
	arr=(${load//,/})
	for var in ${arr[@]}
	do
		load=$(mqload ${var} -w | grep -P "(?<=${var}).*" -o | grep -P "\d+" -o)
		tmp=(${load//,/})
		if [ $node -le ${tmp[3]} ]; then
			echo ${var}
			return 0
		fi
	done
	return 1
}

export DAPL_DBG_TYPE=0

if [ ! -d "logs" ]; then
    mkdir logs
fi
DATAPATH=/home/export/online1/cpc/graph_data/
REP=3

#EXECUTABLE=$1
#DATAFILE=$2
#PROC_NUM=$3
queue=$(select_queue ${PROC_NUM} cpc)
if [ $? -eq 1 ]; then
	echo -e "\033[1;31mnode is not enough !\033[0m"
	mqload -w 
else
	suffix=$(date +%Y%m%d-%H%M%S)
	bsub -I -b -q $queue -host_stack 1024 -share_size 4096 -n ${PROC_NUM} -cgsp 64 ./${EXECUTABLE} ${DATAPATH}/${DATAFILE}.csr ${REP} 2>&1 | tee logs/log.${suffix}
fi
