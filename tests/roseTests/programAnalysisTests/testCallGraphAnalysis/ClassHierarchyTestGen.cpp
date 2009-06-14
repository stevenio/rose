/**
 * Generates random class hierarchies
 *
 * - first a random graph of arbitrary size is generated
 * - then loops are removed ( no loops allowed in a c++ class hierarchy)
 * - when the following situation occurs there is a compiler warning (ambiguity of a):
 *      c0 -> c1 c2
 *      c1 -> c2
 *      i.e. for an edge (0->2) there exist a longer path (0->1->2)
 *      so for every edge there is checked if such a path exist, if yes, this edge is removed
 * - the resulting graph is printed out in topological order, therefore a map exist, which maps classNr to fileNr
 *    ( because multiple classes can be printed in one file)
 */

#include <iostream>
#include <utility>
#include <sstream>
#include <cassert>
#include <fstream>
#include <set>
#include <map>
#include <cstdlib>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/random.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>

#include <boost/filesystem.hpp>


using namespace std;
using namespace boost;
using namespace boost::filesystem;


typedef adjacency_list<vecS, vecS, bidirectionalS > Graph;
//typedef adjacency_list<listS, listS, bidirectionalS > Graph;

typedef graph_traits<Graph>::vertex_iterator VertexIter;

typedef graph_traits < Graph >::edge_descriptor   Edge;
typedef graph_traits < Graph >::vertex_descriptor Vertex;



/// Maps a class/vertex to a file
map<int,int> vertexToFileId;

/// name of the folder where the output files are printed
string outputFolderName;


void printMap()
{
    cout << "vertexToFileId Map" << endl;
    for(map<int,int>::iterator i = vertexToFileId.begin(); i != vertexToFileId.end(); ++i)
        cout << i->first << " " << i->second << endl;
}

/// used for #ifndef argument - convertes everything to uppercase and replaces '.' by '_'
string StringToUpper(const string  & strToConvert)
{
   string res= strToConvert;
   for(unsigned int i=0;i<strToConvert.length(); i++)
   {
      res[i] = toupper(strToConvert[i]);

      if(strToConvert[i]=='.')
          res[i]='_';
   }

   return res;
}


/// generates a vector of count distinct class-names
void generateClassNames(int count, vector<string> & nameVec)
{
    for(int i=0; i<count; i++)
    {
        stringstream s;
        s << "c_" << i;
        nameVec.push_back(s.str());
    }
}

/// opens outputfiles in the outputFolder and returns vector of ostreams
/// prints the #ifndef FILENAME_H on the top
void generateOutStreams(const vector<string> & fileNames, vector<ostream*> & ostreamVec)
{
    ostreamVec.clear();
    ostreamVec.reserve(fileNames.size());

    for(size_t i=0; i<fileNames.size(); i++)
    {
        stringstream path;
        path<<outputFolderName << "/" << fileNames[i];
        ofstream * stream = new ofstream(path.str().c_str());
        ostreamVec.push_back(stream);

        *stream << "#ifndef " << StringToUpper(fileNames[i]) << endl;
        *stream << "#define " << StringToUpper(fileNames[i]) << endl << endl;
    }
}

/// function to close the open streams generated by generateOutStreams() , and closes #ifndef FILENAME_H
void closeOutStream(vector<ostream*> ostreamVec)
{
    for(size_t i=0; i < ostreamVec.size() ; i++)
    {
        *(ostreamVec[i]) << endl << "#endif" << endl;
        ostreamVec[i]->flush();
        delete ostreamVec[i];
    }
}



/// prints a file with an empty main function, which includes all generated headers
void printMain(const vector<string> & headerFiles)
{
    stringstream fileName;
    fileName << outputFolderName << "/" << "main.cpp";
    ofstream os (fileName.str().c_str());

    for(size_t i=0; i < headerFiles.size(); i++)
        os << "#include \"" << headerFiles[i] << "\"" << endl;

    os << endl;
    os << "int main(int argc, char**argv)" << endl;
    os << "{ " << endl << "\treturn 0; " << endl << "}" << endl << endl;
}


/// generates list of outputfile names
void generateFileNames(int count, vector<string> & nameVec)
{
    for(int i=0; i<count; i++)
    {
        stringstream s;
        s << "file_" << i << ".h";
        nameVec.push_back(s.str());
    }
}


