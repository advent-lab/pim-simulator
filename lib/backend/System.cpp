#include "backend/System.h"
#include <chrono>
#include <algorithm>
//#include <thread>

//DRAM addresses don't matter. We just define 1 address
//and use it everywhere.
AddrT DRAM_ADDR;

using namespace pimsim;
using namespace std;

template <class T>
System<T>::System(Config* config) : _config(config)
{
    _nchips = _config->get_nchips();
    _ntiles = _config->get_ntiles();
    _nblocks = _config->get_nblocks();
    _nrows = config->get_nrows();
    _ncols = config->get_ncols();
    _clock_rate = config->get_clock_rate();
    _blockctrl = config->get_blockctrl();
    _tilectrl = config->get_tilectrl();
    _chipctrl = config->get_chipctrl();
    _wordsize_block2block = config->get_wordsize_block2block();
    _wordsize_tile2tile = config->get_wordsize_tile2tile();
    _wordsize_dram = config->get_wordsize_dram();
    if (!(_blockctrl || _tilectrl || _chipctrl))
        _blockctrl = true;
    _blocksize = _nrows * _ncols; // set the blocksize based on columns and rows
    this->_tilesize = this->_blocksize * _nblocks;
    this->_chipsize = this->_tilesize * _ntiles;

    rstFile = fopen(config->get_rstfile().c_str(), "w");

    if (config->get_mem_configuration() == "htree") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::HTree, _wordsize_block2block, _wordsize_tile2tile, _wordsize_dram, _clock_rate);
    } else if (config->get_mem_configuration() == "bus") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::Bus, _wordsize_block2block, _wordsize_tile2tile, _wordsize_dram, _clock_rate);
    } else {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::Ideal, _wordsize_block2block, _wordsize_tile2tile, _wordsize_dram, _clock_rate);
    }

    for (int i = 0; i < _nchips; i++) {
        MemoryChip* chip = new MemoryChip(_ntiles, _nblocks, _nrows, _ncols, _values);
        chip->setValues(_values);
        //We don't need a controller per chip
        //Controller* ctrl = new Controller(chip);
        chip->setId(i);
        //chip->setController(ctrl);
        chip->setParent(NULL);
        _chips.push_back(chip);
    }

    //Inter tile communication
    m1 = new Mailbox();  //use as a semaphore
    m2 = new Mailbox(_ntiles);  //use as a barrier for all tiles
    m3 = new Mailbox();

    //                                      t, b, r
    cram_base_addr_tile0_block0 = getAddress(0,0,0); 
    cram_addr_tile0_block0_row0 = getAddress(0,0,0);  //src1
    cram_addr_tile0_block0_row4 = getAddress(0,0,4);  //src2
    cram_addr_tile0_block0_row8 = getAddress(0,0,8);  //dst

    cram_base_addr_tile0_block1 = getAddress(0,1,0); 
    cram_addr_tile0_block1_row0 = getAddress(0,1,0); 
    cram_addr_tile0_block1_row4 = getAddress(0,1,4); 
    cram_addr_tile0_block1_row8 = getAddress(0,1,8); 

    cram_base_addr_tile0_block2 = getAddress(0,2,0); 
    cram_addr_tile0_block2_row0 = getAddress(0,2,0); 
    cram_addr_tile0_block2_row4 = getAddress(0,2,4); 
    cram_addr_tile0_block2_row8 = getAddress(0,2,8); 

    cram_base_addr_tile0_block3 = getAddress(0,3,0); 
    cram_addr_tile0_block3_row0 = getAddress(0,3,0); 
    cram_addr_tile0_block3_row4 = getAddress(0,3,4); 
    cram_addr_tile0_block3_row8 = getAddress(0,3,8); 
    cram_addr_tile0_block3_row16 =getAddress(0,3,16);
    cram_addr_tile0_block3_row24 =getAddress(0,3,24);

    //                                      t, b, r
    cram_base_addr_tile1_block0 = getAddress(1,0,0); 
    cram_addr_tile1_block0_row0 = getAddress(1,0,0);  //src1
    cram_addr_tile1_block0_row4 = getAddress(1,0,4);  //src2
    cram_addr_tile1_block0_row8 = getAddress(1,0,8);  //dst

    cram_base_addr_tile1_block1 = getAddress(1,1,0); 
    cram_addr_tile1_block1_row0 = getAddress(1,1,0); 
    cram_addr_tile1_block1_row4 = getAddress(1,1,4); 
    cram_addr_tile1_block1_row8 = getAddress(1,1,8); 

    cram_base_addr_tile1_block2 = getAddress(1,2,0); 
    cram_addr_tile1_block2_row0 = getAddress(1,2,0); 
    cram_addr_tile1_block2_row4 = getAddress(1,2,4); 
    cram_addr_tile1_block2_row8 = getAddress(1,2,8); 

    cram_base_addr_tile1_block3 = getAddress(1,3,0); 
    cram_addr_tile1_block3_row0 = getAddress(1,3,0); 
    cram_addr_tile1_block3_row4 = getAddress(1,3,4); 
    cram_addr_tile1_block3_row8 = getAddress(1,3,8); 
    cram_addr_tile1_block3_row16 =getAddress(1,3,16);
    cram_addr_tile1_block3_row24 =getAddress(1,3,24);

    rf_base_addr_tile0 = getAddress(0,0,0); 
    rf_base_addr_tile1 = getAddress(1,0,0); 
    rf_base_addr_tile2 = getAddress(2,0,0); 
    rf_base_addr_tile3 = getAddress(3,0,0); 
}

template <class T>
System<T>::~System()
{
    fclose(rstFile);
}

template <class T>
void System<T>::addChip(MemoryCharacteristics* values, int n_tiles, int n_blocks, int n_rows, int n_cols) {
    int global_chip_id = _chips.size();
    MemoryChip* chip = new MemoryChip(n_tiles, n_blocks, n_rows, n_cols, _values);
    Controller* ctrl = new Controller(chip);
    chip->setId(global_chip_id);
    chip->setController(ctrl);
    chip->setParent(NULL);
    chip->setValues(values);
    _chips.push_back(chip);
}

//Form an address for the given tile, block, row
//Chip and row numbers are assumed to be 0.
template <class T>
AddrT System<T>::getAddress(int tile, int block, int row)
{
    AddrT addr = 0;
    addr *= _ntiles;
    addr += tile;
    addr *= _nblocks;
    addr += block;
    addr *= _nrows;
    addr += row;
    addr *= _ncols;
    addr += 0;
    return addr;
}

