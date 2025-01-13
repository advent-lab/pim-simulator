
// tvm target: c -keys=cpu -link-params=0
#define TVM_EXPORTS
#include <cstdint>

#include "backend/System.h"

/////////////////////////////////////////////////////////////
// Simple program to perform a matrix-vector mul 
/////////////////////////////////////////////////////////////

int32_t gemv_tiled(System* sys, std::string param)
{
    //int matrix_row = 32768*1;//32768 * X
    //int matrix_col = 4096 *1;// 4096 * Y
    int matrix_row = 120*512;
    int matrix_col = 256*8;
    Config* cfg = sys->_config;
    int use_tiles = cfg->_ntiles_used;
    PrecisionT::Precision input_precision = PrecisionT::INT8;
    PrecisionT::Precision multiply_precision = PrecisionT::INT16;
    PrecisionT::Precision accumulate_precision = PrecisionT::INT32;
    
    std::vector<Request> requests;
    Request *request;
    for(int tile=0; tile<use_tiles; tile++){
        request = new Request(Request::Type::RowLoad_RF);
        request->addOperand(cfg->_num_regs_per_rf * tile, 0, input_precision); //dst
        request->addOperand(sys->DRAM_ADDR, 0, input_precision); //src
        requests.push_back(*request);   
    } 

    int numColPerCore = cfg->_ncols*cfg->_nblocks/matrix_row;
    if(numColPerCore==0)numColPerCore=1;
    int iterPerCol = ceil(matrix_row/(float)(cfg->_ncols*cfg->_nblocks));
    cout<<numColPerCore<<endl;
    cout<<iterPerCol<<endl;
    for(int i=0; i<matrix_col; i+=numColPerCore){
        int increase_precision_index = 0;
        int two_to_n = 1;
        int curr_iter = 0;
        PrecisionT::Precision precision_accumulate_temp = multiply_precision;

        int tile=i/ceil(matrix_col/(float)use_tiles);
        for(int j=0; j<iterPerCol; j++){
            request = new Request(Request::Type::RowLoad);
            request->addOperand(sys->getAddress(tile,0,0), 0, input_precision); //dst
            request->addOperand(sys->DRAM_ADDR, matrix_row, input_precision); //src
            requests.push_back(*request);

            request = new Request(Request::Type::RowMul_CRAM_RF);
            request->addOperand(sys->getAddress(tile,0,0), 0, input_precision); //src
            request->addOperand(cfg->_num_regs_per_rf * tile, 0, input_precision);//rf
            request->addOperand(sys->getAddress(tile,0,input_precision.bits()), 0, multiply_precision); //dst
            requests.push_back(*request);

            //Add the result of multiplication into the accumulator
            request = new Request(Request::Type::RowAdd);
            request->addOperand(sys->getAddress(tile,0,input_precision.bits()), 0, precision_accumulate_temp); //src
            request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()), 0, precision_accumulate_temp);//src2
            
            if(curr_iter == increase_precision_index){
                precision_accumulate_temp = PrecisionT::Precision{0,std::min(precision_accumulate_temp.bits()+1,accumulate_precision.bits()),0};
                increase_precision_index += two_to_n;
                two_to_n *= 2;
            }
            curr_iter++;

            request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()), 0, precision_accumulate_temp); //dst
            requests.push_back(*request);
        }
    }

     //reduce
    int round = 0;
    if(use_tiles>1){                
        for(int gap =1; gap<use_tiles; gap = gap*2){
            int increase_precision_index = 0;
            int two_to_n = 1;
            int curr_iter = 0;
            PrecisionT::Precision precision_accumulate_temp = multiply_precision;

            for(int iter_tile=0; iter_tile+gap<use_tiles; iter_tile=iter_tile+2*gap){
                int tile = iter_tile % cfg->_ntiles;
                //receive partial results from tile that is gap away
                request = new Request(Request::Type::TileSend);
                request->addOperand(sys->getAddress((tile+gap)%cfg->_ntiles,0,input_precision.bits()+multiply_precision.bits()), matrix_row, accumulate_precision); //src
                request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()+accumulate_precision.bits()), matrix_row, accumulate_precision); //dst
                requests.push_back(*request);

                request = new Request(Request::Type::TileReceive);
                request->addOperand(sys->getAddress((tile+gap)%cfg->_ntiles,0,input_precision.bits()+multiply_precision.bits()), matrix_row, accumulate_precision); //src
                request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()+accumulate_precision.bits()), matrix_row, accumulate_precision); //dst
                requests.push_back(*request);

                //add
                request = new Request(Request::Type::RowAdd);
                request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()), 0, precision_accumulate_temp); //src
                request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()+accumulate_precision.bits()), 0, accumulate_precision); //src2
                
                if(curr_iter == increase_precision_index){
                    precision_accumulate_temp = PrecisionT::Precision{0,std::min(precision_accumulate_temp.bits()+1,accumulate_precision.bits()),0};
                    increase_precision_index += two_to_n;
                    two_to_n *= 2;
                }
                curr_iter++;
                
                request->addOperand(sys->getAddress(tile,0,input_precision.bits()+multiply_precision.bits()), 0, precision_accumulate_temp); //dst
                requests.push_back(*request);
            }
            round++;
        }
    }

    //Store results into DRAM
    request = new Request(Request::Type::RowStore);
    request->addOperand(sys->getAddress(0, 0, input_precision.bits()+multiply_precision.bits()), 0, accumulate_precision); //src
    request->addOperand(sys->DRAM_ADDR, 0, accumulate_precision); //dst
    requests.push_back(*request);

      
    

    for (unsigned int i = 0; i < requests.size(); i++)
        sys->sendRequest(requests[i]);

    return 0;
}

static __attribute__((unused)) Registry::Entry &__gemv_tiled__ = pimsim::registerFunc("gemv_tiled", gemv_tiled);
