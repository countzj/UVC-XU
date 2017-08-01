#include <string.h>
#include <stdio.h>
//#include <iostream>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include "md5.h"
#ifdef LINUX
#include "linux/uvcvideo.h"
#include <linux/usb/video.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
//#include "UVCPrivateLinux.h"
#elif defined(WINDOWS)
#include "stdafx.h"
#endif
#include "bin_file.h"

int XU_ID = 6;

#ifdef WINDOWS
HWND ghApp=0;
DWORD g_dwGraphRegister=0;

IVideoWindow  * g_pVW = NULL;
IMediaControl * g_pMC = NULL;
IMediaEventEx * g_pME = NULL;
IGraphBuilder * g_pGraph = NULL;
ICaptureGraphBuilder2 * g_pCapture = NULL;
IKsControl *pKsControl2;

GUID HTest = {0x2381B1BA, 0x6A6E, 0x4DD5, {0xA1, 0x75, 0x68, 0x13, 0xF9, 0xEF, 0x5C, 0x1B}};	
/***************************************************************************
					function:GetInterfaces()
					version : 2.0
***************************************************************************/
HRESULT GetInterfaces(void)
{
    HRESULT hr;

    hr = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_INPROC,IID_IGraphBuilder, (void **) &g_pGraph);
	if (FAILED(hr)){
		return hr;
	}

    hr = CoCreateInstance (CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,IID_ICaptureGraphBuilder2, (void **) &g_pCapture);
	if (FAILED(hr)){
		return hr;
	}

    hr = g_pGraph->QueryInterface(IID_IMediaControl,(LPVOID *) &g_pMC);
	if (FAILED(hr)){
		return hr;
	}

    hr = g_pGraph->QueryInterface(IID_IVideoWindow, (LPVOID *) &g_pVW);
	if (FAILED(hr)){
		return hr;
	}

    hr = g_pGraph->QueryInterface(IID_IMediaEventEx, (LPVOID *) &g_pME);
	if (FAILED(hr)){
		return hr;
	}
    return hr;
}
#endif

#ifdef LINUX
int video_fd = -1;
#endif
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
	fprintf(stderr, "\nUsage:\nisi_firmware_update.exe [Bin File To Load]\n\n");
}
/***************************************************************************
					function:FindCaptureDevice()
					version : 2.0
***************************************************************************/
#ifdef WINDOWS
HRESULT FindCaptureDevice(IBaseFilter ** ppSrcFilter, int device, FILE * output)
{
	HRESULT hr = S_OK;
	IBaseFilter * pSrc = NULL;
	IMoniker* pMoniker = NULL;
	IMoniker* pMonikerPBag = NULL;
	IPropertyBag *pPropBag;
	ICreateDevEnum *pDevEnum = NULL;
	IEnumMoniker *pClassEnum = NULL;

	if (!ppSrcFilter){
		return E_POINTER;
	}

	hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
		IID_ICreateDevEnum, (void **)&pDevEnum);
	if (FAILED(hr)){
		fprintf(stderr, "Couldn't create system enumerator! hr=0x%x\n", hr);
	}

	if (SUCCEEDED(hr)){
		hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pClassEnum, 0);
		if (FAILED(hr)){
			fprintf(stderr, "Couldn't create class enumerator! hr=0x%x\n", hr);
		}
	}

	if (SUCCEEDED(hr)){
		if (pClassEnum == NULL){
			fprintf(stderr, "No video capture device detected\n");
			hr = E_FAIL;
		}
	}

	ULONG test = 12345;
	VARIANT varName;

	if (SUCCEEDED(hr)){
		bool loop = true;
		while (loop){
			hr = pClassEnum->Next(1, &pMonikerPBag, &test);
			if (hr == S_FALSE){
				fprintf(stderr, "Unable to access video capture device!\n");
				hr = E_FAIL;
				return hr;
			}

			hr = pMonikerPBag->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pPropBag);
			if (SUCCEEDED(hr)){
				fprintf(stderr, "Found device......... \n");
				loop = false;
			}else{
				fprintf(stderr, "Not Found Device \n");
			}
		}
	}

	if (SUCCEEDED(hr)){
        hr = pMonikerPBag->BindToObject(0,0,IID_IBaseFilter, (void**)&pSrc);
        if (FAILED(hr)){
			fprintf(stderr, "Couldn't bind moniker to filter object! hr=0x%x\n", hr);
        }
    }

	if (SUCCEEDED(hr)){
	    *ppSrcFilter = pSrc;
		(*ppSrcFilter)->AddRef();
	}
	
	if(pSrc){
		pSrc->Release();
		pSrc=NULL;
	}
    if(pMoniker){
		pMoniker->Release();
		pMoniker=NULL;
	}
    if(pDevEnum){
		pDevEnum->Release();
		pDevEnum=NULL;
	}
	if(pClassEnum){
		pClassEnum->Release();
		pClassEnum=NULL;
	}

    return hr;
}
#endif