//Form an address for the given chip, tile, block, row, column
template <class T>
AddrT System<T>::getAddress(int chip, int tile, int block, int row, int col)
{
    AddrT addr = chip;
    addr *= _ntiles;
    addr += tile;
    addr *= _nblocks;
    addr += block;
    addr *= _nrows;
    addr += row;
    addr *= _ncols;
    addr += col;
    return addr;
}

template <class T>
void System<T>::getLocation(AddrT addr, int &chip_idx, int &tile_idx, int &block_idx, int &row_idx, int &col_idx)
{
    /* Here is the code for memory mapping
     * */
    col_idx = addr % _ncols;
    addr /= _ncols;
    row_idx = addr % _nrows;
    addr /= _nrows;
    block_idx = addr % _nblocks;
    addr /= _nblocks;
    tile_idx = addr % _ntiles;
    addr /= _ntiles;
    chip_idx = addr % _nchips;
}

template <class T>
void System<T>::getLocation(AddrT addr, int &chip_idx, int &tile_idx, int &block_idx)
{
    /* Here is the code for memory mapping
     * */
    addr /= _blocksize;
    tile_idx = addr % _nblocks;
    addr /= _nblocks;
    block_idx = addr % _ntiles;
    addr /= _ntiles;
    chip_idx = addr % _nchips;
}

template <class T>
int System<T>::sendPIM_one_operand(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i++) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addAddr(req.addr_list[i], req.size_list[i], req.precision_list[i]);
        pim_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res = _chips[src_chip]->receiveReq(*pim_req);

	    delete pim_req;
    }
    return tot_clks;
}

template <class T>
int System<T>::sendPIM_two_operands(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;
      
        // First address is considered as src1.
        // Second address is the dst.
        // src2 isn't specified because it isn't required to model performance (time) and power (energy)
        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col); 
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addAddr(req.addr_list[i], req.size_list[i], req.precision_list[i]);
        pim_req->addAddr(req.addr_list[i+1], req.size_list[i+1], req.precision_list[i+1]);
        pim_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        pim_req->setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        pim_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        //This queues the request into the queue of the controller.
        //Ordinarily, this method returns True (indicating that queueing is successful)
        //But when the queues hold more than 256 outstanding requests,
        //then this will return False. Then we will go into the while loop
        //And spend some cycles waiting here to put requests into the queue
        //The cycle spending happens via the tick().
        bool res = _chips[src_chip]->receiveReq(*pim_req);
	    delete pim_req;
    }
    return tot_clks;
}

template <class T>
int System<T>::sendRF_one_operand(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i++) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addAddr(req.addr_list[i], req.size_list[i], req.precision_list[i]);
        pim_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res = _chips[src_chip]->receiveReq(*pim_req);

	    delete pim_req;
    }
    return tot_clks;
}

template <class T>
int System<T>::sendRF_two_operands(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;
      
        // First address is considered as src1.
        // Second address is the dst.
        // src2 isn't specified because it isn't required to model performance (time) and power (energy)
        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col); 
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addAddr(req.addr_list[i], req.size_list[i], req.precision_list[i]);
        pim_req->addAddr(req.addr_list[i+1], req.size_list[i+1], req.precision_list[i+1]);
        pim_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        pim_req->setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        pim_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        //This queues the request into the queue of the controller.
        //Ordinarily, this method returns True (indicating that queueing is successful)
        //But when the queues hold more than 256 outstanding requests,
        //then this will return False. Then we will go into the while loop
        //And spend some cycles waiting here to put requests into the queue
        //The cycle spending happens via the tick().
        bool res = _chips[src_chip]->receiveReq(*pim_req);
	    delete pim_req;
    }
    return tot_clks;
}

/*
//I don't think this function will be used standalone
template <class T>
int System<T>::system_sendChipReq(Request& req, int para)
{
    int tot_clks = 0;

    tot_clks = sendChipReq(req, para);

    return tot_clks;
}
*/

