#include <stdio.h>
#include <stdlib.h>

void main (){
  char filename[100];
  FILE *fd;
  unsigned int nodos=1, filas=1, columnas=1,i,j,mc;

  printf("Numero de nodos en el sistema:\n");
  scanf("%d",&nodos);

  printf("Numero de filas:\n");
  scanf("%d",&filas);

  printf("Numero mc:\n");
  scanf("%d",&mc);

  printf("Nombre del fichero de configuracion de red:\n");
  scanf("%s",filename);

  columnas=nodos/filas;
  if(nodos!=columnas*filas) {
    printf("ERROR EL NUMERO DE NODOS NO ES DIVISIBLE POR EL NUMERO DE FILAS\n");
    exit(-1);
  }

  printf("Fichero: %s\n",filename);
  printf("Nodos totales: %d\n",nodos);
  printf("Nodos por fila: %d\n",columnas);
  printf("Filas: %d\n",filas);

  fd=fopen(filename,"w");

  fprintf(fd,"[ Network.mesh ] \n  DefaultInputBufferSize = 128 \n  DefaultOutputBufferSize = 128 \n  DefaultBandwidth = 64 \n\n;;;;;;;;;; NODES ;;;;;;;;;; \n");

  for(i=0;i<nodos+mc;i++){
    fprintf(fd,"\n[ Network.mesh.Node.n%d ] \nType = EndNode \n",i);
  }

  fprintf(fd,"\n;;;;;;;; SWITCHES ;;;;;;;; \n");

  for(i=0;i<nodos;i++){
    fprintf(fd,"\n[ Network.mesh.Node.s%d ] \nType = Switch \n",i);
  }

  fprintf(fd,"\n;;;;;;;; LINKS ;;;;;;;; \n");
  for(i=0;i<filas;i++){
    for(j=0;j<columnas-1;j++){
      int actual=columnas*i+j;
      fprintf(fd,"\n[ Network.mesh.Link.s%ds%d ] \n  Source = s%d \n  Dest = s%d \n  Type = Bidirectional \n",actual,actual+1,actual,actual+1);
    }
  }

  for(i=0;i<filas-1;i++){
    for(j=0;j<columnas;j++){
      int actual=columnas*i+j;
      fprintf(fd,"\n[ Network.mesh.Link.s%ds%d ] \n  Source = s%d \n  Dest = s%d \n  Type = Bidirectional \n",actual,actual+columnas,actual,actual+columnas);
    }
  }

  for(i=0;i<nodos;i++){
    fprintf(fd,"\n[ Network.mesh.Link.n%ds%d ] \n  Source = n%d \n  Dest = s%d \n  Type = Bidirectional \n",i,i,i,i);
  }

  for(i=nodos;i<nodos+mc;i++){
    fprintf(fd,"\n[ Network.mesh.Link.n%ds%d ] \n  Source = n%d \n  Dest = s%d \n  Type = Bidirectional \n",i,-mc+i,i,-mc+i);
  }

   
  fprintf(fd,"\n[ Network.mesh.Routes ]\n");

  for(i=0;i<nodos;i++){
    for(j=0;j<nodos+mc;j++){
      if(i!=j) fprintf(fd,"n%d.to.n%d = s%d\n",i,j,i);
    }
  }
  for(i=nodos;i<nodos+mc;i++){
    for(j=0;j<nodos;j++){
      if(i!=j) fprintf(fd,"n%d.to.n%d = s%d\n",i,j,i-mc);
    }
  }

  for(i=0;i<nodos;i++){
    for(j=0;j<nodos;j++){
      unsigned int next;
      if(i!=j) {
	if(i%columnas!=j%columnas){
	  if(i%columnas>j%columnas){
	    next=i-1;
	  }else{
	    next=i+1;
	  }
	}else{
	  if(i>j){
	    next=i-columnas;
	  }else{
	    next=i+columnas;
	  }
	}
	fprintf(fd,"s%d.to.n%d = s%d\n",i,j,next);
      }
    }
  }

for(i=0;i<nodos;i++){
    	for(j=nodos;j<nodos+mc;j++){
		int j_aux=j-mc;
	      unsigned int next;
	      if(i!=j_aux) {
			if(i%columnas!=j_aux%columnas){
		  		if(i%columnas>j_aux%columnas){
		    			next=i-1;
		  		}else{
		    			next=i+1;
		  		}
			}else{
		  		if(i>j_aux){
		    			next=i-columnas;
		  		}else{
		    			next=i+columnas;
		  		}
			}
			fprintf(fd,"s%d.to.n%d = s%d\n",i,j,next);
      		}
    	}
  }

}