#ifdef LINUX
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
#endif

bool getControl(int xu_id, int controlnumber, int size, uint8_t *value)
{
  #ifdef LINUX  
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
  #elif defined(WINDOWS)
  KSP_NODE ExtensionProp;
  ULONG bytesReturned = 0;

  HRESULT hr;
  
  ExtensionProp.Property.Set = HTest;
  ExtensionProp.Property.Id = controlnumber;
  ExtensionProp.Property.Flags = KSPROPERTY_TYPE_GET | KSPROPERTY_TYPE_TOPOLOGY;
  ExtensionProp.NodeId = xu_id;

  if ((hr = _uvcXUPrivate->ksControl->KsProperty((PKSPROPERTY)&ExtensionProp, sizeof(ExtensionProp), (PVOID)value, size, &bytesReturned)) != S_OK)
  {
    //logger(LOG_ERROR) << "UVCXU: " << _usb->id() << " KsProperty query failed. Error: " << _com_error(hr).ErrorMessage() << std::endl;
    return false;
  }

  if (bytesReturned != size)
  {
    //logger(LOG_ERROR) << "UVCXU: " << _usb->id() << " KsProperty query returned only " << bytesReturned << " bytes, but expected " << size << " bytes." << std::endl;
    return false;
  }
  #endif
  return true;  
}

bool setControl(int xu_id, int controlnumber, int size, uint8_t *value)
{
  #ifdef LINUX
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
  #elif defined(WINDOWS)
  KSP_NODE ExtensionProp;
  ULONG bytesReturned = 0;

  ExtensionProp.Property.Set = HTest;
  ExtensionProp.Property.Id = controlnumber;
  ExtensionProp.Property.Flags = KSPROPERTY_TYPE_SET | KSPROPERTY_TYPE_TOPOLOGY;
  ExtensionProp.NodeId = xu_id;

  if (_uvcXUPrivate->ksControl->KsProperty((PKSPROPERTY)&ExtensionProp, sizeof(ExtensionProp), (PVOID)value, size, &bytesReturned) != S_OK)
  {
    //logger(LOG_ERROR) << "UVCXU: " << _usb->id() << " KsProperty query failed." << std::endl;
    return false;
  }

  #endif
  
  return true;
}
  