template <class T>
int System<T>::sendChipReq(Request& req, int para)
{
    int tot_clks = 0;

    int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
    int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;

    Request* inter_tile_req = new Request(req.type);

    if (para == SEND) {
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        inter_tile_req->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        inter_tile_req->addAddr(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        inter_tile_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        inter_tile_req->setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        //setLocation is used to tell which tile owns this request
        inter_tile_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
    } 
    else if (para == RECEIVE) {
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);

        inter_tile_req->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        inter_tile_req->addAddr(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        inter_tile_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        inter_tile_req->setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        //setLocation is used to tell which tile owns this request
        inter_tile_req->setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
    }
    else {
        std::cout<<"Unsupported request type/direction"<<std::endl;
        assert(0);
    }


    tot_clks++;
    bool res = _chips[src_chip]->receiveReq(*inter_tile_req);

    delete inter_tile_req;

    return tot_clks;
}

/*
//I don't think this function will be used standalone
template <class T>
int System<T>::system_sendTileReq(Request& req, int para)
{
    int tot_clks = 0;

    tot_clks = sendTileReq(req, para);

    return tot_clks;
}
*/

template <class T>
int System<T>::sendTileReq(Request& req, int para)
{
    int tot_clks = 0;

    int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
    int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;

    Request *inter_block_req = new Request(req.type);
    if (para == SEND_RECEIVE) {
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

        assert(src_chip==dst_chip);
        assert(src_tile==dst_tile);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        inter_block_req->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        inter_block_req->addAddr(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        inter_block_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        inter_block_req->setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        inter_block_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
    }
    else {
        std::cout<<"Unsupported request type/direction"<<std::endl;
        assert(0);
    }

    tot_clks++;
    bool res = _chips[src_chip]->receiveReq(*inter_block_req);
    delete inter_block_req;

    return tot_clks;
}

template <class T>
int System<T>::sendSyncReq(Request& req)
{
    int tot_clks = 0;

    int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;

    if (req.type == Request::Type::Barrier) {
        //We just send a req and a wait back to back
        Request *sync_req1 = new Request(Request::Type::Signal, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req1->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req1->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        sync_req1->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res1 = _chips[src_chip]->receiveReq(*sync_req1);
        delete sync_req1;

        Request *sync_req2 = new Request(Request::Type::Wait, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req2->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req2->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        sync_req2->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res2 = _chips[src_chip]->receiveReq(*sync_req2);
        delete sync_req2;
    }
    else {
        Request *sync_req = new Request(req.type, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req->addAddr(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req->setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        sync_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res = _chips[src_chip]->receiveReq(*sync_req);
        delete sync_req;
    }

    return tot_clks;
}

/*
//Wrapper function for sendPimReq. Basically, we add synchronization
//before and after calling sendPimReq.
template <class T>
int System<T>::system_sendPimReq(Request& req)
{
    int clks;
    clks = sendPimReq(req);
    return clks;
}
*/

//Send a PIM request (will get executed by a block)
//Returns number of clocks
template <class T>
int System<T>::sendPimReq(Request& req)
{
    int return_value = 0;
    switch (req.type) {
        case Request::Type::RowRead:
        case Request::Type::RowWrite:
        case Request::Type::ColRead:
        case Request::Type::ColWrite:
        case Request::Type::RowSet:
        case Request::Type::RowReset:
        case Request::Type::ColSet:
        case Request::Type::ColReset:
        case Request::Type::RowReduce:
        case Request::Type::RowShift:
            return_value =  sendPIM_one_operand(req);
            break;
        case Request::Type::RowMv:
        case Request::Type::RowAdd:
        case Request::Type::RowAdd_CRAM_RF:
        case Request::Type::RowSub:
        case Request::Type::RowMul:
        case Request::Type::RowMul_CRAM_RF:
        case Request::Type::RowDiv:
        case Request::Type::RowBitwise:
        case Request::Type::RowSearch:
        case Request::Type::ColMv:
        case Request::Type::ColAdd:
        case Request::Type::ColSub:
        case Request::Type::ColMul:
        case Request::Type::ColDiv:
        case Request::Type::ColBitwise:
        case Request::Type::ColSearch:
            return_value =  sendPIM_two_operands(req);
            break;
        default:
            cout << "Error: cannot handle non-PIM operations here!\n";
            break;
    }
    return return_value;
}

//Send a RF only request (will get executed by the RF block)
//Returns number of clocks
template <class T>
int System<T>::sendRFReq(Request& req)
{
    int return_value = 0;
    switch (req.type) {
        case Request::Type::RowRead_RF:
            return_value =  sendRF_one_operand(req);
            break;
        case Request::Type::RowAdd_RF:
        case Request::Type::RowSub_RF:
            return_value =  sendRF_two_operands(req);
            break;
        default:
            cout << "Error: cannot handle non-RF operations here!\n";
            break;
    }
    return return_value;
}

//This is the main API using which an application program will queue
//requests to be executed on the pimra accelerator
template <class T>
int System<T>::sendRequest(Request& req)
{
#ifdef DEBUG_OUTPUT
    // std::cout << "The system is sending a request - " ;
#endif
    int ticks = 0;
    tot_reqs++;
    switch (req.type) {
        case Request::Type::RowSet:
        case Request::Type::ColSet:
        case Request::Type::RowReset:
        case Request::Type::ColReset:
        case Request::Type::RowRead:
        case Request::Type::ColRead:
        case Request::Type::RowWrite:
        case Request::Type::ColWrite:
        case Request::Type::RowMv:
        case Request::Type::ColMv:
        case Request::Type::RowAdd:
        case Request::Type::RowAdd_CRAM_RF:
        case Request::Type::ColAdd:
        case Request::Type::RowSub:
        case Request::Type::ColSub:
        case Request::Type::RowMul:
        case Request::Type::RowMul_CRAM_RF:
        case Request::Type::RowDiv:
        case Request::Type::ColMul:
        case Request::Type::ColDiv:
        case Request::Type::RowBitwise:
        case Request::Type::ColBitwise:
        case Request::Type::RowSearch:
        case Request::Type::ColSearch:
        case Request::Type::RowReduce:
        case Request::Type::RowShift:
            ticks = sendPimReq(req);
            break;
        //case Request::Type::BlockSend:
        //    ticks = system_sendTileReq(req, SEND);
        //    break;
        //case Request::Type::BlockReceive:
        //    ticks = system_sendTileReq(req, RECEIVE);
        //    break;
        case Request::Type::BlockSend_Receive:
            ticks = sendTileReq(req, SEND_RECEIVE);
            break;
        case Request::Type::TileSend:
            ticks = sendChipReq(req, SEND);
            break;
        case Request::Type::TileReceive:
            ticks = sendChipReq(req, RECEIVE);
            break;
        case Request::Type::Signal:
            ticks = sendSyncReq(req);
            break;
        case Request::Type::Wait:
            ticks = sendSyncReq(req);
            break;
        case Request::Type::Barrier:
            ticks = sendSyncReq(req);
            break;
        case Request::Type::ResetSync:
            ticks = sendSyncReq(req);
            break;
        //case Request::Type::TileSend_Receive:
        //    ticks = system_sendChipReq(req, SEND_RECEIVE);
        //    break;
        //case Request::Type::ChipSend_Receive:
        //    ticks = sendNetReq(req);
        //    break;
        //case Request::Type::SystemRow2Row:
        //    ticks = system_sendRow_receiveRow(req);
        //    break;
        //case Request::Type::SystemRow2Col:
        //    ticks = system_sendRow_receiveCol(req);
        //    break;
        //case Request::Type::SystemCol2Row:
        //    ticks = system_sendCol_receiveRow(req);
        //    break;
        //case Request::Type::SystemCol2Col:
        //    ticks = system_sendCol_receiveCol(req);
        //    break;
        //case Request::Type::SystemLookUpTable:
        //    ticks = system_lookuptable(req);
        //    break;
        case Request::Type::RowStore:
            ticks = sendChipReq(req, SEND);
            break;
        case Request::Type::RowStore_RF:
            ticks = sendChipReq(req, SEND);
            break;
        case Request::Type::RowLoad:
            ticks = sendChipReq(req, RECEIVE);
            break;
        case Request::Type::RowLoad_RF:
            ticks = sendChipReq(req, RECEIVE);
            break;
        //case Request::Type::SystemRowStore:
        //    ticks = system_DramStore(req);
        //    break;
        //case Request::Type::SystemRowLoad:
        //    ticks = system_DramLoad(req);
        //    break;
        //case Request::Type::SystemColRead:
        //    ticks = system_ColRead(req);
        //    break;
        //case Request::Type::SystemColWrite:
        //    ticks = system_ColWrite(req);
        //    break;
        case Request::Type::RowRead_RF:
            ticks = sendRFReq(req);
            break;
        default:
            cout << "[Error] unrecognized request!\n";
            break;
    }
#ifdef SCHED_DEBUG_OUTPUT
    printf("The system receives a Request#%lu: %s, ticks: %d\n",
            tot_reqs, req.reqToStr().c_str(), ticks);
#endif
#ifdef DEBUG_OUTPUT
    // std::cout << req.reqToStr() << std::endl;
#endif
    if (ticks < 0) {
        std::cout << "Wrong Address!" << std::endl;
        exit(1);
    }

    return ticks;
}

template <class T>
int System<T>::sendRequests(std::vector<Request>& reqs)
{
#ifdef DEBUG_OUTPUT
    // std::cout << "The system is sending a request - " ;
#endif
    int ticks = 0;
    for (int i = 0; i < reqs.size(); i++) {
        tot_reqs++;
        switch (reqs[i].type) {
            case Request::Type::RowSet:
            case Request::Type::ColSet:
            case Request::Type::RowReset:
            case Request::Type::ColReset:
            case Request::Type::RowRead:
            case Request::Type::ColRead:
            case Request::Type::RowWrite:
            case Request::Type::ColWrite:
            case Request::Type::RowMv:
            case Request::Type::ColMv:
            case Request::Type::RowAdd:
            case Request::Type::RowAdd_CRAM_RF:
            case Request::Type::ColAdd:
            case Request::Type::RowSub:
            case Request::Type::ColSub:
            case Request::Type::RowMul:
            case Request::Type::RowMul_CRAM_RF:
            case Request::Type::RowDiv:
            case Request::Type::ColMul:
            case Request::Type::ColDiv:
            case Request::Type::RowBitwise:
            case Request::Type::ColBitwise:
            case Request::Type::RowSearch:
            case Request::Type::ColSearch:
            case Request::Type::RowReduce:
            case Request::Type::RowShift:
                ticks = sendPimReq(reqs[i]);
                break;
            //case Request::Type::BlockSend:
            //    ticks = system_sendTileReq(reqs[i], SEND);
            //    break;
            //case Request::Type::BlockReceive:
            //    ticks = system_sendTileReq(reqs[i], RECEIVE);
            //    break;
            case Request::Type::BlockSend_Receive:
                ticks = sendTileReq(reqs[i], SEND_RECEIVE);
                break;
            case Request::Type::TileSend:
                ticks = sendChipReq(reqs[i], SEND);
                break;
            case Request::Type::TileReceive:
                ticks = sendChipReq(reqs[i], RECEIVE);
                break;
            case Request::Type::Signal:
                ticks = sendSyncReq(reqs[i]);
                break;
            case Request::Type::Wait:
                ticks = sendSyncReq(reqs[i]);
                break;
            case Request::Type::Barrier:
                ticks = sendSyncReq(reqs[i]);
                break;
            case Request::Type::ResetSync:
                ticks = sendSyncReq(reqs[i]);
                break;
            //case Request::Type::TileSend_Receive:
            //    ticks = system_sendChipReq(reqs[i], SEND_RECEIVE);
            //    break;
            //case Request::Type::ChipSend_Receive:
            //    ticks = sendNetReq(reqs[i]);
            //    break;
            //case Request::Type::SystemRow2Row:
            //    ticks = system_sendRow_receiveRow(reqs[i]);
            //    break;
            //case Request::Type::SystemRow2Col:
            //    ticks = system_sendRow_receiveCol(reqs[i]);
            //    break;
            //case Request::Type::SystemCol2Row:
            //    ticks = system_sendCol_receiveRow(reqs[i]);
            //    break;
            //case Request::Type::SystemCol2Col:
            //    ticks = system_sendCol_receiveCol(reqs[i]);
            //    break;
            //case Request::Type::SystemLookUpTable:
            //    ticks = system_lookuptable(reqs[i]);
            //    break;
            case Request::Type::RowStore:
                ticks = sendChipReq(reqs[i], SEND);
                break;
            case Request::Type::RowStore_RF:
                ticks = sendChipReq(reqs[i], SEND);
                break;
            case Request::Type::RowLoad:
                ticks = sendChipReq(reqs[i], RECEIVE);
                break;
            case Request::Type::RowLoad_RF:
                ticks = sendChipReq(reqs[i], RECEIVE);
                break;
            //case Request::Type::SystemRowStore:
            //    ticks = system_DramStore(reqs[i]);
            //    break;
            //case Request::Type::SystemRowLoad:
            //    ticks = system_DramLoad(reqs[i]);
            //    break;
            //case Request::Type::SystemColRead:
            //    ticks = system_ColRead(reqs[i]);
            //    break;
            //case Request::Type::SystemColWrite:
            //    ticks = system_ColWrite(reqs[i]);
            //    break;
            case Request::Type::RowRead_RF:
                ticks = sendRFReq(reqs[i]);
                break;
            default:
                cout << "[Error] unrecognized request!\n";
                break;
        }
#ifdef SCHED_DEBUG_OUTPUT
        printf("The system receives a Request#%lu: %s, ticks: %d\n",
            tot_reqs, req.reqToStr().c_str(), ticks);
#endif
#ifdef DEBUG_OUTPUT
        // std::cout << req.reqToStr() << std::endl;
#endif
        if (ticks < 0) {
            std::cout << "Wrong Address!" << std::endl;
            exit(1);
        }
    }

    return ticks;
}

template <class T>
void System<T>::run()
{

    for (int i = 0; i < _nchips; i++) {
        while (!_chips[i]->isFinished())
            _chips[i]->tick();
        _chips[i]->outputStats(rstFile);
    }

}

template <class T>
void System<T>::finish()
{

    fprintf(rstFile, "\n############# Summary #############\n");
    for (int i = 0; i < _nchips; i++) {
        fprintf(rstFile, "--------------------------------\n");
        //fprintf(rstFile, "Chip#%d has ticked %lu clocks\n", i, _chips[i]->getTime());
        //fprintf(rstFile, "Chip#%d has processed %lu instructions\n", i, _chips[i]->getDecoderTime());
        //Commenting out energy for now because any code related to energy hasn't been updated yet
        //fprintf(rstFile, "Chip#%d has consumed %.4lf nj energy\n", i, _chips[i]->getTotalEnergy());
        //fprintf(rstFile, "Chip#%d leakage power is %.4lf W\n", i, _chips[i]->getTotalLeakageEnergy());
        fprintf(rstFile, "--------------------------------\n");

        for (int j = 0; j < _chips[i]->_nchildren; j++) {
            fprintf(rstFile, "Chip#%d Tile#%d has ticked %lu clocks\n", i, j, _chips[i]->_children[j]->getTime());
            fprintf(rstFile, "--------------------------------\n");

            //for (int k = 0; k < _chips[i]->_children[j]->_nchildren; k++) {
            //    fprintf(rstFile, "Chip#%d Tile#%d Block#%d has ticked %lu clocks\n", i, j, k, _chips[i]->_children[j]->_children[k]->getTime());
            //}
            fprintf(rstFile, "--------------------------------\n");

        }
    }
}

/*
template <class T>
int System<T>::system_sendRow_receiveRow(Request& req) {
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile = 0, dst_block = 0, dst_row = 0, dst_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_col + req.size_list[i] > _ncols) || (dst_col + req.size_list[i+1] > _ncols)) {
            return -1;
        }

        if (src_chip != dst_chip) {

            //We dont support this im PIMRA anymore.
            //Only data transfer between blocks across tiles is supported.
            cout<<"Unsupported code";
            assert(0);

            Request read_req(Request::Type::RowRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_req(Request::Type::BlockSend);
            block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_req, SEND);

            Request tile_send_req(Request::Type::TileSend);
            tile_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            tile_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            tile_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendChipReq(tile_send_req, SEND);

            Request chip_send_receive_req(Request::Type::ChipSend_Receive);
            chip_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            chip_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            chip_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            chip_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            chip_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendNetReq(chip_send_receive_req);

            Request tile_receive_req(Request::Type::TileReceive);
            tile_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            tile_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tile_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendChipReq(tile_receive_req, RECEIVE);

            Request block_receive_req(Request::Type::BlockReceive);
            block_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendTileReq(block_receive_req, RECEIVE);

            Request write_req(Request::Type::RowWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);

        } else if (src_chip == dst_chip && src_tile != dst_tile) {

            Request read_req(Request::Type::RowRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_req(Request::Type::BlockSend);
            block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_req, SEND);

            Request tile_send_receive_req(Request::Type::TileSend_Receive);
            tile_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            tile_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            tile_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            tile_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tile_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendChipReq(tile_send_receive_req, SEND_RECEIVE);

            Request block_receive_req(Request::Type::BlockReceive);
            block_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendTileReq(block_receive_req, RECEIVE);

            Request write_req(Request::Type::RowWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);

        } else if (src_chip == dst_chip && src_tile == dst_tile && src_block != dst_block) {

            Request read_req(Request::Type::RowRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_receive_req(Request::Type::BlockSend_Receive);
            block_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_receive_req, SEND_RECEIVE);

            Request write_req(Request::Type::RowWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);

        } else if (src_chip == dst_chip && src_tile == dst_tile && src_block == dst_block) {

            //We dont support this anymore.
            //Only data transfer between blocks across tiles is supported.
            cout<<"Unsupported code";
            assert(0);

            if (dst_col != src_col) {
                Request mv_col_req(Request::Type::ColMv);
                mv_col_req.addAddr(req.addr_list[i], req.size_list[i]);
                mv_col_req.addAddr(req.addr_list[i]+(dst_col-src_col), req.size_list[i]);
                mv_col_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
                mv_col_req.setDstLocation(dst_chip, dst_tile, dst_block, src_row, dst_col);
                mv_col_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
                tot_clks += sendPimReq(mv_col_req);
            }

            Request mv_row_req(Request::Type::RowMv);
            mv_row_req.addAddr(req.addr_list[i]+(dst_col-src_col), req.size_list[i]);
            mv_row_req.addAddr(req.addr_list[i+1], req.size_list[i]);
            mv_row_req.setSrcLocation(src_chip, src_tile, src_block, src_row, dst_col);
            mv_row_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            mv_row_req.setLocation(src_chip, src_tile, src_block, src_row, dst_col);
            tot_clks += sendPimReq(mv_row_req);
        }
    }
    return tot_clks;
}


template <class T>
int System<T>::system_sendRow_receiveCol(Request& req) {
    int tot_clks = 0;
    //This is not supported in PIMRA
    cout<<"Unsupported code";
    assert(0);
    return tot_clks;
}

template <class T>
int System<T>::system_sendCol_receiveRow(Request& req) {
    int tot_clks = 0;
    //This is not supported in PIMRA
    cout<<"Unsupported code";
    assert(0);
    return tot_clks;
}

template <class T>
int System<T>::system_sendCol_receiveCol(Request& req) {
    int tot_clks = 0;

    //This is not supported in PIMRA
    cout<<"Unsupported code";
    assert(0);

    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile = 0, dst_block = 0, dst_row = 0, dst_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_row + req.size_list[i] > _nrows) || (dst_row + req.size_list[i+1] > _nrows)) {
            return -1;
        }

        if (src_chip != dst_chip) {
            Request read_req(Request::Type::ColRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_req(Request::Type::BlockSend);
            block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_req, SEND);

            Request tile_send_req(Request::Type::TileSend);
            tile_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            tile_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            tile_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendChipReq(tile_send_req, SEND);

            Request chip_send_receive_req(Request::Type::ChipSend_Receive);
            chip_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            chip_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            chip_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            chip_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            chip_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendNetReq(chip_send_receive_req);

            Request tile_receive_req(Request::Type::TileReceive);
            tile_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            tile_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tile_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendChipReq(tile_receive_req, RECEIVE);

            Request block_receive_req(Request::Type::BlockReceive);
            block_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendTileReq(block_receive_req, RECEIVE);

            Request write_req(Request::Type::ColWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);

        } else if (src_chip == dst_chip && src_tile != dst_tile) {
            Request read_req(Request::Type::ColRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_req(Request::Type::BlockSend);
            block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_req, SEND);

            Request tile_send_receive_req(Request::Type::TileSend_Receive);
            tile_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            tile_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            tile_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            tile_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tile_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendChipReq(tile_send_receive_req, SEND_RECEIVE);

            Request block_receive_req(Request::Type::BlockReceive);
            block_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_receive_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendTileReq(block_receive_req, RECEIVE);

            Request write_req(Request::Type::ColWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);
        } else if (src_chip == dst_chip && src_tile == dst_tile && src_block != dst_block) {
            Request read_req(Request::Type::ColRead);
            read_req.addAddr(req.addr_list[i], req.size_list[i]);
            read_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            read_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendPimReq(read_req);

            Request block_send_receive_req(Request::Type::BlockSend_Receive);
            block_send_receive_req.addAddr(req.addr_list[i], req.size_list[i]);
            block_send_receive_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            block_send_receive_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
            block_send_receive_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            block_send_receive_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
            tot_clks += sendTileReq(block_send_receive_req, SEND_RECEIVE);

            Request write_req(Request::Type::ColWrite);
            write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
            write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            tot_clks += sendPimReq(write_req);
        } else if (src_chip == dst_chip && src_tile == dst_tile && src_block == dst_block) {
            if (dst_row != src_row) {
                Request mv_row_req(Request::Type::RowMv);
                mv_row_req.addAddr(req.addr_list[i], req.size_list[i]);
                mv_row_req.addAddr(req.addr_list[i]+(dst_row-src_row), req.size_list[i]);
                mv_row_req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
                mv_row_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, src_col);
                mv_row_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
                tot_clks += sendPimReq(mv_row_req);
            }

            Request mv_col_req(Request::Type::ColMv);
            mv_col_req.addAddr(req.addr_list[i]+(dst_col-src_col), req.size_list[i]);
            mv_col_req.addAddr(req.addr_list[i+1], req.size_list[i]);
            mv_col_req.setSrcLocation(src_chip, src_tile, src_block, dst_row, src_col);
            mv_col_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
            mv_col_req.setLocation(src_chip, src_tile, src_block, src_row, dst_col);
            tot_clks += sendPimReq(mv_col_req);
        }
    }
    return tot_clks;
}

template <class T>
int System<T>::system_DramStore(Request& req) {
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i=i+2) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile = 0, dst_block = 0, dst_row = 0, dst_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_col + req.size_list[i] > _ncols) || (dst_col + req.size_list[i+1] > _ncols)) {
            return -1;
        }

        //Only support single chip for now
        assert(src_chip==dst_chip);

        Request write_req(Request::Type::RowRead);
        write_req.addAddr(req.addr_list[i], req.size_list[i]);
        write_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        write_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendPimReq(write_req);

        Request block_send_req(Request::Type::BlockSend);
        block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        block_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendTileReq(block_send_req, SEND);

        //This is not required. The tile-to-tile interconnect basically becomes tile-to-dram
        //interconnect in this case, and it's time is already included in RowStore.
        //Request tile_send_req(Request::Type::TileSend);
        //tile_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        //tile_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        //tile_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        //tot_clks += sendChipReq(tile_send_req, SEND);

        Request dram_store_req(Request::Type::RowStore);
        dram_store_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
        dram_store_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        dram_store_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        tot_clks += sendChipReq(dram_store_req, RECEIVE);

    }
    return tot_clks;
}


template <class T>
int System<T>::system_DramLoad(Request& req) {
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i=i+2) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile = 0, dst_block = 0, dst_row = 0, dst_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        getLocation(req.addr_list[i+1], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_col + req.size_list[i] > _ncols) || (dst_col + req.size_list[i+1] > _ncols)) {
            return -1;
        }

        //Only support single chip for now
        assert(src_chip==dst_chip);

        Request dram_load_req(Request::Type::RowLoad);
        dram_load_req.addAddr(req.addr_list[i], req.size_list[i]);
        dram_load_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        dram_load_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendChipReq(dram_load_req, SEND);

        //This is not required. The tile-to-tile interconnect basically becomes tile-to-dram
        //interconnect in this case, and it's time is already included in RowLoad.
        //Request tile_send_req(Request::Type::TileReceive);
        //tile_send_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
        //tile_send_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        //tile_send_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        //tot_clks += sendChipReq(tile_send_req, RECEIVE);

        Request block_send_req(Request::Type::BlockReceive);
        block_send_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
        block_send_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        block_send_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        tot_clks += sendTileReq(block_send_req, RECEIVE);

        Request write_req(Request::Type::RowWrite);
        write_req.addAddr(req.addr_list[i+1], req.size_list[i+1]);
        write_req.setDstLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        write_req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
        tot_clks += sendPimReq(write_req);

    }
    return tot_clks;
}

template <class T>
int System<T>::system_ColRead(Request& req) {
    int tot_clks = 0;

    //This is not supported in PIMRA
    cout<<"Unsupported code";
    assert(0);

    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i++) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_row + req.size_list[i] > _nrows)) {
            return -1;
        }

        Request write_req(Request::Type::ColRead);
        write_req.addAddr(req.addr_list[i], req.size_list[i]);
        write_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        write_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendPimReq(write_req);

        Request block_send_req(Request::Type::BlockSend);
        block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        block_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendTileReq(block_send_req, SEND);

        Request tile_send_req(Request::Type::TileSend);
        tile_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        tile_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        tile_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendChipReq(tile_send_req, SEND);
    }
    return tot_clks;
}

template <class T>
int System<T>::system_ColWrite(Request& req) {
    int tot_clks = 0;

    //This is not supported in PIMRA
    cout<<"Unsupported code";
    assert(0);

    int n_ops = req.addr_list.size();
    for (int i = 0; i < n_ops; i++) {
        int src_chip = 0, src_tile = 0, src_block = 0, src_row = 0, src_col = 0;

        getLocation(req.addr_list[i], src_chip, src_tile, src_block, src_row, src_col);
        req.setSrcLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        if ((src_row + req.size_list[i] > _nrows)) {
            return -1;
        }

        Request tile_send_req(Request::Type::TileReceive);
        tile_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        tile_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        tile_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendChipReq(tile_send_req, RECEIVE);

        Request block_send_req(Request::Type::BlockReceive);
        block_send_req.addAddr(req.addr_list[i], req.size_list[i]);
        block_send_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        block_send_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendTileReq(block_send_req, RECEIVE);

        Request write_req(Request::Type::ColWrite);
        write_req.addAddr(req.addr_list[i], req.size_list[i]);
        write_req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        write_req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
        tot_clks += sendPimReq(write_req);
    }
    return tot_clks;
}
*/

/////////////////////////////////////////////////////////////
// Queuing requests for tile0
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::gemv_tile0() {

    std::vector<Request> requests;
    Request *request;

    //Load weight matrix values into CRAM
    //This is typically done beforehand/offline.
    //Each core is loading separate data, so no broadcasting here.
    //No need for synchronization either because different
    //cores load data that only they will need/use.
    //4 elements in each column need to be loaded.
    for (int i=0; i<4; i++) {
        request = new Request(Request::Type::RowLoad);
        request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);
    }

    //Now load vector values into RF.
    //Only 4 elements need to be loaded per core, so just 1 instruction is enough.
    request = new Request(Request::Type::RowLoad_RF);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    //Initialize rows that'll hold the accumulator (accumulator size=16)
    request = new Request(Request::Type::RowReset);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //4 multiplications and additions into the accumulator
    for (int i=0; i<4; i++) {
        //Read an element from the RF
        request = new Request(Request::Type::RowRead_RF);
        request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT4); //src
        requests.push_back(*request);

        //Multiply in this core/tile.
        request = new Request(Request::Type::RowMul_CRAM_RF);
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);

        //Add the result of multiplication into the accumulator
        request = new Request(Request::Type::RowAdd);
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
        request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT16); //dst
        requests.push_back(*request);
    }

    //Send partial results to tile1
    request = new Request(Request::Type::TileSend);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);
}

/////////////////////////////////////////////////////////////
// Queuing requests for tile1
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::gemv_tile1() {

    std::vector<Request> requests;
    Request *request;

    //Load weight matrix values into CRAM
    //This is typically done beforehand/offline.
    //Each core is loading separate data, so no broadcasting here.
    //No need for synchronization either because different
    //cores load data that only they will need/use.
    //4 elements in each column need to be loaded.
    for (int i=0; i<4; i++) {
        request = new Request(Request::Type::RowLoad);
        request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);
    }

    //Now load vector values into RF.
    //Only 4 elements need to be loaded per core, so just 1 instruction is enough.
    request = new Request(Request::Type::RowLoad_RF);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(rf_base_addr_tile1, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    //Initialize rows that'll hold the accumulator (accumulator size=16)
    request = new Request(Request::Type::RowReset);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //4 multiplications and additions into the accumulator
    for (int i=0; i<4; i++) {
        //Read an element from the RF
        request = new Request(Request::Type::RowRead_RF);
        request->addAddr(rf_base_addr_tile1, 0, PrecisionT::INT4); //src
        requests.push_back(*request);

        //Multiply in this core/tile.
        request = new Request(Request::Type::RowMul_CRAM_RF);
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);

        //Add the result of multiplication into the accumulator
        request = new Request(Request::Type::RowAdd);
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT16); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
        requests.push_back(*request);
    }

    //Receive partial results from tile0
    request = new Request(Request::Type::TileReceive);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //Now add partial results from the two cores/tiles
    request = new Request(Request::Type::RowAdd);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //Store results into DRAM
    request = new Request(Request::Type::RowStore);
    request->addAddr(cram_addr_tile1_block3_row8, 0, PrecisionT::INT16); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);

}

