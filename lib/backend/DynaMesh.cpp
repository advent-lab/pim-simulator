#include "backend/DynaMesh.h"
#include "backend/Config.h"
#include <math.h> 
#include <string.h>

using namespace pimsim;

DynaMesh::DynaMesh(Config* cfg){
    #ifdef DYNA_MESH_DEBUG_OUTPUT
    printf("creating DynaMesh with width %d, height %d, wordsize_block2block %d:\n",cfg->_meshWidth, cfg->_meshHeight, cfg->_wordsize_block2block);
    #endif
    this->cfg = cfg;
    //create dynaSwitches
    for(int index=0; index<cfg->_meshHeight * cfg->_meshWidth; index++){
        switch_list.push_back(DynaSwitch(index, cfg));
    }
    //set neighbors
    for(int index=0; index<cfg->_meshHeight * cfg->_meshWidth; index++){
        setSwitchNeighbors(index);
    }

}

bool DynaMesh::receive_request(Request* req){
    
    //request should be Send Receive or Load Store or Broadcast
    assert(req->type == Request::Type::TileSend || req->type == Request::Type::TileReceive
    || req->type == Request::Type::RowLoad || req->type == Request::Type::RowStore
    || req->type == Request::Type::RowLoad_RF || req->type == Request::Type::RowStore_RF
    );
    int sourceIndex = get_source_index(req);
    int destIndex = get_dest_index(req);
    if(switch_list[sourceIndex].inject(req)){
        //inject success
        #ifdef DYNA_MESH_DEBUG_OUTPUT
        printf("DynaMesh receives a request (%s), src_tile %d, dest_tile %d\n",  
                        req->print_name(req->type).c_str(), sourceIndex, destIndex);
        #endif
        return true;
    }
    else{
        //inject failed (because source tile's receive queue L is full)
        return false;
    }
}

void DynaMesh::tick(){
    for(int i=0; i<cfg->_meshHeight * cfg->_meshWidth; i++){
        switch_list[i].tick();
    }
}


///////////////////////////Utils/////////////////////////
//get addr_list[0] index
int DynaMesh::get_addr0_index(Request* req){
    if(req->type == Request::Type::RowLoad_RF || req->type == Request::Type::RowStore_RF){
        AddrT addr;
        addr = req->addr_list[0];
        int index = addr/cfg->_num_regs_per_rf;
        return index;
    }
    else{
        AddrT addr;
        //Source
        addr = req->addr_list[0];
        int index = addr/(cfg->_ncols * cfg->_nrows * cfg->_nblocks);
        return index;
    }    
}

//get addr_list[1] index
int DynaMesh::get_addr1_index(Request* req){
    AddrT addr;
    //Source
    addr = req->addr_list[1];
    int index = addr/(cfg->_ncols * cfg->_nrows * cfg->_nblocks);
    return index;
}

int DynaMesh::get_source_index(Request* req){
    if(req->type == Request::Type::RowLoad_RF || req->type == Request::Type::RowLoad) return cfg->_dramTile;
    else return get_addr0_index(req);
}

int DynaMesh::get_dest_index(Request* req){
    if(req->type == Request::Type::RowStore_RF || req->type == Request::Type::RowStore) return cfg->_dramTile;
    else if(req->type == Request::Type::RowLoad_RF || req->type == Request::Type::RowLoad){
        return get_addr0_index(req);
    }
    else{
        return get_addr1_index(req);
    } 
}

void DynaMesh::setSwitchNeighbors(int index){
    int myRow = index/cfg->_meshWidth;
    int myCol = index%cfg->_meshWidth;
    //North neighbor
    if(myRow==0)
        switch_list[index].neighborN = NULL;
    else
        switch_list[index].neighborN = &switch_list[index - cfg->_meshWidth];

    //South neighbor
    if(myRow==cfg->_meshHeight-1)
        switch_list[index].neighborS = NULL;
    else
        switch_list[index].neighborS = &switch_list[index + cfg->_meshWidth];

    //West neighbor
    if(myCol==0)
        switch_list[index].neighborW = NULL;
    else
        switch_list[index].neighborW = &switch_list[index - 1];

    //East neighbor
    if(myRow==cfg->_meshWidth-1)
        switch_list[index].neighborE = NULL;
    else
        switch_list[index].neighborE = &switch_list[index + 1];

}