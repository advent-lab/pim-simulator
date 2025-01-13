#include "gemm_tiled.h"
void func_gemm_tiled( int M, int K, int N,\
                PrecisionT::Precision precision_input, PrecisionT::Precision precision_multiply, PrecisionT::Precision precision_accumulate, PrecisionT::Precision precision_result, \
                std::vector<Request> &requests, System* sys){

    Request *request;
    Config* cfg = sys->_config;



    // // make matrix B smaller than matrix A
    // if(M < N){
    //     std::swap(M, N);
    // }
    //8bit multiplication
    //16bit result
    //need 17 rows to store 8bit operand and 16 bit result simultanously, as operand can be discarded bit by bit during computation
    //tile_capacity indicates available capacity to store matrix A and B, the rest are for temp data, multiply results (16 rows) and accumulated results (32 rows).
    int tile_capacity = (cfg->_nrows-precision_accumulate.bits()-precision_input.bits()) * cfg->_ncols * cfg->_nblocks;
    int matrixBSize = K * N * precision_input.bits();
    

    // Local tile computation Size 
    // int Local_matrixARowNum = 384;
    // int Local_matrixAColNum = matrixAColNum; 
    // int Local_matrixBRowNum = matrixBRowNum; 
    // int Local_N = N; 

    int M_base_tile = cfg->_ntiles;
    int K_base_tile = cfg->_nblocks;
    int N_base_tile = cfg->_ncols;
    int N_multiple = 1;
    int K_multiple = min((int)ceil(K/float(K_base_tile)), (int)floor((tile_capacity- cfg->_ncols*cfg->_nblocks*precision_input.bits())/float(K_base_tile*N_base_tile*precision_input.bits())));//need to leave rows for at least 1 row of A, the rest can store cols of B as much as possible
    int M_multiple = min((int)ceil(M/float(M_base_tile)) , (int)floor((tile_capacity - K_multiple*K_base_tile*N_base_tile*precision_input.bits())/cfg->_ncols/cfg->_nblocks/precision_input.bits()));//the rest can store multiple A rows

    int local_M = M_base_tile * M_multiple;
    int local_K = K_base_tile * K_multiple;
    int local_N = N_base_tile * N_multiple;

    int M_num_tiles = ceil(M/(float)local_M);
    int K_num_tiles = ceil(K/(float)local_K);
    int N_num_tiles = ceil(N/(float)local_N);

    int total_num_tiles = M_num_tiles * K_num_tiles * N_num_tiles;


    std::cout<<"M: "<<M<<std::endl;
    std::cout<<"K: "<<K<<std::endl;
    std::cout<<"N: "<<N<<std::endl;

    std::cout<<"Local Compute tile : "  << local_M << " x " << local_K << " x "
            << local_N <<std::endl;

    std::cout<<"Tiling Grid : "  << M_num_tiles << " x " << K_num_tiles << " x "
            << N_num_tiles <<std::endl;

    std::cout<<"Total tiles  "<< total_num_tiles <<std::endl;
    std::cout<<"Total Reduction required: " << ceil(local_M/(float)120)*ceil(local_N/(float)256)*total_num_tiles <<std::endl;
    std::cout<<"Total Stores required: " << M_num_tiles*N_num_tiles <<std::endl;

    for ( int N_ = 0 ; N_ < N_num_tiles; N_++){
        for ( int K_=0; K_< K_num_tiles; K_++){
            // Row load local tile of B in Tile 0 
            // request = new Request(Request::Type::RowLoad);
            // request->addOperand(sys->getAddress(119,0,8),local_K*local_N,precision_input); //cram addr
            // request->addOperand(sys->DRAM_ADDR, 0, precision_input); //dram addr
            // requests.push_back(*request);
            if(true){
                for (int tile_load=0; tile_load<cfg->_meshWidth; tile_load++){
            
                    request = new Request(Request::Type::RowLoad);
                    request->addOperand(sys->getAddress(tile_load,0,8),local_K*local_N/12,precision_input); //cram addr
                    request->addOperand(sys->DRAM_ADDR, 0, precision_input); //dram addr
                    requests.push_back(*request);
                    int X = 8;
                    std::vector<int> v;
                    for(int tile = tile_load+12; tile < cfg->_ntiles; tile+=12){
                        v.push_back(tile);
                    }
                    for(int x=0; x<X; x++){      
                        sys->broadcast_p2p(tile_load,precision_input,v, local_K*local_N/12/X, requests,0,0,0,0);              
                    }

                    
                    // std::vector<int> v;
                    // for(int tile = tile_load+12; tile < cfg->_ntiles; tile+=12){
                    //     v.push_back(tile);
                    // }
                    // int bc = 16;
                    // for(int bc_ =0; bc_< bc; bc_++){
                    //     sys->broadcast_p2p(tile_load,precision_input,v, local_K*local_N/bc, requests,0,0,0,0);
                    // }
                }
                for(int y=0; y<10; y++){
                    int start_tile = y*12;
                    for(int iter=0; iter<12; iter++){
                        for(int i=0; i<12; i+=2){
                            request = new Request(Request::Type::TileSend);
                            request->addOperand(sys->getAddress(start_tile+i,0,8),local_K*local_N/12,precision_input); //cram addr
                            request->addOperand(sys->getAddress(start_tile+(i+1)%12,0,8),local_K*local_N,precision_input); //cram addr
                            requests.push_back(*request);  
                            request = new Request(Request::Type::TileReceive);
                            request->addOperand(sys->getAddress(start_tile+i,0,8),local_K*local_N/12,precision_input); //cram addr
                            request->addOperand(sys->getAddress(start_tile+(i+1)%12,0,8),local_K*local_N,precision_input); //cram addr
                            requests.push_back(*request);        
                        }
                        for(int i=1; i<12; i+=2){
                            request = new Request(Request::Type::TileSend);
                            request->addOperand(sys->getAddress(start_tile+i,0,8),local_K*local_N/12,precision_input); //cram addr
                            request->addOperand(sys->getAddress(start_tile+(i+1)%12,0,8),local_K*local_N,precision_input); //cram addr
                            requests.push_back(*request);  
                            request = new Request(Request::Type::TileReceive);
                            request->addOperand(sys->getAddress(start_tile+i,0,8),local_K*local_N/12,precision_input); //cram addr
                            request->addOperand(sys->getAddress(start_tile+(i+1)%12,0,8),local_K*local_N,precision_input); //cram addr
                            requests.push_back(*request);        
                        }
                    }
                    
                }
            }
            
            // Broadcast Tile0 Data to all the 119 tiles 
            // std::vector<int> v(119);
            // std::iota (std::begin(v), std::end(v), 1); // Fill with 0, 1, ...
            // int bc = 14;
            // for(int bc_ =0; bc_< bc; bc_++){
            //     sys->broadcast_p2p(0,precision_input,v, local_K*local_N/bc, requests,0,0,0,0);
            // }
            if(true){
                for (int M_=0; M_<M_num_tiles; M_++){
                    for(int tile = 0; tile < 120; tile++){
                        //Load partial sum
                        request = new Request(Request::Type::RowLoad);
                        request->addOperand(sys->getAddress(tile,0,0),256,precision_input); //cram addr
                        request->addOperand(sys->DRAM_ADDR, 0, precision_input); //dram addr
                        request->setShuffle(0, 0, 0, 0);
                        requests.push_back(*request);                    
                        for(int n=0; n<ceil(local_N/(float)256); n++){
                            for(int m=0; m<ceil(local_M/(float)120); m++){

                                int increase_precision_index = 0;
                                int two_to_n = 1;
                                int curr_iter = 0;
                                PrecisionT::Precision precision_accumulate_temp = precision_multiply;
                                for(int k=0; k<ceil(local_K/(float)256); k++){
                                    // Row load MAT A  ROW 0 (0-255) 256 Elements 
                                    request = new Request(Request::Type::RowLoad);
                                    request->addOperand(sys->getAddress(tile,0,0),256,precision_input); //cram addr
                                    request->addOperand(sys->DRAM_ADDR, 0, precision_input); //dram addr
                                    request->setShuffle(0, 1, 0, 1);
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
                        request = new Request(Request::Type::RowStore);
                        request->addOperand(sys->getAddress(tile,0,0),256*M_multiple*N_multiple, precision_result); //cram addr
                        request->addOperand(sys->DRAM_ADDR, 0, precision_result); //dram addr
                        requests.push_back(*request);
                    }
                    //Send back resutls after every 3 times for K = 768 / 256 ( K reduction dimention tile size )
                
                    for(int tile = 0; tile < 120; tile++)
                    {

                        
                    }     
                }
            }
        }
    }

    return;
}