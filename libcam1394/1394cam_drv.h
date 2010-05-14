#include "1394cam.h"

struct libcam1394_driver {
     int (*close)(libcam1394_driver* ctx);
     int (*mmap)(libcam1394_driver* ctx,
		 int port_no, const char *devicename,
		 int channel, 
		 int sz_packet, int num_packet,
		 int num_frame, int *header_size);
     int (*getFrameCount)(libcam1394_driver* ctx,
			  int *counter);
     int (*setFrameCount)(libcam1394_driver* ctx,
			  int counter);
     void* (*updateFrameBuffer)(libcam1394_driver* ctx,
				C1394CameraNode::BUFFER_OPTION opt, 
				BufferInfo* info);
};

libcam1394_driver * open_1394_driver(int port_no, const char *devicename,
				     int channel,
				     int sz_packet, int num_packet,
				     int num_frame,
				     int *header_size);
void close_1394_driver(libcam1394_driver **);
