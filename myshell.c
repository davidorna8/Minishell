/*
				MINISHELL 
	Autores: Eva Gómez Fernández y David Enrique Orna Alcobendas

*/



#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include "parser.h"

// creamos tipo de dato que guarde la línea, el pid y el estado de cada proceso de la línea y un contador de cuantos procesos hay en la línea
typedef struct{
	char nombre[1024];
	pid_t pid;
	int estado; // sera 1 si esta ejecutando y 0 si hecho
	int contadorComandos;
} tpair;

// para background y jobs
tpair comandosBG[100][20]; // estructura que almacena los procesos en segundo plano, lo consideramos estático 
int contadorBG; // contador de líneas en segundo plano

// para fg
int posicion; // posicion introducida (o última si no introduce nada) por el usuario al hacer fg
int bool_fg; // indica si nos encontramos en la ejecución de fg para el manejador

// para jobs
int bool_ejecuta; // para comprobar que la línea contiene algún proceso en ejecución

void manejador(int sign){
	int i;
	if(sign==SIGINT){
		printf("\n");
		if(bool_fg==1){
			for(i=0;i<comandosBG[posicion][0].contadorComandos;i++){
				kill(comandosBG[posicion][i].pid,9); // en el caso de estar en la ejecución del fg debemos terminar todos los comandos de la línea de esa posición
			}
		}
	}
	
}

void cd(tline * line){
	char * dir;
	int errorCambio;
	char buffer[1024];
	if(line->commands[0].argc>2){
		fprintf(stderr, "ERROR: demasiados argumentos\n");
	}
	else{
		if(line->commands[0].argc<=1){
			dir = getenv("HOME"); // en el caso de no recibir ningún argumento cambia al directorio home del usuario
			if(dir==NULL){ 
				fprintf(stderr,"ERROR: no existe la variable $HOME\n");	
			}
		}
		else if(line->commands[0].argc==2){
			dir = line->commands[0].argv[1];
		}
		// cambia de directorio y guarda en la variable si ha habido algun error
		errorCambio = chdir(dir); 
		if(errorCambio != 0){
			fprintf(stderr, "Error al cambiar de directorio\n");
		}
		printf("%s\n",getcwd(buffer, sizeof(buffer))); // se imprime por pantalla el directorio actual
	}
}

void jobs(){
	int j,k;
	for(j=1; j<=contadorBG; j++){
		bool_ejecuta=0;
		if(comandosBG[j][0].nombre!=NULL && strcmp(comandosBG[j][0].nombre,"")!=0){ // si la posición j contiene alguna línea
			for(k=0; k<comandosBG[contadorBG][0].contadorComandos; k++){
				if(comandosBG[j][k].estado==1){
					if(waitpid(comandosBG[j][k].pid,NULL,WNOHANG)==0){
						comandosBG[j][k].estado=1; // si aún no ha terminado se pone su estado a ejecutando
						bool_ejecuta=1;
					}
					else{
						comandosBG[j][k].estado=0; // si ha terminado se pone a hecho
					}
				}
			}
			if(bool_ejecuta==1){	// ejecutandose
				printf("[%d]\tRunning\t%s",j,comandosBG[j][0].nombre);
			}
			else{	// terminado
				printf("[%d]\tDone\t%s",j,comandosBG[j][0].nombre);
				strcpy(comandosBG[j][0].nombre,""); // borramos la línea que ha terminado de la estructura donde se almacenaba
				comandosBG[j][0].contadorComandos=-1;
				if(j==contadorBG){ // en el caso de que fuese el último de la lista movemos el contador hasta donde haya uno ejecutándose todavía
					while(strcmp(comandosBG[contadorBG][0].nombre,"")==0 && contadorBG>0){
						contadorBG=contadorBG-1;
					}
				}
			}
		}
	}
}

