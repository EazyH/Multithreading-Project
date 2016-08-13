/*###########################################################################################################################

#WOJSKOWA AKADEMIA TECHNICZNA
#PROJEKT: ZESTAW PROGRAMOW TYPU PRODUCENT - KONSUMENT REALIZUJACYCH SYNCHRONICZNA KOMUNIKACJE MIEDZYPROCESOWA
#PRZEDMIOT: SYSTEMY OPERACYJNE
#RODZAJ ZAJEC: LABORATORIUM  
#GRUPA: K4X2S1
#AUTOR: HUBERT GO£ASZEWSKI
#ROK: 2015  
 
@MECHANIZM K1: SHARED MEMORY
@MECHANIZM K2: PIPE
@SYGNAL ZAKONCZENIA: SIGQUIT (3)
@SYGNAL ZATRZYMANIA: SIGILL (4)
@SYGNAL WZNOWIENIA: SIGTRAP (5)

#############################################################################################################################*/

#include <semaphore.h> // sem_open(), sem_wait(), sem_post(), sem_t TYPE
#include <stdlib.h> // exit()
#include <stdio.h> // printf(), fgets()
#include <unistd.h> // fork(), getpid(), sleep() etc.
#include <fcntl.h> // sem_open(O_CREAT)
#include <string.h> // memcpy()
#include <stdbool.h> // false, true
#include <sys/shm.h> // shmget(), shmat(), shmdt(), shmctl(), etc.
#include <sys/ipc.h> // key_t TYPE
#include <sys/types.h> // key_t TYPE, pid_t TYPE
#include <signal.h> // sigaction()
#include <time.h> // usleep()
#include <errno.h> // perror()

#define CHARLIMIT 1 // Wczytywanie linii po jednym znaku
#define FILENAMELIMIT 60 // Nazwa wlasnego pliku nie bedzie przekraczac 60 znakow
#define RDX 0 // Deskryptor odczytu dla lacza nienazwanego
#define WRX 1 // Deskryptor zapisu dla lacza nienazwanego

char semNameProduction[] = "/1SEMAPHOREproductionPOSIXn"; // Nazwanie semafora producenta
char semNameConsumption[] = "/2SEMAPHOREconsumptionPOSIXn"; // Nazwanie semafora konsumenta pierwszego
char semNameConsumption2[] = "/3SEMPAHOREconsumption2POSIXn"; // Nazwanie semafora konsumenta drugiego
char semNameEnd[] = "/4SEMAPHOREendPOSIXn"; // Nazwanie semafora konca

pid_t PID1; // PID procesu P1
pid_t PID2; // PID procesu P2
pid_t PID3; // PID procesu P3

char *flagsPointer; // Wskaznik na pamiec dzielona przechowujaca flagi sygnalowe
char *memoryPointer; // Wskaznik na pamiec dzielona przechowujaca pojedyncze znaki z ciagu
int flagsSegID; // ID segmentu pamieci dzielonej flagowej
int charSegID; // ID segmentu pamieci dzielonej znakowej

void quitP1(int); // Prototyp funkcji quitP1()
void quitP2(int); // Prototyp funkcji quitP2()
void quitP3(int); // Prototyp funkcji quitP3()
void stopP1(int); // Prototyp funkcji stopP1()
void stopP2(int); // Prototyp funkcji stopP2()
void stopP3(int); // Prototyp funkcji stopP3()
void resumeP1(int); // Prototyp funkcji resumeP1()
void resumeP2(int); // Prototyp funkcji resumeP2()
void resumeP3(int); // Prototyp funkcji resumeP3()
void systemCleanPM(); // Prototyp funkcji systemCleanPM()
int clearBuffer(char); // Prototyp funkcji clearBuffer()

