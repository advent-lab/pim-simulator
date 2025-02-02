// tvm target: c -keys=cpu -link-params=0
#define TVM_EXPORTS
#include <cstdint>
#include <cmath>
#include <numeric>
#include <sstream>

#include "backend/System.h"
/////////////////////////////////////////////////////////////
// Conv2d microbenchmark
/////////////////////////////////////////////////////////////

int32_t vmul(System* sys, std::string param_file)
{
    std::vector<Request> requests;
    Request *request;
    Config* cfg = sys->_config;

    PrecisionT::Precision precision_input = PrecisionT::INT32;
    PrecisionT::Precision precision_multiply = PrecisionT::INT32;




  

    
   
    
    //Load weights
    for(int l_=0; l_<sys->_config->_ntiles; l_++){//mapped to different tiles
        int tile = l_;

        request = new Request(Request::Type::RowMul);
        request->addOperand(sys->getAddress(tile,0,0), 0, precision_input); //src
        request->addOperand(sys->getAddress(tile,0,0), 0, precision_input);//src_
        request->addOperand(sys->getAddress(tile,0,0), 0, precision_input); //dst
        requests.push_back(*request);

    }

    


    for (unsigned int i = 0; i < requests.size(); i++)
        sys->sendRequest(requests[i]);
    return 0;
}

/////////////////////////////////////////////////////////////
// Simple program to perform an FIR filter
/////////////////////////////////////////////////////////////


static __attribute__((unused)) Registry::Entry &__vmul__ = pimsim::registerFunc("vmul", vmul);

