#include "backend/System.h"
#include "backend/global.h"
#include <chrono>
#include <algorithm>
//#include <thread>
#include <array>
#include <sstream>
#include <iterator>

#define SEND 0
#define RECEIVE 1
#define SEND_RECEIVE 2

using namespace pimsim;
using namespace std;

System::System(Config* config) : _config(config)
{

    std::string csv_filename;
    rstFile = fopen(config->get_rstfile().c_str(), "w");
    //csv_filename = config->get_rstfile() + ".req_count.csv";
    //req_count_csv_file.open(csv_filename.c_str(), ios::out);
    //csv_filename = config->get_rstfile() + ".cycle.csv";
    //cycle_csv_file.open(csv_filename.c_str(), ios::out);
    csv_filename = config->get_rstfile() + ".states_count.csv";
    states_csv_file.open(csv_filename.c_str(),ios::out);
    csv_filename = config->get_rstfile() + ".reqs_stats.csv";
    reqs_csv_file.open(csv_filename.c_str(),ios::out);
    csv_filename = config->get_rstfile() + ".energy.csv";
    energy_csv_file.open(csv_filename.c_str(),ios::out);
    csv_filename = config->get_rstfile() + ".router_hops.csv";
    router_hops_csv_file.open(csv_filename.c_str(),ios::out);
    csv_filename = config->get_rstfile() + ".hdw_config.csv";
    hdw_config_file.open(csv_filename.c_str(),ios::out);
    //hdw_config_file<<"hdw configs"<<std::endl;
    csv_filename = config->get_rstfile() + ".app_param";
    app_param_file.open(csv_filename.c_str(),ios::out);
    csv_filename = config->get_rstfile() + ".alloc_log";
    alloc_log_file.open(csv_filename.c_str(),ios::out);

    if (config->get_mem_configuration() == "htree") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::HTree, _config);
    } else if (config->get_mem_configuration() == "mesh") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::Mesh, _config);
    } else if (config->get_mem_configuration() == "DynaMesh") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::DynaMesh, _config);
    } else if (config->get_mem_configuration() == "bus") {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::Bus, _config);
    } else {
        _values = new MemoryCharacteristics(MemoryCharacteristics::Configuration::Ideal, _config);
    }

    //cout<<"distributed dram: "<<_values->config->_dramDistributed<<std::endl;

    for (int i = 0; i < config->_nchips; i++) {
        MemoryChip* chip = new MemoryChip(_values, &finishedReqNo);
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
    m2 = new Mailbox(config->_ntiles);  //use as a barrier for all tiles
    m3 = new Mailbox();

    mapper = Mapper(config);
}

System::~System()
{
    fclose(rstFile);
    //cycle_csv_file.close();
    //req_count_csv_file.close();
    states_csv_file.close();
    reqs_csv_file.close();
    energy_csv_file.close();
    router_hops_csv_file.close();
    hdw_config_file.close();
    app_param_file.close();
    alloc_log_file.close();
}


//Form an address for the given tile, block, row
//Chip and row numbers are assumed to be 0.
//tile, block, row
AddrT System::getAddress(int tile, int block, int row)
{
    AddrT addr = 0;
    addr *= _config->_ntiles;
    addr += tile;
    addr *= _config->_nblocks;
    addr += block;
    addr *= _config->_nrows;
    addr += row;
    addr *= _config->_ncols;
    addr += 0;
    return addr;
}

AddrT System::getRFAddress(int tile, int rf_index){
    return _config->_num_regs_per_rf * tile + rf_index;
}

//Form an address for the given chip, tile, block, row, column
//chip, tile, block, row, col
AddrT System::getAddress(int chip, int tile, int block, int row, int col)
{
    AddrT addr = chip;
    addr *= _config->_ntiles;
    addr += tile;
    addr *= _config->_nblocks;
    addr += block;
    addr *= _config->_nrows;
    addr += row;
    addr *= _config->_ncols;
    addr += col;
    return addr;
}


void System::getLocation(AddrT addr, int &chip_idx, int &tile_idx, int &block_idx, int &row_idx, int &col_idx)
{
    /* Here is the code for memory mapping
     * */
    col_idx = addr % _config->_ncols;
    addr /= _config->_ncols;
    row_idx = addr % _config->_nrows;
    addr /= _config->_nrows;
    block_idx = addr % _config->_nblocks;
    addr /= _config->_nblocks;
    tile_idx = addr % _config->_ntiles;
    addr /= _config->_ntiles;
    chip_idx = addr % _config->_nchips;
}


void System::getLocation(AddrT addr, int &chip_idx, int &tile_idx, int &block_idx)
{
    /* Here is the code for memory mapping
     * */
    addr /= _config->_blocksize;
    tile_idx = addr % _config->_nblocks;
    addr /= _config->_nblocks;
    block_idx = addr % _config->_ntiles;
    addr /= _config->_ntiles;
    chip_idx = addr % _config->_nchips;
}


