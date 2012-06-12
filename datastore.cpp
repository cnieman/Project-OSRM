#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <string>
#include <cstdlib>
#include <signal.h>
#include <vector>
#include <istream>

#include "typedefs.h"
#include "DataStructures/ExtractorStructs.h"
#include "DataStructures/Percent.h"
#include "DataStructures/QueryEdge.h"
#include "DataStructures/ShMemStaticGraph.h"
#include "Server/ServerConfiguration.h"

typedef ShMemStaticGraph<QueryEdge::EdgeData> QueryGraph;
typedef QueryGraph::_StrEdge QueryGraphEdge;
typedef QueryGraph::_StrNode QueryGraphNode;
typedef boost::interprocess::allocator<_Coordinate, boost::interprocess::managed_shared_memory::segment_manager> ShmemCoordinateAllocator;
typedef boost::interprocess::allocator<char, boost::interprocess::managed_shared_memory::segment_manager> ShmemCharAllocator;
typedef boost::interprocess::allocator<unsigned, boost::interprocess::managed_shared_memory::segment_manager> ShmemIntAllocator;
typedef boost::interprocess::allocator<OriginalEdgeData, boost::interprocess::managed_shared_memory::segment_manager> ShmemOrigEdgeDataAllocator;


//Define an STL compatible allocator of ints that allocates from the managed_shared_memory.
//This allocator will allow placing containers in the segment

//Alias a vector that uses the previous STL-like allocator so that allocates
//its values from the segment
typedef std::vector<QueryGraphNode, QueryGraph::ShmemNodeAllocator> NodeList;
typedef std::vector<QueryGraphEdge, QueryGraph::ShmemEdgeAllocator> EdgeList;
typedef std::vector<_Coordinate, ShmemCoordinateAllocator> CoordinateList;
typedef std::vector<char, ShmemCharAllocator> NameVectorCharList;
typedef std::vector<int, ShmemIntAllocator> StringIndicesList;
typedef std::vector<OriginalEdgeData, ShmemOrigEdgeDataAllocator> OrigEdgeDataList;

