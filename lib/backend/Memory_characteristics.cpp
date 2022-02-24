//
// Created by caspar on 12/19/20.
//

#include "Memory_characteristics.h"
#include <iostream>

using namespace pimsim;
using namespace std;

MemoryCharacteristics::MemoryCharacteristics(Configuration configuration, int wordsize, int freq) {
    _configuration = configuration;
    _wordsize = wordsize;
    _freq = freq;
}

//Helper function to find the number of clocks for a given operation and datatype
int getClocks(string op, string dtype, int exponent, int mantissa) {
    int clocks;
    if (op=="add") {
        if (dtype=="float") {
            clocks = 2 * mantissa * exponent + 9 * mantissa + 7 * exponent + 12;
        }
        else {
            clocks = mantissa + 1;
        }
    }
    else if (op=="mul") {
        if (dtype=="float") {
            clocks = mantissa * mantissa + 7 * mantissa + 3 * exponent + 5;
        }
        else {
            clocks = mantissa * mantissa + 3 * mantissa - 2;
        }
    }
    else {
        //unsopported operation
        clocks = -1;
    }
    return clocks;
}

//double MemoryCharacteristics::getTiming(int idx, PrecisionT precision) {
double MemoryCharacteristics::getTiming(Request req) {
    int idx = int(req.type);
    double time = 0.0;
    switch (idx) {
        case 0: //RowSet
        case 1: //ColSet
            time = T_SET;
            break;
        case 2: //RowReset
        case 3: //ColReset
            time = T_RESET;
            break;
        case 4: //RowMv
        case 5: //ColMv
            time = T_CLK;
            break;
        case 6: //RowRead
        case 7: //ColRead
            time = T_READ;
            break;
        case 8: //RowWrite
        case 9: //ColWrite
            time = T_WRITE;
            break;
        case 10: //RowAdd
        case 11: //ColAdd
        case 12: //RowSub
        case 13: //ColSub

            //time = (12 * N + 1) * T_CLK; //fixed-32 --- 385
            //time = (3 + 16 * N_e + 19 * N_m + N_m * N_m) * T_CLK + (2 * N_m + 1) * T_SEARCH; // float-32 --- 1191
            //TODO: For now just looking at precision of the first operand
            switch(req.precision_list[0]) {
                case 0:  //fp8_e3m4
                    time = getClocks("add", "float", 3, 4) * T_CLK; 
                    break;
                case 1:  //fp16_e8m7
                    time = getClocks("add", "float", 8, 7) * T_CLK;
                    break;
                case 2:  //fp32_e8m23
                    time = getClocks("add", "float", 8, 23) * T_CLK;
                    break;
                case 3:  //INT4
                    time = getClocks("add", "int", 0, 4) * T_CLK;
                    break;
                case 4:  //INT8
                    time = getClocks("add", "int", 0, 8) * T_CLK;
                    break;
                case 5:  //INT16
                    time = getClocks("add", "int", 0, 16) * T_CLK;
                    break;
                case 6:  //MAX
                    time = -1; 
                    break;
            } 
        case 14: //RowMul
        case 15: //RowDiv
        case 16: //ColMul
        case 17: //ColDiv

            //time = (13 * N * N - 14 * N + 6) * T_CLK; // fixed-32 (full precision)
            //time = (6.5 * N * N - 7.5 * N - 2) * T_CLK; // fixed-32 (half precision) --- 6414 3400
            //time = (12 * N_e + 6.5 * N_m * N_m - 7.5 * N_m - 2) * T_CLK; // float-32 --- 3360 2276
            //TODO: For now just looking at precision of the first operand
            switch(req.precision_list[0]) {
                case 0:  //fp8_e3m4
                    time = getClocks("mul", "float", 3, 4) * T_CLK; 
                    break;
                case 1:  //fp16_e8m7
                    time = getClocks("mul", "float", 8, 7) * T_CLK;
                    break;
                case 2:  //fp32_e8m23
                    time = getClocks("mul", "float", 8, 23) * T_CLK;
                    break;
                case 3:  //INT4
                    time = getClocks("mul", "int", 0, 4) * T_CLK;
                    break;
                case 4:  //INT8
                    time = getClocks("mul", "int", 0, 8) * T_CLK;
                    break;
                case 5:  //INT16
                    time = getClocks("mul", "int", 0, 16) * T_CLK;
                    break;
                case 6:  //MAX
                    time = -1; 
                    break;
            } 
        case 18: //RowBitwise
        case 19: //ColBitwise
            time = T_CLK;
            break;
        case 20: //RowSearch
        case 21: //ColSearch
            time = T_SEARCH;
            break;
        case 22: //BlockSend
        case 23: //BlockReceive
        case 24: //BlockSend_Receive
        case 25: //TileSend
        case 26: //TileReceive
        case 27: //TileSend_Receive
        case 28: //ChipSend_Receive
            time = T_CLK; // Assuming the global clock frequency is 1/T_CLK.
            break;
        default:
            time = T_CLK;
            break;
    }
    double fclk = time / (1000000000.0 / float(_freq));
    time = int(fclk + 0.5);
    return time;
}


