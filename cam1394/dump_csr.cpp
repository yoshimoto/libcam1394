#include <iostream>
#include <iomanip>

#include <unistd.h>   //  for usleep

#include <string.h>
#include <malloc.h>

#include <errno.h>
#include <netinet/in.h>  // for ntohl()

#include <libraw1394/raw1394.h>
#include <libraw1394/csr.h>


using namespace std;

#define ERR(msg)  do { std::cerr<< msg << std::endl;} while(0)
#define LOG(msg)  do { std::cout<< msg << std::endl;} while(0)


#define ADDR_CONFIGURATION_ROM  (CSR_REGISTER_BASE + CSR_CONFIG_ROM)

/** 
 * 
 * 
 * @param handle 
 * @param nodeid 
 * @param addr 
 * @param len 
 * @param buf 
 * 
 * @return 
 */
int try_raw1394_read(raw1394handle_t handle, nodeid_t nodeid,
		     nodeaddr_t addr, int len, quadlet_t *buf)
{
     int retry = 3;
     while (retry-->0){
	  int retval = raw1394_read(handle, nodeid, addr, len, buf);
	  if (retval >= 0){
	       return 0;
	  }
	  usleep(500);
     }
     ERR("raw1394_read() failed.");
     return -1;
}

/** 
 * 
 * 
 * @param handle 
 * @param nodeid 
 * @param addr 
 * @param length 
 * 
 * @return 
 */
int get_directory_length(raw1394handle_t handle, nodeid_t nodeid,
			 nodeaddr_t addr, quadlet_t *length)
{
     quadlet_t tmp;    
     if (try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp))
	  return -1;
     tmp = htonl( tmp );
     *length = tmp >> 16;
     return 0;
}

int parse_string_leaf(raw1394handle_t handle,
		      nodeid_t nodeid,
		      nodeaddr_t addr)
{
     int result = 0;
     quadlet_t leaf_length;
     if (get_directory_length(handle, nodeid, addr, &leaf_length))
	  return -1;
     leaf_length *= 4;
     leaf_length -= 8;
     addr += 0x0c;

     char *buf = (char*)malloc((leaf_length+4)&~3);
     char *p=buf;
     while (leaf_length>0){
	  quadlet_t tmp;
	  try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp);
	  tmp = htonl( tmp );	  
	  
	  *p++ = tmp >> 24;
	  *p++ = tmp >> 16;
	  *p++ = tmp >> 8;
	  *p++ = tmp >> 0;

	  leaf_length -= 4;
	  addr+=4;
     }

     LOG(" name: "<< buf );
     free(buf);
     return 0;
}


int parse_unit_dependent_directory(raw1394handle_t handle,
				   nodeid_t nodeid, nodeaddr_t addr)
{
     int result = 0;
     quadlet_t length;
     if (get_directory_length(handle, nodeid, addr, &length))
	  return -1;
     
     while (length-->0 && !result){
	  quadlet_t tmp;
	  
	  int type;
	  quadlet_t value;
	  
	  addr += 4;

	  try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp);

	  tmp = htonl( tmp );
	  
	  type = tmp >> 24;
	  value = tmp & 0xffffff;

	  switch ( type ){
	  case 0x40:
	       LOG(" command_regs_base: " <<  setw(8) << value);
	       break;
	  case 0x81:
	       parse_string_leaf(handle, nodeid,
				 addr + value*sizeof(quadlet_t));
	       break;
	  case 0x82:
	       parse_string_leaf(handle, nodeid, 
				 addr + value*sizeof(quadlet_t));
	       break;
	  default:
	       LOG(" unknown "<< setw(8) << hex << tmp);
	       break;
	  }
     }     
     return result;
}


int parse_node_unique_id_leaf(raw1394handle_t handle,
			 nodeid_t nodeid, nodeaddr_t addr)
{
     int result = 0;
     quadlet_t length;
     if (get_directory_length(handle, nodeid, addr, &length))
	  return -1;

     if (length != 2){
	  ERR("format error");
	  return -1;
     }
     
     addr+=4;

     quadlet_t tmp;
     try_raw1394_read(handle,nodeid, addr+0, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );
     quadlet_t node_vendor_id = tmp>>8;
     quadlet_t chip_id_hi = tmp&0xf;
     try_raw1394_read(handle,nodeid, addr+4, sizeof(tmp), &tmp);     
     tmp = ntohl( tmp );
     quadlet_t chip_id_lo = tmp;

     LOG(" node_vendor_id: " << setw(8) << node_vendor_id);
     LOG(" chip_id_hi: " << setw(8) << chip_id_hi);
     LOG(" chip_id_lo: " << setw(8) <<chip_id_lo);

     return result;
}

int parse_default(raw1394handle_t handle,
		  nodeid_t nodeid, nodeaddr_t addr,
		  unsigned char type, quadlet_t value)
{
     switch (type){
     case 0x81:
	  parse_string_leaf(handle, nodeid,
			    addr + value*sizeof(quadlet_t));
	  break;
     case 0x82:
	  parse_string_leaf(handle, nodeid, 
			    addr + value*sizeof(quadlet_t));
	  break;
     }

     return 0;
}

