#include "image_rotation.h"
 
 
//Global integer to indicate the length of the queue??
int qLen = 0;
int next = 0;
//Global integer to indicate the number of worker threads
int numWorkers;
//Global file pointer for writing to log file in worker??
FILE *log_file;
//Might be helpful to track the ID's of your threads in a global array
//What kind of locks will you need to make everything thread safe? [Hint you need multiple]
//What kind of CVs will you need  (i.e. queue full, queue empty) [Hint you need multiple]
//How will you track the requests globally between threads? How will you ensure this is thread safe?
//How will you track which index in the request queue to remove next?
//How will you update and utilize the current number of requests in the request queue?
//How will you track the p_thread's that you create for workers?
//How will you know where to insert the next request received into the request queue?
pthread_mutex_t lock  = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;
struct request_queue requestQueue[MAX_QUEUE_LEN];
// struct request_queue* requestQueue = malloc(MAX_QUEUE_LEN * sizeof(struct request_queue)); //request_t
/*
    The Function takes:
    to_write: A file pointer of where to write the logs. 
    requestNumber: the request number that the thread just finished.
    file_name: the name of the file that just got processed. 

    The function output: 
    it should output the threadId, requestNumber, file_name into the logfile and stdout.
*/
void log_pretty_print(FILE* to_write, int threadId, int requestNumber, char * file_name){
    int TEMP_STDOUT_FILENO = dup(STDOUT_FILENO);
    int fd = fileno(to_write);
    if(dup2(fd, STDOUT_FILENO) == -1){
        perror("Failed to redirect output\n");
        exit(-1);
    }
    printf("Thread ID: %d, Request Number: %d, Filename: %s\n", threadId, requestNumber, file_name);
    fflush(stdout);
    close(fd);
    if(dup2(TEMP_STDOUT_FILENO, STDOUT_FILENO) == -1){
        perror("Failed to restore output\n");
        exit(-1);
    }
    close(TEMP_STDOUT_FILENO);
    printf("Thread ID: %d, Request Number: %d, Filename: %s\n", threadId, requestNumber, file_name);
}


/*

    1: The processing function takes a void* argument called args. It is expected to be a pointer to a structure processing_args_t 
    that contains information necessary for processing.

    2: The processing thread needs to traverse a given dictionary and add its files into the shared queue while maintaining synchronization using lock and unlock. 

    3: The processing thread should pthread_cond_signal/broadcast once it finish the traversing to wake the worker up from their wait.

    4: The processing thread will block(pthread_cond_wait) for a condition variable until the workers are done with the processing of the requests and the queue is empty.

    5: The processing thread will cross check if the condition from step 4 is met and it will signal to the worker to exit and it will exit.

*/

void *processing(void *args) {
    processing_args_t *procArgs = (processing_args_t *)args;
    DIR *dir = opendir(procArgs->dirPath);
    if(dir == NULL) {
        exit(-1);
    }
    struct dirent *entry;

    while((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
        char dirName[150];
        strcpy(dirName, procArgs->dirPath);
        pthread_mutex_lock(&lock);
        strcat(dirName, "/");
        strcat(dirName, entry->d_name);
        printf("adding file: %s to queue position %d\n", entry->d_name, qLen);
        // requestQueue[qLen] = (request_t) {.fileName = dirName, .angle = procArgs->angle};
        strcpy(requestQueue[qLen].fileName, dirName);
        requestQueue[qLen].angle = procArgs->angle;
        // printf("FileName %s ANGLE: %d\n", requestQueue[qLen - 1].fileName, requestQueue[qLen - 1].angle);
        qLen += 1;
        pthread_mutex_unlock(&lock);
        pthread_cond_signal(&condition);

        }
    }
    // pthread_cond_signal(&condition);
    for(int i = 0; i < qLen; i++) {
        printf("Path: %s\nAngle: %d\n", requestQueue[i].fileName, requestQueue[i].angle);

    }
    pthread_cond_signal(&condition);
    if (pthread_cond_wait(&condition, &lock)) {
        pthread_cond_signal(&condition);
        exit(-1); 
    }
    pthread_cond_wait(&condition, &lock);
    return NULL;
}

/*
    1: The worker threads takes an int ID as a parameter

    2: The Worker thread will block(pthread_cond_wait) for a condition variable that there is a requests in the queue. 

    3: The Worker threads will also block(pthread_cond_wait) once the queue is empty and wait for a signal to either exit or do work.

    4: The Worker thread will processes request from the queue while maintaining synchronization using lock and unlock. 

    5: The worker thread will write the data back to the given output dir as passed in main. 

    6:  The Worker thread will log the request from the queue while maintaining synchronization using lock and unlock.  

    8: Hint the worker thread should be in a While(1) loop since a worker thread can process multiple requests and It will have two while loops in total
        that is just a recommendation f;eel free to implement it your way :) 
    9: You may need diffeent lock depending on the job.  

*/