//double MemoryCharacteristics::getEnergy(int idx) {
double MemoryCharacteristics::getEnergy(Request req) {
    double energy = 0.0;
    int idx = int(req.type);
    switch (idx) {
        case 0: //RowSet
        case 1: //ColSet
            energy = E_SET;
            break;
        case 2: //RowReset
        case 3: //ColReset
            energy = E_RESET;
            break;
        case 4: //RowMv
        case 5: //ColMv
            energy = E_NOR;
            break;
        case 6: //RowRead
        case 7: //ColRead
            energy = E_READ;
            break;
        case 8: //RowWrite
        case 9: //ColWrite
            energy = E_WRITE;
            break;
        case 10: //RowAdd
        case 11: //ColAdd
        case 12: //RowSub
        case 13: //ColSub

            //energy = (12 * N + 1) * E_NOR; //fixed-32
            //energy = (3 + 16 * N_e + 19 * N_m + N_m * N_m) * E_NOR + (2 * N_m + 1) * E_SEARCH; //float-32
            switch(req.precision_list[0]) {
                case 0:  //fp8_e3m4
                    energy = getClocks("add", "float", 3, 4) * E_CLK; 
                    break;
                case 1:  //fp16_e8m7
                    energy = getClocks("add", "float", 8, 7) * E_CLK;
                    break;
                case 2:  //fp32_e8m23
                    energy = getClocks("add", "float", 8, 23) * E_CLK;
                    break;
                case 3:  //INT4
                    energy = getClocks("add", "int", 0, 4) * E_CLK;
                    break;
                case 4:  //INT8
                    energy = getClocks("add", "int", 0, 8) * E_CLK;
                    break;
                case 5:  //INT16
                    energy = getClocks("add", "int", 0, 16) * E_CLK;
                    break;
                case 6:  //MAX
                    energy = -1; 
                    break;
            } 

        case 14: //RowMul
        case 15: //RowDiv
        case 16: //ColMul
        case 17: //ColDiv

            //energy = (13 * N * N - 14 * N + 6) * E_NOR; // fixed-32 (full precision)
            //energy = (6.5 * N * N - 7.5 * N - 2) * E_NOR; // fixed-32 (half precision)
            //energy = (12 * N_e + 6.5 * N_m * N_m - 7.5 * N_m - 2) * E_NOR; // float-32

            switch(req.precision_list[0]) {
                case 0:  //fp8_e3m4
                    energy = getClocks("mul", "float", 3, 4) * E_CLK; 
                    break;
                case 1:  //fp16_e8m7
                    energy = getClocks("mul", "float", 8, 7) * E_CLK;
                    break;
                case 2:  //fp32_e8m23
                    energy = getClocks("mul", "float", 8, 23) * E_CLK;
                    break;
                case 3:  //INT4
                    energy = getClocks("mul", "int", 0, 4) * E_CLK;
                    break;
                case 4:  //INT8
                    energy = getClocks("mul", "int", 0, 8) * E_CLK;
                    break;
                case 5:  //INT16
                    energy = getClocks("mul", "int", 0, 16) * E_CLK;
                    break;
                case 6:  //MAX
                    energy = -1; 
                    break;
            } 
        case 18: //RowBitwise
        case 19: //ColBitwise
            energy = E_NOR;
            break;
        case 20: //RowSearch
        case 21: //ColSearch
            energy = E_SEARCH;
            break;
        case 22: //BlockSend
        case 23: //BlockReceive
        case 24: //BlockSend_Receive
        case 25: //TileSend
        case 26: //TileReceive
        case 27: //TileSend_Receive
        case 28: //ChipSend_Receive
//            if (_configuration == Configuration::Bus)
//                energy = E_internal_bus[(int) log(_wordsize / 32)] + E_switching_bus[(int) log(_wordsize / 32)];
//            else if (_configuration == Configuration::HTree)
//                energy = E_internal_htree[(int) log(_wordsize / 32)] + E_switching_htree[(int) log(_wordsize / 32)];
//            else
//                energy = E_internal_htree[(int) log(_wordsize / 32)] + E_switching_htree[(int) log(_wordsize / 32)];
            energy = 0;
            break;
        default:
            energy = E_NOR;
            break;
    }
    return energy;
}


