//
// Created by Ghufran on 16/10/2022.
//

#ifndef OUR_IDEA_MEMORYCONSUMPTION_H
#define OUR_IDEA_MEMORYCONSUMPTION_H

#include "../storage_engine/HBALStore.h"

class MemoryConsumption
{
public:
    MemoryConsumption(HBALStore *h)
    {
        hs = h;
    }

    size_t getGB()
    {
        /* vertex_t total_size=0;
         for(int i=0;i<hs->getMaxSourceVertex();i++)
         {
             int divSrcVertexId = getIndrVertexArrayIndex(i);
             int SrcVertexId    = getVertexArrayIndex(i, divSrcVertexId);
             if(hs->getVertexIndrList()[divSrcVertexId][SrcVertexId] != NULL)
             {
                 total_size += sizeof(hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]);
                 total_size += hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]->getBlockLen() * fixedSize * 8;
                 for(int j=hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]->getCurPos();j<(hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]->getBlockLen()*fixedSize);j++)
                 {
                     total_size += blockUpdateSize * 8 * sizeof(EdgeBlock); // check to calculate space of EdgeBlock
                     for(int k=hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]->PerVertexBlockArr()[j]->getCurIndex();k<blockUpdateSize;k++)
                     {
                         total_size += sizeof(hs->getVertexIndrList()[divSrcVertexId][SrcVertexId]->PerVertexBlockArr()[j]->getEdgeUpdate()[k]);
                         total_size += sizeof(EdgeInfo);
                     }
                 }
             }
         }*/
        return 0;
    }

private:
    HBALStore *hs;
};

#endif //OUR_IDEA_MEMORYCONSUMPTION_H