/// checks on which files a class(i.e. vertex) depends, and adds the file to the set
void getDependendFiles ( set<int> & files, const Vertex & v, const Graph & g)
{
    Graph::out_edge_iterator j, j_end;
    for (tie(j, j_end) = out_edges(v, g); j != j_end; ++j)
    {
        int depNode = target(*j,g);

        assert(vertexToFileId.find(depNode) != vertexToFileId.end() );

        int fileId = vertexToFileId[depNode];

        files.insert(fileId);
    }

}


/// print a graph to a stream in adjacency list notation
void printGraph(ostream & os, Graph & g, const vector<string> & vertexNames)
{
    pair<VertexIter, VertexIter> vp;
    VertexIter i, i_end;
    for (tie(i, i_end) = vertices(g); i != i_end; ++i)
    {
        if (out_degree(*i, g) == 0)
            continue;

        os << vertexNames[*i] << " -> ";


        Graph::out_edge_iterator j, j_end;
        for (tie(j, j_end) = out_edges(*i, g); j != j_end; ++j)
        {
            os << vertexNames[target(*j,g)] << " ";
        }
        os << endl;
    }
}


void printGraphToFile(Graph & g,const vector<string> & vertexNames )
{
    stringstream fileName;
    fileName << outputFolderName << "/" << "dumpfile.cmp.out";
    ofstream os (fileName.str().c_str());
    printGraph(os,g,vertexNames);
}

/*
class ReachableVisitor : public default_bfs_visitor
{
    public:
        ReachableVisitor(Vertex v, int & count) : counter(count),searchVertex(v)
        {
            counter=0;
        }

        template<typename Edge, typename Graph>
        void examine_edge(Edge e,const Graph & g)
        {
            if(target(e,g)==searchVertex)
                counter++;
        }

    protected:
        int & counter;
        Vertex searchVertex;
};

int isReachable(Graph & g, Vertex v1, Vertex v2)
{
    int counter;
    ReachableVisitor vis(v2,counter);

    breadth_first_search(g, vertex(v1, g), visitor(vis));
    return counter;
}*/



/// checks if v2 is reachable from v1, when all edges in the deletedEdges-set are not available
int isReachable2 (Graph & g, Vertex v1, Vertex v2, const set<Edge> & deletedEdges)
{

    int counter=0;

    vector<bool> visited(num_vertices(g));

    list<Vertex> bfsQueue;

    bfsQueue.push_back(v1);
    visited[v1] = true;

    while(bfsQueue.size() > 0)
    {
        const Vertex curVertex = bfsQueue.front();
        bfsQueue.pop_front();


        Graph::out_edge_iterator j,j_end;
        for (tie(j, j_end) = out_edges(curVertex, g); j != j_end; ++j)
        {
            if( deletedEdges.find(*j) != deletedEdges.end())
                continue;

            Vertex targetVertex = target(*j,g);
            if(! visited[targetVertex])
                bfsQueue.push_back(targetVertex);

            visited[targetVertex]=true;

            if(targetVertex == v2)
                counter++;
        }
    }

    return counter;
}



/// Visitor which record all back-edges (i.e. edges which form loops)
class BackEdgeRecorder: public default_dfs_visitor
{
    public:
        BackEdgeRecorder(vector<Edge> & out) : edgeVec(out) {}

        template<typename Edge, typename Graph>
        void back_edge(Edge e, const Graph &g)     {  edgeVec.push_back(e);  }

    private:
        vector<Edge> & edgeVec;
};

void removeLoops(Graph & g)
{
    vector<Edge> loop_edges;

    BackEdgeRecorder f ( loop_edges );
    depth_first_search(g,visitor(f));

    for(unsigned int i=0; i<loop_edges.size(); i++)
        remove_edge(loop_edges[i],g);

}





int fileIdFromClassId(int classId, int numClasses, int numFiles)
{
    int classesPerFile = numClasses / numFiles;

    return classId / classesPerFile;
}



void writeClassToFile(ostream & os, const vector<string> & classNames, Vertex v, const Graph & g)
{
    os << "class " << classNames[v] << " ";

    if (out_degree(v, g) > 0)
        os << " : ";

    Graph::out_edge_iterator j, j_end;
    for (tie(j, j_end) = out_edges(v, g); j != j_end; ++j)
    {
        if(j != out_edges(v,g).first)
            os << ", ";

       os << "public " << classNames[target(*j,g)];
    }

    os << endl;
    os << "{};" <<endl << endl;
}