int System::sendPIM_one_operand(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    //for (int i = 0; i < n_ops; i++) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;

        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        pim_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res = _chips[src_chip]->receiveReq(*pim_req);

	    delete pim_req;
    //}
    return tot_clks;
}


int System::sendPIM_two_operands(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    //for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;
      
        // First address is considered as src1.
        // Third address is the dst.
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col); 
        getLocation(req.addr_list[2], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        pim_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        pim_req->addOperand(req.addr_list[2], req.size_list[2], req.precision_list[2]);
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
    //}
    return tot_clks;
}

int System::sendPIM_four_operands(Request& req)
{
    int tot_clks = 0;
    int n_ops = req.addr_list.size();
    //for (int i = 0; i < n_ops; i+=2) {
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;
      
        // First address is considered as src1.
        // Third address is the dst.
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col); 
        getLocation(req.addr_list[4], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        Request *pim_req = new Request(req.type);

        pim_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        pim_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        pim_req->addOperand(req.addr_list[2], req.size_list[2], req.precision_list[2]);
        pim_req->addOperand(req.addr_list[3], req.size_list[3], req.precision_list[3]);
        pim_req->addOperand(req.addr_list[4], req.size_list[4], req.precision_list[4]);
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
    //}
    return tot_clks;
}

#ifdef OLD
//operand is rf address
int System::sendRF_one_operand(Request& req)
{
    int chip_index = req.addr_list[0]/(_config->_ntiles*_config->_num_regs_per_rf);
    assert(chip_index<=_chips.size());
    int tile_index = (req.addr_list[0]%(_config->_ntiles*_config->_num_regs_per_rf))/_config->_num_regs_per_rf;
    assert(tile_index<=_chips[chip_index]->_children.size());
    Request* rf_req = new Request(req.type);
    rf_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
    //rf_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
    rf_req->setLocation(chip_index, tile_index, 0, 0, 0);
    //rf_req->precision_bits = _num_bits_per_reg*_num_regs_per_rf/_wordsize_dram;
    bool res = _chips[chip_index]->receiveReq(*rf_req);
    return res;


}

// rf arithmethics here. first operand is cram address. second is rf address
int System::sendRF_two_operands(Request& req)
{
    int chip_index, tile_index, block_index, row, col;
    getLocation(req.addr_list[0], chip_index, tile_index, block_index, row, col);
    assert(chip_index<=_chips.size());
    assert(tile_index<=_chips[chip_index]->_children.size());
    Request* rf_req = new Request(req.type);
    rf_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
    rf_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
    rf_req->setLocation(chip_index, tile_index, 0, 0, 0);
    //rf_req->precision_bits = _num_bits_per_reg*_num_regs_per_rf/_wordsize_dram;
    bool res = _chips[chip_index]->receiveReq(*rf_req);
    return res;
}
#endif