/////////////////////////////////////////////////////////////
// Simple program to perform a matrix-vector mul 
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::gemv()
{

    //We have 2 tiles
    //Each has 4 CRAMs.
    //So total columns = 4 * 256 = 1024
    //Let's consider matrix size to be 1024 rows * 8 columns.
    //Let's consider vector size to be 8 rows * 1 column.

    //We will load 4 columns of matrix into tile0
    //and 4 columns of matrix into tile1.
    //That is, 4 elements in one column of each CRAM.

    //The vector is outside in RF.
    //4 elements in 1 core's RF, 4 in second core's RF.

    //We will perform 4 multiply-adds in each tile.
    //Then we will send data from tile0 to tile1.
    //And add them up in tile1.
    //This is the result (1024x1 vector).
    //One element of the result is in each column of tile1's CRAMs.
    //Then store results in DRAM.
    gemv_tile0();
    gemv_tile1();
}


/////////////////////////////////////////////////////////////
// Queuing requests for tile0
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::fir_tile0()
{
    std::vector<Request> requests;
    Request *request;

    //Load filter coefficients into the RF.
    //This step is typically done beforehand/offline.
    //Also, RFs in all cores will have the same coefficients.
    //So, we can brodcast.
    //When we are broadcasting though, other tiles shouldn't do anything.
    //So they will wait for a semaphore.
    //Assume the number of weights is less than rf_chunk_size, we 
    //only need 1 rowload_rf instruction.
    request = new Request(Request::Type::RowLoad_RF, Request::BroadcastType::ALL);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    //Signal that the load of filter coefficients from DRAM
    //to RFs of all tiles is complete.
    request = new Request(Request::Type::Signal, m1);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    //Now we load inputs from DRAM
    //Currently, we're assuming just one load is enough.
    //Only one input in each column across all cores.
    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    //Initialize rows that'll hold the accumulator (accumulator size=16)
    request = new Request(Request::Type::RowReset);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //Loop over the following set of instructions N times,
    //where N is the number of filter coefficients
    for (int i=0; i< 8; i++) {

        //Read the coefficient we want to multiply with from the RF.
        //This is not required if we make the RF out of flops.
        //Well, we can handle that internally (if we implement using flops, we
        //can make the cycles consumed by this Request to be 0)
        request = new Request(Request::Type::RowRead_RF);
        request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT16); //src
        requests.push_back(*request);

        //Multiply filter coefficient (in RF) with the inputs stored in CRAM.
        //Product will be stored in some rows
        request = new Request(Request::Type::RowMul_CRAM_RF);
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT8); //dst
        requests.push_back(*request);

        //Now, add the product generated above into the accumulator rows
        request = new Request(Request::Type::RowAdd);
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
        requests.push_back(*request);

        //Shift the inputs by 1 column to the left.
        //No need to loop for each bit. That is a part of the RowShift instruction's semantics.
        request = new Request(Request::Type::RowShift);
        request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);

    }

    //Now store the results back into DRAM
    //Only 1 result is present per column.
    //So, just 1 RowStore instruction is enough.
    request = new Request(Request::Type::RowStore);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);
}

