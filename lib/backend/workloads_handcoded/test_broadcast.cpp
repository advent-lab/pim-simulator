// tvm target: c -keys=cpu -link-params=0
#define TVM_EXPORTS
#include <cstdint>

#include "backend/System.h"

int32_t test_broadcast(System* sys){
    std::vector<Request> requests;
    Request *request;

                    
    request = new Request(Request::Type::TileSend_BroadCast);
    request->addOperand(sys->getAddress(0,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);
    request = new Request(Request::Type::Signal, sys->m1);
    request->addOperand(sys->getAddress(0,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::Wait, sys->m1);
    request->addOperand(sys->getAddress(1,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);
    request = new Request(Request::Type::TileReceive_BroadCast);
    request->addOperand(sys->getAddress(1,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::Wait, sys->m1);
    request->addOperand(sys->getAddress(2,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);
    request = new Request(Request::Type::TileReceive_BroadCast);
    request->addOperand(sys->getAddress(2,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::Wait, sys->m1);
    request->addOperand(sys->getAddress(3,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);
    request = new Request(Request::Type::TileReceive_BroadCast);
    request->addOperand(sys->getAddress(3,0,0), 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::BlockBroadCast);
    request->addOperand(sys->getAddress(0,0,0), 1, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::BlockBroadCast);
    request->addOperand(sys->getAddress(1,0,0), 2, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::BlockBroadCast);
    request->addOperand(sys->getAddress(2,0,0), 4, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::BlockBroadCast);
    request->addOperand(sys->getAddress(3,0,0), 8, PrecisionT::INT4); //src
    requests.push_back(*request);

                    


    for (unsigned int i = 0; i < requests.size(); i++)
        sys->sendRequest(requests[i]);
}



static __attribute__((unused)) Registry::Entry &__test_broadcast__ = pimsim::registerFunc("test_broadcast", test_broadcast);