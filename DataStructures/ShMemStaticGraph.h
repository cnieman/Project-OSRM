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

#ifndef SHMEM_ShMemStaticGraph_H_INCLUDED
#define SHMEM_ShMemStaticGraph_H_INCLUDED

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <algorithm>

#include "../typedefs.h"
#include "ImportEdge.h"

template< typename EdgeDataT >
class ShMemStaticGraph {
public:
    typedef NodeID NodeIterator;
    typedef NodeID EdgeIterator;
    typedef EdgeDataT EdgeData;
    class InputEdge {
    public:
        EdgeDataT data;
        NodeIterator source;
        NodeIterator target;
        bool operator<( const InputEdge& right ) const {
            if ( source != right.source )
                return source < right.source;
            return target < right.target;
        }
    };

    struct _StrNode {
        //index of the first edge
        EdgeIterator firstEdge;
    };

    struct _StrEdge {
        NodeID target;
        EdgeDataT data;
    };

    typedef boost::interprocess::allocator<_StrNode, boost::interprocess::managed_shared_memory::segment_manager> ShmemNodeAllocator;
    typedef boost::interprocess::allocator<_StrEdge, boost::interprocess::managed_shared_memory::segment_manager> ShmemEdgeAllocator;

    ShMemStaticGraph( const char * SHARED_NAMESPACE, const char * SHARED_NODELIST, const char * SHARED_EDGELIST ) :
         segment(boost::interprocess::open_only, SHARED_NAMESPACE){

        //Find the vector using the c-string name
        boost::interprocess::offset_ptr<std::vector< _StrNode, ShmemNodeAllocator > > nodeList = segment.find<std::vector< _StrNode, ShmemNodeAllocator > >(SHARED_NODELIST).first;
        if(0 == nodeList) {
            //Usually, a file not found exception is thrown before we get here.
            ERR( "Could not locate node list in memory");
        }
        boost::interprocess::offset_ptr<std::vector< _StrEdge, ShmemEdgeAllocator > > edgeList = segment.find<std::vector< _StrEdge, ShmemEdgeAllocator > >(SHARED_EDGELIST).first;
        if(0 == edgeList) {
            //Usually, a file not found exception is thrown before we get here.
            ERR( "Could not locate edge list in memory");
        }

        _numNodes = nodeList->size();
        _numEdges = edgeList->size();

        _nodes = nodeList;
        _edges = edgeList;
    }

    unsigned GetNumberOfNodes() const {
        return _numNodes;
    }

    unsigned GetNumberOfEdges() const {
        return _numEdges;
    }

    unsigned GetOutDegree( const NodeIterator &n ) const {
        return BeginEdges(n)-EndEdges(n) - 1;
    }

    inline NodeIterator GetTarget( const EdgeIterator &e ) const {
        return NodeIterator( (*_edges)[e].target );
    }

    inline EdgeDataT &GetEdgeData( const EdgeIterator &e ) {
        return (*_edges)[e].data;
    }

    inline const EdgeDataT &GetEdgeData( const EdgeIterator &e ) const {
        return (*_edges)[e].data;
    }

    EdgeIterator BeginEdges( const NodeIterator &n ) const {
        return EdgeIterator( (*_nodes)[n].firstEdge );
    }

    EdgeIterator EndEdges( const NodeIterator &n ) const {
        return EdgeIterator( (*_nodes)[n+1].firstEdge );
    }

    //searches for a specific edge
    EdgeIterator FindEdge( const NodeIterator &from, const NodeIterator &to ) const {
        EdgeIterator smallestEdge = SPECIAL_EDGEID;
        EdgeWeight smallestWeight = UINT_MAX;
        for ( EdgeIterator edge = BeginEdges( from ); edge < EndEdges(from); edge++ ) {
            const NodeID target = GetTarget(edge);
            const EdgeWeight weight = GetEdgeData(edge).distance;
            if(target == to && weight < smallestWeight) {
                smallestEdge = edge; smallestWeight = weight;
            }
        }
        return smallestEdge;
    }

    EdgeIterator FindEdgeInEitherDirection( const NodeIterator &from, const NodeIterator &to ) const {
        EdgeIterator tmp =  FindEdge( from, to );
        return (UINT_MAX != tmp ? tmp : FindEdge( to, from ));
    }

    EdgeIterator FindEdgeIndicateIfReverse( const NodeIterator &from, const NodeIterator &to, bool & result ) const {
        EdgeIterator tmp =  FindEdge( from, to );
        if(UINT_MAX == tmp) {
            tmp =  FindEdge( to, from );
            if(UINT_MAX != tmp)
                result = true;
        }
        return tmp;
    }

private:
    NodeIterator _numNodes;
    EdgeIterator _numEdges;

    boost::interprocess::offset_ptr<std::vector< _StrNode, ShmemNodeAllocator > > _nodes;
    boost::interprocess::offset_ptr<std::vector< _StrEdge, ShmemEdgeAllocator > > _edges;
    boost::interprocess::managed_shared_memory segment;
};

#endif // SHMEM_ShMemStaticGraph_H_INCLUDED
