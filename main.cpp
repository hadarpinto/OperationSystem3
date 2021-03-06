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
#include <atomic>
#define CO_EDITORS 3

using namespace std;

struct confData {
    int index;
    int newsNum;
    int capacity;
};


class BoundedQ : public queue<string> {
public:
    int size;
    sem_t semEmpty;
    sem_t semFull;

    pthread_mutex_t mutexBuffer;

    explicit BoundedQ(int capacity) : size(capacity) {
        pthread_mutex_init(&mutexBuffer, nullptr);
        sem_init(&semEmpty, 0, capacity);
        sem_init(&semFull, 0, 0);
    }

    void insert(string s) {
        sem_wait(&semEmpty);
        pthread_mutex_lock(&mutexBuffer);
        push(s);
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);

    }

    string remove() {
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        string x = front();
        pop();
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semEmpty);
        return x;

    }

    void destroy() {
        sem_destroy(&semEmpty);
        sem_destroy(&semFull);
        pthread_mutex_destroy(&mutexBuffer);
    }
};

class UnBoundedQ : public queue<string> {
public:
    sem_t semFull;
    pthread_mutex_t mutexBuffer;

    explicit UnBoundedQ() {
        pthread_mutex_init(&mutexBuffer, NULL);
        sem_init(&semFull, 0, 0);
    }

    void insert(string s) {
        pthread_mutex_lock(&mutexBuffer);
        push(s);
        pthread_mutex_unlock(&mutexBuffer);
        sem_post(&semFull);

    }

    string remove() {
        sleep(5);
        sem_wait(&semFull);
        pthread_mutex_lock(&mutexBuffer);
        string x = front();
        pop();
        pthread_mutex_unlock(&mutexBuffer);
        return x;

    }

    void destroy() {
        sem_destroy(&semFull);
        pthread_mutex_destroy(&mutexBuffer);
    }
};


string produce(int newNumber, int index) {
    int stat = newNumber % 3;
    string s = to_string(index);
    string newNumberString = to_string(newNumber / 3);
    string concatenate;
    if (stat == 0) {
        concatenate = "Producer " + s + " SPORTS " + newNumberString;
        return concatenate;
    } else if (stat == 1) {
        concatenate = "Producer " + s + " WEATHER " + newNumberString;
        return concatenate;
    } else if (stat == 2) {
        concatenate = "Producer " + s + " NEWS " + newNumberString;
        return concatenate;
    }
}
/**
 * Global vars
 */

vector<BoundedQ *> vecQs;
vector<UnBoundedQ *> coQs;
BoundedQ *boundedQ;

void setToRightQ(string s) {
    //return -1 if not found
    int news = s.find("NEWS");
    int sport = s.find("SPORTS");
    int weather = s.find("WEATHER");
    if (news != -1) {
        coQs[0]->insert(s);
    }
    if (sport != -1) {
        coQs[1]->insert(s);
    }
    //weather
    if (weather != -1) {
        coQs[2]->insert(s);
    }
    if (sport == -1 && weather == -1 && news == -1) {
        coQs[0]->insert("DONE");
        coQs[1]->insert("DONE");
        coQs[2]->insert("DONE");
    }
}

void* producer(void *args) {
    struct confData *d = (struct confData *) args;
    int producerIndex = d->index;
    int newsNum = d->newsNum;
    atomic<int> i;
    i = 0;
    while (i < newsNum) {
        // Produce
        string s = produce(i, producerIndex);
        // Add to the buffer
        vecQs[producerIndex - 1]->insert(s);
        //count++;
        i++;

    }
    vecQs[producerIndex - 1]->insert("DONE");
}

void* dispatcher(void *args) {
    int sizeOfvector = *(int *) args;
    int done = sizeOfvector;
    while (done != 0) {
        for (int i = 0; i < sizeOfvector; i++) {
            if (vecQs[i] == nullptr) {
                continue;
            }
            string s;
            s = vecQs[i]->remove();
            if (s == "DONE") {
                vecQs[i]->destroy();
                vecQs[i] = nullptr;
                done--;
                continue;
            }
            setToRightQ(s);
        }
    }
    setToRightQ("DONE");
}

void* coEditor(void *args) {
    int coEditorQueue = *(int *) args;
    while (1) {
        string s = coQs[coEditorQueue]->remove();
        if (s == "DONE") {
            boundedQ->insert(s);
            break;
        }
        boundedQ->insert(s);
    }
}

void* screenManger(void *args) {
    int done = 0;

    while (1) {
        string s = boundedQ->remove();
        if (s != "DONE") {
            cout << s << endl;
        } else {
            done++;
        }
        if (done == 3) {
            break;
        }
    }
}

int main(int argv, char **argc) {
    // getting num of producers
    fstream conf;
    int producersNum = 0, coEditorSize;
    string s;
    string filename = argc[1];
    conf.open(filename, ios::in);
    if (conf.is_open()) {
        while (!conf.eof()) {
            getline(conf, s);
            producersNum++;
        }
        conf.close();
    }
    producersNum = producersNum / 4;
    // reading from conf file
    vector<struct confData> dataVector;
    conf.open(filename, ios::in);
    if (conf.is_open()) {
        string line;
        for (int i = 0; i < producersNum; i++) {
            struct confData d{};
            getline(conf, line);
            d.index = stoi(line);
            getline(conf, line);
            d.newsNum = stoi(line);
            getline(conf, line);
            d.capacity = stoi(line);
            dataVector.push_back(d);
            getline(conf, line);
        }
        getline(conf, line);
        coEditorSize = stoi(line);
        conf.close();
    }
    for (int j = 0; j < producersNum; j++) {
        BoundedQ *b = new BoundedQ(dataVector[j].capacity);
        vecQs.push_back(b);
    }

    //initialize co-editors queues
    for (int j = 0; j < CO_EDITORS; j++) {
        UnBoundedQ *b = new UnBoundedQ();
        coQs.push_back(b);
    }
    boundedQ = new BoundedQ(coEditorSize);

    pthread_t producers[producersNum];
    pthread_t dispatch;
    pthread_t coEditors[CO_EDITORS];
    pthread_t screen;

    for (int i = 0; i < producersNum; i++) {
        sleep(0.11);
        struct confData *cd = &dataVector[i];
        if (pthread_create(&producers[i], nullptr, &producer, cd) != 0) {
            perror("Failed to create thread");
        }
    }

    if (pthread_create(&dispatch, nullptr, &dispatcher, &producersNum) != 0) {
        perror("Failed to create thread");
    }
    sleep(1);

    atomic<int> queueIndex;
    queueIndex = -1;
    mutex m;
    for (int j = 0; j < CO_EDITORS;j++) {
    sleep(0.11);
        if (pthread_create(&coEditors[j], nullptr, &coEditor, &queueIndex) != 0) {
            perror("Failed to create thread");
        }
        m.lock();
        queueIndex++;
        m.unlock();
    }

    if (pthread_create(&screen, nullptr, &screenManger, nullptr) != 0) {
        perror("Failed to create thread");
    }

    for (int i = 0; i < producersNum; i++) {
        if (pthread_join(producers[i], nullptr) != 0) {
            perror("Failed to create thread");
        }
    }

    if (pthread_join(dispatch, nullptr) != 0) {
        perror("Failed to create thread");
    }

    for (int i = 0; i < CO_EDITORS; i++) {
        if (pthread_join(coEditors[i], nullptr) != 0) {
            perror("Failed to create thread");
        }
    }

    if (pthread_join(screen, nullptr) != 0) {
        perror("Failed to create thread");
    }

    cout << "DONE" << endl;
    return 0;
}