/////////////////////////////////////////////////////////////
// Queuing requests for tile1
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::fir_tile1()
{
    std::vector<Request> requests;
    Request *request;

    //Wait until the broadcast of filter coefficients
    request = new Request(Request::Type::Wait, m1);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    //Now we load inputs from DRAM
    //Currently, we're assuming just one load is enough.
    //Only one input in each column across all cores.
    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    //Initialize rows that'll hold the accumulator (accumulator size=16)
    request = new Request(Request::Type::RowReset);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    //Loop over the following set of instructions N times,
    //where N is the number of filter coefficients
    for (int i=0; i< 8; i++) {

        //Read the coefficient we want to multiply with from the RF.
        //This is not required if we make the RF out of flops.
        //Well, we can handle that internally (if we implement using flops, we
        //can make the cycles consumed by this Request to be 0)
        request = new Request(Request::Type::RowRead_RF);
        request->addAddr(rf_base_addr_tile1, 0, PrecisionT::INT16); //src
        requests.push_back(*request);

        //Multiply filter coefficient (in RF) with the inputs stored in CRAM.
        //Product will be stored in some rows
        request = new Request(Request::Type::RowMul_CRAM_RF);
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);

        //Now, add the product generated above into the accumulator rows
        request = new Request(Request::Type::RowAdd);
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);

        //Shift the inputs by 1 column to the left.
        //No need to loop for each bit. That is a part of the RowShift instruction's semantics.
        request = new Request(Request::Type::RowShift);
        request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
        request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
        requests.push_back(*request);
    }

    //Now store the results back into DRAM
    //Only 1 result is present per column.
    //So, just 1 RowStore instruction is enough.
    request = new Request(Request::Type::RowStore);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT16); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT16); //dst
    requests.push_back(*request);

    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);
}

