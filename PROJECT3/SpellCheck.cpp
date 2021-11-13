#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <queue>
#include <vector>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <typeinfo>
#include <signal.h>

// Default Dictionary
#define DEFAULT_DICTIONARY "dictionary.txt";
// Default Port
#define DEFAULT_PORT 8888;

using namespace std;

// Dictionary into vector of strings
vector<string> vectorOfWords;

// Client socket queue with integer queue
std::queue<int> clientSocket;
// Logger thread queue with character pointer queue
std::queue<string> loggerQueue;

// Job queue lock
pthread_mutex_t client_queue_lock;
// Empty connection
pthread_cond_t conditionClientFree;
// Connection is full/being used
pthread_cond_t conditionClientUsed;

// Log queue lock
pthread_mutex_t log_queue_lock;
pthread_cond_t conditionLogFree;
pthread_cond_t conditionLogUsed;

// Function string spell_Check()
string spell_Check(string wordBuffer)
{
    //
    wordBuffer = wordBuffer.substr(0, wordBuffer.size() - 1);

    for (unsigned int i = 0; i < vectorOfWords.size(); i++)
    {
        // Compare the entered user word with words in the dictionary
        if (wordBuffer.compare(vectorOfWords[i]) == 0)
        {
            wordBuffer = wordBuffer + " Correct!\n";
            return wordBuffer;
        } // end if

    } // end for

    wordBuffer = wordBuffer + " Incorrect!\n";
    return wordBuffer;
} // end string spell_Check()

// Function void word_Clear()
void word_Clear(char *word)
{
    for (unsigned int i = 0; i < sizeof(word); i++)
    {
        word[i] = '\0';
    }
} // end void word_Clear()

// Function void manager_Client()
void manager_Client(int socket)
{
    // unlock the lock from queue lock
    pthread_mutex_unlock(&client_queue_lock);
    char word[100];

    string text = "Enter word to test with the spell checker:\n";
    if (send(socket, text.c_str(), text.length(), 0) == -1)
    {
        cout << "ERROR! Couldnt send message to client" << endl;
    } // end if

    while (true)
    {

        word_Clear(word);

        // get input from client
        int checking = recv(socket, word, sizeof(word), 0);
        if (checking == -1)
        {
            cout << "ERROR! Could not recieving message from client" << endl;
        } // end if

        if (checking == 0)
        {
            cout << "Disconnected" << endl;
            close(socket);
            break;
        } // end if

        // checked word send it to client
        string result;

        if (word == NULL || word[0] == '\0')
        {
            result = "Empty Word!";
        }
        else
        {
            result = spell_Check(word);
        }

        if (send(socket, result.c_str(), result.length(), 0) == -1)
        {
            cout << "ERROR! Could not send message to client" << endl;
        } // end if

        cout << result << endl;

        // get lock and send signal to log thread
        pthread_mutex_lock(&log_queue_lock);
        // push result in the logger queue
        loggerQueue.push(result);
        // signal log is used
        pthread_cond_signal(&conditionLogUsed);

        // unlocked log queue
        pthread_mutex_unlock(&log_queue_lock);

    } // end while

} // end void manager_Client()

// Function void *worker_Thread()
void *worker_Thread(void *temp)
{
    while (true)
    {
        pthread_mutex_lock(&client_queue_lock);

        if (clientSocket.size() == 0)
        {
            pthread_cond_wait(&conditionClientUsed, &client_queue_lock);
        } // end if
        while (clientSocket.size() > 0)
        {
            int socket = (clientSocket.front());
            clientSocket.pop();
            pthread_cond_signal(&conditionClientFree);
            manager_Client(socket);
        } // end while
        pthread_mutex_unlock(&client_queue_lock);

    } // end while

} // end void *worker_Thread()

