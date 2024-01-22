#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>


// Define Semaphores
sem_t serv_sem, cli_sem;

// Define Mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// awk script for update process
const char *updateScript = "awk '{ print $0 }' file.txt > temp.txt && mv 
temp.txt file.txt";

// Global variable to indicate whether the program should exit
int exitFlag = 0;

// file name which we want to do changes
char currentFileName[256] = "";

void *readFile(void *arg) {
    // mutex for protecting critical section
    pthread_mutex_lock(&mutex);

    // Open file Read Only
    int fileDescriptor = open(currentFileName, O_RDONLY);

    // The size of text for read
    char buffer[1024];

    // Read from file
    ssize_t bytesRead = read(fileDescriptor, buffer, sizeof(buffer));

    // Print the read data to screen
    write(STDOUT_FILENO, buffer, bytesRead);
    printf("\n");
    
    //unlock mutex
    //mutex logic is same for other threads
    pthread_mutex_unlock(&mutex);
    
    // Signal to mainThread and server semaphore will wait for next signal
    sem_post(&serv_sem);
    sem_wait(&cli_sem);
}

void *writeFile(void *arg) {
    
    pthread_mutex_lock(&mutex);

    //get info form user
    char name[50];
    int price, quantity;

    //get product information
    printf("Product name: ");
    scanf("%s", name);

    printf("Price: ");
    scanf("%d", &price);

    printf("Quantity: ");
    scanf("%d", &quantity);

    // open file write only
    int fileDescriptor = open(currentFileName, O_WRONLY | O_CREAT | 
O_APPEND, S_IRUSR | S_IWUSR);

    //writing process
    const char format[] = "%-20s %-20d %-20d\n";
    char buffer[100];  
    int n = snprintf(buffer, sizeof(buffer), format, name, price, 
quantity);
    write(fileDescriptor, buffer, n);

    pthread_mutex_unlock(&mutex);

    //signal to mainThread
    sem_post(&serv_sem);
    sem_wait(&cli_sem);
}

void *openFile(void *arg) {
    pthread_mutex_lock(&mutex);

    char fileName[256];
    // Get file name from the user
    printf("Enter the file name: ");
    scanf("%255s", fileName);

    // Open the file in write mode, create if not exists, and append if it 
exists
    int fileDescriptor = open(fileName, O_WRONLY | O_CREAT | O_APPEND, 
S_IRUSR | S_IWUSR);

    // If the file is newly created, write header lines to the file
    if (lseek(fileDescriptor, 0, SEEK_END) == 0) {
        // File is empty, write Header lines
        const char header1[] = "Name                 Price                
Quantity            \n";
        const char header2[] = "======               ======               
=======\n";

        write(fileDescriptor, header1, strlen(header1));
        write(fileDescriptor, header2, strlen(header2));
    }

    // Update the currently opened file name
    strcpy(currentFileName, fileName);

    pthread_mutex_unlock(&mutex);
    sem_post(&serv_sem);
    sem_wait(&cli_sem);
}


