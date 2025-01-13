// tvm target: c -keys=cpu -link-params=0
#define TVM_EXPORTS
#include <cstdint>
#include <numeric>
#include <sstream>

#include "backend/System.h"
#include "gemm_tiled.h"
int32_t gemm_tiled_compute_only(System* sys, std::string param_file){
    //Parameters:
    int M = 256;
    int K = 256;
    int N = 256;
    if(param_file == ""){
        std::cout<<"using default parameters"<<std::endl;
    }
    else{
        std::cout<<"using parameters from file: "<<param_file<<std::endl;
    }
    std::ifstream input( param_file);
    for( std::string line; getline( input, line ); )
    {
        std::istringstream iss(line);
        std::string name;
        int value;
        if (!(iss >> name >> value)) { break; } // error
        if(name == "M"){
            M = value;
        }
        else if(name == "K"){
            K = value;
        }
        else if(name == "N"){
            N = value;
        }
    }
    
    std::vector<Request> requests;
    Request *request;
    Config* cfg = sys->_config;

    

    int matrixARowNum = M;
    int matrixAColNum = K;
    int matrixBRowNum = matrixAColNum;
    int matrixBColNum = N;

    PrecisionT::Precision precision_input = PrecisionT::INT8;
    PrecisionT::Precision precision_multiply = PrecisionT::INT16;
    PrecisionT::Precision precision_accumulate = PrecisionT::INT32;
    PrecisionT::Precision precision_result = PrecisionT::INT8;

    func_gemm_tiled_compute_only(M, K, N, precision_input, precision_multiply, precision_accumulate, precision_result, requests, sys);
    for (unsigned int i = 0; i < requests.size(); i++)
        sys->sendRequest(requests[i]);

    return 0;
}



static __attribute__((unused)) Registry::Entry &__gemm_tiled_compute_only__ = pimsim::registerFunc("gemm_tiled_compute_only", gemm_tiled_compute_only);
