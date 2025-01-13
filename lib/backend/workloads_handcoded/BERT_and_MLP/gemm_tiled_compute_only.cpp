#include "gemm_tiled.h"
void func_gemm_tiled_compute_only( int M, int K, int N,\
                PrecisionT::Precision precision_input, PrecisionT::Precision precision_multiply, PrecisionT::Precision precision_accumulate, PrecisionT::Precision precision_result, \
                std::vector<Request> &requests, System* sys){

    Request *request;
    Config* cfg = sys->_config;

    
        for(int tile = 0; tile < 120; tile++){
            for(int n=0; n<ceil(N/(float)256); n++){
                for(int m=0; m<ceil(M/(float)120); m++){

                    int increase_precision_index = 0;
                    int two_to_n = 1;
                    int curr_iter = 0;
                    PrecisionT::Precision precision_accumulate_temp = precision_multiply;
                    for(int k=0; k<ceil(K/(float)256); k++){
                        request = new Request(Request::Type::BlockBroadCast);
                        request->addOperand(sys->getAddress(tile,0,0), 0, precision_input);//rows to be broadcasted to all blocks from one block
                        requests.push_back(*request); 

                        request = new Request(Request::Type::RowMul);
                        request->addOperand(sys->getAddress(tile,0,0), 0, precision_input); //src
                        request->addOperand(sys->getAddress(tile,0,8), 0, precision_input); //src
                        request->addOperand(sys->getAddress(tile,0,16), 0, precision_multiply); //dst
                        requests.push_back(*request); 
                        
                        request = new Request(Request::Type::RowAdd);
                        request->addOperand(sys->getAddress(tile,0,0), 0, precision_accumulate_temp); //src
                        request->addOperand(sys->getAddress(tile,0,10), 0, precision_accumulate_temp); //src
                        if(curr_iter == increase_precision_index){
                            precision_accumulate_temp = PrecisionT::Precision{0,std::min(precision_accumulate_temp.bits()+1,precision_accumulate.bits()),0};
                            increase_precision_index += two_to_n;
                            two_to_n *= 2;
                        }
                        curr_iter++;
                        request->addOperand(sys->getAddress(tile,0,30), 0, precision_accumulate_temp); //dst
                        requests.push_back(*request); 
                    }
                    request = new Request(Request::Type::RowReduce_WithinTile);
                    request->addOperand(sys->getAddress(tile,0,16), (int)log2(256), precision_accumulate_temp); //src
                    request->addOperand(sys->getAddress(tile,0,24), (int)log2(256), precision_accumulate); //dst
                    requests.push_back(*request);
                    // store partial sum
                    // request = new Request(Request::Type::RowStore);
                    // request->addOperand(sys->getAddress(tile,0,0),256, precision_result); //cram addr
                    // request->addOperand(sys->DRAM_ADDR, 0, precision_result); //dram addr
                    // requests.push_back(*request);
                }
            }
        }
        //Send back resutls after every 3 times for K = 768 / 256 ( K reduction dimention tile size )
    

    return;
}