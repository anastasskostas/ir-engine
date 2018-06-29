#include <iostream>
#include <fstream>
#include <thread>
#include <sstream> //istringstream
#include <iterator>
#include <mutex>
#include <math.h> //log2
#include <algorithm>
#include "InvertedIndex.h"

using namespace std;

mutex InvertedIndex::printMutex;

/**
* Sets the max freq and magnitude vectors to the
* number of docs that we have.
*/
InvertedIndex::InvertedIndex(int totalDocs)
{
    docsMaxFreq.resize(totalDocs);
    docsMagnitudes.resize(totalDocs);
}

/**
* Destroys the built inverted index, clearing out
* the memory used.
*/
InvertedIndex::~InvertedIndex()
{
    for( unordered_map<string,list<DocWordData>*>::iterator unordered_mapIt = dictionary.begin(); unordered_mapIt != dictionary.end(); ++unordered_mapIt)
    {
        delete unordered_mapIt->second;
    }
    dictionary.clear();
}


/**
* Adds a word to the inverted index. We have 3 cases:
* a) word doesnt exist already at the Inverted Index.  --> add the document to the word's list.
* b) document doesnt exist in word's list of document at the Index. --> add the document into word's list.
* c) documents exist in word's list. --> increase the word's frequency in the document and add the word's position.
*
* Notes: in case "b" we check the last element of the list because its the first and last time we examine the document.
* That means that the document can only be at the end, so the word has occured again in the document just before.
*/
void InvertedIndex::add(string word, int documentID,int posInDoc)
{
    if( dictionary[word] == nullptr) //case: a.
    {
        dictionary[word] = new list<DocWordData>();

        DocWordData n;
        n.docID = documentID;
        n.freq = 1;
        n.TF = 0;
        n.positions.push_back(posInDoc);
        dictionary[word]->push_back(n);

    } else if (dictionary[word]->back().docID != documentID) //case b.
    {
        DocWordData n;
        n.docID = documentID;
        n.freq = 1;
        n.TF = 0;
        n.positions.push_back(posInDoc);
        dictionary[word]->push_back(n);

    } else //case c.
    {
        dictionary[word]->back().freq++;
        dictionary[word]->back().positions.push_back(posInDoc);
    }
}

/**
* Converts line to lower case. We are aware of toLower() function,
* but we read that it has some issues and we prefer this safe mode.
*/
string InvertedIndex::convertToLowerCase(string line)
{
    // Parse words of the document
    for(int i = 0 ; i < line.length(); i++)
    {
        //If lowercase or space, continue to next character
        if((line[i] >= 'a' && line[i] <= 'z') || line[i] == ' ' || (line[i] >= '0' && line[i] <= '9' ) )
        {
            continue;
        }
        //If uppercase, convert to lowercase
        if(line[i] >= 'A' && line[i] <= 'Z')
        {
            line[i] += 32;
            continue;
        }
        //Else convert to space character
        line[i] = ' ';
    }

    return line;
}

/**
* Adds a document to the inverted index.
*/
void InvertedIndex::addDocument(string documentLine, int docID)
{
    documentLine = convertToLowerCase(documentLine); //string to lower case

    istringstream temp(documentLine); // Input stream class to operate on strings.

    vector<string> tokens;
    copy(istream_iterator<string>(temp),istream_iterator<string>(),back_inserter(tokens));


    for(int i = 0 ; i < tokens.size() ; i++)
    {
        this->add(tokens[i],docID,i);
    }
}

/**
* Calculates the maximum frequency that occurs in any word of the doc.
*/
 void InvertedIndex::calculateDocMaxFreq()
 {
    //Calculate the maximum frequency of any term for each document
    for(unordered_map<string,list<DocWordData>*>::iterator it = dictionary.begin(); it != dictionary.end(); ++it)
    {
        list<DocWordData> *documentEntries = it->second;

        //For every document that has this word
        for(list<DocWordData>::iterator listIt = documentEntries->begin(); listIt != documentEntries->end(); ++listIt)
        {
            if(docsMaxFreq[listIt->docID] < listIt->freq) //if frequency of the word is greater than current maxFreq of the document, update it.
            {
                docsMaxFreq[listIt->docID] = listIt->freq;
            }
        }
    }

 }

/**
* Calculates the Term Frequency for all the words in all docs.
*/
void InvertedIndex::calculateTF()
{
    for(unordered_map<string,list<DocWordData>*>::iterator it = dictionary.begin(); it != dictionary.end(); ++it)
    {
        list<DocWordData> *documentEntries = it->second;

        //For every document that has this word
        for(list<DocWordData>::iterator listIt = documentEntries->begin(); listIt != documentEntries->end(); ++listIt)
        {
            listIt->TF = 1.0 * listIt->freq / docsMaxFreq[listIt->docID]; //term_freq / max_term_freq
        }
    }
}