void fg(tline * line){
	int l;
	int o;
	int n;
	if(line->commands[0].argc>2){
		fprintf(stderr, "ERROR: demasiados argumentos\n");
	}
	else{
		for(n=1; n<=contadorBG; n++){ // igual que en el jobs, se comprueba si ha terminado alguna línea antes de ejcutar el fg
			bool_ejecuta=0;
			if(comandosBG[n][0].nombre!=NULL && strcmp(comandosBG[n][0].nombre,"")!=0){
				for(o=0; o<comandosBG[contadorBG][0].contadorComandos; o++){
					if(comandosBG[n][o].estado==1){
						if(waitpid(comandosBG[n][o].pid,NULL,WNOHANG)==0){
							comandosBG[n][o].estado=1;
							bool_ejecuta=1;
						}
						else{
							comandosBG[n][o].estado=0;
						}
					}
				}
				if(bool_ejecuta==0){	// terminado
					printf("[%d]\tDone\t%s",n,comandosBG[n][0].nombre);
					strcpy(comandosBG[n][0].nombre,"");
					comandosBG[n][0].contadorComandos=-1;
					if(n==contadorBG){
						while(strcmp(comandosBG[contadorBG][0].nombre,"")==0 && contadorBG>0){
							contadorBG=contadorBG-1;
						}
					}
				}
			}
		}
		bool_fg=1;
		signal(SIGINT,manejador); // asignamos a la señal SIGINT el manejador implementado anteriormente
		if(line->commands[0].argv[1]!=NULL){
			posicion= strtol(line->commands[0].argv[1],NULL,10); // recibe el argumento escrito por teclado
		}
		else{
			posicion= contadorBG; // si no se reciben argumentos es la última posición 
		}
		if(comandosBG[posicion][0].nombre!=NULL && strcmp(comandosBG[posicion][0].nombre,"")!=0){ // si el proceso no está terminado se pasa a primer plano
			printf("%s",comandosBG[posicion][0].nombre);
			for(l=0; l<comandosBG[posicion][0].contadorComandos;l++){
				waitpid(comandosBG[posicion][l].pid,NULL,0); // el padre (la minishell) espera a que terminen los procesos de esa línea
			}
			strcpy(comandosBG[posicion][0].nombre,""); // eliminamos el proceso de las líneas en segundo plano
			comandosBG[posicion][0].contadorComandos=-1; 
			if(posicion==contadorBG){
				while(strcmp(comandosBG[contadorBG][0].nombre,"")==0 && contadorBG>0){
					contadorBG=contadorBG-1;
				}
			}
				
		}else{
			fprintf(stderr, "ERROR: El trabajo está terminado\n");
		}
	}
}