void removeAmbiguityWarning2(Graph & g)
{
    set<Edge> deletedEdges;

    Graph::edge_iterator ei,ei_end;
    for (tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
    {
        int counter = isReachable2(g,source(*ei,g), target(*ei,g),deletedEdges );
        //cout << "Testing " << source(*ei,g) <<"/" << target(*ei,g) << " counter:" << counter << endl;
        if(counter > 1)
        {
            //cout << "Delete (" <<  source(*ei,g) <<"/" << target(*ei,g) << ") " << endl;
            deletedEdges.insert(*ei);
        }
    }

    for(set<Edge>::iterator i = deletedEdges.begin(); i != deletedEdges.end(); ++i)
        remove_edge(*i,g);
}

/*
void removeAmbiguityWarning(Graph & g)
{

    Graph::edge_iterator ei,ei_end;
    for (tie(ei, ei_end) = edges(g); ei != ei_end; ++ei)
    {
        int counter = isReachable(g,source(*ei,g), target(*ei,g) );
        //cout << "Testing " << source(*ei,g) <<"/" << target(*ei,g) << " counter:" << counter << endl;
        if(counter > 1)
        {
            //cout << "(" <<  source(*ei,g) <<"/" << target(*ei,g) << ") ";
            remove_edge(*ei,g);
            removeAmbiguityWarning(g);//and start again for further edges, not very efficient (use coloring if too slow)
            return;
        }
    }
}*/



int main(int argc, char**argv)
{
    size_t classCount = 50;
    size_t fileCount = 50;

    //arg0 progname
    //arg1 classCount
    //arg2 fileCount
    //arg3 egdeRate
    //arg4 folderName

    if(argc!=5)
    {
        cerr << "Class Hierarchy Testfile Generator:" << endl;
        cerr << "Usage:" << endl;
        cerr << "./classHierarchyTestGen <classCount> <fileCount> <edgeRate> <outputFolderName>" << endl;
        return 1;
    }

    classCount = atoi (argv[1]);
    fileCount = atoi(argv[2]);
    float edgeRate = atof(argv[3]);
    outputFolderName = string(argv[4]);

    if(classCount<fileCount)
    {
        cerr << "More files than classes!";
        return 1;
    }

    create_directory(outputFolderName);

    vector<string> classNames;
    generateClassNames(classCount, classNames);

    Graph g;

    // Generate random graph
    mt19937 rng;
    rng.seed((unsigned int)time(NULL));

    rng.seed(32);
    generate_random_graph(g, classCount,edgeRate*classCount, rng, false, false);


    // remove cycles
    removeLoops(g);
    removeAmbiguityWarning2(g);

    // topological sort
    list<Vertex> vlist;
    topological_sort(g, back_inserter(vlist));

    assert(fileCount <= classCount);

    vector<ostream*> ostreamVec;
    vector<string> fileNameVec;
    generateFileNames(fileCount,fileNameVec);
    generateOutStreams(fileNameVec,  ostreamVec);



    int classesPerFile = classCount / fileCount;
    if(classCount % fileCount !=0)
        classesPerFile++;


    //write a map, which vertex/class comes into which file
    int vCount=0;
    for(list<Vertex>::iterator i = vlist.begin(); i != vlist.end(); ++i)
    {
        int fileId = vCount / classesPerFile;
        vertexToFileId.insert(make_pair<int,int>(*i, fileId ));
        vCount++;
    }



    list<Vertex>::iterator vIter =vlist.begin();
    for(size_t file=0; file < fileCount; file++)
    {
        stringstream includePart;
        stringstream contentPart;

        set<int> depFiles;
        for(int c=0; c < classesPerFile; c++)
        {
            getDependendFiles ( depFiles, *vIter,g);

            writeClassToFile( contentPart,classNames,*vIter,g);

            vIter++;
            if(vIter==vlist.end())
                break;
        }

        //remove dependency to current file (otherwise the own include would be included)
        depFiles.erase(file);
        for(set<int>::iterator i= depFiles.begin(); i != depFiles.end(); ++i)
            includePart << "#include \"" << fileNameVec[*i] << "\"" << endl;

        *(ostreamVec[file]) << includePart.str() << endl << contentPart.str() << endl;

    }


    printGraphToFile(g,classNames) ;

    closeOutStream(ostreamVec);

    printMain(fileNameVec);

    return 0;
}