void *updateFile(void *arg){
    
    pthread_mutex_lock(&mutex);

    // Ask user for the operation
    printf("Choose process:\n");
    printf("1. Add\n");
    printf("2. Delete\n");
    printf("3. Update\n");
    int operation;
    scanf("%d", &operation);

    if (operation == 1) {
        // append process
        char name[50];
        int price, quantity;

        // get info
        printf("Product Name: ");
        scanf("%s", name);

        printf("Price: ");
        scanf("%d", &price);

        printf("Quantity: ");
        scanf("%d", &quantity);

        // Use awk to append to the file
        char awkCommand[200];
        snprintf(awkCommand, sizeof(awkCommand), "awk 'END { printf 
\"%%-20s %%-20d %%-20d\\n\", \"%s\", %d, %d >> \"%s\" }' \"%s\"",
                 name, price, quantity, currentFileName, currentFileName);

        system(awkCommand);

    } else if (operation == 2) {
        // delete process
        printf("Listing the Products...\n");
        char awkList[256];
        sprintf(awkList,"awk 'NR > 2 { print $1 }' %s",currentFileName);
        system(awkList);  // list products

        char nameToDelete[50];
        printf("Enter the name of product you want to delete: ");
        scanf("%s", nameToDelete);

        // delete with awk
        char awkCommand[108];
        sprintf(awkCommand, "awk '$1 != \"%s\"' %s > temp.txt && mv 
temp.txt %s", nameToDelete,currentFileName,currentFileName);
        system(awkCommand);

    } else if (operation == 3) {
    // update process
    char awkList[256];
    sprintf(awkList, "awk 'NR > 2 { print $1 }' %s", currentFileName);
    system(awkList);  // list

    char nameToUpdate[50];
    printf("Enter the name of the product you want to update: ");
    scanf("%s", nameToUpdate);

    char newName[50], newPrice[20], newQuantity[20];
    printf("Enter the new name: ");
    scanf("%s", newName);

    printf("Enter the new price: ");
    scanf("%s", newPrice);

    printf("Enter the new Quantity: ");
    scanf("%s", newQuantity);

    // update with awk
    char awkCommand[200];
    char formattedNewName[50];
    char formattedPrice[20];
    char formattedQuantity[20];

    snprintf(formattedNewName, sizeof(formattedNewName), "%-20s", 
newName);
    snprintf(formattedPrice, sizeof(formattedPrice), "%-20s", newPrice);
    snprintf(formattedQuantity, sizeof(formattedQuantity), "%-20s", 
newQuantity);

    sprintf(awkCommand, "awk 'NR == 1 || $1 != \"%s\" { print } $1 == 
\"%s\" { $1 = \"%s\"; $2 = \"%s\"; $3 = \"%s\"; print $0 }' %s > temp.txt 
&& mv temp.txt %s", nameToUpdate, nameToUpdate, formattedNewName, 
formattedPrice, formattedQuantity, currentFileName, currentFileName);
    system(awkCommand);
}

    // update with awk
    system(updateScript);

    pthread_mutex_unlock(&mutex);
    sem_post(&serv_sem);
    sem_wait(&cli_sem);
}

void *closeFile(void *arg){
    
    pthread_mutex_lock(&mutex);
    printf("Closing the file...\n");

    // Close file
    int result = close(open(currentFileName, O_WRONLY | O_CREAT, S_IRUSR | 
S_IWUSR));

    //set current file name empty again
    strcpy(currentFileName, "");
    
    pthread_mutex_unlock(&mutex);
    sem_post(&serv_sem);
    sem_wait(&cli_sem);
}


void *mainThreadFunction(void *arg) {
    while (!exitFlag) {
        // command from user
        char command[10];

        // get the command
        printf("Command (open/read/write/update/close/exit): ");
        scanf("%s", command);

        sem_post(&cli_sem);  // Signal command threads to proceed

        //allow selected process to run according to input command
        if (strcmp(command, "open") == 0) {
            pthread_t openThread;
            pthread_create(&openThread, NULL, openFile, NULL);
            sem_wait(&serv_sem);
        } else if (strcmp(command, "read") == 0) {
            pthread_t readThread;
            pthread_create(&readThread, NULL, readFile, NULL);
            sem_wait(&serv_sem);
        } else if (strcmp(command, "write") == 0) {
            pthread_t writeThread;
            pthread_create(&writeThread, NULL, writeFile, NULL);
            sem_wait(&serv_sem);
        } else if (strcmp(command, "update") == 0) {
            pthread_t updateThread;
            pthread_create(&updateThread, NULL, updateFile, NULL);
            sem_wait(&serv_sem);
        } else if (strcmp(command, "close") == 0) {
            pthread_t closeThread;
            pthread_create(&closeThread, NULL, closeFile, NULL);
            sem_wait(&serv_sem);
        } else if (strcmp(command, "exit") == 0) {
            exitFlag = 1;  // Set the exit flag to terminate the main 
thread
            break;
        } else {
            printf("Invalid command. Please enter 'open', 'read', 'write', 
'update', 'close' or 'exit'.\n");
        }

        sem_post(&cli_sem);  // Allow the main thread to wait for the next 
command
    }

    pthread_exit(NULL);
}

int main() {
    // Initilize semaphores
    sem_init(&serv_sem, 0, 0);
    sem_init(&cli_sem, 0, 1); // 1 for initializing client semaphore first

    // Define threads
    pthread_t mainThread;

    pthread_create(&mainThread, NULL, mainThreadFunction, NULL);

    // Join main thread to wait for its completion
    pthread_join(mainThread, NULL);

    // Destroy semaphores and threads
    sem_destroy(&serv_sem);
    sem_destroy(&cli_sem);
    pthread_mutex_destroy(&mutex);

    return 0;
}