int main(void) {
	char buf[1024];
	tline * line;
	int i, m, n, o;
	
	// booleano para segundo plano
	int bool_background;
	
	// booleanos para redirecciones
	int bool_salida; 	
	int bool_entrada; 
	int bool_error;
	
	// descriptores de los ficheros
	int salida;
	int entrada;
	int error;
	
	// pipes
	int p1[2];
	int p2[2];
	
	// para umask
	mode_t modo; 
	mode_t mascara;
	
	// para wait
	int status;
	pid_t *pid;
	
	// contador para saber quien es el ultimo indice usado 
	contadorBG = 0;
	
	signal(SIGINT,manejador); // asignamos la señal SIGINT al manejador para que ignore la señal en caso de estar en el proceso padre
	
	modo = 0002;
	mascara = umask(0002); // máscara por defecto, no da escritura a otros
	
	printf("msh> ");	
	while (fgets(buf, 1024, stdin)) {
		// inicializamos los booleanos
		bool_fg=0;
		bool_salida=0;
		bool_entrada=0;
		bool_error=0;
		bool_background=0;
		
		line = tokenize(buf);
		if (line==NULL) {
			continue;
		}
		if (line->redirect_input != NULL) {
			bool_entrada=1;
		}
		if (line->redirect_output != NULL) {
			bool_salida=1;
		}
		if (line->redirect_error != NULL) {
			bool_error=1;
		}
		if (line->background) {
			bool_background=1;
		} 
		
		// creamos las pipes
		pipe(p1);
		pipe(p2);
		
		// array de pid para hacer wait
		pid=(pid_t *) malloc(line->ncommands * sizeof(pid_t)); // el tamaño del array es el numero de comandos de la línea por lo que ocupa cada pid
		
		for (i=0; i<line->ncommands; i++) {
			
			// comando exit
		
			if(strcmp(line->commands[0].argv[0],"exit")==0){
				exit(0);
			}
			// comando cd
	
			else if((strcmp(line->commands[0].argv[0],"cd")==0) && (line->ncommands==1)){ // el cd en pipes da error
				cd(line);
			}
			
			// comando umask
	
			else if(strcmp(line->commands[0].argv[0],"umask")==0){ 
				if(line->commands[0].argc>2){
					fprintf(stderr, "ERROR: demasiados argumentos\n");
				}
				else{
					if(line->commands[0].argc<=1){
						fprintf(stdout, "La máscara actual es: %d\n",modo); // si no se reciben argumentos muestra la máscara actual (en decimal)
					}
					else if(line->commands[0].argc==2){
						modo = (int) (strtol(line->commands[0].argv[1], NULL, 8)); // se pasa a octal el numero introducido por el usuario
						umask(modo); // se cambia la máscara
					}
				}
			}
			
			// comando jobs
			
			else if(strcmp(line->commands[0].argv[0],"jobs")==0){
				jobs();
			}
			
			// comando fg
			
			else if(strcmp(line->commands[0].argv[0],"fg")==0){
				fg(line);				
			}
			
			// si el mandato no existe
			
			else if(line->commands[i].filename==NULL){
				fprintf(stderr, "ERROR: El mandato '%s' no existe\n",line->commands[i].argv[0]);
			}
			
			// ejecutar mandatos en foreground
			
			else{
					
				pid[i]=fork(); //creo proceso hijo
				
				if(pid[i]==0){ // estamos en el hijo
				
					if(bool_background==0){ // si no estamos en background el SIGINT podrá detener la ejecución de los mandatos
						signal(SIGINT, SIG_DFL);
					}else{ // en background se ignora la señal
						signal(SIGINT,SIG_IGN);
					}
					
					if(line->ncommands>1){ // si hay tuberías
						if(i==0){ // de los comandos de la linea si estamos en el primero
							close(p1[0]); // cerramos lectura de p1
							close(p2[0]);
							close(p2[1]); // cerramos p2 porque no la va a utilizar
							dup2(p1[1],1); // redirigimos la salida estándar a la tubería p1
							close(p1[1]); // cerramos tras haber redirigido

							if (bool_entrada==1){ // en el caso de estar en el primero hay que ver si se quiere redirigir la entrada
								entrada=open(line->redirect_input,O_RDWR); // abre el descriptor de fichero de redirección
								if(entrada<0){
									fprintf(stderr,"Error en la redirección de entrada\n");
									exit(1);// finaliza el proceso hijo
								}else{
									dup2(entrada,0); // redirecciona la entrada estándar al fichero
								}
							}
							
						}
						
						else if(i==(line->ncommands - 1)){ // si estamos en el último proceso de la línea
							if(i%2==0){ // caso par: lee de p2
								close(p2[1]); // cerramos escritura de p2
								close(p1[0]);
								close(p1[1]); // cerramos p1 porque no la va a utilizar
								dup2(p2[0],0); // redirigimos la entrada a p2
								close(p2[0]);
							}else { // caso impar: lee de p1
								close(p2[0]); // cerramos escritura de p1
								close(p2[1]);
								close(p1[1]); // cerramos p2 porque no la va a utilizar
								dup2(p1[0],0); // redirigimos la entrada a p1
								close(p1[0]);
							}
							
							if(bool_salida==1){ // en el caso de estar en el último se debe comprobar si se quiere redirigir la salida
							// se abre el descriptor de fichero de acuerdo al umask actual, se hace el XOR con 0666 porque los ficheros no tienen permisos de ejecución
								salida=open(line->redirect_output,O_CREAT|O_TRUNC|O_RDWR,0666 ^ mascara);
								// creamos el descriptor de fichero de la redireccion
								if(salida<0){
									fprintf(stderr,"Error en la redirección de salida\n");
									exit(1);// finaliza el proceso hijo
								} else{
									dup2(salida,1); // se redirecciona la salida estándar al fichero
								}
							}
							
							if(bool_error==1){
							// se abre el descriptor de fichero de acuerdo al umask actual, se hace el XOR con 0666 porque los ficheros no tienen permisos de ejecución
							// si se ha hecho umask, este prevalece a los permisos pasados como parámetro
								error=open(line->redirect_error,O_CREAT|O_TRUNC|O_RDWR,0666 ^ mascara);
								if(error<0){
									fprintf(stderr,"Error en la redirección de error\n");
									exit(1);// finaliza el proceso hijo
								} else{
									dup2(error,2); // se redirecciona la salida de error estándar al fichero
								}
							}
							
						}else{ // si no es el último ni el primero de la línea
							if(i%2==0){ // caso par: leer de p2 y escribir en p1
								close(p2[1]); // cerramos el extremo de escritura de p2
								close(p1[0]); // cerramos el extremo de lectura  de p1
								dup2(p2[0],0); // redireccionamos la entrada a p2
								close(p2[0]);
								dup2(p1[1],1); // redireccionamos la salida a p1
								close(p1[1]);
							}else { // caso impar: leer de p1 y escribir en p2
								close(p2[0]);
								close(p1[1]);
								dup2(p1[0],0);
								close(p1[0]);
								dup2(p2[1],1);
								close(p2[1]);
							}
						}
					}
					else{ // en caso de que sólo haya un comando se deben redirigir la salida, entrada y error si fuese necesario (igual que en el otro caso)
						if(bool_salida==1){
							salida=open(line->redirect_output,O_CREAT|O_TRUNC|O_RDWR, 0666 ^ mascara);
							// creamos el descriptor de fichero de la redireccion
							if(salida<0){
								fprintf(stderr,"Error en la redirección de salida\n");
								exit(1);// finaliza el proceso hijo
							} else{
								dup2(salida,1); 
							}
						}
						if(bool_error==1){
							error=open(line->redirect_error,O_CREAT|O_TRUNC|O_RDWR,0666 ^ mascara);
							if(error<0){
								fprintf(stderr,"Error en la redirección de error\n");
								exit(1);// finaliza el proceso hijo
							} else{
								dup2(error,2);
							}
						}
						if (bool_entrada==1){
							entrada=open(line->redirect_input,O_RDWR);
							
							if(entrada<0){
								fprintf(stderr,"Error en la redirección de entrada\n");
								exit(1);// finaliza el proceso hijo
							}else{
								dup2(entrada,0);
							}
						}
					}
					
					// el hijo ejecuta el comando
					execvp(line->commands[i].filename,line->commands[i].argv); // se pasan el nombre del fichero del comando y sus argumentos
					fprintf(stderr,"ERROR: no se ha ejecutado correctamente\n"); // si se ha producido un error se muestra por la salida de error y se finaliza el proceso hijo
					exit(1);
					
				}else{ // estamos en el padre
					if(i%2==0){ // caso par
						close(p2[0]);
						close(p2[1]);
						pipe(p2);
						// se cierra y se vuelve a crear la pipe que ya se usó y queremos reutilizar (p2)
					}else { //caso impar
						close(p1[0]);
						close(p1[1]);
						pipe(p1);
						// se cierra y se vuelve a crear la pipe que ya se usó y queremos reutilizar (p1)
					}
				
				}
			}
		}
		
		// una vez terminada la ejecución se cierran las pipes en el caso de que se hubieran quedado abiertas
		close(p1[0]);
		close(p2[0]);
		close(p1[1]);
		close(p2[1]);
		
		if(bool_background==1){ // si estamos en background
			// añadimos la línea a la estructura donde se almacenan los procesos en background
			contadorBG = contadorBG + 1;
			comandosBG[contadorBG][0].contadorComandos=line->ncommands; 
			strcpy(comandosBG[contadorBG][0].nombre,buf);				
			for (m=0; m<line->ncommands; m++){
				// se comprueba si el proceso añadido sigue en ejcución (estado=1) o ya ha terminado (estado=0)
				if(waitpid(pid[m],&status,WNOHANG)==0){
					comandosBG[contadorBG][m].estado=1;
				}
				else{
					comandosBG[contadorBG][m].estado=0;
				}
				comandosBG[contadorBG][m].pid=pid[m];
			}
			// al igual que en la bash se muestra la posición que ocupa y el pid del último mandato
			printf("[%d] %d \n", contadorBG, comandosBG[contadorBG][comandosBG[contadorBG][0].contadorComandos-1].pid);
		} 
		else{ // si no estamos en background la minishell debe esperar a que terminen de ejecutarse todos los procesos de la línea
			for (m=0; m<line->ncommands; m++){
				waitpid(pid[m],&status,0); 
			}
		}
		
		// se comprueba que mientras se ejcutaba la línea ha terminado alguno de los comandos que estaban en segundo plano
		for(n=1; n<=contadorBG; n++){
			bool_ejecuta=0;
			if(comandosBG[n][0].nombre!=NULL && strcmp(comandosBG[n][0].nombre,"")!=0){
				for(o=0; o<comandosBG[contadorBG][0].contadorComandos; o++){
					if(comandosBG[n][o].estado==1){
						if(waitpid(comandosBG[n][o].pid,NULL,WNOHANG)==0){
							comandosBG[n][o].estado=1;
							bool_ejecuta=1;
						}
						else{
							comandosBG[n][o].estado=0;
						}
					}
				}
				if(bool_ejecuta==0){	// terminado
					printf("[%d]\tDone\t%s",n,comandosBG[n][0].nombre);
					strcpy(comandosBG[n][0].nombre,"");
					comandosBG[n][0].contadorComandos= comandosBG[n][0].contadorComandos - 1;
					if(n==contadorBG){
						while(strcmp(comandosBG[contadorBG][0].nombre,"")==0 && contadorBG>0){
							contadorBG=contadorBG-1;
						}
					}
				}
			}
		}
		
		free(pid); // se libera la memoria ocupada por el array de pids
		printf("msh> "); // volvemos a mostrar el prompt
	}
	return 0;
}