/////////////////////////////////////////////////////////////
// Simple program to perform an FIR filter
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::fir()
{
    //We have 2 tiles
    //Each has 4 CRAMs.
    //There are 256 columns in each CRAM.
    //So, total columns is 1024.
    //Let's consider the input length to be 1024*2 = 2048 (minus some halo).
    //And filter size of say 8 .

    //The coefficients are outside in RF (same coefficients are broadcasted in all cores).
    //The input is loaded into CRAMs.
    //Just one element per column of each CRAM.

    //The result's length will be 2048 (minus halo).
    //One element in each column of each CRAM.
    fir_tile0();
    fir_tile1();
}

/////////////////////////////////////////////////////////////
// Queuing requests for tile0
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::test_tile0()
{
    std::vector<Request> requests;
    Request *request;

    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowMul);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowAdd);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::Signal, m1);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::BlockSend_Receive);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::Barrier, m2);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::RowStore);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowMul);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowLoad_RF);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowStore_RF);
    request->addAddr(rf_base_addr_tile0, 0, PrecisionT::INT4); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowMul_CRAM_RF);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile0_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowAdd_CRAM_RF);
    request->addAddr(cram_addr_tile0_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);
}

/////////////////////////////////////////////////////////////
// Queuing requests for tile1
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::test_tile1()
{
    std::vector<Request> requests;
    Request *request;

    request = new Request(Request::Type::RowAdd);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT8); //src
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT8); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowMul);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowShift);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::Wait, m1);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::RowStore);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::Barrier, m2);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    requests.push_back(*request);

    request = new Request(Request::Type::RowMul);
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row8, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);

    request = new Request(Request::Type::RowLoad);
    request->addAddr(DRAM_ADDR, 0, PrecisionT::INT4); //src
    request->addAddr(cram_addr_tile1_block0_row0, 0, PrecisionT::INT4); //dst
    requests.push_back(*request);


    for (unsigned int i = 0; i < requests.size(); i++)
        sendRequest(requests[i]);
}