void * worker(void *args)
{
    working_args_t *workArgs = (working_args_t *)args;
    int id = workArgs->id;
    printf("id: %d\n", id);
    int width, height, bpp;

    while (1) {
        pthread_mutex_lock(&lock);

        // Wait for a signal that there's work to do or the processing is done
        while (qLen <= 0) {
            pthread_cond_wait(&condition, &lock);
        }
    // pthread_exit(NULL);
        printf("Qlen %d next %d filName %s\n", qLen, next, requestQueue[next].fileName);
        next ++;
        // return NULL;
            // Stbi_load takes: A file name, int pointer for width, height, and bpp


       uint8_t* image_result = stbi_load(requestQueue[next].fileName, &width, &height, &bpp,  CHANNEL_NUM);
        

        uint8_t **result_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        uint8_t** img_matrix = (uint8_t **)malloc(sizeof(uint8_t*) * width);
        for(int i = 0; i < width; i++){
             result_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
             img_matrix[i] = (uint8_t *)malloc(sizeof(uint8_t) * height);
        }
        /*
        linear_to_image takes: 
            The image_result matrix from stbi_load
            An image matrix
            Width and height that were passed into stbi_load
        
        */
        linear_to_image(image_result, img_matrix, width, height);
        qLen -= 1;
        printf("Qlen: %d\n", qLen);
        pthread_mutex_unlock(&lock);
        ////TODO: you should be ready to call flip_left_to_right or flip_upside_down depends on the angle(Should just be 180 or 270)
        //both take image matrix from linear_to_image, and result_matrix to store data, and width and height.
        //Hint figure out which function you will call. 
        //flip_left_to_right(img_matrix, result_matrix, width, height); or flip_upside_down(img_matrix, result_matrix ,width, height);
        if(request.angle == 270) {
            flip_left_to_right(img_matrix, result_matrix, width, height);
        }
        else {
            flip_upside_down(img_matrix, result_matrix ,width, height);
        }

        
        
        uint8_t* img_array = (uint8_t *) malloc(sizeof(uint8_t) * width * height); ///Hint malloc using sizeof(uint8_t) * width * height
    

        ///TODO: you should be ready to call flatten_mat function, using result_matrix
        //img_arry and width and height; 
        flatten_mat(result_matrix, img_array, width, height);


        ///TODO: You should be ready to call stbi_write_png using:
        //New path to where you wanna save the file,
        //Width
        //height
        //img_array
        //width*CHANNEL_NUM
        stbi_write_png(logFIle, width, height, CHANNEL_NUM, img_array, width*CHANNEL_NUM);

            
    }
    return NULL;
}

/*
    Main:
        Get the data you need from the command line argument 
        Open the logfile
        Create the threads needed
        Join on the created threads
        Clean any data if needed. 


*/

int main(int argc, char* argv[])
{
    if(argc != 5)
    {
        fprintf(stderr, "Usage: File Path to image dirctory, File path to output dirctory, number of worker thread, and Rotation angle\n");
    }
    ///TODO: 

    //creating args
    numWorkers = atoi(argv[3]);
    int rotationAngle = atoi(argv[4]);
    char *imgDirectorypath = argv[1];
    char *outputDirectorypath = argv[2];

    //creates 1 processor thread
    processing_args_t procArgs = {imgDirectorypath, numWorkers, rotationAngle};
    pthread_t processor;
    if(pthread_create(&processor, NULL, (void*)processing, &procArgs) != 0) {
        fprintf(stderr, "Error creating processor!!!\n");
        exit(-1);
    }
    //creates n worker threads
    pthread_t workers[numWorkers];
    working_args_t workArgs[numWorkers]; // ???
    for (int i = 0; i < numWorkers; i++) {
        int threadId = i;
        workArgs[i].id = threadId;
        if(pthread_create(&workers[i], NULL, (void *)worker, &workArgs[i]) != 0) {
            fprintf(stderr, "Error creating worker!!!\n");
            exit(-1);
        }
    }
    for (int i = 0; i < numWorkers; i++) {
        if(pthread_join(workers[i], NULL) != 0) {
            fprintf(stderr, "Error joining worker!!!\n");
            exit(-1);
        }
    }
    if(pthread_join(processor, NULL) != 0) {
        fprintf(stderr, "Error joining processor!!!\n");
        exit(-1);
    }

    return 0;
}