//load store send receive requests handled here
int System::sendChipReq(Request& req, int para)
{
    if(req.type == Request::Type::RowLoad_RF || req.type == Request::Type::RowStore_RF){
        //first operand is rf address, second operand is DRAM address
        int chip_index = req.addr_list[0]/(_config->_ntiles*_config->_num_regs_per_rf);
        assert(chip_index<=_chips.size());
        int tile_index = (req.addr_list[0]%(_config->_ntiles*_config->_num_regs_per_rf))/_config->_num_regs_per_rf;
        assert(tile_index<=_chips[chip_index]->_children.size());
        Request* rf_req = new Request(req.type);
        rf_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        rf_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
        rf_req->setLocation(chip_index, tile_index, 0, 0, 0);
        rf_req->bits = 1;
        //rf_req->precision_bits = _num_bits_per_reg*_num_regs_per_rf/_wordsize_dram;
        bool res = _chips[chip_index]->receiveReq(*rf_req);
        return res;
    }
    else if(req.type == Request::Type::TileSend_BroadCast){
        Request* inter_tile_req = new Request(req.type);
        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
        //req.setDstLocation(src_chip, src_tile, src_block, src_row, src_col);
        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
         inter_tile_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        //setLocation is used to tell which tile owns this request
        inter_tile_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
       
        bool res = _chips[src_chip]->receiveReq(*inter_tile_req);

        delete inter_tile_req;

        return res;
    }
    else if(req.type == Request::Type::TileReceive_BroadCast){
        Request* inter_tile_req = new Request(req.type);
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;
        getLocation(req.addr_list[0], dst_chip, dst_tile, dst_block, dst_row, dst_col);
        req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
         inter_tile_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        //setLocation is used to tell which tile owns this request
        inter_tile_req->setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);
       
        bool res = _chips[dst_chip]->receiveReq(*inter_tile_req);

        delete inter_tile_req;

        return res;
    }
    else{
        int tot_clks = 0;

        int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
        int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;

        Request* inter_tile_req = new Request(req.type);

        if (para == SEND) {
            getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
            getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

            req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

            inter_tile_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
            inter_tile_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
            inter_tile_req->bits = req.precision_list[0].bits();
            //setLocation is used to tell which tile owns this request
            inter_tile_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
        } 
        else if (para == RECEIVE) {
            getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
            getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

            req.setLocation(dst_chip, dst_tile, dst_block, dst_row, dst_col);

            inter_tile_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
            inter_tile_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
            inter_tile_req->bits = req.precision_list[0].bits();
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
}


int System::sendTileReq(Request& req, int para)
{
    int tot_clks = 0;

    int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;
    int dst_chip = 0, dst_tile= 0, dst_block= 0, dst_row = 0, dst_col = 0;

    Request *inter_block_req = new Request(req.type);
    if (para == SEND_RECEIVE) {
        if(req.type == Request::Type::BlockBroadCast){
                getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
                req.setLocation(src_chip, src_tile, src_block, src_row, src_col);
                inter_block_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
                inter_block_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
        }
        else{
            getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);
            getLocation(req.addr_list[1], dst_chip, dst_tile, dst_block, dst_row, dst_col);

            assert(src_chip==dst_chip);
            assert(src_tile==dst_tile);

            req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

            inter_block_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
            inter_block_req->addOperand(req.addr_list[1], req.size_list[1], req.precision_list[1]);
            inter_block_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);
        }
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


int System::sendSyncReq(Request& req)
{
    int tot_clks = 0;

    int src_chip = 0, src_tile= 0, src_block= 0, src_row = 0, src_col = 0;

    if (req.type == Request::Type::Barrier) {
        //We just send a req and a wait back to back
        Request *sync_req1 = new Request(Request::Type::Signal, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req1->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req1->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res1 = _chips[src_chip]->receiveReq(*sync_req1);
        delete sync_req1;

        Request *sync_req2 = new Request(Request::Type::Wait, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req2->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req2->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res2 = _chips[src_chip]->receiveReq(*sync_req2);
        delete sync_req2;
    }
    else {
        Request *sync_req = new Request(req.type, req.mail);
        getLocation(req.addr_list[0], src_chip, src_tile, src_block, src_row, src_col);

        req.setLocation(src_chip, src_tile, src_block, src_row, src_col);

        sync_req->addOperand(req.addr_list[0], req.size_list[0], req.precision_list[0]);
        sync_req->setLocation(src_chip, src_tile, src_block, src_row, src_col);

        tot_clks++;
        bool res = _chips[src_chip]->receiveReq(*sync_req);
        delete sync_req;
    }

    return tot_clks;
}

//Send a PIM request (will get executed by a block)
//Returns number of clocks

int System::sendPimReq(Request& req)
{
    int return_value = 0;
    switch (req.type) {
        case Request::Type::RowRead:
        case Request::Type::RowWrite:
        case Request::Type::RowSet:
        case Request::Type::RowReset:
        case Request::Type::RowReduce:
        case Request::Type::RowReduce_WithinTile:
        case Request::Type::RowShift:
        case Request::Type::ColBroadcast:
            return_value =  sendPIM_one_operand(req);
            break;
        case Request::Type::RowAdd:
        case Request::Type::RowAdd_CRAM_RF:
        case Request::Type::RowSub:
        case Request::Type::RowCompare:
        case Request::Type::RowMul:
        case Request::Type::RowMul_CRAM_RF:
        case Request::Type::RowBitwise:
            return_value =  sendPIM_two_operands(req);
            break;
        case Request::Type::RowDotProduct_CRAM_RF:
            return_value =  sendPIM_four_operands(req);
            break;
        default:
            cout << "Error: cannot handle non-PIM operations here!\n";
            break;
    }
    return return_value;
}

#ifdef OLD
//Send a RF only request (will get executed by the RF block)
//Returns number of clocks
int System::sendRFReq(Request& req)
{
    int return_value = 0;
    switch (req.type) {
        case Request::Type::RowRead_RF:
            return_value =  sendRF_one_operand(req);
            break;
        case Request::Type::RowAdd_RF:
        case Request::Type::RowSub_RF:
        case Request::Type::PopCountReduce_RF:
            return_value =  sendRF_two_operands(req);
            break;
        default:
            cout << "Error: cannot handle non-RF operations here!\n";
            break;
    }
    return return_value;
}
#endif

#ifdef NEW
void System::decode(Request& req, int& chip, int& tile){
    int block= 0, row = 0, col = 0;
    switch (req.type) {
        case Request::Type::RowLoad_RF:
        case Request::Type::RowStore_RF:
        case Request::Type::RowRead_RF:
            chip = req.addr_list[0]/(_config->_ntiles*_config->_num_regs_per_rf);
            assert(chip<=_chips.size());
            tile = (req.addr_list[0]%(_config->_ntiles*_config->_num_regs_per_rf))/_config->_num_regs_per_rf;
            assert(tile<=_chips[chip]->_children.size());
            req.setLocation(chip, tile, 0, 0, 0);
            req.bits = 1;
            break;
        case Request::Type::TileReceive:
            getLocation(req.addr_list[1], chip, tile, block, row, col);
            req.setLocation(chip, tile, block, row, col);
            req.bits = req.precision_list[0].bits();
            break;
        case Request::Type::Wait:
            getLocation(req.addr_list[1], chip, tile, block, row, col);
            req.setLocation(chip, tile, block, row, col);
            req.bits =1;
            break;
        default:
            getLocation(req.addr_list[0], chip, tile, block, row, col);
            req.setLocation(chip, tile, block, row, col);
            req.bits = req.precision_list[0].bits();
            break;
    }
}

//This is the main API using which an application program will queue
//requests to be executed on the pimra accelerator
bool System::sendRequest(Request& req)
{
    int chip = 0;
    int tile = 0;
    if (req.type == Request::Type::RowLoad ||
        req.type == Request::Type::RowStore ||
        req.type == Request::Type::RowLoad_RF) {
      for (auto &elem : req.precision_list) {
        if (!elem.isfloat) {
          int residue = elem.mantissa % 8;
          if (residue && 8 % elem.mantissa != 0)
            elem.mantissa += (8 - residue);
        }
      }
    }
    // for hardware ablation study: if shuffle is off, convert this request to a simple load/store of size ncol*nblock
    if(!_config->_shuffle_on && 
        (req.type == Request::Type::RowLoad ||
        req.type == Request::Type::RowStore)){
            if(req.broadcast_en || req.multicast_en){
                req.broadcast_en = false;
                req.multicast_en = false;
                req.size_list[0] = _config->_ncols*_config->_nblocks;
                req.size_list[1] = _config->_ncols*_config->_nblocks;
            }
    }
    if(!_config->_const_op_on &&
         _config->_const_op_kick_out_1_row && 
        (req.type == Request::Type::RowAdd_CRAM_RF || req.type == Request::Type::RowMul_CRAM_RF || req.type == Request::Type::RowDotProduct_CRAM_RF))
    {
        decode(req, chip, tile);
        int temp_chip=0;
        int temp_tile=0;

        bool success = true;
        //store request
        Request storeReq = Request(Request::Type::RowStore);
        storeReq.addOperand(getAddress(tile,0,0), _config->_ncols*_config->_nblocks, PrecisionT::INT1);//cram
        storeReq.addOperand(DRAM_ADDR,0, PrecisionT::INT1);//dram
        decode(storeReq, temp_chip, temp_tile);
        storeReq.reqNo = currReqNo;
        success = success && _chips[chip]->receiveReq(storeReq);
        currReqNo++;
        totalReqNo++;

        //compute request
        req.reqNo = currReqNo;
        success = success && _chips[chip]->receiveReq(req);
        currReqNo++;
        totalReqNo++;

        //load request
        Request loadReq = Request(Request::Type::RowLoad);
        loadReq.addOperand(getAddress(tile,0,0), _config->_ncols*_config->_nblocks, PrecisionT::INT1);//cram
        loadReq.addOperand(DRAM_ADDR,0, PrecisionT::INT1);//dram
        decode(loadReq, temp_chip, temp_tile);
        loadReq.reqNo = currReqNo;
        success = success && _chips[chip]->receiveReq(loadReq);
        currReqNo++;
        totalReqNo++;

        
        return success;      
    }
    // for hardware ablation study: if cross_cram_shift is off, convert this request to store + load
    if(!_config->_cross_cram_shift_on && req.type == Request::Type::RowShift){
        decode(req, chip, tile);
        int temp_chip=0;
        int temp_tile=0;

        bool success = true;

        Request storeReq = Request(Request::Type::RowStore);
        if(_config->_bypass_dram)
            storeReq.bypass_dram = true;
        storeReq.addOperand(getAddress(tile,0,0), _config->_ncols*_config->_nblocks, req.precision_list[0]);//cram
        storeReq.addOperand(DRAM_ADDR,0, req.precision_list[0]);//dram
        decode(storeReq, temp_chip, temp_tile);
        storeReq.reqNo = currReqNo;
        success = success && _chips[chip]->receiveReq(storeReq);
        currReqNo++;
        totalReqNo++;

        Request loadReq = Request(Request::Type::RowLoad);
        if(_config->_bypass_dram)
            loadReq.bypass_dram = true;
        loadReq.addOperand(getAddress(tile,0,0), _config->_ncols*_config->_nblocks, req.precision_list[0]);//cram
        loadReq.addOperand(DRAM_ADDR,0, req.precision_list[0]);//dram
        decode(loadReq, temp_chip, temp_tile);
        loadReq.reqNo = currReqNo;
        success = success && _chips[chip]->receiveReq(loadReq);
        currReqNo++;
        totalReqNo++;

        
        return success;
    }
    decode(req, chip, tile);
    req.reqNo = currReqNo;
    bool success = _chips[chip]->receiveReq(req);
    currReqNo++;
    totalReqNo++;
    return success;
}
#endif






void System::run(std::string workload)
{
    generate_hdw_config_file();
    
    this->workload = workload;
    int finished = false;
    
    while(!finished){
        //update chip status
        for (int i = 0; i < _config->_nchips; i++) {
            
            _chips[i]->tick();
            
        }
        //update time
        _time++;
        #ifdef PRINT_TICK
        #ifdef REDUCE_IO
        //Reducing I/O
        if((_time%5000==0) || (_time==1) || (finishedReqNo==totalReqNo))
        #endif
        cout<<"\r"<<"current time: "<<_time <<" requests:"<<finishedReqNo<<"/"<<totalReqNo<<std::flush;
        #endif
        
        //check if all chips finished
        finished = true;
        for(int i=0; i< _config->_nchips; i++){
            if (!_chips[i]->isFinished()){
                finished = false;
            }
        }
    }
    //print stats
    for (int i = 0; i < _config->_nchips; i++) {
        _chips[i]->outputStats(rstFile);
    }

}


void System::finish()
{

    fprintf(rstFile, "\n############# Summary #############\n");
    for (int i = 0; i < _config->_nchips; i++) {
        fprintf(rstFile, "--------------------------------\n");
        //fprintf(rstFile, "Chip#%d has ticked %lu clocks\n", i, _chips[i]->getTime());
        //fprintf(rstFile, "Chip#%d has processed %lu instructions\n", i, _chips[i]->getDecoderTime());
        //Commenting out energy for now because any code related to energy hasn't been updated yet
        //fprintf(rstFile, "Chip#%d has consumed %.4lf nj energy\n", i, _chips[i]->getTotalEnergy());
        //fprintf(rstFile, "Chip#%d leakage power is %.4lf W\n", i, _chips[i]->getTotalLeakageEnergy());
        fprintf(rstFile, "--------------------------------\n");

        cout<<std::endl;
        cout<<"Bits read from DRAM: "<< _chips[i]->_values->bitsReadFromDram<<std::endl;
        cout<<"Bits written to DRAM: "<< _chips[i]->_values->bitsWrittenToDram;

        for (int j = 0; j < _chips[i]->_values->config->_ntiles; j++) {
            fprintf(rstFile, "Chip#%d Tile#%d has ticked %lu clocks, last active time is %lu\n", i, j, (long) _time, (long)_chips[i]->_children[j]->_last_req_time);
            fprintf(rstFile, "--------------------------------\n");

            //for (int k = 0; k < _chips[i]->_children[j]->_nchildren; k++) {
            //    fprintf(rstFile, "Chip#%d Tile#%d Block#%d has ticked %lu clocks\n", i, j, k, _chips[i]->_children[j]->_children[k]->getTime());
            //}
            fprintf(rstFile, "--------------------------------\n");

        }
    }

    //Remove the two functions below because the variables they use to collect stats are not maintained any more
    //generate_req_count_csv();
    //generate_cycle_csv();
    generate_states_csv();
    generate_req_states_csv();
    generate_energy_csv();
    generate_router_hops_csv();

}


std::map<std::string, Registry::Entry> & Registry::registeredSimulation() {
  static std::map<std::string, Registry::Entry> *res = nullptr;
  if (res == nullptr) {
    res = new std::map<std::string, Registry::Entry>();
  }
  return *res;
}


Registry::Entry::Entry(const Registry::Entry &re) : name(re.name), f(re.f) {}

Registry::Entry::Entry(const std::string &s, std::function<int32_t(System*, std::string)> f) :
 name(s), f(f) {}

namespace pimsim {

Registry::Entry &registerFunc(const std::string &ky, std::function<int32_t(System *, std::string)> func) {
  Registry::Entry entry(ky, func);
  Registry::registeredSimulation()[ky] = entry;
  return Registry::registeredSimulation()[ky];
}

}

//////////////////////////////////////////////////////
// Send data located in tile pointed to by 'addr'
// to multiple other tiles. This uses a one-to-many method, so it 
// generates a lot of traffic in the mesh.
// The precision (number of rows) is specified by 'precision_input'. 
// The list of receivers is populated in 'receivers'. If the list 
// contains all tiles (except the originator), then it is a broacast.
// If the list contains some tiles, then it is a multicast.
// When the the data is received inside the tiles into CRAMs,
// then shuffling can be specified by using the ben, men, samt, bcnt
// args. For details, of these args see: 
// https://docs.google.com/document/d/1Bf0mXqqMR2pZI0vSRqwMha4yL-xd5H8vjs1tdAiPKfM/edit?usp=sharing
// 'size' should be equal to number of columns in a CRAM for
// these shuffle options to work.
//////////////////////////////////////////////////////
void System::broadcast(int addr, 
                       PrecisionT::Precision precision_input, 
                       std::vector<int> receivers, 
                       int size, 
                       std::vector<Request>& requests,
                       bool ben, bool men, int samt, int bcnt){
    Request *request;
    int tile = addr/(_config->_nblocks*_config->_nrows*_config->_ncols);
    //assert(tile<_config->_meshWidth);
    int block =  (addr%(_config->_nblocks*_config->_nrows*_config->_ncols))/(_config->_nrows*_config->_ncols);
    assert(block==0);
    int row = ((addr%(_config->_nblocks*_config->_nrows*_config->_ncols))%(_config->_nrows*_config->_ncols))/_config->_ncols;

    //Check sanity of shuffle related args
    if ((ben==1) || (men==1)) {
        if (size != _config->_ncols) {
            std::cout<<"If broadcast_en or multicast_en is 1, then size should be equal to number of cols in a CRAM";
            assert(0);
        }
    }

    for(int col=0; col<_config->_meshWidth; col++){
        int destRow = tile/_config->_meshWidth;
        int destTile = destRow*_config->_meshWidth + col;
        if(destTile==tile) continue;
        bool isReceiver = false;
        for(int i:receivers){
            if (destTile==i) isReceiver = true;
        }
        if(isReceiver){
            request = new Request(Request::Type::TileSend);
            request->addOperand(getAddress(tile,0,row), size, precision_input); //src
            request->addOperand(getAddress(destTile,0,row), size, precision_input); //dst
            requests.push_back(*request);

            request = new Request(Request::Type::TileReceive);
            request->addOperand(getAddress(tile,0,row), size, precision_input); //src
            request->addOperand(getAddress(destTile,0,row), size, precision_input); //dst
            request->setShuffle(ben, men, samt, bcnt);
            requests.push_back(*request);
        }
    }

    int srcRow = tile/_config->_meshWidth;
    for(int col=0; col<_config->_meshWidth; col++){
        for(int row=0; row<_config->_meshHeight; row++){
            if(row == srcRow) continue;
            bool isReceiver = false;
            for(int i:receivers){
                if (row*_config->_meshWidth+col==i) isReceiver = true;
            }
            if(isReceiver){
                request = new Request(Request::Type::TileSend);
                request->addOperand(getAddress(srcRow * _config->_meshWidth + col,0,row), size, precision_input); //src
                request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                requests.push_back(*request);

                request = new Request(Request::Type::TileReceive);
                request->addOperand(getAddress(srcRow * _config->_meshWidth + col,0,row), size, precision_input); //src
                request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                request->setShuffle(ben, men, samt, bcnt);
                requests.push_back(*request);      
            }  
        }
    }
    //for (unsigned int i = 0; i < requests.size(); i++)
    //    sendRequest(requests[i]);
}

//////////////////////////////////////////////////////
// Send data located in tile pointed to by 'addr'
// to multiple other tiles. This uses the peer-to-peer method, so it 
// reduces traffic in the mesh.
// The precision (number of rows) is specified by 'precision_input'. 
// The list of receivers is populated in 'receivers'. If the list 
// contains all tiles (except the originator), then it is a broacast.
// If the list contains some tiles, then it is a multicast.
// When the the data is received inside the tiles into CRAMs,
// then shuffling can be specified by using the ben, men, samt, bcnt
// args. For details, of these args see: 
// https://docs.google.com/document/d/1Bf0mXqqMR2pZI0vSRqwMha4yL-xd5H8vjs1tdAiPKfM/edit?usp=sharing
// 'size' should be equal to number of columns in a CRAM for
// these shuffle options to work.
//////////////////////////////////////////////////////
void System::broadcast_p2p(int addr, 
                           PrecisionT::Precision precision_input, 
                           std::vector<int> receivers, 
                           int size, 
                           std::vector<Request>& requests,
                           bool ben, bool men, int samt, int bcnt){
    if(_config->_broadcast_type == "one_to_all"){
        std::cout<<"Using one-to-all broadcast"<<std::endl;
        broadcast(addr, precision_input, receivers, size, requests, ben, men, samt, bcnt);
        return;
    }
    else if(_config->_broadcast_type == "systolic"){}
    else{
        std::cout<<"broadcast type not set. using p2p systolic broadcast by default"<<std::endl;
    }
    // std::cout<<"Using p2p systolic broadcast"<<std::endl;
    Request *request;
    int tile = addr/(_config->_nblocks*_config->_nrows*_config->_ncols);
    assert(tile<_config->_meshWidth * _config->_meshHeight);
    int block =  (addr%(_config->_nblocks*_config->_nrows*_config->_ncols))/(_config->_nrows*_config->_ncols);
    assert(block==0);
    int row = ((addr%(_config->_nblocks*_config->_nrows*_config->_ncols))%(_config->_nrows*_config->_ncols))/_config->_ncols;

    //Check sanity of shuffle related args
    if ((ben==1) || (men==1)) {
        if (size != _config->_ncols) {
            std::cout<<"If broadcast_en or multicast_en is 1, then size should be equal to number of cols in a CRAM";
            assert(0);
        }
    }
    int lastTile = tile;
    for(int col=tile+1; col<(tile/_config->_meshWidth +1)*_config->_meshWidth; col++){
        bool isReceiver = false;
        for(int i:receivers){
            if (col==i) isReceiver = true;
        }
        if(isReceiver){
            request = new Request(Request::Type::TileSend);
            request->addOperand(getAddress(lastTile,0,row), size, precision_input); //src
            request->addOperand(getAddress(col,0,row), size, precision_input); //dst
            requests.push_back(*request);

            request = new Request(Request::Type::TileReceive);
            request->addOperand(getAddress(lastTile,0,row), size, precision_input); //src
            request->addOperand(getAddress(col,0,row), size, precision_input); //dst
            request->setShuffle(ben, men, samt, bcnt);
            requests.push_back(*request);

            lastTile = col;
        }
    }
    lastTile = tile;
    for(int col=tile-1; col>=(tile/_config->_meshWidth)*_config->_meshWidth; col--){
        bool isReceiver = false;
        for(int i:receivers){
            if (col==i) isReceiver = true;
        }
        if(isReceiver){
            request = new Request(Request::Type::TileSend);
            request->addOperand(getAddress(lastTile,0,row), size, precision_input); //src
            request->addOperand(getAddress(col,0,row), size, precision_input); //dst
            requests.push_back(*request);

            request = new Request(Request::Type::TileReceive);
            request->addOperand(getAddress(lastTile,0,row), size, precision_input); //src
            request->addOperand(getAddress(col,0,row), size, precision_input); //dst
            request->setShuffle(ben, men, samt, bcnt);
            requests.push_back(*request);

            lastTile = col;
        }
    }

    for(int col=0; col<_config->_meshWidth; col++){
        bool isSender = false;
        int senderRow = tile/_config->_meshWidth;
        for(int i:receivers){
            if (senderRow*_config->_meshWidth+col==i) isSender = true;
        }
        if(senderRow*_config->_meshWidth+col == tile) isSender = true;
        if(isSender){
            int lastRow = senderRow;
            for(int row = senderRow +1; row<_config->_meshHeight; row++){
                bool isReceiver = false;
                for(int i:receivers){
                    if (row*_config->_meshWidth+col==i) isReceiver = true;
                }
                if(isReceiver){
                    request = new Request(Request::Type::TileSend);
                    request->addOperand(getAddress((lastRow) * _config->_meshWidth + col,0,row), size, precision_input); //src
                    request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                    requests.push_back(*request);

                    request = new Request(Request::Type::TileReceive);
                    request->addOperand(getAddress((lastRow) * _config->_meshWidth + col,0,row), size, precision_input); //src
                    request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                    request->setShuffle(ben, men, samt, bcnt);
                    requests.push_back(*request);  

                    lastRow = row; 
                }     
            }
            lastRow = senderRow;
            for(int row = senderRow -1; row>=0; row--){
                bool isReceiver = false;
                for(int i:receivers){
                    if (row*_config->_meshWidth+col==i) isReceiver = true;
                }
                if(isReceiver){
                    request = new Request(Request::Type::TileSend);
                    request->addOperand(getAddress((lastRow) * _config->_meshWidth + col,0,row), size, precision_input); //src
                    request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                    requests.push_back(*request);

                    request = new Request(Request::Type::TileReceive);
                    request->addOperand(getAddress((lastRow) * _config->_meshWidth + col,0,row), size, precision_input); //src
                    request->addOperand(getAddress(row * _config->_meshWidth + col,0,row), size, precision_input); //dst
                    request->setShuffle(ben, men, samt, bcnt);
                    requests.push_back(*request);  

                    lastRow = row;  
                }     
            }
        }
    }
    //for (unsigned int i = 0; i < requests.size(); i++)
    //    sendRequest(requests[i]);
}

void System::broadcast_p2p(int addr, 
                           PrecisionT::Precision precision_input, 
                           std::vector<int> receivers, 
                           int size, 
                           bool ben, bool men, int samt, int bcnt) {
  std::vector<Request> reqs;
  if(_config->_broadcast_type == "systolic")
    broadcast_p2p(addr, precision_input, receivers, size, reqs, ben, men, samt, bcnt);
  else if(_config->_broadcast_type == "one_to_all")
    broadcast(addr, precision_input, receivers, size, reqs, ben, men, samt, bcnt);
  else{
    std::cout<<"broadcast type not set. using p2p systolic broadcast by default"<<std::endl;
    broadcast_p2p(addr, precision_input, receivers, size, reqs, ben, men, samt, bcnt);
  }
  for (auto &elem : reqs)
    sendRequest(elem);
}

void System::map_vec_to_tile(std::vector<Request>& requests, int tile, const std::string &origVecName, int offset, int partition_size, int bits, int length, int ben, int men, int samt, int bcnt){
    //no tile to tile transfer implementation
    // if(mapper.isVecAllocatedAtTile(tile, origVecName, offset, bits, length, ben, men,samt, bcnt)){
    //     std::cout<<"vector "<<origVecName<< "with offset"<<offset<<" is already allocated at tile "<<tile<<std::endl;
    //     return;
    // }
    // else{
    //     std::cout<<"allocate vector "<<origVecName<< "with offset"<<offset<<" at tile "<<tile<<std::endl;
    //     mapper.allocVecAtTile(tile, origVecName, offset, bits, length, ben, men,samt, bcnt);
    //     Request *request;
    //     request = new Request(Request::Type::RowLoad);
    //     request->addOperand(getAddress(tile,0,0), length, PrecisionT::Precision{0, bits, 0});//cram
    //     request->addOperand(DRAM_ADDR, length, PrecisionT::Precision{0, bits, 0}); //dram addr
    //     request->setShuffle(ben,men, samt, bcnt);
    //     requests.push_back(*request);   
    // }

    //assuming tile-to-tile transfer
    //TODO: rewrite the following code:
    //assume all vectors have aligned offset. 
    //calculate aligned offset
    //check allocation based on aligned offset instead of original offset.
    
    if(mapper.isVecAllocated(origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt)){
        //already allocated somewhere
        if(!mapper.isVecAllocatedAtTile(tile, origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt)){
            std::stringstream result;
            std::vector<int> tiles = mapper.getTileOfVec(origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt);
            std::copy(tiles.begin(), tiles.end(), std::ostream_iterator<int>(result, " "));
            alloc_log_file<<"transfer: "<<origVecName<< " offset "<<offset<<" from tile "<< result.str()<<" to tile "<<tile<<std::endl;
            mapper.allocVecAtTile(tile, origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt);
        }
        else{
            alloc_log_file<<"present: "<<origVecName<< " offset "<<offset<<" at tile "<<tile<<std::endl;
        }
        total_bits_accessed_data += length*bits;
        total_num_access_request += 1;
        return;
    }
    else{
        
        int aligned_offset =offset/partition_size*partition_size;
        if(ben==0 && men==0 && samt==0 && bcnt==0 && mapper.isVecAllocated(origVecName, aligned_offset, partition_size, bits, length, ben, men,samt, bcnt)){
            //if vector can be shifted from an existing one
            //shift the existing vector instead of loading it from dram
            if(!mapper.isVecAllocatedAtTile(tile, origVecName, aligned_offset, partition_size, bits, length, ben, men,samt, bcnt)){
                std::stringstream result;
                std::vector<int> tiles = mapper.getTileOfVec(origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt);
                std::copy(tiles.begin(), tiles.end(), std::ostream_iterator<int>(result, " "));
                alloc_log_file<<"transfer: "<<origVecName<<" offset "<<offset <<" aligned_offset "<<aligned_offset<<" from tile "<< result.str()<<" to tile "<<tile<<std::endl;
                mapper.allocVecAtTile(tile, origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt);
            }
            else{
                alloc_log_file<<"present: "<<origVecName<<" offset "<<offset<< " aligned_offset "<<aligned_offset<<" at tile "<<tile<<std::endl;
            }
            total_bits_accessed_data += length*bits;
            total_num_access_request += 1;
            return;
            
        }
        else{
            //even aligned offset is not allocated anywhere
            alloc_log_file<<"allocate: "<<origVecName<< " offset "<<offset<<" at tile "<<tile<<std::endl;
            mapper.allocVecAtTile(tile, origVecName, offset, partition_size, bits, length, ben, men,samt, bcnt);
            Request *request;
            request = new Request(Request::Type::RowLoad);
            request->addOperand(getAddress(tile,0,0), length, PrecisionT::Precision{0, bits, 0});//cram
            request->addOperand(DRAM_ADDR, length, PrecisionT::Precision{0, bits, 0}); //dram addr
            request->setShuffle(ben,men, samt, bcnt);
            requests.push_back(*request);  
            
            total_bits_missed_data += length*bits;
            total_num_missed_request += 1;

            //std::cout<<"load "<<length*bits<<" bits, "<<"total load bits: "<<total_bits_missed_data<<std::endl;
        } 
    }
    total_bits_accessed_data += length*bits;
    total_num_access_request += 1;
    //std::cout<<"access "<<length*bits<<" bits, "<<"total access bits: "<<total_bits_accessed_data<<std::endl;
}

void System::print_data_hit_rate(){
    if(total_bits_accessed_data==0){
        alloc_log_file<<"0 bits of data accessed, data hit rate omitted"<<std::endl;
        return;
    }
    alloc_log_file<<"data hit rate: "<<1-total_bits_missed_data/(double)total_bits_accessed_data\
    <<", hit: "<<total_bits_accessed_data-total_bits_missed_data\
    <<", accessed: "<<total_bits_accessed_data<<std::endl;
}

void System::print_req_hit_rate(){
    if(total_num_access_request==0){
        alloc_log_file<<"0 request accessed data, req hit rate omitted"<<std::endl;
        return;
    }
    alloc_log_file<<"req hit rate: "<<1-total_num_missed_request/(double)total_num_access_request\
    <<", "<<total_num_access_request-total_num_missed_request\
    <<" out of "<<total_num_access_request<<" requests hit their data"<<std::endl;
}