int main(int argc, const char *argv[]){
    
	setbuf(stdout, NULL); // Wylaczenie buforowania STDOUT

	key_t KEY_flags = 0x2AB; // Identyfikator segmentu pamieci
    
	if((flagsSegID = shmget(KEY_flags, sizeof(char)*4, 0666 | IPC_CREAT)) < 0) perror("SHMGET(FLAGS) - PM"); // Pobranie ID pamieci wspoldzielonej
	if((flagsPointer = shmat(flagsSegID, NULL, 0))  == (char *) -1) perror("SHMAT(FLAGS) - PM"); // Przylaczenie sie do segmentu pamieci dzielonej flagowej
    
	*flagsPointer = 'W'; // Ustawienie flagi procesu P1 na 'W' - pracujacy
	*(flagsPointer+1) = 'W'; // Ustawienie flagi procesu P2 na 'W' - pracujacy
	*(flagsPointer+2) = 'W'; // Ustawienie flagi procesu P3 na 'W' - pracujacy
    
	char data[CHARLIMIT] = {}; // Tablica przechowujaca pojedyncze znaki z ciagu
    
	key_t KEY_char = 0x29A; // Identyfikator segmentu pamieci
    
	if((charSegID = shmget(KEY_char, sizeof(data), 0666 | IPC_CREAT)) < 0) perror("SHMGET(CHAR) - PM"); // Pobranie ID segmentu pamieci wspoldzielonej
	if((memoryPointer = shmat(charSegID, NULL, 0)) == (char *) -1) perror("SHMAT(CHAR) - PM"); // Przylaczenie sie do segmentu pamieci dzielonej znakowej
   	 
	int signalIgnore = 1; // Zmienna przechowujaca numer sygnalu do zignorowania
    
	for(signalIgnore; signalIgnore <= 64; signalIgnore++){
       	 
    	signal(signalIgnore, SIG_IGN); // Zignorowanie wszystkich sygnalow przez proces PM
       	 
	}
    
	sem_t *mutexProduction; // Semefor produkcyjny obslugiwany przez P1/P2/P3
	sem_t *mutexConsumption; // Semafor konsumpcyjny pierwszy obslugiwany przez P1/P2
   	sem_t *mutexConsumption2; // Semefor konsumpcyjny drugi obslugiwany przez P2/P3
   	sem_t *mutexEnd; // Semafor konca obslugiwany przez PM/P3
      	 
	if((mutexProduction = sem_open(semNameProduction, O_CREAT, 0666, 1)) == SEM_FAILED) perror("SEM_OPEN(P) - PM"); // Stworzenie semafora producenta i podniesienie go
	if((mutexConsumption = sem_open(semNameConsumption, O_CREAT, 0666, 0)) == SEM_FAILED) perror("SEM_OPEN(C1) - PM"); // Stworzenie semafora konsumenta pierwszego i opuszczenie go
	if((mutexConsumption2 = sem_open(semNameConsumption2, O_CREAT, 0666, 0)) == SEM_FAILED) perror("SEM_OPEN(C2) - PM"); // Stworzenie semafora konsumenta drugiego i opuszczenie go
	if((mutexEnd = sem_open(semNameEnd, O_CREAT, 0666, 0)) == SEM_FAILED) perror("SEM_OPEN(E) - PM"); // Stworzenie semafora konca i podniesienie go
    
	usleep(100000);
    
	if(!fork()){ // STWORZENIE PROCESU PIERWSZEGO
  	 
    	if((sem_close(mutexConsumption2)) == -1) perror("SEM_CLOSE(C2) - P1"); // Zamkniecie semafora konsumenta drugiego dla procesu P1
        if((sem_close(mutexEnd)) == -1) perror("SEM_CLOSE(E) - P1"); // Zamkniecie semafora konca dla procesu P1
      	 
       	PID1 = getpid(); // Pobranie PIDu procesu P1
      	 
       	signal(SIGILL, stopP1); // Obsluga sygnalu zatrzymania
    	signal(SIGQUIT, quitP1); // Obsluga sygnalu zakoczenia
      	 
    	char option; // Opcja wprowadzania danych
    	bool end = false; // Zmienna okreslajaca czy pobrano caly ciag (np. po obsludze przerwania)
    	bool first = true; // Zmienna pomagajaca wyswietlic prawidlowo wyniki dla option == '1'
    	int decision = -1; // Zmienna okreslajaca czy wybrano prawidlowa opcje
       	 
    	do{ // Menu
       	 
        	printf("CHOOSE YOUR INPUT METHOD: \n");
        	printf("1) KEYBOARD\n");
        	printf("2) YOUR FILE\n");
        	printf("3) /DEV/URANDOM FILE\n");
        	printf("OPTION: ");
       	 
        	option = getchar(); // Podaj opcje wprowadzania danych i ignoruj znak '\n' poniewaz zostalby w buforze
   
        	decision = clearBuffer(option); // Wyczyszczenie bufora i sprawdzenie poprawnosci wybranej opcji

        	fflush(stdout); // Wyczyszczenie bufora dla pewnosci (prawdopodobnie nie dziala na Linuxie)
           	fflush(stdin);	// Wyczyszczenie bufora dla pewnosci (prawdopodobnie nie dziala na Linuxie)  
          	printf("\033[H\033[J"); // Wyczyszczenie ekranu
       	 
    	}
    	while(decision != 1); // Pytaj dopoki decyzcja nie bedzie pozytywna
   	 
    	if(option == '1'){ // Jezeli wybrano wprowadzanie z klawiatury
        	 
        	*(flagsPointer+3) = 'K'; // Podniesienie flagi trybu 'K' - keyboard
                  	 
        	while(*flagsPointer != 'Q' || end != true){
                                  	 
            	sem_wait(mutexProduction); // Opuszczenie semafora producenta
	 
            	if(first == false && end == true) printf("\n"); // Poprawne wyswietlenie wynikow

            	if(end == true) end = false; // Stwierdzenie ze wczytywany jest nowy ciag

            	read(0, &data, CHARLIMIT); // Wczytywanie danych znak po znaku do tablicy 1 - elementowej z STDIN

            	*memoryPointer = data[0]; // Zapisanie znaku do pamieci wspoldzielonej
           	 
            	if(data[0] == '\n') end = true; // Jezeli wystapil znak nowej linii to zmienna end = true
           	 
            	if(first == true) first = false; // Stwierdzenie, ze od tej pory nie jest to pierwsze przejscie petli
               	 
            	sem_post(mutexConsumption); // Podniesienie semafora konsumenta pierwszego
 
        	}
       	 
    	}
   	 
    	else if(option == '2'){ // Jezeli wybrano wprowadzanie z wlasnego pliku
          	 
           	*(flagsPointer+3) = 'O'; // Podniesienie flagi trybu 'O' - own file
          	 
        	char fileName[FILENAMELIMIT]; // Tablica przechowujaca nazwe pliku
        	int fileData; // Deskryptor pliku

        	do{    
      	 
                printf("INPUT YOUR FILE NAME: ");
               	scanf("%s", fileName); // Podanie nazwy pliku z danymi
               	fileData = open(fileName, O_RDONLY); // Otwarcie pliku tylko do odczytu
                printf("\033[H\033[J"); // Wyczyszczenie ekranu
               	 
            	if(fileData == -1) perror("OPEN() - OWN FILE"); // Jezeli zla nazwa zwroc blad

            }    
        	while(fileData == -1); // Wczytywanie nazwy dopoki bedzie prawidlowa
       	 
           	while(*(flagsPointer+1) != 'Q' || end != true){
 
            	sem_wait(mutexProduction); // Opuszczenie semafora producenta
               	
               	if(end == true) end = false; // Stwierdzenie ze wczytywany jest nowy ciag
               	
            	if(read(fileData, &data, CHARLIMIT) == 0) quitP1(3); // Wczytywanie danych do tablicy 1 - elementowej, jezeli EOF zakoncz program
                  	 
            	*memoryPointer = data[0]; // Zapisanie znaku do pamieci dzielonej
               	 
            	if(data[0] == '\n') end = true; // Jezeli wystapil znak nowej linii to zmienna end = true
               	 
               	sem_post(mutexConsumption); // Podniesienie semafora konsumenta pierwszego
      	 
        	}
       	 
    		if((close(fileData)) == -1) perror("CLOSE(OWN FILE) - P1"); // Zamkniecie pliku
   	 
    	}
   	 
    	else if(option == '3'){ // Jezeli wybrano wprowadzanie z pliku /dev/urandom
   	 
    		*(flagsPointer+3) = 'U'; // Podniesienie flagi trybu 'U' - urandom file
   	 
    		char fileName[FILENAMELIMIT] = "/dev/urandom"; // Tablica przechowujaca nazwe pliku
    		int fileData; // Deskryptor pliku

    		do{    
      	 	 
               	fileData = open(fileName, O_RDONLY); // Otwarcie pliku tylko do odczytu
                printf("\033[H\033[J"); // Wyczyszczenie ekranu
               	 
            	if(fileData == -1){
                   	 
                    perror("OPEN() - URANDOM FILE"); // Jezeli zla sciezka zwroc blad
                    printf("INPUT URANDOM FILE PATH: ");
                    scanf("%s", fileName); // Podanie nazwy pliku z danymi
              	 
            	}
				    
            }
        	while(fileData == -1); // Wczytywanie nazwy dopoki bedzie prawidlowa

           	while(*flagsPointer != 'Q' || end != true){  

            	sem_wait(mutexProduction); // Opuszczenie semafora producenta, $
	 
	 			if(end == true) end = false; // Stwierdzenie ze wczytywany jest nowy ciag
	 			
            	read(fileData, &data, CHARLIMIT); // Wczytywanie danych do tablicy 1 - elementowej

            	*memoryPointer = data[0]; // Zapisanie znaku do pamieci dzielonej

            	if(data[0] == '\n') end = true; // Jezeli wystapil znak nowej linii to zmienna end = true

            	sem_post(mutexConsumption); // Podniesienie semafora konsumenta pierwszego

        	}

        	if((close(fileData)) == -1) perror("CLOSE(URANDOM FILE) - P1"); // Zamkniecie pliku
          	 
    	}
            	 
    	if((sem_close(mutexProduction)) == -1) perror("SEM_CLOSE(P) - P1"); // Zamkniecie semafora producenta dla procesu P1
    	if((sem_close(mutexConsumption)) == -1) perror("SEM_CLOSE(C1) - P1"); // Zamkniecie semafora konsumenta pierwszego dla procesu P1
    	if((shmdt(memoryPointer)) == -1) perror("SHMDT(CHAR) - P1"); // Odlaczenie pamieci dzielonej dla procesu P1
      	if((shmdt(flagsPointer)) == -1) perror("SHMDT(FLAGS) - P1"); // Odlaczenie pamieci dzielonej dla procesu P1
     	 
    	exit(0); // Zakonczenie procesu P1
       	 
	}    

	int dsxALPHA[2]; // Uchwyty na deskryptory odczytu/zapisu
	if((pipe(dsxALPHA)) == -1) perror("PIPE() - PM"); // Utworzenie lacza nienazwanego
   	 
	if(!fork()){ // STWORZENIE PROCESU DRUGIEGO
   	 
        if((close(dsxALPHA[RDX])) == -1) perror("CLOSE(RDX) - P2"); // Zamkniecie odczytu z potoku dla procesu P2 (jako producent)
        if((sem_close(mutexEnd)) == -1) perror("SEM_CLOSE(E) - P2"); // Zamkniecie semafora konca dla procesu P2
               	 
    	PID2 = getpid(); // Pobranie PIDu procesu P2
       	 
    	signal(SIGILL, stopP2); // Obsluga sygnalu zatrzymania
    	signal(SIGQUIT, quitP2); // Obsluga sygnalu zakonczenia
   	 
    	char data[CHARLIMIT] = {}; // Tablica przechowujaca dane znakowe, zainicjalizowana na 0
    	int counter = 0; // Licznik znakow
    	bool end = false; // Zmienna
       	 
    	while(*(flagsPointer+1) != 'Q' || end != true){ // Odczytywanie danych z pamieci dzielonej od procesu P1 i wysylanie laczem do procesu P3

        	sem_wait(mutexConsumption); // Opuszczenie semafora konsumenta pierwszego
          	
          	if(end == true) end = false; // Stwierdzenie ze wczytywany jest nowy ciag
          	
           	counter++; // Zliczanie znakow w pojedynczym ciagu
 
        	if(*memoryPointer != '\n') sem_post(mutexProduction); // Jesli nie pobrano calego ciagu, wczytuj dalej
        	else{ // Jesli pobrano caly ciag przekaz liczbe znakow do procesu P3
           	 
            	end = true; // Stwierdzenie ze wczytano caly ciag
            	write(dsxALPHA[WRX], &counter, sizeof(counter)); // Przekazanie liczby znakow do procesu P3
            	counter = 0; // Wyzerowanie licznika znakow
            	sem_post(mutexConsumption2); // Podniesienie semafora konsumenta drugiego
                 	 
        	}
       	 
    	}

    	usleep(200000); // Danie czasu na zakonczenie procesu P1
   	 
    	if((close(dsxALPHA[WRX])) == -1) perror("CLOSE(WRX) - P2"); // Zamkniecie zapisu do potoku dla procesu P2
    	if((sem_close(mutexProduction)) == -1) perror("SEM_CLOSE(P) - P2"); // Zamkniecie semafora konsumenta pierwszego dla procesu P2
    	if((sem_close(mutexConsumption)) == -1) perror("SEM_CLOSE(C1) - P2"); // Zamkniecie semafora konsumenta pierwszego dla procesu P2
    	if((sem_close(mutexConsumption2)) == -1) perror("SEM_CLOSE(C2) - P2"); // Zamkniecie semafora konsumenta drugiego dla procesu P2
    	if((shmdt(memoryPointer)) == -1) perror("SHMDT(CHAR) - P2"); // Odlaczenie pamieci dzielonej znakowej dla procesu P2
    	if((shmdt(flagsPointer)) == -1) perror("SHMDT(FLAGS) - P2"); // Odlaczenie pamieci dzielonej flagowej dla procesu P2
   	 
    	exit(0); // Zakonczenie procesu P2
     	 
	}
     
	if(!fork()){ // STWORZENIE PROCESU TRZECIEGO
      	 
       	if((sem_close(mutexConsumption)) == -1) perror("SEM_CLOSE(C1) - P3"); // Zamkniecie semafora konsumenta pierwszego dla procesu P3
       	if((shmdt(memoryPointer)) == -1) perror("SHMDT(CHAR) - P3"); // Odlaczenie pamieci dzielonej znakowej dla procesu P3
       	if((close(dsxALPHA[WRX])) == -1) perror("CLOSE(WRX) - P3"); // Zamkniecie zapisu do potoku dla procesu P3
      	 
       	PID3 = getpid(); // Pobranie PIDu procesu P3
      	 
    	signal(SIGILL, stopP3); // Obsluga sygnalu zatrzymania
    	signal(SIGQUIT, quitP3); // Obsluga sygnalu zakonczenia
       	 
    	int value; // Ilosc znakow we wczytanej linii
    	int newline = 0; // Zmienna okreslajaca ile jednostek wynikow ma byc wypisywanych w jednej linii
   	 
    	while(*(flagsPointer+2) != 'Q'){
       	 
        	sem_wait(mutexConsumption2); // Opuszczenie semafora konsumenta drugiego
				       	 
           	read(dsxALPHA[RDX], &value, sizeof(value)); // Odczytanie liczby znakow od procesu P2      	 

        	printf("%d ", value); // Wypisanie liczby znakow na STDOUT
			       	 
            if(*(flagsPointer+3) != 'K'){
           	 
            	newline++; // Zwiekszenie licznika jednostek
       	 
             	if(newline == 10){ // Jezeli wystapilo 10 jednostek to wypisz znak nowej linii i zresetuj licznik jednostek
           	 
                	printf("\n");
                	newline = 1;
           	 
            	}
           	 
        	}
       	 
        	sem_post(mutexProduction); // Podniesienie semafora producenta

    	}
   	 
    	usleep(400000); // Danie czasu na zakoczenie procesow P1 i P2
   	 
    	if((close(dsxALPHA[RDX])) == -1) perror("CLOSE(RDX) - P3"); // Zamkniecie odczytu do potoku dla procesu P3
    	if((sem_close(mutexProduction)) == -1) perror("SEM_CLOSE(P) - P3"); // Zamkniecie semafora producenta dla procesu P3
    	if((sem_close(mutexConsumption2)) == -1) perror("SEM_CLOSE(C2) - P3"); // Zamkniecie semafora konsumenta drugiego dla procesu P3
    	if((shmdt(flagsPointer)) == -1) perror("SHMDT(FLAGS) - P3"); // Odlaczenie pamieci dzielonej flagowej dla procesu P3
   	 
    	sem_post(mutexEnd); // Podniesienie semafora konca (oznacza zakonczenie dzialania programu)
    	if((sem_close(mutexEnd)) == -1) perror("SEM_CLOSE(E) - P3"); // Zamkniecie semafora konca dla procesu P3
   	 
    	exit(0); // Zakonczenie procesu P3

	}

	if((close(dsxALPHA[RDX])) == -1) perror("CLOSE(RDX) - PM"); // Zamkniecie odczytu do potoku dla procesu PM
	if((close(dsxALPHA[WRX])) == -1) perror("CLOSE(WRX) - PM"); // Zamkniecie zapisu do potoku dla procesu PM
	if((sem_close(mutexProduction)) == -1) perror("SEM_CLOSE(P) - PM"); // Zamkniecie semafora produkcyjnego dla procesu PM
	if((sem_close(mutexConsumption)) == -1) perror("SEM_CLOSE(C1) - PM"); // Zamkniecie semafora konsumpcyjnego pierwszego dla procesu PM
	if((sem_close(mutexConsumption2)) == -1) perror("SEM_CLOSE(C2) - PM"); // Zamkniecie semafora konsumpcyjnego drugiego dla procesu PM
	if((shmdt(flagsPointer)) == -1) perror("SHMDT(FLAGS) - PM"); // Odlaczenie pamieci dzielonej flagowej dla procesu PM
	if((shmdt(memoryPointer)) == -1) perror("SHMDT(CHAR) - PM"); // Odlaczenie pamieci dzielonej znakowej dla procesu PM
    
	sem_wait(mutexEnd); // Oczekiwanie na podniesienie semafora konca przez proces P3 (zakonczenie dzialania calego programu)
    
	systemCleanPM(); // Funkcja zwalniajaca wszystkie uzyte zasoby
    
	if((sem_close(mutexEnd)) == -1) perror("SEM_CLOSE(E) - PM"); // Zamkniecie semafora konca dla PM
    
	printf("\nEND OF ALL PROCESSES!\n"); // Komunikat inforujacy o poprawnym zakonczeniu calego programu!

	return 0;

}

