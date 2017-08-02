#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include "linux/uvcvideo.h"
#include <linux/usb/video.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

#include "md5.h"
#include "bin_file.h"

const int XU_ID = 6;

int video_fd = -1;

/***************************************************************************
					function:GetHex()
					version : 2.0
***************************************************************************/
static uint32_t GetHex (char * ptr, int count)
{
     uint32_t    acc;
     int        c;

     acc = 0;
     while ( count-- > 0 ){    
		 c = *ptr++;

              if ( c >= '0' && c <= '9' ) c -= '0';
         else if ( c >= 'a' && c <= 'f' ) c = (c - 'a') + 10;
         else if ( c >= 'A' && c <= 'F' ) c = (c - 'A') + 10;

         acc = (acc << 4) | c;
     }

     return acc;
}

static uint32_t GetInt (uint8_t *ptr)
{
	uint32_t t, t1, t2, t3, t4;
	t1 = ptr[0];
	t2 = ptr[1];
	t3 = ptr[2];
	t4 = ptr[3];

	t = t4<<24 | t3<< 16 | t2 << 8 | t1;

	return t;
}
/***************************************************************************
					function:ReadSrecFile()
					version : 2.0
***************************************************************************/
int ReadSrecFile ( char *filename , uint8_t buffer[], FILE * output)
{
     FILE *     fp;
     char       line[128];
     uint8_t    data[32]={0};
     uint32_t   packed[8];
     uint32_t   verify[8];
     uint32_t   addr;
     int        count;
     int        offset;
     int        shift;
     int        i;
     int        records;
     int        first_addr;


	 if((filename[strlen(filename)-1]!= 'c') && (filename[strlen(filename)-2]!= 'e') && (filename[strlen(filename)-3]!= 'r') && (filename[strlen(filename)-4]!= 's') &&  (filename[strlen(filename)-5]!= '.')){   
			fprintf(stderr,"%s is not an SREC file\n", filename);
			return -1;
	 }else{
		if ((fp=fopen(filename, "r"))==NULL){
			fprintf(stderr,"Unable to open file %s\n", filename);
			return -1;
		}else{
			fprintf(stderr,"Opened file %s\n", filename);	 
		}
	 }
     records = 0;
     first_addr = -1;
     while ( fgets(line, 128, fp) != 0 ){
		 if (line[0] != 'S' || line[1] != '3'){
			 continue;
		 }

        count  = GetHex(line + 2, 2) - 5;
        addr   = GetHex(line + 4, 8);
		if (first_addr < 0){
			first_addr = addr;
		}

		addr = addr - first_addr;

        offset = 12;
        for (i = 0; i < count; i++){
            data[i] = GetHex(line + offset, 2);
            offset += 2;
        }

        records++;
		for(int i = 0; i < count; i++){
			buffer[addr+i]= data[i];
		}
     }
     fclose(fp);
	 return first_addr;
}
/***************************************************************************
					function:usage()
					version : 2.0
***************************************************************************/
void usage()
{
	fprintf(stderr, "\nUsage:cam_update [Bin Path To Load]\n\n");
}


#define FD_RETRY_LIMIT 4

int xioctl(int request, void *arg)
{
  int ret = 0;
  int tries = FD_RETRY_LIMIT;
  do
  {
    ret = ioctl(video_fd, request, arg);
  }
  while(ret && tries-- && ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)));
  
  if (ret) 
    printf("UVC%d: ioctl %d failed, try %d, error=%s\n",video_fd, request, tries, strerror(errno));
  
  return ret;
}


bool getControl(int xu_id, int controlnumber, int size, uint8_t *value)
{
  struct uvc_xu_control_query uvc;
  
  memset(&uvc, 0, sizeof(uvc));
  
  uvc.unit = xu_id;
  uvc.selector = controlnumber;
  uvc.query = UVC_GET_CUR;
  uvc.size = size;
  uvc.data = value;
  
  if (xioctl(UVCIOC_CTRL_QUERY, &uvc) == -1) 
  {
	  printf("getcontrol %d failed\n", controlnumber);
    return false;
  }
 
  return true;  
}

bool setControl(int xu_id, int controlnumber, int size, uint8_t *value)
{
  struct uvc_xu_control_query uvc;
  
  memset(&uvc, 0, sizeof(uvc));
  
  //logger(LOG_DEBUG) << "UVCXU: set control " << controlnumber << ", size " << size << ", value[0] 0x" << std::hex << (uint)value[0] << std::endl;
  
  uvc.unit = xu_id;
  uvc.selector = controlnumber;
  uvc.query = UVC_SET_CUR;
  uvc.size = size;
  uvc.data = value;
  
  if (xioctl(UVCIOC_CTRL_QUERY, &uvc) == -1) 
  {
    printf("setControl %d failed\n",controlnumber);
    return false;
  }
  
  return true;
}
  
