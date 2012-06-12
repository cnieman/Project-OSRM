/*
 open source routing machine
 Copyright (C) Dennis Luxen, others 2010

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU AFFERO General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU Affero General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 or see http://www.gnu.org/licenses/agpl.txt.
 */

#include "TrivialDegreeTwoContractor.h"

TrivialDegreeTwoContractor::TrivialDegreeTwoContractor(int nodes, std::vector<NodeBasedEdge> & inputEdges, std::vector<NodeID> & bn, std::vector<NodeID> & tl, std::vector<_Restriction> & irs, std::vector<NodeInfo> & nI) : inputNodeInfoList(nI), numberOfTurnRestrictions(irs.size()) {
    BOOST_FOREACH(_Restriction & restriction, irs) {
        std::pair<NodeID, NodeID> restrictionSource = std::make_pair(restriction.fromNode, restriction.viaNode);
        unsigned index;
        RestrictionMap::iterator restrIter = _restrictionMap.find(restrictionSource);
        if(restrIter == _restrictionMap.end()) {
            index = _restrictionBucketVector.size();
            _restrictionBucketVector.resize(index+1);
            _restrictionMap[restrictionSource] = index;
        } else {
            index = restrIter->second;
            //Map already contains an is_only_*-restriction
            if(_restrictionBucketVector.at(index).begin()->second)
                continue;
            else if(restriction.flags.isOnly){
                //We are going to insert an is_only_*-restriction. There can be only one.
                _restrictionBucketVector.at(index).clear();
            }
        }

        _restrictionBucketVector.at(index).push_back(std::make_pair(restriction.toNode, restriction.flags.isOnly));
    }

    std::string usedSpeedProfile( speedProfile.get_child("").begin()->first );
    BOOST_FOREACH(boost::property_tree::ptree::value_type &v, speedProfile.get_child(usedSpeedProfile)) {
        if("trafficSignalPenalty" ==  v.first) {
            std::string value = v.second.get<std::string>("");
            try {
                trafficSignalPenalty = 10*boost::lexical_cast<int>(v.second.get<std::string>(""));
            } catch(boost::bad_lexical_cast &) {
                trafficSignalPenalty = 0;
            }
        }
        if("uturnPenalty" ==  v.first) {
            std::string value = v.second.get<std::string>("");
            try {
                uturnPenalty = 10*boost::lexical_cast<int>(v.second.get<std::string>(""));
            } catch(boost::bad_lexical_cast &) {
                uturnPenalty = 0;
            }
        }
        if("takeMinimumOfSpeeds" ==  v.first) {
            std::string value = v.second.get<std::string>("");
            takeMinimumOfSpeeds = (v.second.get<std::string>("") == "yes");
        }
    }

//    INFO("traffic signal penalty: " << trafficSignalPenalty << ", U-Turn penalty: " << uturnPenalty << ", takeMinimumOfSpeeds=" << (takeMinimumOfSpeeds ? "yes" : "no"));

    BOOST_FOREACH(NodeID id, bn) {
        _barrierNodes[id] = true;
    }
    BOOST_FOREACH(NodeID id, tl) {
        _trafficLights[id] = true;
    }

    DeallocatingVector< _NodeBasedEdge > edges;
//    edges.reserve( 2 * inputEdges.size() );
    for ( std::vector< NodeBasedEdge >::const_iterator i = inputEdges.begin(); i != inputEdges.end(); ++i ) {

        _NodeBasedEdge edge;
        if(!i->isForward()) {
            edge.source = i->target();
            edge.target = i->source();
            edge.data.backward = i->isForward();
            edge.data.forward = i->isBackward();
       } else {
            edge.source = i->source();
            edge.target = i->target();
            edge.data.forward = i->isForward();
            edge.data.backward = i->isBackward();
        }
        if(edge.source == edge.target)
            continue;

        edge.data.distance = (std::max)((int)i->weight(), 1 );
        assert( edge.data.distance > 0 );
        edge.data.shortcut = false;
        edge.data.roundabout = i->isRoundabout();
        edge.data.ignoreInGrid = i->ignoreInGrid();
        edge.data.nameID = i->name();
        edge.data.type = i->type();
        edge.data.isAccessRestricted = i->isAccessRestricted();
//        edge.data.edgeBasedNodeID = edges.size();
        edges.push_back( edge );
        if( edge.data.backward ) {
            std::swap( edge.source, edge.target );
            edge.data.forward = i->isBackward();
            edge.data.backward = i->isForward();
//            edge.data.edgeBasedNodeID = edges.size();
            edges.push_back( edge );
        }
    }
    std::vector<NodeBasedEdge>().swap(inputEdges);
    //std::vector<_NodeBasedEdge>(edges).swap(edges);
    std::sort( edges.begin(), edges.end() );

    _nodeBasedGraph.reset(new _NodeBasedDynamicGraph( nodes, edges ));

TrivialDegreeTwoContractor::~TrivialDegreeTwoContractor() {
    // TODO Auto-generated destructor stub
}

TrivialDegreeTwoContractor::Run() {
    //loop over graph and remove all trivially contractable edges.
    for(_NodeBasedDynamicGraph::NodeIterator u = 0; u < _nodeBasedGraph->GetNumberOfNodes(); ++u ) {
        for(_NodeBasedDynamicGraph::EdgeIterator e1 = _nodeBasedGraph->BeginEdges(u); e1 < _nodeBasedGraph->EndEdges(u); ++e1) {
            _NodeBasedDynamicGraph::NodeIterator v = _nodeBasedGraph->GetTarget(e1);
            const _NodeBasedDynamicGraph::EdgeData edgeData1 = _nodeBasedGraph->GetEdgeData(e1);
            for(_NodeBasedDynamicGraph::EdgeIterator e2 = _nodeBasedGraph->BeginEdges(v); e2 < _nodeBasedGraph->EndEdges(v); ++e2) {

                const _NodeBasedDynamicGraph::EdgeData edgeData2 = _nodeBasedGraph->GetEdgeData(e2);
                _NodeBasedDynamicGraph::NodeIterator w = _nodeBasedGraph->GetTarget(e2);

                double testAngle = GetAngleBetweenTwoEdges(inputNodeInfoList[u], inputNodeInfoList[v], inputNodeInfoList[w]);
                //TODO: !!Merken: Graph enthält auch Rückwärtskanten!!

                if((testAngle > 179. && testAngle < 181.) && _nodeBasedGraph->GetOutDegree(v) == 2 && edgeData1.nameID == edgeData2.nameID &&
                        edgeData1.backward == edgeData2.backward && edgeData1.isAccessRestricted == edgeData2.isAccessRestricted) {

                    //TODO: Check, ob hier nicht sowieso nicht abgebogen werden darf wegen Abbiegeverbot oder bollards.
                    if(turnIsProhibited(u,v,w))
                        continue;

                    //remove edges (u,v), (v,w)
                    edgesToRemove.push_back(std::make_pair(u,v));
                    edgesToRemove.push_back(std::make_pair(v,w));

                    //add edge (v,w) with combined distance
                    _NodeBasedEdge newEdge;
                    newEdge.source = u; newEdge.target = w;
                    newEdge.data = edgeData1; newEdge.data.distance += edgeData2.distance;
                    ++triviallySkippedEdges;
                    edgesToInsert.push_back(newEdge);
                }
            }
        }
    }

    Percent percent(edgesToInsert.size());

    INFO("Edges to insert: " << edgesToInsert.size() << ", edgesToRemove: " << edgesToRemove.size());

    INFO("before trivial contraction: " << _nodeBasedGraph->GetNumberOfEdges());
    //replace contractable edges
    for(unsigned i = 0; i < edgesToInsert.size(); ++i) {
        _nodeBasedGraph->DeleteEdge(edgesToRemove[2*i].first, edgesToRemove[2*i].second);
        _nodeBasedGraph->DeleteEdge(edgesToRemove[2*i+1].first, edgesToRemove[2*i+1].second);

//        INFO("->delete edge (" << edgesToRemove[2*i].first << "," << edgesToRemove[2*i].second << ")");
//        INFO("->delete edge (" << edgesToRemove[2*i+1].first << "," << edgesToRemove[2*i+1].second << ")");

        _NodeBasedEdge edge = edgesToInsert[i];
//        INFO("  inserted edge (" << edge.source << "," << edge.target << ") " << i << "/" << edgesToInsert.size());
        assert(edge.source < _nodeBasedGraph->GetNumberOfNodes());
        assert(edge.target < _nodeBasedGraph->GetNumberOfNodes());
        _nodeBasedGraph->InsertEdge(edge.source, edge.target, edge.data);
        percent.printIncrement();
    }

    INFO("After trivial contraction: " << _nodeBasedGraph->GetNumberOfEdges());

    unsigned edgeBasedNodeIDCounter = 0;
    std::vector<NodeID> renumberingTable(_nodeBasedGraph->GetNumberOfNodes(), UINT_MAX);
    unsigned renumberedNodes = 0;
    //loop over graph and generate contigous(!) edge-expanded node ids and new node-based node ids
    for(_NodeBasedDynamicGraph::NodeIterator u = 0; u < _nodeBasedGraph->GetNumberOfNodes(); ++u ) {
        _NodeBasedDynamicGraph::EdgeIterator edgeID = _nodeBasedGraph->BeginEdges(u);
        _NodeBasedDynamicGraph::EdgeIterator endEdgeID = _nodeBasedGraph->EndEdges(u);
        if(endEdgeID-edgeID > 0) {
            for( ; edgeID < endEdgeID; ++edgeID) {
                _NodeBasedDynamicGraph::NodeIterator v = _nodeBasedGraph->GetTarget(edgeID);
                if(UINT_MAX == v)
                    continue;
                if(UINT_MAX == renumberingTable[u])          // Do we see this node for the first time
                    renumberingTable[u] = renumberedNodes++; // assign and increment after that
                if(UINT_MAX == renumberingTable[v])          // Do we see this node for the first time
                    renumberingTable[v] = renumberedNodes++; // assign and increment after that

               _NodeBasedDynamicGraph::EdgeData & edgeData = _nodeBasedGraph->GetEdgeData(edgeID);
                edgeData.edgeBasedNodeID = edgeBasedNodeIDCounter++;
                if(_nodeBasedGraph->GetEdgeData(edgeID).type != SHRT_MAX) {
                    assert(UINT_MAX != edgeID);
                    assert(UINT_MAX != u);
                    assert(UINT_MAX != v);
                    InsertEdgeBasedNode(edgeID, u, v);
                }
            }
        }
    }

    INFO("Generating Edge-based Nodes, removing " << renumberingTable.size() - renumberedNodes << " node-based nodes, generated " << edgeBasedNodeIDCounter << " edge-expanded nodes");

    //TODO: fetch all edges and rebuild node-based graph

    //loop over all edges and generate new set of nodes.
//    for(_NodeBasedDynamicGraph::NodeIterator u = 0; u < _nodeBasedGraph->GetNumberOfNodes(); ++u ) {
//        for(_NodeBasedDynamicGraph::EdgeIterator e1 = _nodeBasedGraph->BeginEdges(u); e1 < _nodeBasedGraph->EndEdges(u); ++e1) {
//            _NodeBasedDynamicGraph::NodeIterator v = _nodeBasedGraph->GetTarget(e1);
//            if(_nodeBasedGraph->GetEdgeData(e1).type != SHRT_MAX) {
//                assert(e1 != UINT_MAX);
//                assert(u != UINT_MAX);
//                assert(v != UINT_MAX);
//                InsertEdgeBasedNode(e1, renumberingTable[u], renumberingTable[v]);
//            }
//        }
//    }

    //TODO: renumber forbidden turns

    //TODO: renumber bollards and such

    //TODO: renumber traffic lights

}