void quitP1(int signalX){ // Funkcja konczaca proces P1
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP
    
	if(*flagsPointer != 'Q') *flagsPointer = 'Q'; // Podniesienie flagi zakonczenia dla procesu P1

	usleep(200000); // Zapobiega powielaniu sygnalow
   
	if(*(flagsPointer+1) != 'Q') kill(PID2, SIGQUIT); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+2) != 'Q') kill(PID3, SIGQUIT); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany
           	 
}

void quitP2(int signalX){ // Funkcja konczaca proces P2
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP
    
	if(*(flagsPointer+1) != 'Q') *(flagsPointer+1) = 'Q'; // Podniesienie flagi zakonczenia dla procesu P2
    
	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*flagsPointer != 'Q') kill(PID1, SIGQUIT); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+2) != 'Q') kill(PID3, SIGQUIT); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany
   	 
}

void quitP3(int signalX){ // Funkcja konczaca proces P3
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP

	if(*(flagsPointer+2) != 'Q') *(flagsPointer+2) = 'Q'; // Podniesienie flagi zakonczenia dla procesu P3
    
	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*flagsPointer != 'Q') kill(PID1, SIGQUIT); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+1) != 'Q') kill(PID2, SIGQUIT); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany
   	 
}

void stopP1(int signalX){ // Funkcja zatrzymujaca proces P1

	signal(SIGTRAP, resumeP1); // Obsluga sygnalu SIGTRAP
	signal(SIGQUIT, quitP1); // Obsluga sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
    
	if (*flagsPointer == 'W') *flagsPointer = 'S'; // Podniesienie flagi zatrzymania dla procesu P1

	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer+1) == 'W') kill(PID2, SIGILL); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany  
	if(*(flagsPointer+2) == 'W') kill(PID3, SIGILL); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany

	pause(); // Zatrzymanie procesu P1

	signal(SIGILL, stopP1); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP1); // Przywrocenie starej obslugi sygnalu SIGQUIT
    
}

