#include <iostream>
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

    pthread_mutex_t mutexBuffer;
    explicit BoundedQ(int capacity): size(capacity){
        pthread_mutex_init(&mutexBuffer, NULL);
        sem_init(&semEmpty, 0, 10);
        sem_init(&semFull, 0, 0);
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


void* producer(void* args) {
    struct confData* d = (struct confData*) args;
    int producerIndex = d->index;
    int newsNum = d->newsNum;
    int i = 0;
    while (i < newsNum) {
        // Produce
        string s = produce(i, 1);
        sleep(0.1);

        // Add to the buffer

        vecQs[0]->insert(s);
        //count++;
        i++;

    }
}

void* dispatcher(void* args) {
    int sizeOfvector = *(int*) args;
    int flag = 0;
    while (1) {
        for (int i = 0; i < sizeOfvector; i++){
            string y;

            // Remove from the buffer

            y = vecQs[i]->remove();
            // Consume
            cout << y << endl;
            sleep(1);
            if(vecQs[i]->empty()) {
                flag = 1;
                vecQs[i]->destroy();
                break;
            }
        }

        if (flag){
            break;
        }
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

    srand(time(NULL));
    pthread_t th[producersNum+1];


    int i;
    for (i = 0; i <= producersNum; i++) {
        if (i > 0) {
            struct confData* cd = &dataVector[i-1];
            if (pthread_create(&th[i], NULL, &producer, cd) != 0) {
                perror("Failed to create thread");
            }
        } else {
            if (pthread_create(&th[i], NULL, &dispatcher, &producersNum) != 0) {
                perror("Failed to create thread");
            }
        }
    }
    for (i = 0; i < 2; i++) {
        if (pthread_join(th[i], NULL) != 0) {
            perror("Failed to join thread");
        }
    }

    return 0;


}