//Main function. For parent process argc == 1, for child process argc == 2
int main(int argc, char *argv[]) {

    if(argc == 1){ //Parent process
        ServerConfiguration serverConfig("server.ini");
        try {
            int sig = 0;
            sigset_t new_mask;
            sigset_t old_mask;
            sigfillset(&new_mask);
            pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);

            //Remove shared memory on construction and destruction
            struct SharedMemoryRemover {
                SharedMemoryRemover() {
                    INFO("Removing data c'tor");
                    boost::interprocess::shared_memory_object::remove("NodeList");
                    boost::interprocess::shared_memory_object::remove("EdgeList");
                    boost::interprocess::shared_memory_object::remove("CoordinateList");
                    boost::interprocess::shared_memory_object::remove("OrigEdgeDataList");
                    boost::interprocess::shared_memory_object::remove("SharedQueryData");
                }
                ~SharedMemoryRemover(){
                    INFO("Removing data d'tor");
                    boost::interprocess::shared_memory_object::remove("NodeList");
                    boost::interprocess::shared_memory_object::remove("EdgeList");
                    boost::interprocess::shared_memory_object::remove("CoordinateList");
                    boost::interprocess::shared_memory_object::remove("OrigEdgeDataList");
                    boost::interprocess::shared_memory_object::remove("SharedQueryData");
                }
            } remover;

            unsigned numberOfNodes = 0;
            unsigned numberOfEdges = 0;
            std::ifstream hsgrGraphInputStream(serverConfig.GetParameter("hsgrData").c_str(), std::ios::binary);
            {
                unsigned checkSum = 0;
                hsgrGraphInputStream.read((char*) &checkSum, sizeof(unsigned));
                hsgrGraphInputStream.read((char*) & numberOfNodes, sizeof(unsigned));
                INFO("Loading graph data");
                INFO("Data checksum is " << checkSum);
                //Create a new segment with given name and size
                boost::interprocess::managed_shared_memory segment(boost::interprocess::create_only, "SharedQueryData", numberOfNodes*2*sizeof(QueryGraphNode));
                //Initialize shared memory STL-compatible allocator
                const QueryGraph::ShmemNodeAllocator node_alloc_inst (segment.get_segment_manager());

                //Construct a vector named "MyVector" in shared memory with argument alloc_inst
                NodeList *myvector = segment.construct<NodeList>("NodeList")(node_alloc_inst);
                myvector->reserve(numberOfNodes);
                QueryGraphNode currentNode;
                for(unsigned nodeCounter = 0; nodeCounter < numberOfNodes; ++nodeCounter ) {
                    hsgrGraphInputStream.read((char*) &currentNode, sizeof(QueryGraphNode));
                    myvector->push_back(currentNode);
                }
                hsgrGraphInputStream.read((char*) &numberOfEdges, sizeof(unsigned));
            }
            {
                boost::interprocess::managed_shared_memory::grow("SharedQueryData", numberOfEdges*sizeof(QueryGraphEdge)*2);
                boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "SharedQueryData");

                //Initialize shared memory STL-compatible allocator
                const QueryGraph::ShmemEdgeAllocator edge_alloc_inst (segment.get_segment_manager());

                EdgeList *edgeList = segment.construct<EdgeList>("EdgeList")(edge_alloc_inst);
                edgeList->reserve(numberOfEdges);
                QueryGraphEdge currentEdge;
                for(unsigned edgeCounter = 0; edgeCounter < numberOfEdges; ++edgeCounter) {
                    hsgrGraphInputStream.read((char*) &currentEdge, sizeof(QueryGraphEdge));
                    edgeList->push_back(currentEdge);
                }
                hsgrGraphInputStream.close();
            }
            {
                INFO("Loading coordinates vector");
                boost::interprocess::managed_shared_memory::grow("SharedQueryData", numberOfNodes*sizeof(_Coordinate)*2);
                boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "SharedQueryData");

                ShmemCoordinateAllocator coordinate_alloc_inst(segment.get_segment_manager());
                CoordinateList *coordinateList = segment.construct<CoordinateList>("CoordinateList")(coordinate_alloc_inst);
                coordinateList->reserve(numberOfNodes);

                std::ifstream coordinateVectorInputStream(serverConfig.GetParameter("nodesData").c_str(), std::ios::binary);

                for(unsigned i = 0; i < numberOfNodes; ++i) {
                    NodeInfo b;
                    coordinateVectorInputStream.read((char *)&b, sizeof(b));
                    coordinateList->push_back(_Coordinate(b.lat, b.lon));
                }
                coordinateVectorInputStream.close();
            }
            {
                //deserialize street name list
                INFO("Loading names index");
                std::ifstream namesInStream(serverConfig.GetParameter("namesData").c_str(), ios::binary);
                unsigned numberOfStrings;

                long begin,end;
                begin = namesInStream.tellg();
                namesInStream.seekg (0, std::ios::end);
                end = namesInStream.tellg();
                namesInStream.seekg( 0, std::ios::beg);

                namesInStream.read((char *)&numberOfStrings, sizeof(unsigned));

                boost::interprocess::managed_shared_memory::grow("SharedQueryData", (end-begin)*sizeof(char)*2 + numberOfStrings*sizeof(int)*2);
                boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "SharedQueryData");

                ShmemCharAllocator char_alloc_inst(segment.get_segment_manager());
                ShmemIntAllocator int_alloc_inst(segment.get_segment_manager());

                NameVectorCharList *nameVectorCharList = segment.construct<NameVectorCharList>("NameVectorCharList")(char_alloc_inst);
                StringIndicesList  *stringIndicesList  = segment.construct<StringIndicesList >("StringIndicesList" )(char_alloc_inst);

                nameVectorCharList->reserve((end-begin));
                stringIndicesList->reserve(numberOfStrings+1);
                stringIndicesList->push_back(0);
                char buf[1024];
                int sizecount = 0;
                for(unsigned i = 0; i < numberOfStrings; ++i) {
                    unsigned sizeOfString = 0;
                    namesInStream.read((char *)&sizeOfString, sizeof(unsigned));
                    sizecount += sizeOfString;
                    stringIndicesList->push_back(sizecount);
                    namesInStream.read(buf, sizeOfString);
                    for(unsigned j = 0; j < sizeOfString; ++j)
                        nameVectorCharList->push_back(buf[j]);
                }
                namesInStream.close();
            }

            {//load orig edge date
                INFO("Loading original edge data");
                std::ifstream edgesInStream(serverConfig.GetParameter("edgesData").c_str(), ios::binary);
                unsigned numberOfOrigEdges(0);
                edgesInStream.read((char*)&numberOfOrigEdges, sizeof(unsigned));

                boost::interprocess::managed_shared_memory::grow("SharedQueryData", numberOfOrigEdges*sizeof(OriginalEdgeData)*2);
                boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "SharedQueryData");

                ShmemOrigEdgeDataAllocator orig_alloc_inst(segment.get_segment_manager());

                OrigEdgeDataList *origEdgeData = segment.construct<OrigEdgeDataList>("OrigEdgeDataList")(orig_alloc_inst);

                origEdgeData->reserve(numberOfOrigEdges);
                for(unsigned i = 0; i < numberOfOrigEdges; ++i) {
                    OriginalEdgeData data;
                    edgesInStream.read((char*)&data, sizeof(OriginalEdgeData));
                    origEdgeData->push_back(data);
                }
                edgesInStream.close();
                DEBUG("Loaded " << numberOfOrigEdges << " orig edges");
                DEBUG("Opening NN indices");
            }

            INFO("All query data structures loaded");
            sigset_t wait_mask;
            pthread_sigmask(SIG_SETMASK, &old_mask, 0);
            sigemptyset(&wait_mask);
            sigaddset(&wait_mask, SIGINT);
            sigaddset(&wait_mask, SIGQUIT);
            sigaddset(&wait_mask, SIGTERM);
            pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
            INFO("Data store running");
            sigwait(&wait_mask, &sig);
        } catch (boost::interprocess::bad_alloc &ex) {
            std::cerr << ex.what() << std::endl;
            return -1;
        }
        INFO("Shutting down")
    }
    else{ //Child process
        try {
            boost::interprocess::managed_shared_memory segment(boost::interprocess::open_only, "SharedQueryData");
            QueryGraph* _graph = (new QueryGraph("SharedQueryData", "NodeList", "EdgeList") );
            INFO("Graph has " << _graph->GetNumberOfNodes() << " nodes and " << _graph->GetNumberOfEdges() << " edges");
            Percent p(_graph->GetNumberOfNodes());
#ifndef DNDEBUG
                    for(unsigned u = 0; u < _graph->GetNumberOfNodes(); ++u) {
                        for(unsigned eid = _graph->BeginEdges(u); eid < _graph->EndEdges(u); ++eid) {
                            unsigned v = _graph->GetTarget(eid);
                            QueryEdge::EdgeData & data = _graph->GetEdgeData(eid);
                            if(data.shortcut) {
                                unsigned eid2 = _graph->FindEdgeInEitherDirection(u, data.id);
                                if(eid2 == UINT_MAX) {
                                    DEBUG("cannot find first segment of edge (" << u << "," << data.id << "," << v << ")");
                                    data.shortcut = false;
                                }
                                eid2 = _graph->FindEdgeInEitherDirection(data.id, v);
                                if(eid2 == UINT_MAX) {
                                    DEBUG("cannot find second segment of edge (" << u << "," << data.id << "," << v << ")");
                                    data.shortcut = false;
                                }
                            }
                        }
                        p.printIncrement();
                    }
#endif
            boost::interprocess::offset_ptr<std::vector< _Coordinate, ShmemCoordinateAllocator > > coordinateList = segment.find<std::vector< _Coordinate, ShmemCoordinateAllocator > >("CoordinateList").first;
            NameVectorCharList *nameVectorCharList = segment.find<NameVectorCharList>("NameVectorCharList").first;
            StringIndicesList  *stringIndicesList  = segment.find<StringIndicesList >("StringIndicesList" ).first;
            INFO("length of string 1: " << stringIndicesList->at(2) - stringIndicesList->at(1));
            for(unsigned i = stringIndicesList->at(1); i < stringIndicesList->at(2); ++i)
                std::cout << nameVectorCharList->at(i);
            std::cout << std::endl;
            INFO("length of string 2: " << stringIndicesList->at(3) - stringIndicesList->at(2));
            for(unsigned i = stringIndicesList->at(2); i < stringIndicesList->at(3); ++i)
                std::cout << nameVectorCharList->at(i);
            std::cout << std::endl;
            INFO("length of string 3: " << stringIndicesList->at(4) - stringIndicesList->at(3));
            for(unsigned i = stringIndicesList->at(3); i < stringIndicesList->at(4); ++i)
                std::cout << nameVectorCharList->at(i);
            std::cout << std::endl;
            INFO("length of string 4: " << stringIndicesList->at(5) - stringIndicesList->at(4));
            for(unsigned i = stringIndicesList->at(4); i < stringIndicesList->at(5); ++i)
                std::cout << nameVectorCharList->at(i);
            std::cout << std::endl;

            OrigEdgeDataList *origEdgeDataList = segment.find<OrigEdgeDataList>("OrigEdgeDataList").first;

            INFO("Name of orig edges 0-4:");
            for(unsigned i = 0; i < 4; ++i) {
                unsigned indexOfOrigEdge = origEdgeDataList->at(i).nameID;
                INFO("NameID of orig edge " << i << "=" << indexOfOrigEdge);
                for(unsigned j = stringIndicesList->at(indexOfOrigEdge); j < stringIndicesList->at(indexOfOrigEdge+1); ++j)
                    std::cout << nameVectorCharList->at(j);
                std::cout << std::endl;
            }


            delete _graph;
        } catch(...) {
            ERR("Could not open graph");
        }
    }
    return 0;
};