void stopP2(int signalX){ // Funkcja zatrzymujaca proces P2

	signal(SIGTRAP, resumeP2); // Obsluga sygnalu SIGTRAP
	signal(SIGQUIT, quitP2); // Obsluga sygnalu SIGQUIT
  	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
    
	if (*(flagsPointer+1) == 'W') *(flagsPointer+1) = 'S'; // Podniesienie flagi zatrzymania dla procesu P2
    
	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer) == 'W') kill(PID1, SIGILL); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany  
	if(*(flagsPointer+2) == 'W') kill(PID3, SIGILL); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany

	pause(); // Zatrzymanie procesu P2
    
	signal(SIGILL, stopP2); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP2); // Przywrocenie starej obslugi sygnalu SIGQUIT
    
}

void stopP3(int signalX){ // Funkcja zatrzymujaca proces P3

	signal(SIGTRAP, resumeP3); // Obsluga sygnalu SIGTRAP
	signal(SIGQUIT, quitP3); // Obsluga sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
    
 	if (*(flagsPointer+2) == 'W') *(flagsPointer+2) = 'S'; // Podniesienie flagi zatrzymania dla procesu P3

	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer) == 'W') kill(PID1, SIGILL); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+1) == 'W') kill(PID2, SIGILL); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany

	pause(); // Zatrzymanie procesu P2
    
	signal(SIGILL, stopP3); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP3); // Przywrocenie starej obslugi sygnalu SIGQUIT
    
}

