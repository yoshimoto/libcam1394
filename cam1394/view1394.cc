#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <features.h>
#include <netinet/in.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <popt.h>
#include <string.h>

#include <libraw1394/raw1394.h> 
#include <libraw1394/csr.h>

#include <linux/ohci1394_iso.h> /* ドライバとのインタフェイス関連 */
#include <libcam1394/1394cam.h>
#include <libcam1394/yuv.h>     /* 色変換 */
#include "common.h"
#include "xview.h"              /* Xの表示オブジェクト*/


typedef unsigned int uint;

struct ImgInfo {
     int  w;
     int  h;
     int  packet_sz;
     int  num_packet;

     uint start_frame;
     uint end_frame;
     uint skip;

     int getImageSize(){ return packet_sz*num_packet; }
};

/** 
 * 画像一枚を読込む。
 * 
 * @param fd 
 * @param buf 
 * @param info 
 * 
 * @return true 成功
 */
bool read_one_frame(int fd, char *buf, ImgInfo *info)
{
     const int sz=info->getImageSize();
     if ( sz != read(fd, buf, sz) ){
	  return false;
     }else{
	  return true;
     }
}

/**  
 * フレームを読み飛ばす。
 * 
 * @param fd 
 * @param sz 
 * @param frame 
 * 
 * @return 読み飛ばしたフレーム数
 */
int skip_frame(int fd, ImgInfo *info)
{
     char buf[info->getImageSize()];
     int i;
     for (i=0;i<info->skip;i++){
	  /* 画像読み込む */
	  if (!read_one_frame(fd, buf, info)){
	       return i;
	  }
     }
     return i;
}

int split_to_ppm(const char *srcfile, const char *dstfile,
		 ImgInfo* info){
     
     int fd=open(srcfile,O_RDONLY);
     if (fd<=0){
	  cerr << "can not open file "<<srcfile<<endl;
	  return -1;
     }
     
     int frame=info->start_frame;
     while (frame<info->end_frame){
	  RGBA work[info->w*info->h];  
	  char buf[info->getImageSize()];
       
	  read_one_frame(fd, buf, info);
       
	  // YUV422形式からRGBA形式へ変換 
	  copy_YUV422toRGBA(work,buf,
			    info->packet_sz,
			    info->num_packet,REMOVE_HEADER);
       
	  char fmt[1024];
	  snprintf(fmt,sizeof(fmt),"%s.ppm",dstfile);
	  char fname[1024];
	  snprintf(fname,sizeof(fname),fmt, frame);
       
	  SaveRGBAtoFile(fname, work, info->w, info->h, 0);
	  frame++;

	  // nSkip フレーム分読み飛ばす 
	  int skipped=skip_frame(fd, info);
	  
	  if ( skipped < info->skip ){
	       // ファイルが読めない
	       return false;
	  }	
	  frame += skipped;
     }
     return true;
}

int main(int argc,char *argv[]){


  char *srcfile=argv[1];
  char *dstfile=argv[2];

  
  // create table to convert YUV to RGB.
  ::CreateYUVtoRGBAMap();


  FORMAT fmt=Format_0;
  VMODE  mode=Mode_1;
  FRAMERATE rate=FrameRate_3;
  
  ImgInfo info;
  info.w           = GetImageWidth ( fmt, mode);
  info.h           = GetImageHeight( fmt, mode);
  info.packet_sz   = GetPacketSize ( fmt, mode, rate);
  info.num_packet  = GetNumPackets ( fmt, mode, rate);
  info.start_frame = 0;
  info.end_frame   = 0x7fffffff;
  info.skip = 0;



  info.skip = 10;
  info.packet_sz  += 8;

  
  split_to_ppm(srcfile, dstfile, &info);
  
  return 0;
}