/**
* Calculates the IDF value for every term of the dictionary and build the sum of magnitude.
* Then, calculate the square of the magnitude calculated in order to produce the final value.
*/
void InvertedIndex::calculateIDFandBuildDocMagnitudes()
{
    //Calculate the IDF of each word and then increase the sum of each document's magnitude vector
    for(unordered_map<string,list<DocWordData>*>::iterator it = dictionary.begin(); it != dictionary.end(); ++it)
    {
        //it->first: the string of the current word being processed.
        //it->second->size(): the number of documents that possess the current word.
        IDF[it->first] = log2(1.0  + (1.0*docsMaxFreq.size()) / it->second->size()); //log2(1 + N/nt)
        list<DocWordData> *documentEntries = it->second;

        //Increase the mangnitude of all the documents that contain the word, building the magnitude sum
        for(list<DocWordData>::iterator listIt = documentEntries->begin(); listIt != documentEntries->end(); ++listIt)
        {
            float tmp = listIt->TF * IDF[it->first];
            docsMagnitudes[listIt->docID] += tmp * tmp;
        }
    }

    //Since all words are finished, it's time to simply calculate
    //the square root of runing magnitude to produce the final one.
    for(int i = 0; i < docsMagnitudes.size(); i++)
    {
        docsMagnitudes[i] = sqrt(docsMagnitudes[i]);
    }
}

/**
* Combines two indexes into one (the first). There are 2 case during this connection:
* a) word of index2 doesnt exist in index1 --> add it just like it is.
* b) word of index2 exists in index1 --> add the documents list of index2 word to the relevant list of index1.
*
* Finally, update the maximum frequencies of the incoming documents (from index2).
*/
void InvertedIndex::joinIndex(InvertedIndex *otherIndex)
{

    if(otherIndex->dictionary.size() == 0) return;

    for(unordered_map<string,list<DocWordData>*>::iterator it = otherIndex->dictionary.begin(); it != otherIndex->dictionary.end(); ++it)
    {
        //Get the word
        string word = it->first;

        if( dictionary[word] == nullptr)
        {
            dictionary[word] = new list<DocWordData>();
        }

        //Add to the list's end of this dictionaries word
        dictionary[word]->splice(dictionary[word]->end(), *(it->second));
    }


    //Transfer the documents Max frequences
    for(int i = 0; i < otherIndex->docsMaxFreq.size(); i++)
    {
        if(otherIndex->docsMaxFreq[i] > 0)
        {
            docsMaxFreq[i] = otherIndex->docsMaxFreq[i];
        }
    }
}


/**
* Prints the current index. Used for debugging.
*/
 void InvertedIndex::printIndex()
 {
    typedef std::unordered_map<std::string, list<DocWordData>*>::iterator it_type;

    for(it_type iterator = dictionary.begin(); iterator != dictionary.end(); iterator++)
    {
        string word = iterator->first;
        cout << "===================================" <<endl;
        cout << "word: " << word << endl;
        cout << "IDF: "<< IDF[word] <<endl;

        for(std::list<DocWordData>::iterator it = iterator->second->begin(); it!= iterator->second->end(); ++it)
        {
            cout << "-------" << endl;
            DocWordData temp = *it;
            cout << "docID: " << temp.docID <<endl;
            cout << "TF: "<< temp.TF <<endl;
            cout << "frequency: " << temp.freq <<endl;
            cout << "positions: <";
            for(std::list<int>::const_iterator it = temp.positions.begin(); it!= temp.positions.end(); ++it)
            {
                cout << *it << ",";
            }
            cout<< ">"<<endl<<endl;
        }
    }

    cout<< "DOCS TIME !!!!" <<endl;

    for(int i = 0; i < docsMagnitudes.size(); i++)
    {
        cout << " ---------------- " << endl;
        cout << "docID: " << i << endl;
        cout << "maxFreq: " << docsMaxFreq[i] << endl;
        cout << "magnitude: " << docsMagnitudes[i] << endl;
    }
 }




/**
* It is needed to sort the results in descending.
*/
bool myCompare (pair<float,int> i,pair<float,int> j)
{
    return (i.first>j.first);
}

