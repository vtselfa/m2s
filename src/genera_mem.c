#include <stdio.h>
#include <stdlib.h>

void main (){
  char filename[100];
  FILE *fd;
  unsigned int nodos=1,i,j, channels, ranks,banks,mc=1;
int block;

  printf("Numero de nodos en el sistema:\n");
  scanf("%d",&nodos);

  printf("Numero de controladores:\n");
  scanf("%d",&mc);

  printf("Numero de canales por controlador:\n");
  scanf("%d",&channels);

  printf("Numero de rangos de la memoria principal:\n");
  scanf("%d",&ranks);

 printf("Numero de bancos por rango:\n");
  scanf("%d",&banks);

 printf("Numero de ways:\n");
  scanf("%d",&ways);

block=1024/ways;

  printf("Nombre del fichero de configuracion de red:\n");
  scanf("%s",filename);

  printf("Fichero: %s\n",filename);
  printf("Nodos totales: %d\n",nodos);
   printf("COntroladores: %d\n",mc);
   printf("Canales: %d\n",channels);
 printf("Rangos: %d\n",ranks);
  printf("Bancos: %d\n",banks);
	

  fd=fopen(filename,"w");

  fprintf(fd,"[ CacheGeometry l1 ]\nSets = 256\nAssoc = 2\nBlockSize = 64\nLatency = 1\n; Prefetch = 0\n\n[ CacheGeometry l2 ]\nSets = 1024\nAssoc = %d\nBlockSize = %d\nLatency = 10\n; Prefetch = 1\n\n#\n# Level 1 Caches\n#\n",ways,block);

  for(i=0;i<nodos;i++){
    fprintf(fd,"[ Module dl1-%d ]\n",i);
    fprintf(fd,"Type = Cache\n");
    fprintf(fd,"Geometry = l1\n");
    fprintf(fd,"LowNetwork = net-%d\n",i);
    fprintf(fd,"LowModules = l2-%d\n\n",i);

    fprintf(fd,"[ Module il1-%d ]\n",i);
    fprintf(fd,"Type = Cache\n");
    fprintf(fd,"Geometry = l1\n");
    fprintf(fd,"LowNetwork = net-%d\n",i);
    fprintf(fd,"LowModules = l2-%d\n\n",i);
  }

  fprintf(fd,"#\n# Level 2 Caches\n#\n");

  for(i=0;i<nodos;i++){
    fprintf(fd,"[ Module l2-%d ]\n",i);
    fprintf(fd,"Type = Cache\n");
    fprintf(fd,"Geometry = l2\n");
    fprintf(fd,"HighNetwork = net-%d\n",i);
    fprintf(fd,"LowNetwork = mesh\n");
    fprintf(fd,"LowNetworkNode = n%d\n",i);
    fprintf(fd,"LowModules =");
    for(j=1;j<=mc;j++)
 	fprintf(fd," mod-mm-%d",j);
    fprintf(fd,"\n");
  }
  fprintf(fd,"#\n# Main memory\n#\n");
 	
 for(i=1;i<=mc;i++){
 	fprintf(fd,"[ Module mod-mm-%d ]\n",i);
 	fprintf(fd,"Type = MainMemory\n");
 	fprintf(fd,"HighNetwork = mesh\n");
 	fprintf(fd,"HighNetworkNode = n%d\n",nodos+i-1);
 	fprintf(fd,"Latency = 200\n");
 	fprintf(fd,"BlockSize = 64\n");
	fprintf(fd,"Ranks = %d\n",ranks);
	fprintf(fd,"Banks = %d\n",banks);
	fprintf(fd,"Channels = %d\n",channels);
	fprintf(fd,"CyclesSendRequest = 1\n");
	fprintf(fd,"CyclesRowBufferHit = 33\n");
	fprintf(fd,"CyclesRowBufferMiss = 100\n");
	fprintf(fd,"Bandwith = 16\n");
	fprintf(fd,"CyclesProcByCyclesBus = 3\n");
	fprintf(fd,"Threshold= 100000\n");
	fprintf(fd,"PolicyMCQueues = PrefetchNormalQueue\n");
	fprintf(fd,"Coalesce = Disabled\n");
	fprintf(fd,"PriorityMCQueues = Threshold-RowBufferHit-FCFS\n");
	fprintf(fd,"QueuePerBank = 1\n");
	fprintf(fd,"SizeQueue = 128\n");
	fprintf(fd,"MemoryControllerEnabled = 1\n");
	fprintf(fd,"AddressRange = ADDR DIV 4096 MOD %d EQ %d",mc,i-1);
	fprintf(fd,"\n");
	
}
  fprintf(fd,"\n#\n# Nodes\n#\n",nodos);
  for(i=0;i<nodos;i++){
    fprintf(fd,"[ Entry core-%d ]\n",i);
    fprintf(fd,"Type = CPU\n");
    fprintf(fd,"Core = %d\n",i);
    fprintf(fd,"Thread = 0\n");
    fprintf(fd,"InstModule = il1-%d\n",i);
    fprintf(fd,"DataModule = dl1-%d\n\n",i);
  }

  fprintf(fd,"#\n# Interconnects\n#\n");

  for(i=0;i<nodos;i++){
    fprintf(fd,"[ Network net-%d ]\n",i);
    fprintf(fd,"DefaultBandWidth = 128\n");
    fprintf(fd,"DefaultInputBufferSize = 512\n");
    fprintf(fd,"DefaultOutputBufferSize = 512\n\n");
  }
}
