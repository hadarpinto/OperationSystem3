#include <iostream>
#include <deque>
#include <semaphore.h>
#include <unistd.h>
#include <queue>
#include <vector>
#include <pthread.h>
#include <mutex>
#include <fstream>
#include <string>

#define CO_EDITORS 3
#define DISPATCHER 1
#define SCREEN_MANAGER 1
using namespace std;

struct confData {
    int index;
    int newsNum;
    int capacity;
};


class BoundedQ:public queue<string> {
public:
    int size;
    sem_t semEmpty;
    sem_t semFull;
    int doneFlag;

    pthread_mutex_t mutexBuffer;
    explicit BoundedQ(int capacity): size(capacity){
        pthread_mutex_init(&mutexBuffer, NULL);
        sem_init(&semEmpty, 0, capacity);
        sem_init(&semFull, 0, 0);
        doneFlag = 0;
    }

    void insert(string s){
        sem_wait(&semEmpty);
        pthread_mutex_lock(&mutexBuffer);
        push(s);
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);
    }
    string remove(){
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        string x = front();
        pop();
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semEmpty);
        return x;

    }

    void destroy(){
        sem_destroy(&semEmpty);
        sem_destroy(&semFull);
        pthread_mutex_destroy(&mutexBuffer);
    }
};

class UnBoundedQ:public queue<string> {
public:
    int size;
    sem_t semFull;
    int doneFlag;

    pthread_mutex_t mutexBuffer;
    explicit UnBoundedQ() {
        pthread_mutex_init(&mutexBuffer, NULL);
        sem_init(&semFull, 0, 0);
        doneFlag = 0;
    }

    void insert(string s){
        pthread_mutex_lock(&mutexBuffer);
        push(s);
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);
    }
    string remove(){
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        string x = front();
        pop();
        pthread_mutex_unlock(&mutexBuffer);
        return x;

    }

    void destroy(){
        sem_destroy(&semFull);
        pthread_mutex_destroy(&mutexBuffer);
    }
};


string produce(int newNumber, int index){
    int stat = newNumber % 3;
    string s = to_string(index);
    string newNumberString = to_string(newNumber/3);
    string concatenate;
    if (stat == 0){
        concatenate= "Producer " + s + " SPORTS " + newNumberString;
        return concatenate;
    }
    else if (stat == 1){
        concatenate= "Producer " + s + " WEATHER " + newNumberString;
        return concatenate;
    }
    else if (stat == 2) {
        concatenate= "Producer " + s + " NEWS " + newNumberString;
        return concatenate;
    }
}



vector<BoundedQ *> vecQs;
vector<BoundedQ *> coQs;
UnBoundedQ* unBoundedQ = new UnBoundedQ();

void setToRightQ(string s){
    //return -1 if not found
    int news = s.find("NEWS");
    int sport = s.find("SPORTS");
    if (news != -1){
        coQs[0]->insert(s);
    }
    else if(sport != -1){
        coQs[1]->insert(s);
    }
    //weather
    else {
        coQs[2]->insert(s);
    }
}

void* producer(void* args) {
    struct confData* d = (struct confData*) args;
    int producerIndex = d->index;
    int newsNum = d->newsNum;
    int i = 0;
    while (i < newsNum) {
        // Produce
        string s = produce(i, producerIndex);
        sleep(0.1);

        // Add to the buffer

        vecQs[producerIndex-1]->insert(s);
        //count++;
        i++;

    }
    while (1) {
        vecQs[producerIndex-1]->insert("DONE" + to_string(producerIndex));
    }
//    if (i == newsNum){
//        vecQs[producerIndex-1]->insert("DONE");
//        vecQs[producerIndex-1]->doneFlag = 1;
//    }



}

void* dispatcher(void* args) {
    int sizeOfvector = *(int*) args;
    int flag = 0;
    while (1) {
        for (int i = 0; i < sizeOfvector; i++){
            string y;
            // Remove from the buffer
            if(vecQs[i]->doneFlag == 1){
                continue;
            }
            y = vecQs[i]->remove();
            // Consume
//            if (y == "DONE") {
//                vecQs[i]->destroy();
//                //++
//            }
            setToRightQ(y);
            //cout << y << endl;
            sleep(1);

        }

    }
}

void* coEditor(void* args){
    int coEditorQueue = *(int*) args;
    //int coEditorQueue = 0;
    while(1) {
        string s = coQs[coEditorQueue]->remove();
        sleep(1);
        cout << s << endl;
        unBoundedQ->insert(s);
        sleep(1);
    }
}
int main() {
    // getting num of producers
    fstream conf;
    int producersNum = 0, coEditorSize;
    string s;
    conf.open("conf.txt", ios::in);
    if (conf.is_open()){
        while(!conf.eof()) {
            getline(conf,s);
            producersNum++;
        }
        conf.close();
    }
    producersNum = producersNum / 4;
    // reading from conf file
    vector<struct confData> dataVector;
    conf.open("conf.txt", ios::in);
    if (conf.is_open()){
        string line;
        for(int i = 0; i < producersNum; i++) {
            struct confData d{};
            getline(conf,line);
            d.index = stoi(line);
            getline(conf,line);
            d.newsNum = stoi(line);
            getline(conf,line);
            d.capacity = stoi(line);
            dataVector.push_back(d);
            getline(conf,line);
        }
        getline(conf,line);
        coEditorSize = stoi(line);
        conf.close();
    }
    for(int j = 0; j < producersNum; j++){
        BoundedQ* b = new BoundedQ(dataVector[j].capacity);
        vecQs.push_back(b);
    }

    //initialize co-editors queues
    for(int j = 0; j < 3 ; j++) {
        BoundedQ* b = new BoundedQ(coEditorSize);
        coQs.push_back(b);
    }

    srand(time(NULL));
    //int totalThreads = producersNum + DISPATCHER + CO_EDITORS + SCREEN_MANAGER;
    int totalThreads = producersNum + DISPATCHER + CO_EDITORS;
    pthread_t th[totalThreads];


    int i;
    for (i = 0; i < totalThreads ; i++) {
        if (i == 0){
            if (pthread_create(&th[i], NULL, &dispatcher, &producersNum) != 0) {
                perror("Failed to create thread");
            }
        }
        else if (i > 0 && i<=producersNum) {
            struct confData* cd = &dataVector[i-1];
            if (pthread_create(&th[i], NULL, &producer, cd) != 0) {
                perror("Failed to create thread");
            }
        } else if (i > producersNum && i<=producersNum+3) {
            int queueIndex = i - producersNum - 1;
            if (pthread_create(&th[i], NULL, &coEditor, &queueIndex) != 0) {
                perror("Failed to create thread");
            }
        }
    }
    for (i = 0; i < producersNum; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }

    return 0;


}