/**
* The main function for answering the queries.
* We find the ID and the amount of results that we should return.
* We calculate the TF*IDF for each word and the similarity of each document to the query.
* Finally, we sort the results and print specific amount.
*/
 void InvertedIndex::executeQuery(string queryLine)
 {
    //find the document ID
    int c1=0,c2,queryID = 0,querySize = 0;
    while(queryLine[c1] != ' ')
    {
        c1++;
    }

    for(int j = 0; j < c1 ; j++)
    {
        queryID = queryID * 10;
        queryID += queryLine[j] - '0';
        queryLine[j] = ' ';
    }

    //Find the number of documents to return
    c2=c1+1;

    while(queryLine[c2] != ' ')
    {
        c2++;
    }

    for(int j = c1+1; j < c2 ; j++)
    {
        querySize = querySize * 10;
        querySize += queryLine[j] - '0';
        queryLine[j] = ' ';
    }

    queryLine = convertToLowerCase(queryLine); //string to lower case

    istringstream temp(queryLine); // Input stream class to operate on strings.

    vector<string> tokens;
    copy(istream_iterator<string>(temp),istream_iterator<string>(),back_inserter(tokens));




    unordered_map<string, float> queryVector;

    int max = 0;
    //find the word with the max frequency, so after to calculate TF of each word in query
    for(int i = 0; i < tokens.size() ; i++)
    {
        string word = tokens[i];
        std::unordered_map<string,float>::const_iterator gotWord = queryVector.find (word);
        //If the word doesnt exist in the dictionary of query
        if (gotWord == queryVector.end())
        {
            queryVector[word] = 1;

        }
        //else increase the total number of the word
        else
        {
            queryVector[word] = queryVector[word] + 1;
        }

        if(queryVector[word] > max)
        {
            max = queryVector[word];
        }
    }

    //Calculate TF*IDF of each word in query
    for(unordered_map<string, float >::iterator queryIt = queryVector.begin(); queryIt != queryVector.end(); ++queryIt)
    {
        string word = queryIt->first;
        queryIt->second = queryIt->second/max;


        std::unordered_map<string,float>::const_iterator gotIDF = IDF.find (word);

        //If the word doesnt exist in the dictionary of query
        if ( gotIDF == IDF.end())
        {
            queryIt->second = 0;
        }
        else
        {
            queryIt->second = queryIt->second * IDF[word];
        }
    }

    unordered_map<int,float> similarities;

    //Calculate the cosine similarity of each document with the query
    for(int i = 0; i < tokens.size(); i++)
    {
        string queryWord = tokens[i];

        //find the value of each word in the query
        float queryWordWeight = queryVector[queryWord];
        if(queryWordWeight == 0)
        {
            continue;
        }


        float wordIDF = IDF[queryWord];

        //Get iterator to the word's list
        std::unordered_map<string,list<DocWordData>*>::const_iterator gotWord = dictionary.find (queryWord);

        //If the word doesnt exist in the dictionary of query
        if ( gotWord == dictionary.end())
        {
            continue;
        }
        //else increase the similarity with the query for this word for each document
        else
        {
            //Foreach document that contains this word
            list<DocWordData> *documentEntries = gotWord->second;
            for(list<DocWordData>::iterator listIt = documentEntries->begin(); listIt != documentEntries->end(); ++listIt)
            {
                int docID = listIt->docID;

                std::unordered_map<int,float>::const_iterator gotDoc = similarities.find (docID);
                //If first time
                if(gotDoc == similarities.end())
                {
                    similarities[docID] = 0;
                }

                //increase similarity with this doc
                similarities[docID] += listIt->TF * wordIDF * queryWordWeight;



            }
        }
    }




    vector<pair<float,int>> results(similarities.size());
    int i=0;
    //divide by the documents magnitude
    for(unordered_map<int,float>::iterator simIt = similarities.begin(); simIt != similarities.end(); ++simIt)
    {
        simIt->second = simIt->second / docsMagnitudes[simIt->first];
        results[i].first = simIt->second;
        results[i].second = simIt->first;
        i++;
    }



    //Print top-k results that the user wants if we find more
    if(querySize < results.size())
    {
        nth_element(results.begin(),results.begin() + querySize - 1,   results.end(),myCompare);
        results.resize(querySize);
    }
    std::sort(results.begin(),results.end(),myCompare); //sort results


    printMutex.lock();
    cout <<"Top-" <<querySize << " results of query "<<queryID<< ":\""<<queryLine<<"\""<<endl;
    if(results.size() == 0){
        cout<<"No results found!!!"<<endl;
    }

    for(int j=0; j < results.size(); j++)
    {
        cout << j+1<<":  DocID:"<<results[j].second<<"    Score :"<<results[j].first<<endl;
    }
    cout<<endl;
    printMutex.unlock();


 }