// Function void *service_Log()
void *service_Log(void *temp)
{
    ofstream output;

    while (true)
    {
        pthread_mutex_lock(&log_queue_lock);

        while (loggerQueue.size() == 0)
        {
            pthread_cond_wait(&conditionLogUsed, &log_queue_lock);
        } // end while

        while (loggerQueue.size() > 0)
        {
            output.open("log.txt", ios::app);
            output << loggerQueue.front();
            loggerQueue.pop();
            pthread_cond_signal(&conditionLogFree);
            output.close();
        } // end while
        pthread_mutex_unlock(&log_queue_lock);
    } // end while

} // end void *service_Log()

// Functionvoid create_Thread()
void create_Thread()
{

    // lock the job queue
    pthread_mutex_init(&client_queue_lock, NULL);
    // free the client
    pthread_cond_init(&conditionClientFree, NULL);
    // use the client
    pthread_cond_init(&conditionClientUsed, NULL);

    pthread_mutex_init(&log_queue_lock, NULL);
    pthread_cond_init(&conditionLogFree, NULL);

    pthread_t worker1, worker2, worker3, worker4, worker5, log;

    pthread_create(&worker1, NULL, &worker_Thread, NULL);
    pthread_create(&worker2, NULL, &worker_Thread, NULL);
    pthread_create(&worker3, NULL, &worker_Thread, NULL);
    pthread_create(&worker4, NULL, &worker_Thread, NULL);
    pthread_create(&worker5, NULL, &worker_Thread, NULL);
    pthread_create(&log, NULL, &service_Log, NULL);

} // end create_Thread()

// Function void initialize_Directory()
void initialize_Directory(string dictionaryTitle)
{
    ifstream infile;
    string temp;

    infile.open(dictionaryTitle.c_str());
    while (!infile.eof())
    {
        getline(infile, temp);
        vectorOfWords.push_back(temp);
    } // end while

} // end void initialize_Directory()

// Function bool isNumber()
bool isNumber(string string)
{
    for (unsigned int i = 0; i < string.length(); i++)
        if (isdigit(string[i]) == false)
        {
            return false;
        } // end if
    return true;
} // end bool isNumber()

int main(int argc, char *argv[])
{
    string dictionaryTitle;
    int portNumber;
    int socket_desc, new_socket, c;
    struct sockaddr_in server, client;

    if (argc == 1)
    {
        dictionaryTitle = DEFAULT_DICTIONARY;
        portNumber = DEFAULT_PORT;
    } // end if
    else if (argc == 2)
    {
        if (isNumber(argv[1]))
        {
            portNumber = atoi(argv[1]);
            dictionaryTitle = DEFAULT_DICTIONARY;
        } // end if
        else
        {
            portNumber = DEFAULT_PORT;
            dictionaryTitle = argv[1];
        } // end else

    } // end else if
    else if (argc == 3)
    {
        if (isNumber(argv[1]))
        {
            portNumber = atoi(argv[1]);
            dictionaryTitle = argv[2];
        } // end if
        else
        {
            dictionaryTitle = argv[1];
            portNumber = atoi(argv[2]);
        } // end else

    } // end else if
    initialize_Directory(dictionaryTitle);

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1)
    {
        puts("ERROR! socket creation has failed!");
        exit(1);
    } // end if

    server.sin_family = AF_INET;
    // defaults to 127.0.0.1
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(portNumber);

    int bind_result = bind(socket_desc, (struct sockaddr *)&server, sizeof(server));
    if (bind_result < 0)
    {
        puts("ERROR! Binding has failed.");
        exit(1);
    } // end if

    puts("Binding is done.");

    listen(socket_desc, 3);
    puts("Waiting for incoming connections...");

    while (true)
    {

        c = sizeof(struct sockaddr_in);
        new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c);
        if (new_socket < 0)
        {
            puts("Error: Accept failed");
            continue;
        } // end if
        puts("Connection accepted!");

        create_Thread();

        pthread_mutex_lock(&client_queue_lock);

        clientSocket.push(new_socket);
        pthread_cond_signal(&conditionClientUsed);

        pthread_mutex_unlock(&client_queue_lock);
    } // end while
    return 0;
} // end main()
