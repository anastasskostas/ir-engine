#ifndef INVERTEDINDEX_H
#define INVERTEDINDEX_H

#include <string>
#include <unordered_map>
#include <list>
#include <vector>

using namespace std;

typedef struct DocWordData{
    int docID;  // identificator of the document
    float TF;   // freq/maxfreq_of_any_word_in_this_document
    int freq;   // how many times word appears on document = positions.size()
    list<int> positions; //in which positions of the doc the word appears
} DocWordData;

class InvertedIndex
{
    private:
        unordered_map<string,list<DocWordData>*> dictionary; //keeps all the words of the index with its associated data
        unordered_map<string, float> IDF; // the idf value for each word in our dictionary
        vector<int> docsMaxFreq; //max term frequency of every document
        vector<float> docsMagnitudes; //|doc| the magnitude (metro dianismatos) of the doc

    public:
        InvertedIndex(int totalDocs);
        virtual ~InvertedIndex();
        void add(string word, int documentID,int position); // add word to index
        void addDocument(string documentLine, int docID);   // add document to index
        void calculateDocMaxFreq();                         // calculate max frequency of any term in a doc
        void calculateTF();                                 // calculate term frequency
        void calculateIDFandBuildDocMagnitudes();           // calculate IDF values and doc magnitudes sums
        void joinIndex(InvertedIndex *otherIndex);          // connect all created indexes in one
        void executeQuery(string queryLine);                // answer the queries with consine similarity(documents-query)
        static mutex printMutex;                            // necessary variable to print the results seperately for each query
        void printIndex();                                  // prints all elements of index - used for debugging
        string convertToLowerCase(string documentLine);     // convert document words into lower case
};

#endif // INVERTEDINDEX_H
