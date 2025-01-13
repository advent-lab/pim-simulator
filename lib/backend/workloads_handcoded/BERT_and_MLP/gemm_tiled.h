#include <cstdint>
#include <numeric>

#include "backend/System.h"

void func_gemm_tiled( int M, int K, int N,\
                PrecisionT::Precision precision_input, PrecisionT::Precision precision_multiply, PrecisionT::Precision precision_accumulate, PrecisionT::Precision precision_result, \
                std::vector<Request> &requests, System* sys);
void func_gemm_tiled_nap( int M, int K, int N,\
                PrecisionT::Precision precision_input, PrecisionT::Precision precision_multiply, PrecisionT::Precision precision_accumulate, PrecisionT::Precision precision_result, \
                std::vector<Request> &requests, System* sys);
void func_gemm_tiled_compute_only( int M, int K, int N,\
                PrecisionT::Precision precision_input, PrecisionT::Precision precision_multiply, PrecisionT::Precision precision_accumulate, PrecisionT::Precision precision_result, \
                std::vector<Request> &requests, System* sys);