/////////////////////////////////////////////////////////////
// Simple program to perform an test filter
/////////////////////////////////////////////////////////////
template <class T>
void System<T>::test()
{
    test_tile0();
    test_tile1();
}

/*
template <class T>
void System<T>::matrix_mul_area_optimized(int A_row, int A_col, int B_row, int B_col)
{
}

template <class T>
void System<T>::matrix_mul_time_optimized(int A_row, int A_col, int B_row, int B_col)
{
    AddrT storage_start_address =  (AddrT)_ncols *_nrows * _nblocks * _ntiles / 4 * 3; // use the last 3/4 for storage units

    AddrT storage_start_address_A =  storage_start_address;
    AddrT storage_start_address_B =  storage_start_address_A + A_row * A_col * 32;
    AddrT storage_start_address_C =  storage_start_address_B + B_row * B_col * 32;

    AddrT pim_start_address = 0;
    AddrT block_size = _nrows * _ncols;
    std::vector<Request> requests;
    Request *request;

    for (int i = 0; i < A_row; i++) {

        request = new Request(Request::Type::SystemRow2Row);
        for (int j = 0; j < B_col; j++) {
            for (int m = 0; m < A_col; m++) {
                request->addAddr(storage_start_address_A + (i * A_col + m) * _datatype, _datatype);
                request->addAddr(pim_start_address + j * block_size + m * _ncols, _datatype);
                request->addAddr(storage_start_address_B + (j * B_row + m) * _datatype, _datatype);
                request->addAddr(pim_start_address + j * block_size + m * _ncols + _datatype, _datatype);
            }
        }
        requests.push_back(*request);


        request = new Request(Request::Type::RowMul);
        for (int j = 0; j < B_col; j++) {
            for (int m = 0; m < A_col; m++) {
                request->addAddr(pim_start_address + j * block_size + m * _ncols, _datatype);
                request->addAddr(pim_start_address + j * block_size + m * _ncols + _datatype, _datatype);
            }
        }
        requests.push_back(*request);

        int size = log2(A_col);
        for (int exp = 1; exp < size; exp++) {

            request = new Request(Request::Type::ColMv);
            for (int j = 0; j < B_col; j++) {
                for (int m = 0; m < A_col; m += pow(2, exp)) {
                    AddrT shift_address = pim_start_address + j * block_size + (m + pow(2, exp - 1) )* _ncols;
                    request->addAddr(shift_address, _datatype);
                    request->addAddr(shift_address + _datatype * (exp - 1), _datatype);
                }
            }
            requests.push_back(*request);

            request = new Request(Request::Type::RowMv);
            for (int j = 0; j < B_col; j++) {
                for (int m = 0; m < A_col; m += pow(2, exp)) {
                    AddrT src_addr = pim_start_address + j * block_size + (m + pow(2, exp - 1) ) * _ncols;
                    AddrT dst_addr = pim_start_address + j * block_size + m * _ncols;
                    request->addAddr(src_addr + _datatype * (exp - 1),  _datatype);
                    request->addAddr(dst_addr + _datatype * (exp - 1),  _datatype);
                }
            }
            requests.push_back(*request);

            request = new Request(Request::Type::RowAdd);
            for (int j = 0; j < B_col; j++) {
                for (int m = 0; m < A_col; m += pow(2, exp)) {
                    AddrT addr = pim_start_address + j * block_size + m * _ncols;
                    request->addAddr(addr, _datatype);
                    request->addAddr(addr + _datatype * (exp - 1),  _datatype);
                }
            }
            requests.push_back(*request);

        }

        request = new Request(Request::Type::SystemRow2Row); // Send S back to storage units
        for (int j = 0; j < B_col; j++) {
            request->addAddr(pim_start_address, _datatype);
            request->addAddr(storage_start_address_C + (i * A_row + j) * _datatype, _datatype);
        }
        requests.push_back(*request);
    }

//    sendRequests(requests); // cannot do this due to data dependency

//    std::cout << requests.size() << std::endl;
    for (unsigned int i = 0; i < requests.size(); i++) {
//        std::cout << requests[i].reqToStr() << std::endl;
        sendRequest(requests[i]);
    }

}

template <class T>
void System<T>::matrix_mul_balanced(int A_row, int A_col, int B_row, int B_col)
{
}

*/
