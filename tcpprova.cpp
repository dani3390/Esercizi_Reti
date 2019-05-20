#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h> //for threading , link with lpthread
#include <fstream>
#include <iostream>
#include <my_global.h>
#include <mysql.h>
#include <time.h>

//per inizializzazione statica mutex
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

//funzione che si occupa dei vari thread
void *connection_handler(void *);
 
int main(int argc , char *argv[])
{
    int socket_desc, client_sock, c;
    struct sockaddr_in server, client;
     
    //creazione socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        printf("Errore creazione socket!");
    }
    puts("Socket creato!");
     
    //struttura sockaddr_in
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(12376);
     
    //bind
    if(bind(socket_desc, (struct sockaddr*)&server, sizeof(server)) < 0)
    {
        perror("Errore bind!");
        return 1;
    }
    puts("Bind effettuato con successo!");
     
    //attesa e accettazione connessioni
	listen(socket_desc, 3);
    puts("In attesa di nuove connessioni...");
	
    c = sizeof(struct sockaddr_in);
	
	pthread_t thread_id[50];
	int i = 0;
	
	while(1) //infinito
    {
		//accetto connessione da un client e salvo il risultato: un intero che funziona da descrittore del socket
		if((client_sock = accept(socket_desc, (struct sockaddr*)&client, (socklen_t*)&c)) >= 0){
			puts("Un nuovo client si e' connesso!");
		}
         
		//faccio partire un nuovo thread per ogni nuova connessione
        if((pthread_create(&thread_id[i], NULL, connection_handler, (void*)&client_sock)) < 0)
        {
            perror("Errore creazione thread!");
            return 1;
        }
		
		//per tenere conto delle varie connessioni, e fare in modo che vengano assegnate 
		if( i >= 10){
			i = 0;
			while(i < 10){
				puts("Handler assegnato!");
				pthread_join(thread_id[i++], NULL); //aspetta che un determinato thread termini
			}
			i = 0;
		}

        
    }
     
     
    return 0;
}


void *connection_handler(void *socket_desc)
{
	
	//allogo un oggetto MYSQL per poter iniziare una connessione
	MYSQL *con = mysql_init(NULL);
	// mi connetto al server
	mysql_real_connect(con, "localhost", "root", "all4one4all", "esercizio", 0, NULL, 0);
	
	//prendo le informazioni del socket
    int sock = *(int*)socket_desc;
    int read_size;
    char *message , client_message[2000], send_messagge[200];
	struct sockaddr_in addr;
	char sql_query[2000], sql_query2[2000];
	
	//serie di comandi per ottenere l'ip del client partendo dal descrittore del socket
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(sock, (struct sockaddr*)&addr, &addr_size);
	char *IPserver = new char[20];
    strcpy(IPserver, inet_ntoa(addr.sin_addr));
    
	
    //mando dei messaggi al client
    message = "Salve! Sono il tuo handler.\n";
    write(sock, message, strlen(message));
     
    message = "Scrivi qualcosa, e te lo rinvierò.\n";
    write(sock, message, strlen(message));
	
	//effettuo il lock della mutex
	pthread_mutex_lock(&lock);
	
    //Ricevere messaggio dal client
    while((read_size = recv(sock, client_message, 2000, 0)) > 0 )
    {
		/* data ed orario attuali */
		time_t timer;
		char tempo[26];
		struct tm* info;

		time(&timer);
		info = localtime(&timer);

		strftime(tempo, 26, "%Y-%m-%d %H:%M:%S", info);
		/* --------------------------------------------- */
		
        //marcatore di fine messaggio
		client_message[read_size] = '\0';	
		
		//stampo il messaggio ricevuto da un certo client
		printf("Messaggio ricevuto: %s. \nDa parte di: %s.\n", client_message, IPserver);
		
		//salvo il messaggio di risposta nella variabile send_message e la invio con write al client
		fgets(send_messagge, 200, stdin);
		write(sock, send_messagge, strlen(send_messagge));

		//tramite sprintf salvo le query, con le apposite variabili inserite, in delle stringhe 
		sprintf(sql_query, "Insert into utenti(IndirizzoIp) select ip from (select '%s' as ip) t where not exists (select 1 from utenti where binary t.ip = binary IndirizzoIp)", IPserver);
		sprintf(sql_query2,"Insert into messaggi(Messaggio, DataInvio, IndirizzoIp) select mes, dat, ip from (select '%s' as mes, '%s' as dat, '%s' as ip) t where exists (select 1 from utenti, messaggi where binary t.ip = binary utenti.IndirizzoIp)", client_message, tempo, IPserver);
		
		//libero il buffer del messaggio ricevuto dal client, e di quello inviato al client
		memset(client_message, 0, 2000);
		memset(send_messagge, 0, 200);
		mysql_query(con, sql_query);
		mysql_query(con, sql_query2);
		
		//controllo se le query vanno a buon fine
		/*if((mysql_query(con, sql_query)) == 0){
			puts("Query di inserimento ip eseguita con successo!");
		}
		else{
			puts("Problemi con query di inserimento ip!");
		}
		
		if((mysql_query(con, sql_query2)) == 0){
			puts("Query di inserimento dati nel database eseguita con successo!");
		}
		else{
			puts("Problemi nell'inserimento dei dati nel database!");
		}	*/	

		//ora posso unlockare la mutex
		pthread_mutex_unlock(&lock);
    }
	
	
	
	
    //controllo se il client si disconnette, oppure se c'è un errore del recv
    if(read_size == 0)
    {
        puts("Un client si e' disconnesso!");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("Errore recv!");
    }
	
	//chiudo il socket, la connessione al database ed esco dal thread
	close(sock);
	mysql_close(con);
	pthread_exit(NULL);
         
    return 0;
} 