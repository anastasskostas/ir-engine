#include <iostream>
#include <fstream>
#include <mutex>
#include <thread>
#include <sys/time.h>
#include "InvertedIndex.h"

using namespace std;

mutex documentsMutex;
mutex queriesMutex;
volatile int documentsCounter,queriesCounter;
volatile int totalDocs,totalQueries;
ifstream input;

/**
* Gets the next document-line from file and returns its content.
*/
string getNextDocument()
{
    string docLine;

    documentsMutex.lock();
    if(documentsCounter >= totalDocs) //if doc-lines are finished
    {
        docLine = "";
    }
    else
    {
        std::getline(input, docLine);
        documentsCounter++;
    }
    documentsMutex.unlock();

    return docLine;
}


/**
* Start building an inverted index. Called by threads.
*/
void buildInvertedIndex(InvertedIndex *index)
{
    string document = getNextDocument();
    while(document != "")
    {
        index->addDocument(document, documentsCounter);
        document = getNextDocument();
    }
    index->calculateDocMaxFreq();
    index->calculateTF();
}

/**
* Gets the next query-line from file and returns its content.
*/
string getNextQuery(){
    string queryLine;

    queriesMutex.lock();
    if(queriesCounter >= totalQueries)
    {
        queryLine = "";
    }
    else
    {
        std::getline(input, queryLine);
        queriesCounter++;
    }
    queriesMutex.unlock();

    return queryLine;
}

/**
* Start answering the queries from the file. Called by threads.
*/
void executeQueries(InvertedIndex *index)
{
    string query = getNextQuery();
    while(query != "")
    {
        index->executeQuery(query);
        query = getNextQuery();
    }
}


int main()
{
    //used to acquire documents, line by line
    std::string line;
    input.open("documents/documents.txt");

    //first line of docs is the number of documents we have
    std::getline(input, line);
    totalDocs = atoi(line.c_str());
    cout << "Total Documents: " << totalDocs << endl;

    //used to identify number of threads available
    int noConcurrentThreads = thread::hardware_concurrency();
    cout << "We will work on " << noConcurrentThreads << " concurrent threads" << endl;

    documentsCounter = -1; //set to start docIDs from zero (0).

    struct timeval startTime,endTime,midTime;
    gettimeofday(&startTime,NULL);

    vector<InvertedIndex*> indexes(noConcurrentThreads);

    for(int i = 0 ; i < noConcurrentThreads; i++)
    {
        indexes[i] = new InvertedIndex(totalDocs);
    }

    vector<thread> threads(noConcurrentThreads);

    for(unsigned int i=0; i<threads.size(); ++i)
    {
        threads[i] = thread(buildInvertedIndex, indexes[i]);
    }

    for(unsigned int i=0; i<threads.size(); ++i)
    {
        threads[i].join();
    }

    input.close();

    //join all the indexes to the first index in indexes[0]
    for(unsigned int i=1; i<indexes.size(); i++)
    {
        indexes[0]->joinIndex(indexes[i]);
    }
    indexes[0]->calculateIDFandBuildDocMagnitudes();

    gettimeofday(&midTime,NULL);

    long startTotalMicro = startTime.tv_sec * 1000000 + startTime.tv_usec;
    long endTotalMicro = midTime.tv_sec * 1000000 + midTime.tv_usec;
    long microseconds = endTotalMicro - startTotalMicro ;
    double seconds = microseconds / 1000000.0;

    cout<<endl<<"Index created in: "<< seconds <<"  seconds."<<endl<<endl<<endl;

    threads.clear();
    threads.resize(noConcurrentThreads);

    input.open("queries/queries2.txt");
    queriesCounter = -1;
    std::getline(input,line);
    totalQueries = atoi(line.c_str());
    cout << "Total Queries: " << totalQueries <<endl<<endl;


    for(unsigned int i=0; i<threads.size(); ++i)
    {
        threads[i] = thread(executeQueries, indexes[0]);
    }

    for(unsigned int i=0; i<threads.size(); ++i)
    {
        threads[i].join();
    }

    input.close();

    gettimeofday(&endTime,NULL);
    startTotalMicro = midTime.tv_sec * 1000000 + midTime.tv_usec;
    endTotalMicro = endTime.tv_sec * 1000000 + endTime.tv_usec;
    microseconds = endTotalMicro - startTotalMicro ;
    seconds = microseconds / 1000000.0;

    cout<<endl<<"All queries where answered in: "<< seconds <<"  seconds."<<endl<<endl<<endl;

    for(unsigned int i = 0; i < indexes.size(); i++){
        delete indexes[i];
    }

    return 0;
}