void reset()
{
	uint8_t buf_control1[13];
	
    fprintf(stderr,"reset begin.\n");
	memset(buf_control1, 0, sizeof(buf_control1));

	if (!setControl(XU_ID, 1, 13, buf_control1))
	{
		fprintf(stderr,"Unable to reset system to normal state. This will require a system reset. Try update again after.\n");		
	}

}
/***************************************************************************
					function:main()
					version : 2.0
***************************************************************************/

int main(int argc, char* argv[])
{
	int ret = 1;
	int device = 0;
	char *srec=NULL;

	FILE *pFile=NULL;
	uint8_t *buffer=NULL;
	uint8_t *buffer_srec=NULL;
	size_t result;

	uint8_t buf_control1[13];
	uint8_t buf_control3[2];
	uint8_t buf_control7[1];
	uint8_t buf_control8[88];

	uint32_t srecKBytes=256;
	uint32_t srec_size = srecKBytes*1024;
	uint32_t length_of_srec = 262144;

	FILE * firmware_output = NULL;

	int bytes_left = srec_size;
	int bytes_to_read = 1024;
	int bytes_read=0;
	int first_address;
	int fd;
	uint32_t data_length;
	uint32_t lSize;
	uint8_t partition;
    MD5 md5;

	bin_file_hdr header;
	char signature[MAX_SIZE_SIG+1];
	char svn_rev[MAX_SIZE_SVN_REV+1];
	char date[MAX_SIZE_DATE+1];
	FILE* tFile;

	fprintf(stderr, "***enter cam update process***\n");

	if (argc != 3){
		fprintf(stderr, "Incorrect number of arguments \n");
		usage();
		return 1;
	}

    for (;;)
    {
	    video_fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
	    if (video_fd == -1)
	    {
	        // couldn't find capture device
	        fprintf(stderr, "Opening Video device failed: %s \n",strerror(errno));
	        sleep(1);
			continue;
	    }
		break;
    }

	char path[128] = {'\0'};
	char logfile[128] = {'\0'};
	
	strcpy(logfile, argv[2]);
	strcat(logfile, "update_log.txt");

 	DIR           *pDir = NULL;
	struct dirent *ent;

	strcpy(path, argv[2]);
	pDir = opendir(path);	
	while ((ent = readdir(pDir)) != NULL) {
		if (strstr(ent->d_name, "update")) {
			fprintf(stderr, "had updated, exit\n");
            ret = 0;
			goto fail;
		} 
	}

    strcpy(path, argv[1]);
	pDir = opendir(path);	
	while ((ent = readdir(pDir)) != NULL) {
		if (strstr(ent->d_name, "firm")) {
			strcat(path, ent->d_name);
			break;
		} 
	}

	if (strcmp(path, argv[1]) <= 0){
		fprintf(stderr, "not found update file\n");
        ret = 0;
		goto fail;
	}
	
	fprintf(stderr, "have found update file %s\n", path);

	srec = path;

	if((srec[strlen(srec)-1]!= 'n') && (srec[strlen(srec)-2]!= 'i') && (srec[strlen(srec)-3]!= 'b')){   
			fprintf(stderr,"%s is not a .bin file\n", srec);
			goto fail;
	 }else{
		if ((tFile=fopen(srec, "rb"))==NULL){
			fprintf(stderr,"Unable to open file %s, %s\n", srec, strerror(errno));
			goto fail;
		}else{
			fprintf(stderr,"Opened file %s\n", srec);	 
		}
	 }

	fseek(tFile, 0, SEEK_END);
	lSize = ftell(tFile);
	rewind(tFile);
	
	fread(&header, sizeof(bin_file_hdr), 1, tFile);
	strncpy(signature, (char*)header.signature, MAX_SIZE_SIG);
	signature[MAX_SIZE_SIG] = '\0';
	strncpy(svn_rev, (char*)header.svn_rev+4, MAX_SIZE_SVN_REV);
	svn_rev[MAX_SIZE_SVN_REV] = '\0';
	strncpy(date, (char*)header.date+5, MAX_SIZE_DATE);
	date[MAX_SIZE_DATE] = '\0';
	fprintf(stderr, "Length of header: %d\n", header.hdr_len);
	fprintf(stderr, "Board type: %s\nStart address: %08x\nVersion: %08x\nCRC32: %08x\n", header.type, header.start_address, header.version,header.crc32);
	fprintf(stderr, "Signature: %s\nSVN Revision: %s\nDate: %s\n", signature, svn_rev, date);
	first_address= header.start_address;

	if(sizeof(bin_file_hdr)!=header.hdr_len){
		fprintf(stderr, "sizeof(header) does not equal header.hdr_len\n");
		goto fail0;
	}
	
	buffer_srec = (uint8_t *) malloc(length_of_srec);
	if(buffer_srec==NULL){
		fprintf(stderr,"Memory Error. Could not allocate buffer size\n");
		goto fail0;
	}
	memset(buffer_srec, 0xFF, length_of_srec);
	
	data_length = lSize - header.hdr_len;
	fread(buffer_srec, data_length, 1, tFile);

    fprintf(stderr, "MD5 CheckSum: %s\n", md5.digestString((char*)buffer_srec, data_length));
    if (strcmp(signature, md5.digestString((char*)buffer_srec, data_length))!=0){
        fprintf(stderr, "Chosen firmware file does not have the correct md5 checksum signature.\n");
        goto fail1;
    }

	fprintf(stderr, "\nRead in bin size: %d\n", data_length);
	length_of_srec = data_length;
	fprintf(stderr, "Set bin size: %d\n", length_of_srec);

	fclose(tFile);

	partition = 0x00;
	if(first_address==0x000000){
		partition = 0x00;
	}
	if(first_address==0x040000){
		partition = 0x01;
	}
	if(first_address==0x080000){
		partition = 0x02;
	}
	if(first_address==0x0c0000){
		partition = 0x03;
	}
	if(first_address==0x100000){
		partition = 0x04;
	}
	if(first_address==0x140000){
		partition = 0x05;
	}
	if(first_address==0x180000){
		partition = 0x06;
	}
	if(first_address==0x1c0000){
		partition = 0x07;
	}

	if (getControl(XU_ID, 8, 88, buf_control8))
	{
		uint32_t version = GetInt(buf_control8);
		fprintf(stderr, "Read Version Info, version = %08x::%0x,%0x,%0x,%0x\n", version, buf_control8[0], buf_control8[1],buf_control8[2],buf_control8[3]);
	}
	else
	{
        fprintf(stderr, "Can't Read Version Info\n");
    }

	buffer = (uint8_t*)malloc(1024);
	if (buffer == NULL){ 
		fprintf(stderr, "Memory Error. Could not allocate buffer size\n"); 
		goto fail1;
	}

	uint8_t * buffer_zero;
	buffer_zero = (uint8_t*)malloc(1024);
	if(buffer_zero==NULL){
		fprintf(stderr,"Memory Error. Could not allocate buffer size\n");
		goto fail3;
	}
	for(int i =0; i <1024; i++){
		buffer_zero[i] = 0x00;
	}

	buf_control1[0] = 0x01;
	buf_control1[1] = 0x00;
	buf_control1[2] = partition;
	buf_control1[3] = 0x00;
	buf_control1[4] = (length_of_srec>>0)&0xff;
	buf_control1[5] = (length_of_srec>>8)&0xff;
	buf_control1[6] = (length_of_srec>>16)&0xff;
	buf_control1[7] = (length_of_srec>>24)&0xff;
	buf_control1[8] = (header.crc32>>0)&0xff;
	buf_control1[9] = (header.crc32>>8)&0xff;
	buf_control1[10] = (header.crc32>>16)&0xff;
	buf_control1[11] = (header.crc32>>24)&0xff;
	buf_control1[12] = header.isp_cfg;

	fprintf(stderr,"Initiating firmware update.\n");
    if (!setControl(XU_ID, 1, 13, buf_control1))
    {
	    fprintf(stderr,"Unable to initiate Firmware update. Please check to make sure video is not running.\n");
	    goto fail3;
    }
	
	fprintf(stderr, "System is ready to update. Sending binary data...\n");

	while(bytes_left > 0){
		for(int i =0; i < bytes_to_read;i++){
			buffer[i]=buffer_srec[bytes_read+i];
		}

		if (!setControl(XU_ID, 2, bytes_to_read, buffer))
		{
			reset();
			goto fail3;
		}

		bytes_left = bytes_left - bytes_to_read;
		bytes_read= bytes_read+bytes_to_read;
	}
	free(buffer);

	fprintf(stderr,"Sending data complete.\n");	
	fprintf(stderr, "Waiting for update completion...\n");	

	buf_control3[0] = 0x00;
	buf_control3[1] = 0x00;
	while(buf_control3[0]!= 0x01){
		if(buf_control3[0]==0x02){
			fprintf(stderr, "Flash write failure returned from SYS ARC! Try update again.\n");
			reset();
			goto fail3;
		}

		if (!getControl(XU_ID, 3, 2, buf_control3))
		{
			fprintf(stderr,"Unable to verify flash completion/failure. Board reset advised. Results of flashing unknown.\n");
			reset();
			goto fail3;
		}
	}
	
	fprintf(stderr,"Flash process completed.\n");
	fprintf(stderr,"Finishing firmware update.\n");
    reset();
	
	fprintf(stderr,"Resetting System...\n");

	buf_control7[0] = 0x01;

	if (!setControl(XU_ID, 7, 1, buf_control7))
	{
        fprintf(stderr,"Unable to automatically reset system. Manual reset advised. \n");
	}

	fprintf(stderr, "***update finish***\n");
    fd = open (logfile, O_CREAT | O_TRUNC | O_RDWR, 0666);
	if (fd < 0)
	{
		fprintf(stderr, "creat update_log.txt failure: %s \n", strerror(errno));
		goto fail3;
	}
	fprintf(stderr, "create log file success\n");	
	close(fd);
	ret = 0;

    fail3:
	free(buffer);
	fail1:
	free(buffer_srec);
	fail0:
	fclose(tFile);
	fail:
	close(video_fd);
	return ret;
}