void resumeP1(int signalX){ // Funkcja wznawiajaca proces P1
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP
    
	if (*flagsPointer == 'S') *flagsPointer = 'W'; // Podniesienie flagi wznowienia dla procesu P1

	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer+1) == 'S') kill(PID2, SIGTRAP); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+2) == 'S') kill(PID3, SIGTRAP); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany
   	 
	signal(SIGILL, stopP1); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP1); // Przywrocenie starej obslugi sygnalu SIGQUIT
   	 
}

void resumeP2(int signalX){ // Funkcja wznawiajaca proces P2
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP
    
	if (*(flagsPointer+1) == 'S') *(flagsPointer+1) = 'W'; // Podniesienie flagi wznowienia dla procesu P2

	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer) == 'S') kill(PID1, SIGTRAP); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+2) == 'S') kill(PID3, SIGTRAP); // Wyslanie sygnalu do procesu P3 jesli nie byl wczesniej wyslany
   	 
	signal(SIGILL, stopP2); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP2); // Przywrocenie starej obslugi sygnalu SIGQUIT
   	 
}

void resumeP3(int signalX){ // Funkcja wznawiajaca proces P3
    
	signal(SIGQUIT, SIG_IGN); // Zignorowanie sygnalu SIGQUIT
	signal(SIGILL, SIG_IGN); // Zignorowanie sygnalu SIGILL
	signal(SIGTRAP, SIG_IGN); // Zignorowanie sygnalu SIGTRAP
    
	if (*(flagsPointer+2) == 'S') *(flagsPointer+2) = 'W'; // Podniesienie flagi wznowienia dla procesu P3

	usleep(200000); // Zapobiega powielaniu sygnalow
    
	if(*(flagsPointer) == 'S') kill(PID1, SIGTRAP); // Wyslanie sygnalu do procesu P1 jesli nie byl wczesniej wyslany
	if(*(flagsPointer+1) == 'S') kill(PID2, SIGTRAP); // Wyslanie sygnalu do procesu P2 jesli nie byl wczesniej wyslany

 	signal(SIGILL, stopP3); // Przywrocenie starej obslugi sygnalu SIGILL
	signal(SIGQUIT, quitP3); // Przywrocenie starej obslugi sygnalu SIGQUIT
  	 
}