int parse_unit_directory(raw1394handle_t handle,
			 nodeid_t nodeid, nodeaddr_t addr)
{
     int result = 0;
     quadlet_t length;
     if (get_directory_length(handle, nodeid, addr, &length))
	  return -1;

     while (length-->0 && !result){
	  quadlet_t tmp;
	  
	  int type;
	  quadlet_t value;
	  
	  addr += 4;

	  try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp);

	  tmp = htonl( tmp );
	  
	  type = tmp >> 24;
	  value = tmp & 0xffffff;

	  switch ( type ){
	  case 0x12:
	       LOG(" unit_spec_ID(=0x00A02D): " <<  setw(8) <<value);
	       break;
	  case 0x13:
	       LOG(" unit_sw_version: " <<  setw(8) << value);
	       break;
	  case 0xd4:
	       result =parse_unit_dependent_directory(handle, nodeid,
						      addr+value*sizeof(quadlet_t));
	       break;

	  default:
	       result = parse_default(handle, nodeid, addr, type, value);
	       break;
	  }
     }     
     return result;
}

int parse_bus_information_block(raw1394handle_t handle,
			    nodeid_t nodeid, nodeaddr_t addr)
{
     LOG(setw(8)<<setfill('0')<<hex);
     quadlet_t tmp;

     try_raw1394_read(handle,nodeid, addr+0x00, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );
     int bus_info_length = (tmp >> 24)*sizeof(quadlet_t);
     LOG(" bus info length             : "<< setw(8) << bus_info_length);

     try_raw1394_read(handle,nodeid, addr+0x04, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );
     LOG(" bus name (=0x31333934)      : "<< setw(8) << tmp);
     try_raw1394_read(handle,nodeid, addr+0x08, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );    
     LOG(" bus-depended Information    : "<< setw(8) << tmp);
     try_raw1394_read(handle,nodeid, addr+0x0c, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );     
     LOG(" Extended Unique Identififer : "<< setw(8) << tmp);
     try_raw1394_read(handle,nodeid, addr+0x10, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );
     LOG(" Extended Unique Identififer : "<< setw(8) << tmp);
     

     return bus_info_length;
}

int parse_root_directory(raw1394handle_t handle,
			 nodeid_t nodeid, nodeaddr_t addr)
{
     LOG(setw(8)<<setfill('0')<<hex);
     quadlet_t tmp;

 
     try_raw1394_read(handle,nodeid, addr+0x0c, sizeof(tmp), &tmp);
     tmp = ntohl( tmp );
     quadlet_t node_vendor_id = tmp>>8;
     quadlet_t chip_id_hi = tmp&0xf;
     try_raw1394_read(handle,nodeid, addr+0x10, sizeof(tmp), &tmp);     
     tmp = ntohl( tmp );
     quadlet_t chip_id_lo = tmp;

     LOG(" node_vendor_id: " << setw(8) << node_vendor_id);
     LOG(" chip_id_hi: " << setw(8) << chip_id_hi);
     LOG(" chip_id_lo: " << setw(8) <<chip_id_lo);


     int result = 0;
     quadlet_t length;
     if (get_directory_length(handle, nodeid, addr, &length))
	  return -1;

     while (length-->0 && !result){
	  quadlet_t tmp;
	  
	  int type;
	  quadlet_t value;
	  
	  addr += 4;

	  try_raw1394_read(handle, nodeid, addr, sizeof(tmp), &tmp);

	  tmp = htonl( tmp );
	  
	  type = tmp >> 24;
	  value = tmp & 0xffffff;

	  switch ( type ){
	  case 0x8d:
	       result = parse_node_unique_id_leaf(handle, nodeid, 
						  addr+value*sizeof(quadlet_t));
	       break;
	  case 0xd1:
	       result = parse_unit_directory(handle, nodeid,
					     addr+value*sizeof(quadlet_t));
	       break;
	  default:
	       result = parse_default(handle, nodeid, addr, type, value);
	       break;
	  }
     }     
     return result;
}

int main()
{

     raw1394handle_t handle;
     handle = raw1394_new_handle();
     if (!handle) {
	  if (!errno) {
	       ERR("not_compatible");
	  } else {
	       ERR("driver is not loaded");
	  }
	  return -1;
     }

     int  numcards;
     const int NUM_PORT = 16;  /*  port   */
     struct raw1394_portinfo portinfo[NUM_PORT];
     
     numcards = raw1394_get_port_info(handle, portinfo, NUM_PORT);
     if (numcards < 0) {
	  perror("couldn't get card info");
	  return false;
     } else {
	  LOG(numcards << "card(s) found");
     }

     int port_no = 0;

     raw1394_set_port(handle, port_no);

     LOG( portinfo[port_no].nodes << " nodes are on card #0");

     int i;
     for (i=0; i< portinfo[port_no].nodes; i++){
	  nodeid_t nodeid = 0xffc0 | i;

	  LOG("node:" << setfill('0') << i);
	  nodeaddr_t businfo_dir = ADDR_CONFIGURATION_ROM ;
	  nodeaddr_t root_dir = businfo_dir + sizeof(quadlet_t);
	  root_dir += parse_bus_information_block(handle, nodeid, businfo_dir);
	  parse_root_directory(handle, nodeid, root_dir);
     }


     raw1394_destroy_handle(handle);

     return 0;
}
