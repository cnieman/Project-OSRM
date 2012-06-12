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

#ifndef TRIVIALDEGREETWOCONTRACTOR_H_
#define TRIVIALDEGREETWOCONTRACTOR_H_

#include <climits>
#include <boost/foreach.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "../typedefs.h"

#include "../DataStructures/DynamicGraph.h"
#include "../DataStructures/EdgeDataCollection.h"
#include "../DataStructures/ExtractorStructs.h"
#include "../DataStructures/ImportEdge.h"

class TrivialDegreeTwoContractor {
    typedef DynamicGraph< _NodeBasedEdgeData > _NodeBasedDynamicGraph;
    typedef _NodeBasedDynamicGraph::InputEdge _NodeBasedEdge;
    boost::unordered_map<NodeID, bool>          _barrierNodes;
    boost::unordered_map<NodeID, bool>          _trafficLights;
    std::vector<NodeInfo>               inputNodeInfoList;
    typedef std::pair<NodeID, NodeID> RestrictionSource;
    typedef std::pair<NodeID, bool>   RestrictionTarget;
    typedef std::vector<RestrictionTarget> EmanatingRestrictionsVector;
    typedef boost::unordered_map<RestrictionSource, unsigned > RestrictionMap;
    unsigned numberOfTurnRestrictions;
public:
    TrivialDegreeTwoContractor(int nodes, std::vector<NodeBasedEdge> & inputEdges, std::vector<NodeID> & bn, std::vector<NodeID> & tl, std::vector<_Restriction> & irs, std::vector<NodeInfo> & nI);
    virtual ~TrivialDegreeTwoContractor();
    void Run();
private:
    boost::shared_ptr<_NodeBasedDynamicGraph>   _nodeBasedGraph;

};

#endif /* TRIVIALDEGREETWOCONTRACTOR_H_ */