double MemoryCharacteristics::getTotalLeakageEnergy(int cycles, int ntiles, int nblocks) {
    double energy = 0.0;

    int _tile_level = log2(ntiles) / 2;
    int _block_level = log2(nblocks) / 2;
    int switch_numbers = 0;
    int bus_numbers = 0;

    for (int i = 0; i < _block_level; i++)
        switch_numbers += pow(4, i);

    cout << "switch_numbers: " << switch_numbers << endl;
    switch_numbers *= ntiles;

    for (int i = 0; i < _tile_level; i++)
        switch_numbers += pow(4, i);

    cout << "switch_numbers: " << switch_numbers << endl;
    bus_numbers = ntiles + 1;
    cout << "bus_numbers: " << bus_numbers << endl;

    if (_configuration == Configuration::Bus)
        energy += E_leakage_bus[(int) log(_wordsize / 32)] * bus_numbers * ((float) cycles / _freq);
    else if (_configuration == Configuration::HTree)
        energy += E_leakage_htree[(int) log(_wordsize / 32)] * switch_numbers * ((float) cycles / _freq);
    else
        energy += E_leakage_htree[(int) log(_wordsize / 32)] * switch_numbers * ((float) cycles / _freq);

    energy += E_leakage_per_block * ntiles * nblocks * ((float) cycles / _freq);

    return energy;
}


double MemoryCharacteristics::getTotalLeakageEnergy(int ntiles, int nblocks) {
    double energy = 0.0;

    int _tile_level = log2(ntiles) / 2;
    int _block_level = log2(nblocks) / 2;
    int switch_numbers = 0;
    int bus_numbers = 0;

    for (int i = 0; i < _block_level; i++)
        switch_numbers += pow(4, i);

    cout << "switch_numbers: " << switch_numbers << endl;
    switch_numbers *= ntiles;

    for (int i = 0; i < _tile_level; i++)
        switch_numbers += pow(4, i);

    cout << "switch_numbers: " << switch_numbers << endl;
    bus_numbers = ntiles + 1;
    cout << "bus_numbers: " << bus_numbers << endl;

    if (_configuration == Configuration::Bus)
        energy += E_leakage_bus[(int) log(_wordsize / 32)] * bus_numbers;
    else if (_configuration == Configuration::HTree)
        energy += E_leakage_htree[(int) log(_wordsize / 32)] * switch_numbers;
    else
        energy += E_leakage_htree[(int) log(_wordsize / 32)] * switch_numbers;

    energy += E_leakage_per_block * ntiles * nblocks;

    cout << (E_leakage_bus[5] + E_internal_bus[5] + E_switching_bus[5]) / pow(10, 12) << endl;

    return energy;
}