int clearBuffer(char option){ // Funkcja czyszczaca bufor po blednym wybraniu opcji z menu

	char symbol; // Kolejny znak do pobrania
    
	symbol = getchar(); // Pobierz kolejny znak

	if((option == '1' || option == '2' || option  == '3') && symbol == '\n') return 1; // Jezeli wybrano prawidlowo zwroc 1

	while(symbol != '\n') symbol = getchar(); // Wczytywanie znaku dopoki nie napotka nowej linii

	return -1; // Wybrano zle wiec zwroc -1
    
}

void systemCleanPM(){ // Funkcja zwalniajaca zasoby przez PM
    
	usleep(200000); // Danie czasu na poprawne zakonczenie procesow P1, P2 i P3
    
	if((sem_unlink(semNameProduction)) == -1) perror("SEM_UNLINK() - P"); // Odlaczenie semafora producenta
	if((sem_unlink(semNameConsumption)) == -1) perror("SEM_UNLINK() - C1"); // Odlaczenie semafora konsumenta pierwszego
	if((sem_unlink(semNameConsumption2)) == -1) perror("SEM_UNLINK() - C2"); // Odlaczenie semafora konsumenta drugiego
	if((sem_unlink(semNameEnd)) == -1) perror("SEM_UNLINK() - E"); // Odlaczenie semafora konca
	if((shmctl(charSegID, IPC_RMID, NULL)) == -1) perror("SHMCTL() - CHAR"); // Usuniecie pamieci dzielonej
	if((shmctl(flagsSegID, IPC_RMID, NULL)) == -1) perror("SHMCTL() - FLAGS"); // Usuniecie pamieci dzielonej
    
}