void reset()
{
	uint8_t buf_control1[13];
	
    fprintf(stderr,"reset begin.\n");
	buf_control1[0] = 0x00;
	buf_control1[1] = 0x00;
	buf_control1[2] = 0x00;
	buf_control1[3] = 0x00;
	buf_control1[4] = 0x00;
	buf_control1[5] = 0x00;
	buf_control1[6] = 0x00;
	buf_control1[7] = 0x00;
	buf_control1[8] = 0x00;
	buf_control1[9] = 0x00;
	buf_control1[10] = 0x00;
	buf_control1[11] = 0x00;
    buf_control1[12] = 0x00;
	#ifdef WINDOWS
	HR = pKsControl->KsProperty((PKSPROPERTY)&WC1, sizeof(WC1), buf_control1, 13, &ulBytesReturned);
	if(HR!=S_OK){
		fprintf(stderr,"Unable to reset system to normal state. This will require a system reset. Try update again after.\n");		
	}
	#endif

	#ifdef LINUX
	if (!setControl(XU_ID, 1, 13, buf_control1))
	{
		fprintf(stderr,"Unable to reset system to normal state. This will require a system reset. Try update again after.\n");		
	}
	#endif
}
/***************************************************************************
					function:main()
					version : 2.0
***************************************************************************/
int main(int argc, char* argv[])
{
	int device = 0;
	char *srec;

    #ifdef WINDOWS
	HRESULT hr;
    IBaseFilter *pSrcFilter=NULL;
	#endif

	FILE *pFile=NULL;
	uint8_t *buffer;
	size_t result;

    #ifdef WINDOWS
	HRESULT HR;
	IKsTopologyInfo *pKsTopologyInfo;
	IUnknown *pUnk;
	IKsControl *pKsControl;
	DWORD uiNumNodes;
	GUID guidNodeType;
	ULONG ulBytesReturned;
	KSP_NODE s;
	KSP_NODE WC1;
	KSP_NODE RC1;
	KSP_NODE WC2;
	KSP_NODE RC3;
	KSP_NODE WC7;
	KSP_NODE RC8;
	#endif
	uint8_t buf_control1[13];
	uint8_t buf_control3[2];
	uint8_t buf_control7[1];
	uint8_t buf_control8[88];

	int immedia_XUFound=0;
	#ifdef WINDOWS
	ULONG trueNodeID;
	#endif

	uint32_t srecKBytes=256;
	uint32_t srec_size = srecKBytes*1024;
	uint32_t length_of_srec = 262144;

	FILE * firmware_output = NULL;

	if(argc !=2){
		fprintf(stderr, "Incorrect number of arguments \n");
		usage();
		return 1;
	}
	srec=argv[1];
	
	uint8_t buffer_srec[262144];
	for(int i = 0; i<srec_size; i++){
		buffer_srec[i]=0xFF;
	}

	bin_file_hdr header;
	char signature[MAX_SIZE_SIG+1];
	char svn_rev[MAX_SIZE_SVN_REV+1];
	char date[MAX_SIZE_DATE+1];
	FILE* tFile;
	if((srec[strlen(srec)-1]!= 'n') && (srec[strlen(srec)-2]!= 'i') && (srec[strlen(srec)-3]!= 'b')){   
			fprintf(stderr,"%s is not a .bin file\n", srec);
			return -1;
	 }else{
		if ((tFile=fopen(srec, "rb"))==NULL){
			fprintf(stderr,"Unable to open file %s\n", srec);
			return -1;
		}else{
			fprintf(stderr,"Opened file %s\n", srec);	 
		}
	 }

	fseek(tFile, 0, SEEK_END);
	uint32_t lSize = ftell(tFile);
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
	int first_address= header.start_address;

	if(sizeof(bin_file_hdr)!=header.hdr_len){
		fprintf(stderr, "sizeof(header) does not equal header.hdr_len\n");
		return -1;
	}
	
	uint8_t buffer_srec2[262144];
	for(int i = 0; i<srec_size; i++){
		buffer_srec2[i]=0xFF;
	}
	
	uint32_t data_length = lSize - header.hdr_len;
	fread(buffer_srec, data_length, 1, tFile);

    MD5 md5;
    fprintf(stderr, "MD5 CheckSum: %s\n", md5.digestString((char*)buffer_srec, data_length));
    if (strcmp(signature, md5.digestString((char*)buffer_srec, data_length))!=0){
        fprintf(stderr, "Chosen firmware file does not have the correct md5 checksum signature.\n");
        return 0;
    }

	fprintf(stderr, "\nRead in bin size: %d\n", data_length);
	length_of_srec = data_length;
	fprintf(stderr, "Set bin size: %d\n", length_of_srec);

	fclose(tFile);

	#ifdef WINDOWS	
    if(FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED))){  
        fprintf(stderr, "CoInitialize Fails! \n");
		return 1;
    } 

    hr = GetInterfaces();
    if (FAILED(hr)){
        fprintf(stderr,"Failed to get video interfaces!\n");
		return 1;
    }
	
    hr = g_pCapture->SetFiltergraph(g_pGraph);
    if (FAILED(hr)){
		fprintf(stderr,"Failed to set capture filter graph!\n");
		return 1;
    }

	hr = FindCaptureDevice(&pSrcFilter, device, firmware_output);
    if (FAILED(hr)){
	    fprintf(stderr, "Couldn't find capture device\n");
		return 1;
    }

	hr = g_pGraph->AddFilter(pSrcFilter, L"Video Capture");
    if (FAILED(hr)){
        fprintf(stderr, "Couldn't add capture filter to the graph!\n");
		return 1;
    }
	
	
	hr = g_pCapture->RenderStream (&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video, pSrcFilter, NULL, NULL);
    if (FAILED(hr)){
		fprintf(stderr,"Couldn't render the video capture stream!\n");
        pSrcFilter->Release();
		return 1;
    }
	
	HR = pSrcFilter->QueryInterface(__uuidof(IKsTopologyInfo), (VOID**)&pKsTopologyInfo);
	if(SUCCEEDED(HR)){
		if(pKsTopologyInfo->get_NumNodes(&uiNumNodes)==S_OK){
			for(UINT i =0; i < uiNumNodes+1; i++){
				if(pKsTopologyInfo->get_NodeType(i, &guidNodeType)==S_OK){
					if(guidNodeType==KSNODETYPE_DEV_SPECIFIC){
						HR = pKsTopologyInfo->CreateNodeInstance(i, __uuidof(IUnknown), (VOID**)&pUnk);
						if(HR==S_OK){						
							HR=pUnk->QueryInterface(__uuidof(IKsControl), (VOID**)&pKsControl);							
							if(HR==S_OK){								
								s.Property.Set=HTest;
								s.Property.Id=8;
								s.NodeId=i;
								
								s.Property.Flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_TOPOLOGY;
								HR = pKsControl->KsProperty((PKSPROPERTY)&s, sizeof(s), buf_control8, 88, &ulBytesReturned);
								
								if(HR==S_OK){
									immedia_XUFound = 1;
									trueNodeID=i;
								}
							}
						}
					}					
				}
			}
		}
	}
	
	if(immedia_XUFound == 1){
		fprintf(stderr,"Webcam Selected!\n");

		WC1.Property.Set=HTest;
		WC1.Property.Id=1;
		WC1.NodeId=trueNodeID;
		WC1.Property.Flags=KSPROPERTY_TYPE_SET|KSPROPERTY_TYPE_TOPOLOGY;

		RC1.Property.Set=HTest;
		RC1.Property.Id=1;
		RC1.NodeId=trueNodeID;
		RC1.Property.Flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_TOPOLOGY;

		WC2.Property.Set=HTest;
		WC2.Property.Id=2;
		WC2.NodeId=trueNodeID;
		WC2.Property.Flags=KSPROPERTY_TYPE_SET|KSPROPERTY_TYPE_TOPOLOGY;

		RC3.Property.Set=HTest;
		RC3.Property.Id=3;
		RC3.NodeId=trueNodeID;
		RC3.Property.Flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_TOPOLOGY;

		WC7.Property.Set=HTest;
		WC7.Property.Id=7;
		WC7.NodeId=trueNodeID;
		WC7.Property.Flags=KSPROPERTY_TYPE_SET|KSPROPERTY_TYPE_TOPOLOGY;
	}else{
		fprintf(stderr,"Webcam Not Selected!\n");
		return 1;
	}
    #endif

    #ifdef LINUX
    video_fd = open("/dev/video0", O_RDWR | O_NONBLOCK);
    if (video_fd == -1)
    {
        // couldn't find capture device
        printf("Opening Video device failed\n");
        return 1;
    }
	#endif

	uint8_t partition = 0x00;
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

    #ifdef WINDOWS
    RC8.Property.Set=HTest;
	RC8.Property.Id=8;
	RC8.NodeId=trueNodeID;
	RC8.Property.Flags=KSPROPERTY_TYPE_GET|KSPROPERTY_TYPE_TOPOLOGY;		
    printf("About to read control\n");
	HR = pKsControl->KsProperty((PKSPROPERTY)&RC8, sizeof(RC8), buf_control8, 88, &ulBytesReturned);

    if (HR==S_OK){		
        printf("Read Version Info\n");
    }
	else{
        printf("Can't Read Version Info\n");
    }
	#endif

	#ifdef LINUX
	//for (int i = 1; i <= 255; i++)
	//{
		printf("read version xu_id = %d\n", XU_ID);
		if (getControl(XU_ID, 8, 88, buf_control8))
		{
			printf("Read Version Info\n");
			//break;
			//XU_ID = i;
		}
		else
		{
	        printf("Can't Read Version Info\n");
	    }
		sleep(1);
	//}
	#endif

	buffer = (uint8_t*)malloc(1024);
	if (buffer == NULL){ 
		fprintf(stderr, "Memory Error. Could not allocate buffer size\n"); 
		return 1;
	}

	uint8_t * buffer_zero;
	buffer_zero = (uint8_t*)malloc(1024);
	if(buffer_zero==NULL){
		fprintf(stderr,"Memory Error. Could not allocate buffer size\n");
		return 1;
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
	#ifdef WINDOWS
	HR = pKsControl->KsProperty((PKSPROPERTY)&WC1, sizeof(WC1), buf_control1, 13, &ulBytesReturned);
	if(HR!=S_OK){
		fprintf(stderr,"Unable to initiate Firmware update. Please check to make sure video is not running.\n");
		return 1;
	}
	#endif

	#ifdef LINUX
	//for (int i = 1; i <= 255; i++)
	//{
		printf("initiate firmware xu_id=%d\n", XU_ID);
	    if (!setControl(XU_ID, 1, 13, buf_control1))
	    {
		    fprintf(stderr,"Unable to initiate Firmware update. Please check to make sure video is not running.\n");
		    return 1;
	    }
	    //if (i >= 255)
			//return 1;
	//}
	#endif
	
	fprintf(stderr, "System is ready to update. Sending binary data...\n");

	int bytes_left = srec_size;
	int bytes_to_read = 1024;
	int bytes_read=0;
	while(bytes_left > 0){
		for(int i =0; i < bytes_to_read;i++){
			buffer[i]=buffer_srec[bytes_read+i];
		}
		#ifdef WINDOWS
		HR = pKsControl->KsProperty((PKSPROPERTY)&WC2, sizeof(WC2), buffer, bytes_to_read, &ulBytesReturned);
		if(HR!=S_OK){
			reset();
			return 1;
		}
		#endif
		#ifdef LINUX
		if (!setControl(XU_ID, 2, bytes_to_read, buffer))
		{
			reset();
			return 1;
		}
		#endif
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
			return 1;
		}
		#ifdef WINDOWS
		HR = pKsControl->KsProperty((PKSPROPERTY)&RC3, sizeof(RC3), buf_control3, 2, &ulBytesReturned);
		if(HR!=S_OK){
			fprintf(stderr,"Unable to verify flash completion/failure. Board reset advised. Results of flashing unknown.\n");
			reset();
			return 1;
		}
		#endif

        #ifdef LINUX
		if (!getControl(XU_ID, 3, 2, buf_control3))
		{
			fprintf(stderr,"Unable to verify flash completion/failure. Board reset advised. Results of flashing unknown.\n");
			reset();
			return 1;
		}
		#endif
	}
	
	fprintf(stderr,"Flash process completed.\n");
	fprintf(stderr,"Finishing firmware update.\n");
    reset();
	
	fprintf(stderr,"Resetting System...\n");

	buf_control7[0] = 0x01;
	#ifdef WINDOWS
	HR = pKsControl->KsProperty((PKSPROPERTY)&WC7, sizeof(WC7), buf_control7, 1, &ulBytesReturned);
	if(HR!=S_OK){
		fprintf(stderr,"Unable to automatically reset system. Manual reset advised. \n");
		return 1;
	}
	#endif

	#ifdef LINUX
	if (!setControl(XU_ID, 7, 1, buf_control7))
	{
        fprintf(stderr,"Unable to automatically reset system. Manual reset advised. \n");
		return 1;
	}
	#endif

	printf("update finish\n");

	return 0;
}
