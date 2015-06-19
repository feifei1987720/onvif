/***********************************************************************
** Copyright (C) CHENQIANG . All rights reserved.
** Author			: CHEN QIANG
** Date				: 2014.03.05
** Name				: onvif_client.c
** Version			: 1.0
** Description			: onvif
*  //标注
* 1 ，先假设本机只有一个网卡
* 2 ，数据不进行过滤
*
*
***********************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "soapH.h"
#include "soapStub.h"
#include "nsmap.h"
#include <string.h>
#include "onvif_client.h"
#include "onvif_header.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <pthread.h>


#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <linux/sockios.h>
#include <linux/if_ether.h>
#include <unistd.h>

#define	MULTICAST_ADDRESS "soap.udp://239.255.255.250:3702"		//multicast address and port

static  pthread_mutex_t onvif_data_mutex= PTHREAD_MUTEX_INITIALIZER;
//pthread_mutex_t onvif_data_mutex ;


//发送onvif命令的超时时间
static int  g_onvif_time_out =  3 ;
#ifndef ONVIF_TIMEOUT
#define ONVIF_TIMEOUT
#endif


#if 0
#define ONVIF_DBP(fmt, args...) do { fprintf(stderr, "\n========= [%s ] %s line %d " fmt "\n", __FILE__, __FUNCTION__, __LINE__, ##args ); fflush(stderr);   } while ( 0 )
#else
#define ONVIF_DBP(fmt, args...)
#endif

extern  int DevPort ;
extern  char DevIp[MAX_LEN] ;
static int MaxCamera = 32;
ONVIF_CAMERAS g_onvifCameras  ;

typedef struct _IPAddress_
{
    char ip[MAX_LEN];
    char mask[MAX_LEN];
    char gateway[MAX_LEN];
}IPAddress;

IPAddress defaultSearchIPAddress[] = {
    {"192.0.0.5", "255.255.255.0", ""},
    {"192.168.0.5", "255.255.255.0", ""},
    {"192.168.1.5", "255.255.255.0", ""},
    {"192.168.2.5", "255.255.255.0", ""},
    {"192.168.88.5", "255.255.255.0", ""},
};

int GetNetInterfaceName(char *pInerfaceName);
int GetNetInterfaceInfo(const char *pInerfaceName, IPAddress *pIpAddress, char *pMac);
int Arpping(unsigned int test_ip, unsigned int from_ip, unsigned char *from_mac, const char *pInterfaceName);
int DelSecendIp(const char* pInterfaceName);
int SetSecendIp(const char* pInterfaceName, const IPAddress *pIpAddress);
int IsSameSubnet(const char *ip1, const char *mask1, const char *ip2, const char *mask2);
int FindNotExistIP(unsigned int *ip, char *mac, char *pInterface);

void ONVIF_GetIPAndPort(unsigned short *DevicePort,  char * DeviceIP, const char * ServiceXAddr)
{
    //去除''http://"前缀"
    char XAddrBuff[256] ={0};
    strncpy(XAddrBuff,ServiceXAddr +7,255);
    XAddrBuff[255] = '\0';

    int j = 0;
    bool bReadPort = false;//是否开始读取端口号
    unsigned short l_Port = 0;//临时存储端口
    char l_IPAddr[256] = {0};
    unsigned int i;
    for( i = 0; i <= strlen(XAddrBuff); ++i)
    {
        if(XAddrBuff[i] == '/'  &&  !bReadPort)
        {
            //默认端口
            l_Port = 80;
            break;
        }
        else if(XAddrBuff[i] == ':')
        {
            bReadPort = true;//开始读取端口号
        }
        else if(bReadPort)
        {
            if(isdigit(XAddrBuff[i]))
            {
                l_Port = 10 * l_Port +( XAddrBuff[i] - '0') ;
            }
            else
            {
                break;
            }
        }
        else
        {
            DeviceIP[j++] = XAddrBuff[i];
        }
    }
    DeviceIP[j] = '\0';
    *DevicePort = l_Port;
}


const char* soap_wsa_rand_uuid(struct soap *soap)
{
    const int uuidlen = 48;
    char *uuid = (char*)soap_malloc(soap, uuidlen);
    int r1, r2, r3, r4;
#ifdef WITH_OPENSSL
    r1 = soap_random;
    r2 = soap_random;
#else
    static int k = 0xFACEB00B;
    int lo = k % 127773;
    int hi = k / 127773;
# ifdef HAVE_GETTIMEOFDAY
    struct timeval tv;
    gettimeofday(&tv, NULL);
    r1 = 10000000 * tv.tv_sec + tv.tv_usec;
#else
    r1 = (int)time(NULL);
# endif
    k = 16807 * lo - 2836 * hi;
    if (k <= 0)
        k += 0x7FFFFFFF;
    r2 = k;
    k &= 0x8FFFFFFF;
    r2 += *(int*)soap->buf;
#endif
    r3 = soap_random;
    r4 = soap_random;
    //#ifdef HAVE_SNPRINTF
    //  soap_snprintf(uuid, uuidlen, "urn:uuid:%8.8x-%4.4hx-4%3.3hx-%4.4hx-%4.4hx%8.8x", r1, (short)(r2 >> 16), ((short)r2 >> 4) & 0x0FFF, ((short)(r3 >> 16) & 0x3FFF) | 0x8000, (short)r3, r4);
    //#else
    sprintf(uuid, "urn:uuid:%8.8x-%4.4hx-4%3.3hx-%4.4hx-%4.4hx%8.8x", r1, (short)(r2 >> 16), ((short)r2 >> 4) & 0x0FFF, ((short)(r3 >> 16) & 0x3FFF) | 0x8000, (short)r3, r4);
    //#endif
    DBGFUN1("soap_wsa_rand_uuid", "%s", uuid);
    return uuid;
}



typedef struct _IP
{
    char IPaddr[MAX_LEN];
}IP;

static IP Allip[256] = {0};

# if 0
int Compare(const void *elem1, const void *elem2)
{
    int i;
    IP *ptem1 = (IP*)elem1;
    IP *ptem2 = (IP*)elem2;
    //从ip地址的网段地址开始比较
    for (i = 2; i < 4; i++)
    {
        if (ptem1->IPblock[i] < ptem2->IPblock[i])
        {
            return -1;
        }
        else if (ptem1->IPblock[i] > ptem2->IPblock[i])
        {
            return 1;
        }
    }
    return 0;
}
#endif

bool Lookup(int count,IP * Allip,char * strip)
{

    if(0==count)
        return false ;
    int i;
    for(i=0; i!=count;++i)
    {
        if(!strncmp(Allip[i].IPaddr,strip,MAX_LEN))
        {
            return true ;
        }
        else
        {
            continue;
        }
    }

    return false ;

}

#if 0
int StartDiscovery( Discovery_Callback pCallback)
{
    int count = 0 ;
    int result = 0;
    wsdd__ProbeType req;
    struct  __wsdd__ProbeMatches resp;
    wsdd__ScopesType sScope;
    struct SOAP_ENV__Header header;
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 1;
    soap->recv_timeout = 1;
    soap->send_timeout = 1;
#endif

    soap_set_namespaces(soap, discoverynamespaces);
    soap_default_SOAP_ENV__Header(soap, &header);

    header.wsa__MessageID = (char * )soap_wsa_rand_uuid(soap);
    header.wsa__To = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
    header.wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
    soap->header = &header;

    soap_default_wsdd__ScopesType(soap, &sScope);
    sScope.__item = "";
    soap_default_wsdd__ProbeType(soap, &req);
    req.Scopes = &sScope;
    //命名空间和前缀必须要对应
    req.Types = "tds:Device";

    result = soap_send___wsdd__Probe(soap, MULTICAST_ADDRESS, NULL, &req);
    if(result != SOAP_OK)
    {
        printf("send probe err \r\n");
        return -1 ;
    }
    header.wsa__MessageID = (char * )soap_wsa_rand_uuid(soap);
    header.wsa__To = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
    header.wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
    soap->header = &header;
    soap_default_wsdd__ScopesType(soap, &sScope);
    sScope.__item = "";
    soap_default_wsdd__ProbeType(soap, &req);
    req.Scopes = &sScope;
    req.Types = "dn:NetworkVideoTransmitter";
    result = soap_send___wsdd__Probe(soap, MULTICAST_ADDRESS, NULL, &req);
    int j ;
    unsigned short DevicePort = 0 ;
    char DeviceIp[MAX_LEN] = {0};
    IP Allip[256] = {0};
    for(j =0;j<256 ;++j)
    {
        bool l_status = false ;   //表明探测状态，区分已经探测到设备后（探测设备完成）和探测超时
        result = soap_recv___wsdd__ProbeMatches(soap, &resp);
        ONVIF_DBP("################### return  new lib ===%d !!!!!!!!!!!#######################！\r\n",result);
        if(result == SOAP_OK)
        {
            if(soap->error)
            {
                ONVIF_DBP("#####################found device but err #######################！\r\n");
                fprintf(stderr,"soap error 1: %d, %s, %s\n", soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                result = soap->error;
                continue;
            }
            else
            {
                l_status = true ;
                ONVIF_DBP("###################new lib found device !!!!!!!!!!!#######################！\r\n");
                ONVIF_GetIPAndPort(&DevicePort,DeviceIp,resp.wsdd__ProbeMatches->ProbeMatch->XAddrs);
#if 0
                if(DeviceIp[0]!=0)
                {
                    sscanf(DeviceIp,"%d.%d.%d.%d",&allIP[j].IPblock[0],&allIP[j].IPblock[1],&allIP[j].IPblock[2],&allIP[j].IPblock[3]);
                    allIP[j].port = DevicePort;
                    strcpy(allIP[j].devuuid,resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address);
                }
#endif
                if(isdigit(DeviceIp[0]))
                {
                    if(!Lookup(count,Allip,DeviceIp))
                    {
                        pCallback(DeviceIp,DevicePort,resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address);
                        strncpy(Allip[count].IPaddr,DeviceIp,MAX_LEN);
                        count++ ;
                    }
                }

            }
        }
        else if (result == -1)
        {
            if(l_status ==false)
            {
                fprintf(stderr,"#####################time out #######################！\r\n");
                break;
            }
        }
    }
#if 0
    //ip地址排序
    qsort(allIP, count, sizeof(IP), Compare);
    for (i = 0; i < count; i++)
    {
        if(allIP[i].IPblock[0])
        {
            sprintf(DeviceIp,"%d.%d.%d.%d", allIP[i].IPblock[0], allIP[i].IPblock[1], allIP[i].IPblock[2], allIP[i].IPblock[3]);
            pCallback(DeviceIp,allIP[i].port,allIP[i].devuuid);
        }
    }
#endif
    fprintf(stderr,"###################NUM = %d !!!!!!!!!!!#######################！\r\n",count);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    ONVIF_DBP("##########################discovery finish ！！！！############################### !\n");
    return 0;
}
#endif

int FindNotExistIP(unsigned int *ip, char *mac, char *pInterface)
{
    int ret = -1;
    int i;
    unsigned int tempIp = *ip;
    for (i = 0; i < 24; ++i)
    {
        if (Arpping(tempIp, 0, mac, pInterface) == 0)
        {
            *ip = tempIp;
            ret = 0;
            break;
        }

        tempIp += (10 << 24);
    }

    return ret;
}

int StartDiscovery(Discovery_Callback pCallback, bool bMultiSegment)
{
    char szName[128] = "eth0";
    IPAddress localAddress;
    char szMac[6];
    int i, n;
    unsigned int iaIP;
    IPAddress tempIP;
    char szNameforSecondIp[128] = {0};

    if(NULL == pCallback)
    {

        return -1;
    }
   fprintf(stderr,"%s:%d\r\n",__func__,__LINE__);
    /*****************search by locoal ip**************/
   #if 0
    if(GetNetInterfaceName(szName) == -1)

    {
        fprintf(stderr,"%s:%d\r\n",__func__,__LINE__);
        return -1;
    }
#endif

    if(GetNetInterfaceInfo(szName, &localAddress, szMac) == -1)

    {
        fprintf(stderr,"%s:%d\r\n",__func__,__LINE__);
        return -1;
    }

    bzero(Allip, sizeof(Allip));
    OnvifDiscoveryByIp(pCallback, localAddress.ip, -1);


    if(!bMultiSegment)
    {
        return 0;
    }

    /*****************search by default ip**************/
    n = sizeof(defaultSearchIPAddress)/sizeof(IPAddress);
    snprintf(szNameforSecondIp, 128, "%s:0", szName);
    for(i = 0; i < n; ++i)
    {
        if(IsSameSubnet(localAddress.ip, localAddress.mask,
                        defaultSearchIPAddress[i].ip,
                        defaultSearchIPAddress[i].mask) == 0)
        {
            continue;
        }

        //找一个不存在的IP
        iaIP = inet_addr(defaultSearchIPAddress[i].ip); 
        if (FindNotExistIP(&iaIP, szMac, szName) == -1)
        {
            continue;
        }

        memcpy(&tempIP, &defaultSearchIPAddress[i], sizeof(IPAddress));
        strcpy(tempIP.ip, inet_ntoa(*(struct in_addr*)&iaIP));

        fprintf(stderr,"Discovery by local ip: %s  mask: %s\n", tempIP.ip, tempIP.mask);

        if(SetSecondIp(szNameforSecondIp, &tempIP) == -1)
        {
            continue;
        }


        OnvifDiscoveryByIp(pCallback, tempIP.ip, i);

        DelSecondIp(szNameforSecondIp);
    }
    /***************** end **************/

    return 0;
}

int OnvifDiscoveryByIp(Discovery_Callback pCallback, const char *ip, int nIndex)
{
    int count = 0;
    int result = 0;
    wsdd__ProbeType req;
    struct  __wsdd__ProbeMatches resp;
    wsdd__ScopesType sScope;
    struct SOAP_ENV__Header header;
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 1;
    soap->recv_timeout = 1;
    soap->send_timeout = 1;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_default_SOAP_ENV__Header(soap, &header);

    header.wsa__MessageID = (char * )soap_wsa_rand_uuid(soap);
    header.wsa__To = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
    header.wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
    soap->header = &header;

    //use "user" to assign local ip for search, this behavior need to modify source code
    soap->user = (void*)ip;

    soap_default_wsdd__ScopesType(soap, &sScope);
    sScope.__item = "";
    soap_default_wsdd__ProbeType(soap, &req);
    req.Scopes = &sScope;
    //命名空间和前缀必须要对应
    req.Types = "tds:Device";
    result = soap_send___wsdd__Probe(soap, MULTICAST_ADDRESS, NULL, &req);
    if(result != SOAP_OK)
    {
        printf("send probe err \r\n");
        return -1 ;
    }
    header.wsa__MessageID = (char * )soap_wsa_rand_uuid(soap);
    header.wsa__To = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
    header.wsa__Action = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
    soap->header = &header;
    soap->user = (void*)ip;
    soap_default_wsdd__ScopesType(soap, &sScope);
    sScope.__item = "";
    soap_default_wsdd__ProbeType(soap, &req);
    req.Scopes = &sScope;
    req.Types = "dn:NetworkVideoTransmitter";
    result = soap_send___wsdd__Probe(soap, MULTICAST_ADDRESS, NULL, &req);

    int j;
    unsigned short DevicePort = 0 ;
    char DeviceIp[MAX_LEN] = {0};
    for(j =0; j<256; ++j)
    {
        bool l_status = false ;   //表明探测状态，区分已经探测到设备后（探测设备完成）和探测超时
        result = soap_recv___wsdd__ProbeMatches(soap, &resp);
        ONVIF_DBP("################### return  new lib ===%d !!!!!!!!!!!#######################！\r\n",result);
        if(result == SOAP_OK)
        {
            if(soap->error)
            {
                ONVIF_DBP("#####################found device but err #######################！\r\n");
                ONVIF_DBP("soap error 1: %d, %s, %s\n", soap->error, *soap_faultcode(soap), *soap_faultstring(soap));
                result = soap->error;
                continue;
            }
            else
            {
                l_status = true ;
             //   fprintf(stderr,"###################device = %s！\r\n",resp.wsdd__ProbeMatches->ProbeMatch->XAddrs);
                ONVIF_GetIPAndPort(&DevicePort,DeviceIp,resp.wsdd__ProbeMatches->ProbeMatch->XAddrs);
#if 0
                if(DeviceIp[0]!=0)
                {
                    sscanf(DeviceIp,"%d.%d.%d.%d",&allIP[j].IPblock[0],&allIP[j].IPblock[1],&allIP[j].IPblock[2],&allIP[j].IPblock[3]);
                    allIP[j].port = DevicePort;
                    strcpy(allIP[j].devuuid,resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address);
                }
#endif
                if(isdigit(DeviceIp[0]))
                {
                    if(!Lookup(count,Allip,DeviceIp))
                    {
                        pCallback(DeviceIp, DevicePort, resp.wsdd__ProbeMatches->ProbeMatch->wsa__EndpointReference.Address, nIndex);
                        strncpy(Allip[count].IPaddr,DeviceIp,MAX_LEN);
                        count++;
                    }
                }

            }
        }
        else if (result == -1)
        {
            if(l_status ==false)
            {
                ONVIF_DBP("#####################time out #######################！\r\n");
                break;
            }
        }
    }
#if 0
    //ip地址排序
    qsort(allIP, count, sizeof(IP), Compare);
    for (i = 0; i < count; i++)
    {
        if(allIP[i].IPblock[0])
        {
            sprintf(DeviceIp,"%d.%d.%d.%d", allIP[i].IPblock[0], allIP[i].IPblock[1], allIP[i].IPblock[2], allIP[i].IPblock[3]);
            pCallback(DeviceIp,allIP[i].port,allIP[i].devuuid);
        }
    }
#endif
    fprintf(stderr,"###################NUM = %d !!!!!!!!!!!#######################！\r\n",count);
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    ONVIF_DBP("##########################discovery finish ！！！！############################### !\n");
    return 0;
}

//设置时间和时区
int  ONVIF_SetSystemTimeAndDate(Remote_Device_Info *pDeviceInfo,TimeAndDate * pTimeandDate)
{
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    char ServiceAddr[MAX_LEN] = {0};
    int l_Return = -1;
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    struct _tds__SetSystemDateAndTime * SetSystemDateAndTime = (struct _tds__SetSystemDateAndTime *)soap_malloc(soap,sizeof(struct _tds__SetSystemDateAndTime)) ;
    struct _tds__SetSystemDateAndTimeResponse * SetSystemDateAndTimeResponse = (struct _tds__SetSystemDateAndTimeResponse *) soap_malloc(soap,sizeof(struct _tds__SetSystemDateAndTimeResponse ));


    SetSystemDateAndTime->TimeZone = (struct tt__TimeZone *)soap_malloc(soap,sizeof(struct tt__TimeZone));
    SetSystemDateAndTime->TimeZone->TZ = (char *)soap_malloc(soap,sizeof(char)*MAX_STRING_LEN);
    SetSystemDateAndTime->UTCDateTime = (struct tt__DateTime *)soap_malloc(soap,sizeof(struct tt__DateTime));
    SetSystemDateAndTime->UTCDateTime->Date = (struct tt__Date *)soap_malloc(soap,sizeof(struct tt__Date));
    SetSystemDateAndTime->UTCDateTime->Time = (struct tt__Time *)soap_malloc(soap,sizeof(struct tt__Time));

    sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);

    strcpy(SetSystemDateAndTime->TimeZone->TZ,pTimeandDate->TZ);
    SetSystemDateAndTime->DaylightSavings = xsd__boolean__false_;
    SetSystemDateAndTime->DateTimeType = tt__SetDateTimeType__Manual;
    SetSystemDateAndTime->UTCDateTime->Date->Year = pTimeandDate->Year;
    SetSystemDateAndTime->UTCDateTime->Date->Month = pTimeandDate->Month;
    SetSystemDateAndTime->UTCDateTime->Date->Day = pTimeandDate->Day;
    SetSystemDateAndTime->UTCDateTime->Time->Hour = pTimeandDate->Hour;
    SetSystemDateAndTime->UTCDateTime->Time->Minute = pTimeandDate->Minute;
    SetSystemDateAndTime->UTCDateTime->Time->Second = pTimeandDate->Second;


    l_Return=  soap_call___tds__SetSystemDateAndTime(soap, ServiceAddr, NULL, SetSystemDateAndTime, SetSystemDateAndTimeResponse) ;
    //成功获取设备的日期和时间信息
    if(l_Return != SOAP_OK)
    {
       fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));

    }

    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    return l_Return ;
}





//获取时间和日期
int  ONVIF_GetSystemTimeAndDate(Remote_Device_Info *pDeviceInfo,TimeAndDate * pTimeandDate)
{
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    char ServiceAddr[MAX_LEN] = {0};
    int l_Return = -1;
    char TimeZone[MAX_LEN] = {0};
    struct _tds__GetSystemDateAndTime  GetSystemDateAndTime ;
    struct _tds__GetSystemDateAndTimeResponse GetSystemDateAndTimeResponse = {0};
    int num = 0;
    sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);

    l_Return=  soap_call___tds__GetSystemDateAndTime(soap, ServiceAddr, NULL, &GetSystemDateAndTime, &GetSystemDateAndTimeResponse) ;
    //成功获取设备的日期和时间信息
    if(l_Return == SOAP_OK)
    {
        if(NULL != GetSystemDateAndTimeResponse.SystemDateAndTime)
        {
            if(NULL != GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime)
            {
                pTimeandDate->isUTC = true;
                if(NULL != GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date)
                {
                    pTimeandDate->Year = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Year;
                    pTimeandDate->Month = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Month;
                    pTimeandDate->Day = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Day;

                }
                if(NULL != GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Time)
                {
                    pTimeandDate->Hour = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Time->Hour;
                    pTimeandDate->Minute = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Time->Minute;
                    pTimeandDate->Second = GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Time->Second;
                }

                fprintf(stderr,"%d:%d:%d\r\n",GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Year,GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Month,GetSystemDateAndTimeResponse.SystemDateAndTime->UTCDateTime->Date->Day);
            }
            else
            {
                pTimeandDate->isUTC = false;
            }
            if(GetSystemDateAndTimeResponse.SystemDateAndTime->TimeZone->TZ)
            {

                sprintf(pTimeandDate->TZ,GetSystemDateAndTimeResponse.SystemDateAndTime->TimeZone->TZ);
                fprintf(stderr,"tz =%s\r\n",pTimeandDate->TZ);

            }
            ONVIF_DBP("#####获取设备日期及时间信息成功\r\n");
        }

    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    return l_Return ;
}




//权限认证
int ONVIF_SetAuthenticationInformation(struct soap * soap_overall ,const char * Username, const char * Password)
{
    int l_Return = 0 ;
    l_Return = soap_wsse_add_UsernameTokenDigest(soap_overall,Username,Username,Password);
    if(SOAP_OK != l_Return)
    {
        ONVIF_DBP("$$$$$$$$$$$$$$$$$$$$$$$$$$$$$认证失败！$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$$");
    }
    return l_Return ;
}





int ONVIF_DeleteRemoteDevice(int  channel ,Remote_Device_Info *pDeviceInfo)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if(channel >= MAX_CAMERA)
        return -1;
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip  )
    {
        return -1;
    }
    if ( strlen(pDeviceInfo->ip) < 1 )
    {
        return -1;
    }

    g_onvifCameras.cameraIpInfo[channel].ip[0] = '\0';
    memset(&g_onvifCameras.onvif_info[channel], 0, sizeof(ONVIF_INFO));
    memset(&g_onvifCameras.CameraEvents[channel],0,sizeof(Onvif_Event_Item));
    memset(&g_onvifCameras.cameraIpInfo[channel],0,sizeof(CameraIpInfo));

    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return 1;
}



int ONVIF_AddRemoteDevice(unsigned int Channel ,Remote_Device_Info *pDeviceInfo)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip)
    {
        return -1;
    }
    if ( strlen(pDeviceInfo->ip) < 1 )
    {
        return -1;
    }
    if ( Channel >= MAX_CAMERA )
    {
        return -1;
    }
    if( strncmp(g_onvifCameras.cameraIpInfo[Channel].ip, pDeviceInfo->ip, MAX_IP_LEN - 1 ) != 0 ||
            strncmp(g_onvifCameras.cameraIpInfo[Channel].User, pDeviceInfo->userName, MAX_IP_LEN - 1) !=0 ||
            strncmp(g_onvifCameras.cameraIpInfo[Channel].Password, pDeviceInfo->password, MAX_IP_LEN - 1 ) !=0 )
    {
        ONVIF_DBP("===old ip=%s ;=====%s %d \n",g_onvifCameras.cameraIpInfo[Channel].ip);
        ONVIF_DBP( "===new ip=%s ;=====%s %d \n",pDeviceInfo->ip);
        strncpy(g_onvifCameras.cameraIpInfo[Channel].ip, pDeviceInfo->ip, MAX_IP_LEN - 1);
        strncpy(g_onvifCameras.cameraIpInfo[Channel].User, pDeviceInfo->userName, MAX_IP_LEN - 1);
        strncpy(g_onvifCameras.cameraIpInfo[Channel].Password, pDeviceInfo->password, MAX_IP_LEN - 1);
        g_onvifCameras.cameraIpInfo[Channel].port = pDeviceInfo->port;
    }
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return  Channel ;
}


int ONVIF_GetCameraIndex( const char *ip )
{
    ONVIF_DBP("################%s\n",ip);
    if (!ip)
    {
        return -1;
    }
    if ( strlen(ip) < 1 )
    {
        return -1;
    }
    int index = 0;
    for ( index = 0; index < MAX_CAMERA; index++ )
    {
        if ( strncmp(g_onvifCameras.cameraIpInfo[index].ip, ip, MAX_IP_LEN - 1 ) == 0 )
        {
            return index;
        }
    }
    ONVIF_DBP("################%s\n",ip);
    return -1;

}


int GetCapabilities(Remote_Device_Info *pDeviceInfo)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo ) {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    char ServiceAddr[MAX_LEN] ;

    //必须先设置设备的服务地址
    sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    ONVIF_DBP("ServiceAddr =======%s\n",ServiceAddr);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tds__GetCapabilities *p_tds__GetCapabilities =(struct _tds__GetCapabilities *)soap_malloc(soap, sizeof(struct _tds__GetCapabilities));
    memset(p_tds__GetCapabilities, 0, sizeof(struct _tds__GetCapabilities));
    p_tds__GetCapabilities->__sizeCategory = 0;
    p_tds__GetCapabilities->Category = soap_malloc(soap, sizeof(enum tt__CapabilityCategory));
    p_tds__GetCapabilities->Category = tt__CapabilityCategory__All;

    struct _tds__GetCapabilitiesResponse *p_tds__GetCapabilitiesResponse =(struct _tds__GetCapabilitiesResponse *)soap_malloc(soap, sizeof(struct _tds__GetCapabilitiesResponse));;
    memset(p_tds__GetCapabilitiesResponse, 0, sizeof(struct _tds__GetCapabilitiesResponse));

    soap_call___tds__GetCapabilities(soap, ServiceAddr, NULL, p_tds__GetCapabilities, p_tds__GetCapabilitiesResponse);

    if (!soap->error)
    {
        int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
        if ( (-1 != index) && index < MAX_CAMERA )
        {
            memset(&g_onvifCameras.onvif_info[index].tdsCapabilities, 0, sizeof(TdsCapabilities));
            if ( p_tds__GetCapabilitiesResponse->Capabilities->Analytics )
            {

            }

            if ( p_tds__GetCapabilitiesResponse->Capabilities->Device)
            {
                g_onvifCameras.onvif_info[index].tdsCapabilities.hasDeviceCapabilities = 1;

                if ( p_tds__GetCapabilitiesResponse->Capabilities->Device->XAddr )
                {

                    strncpy(g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.XAddr,
                            p_tds__GetCapabilitiesResponse->Capabilities->Device->XAddr, MAX_URL_LEN - 1);
                    g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.XAddr[MAX_URL_LEN - 1] = '\0';
                    ONVIF_DBP(" device xaddr %s \n", p_tds__GetCapabilitiesResponse->Capabilities->Device->XAddr);
                }

#if 0

                if ( p_tds__GetCapabilitiesResponse->Capabilities->Device->IO->RelayOutputs )

                {
                }

#endif



                if( p_tds__GetCapabilitiesResponse->Capabilities->Device->Network )
                {
                }
                if ( p_tds__GetCapabilitiesResponse->Capabilities->Device->System ) {

                    p_tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryBye;
                    p_tds__GetCapabilitiesResponse->Capabilities->Device->System->DiscoveryResolve;
                    p_tds__GetCapabilitiesResponse->Capabilities->Device->System->FirmwareUpgrade;

                    unsigned int sizeSupportedVersions = p_tds__GetCapabilitiesResponse->Capabilities->Device->System->__sizeSupportedVersions;
                    g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.sizeOfVersion =
                            sizeSupportedVersions > MAX_ONVIF_VERSION_SUPPORT - 1 ? MAX_ONVIF_VERSION_SUPPORT - 1 : sizeSupportedVersions;
                    sizeSupportedVersions = g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.sizeOfVersion;

                    int k = 0;
                    while( k < sizeSupportedVersions ) {
                        // ONVIF_DBP(" onvif version %d.%d \n",
                        //         p_tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions[k].Major,
                        //         p_tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions[k].Minor);

                        g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.onvifVersion[k].major =
                                p_tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions[k].Major;

                        g_onvifCameras.onvif_info[index].tdsCapabilities.deviceCapabilities.onvifVersion[k].minor=
                                p_tds__GetCapabilitiesResponse->Capabilities->Device->System->SupportedVersions[k].Minor;

                        k++;
                    }
                }

                if ( p_tds__GetCapabilitiesResponse->Capabilities->Events ) {

                    g_onvifCameras.onvif_info[index].tdsCapabilities.hasEventCapabilities = 1;

                    g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.WSPausableSubscriptionManagerInterfaceSupport
                            = p_tds__GetCapabilitiesResponse->Capabilities->Events->WSPausableSubscriptionManagerInterfaceSupport;

                    g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.WSPullPointSupport
                            = p_tds__GetCapabilitiesResponse->Capabilities->Events->WSPullPointSupport;

                    g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.WSSubscriptionPolicySupport
                            = p_tds__GetCapabilitiesResponse->Capabilities->Events->WSSubscriptionPolicySupport;

                    if ( p_tds__GetCapabilitiesResponse->Capabilities->Events->XAddr ) {
                        strncpy(g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr,
                                p_tds__GetCapabilitiesResponse->Capabilities->Events->XAddr, MAX_URL_LEN - 1);
                        g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr[MAX_URL_LEN - 1] = '\0';

                        //   ONVIF_DBP(" event xaddr %s \n", p_tds__GetCapabilitiesResponse->Capabilities->Events->XAddr );
                    }
                }
                if ( p_tds__GetCapabilitiesResponse->Capabilities->Imaging ) {

                    g_onvifCameras.onvif_info[index].tdsCapabilities.hasImagingCapabilities = 1;

                    if ( p_tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr ) {
                        strncpy(g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr,
                                p_tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr, MAX_URL_LEN - 1);

                        g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr[MAX_URL_LEN - 1] = '\0';
                        //  ONVIF_DBP(" imaging xaddr %s \n", p_tds__GetCapabilitiesResponse->Capabilities->Imaging->XAddr);
                    }
                }
                if ( p_tds__GetCapabilitiesResponse->Capabilities->Media ) {
                    g_onvifCameras.onvif_info[index].tdsCapabilities.hasMediaCapabilities = 1;
                    if ( p_tds__GetCapabilitiesResponse->Capabilities->Media->XAddr ) {
                        strncpy(g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr,
                                p_tds__GetCapabilitiesResponse->Capabilities->Media->XAddr, MAX_URL_LEN - 1);
                        g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[MAX_URL_LEN - 1] = '\0';
                        ONVIF_DBP(" media xaddr %s \n", p_tds__GetCapabilitiesResponse->Capabilities->Media->XAddr );
                    }
                    if ( p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities ) {
                        if ( p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTPMulticast ) {
                            g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.StreamingCapabilities.RTPMulticast
                                    = *p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTPMulticast;
                            //   ONVIF_DBP(" streaming rtp multicast \n");
                        }

                        if ( p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP ) {
                            g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.StreamingCapabilities.RTP_USCORERTSP_USCORETCP
                                    = *p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORERTSP_USCORETCP;

                            //   ONVIF_DBP(" streaming rtp RTP_USCORERTSP_USCORETCP \n");
                        }

                        if ( p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP ) {
                            g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.StreamingCapabilities.RTP_USCORETCP
                                    = *p_tds__GetCapabilitiesResponse->Capabilities->Media->StreamingCapabilities->RTP_USCORETCP;

                            //  ONVIF_DBP(" streaming rtp RTP_USCORETCP \n");
                        }
                    }
                }

                if ( p_tds__GetCapabilitiesResponse->Capabilities->PTZ ) {

                    g_onvifCameras.onvif_info[index].tdsCapabilities.hasPTZCapabilities = 1;
                    if ( p_tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr ) {
                        strncpy(g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,
                                p_tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr, MAX_URL_LEN - 1);

                        g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[MAX_URL_LEN - 1] = '\0';

                        // ONVIF_DBP(" ptz xaddr %s \n", p_tds__GetCapabilitiesResponse->Capabilities->PTZ->XAddr);
                    }
                }
            }
        }
    }

    if (soap->error) {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}



int ONVIF_GetDeviceInformation( Remote_Device_Info *pDeviceInfo )
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    char ServiceAddr[MAX_LEN] ;
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);


    struct _tds__GetDeviceInformation tds__GetDeviceInformation;
    struct _tds__GetDeviceInformationResponse p_tds__GetDeviceInformationResponse = {0};
    //必须先设置设备的服务地址
    sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    soap_call___tds__GetDeviceInformation(soap, ServiceAddr, NULL, &tds__GetDeviceInformation, &p_tds__GetDeviceInformationResponse);

    if ( ! soap->error )
    {
        int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
        if ( (-1 != index) && index < MAX_CAMERA )
        {
            memset(&g_onvifCameras.onvif_info[index].onvif_deviceinfo.Manufacturer, 0, sizeof(char)*MAX_STRING_LENGTH);
            memset(&g_onvifCameras.onvif_info[index].onvif_deviceinfo.FirmwareVersion, 0, sizeof(char)*MAX_STRING_LENGTH);
            memset(&g_onvifCameras.onvif_info[index].onvif_deviceinfo.SerialNumber, 0, sizeof(char)*MAX_STRING_LENGTH);
            memset(&g_onvifCameras.onvif_info[index].onvif_deviceinfo.HardwareId, 0, sizeof(char)*MAX_STRING_LENGTH);
            memset(&g_onvifCameras.onvif_info[index].onvif_deviceinfo.Model, 0, sizeof(char)*MAX_STRING_LENGTH);
            if (p_tds__GetDeviceInformationResponse.Manufacturer )
            {
                strncpy(g_onvifCameras.onvif_info[index].onvif_deviceinfo.Manufacturer,
                        p_tds__GetDeviceInformationResponse.Manufacturer, MAX_STRING_LENGTH - 1);
                g_onvifCameras.onvif_info[index].onvif_deviceinfo.Manufacturer[MAX_STRING_LENGTH - 1] = '\0';
                // ONVIF_DBP("Manufacturer  %s \n", p_tds__GetDeviceInformationResponse.Manufacturer);
            }
            if (p_tds__GetDeviceInformationResponse.FirmwareVersion )
            {
                strncpy(g_onvifCameras.onvif_info[index].onvif_deviceinfo.FirmwareVersion,
                        p_tds__GetDeviceInformationResponse.FirmwareVersion, MAX_STRING_LENGTH - 1);
                g_onvifCameras.onvif_info[index].onvif_deviceinfo.FirmwareVersion[MAX_STRING_LENGTH - 1] = '\0';
                // ONVIF_DBP("FirmwareVersion  %s \n", p_tds__GetDeviceInformationResponse.FirmwareVersion );
            }
            if (p_tds__GetDeviceInformationResponse.SerialNumber )
            {
                strncpy(g_onvifCameras.onvif_info[index].onvif_deviceinfo.SerialNumber,
                        p_tds__GetDeviceInformationResponse.SerialNumber, MAX_STRING_LENGTH - 1);
                g_onvifCameras.onvif_info[index].onvif_deviceinfo.SerialNumber[MAX_STRING_LENGTH - 1] = '\0';
                // ONVIF_DBP("SerialNumber  %s \n", p_tds__GetDeviceInformationResponse.SerialNumber );
            }
            if (p_tds__GetDeviceInformationResponse.HardwareId )
            {
                strncpy(g_onvifCameras.onvif_info[index].onvif_deviceinfo.HardwareId,
                        p_tds__GetDeviceInformationResponse.HardwareId, MAX_STRING_LENGTH - 1);
                g_onvifCameras.onvif_info[index].onvif_deviceinfo.HardwareId[MAX_STRING_LENGTH - 1] = '\0';
                // ONVIF_DBP("HardwareId  %s \n", p_tds__GetDeviceInformationResponse.HardwareId );
            }
            if (p_tds__GetDeviceInformationResponse.Model )
            {
                strncpy(g_onvifCameras.onvif_info[index].onvif_deviceinfo.Model,
                        p_tds__GetDeviceInformationResponse.Model, MAX_STRING_LENGTH - 1);
                g_onvifCameras.onvif_info[index].onvif_deviceinfo.Model[MAX_STRING_LENGTH - 1] = '\0';
                // ONVIF_DBP("HardwareId  %s \n", p_tds__GetDeviceInformationResponse.HardwareId );
            }
            g_onvifCameras.onvif_info[index].onvif_deviceinfo.Status = true;

        }
    }
    if (soap->error)
    {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;
}





int GetProfiles(Remote_Device_Info *pDeviceInfo)
{

    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo ) {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( ( index < 0 ) || index > MAX_CAMERA -1 )
    {
        ONVIF_DBP(" ###############################没有IP地址 index = %d \n", index );
        return -1;
    }
    ONVIF_DBP("!!!!!!!!!!!!!!!!!!!!!!!%s",g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr);
    char ServiceAddr[MAX_LEN] ;
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);


    //权限认
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _trt__GetProfiles trt__GetProfiles;

    struct _trt__GetProfilesResponse *p_trt__GetProfilesResponse = (struct _trt__GetProfilesResponse *)soap_malloc(soap, sizeof(struct _trt__GetProfilesResponse));;
    memset(p_trt__GetProfilesResponse, 0, sizeof(struct _trt__GetProfilesResponse));
    if(!isalnum(g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[0]))
    {
        sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
        soap_call___trt__GetProfiles(soap, ServiceAddr, NULL, &trt__GetProfiles, p_trt__GetProfilesResponse);
        ONVIF_DBP(" &&&&&&&&&&&&&媒体服务地址为空 \n");
    }
    else
    {
        soap_call___trt__GetProfiles(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr, NULL, &trt__GetProfiles, p_trt__GetProfilesResponse);
    }

    if ( !soap->error )
    {
        // int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
        if ( (-1 != index) && index < MAX_CAMERA )
        {
            g_onvifCameras.onvif_info[index].profileSize = p_trt__GetProfilesResponse->__sizeProfiles;
            memset(g_onvifCameras.onvif_info[index].Profiles, 0, g_onvifCameras.onvif_info[index].profileSize);
            ONVIF_DBP(" profiles size %d \n", p_trt__GetProfilesResponse->__sizeProfiles);
            int profileIndex = 0;
            for ( profileIndex = 0; profileIndex < p_trt__GetProfilesResponse->__sizeProfiles; profileIndex++ )
            {
                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].Name )
                {
                    strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].profilename,
                            p_trt__GetProfilesResponse->Profiles[profileIndex].Name, MAX_PROF_TOKEN - 1 );
                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].profilename[MAX_PROF_TOKEN - 1] = '\0';

                    ONVIF_DBP("profile name %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].Name );
                }

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].token ) {
                    strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].profiletoken,
                            p_trt__GetProfilesResponse->Profiles[profileIndex].token, MAX_PROF_TOKEN - 1 );
                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].profiletoken[MAX_PROF_TOKEN - 1] = '\0';

                    ONVIF_DBP("profile token %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].token );
                }

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration ) {
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->SourceToken ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vsourcetoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->SourceToken,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vsourcetoken[MAX_PROF_TOKEN - 1] = '\0';

                        //       ONVIF_DBP( " VideoSourceToken %s \n",
                        //                 p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->SourceToken);
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->Name ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vname,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->Name,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vname[MAX_PROF_TOKEN - 1] = '\0';
                        //       ONVIF_DBP( " VideoSourceName %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->Name);
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->token  ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vtoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->token,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AVSC.Vtoken[MAX_PROF_TOKEN - 1] = '\0';
                        //       ONVIF_DBP( " VideoSourcetoken %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoSourceConfiguration->token);
                    }
                }

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration ) {

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Name ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.VEname,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Name,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.VEname[MAX_PROF_TOKEN - 1] = '\0';

                        //       ONVIF_DBP("videoEncoderName %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Name);
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->token ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.VEtoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->token,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.VEtoken[MAX_PROF_TOKEN - 1] = '\0';

                        // ONVIF_DBP(stderr,"videoEncodertoken %s \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->token);
                    }

                    //   ONVIF_DBP("Encoding %d \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Encoding );


                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution ) {

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.Rwidth =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Width;

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.Rheight =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Height;

                        //       ONVIF_DBP(" width %d height %d \n",
                        //                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Width,
                        //                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Height );
                    }

                    //编码类型
                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.Vencoder = p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Encoding;
                    ONVIF_DBP("AESC.Vencoder %d \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Encoding );
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->H264 )
                    {
                        //     ONVIF_DBP("h264 govlenght %d \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->H264->GovLength );
                        //     ONVIF_DBP("h264 profile %d \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->H264->H264Profile );




                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.H264govlength =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->H264->GovLength;

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.H264profile =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->H264->H264Profile;
                    }


                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.VEquality =
                            p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Quality;


                    //   ONVIF_DBP("quality %f \n", p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Quality );

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl ) {

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.frameratelimit =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->FrameRateLimit;

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.encodinginterval =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->EncodingInterval;

                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.bitratelimit =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->BitrateLimit;

                        //    ONVIF_DBP(" framelimit %d encodingInterval %d bitratelimit %d \n",
                        //              p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->FrameRateLimit,
                        //               p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->EncodingInterval,
                        //              p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->RateControl->BitrateLimit );

                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution)
                    {
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.Rwidth =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Width;
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AESC.Rheight =
                                p_trt__GetProfilesResponse->Profiles[profileIndex].VideoEncoderConfiguration->Resolution->Height;
                    }

                }
#if 0

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration )
                {
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->Name ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Aname,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->Name,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Aname[MAX_PROF_TOKEN - 1] = '\0';
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->token ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Atoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->token,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Atoken[MAX_PROF_TOKEN - 1] = '\0';

                    }
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->SourceToken ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Asourcetoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->SourceToken,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AASC.Asourcetoken[MAX_PROF_TOKEN - 1] = '\0';

                    }

                }

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration )
                {

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->Name ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.AEname,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->Name,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.AEname[MAX_PROF_TOKEN - 1] = '\0';

                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->token ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.AEtoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].AudioSourceConfiguration->token,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.AEtoken[MAX_PROF_TOKEN - 1] = '\0';

                    }


                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.AEencoding =
                            p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->Encoding;

                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.samplerate =
                            p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->SampleRate;

                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].AAEC.bitrate =
                            p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->Bitrate;

                    //  ONVIF_DBP("audio encoding %d sample %d bitrate %d \n",
                    //         p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->Encoding,
                    //         p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->SampleRate,
                    //         p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->Bitrate );

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->SessionTimeout ) {
                        //    ONVIF_DBP("audio encoder session timeout %d \n",
                        //              p_trt__GetProfilesResponse->Profiles[profileIndex].AudioEncoderConfiguration->SessionTimeout);
                    }

                }
#endif

                if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration ) {

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->token ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZtoken ,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->token,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZtoken[MAX_PROF_TOKEN - 1] = '\0';
                    }


                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->Name ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZname,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->Name,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZname[MAX_PROF_TOKEN - 1] = '\0';
                    }
#if 0
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->NodeToken ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZnodetoken,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->NodeToken,
                                MAX_PROF_TOKEN - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZnodetoken[MAX_PROF_TOKEN - 1] = '\0';
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsolutePantTiltPositionSpace ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultAbsolutePantTiltPositionSpace,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsolutePantTiltPositionSpace,
                                MAX_STRING_LENGTH - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultAbsolutePantTiltPositionSpace[MAX_STRING_LENGTH - 1] = '\0';
                        //    ONVIF_DBP("DefaultAbsolutePantTiltPositionSpace %s \n",
                        //              p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsolutePantTiltPositionSpace );
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsoluteZoomPositionSpace ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultAbsoluteZoomPositionSpace,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsoluteZoomPositionSpace,
                                MAX_STRING_LENGTH - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultAbsoluteZoomPositionSpace[MAX_STRING_LENGTH - 1] = '\0';


                        //    ONVIF_DBP("DefaultAbsoluteZoomPositionSpace %s \n",
                        //             p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultAbsoluteZoomPositionSpace );
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousPanTiltVelocitySpace ) {
                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultContinuousPanTiltVelocitySpace,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousPanTiltVelocitySpace,
                                MAX_STRING_LENGTH - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultAbsoluteZoomPositionSpace[MAX_STRING_LENGTH - 1] = '\0';

                        //    ONVIF_DBP("DefaultContinuousPanTiltVelocitySpace %s \n",
                        //             p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousPanTiltVelocitySpace );
                    }

                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousZoomVelocitySpace ) {

                        strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultContinuousZoomVelocitySpace,
                                p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousZoomVelocitySpace,
                                MAX_STRING_LENGTH - 1);
                        g_onvifCameras.onvif_info[index].Profiles[profileIndex].DefaultContinuousZoomVelocitySpace[MAX_STRING_LENGTH - 1] = '\0';


                        //    ONVIF_DBP("DefaultContinuousZoomVelocitySpace %s \n",
                        //             p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultContinuousZoomVelocitySpace );
                    }

#endif
                    if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed ) {

                        if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt ) {

                            if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->space ) {
                                //         ONVIF_DBP("panTileSpace %s \n",
                                //                   p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->space);
                            }

                            g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZspeedx =
                                    p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->x;

                            g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZspeedy =
                                    p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->y;

                            //     ONVIF_DBP("PTZspeedx %f ,,PTZspeedy %f \n",
                            //               p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->x,
                            //               p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->PanTilt->y );
                        }

                        if ( p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->Zoom ) {
                            if (p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->Zoom->space ) {
                                //        ONVIF_DBP("zoom space %s \n",
                                //                 p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->Zoom->space);
                            }

                            g_onvifCameras.onvif_info[index].Profiles[profileIndex].PTZzoom =
                                    p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->Zoom->x;

                            //     ONVIF_DBP("zoom x %f \n", p_trt__GetProfilesResponse->Profiles[profileIndex].PTZConfiguration->DefaultPTZSpeed->Zoom->x);
                        }

                    }
                }
#if 1
                if( p_trt__GetProfilesResponse->Profiles[profileIndex].VideoAnalyticsConfiguration )
                {
                    if(  p_trt__GetProfilesResponse->Profiles[profileIndex].VideoAnalyticsConfiguration->token)
                {
                    strncpy(g_onvifCameras.onvif_info[index].Profiles[profileIndex].VAtoken,
                            p_trt__GetProfilesResponse->Profiles[profileIndex].VideoAnalyticsConfiguration->token,
                            MAX_PROF_TOKEN - 1);
                    g_onvifCameras.onvif_info[index].Profiles[profileIndex].VAtoken[ MAX_PROF_TOKEN - 1] ='\0';
                 }

                }
#endif


            }
        }

    }

    if (soap->error) {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);

    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}


int ONVIF_GetProfiles(Remote_Device_Info *pDeviceInfo, Onvif_Profiles *pGetProfilesRespone)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pGetProfilesRespone ) {
        return -1;
    }
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    int index = 0;
    int ret = 0;
    pthread_mutex_lock(&onvif_data_mutex);
    ONVIF_DBP("@@@@@@@@@@@@@@@@@@@ %s got mutex \n",pDeviceInfo->ip);
    index = ONVIF_GetCameraIndex(pDeviceInfo);
    if ( index < 0 )
    {
        pthread_mutex_unlock(&onvif_data_mutex);
        return -1;
    }
    int result  = GetCapabilities(pDeviceInfo);
    if ( 1 != result )
    {
        pthread_mutex_unlock(&onvif_data_mutex);
        return -1;
    }
    ONVIF_GetDeviceInformation(pDeviceInfo);
    result = GetProfiles(pDeviceInfo);
    if ( 1 != result )
    {
        pthread_mutex_unlock(&onvif_data_mutex);
        return -1;
    }
    pthread_mutex_unlock(&onvif_data_mutex);
    pGetProfilesRespone->sizeOfProfiles = g_onvifCameras.onvif_info[index].profileSize;
    int k = 0;
    for ( k = 0; k < pGetProfilesRespone->sizeOfProfiles; k++ )
    {

        if (g_onvifCameras.onvif_info[index].Profiles[k].profilename[0] ) {
            strncpy(pGetProfilesRespone->Onvif_Profiles[k].profieName,
                    g_onvifCameras.onvif_info[index].Profiles[k].profilename, MAX_LEN - 1);
            pGetProfilesRespone->Onvif_Profiles[k].profieName[MAX_LEN - 1] = '\0';
        }

        if ( g_onvifCameras.onvif_info[index].Profiles[k].profiletoken[0] ) {
            strncpy(pGetProfilesRespone->Onvif_Profiles[k].profileToken,
                    g_onvifCameras.onvif_info[index].Profiles[k].profiletoken, MAX_LEN - 1);
            pGetProfilesRespone->Onvif_Profiles[k].profileToken[MAX_LEN - 1] = '\0';

        }

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoConfig.Name,
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEname, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.Name[MAX_LEN - 1] = '\0';

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEtoken, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.token[MAX_LEN - 1] = '\0';

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.videoEncoding
                = g_onvifCameras.onvif_info[index].Profiles[k].AESC.Vencoder;


        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.width =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rwidth;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.height =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rheight;

        int encoding = g_onvifCameras.onvif_info[index].Profiles[k].AESC.Vencoder;

        if ( 2 == encoding )
        { // h264
            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.h264.GovLength =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.H264govlength;

            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.h264.H264Profile =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.H264profile;
        }
        if ( 1 == encoding )
        {
            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.mpeg4.GovLength =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.Mpeggovlength;

            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.mpeg4.Mpeg4Profile =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.Mpegprofile;
        }

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.FrameRateLimit =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.frameratelimit;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.BitrateLimit =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.bitratelimit;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.EncodingInterval =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.encodinginterval;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.width =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rwidth;
        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.height =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rheight;


        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.BitrateLimit =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.bitratelimit;

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoSourceToken,
                g_onvifCameras.onvif_info[index].Profiles[k].AVSC.Vsourcetoken, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoSourceToken[MAX_LEN - 1] = '\0';

        // PTZ
        strncpy(pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].PTZtoken,
                sizeof(pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.token) - 1 );

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultPanTiltSpeedX =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZspeedx;

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultPanTiltSpeedY =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZspeedy;

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultZoomSpeedX  =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZzoom;

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].VAtoken,
                sizeof(pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token) - 1 );
        pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token[MAX_LEN - 1] = '\0';



    }
    //pthread_mutex_unlock(&onvif_data_mutex);
    //Onvif_Ptz_Config ptzConfig;
    //memset(&ptzConfig, 0, sizeof(Onvif_Ptz_Config));
    //ONVIF_PtzGetConfiguration(pDeviceInfo, &ptzConfig);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    //memcpy(&g_onvifCameras.onvif_info[index].Profiles[0].ptzConfig, &ptzConfig, sizeof(ptzConfig));
    return pGetProfilesRespone->sizeOfProfiles ? 1 : -1;

}


int ONVIF_GetStreamUri( Remote_Device_Info *pDeviceInfo, const  char *profileToken, char *urlBuf)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( (index < 0) || index > MAX_CAMERA -1 )
    {
        ONVIF_DBP(" &&&&&&&&&&&&没有IP地址\n");
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&媒体服务地址为空 \n");
        return -1;
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);
#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    char ServiceAddr[MAX_LEN];
    struct tt__Transport Transport;

    // changed by cq 2015-03-25
    //Transport.Protocol = tt__TransportProtocol__UDP;
    Transport.Protocol = tt__TransportProtocol__TCP;
    Transport.Tunnel=NULL;
    struct tt__StreamSetup  StreamSetup;
    StreamSetup.Transport = (struct tt__Transport *)soap_malloc(soap, sizeof( struct tt__Transport));
    memset(StreamSetup.Transport, 0, sizeof(struct tt__Transport));
    StreamSetup.Stream = tt__StreamType__RTP_Unicast;
    StreamSetup.Transport = &Transport;
    StreamSetup.__size = 0;
    StreamSetup.__any= NULL;
    StreamSetup.__anyAttribute= NULL;


    struct _trt__GetStreamUri *p_trt__GetStreamUri = (struct _trt__GetStreamUri *)soap_malloc(soap, sizeof(struct _trt__GetStreamUri));
    memset(p_trt__GetStreamUri, 0, sizeof(struct _trt__GetStreamUri));

    p_trt__GetStreamUri->StreamSetup  = &StreamSetup;
    p_trt__GetStreamUri->ProfileToken = (char*)soap_malloc(soap, 128);
    memset(p_trt__GetStreamUri->ProfileToken, 0, 128);

    if ( profileToken )
    {
        strncpy(p_trt__GetStreamUri->ProfileToken, profileToken, 127);
    }

    struct _trt__GetStreamUriResponse *p_trt__GetStreamUriResponse =
            (struct _trt__GetStreamUriResponse *)soap_malloc(soap, sizeof(struct _trt__GetStreamUriResponse));

            memset(p_trt__GetStreamUriResponse, 0, sizeof(struct _trt__GetStreamUriResponse));

            if(!isalnum(g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[0]))
            {
                sprintf(ServiceAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
                soap_call___trt__GetStreamUri(soap, ServiceAddr, NULL, p_trt__GetStreamUri, p_trt__GetStreamUriResponse);
                ONVIF_DBP(" &&&&&&&&&&&&&媒体服务地址为空 \n");
            }
            else
            {
                soap_call___trt__GetStreamUri(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr, NULL, p_trt__GetStreamUri, p_trt__GetStreamUriResponse);
            }
            if ( !soap->error )
            {

                ONVIF_DBP("&&&&&&&&&&&&&&&&&&&&&get Uri success : %s\nTimeout %d\nInvalidAfterConnect %d\nInvalidAfterReboot %d \n",
                          p_trt__GetStreamUriResponse->MediaUri->Uri, p_trt__GetStreamUriResponse->MediaUri->Timeout,
                          p_trt__GetStreamUriResponse->MediaUri->InvalidAfterConnect,
                          p_trt__GetStreamUriResponse->MediaUri->InvalidAfterReboot);

                strcpy(urlBuf, p_trt__GetStreamUriResponse->MediaUri->Uri);
            }

            if (soap->error)
            {
                ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
            }
            soap_destroy(soap);
            soap_end(soap);
            soap_done(soap);
            soap->error = (soap->error == 0 ? 1 : -1);
            ONVIF_DBP("################%s\n",pDeviceInfo->ip);
            return soap->error;
}


int ONVIF_GetDeviceInformationFromCache(Remote_Device_Info *pDeviceInfo, Onvif_DeviceInfo *pGetDevinfoRespone)
{

    if ( !pDeviceInfo || !pGetDevinfoRespone)
    {
        ONVIF_DBP("################%s\n",pDeviceInfo->ip);
        return -1;
    }
    int index = 0;
    int ret = 0;
    index = ONVIF_GetCameraIndex(pDeviceInfo);
    if ( index < 0 || index > MAX_CAMERA - 1)
    {
        ONVIF_DBP("################%s\n",pDeviceInfo->ip);
        return -1;
    }

    if(g_onvifCameras.onvif_info[index].onvif_deviceinfo.Status)
    {
        if ( g_onvifCameras.onvif_info[index].onvif_deviceinfo.FirmwareVersion[0] )
        {
            strncpy(pGetDevinfoRespone->FirmwareVersion,
                    g_onvifCameras.onvif_info[index].onvif_deviceinfo.FirmwareVersion, MAX_STRING_LEN - 1);
            pGetDevinfoRespone->FirmwareVersion[MAX_STRING_LEN - 1] = '\0';
        }
        if ( g_onvifCameras.onvif_info[index].onvif_deviceinfo.HardwareId[0] )
        {
            strncpy(pGetDevinfoRespone->HardwareId,
                    g_onvifCameras.onvif_info[index].onvif_deviceinfo.HardwareId, MAX_STRING_LEN - 1);
            pGetDevinfoRespone->HardwareId[MAX_STRING_LEN - 1] = '\0';
        }
        if ( g_onvifCameras.onvif_info[index].onvif_deviceinfo.Manufacturer[0] )
        {
            strncpy(pGetDevinfoRespone->Manufacturer,
                    g_onvifCameras.onvif_info[index].onvif_deviceinfo.Manufacturer, MAX_STRING_LEN - 1);
            pGetDevinfoRespone->Manufacturer[MAX_STRING_LEN - 1] = '\0';
        }
        if ( g_onvifCameras.onvif_info[index].onvif_deviceinfo.Model[0] )
        {
            strncpy(pGetDevinfoRespone->Model,
                    g_onvifCameras.onvif_info[index].onvif_deviceinfo.Model, MAX_STRING_LEN - 1);
            pGetDevinfoRespone->Model[MAX_STRING_LEN - 1] = '\0';
        }
        if ( g_onvifCameras.onvif_info[index].onvif_deviceinfo.SerialNumber[0] )
        {
            strncpy(pGetDevinfoRespone->SerialNumber,
                    g_onvifCameras.onvif_info[index].onvif_deviceinfo.SerialNumber, MAX_STRING_LEN - 1);
            pGetDevinfoRespone->SerialNumber[MAX_STRING_LEN - 1] = '\0';
        }
        pGetDevinfoRespone->Status = g_onvifCameras.onvif_info[index].onvif_deviceinfo.Status;
    }

    return 0;

}


int ONVIF_GetProfilesFromCache( Remote_Device_Info *pDeviceInfo, Onvif_Profiles *pGetProfilesRespone )
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pGetProfilesRespone) {
        ONVIF_DBP("################%s\n",pDeviceInfo->ip);
        return -1;
    }
    int index = 0;
    int ret = 0;
    index = ONVIF_GetCameraIndex(pDeviceInfo);
    if ( index < 0 || index > MAX_CAMERA - 1)
    {
        ONVIF_DBP("################%s\n",pDeviceInfo->ip);
        return -1;
    }
    pGetProfilesRespone->sizeOfProfiles = g_onvifCameras.onvif_info[index].profileSize;

    int k = 0;
    for ( k = 0; k < pGetProfilesRespone->sizeOfProfiles; k++ ) {


        if ( g_onvifCameras.onvif_info[index].Profiles[k].profilename[0] ) {

            strncpy(pGetProfilesRespone->Onvif_Profiles[k].profieName,
                    g_onvifCameras.onvif_info[index].Profiles[k].profilename, MAX_LEN - 1);
            pGetProfilesRespone->Onvif_Profiles[k].profieName[MAX_LEN - 1] = '\0';
        }

        if ( g_onvifCameras.onvif_info[index].Profiles[k].profiletoken[0] ) {
            strncpy(pGetProfilesRespone->Onvif_Profiles[k].profileToken,
                    g_onvifCameras.onvif_info[index].Profiles[k].profiletoken, MAX_LEN - 1);
            pGetProfilesRespone->Onvif_Profiles[k].profileToken[MAX_LEN - 1] = '\0';
        }


        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoConfig.Name,
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEname, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.Name[MAX_LEN - 1] = '\0';

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEtoken, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.token[MAX_LEN - 1] = '\0';

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.videoEncoding
                = g_onvifCameras.onvif_info[index].Profiles[k].AESC.Vencoder;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.width =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rwidth;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.height =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.Rheight;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.Quality =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEquality;

        int encoding = g_onvifCameras.onvif_info[index].Profiles[k].AESC.Vencoder;

        if ( 2 == encoding ) { // h264
            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.h264.GovLength =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.H264govlength;

            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.h264.H264Profile =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.H264profile;

        }

        if ( 1 == encoding ) {

            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.mpeg4.GovLength =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.Mpeggovlength;

            pGetProfilesRespone->Onvif_Profiles[k].videoConfig.mpeg4.Mpeg4Profile =
                    g_onvifCameras.onvif_info[index].Profiles[k].AESC.Mpegprofile;
        }

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.FrameRateLimit =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.frameratelimit;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.BitrateLimit =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.bitratelimit;

        pGetProfilesRespone->Onvif_Profiles[k].videoConfig.EncodingInterval =
                g_onvifCameras.onvif_info[index].Profiles[k].AESC.encodinginterval;

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoSourceToken,
                g_onvifCameras.onvif_info[index].Profiles[k].AVSC.Vsourcetoken, MAX_LEN - 1);
        pGetProfilesRespone->Onvif_Profiles[k].videoSourceToken[MAX_LEN - 1] = '\0';


        // PTZ
        strncpy(pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].PTZtoken,
                sizeof(pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.token) - 1 );

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultPanTiltSpeedX =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZspeedx;

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultPanTiltSpeedY =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZspeedy;

        pGetProfilesRespone->Onvif_Profiles[k].ptzConfig.DefaultZoomSpeedX  =
                g_onvifCameras.onvif_info[index].Profiles[k].PTZzoom;

        strncpy(pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token,
                g_onvifCameras.onvif_info[index].Profiles[k].VAtoken,
                sizeof(pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token) - 1 );
        pGetProfilesRespone->Onvif_Profiles[k].videoAnalyticsConfig.token[MAX_LEN - 1] = '\0';


    }
    return 1;
}


int ONVIF_SetTimeOut(unsigned int ms)
{
    g_onvif_time_out = ms;
}


//获取音频编码配置参数
int GetAudioEncoderConfigurationOptions( Remote_Device_Info *pDeviceInfo, const char* ProfileToekn, const char* ConfigurationToken )
{
    if ( !pDeviceInfo ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&媒体服务地址为空 \n");
        return -1;
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif
    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _trt__GetAudioEncoderConfigurationOptions p_trt__GetAudioEncoderConfigurationOptions ;
    struct _trt__GetAudioEncoderConfigurationOptionsResponse p_trt__GetAudioEncoderConfigurationOptionsResponse = {0};

    soap_call___trt__GetAudioEncoderConfigurationOptions(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr, NULL,
                                                         &p_trt__GetAudioEncoderConfigurationOptions, &p_trt__GetAudioEncoderConfigurationOptionsResponse );
    if (!soap->error)
    {
        /********暂时不保存音频参数****************/
        ONVIF_DBP("获取音频参数成功\n");
    }
    if (soap->error) {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }

    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);

    return soap->error;

}


//获取视频配置信息（内部接口）
int GetVideoEncoderConfigurationOptions( Remote_Device_Info *pDeviceInfo, const char* ProfileToekn, const char* ConfigurationToken  )
{

    ONVIF_DBP(stderr,"################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );

    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr,255);
    }

    int sizeResolutionsAvailable = 0;
    int k = 0;

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;

#endif
    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    struct _trt__GetVideoEncoderConfigurationOptions *p_trt__GetVideoEncoderConfigurationOptions =
            (struct _trt__GetVideoEncoderConfigurationOptions *)soap_malloc(soap, sizeof( struct _trt__GetVideoEncoderConfigurationOptions ));

            p_trt__GetVideoEncoderConfigurationOptions->ProfileToken = (char*)soap_malloc(soap, 128);
            memset(p_trt__GetVideoEncoderConfigurationOptions->ProfileToken, 0, 128);
            p_trt__GetVideoEncoderConfigurationOptions->ConfigurationToken = (char*)soap_malloc(soap, 128);
            memset(p_trt__GetVideoEncoderConfigurationOptions->ConfigurationToken, 0, 128);

            if ( ProfileToekn ) {
                strncpy(p_trt__GetVideoEncoderConfigurationOptions->ProfileToken, ProfileToekn, MAX_PROF_TOKEN - 1);
            }

            if ( ConfigurationToken ) {
                strncpy(p_trt__GetVideoEncoderConfigurationOptions->ConfigurationToken, ConfigurationToken, MAX_PROF_TOKEN - 1 );
            }
            struct _trt__GetVideoEncoderConfigurationOptionsResponse *p_trt__GetVideoEncoderConfigurationOptionsResponse =
                    soap_malloc(soap, sizeof(struct _trt__GetVideoEncoderConfigurationOptionsResponse));
            memset(p_trt__GetVideoEncoderConfigurationOptionsResponse, 0, sizeof( struct _trt__GetVideoEncoderConfigurationOptionsResponse ));
            soap_call___trt__GetVideoEncoderConfigurationOptions(soap, ServerAddr, NULL,p_trt__GetVideoEncoderConfigurationOptions, p_trt__GetVideoEncoderConfigurationOptionsResponse );
            if (!soap->error) {

                if ( (-1 != index) && index < MAX_CAMERA )
                {

                    memset(&g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions, 0, sizeof(VideoEncoderConfigurationOptions));
                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.QualityRange.Min =
                            p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange->Min;

                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.QualityRange.Max =
                            p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->QualityRange->Max;

                    if ( p_trt__GetVideoEncoderConfigurationOptionsResponse->Options )
                    {

                        if ( p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264 )
                        {

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.EncodingIntervalRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.EncodingIntervalRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->EncodingIntervalRange->Min;

                            }
                            ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange )
                            {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.FrameRateRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange->Max;

                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.FrameRateRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->FrameRateRange->Min;
                            }



#if 0

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension)
                            {
                                //  fprintf(stderr,"##########got max%d\r\n",p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange->Max);
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.BitrateRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.BitrateRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->Extension->H264->BitrateRange->Min;
                            }
#endif
                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.GovLengthRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.GovLengthRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->GovLengthRange->Min;
                            }

                            sizeResolutionsAvailable = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->__sizeResolutionsAvailable;


                            if ( sizeResolutionsAvailable > 0 ) {

                                sizeResolutionsAvailable = sizeResolutionsAvailable > MAX_RESOLUTIONS_AVAILABLE - 1 ?
                                            MAX_RESOLUTIONS_AVAILABLE - 1 : sizeResolutionsAvailable;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.__sizeResolutionsAvailable
                                        = sizeResolutionsAvailable;

                                for (k = 0; k < sizeResolutionsAvailable; k++)
                                {
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Width
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[k].Width;
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Height
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->ResolutionsAvailable[k].Height;

                                }

                            }

                            int sizeH264Profiles = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->__sizeH264ProfilesSupported;

                            if ( sizeH264Profiles > 0  ) {
                                sizeH264Profiles = sizeH264Profiles > MAX_H264_PROFILES ? MAX_H264_PROFILES - 1 : sizeH264Profiles;
                                for ( k = 0; k < sizeH264Profiles; k++) {
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.H264ProfilesSupported[k] =
                                            p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->H264->H264ProfilesSupported[k];
                                }
                            }


                        }
#if 0

                        if ( p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG )
                        {

                            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasJPEG = 1;

                            sizeResolutionsAvailable = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->__sizeResolutionsAvailable;

                            if ( sizeResolutionsAvailable > 0) {

                                sizeResolutionsAvailable = sizeResolutionsAvailable > MAX_RESOLUTIONS_AVAILABLE - 1 ?
                                            MAX_RESOLUTIONS_AVAILABLE - 1 : sizeResolutionsAvailable;

                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.__sizeResolutionsAvailable =
                                        sizeResolutionsAvailable;

                                for (k = 0; k < sizeResolutionsAvailable; k++)
                                {
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.ResolutionsAvailable[k].Width
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->ResolutionsAvailable[k].Width;
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.ResolutionsAvailable[k].Height
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->ResolutionsAvailable[k].Height;
                                }

                            }

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->FrameRateRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.FrameRateRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->FrameRateRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.FrameRateRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->FrameRateRange->Min;

                            }

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->EncodingIntervalRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.EncodingIntervalRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->EncodingIntervalRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.JpegOptions.EncodingIntervalRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->JPEG->EncodingIntervalRange->Min;

                            }

                        }


                        if ( p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4 ) {
                            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasMpeg4 = 1;

                            sizeResolutionsAvailable = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->__sizeResolutionsAvailable;

                            if ( sizeResolutionsAvailable > 0  ) {

                                sizeResolutionsAvailable = sizeResolutionsAvailable > MAX_RESOLUTIONS_AVAILABLE - 1 ?
                                            MAX_RESOLUTIONS_AVAILABLE - 1 : sizeResolutionsAvailable;

                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.__sizeResolutionsAvailable =
                                        sizeResolutionsAvailable;

                                for (k = 0; k < sizeResolutionsAvailable; k++)
                                {
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.ResolutionsAvailable[k].Width
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->ResolutionsAvailable[k].Width;
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.ResolutionsAvailable[k].Height
                                            = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->ResolutionsAvailable[k].Height;
                                }
                            }


                            int sizeMp4Profiles = p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->__sizeMpeg4ProfilesSupported;

                            if ( sizeMp4Profiles > 0  ) {
                                sizeMp4Profiles = sizeMp4Profiles > MAX_H264_PROFILES ? MAX_H264_PROFILES - 1 : sizeMp4Profiles;
                                for ( k = 0; k < sizeMp4Profiles; k++) {
                                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.Mpeg4ProfilesSupported[k] =
                                            p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->Mpeg4ProfilesSupported[k];

                                }
                            }


                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->GovLengthRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.GovLengthRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->GovLengthRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.GovLengthRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->GovLengthRange->Min;
                            }

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->FrameRateRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.FrameRateRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->FrameRateRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.FrameRateRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->FrameRateRange->Min;
                            }

                            if (p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->EncodingIntervalRange ) {
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.EncodingIntervalRange.Max =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->EncodingIntervalRange->Max;
                                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.Mpeg4Options.EncodingIntervalRange.Min =
                                        p_trt__GetVideoEncoderConfigurationOptionsResponse->Options->MPEG4->EncodingIntervalRange->Min;
                            }
                        }

#endif

                    }

                }
            }


            if (soap->error) {
                fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
            }
            soap_destroy(soap);
            soap_end(soap);
            soap_done(soap);

            soap->error = (soap->error == 0 ? 1 : -1);
            ONVIF_DBP("################%s\n",pDeviceInfo->ip);
            return soap->error;

}




//获取视频配置信息（内部接口）
int GetVideoEncoderConfiguration( Remote_Device_Info *pDeviceInfo,  const char* ConfigurationToken )
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo )
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] )
    {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr,256);

    }
    int sizeResolutionsAvailable = 0;
    int k = 0;

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;

#endif
    soap_set_namespaces(soap, namespaces);
    soap_header(soap);


    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    struct _trt__GetVideoEncoderConfiguration *p_trt__GetVideoEncoderConfiguration = (struct _trt__GetVideoEncoderConfiguration *)soap_malloc(soap, sizeof( struct _trt__GetVideoEncoderConfiguration ));


    p_trt__GetVideoEncoderConfiguration->ConfigurationToken = (char*)soap_malloc(soap, 128);
    memset(p_trt__GetVideoEncoderConfiguration->ConfigurationToken, 0, 128);


    if ( ConfigurationToken ) {
        strncpy(p_trt__GetVideoEncoderConfiguration->ConfigurationToken, ConfigurationToken, MAX_PROF_TOKEN - 1 );
    }

    struct _trt__GetVideoEncoderConfigurationResponse *p_trt__GetVideoEncoderConfigurationResponse =
            soap_malloc(soap, sizeof(struct _trt__GetVideoEncoderConfigurationResponse));
    memset(p_trt__GetVideoEncoderConfigurationResponse, 0, sizeof( struct _trt__GetVideoEncoderConfigurationResponse ));

    soap_call___trt__GetVideoEncoderConfiguration(soap, ServerAddr, NULL,p_trt__GetVideoEncoderConfiguration, p_trt__GetVideoEncoderConfigurationResponse );
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if (!soap->error)
    {

        if ( (-1 != index) && index < MAX_CAMERA )
        {
            memset(&g_onvifCameras.onvif_info[index].videoEncoderConfiguration, 0, sizeof(VideoEncoderConfiguration));
            if ( p_trt__GetVideoEncoderConfigurationResponse->Configuration )
            {
                g_onvifCameras.onvif_info[index].videoEncoderConfiguration.Quality =
                        p_trt__GetVideoEncoderConfigurationResponse->Configuration->Quality;

                if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->EncodingInterval )
                {
                    g_onvifCameras.onvif_info[index].videoEncoderConfiguration.EncodingInterval=
                            p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->EncodingInterval;
                }
                ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->FrameRateLimit )
                {
                    g_onvifCameras.onvif_info[index].videoEncoderConfiguration.FrameRateLimit =
                            p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->FrameRateLimit;
                }

                if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->BitrateLimit )
                {
                    g_onvifCameras.onvif_info[index].videoEncoderConfiguration.BitrateLimit =
                            p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->BitrateLimit;

                }
                if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution )
                {

                    g_onvifCameras.onvif_info[index].videoEncoderConfiguration.VideoResolution.Width
                            = p_trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Width;
                    g_onvifCameras.onvif_info[index].videoEncoderConfiguration.VideoResolution.Height
                            = p_trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Height;

                }
            }

            if ( p_trt__GetVideoEncoderConfigurationResponse->Configuration )
            {

                if ( p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264)
                {
                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasH264 = 1;

                    if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264->GovLength )
                    {
                        g_onvifCameras.onvif_info[index].videoEncoderConfiguration.h264.GovLength=
                                p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264->GovLength;
                    }
                    if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264 )
                    {
                        g_onvifCameras.onvif_info[index].videoEncoderConfiguration.h264.H264Profile=
                                p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264->H264Profile;
                    }
                }

                if ( p_trt__GetVideoEncoderConfigurationResponse->Configuration )
                {
                    g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasMpeg4 = 1;

                    if (p_trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4 )
                    {
                        g_onvifCameras.onvif_info[index].videoEncoderConfiguration.mpeg4.GovLength=
                                p_trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4->GovLength;
                        g_onvifCameras.onvif_info[index].videoEncoderConfiguration.mpeg4.Mpeg4Profile=
                                p_trt__GetVideoEncoderConfigurationResponse->Configuration->MPEG4->Mpeg4Profile;
                    }

                }
            }
        }
    }

    if (soap->error)
    {
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);

    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}



//获取视频配置信息（对外接口）
int ONVIF_GetVideoEncoderConfiguration(Remote_Device_Info *pDeviceInfo,  const char* ProfileToekn,const char * videoEncodertoken,
                                       Onvif_Video_Config *pGetVideoEncoderConfigurationResponse,Onvif_VideoEncoderConfigurationOptions * pGetVideoEncoderConfigurationOptionsResponse)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !videoEncodertoken || !pGetVideoEncoderConfigurationResponse )
    {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = 0;
    int ret = 0;
    int sizeOfCodecProfile  = 0;
    int k= 0;
    int sizeOfResolution = 0;

    index = ONVIF_GetCameraIndex(pDeviceInfo);
    if ( index < 0 )
    {
        return -1;
    }

    int result =  GetVideoEncoderConfigurationOptions(pDeviceInfo,ProfileToekn,videoEncodertoken);
    if(1!=result)
    {
        return -2;
    }

    //可能导致崩溃
#if 0
    pGetVideoEncoderConfigurationOptionsResponse->H264Options.BitrateRange.Max =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.BitrateRange.Max;
    pGetVideoEncoderConfigurationOptionsResponse->H264Options.BitrateRange.Min =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.BitrateRange.Min;
#endif

    pGetVideoEncoderConfigurationOptionsResponse->H264Options.FrameRateRange.Max =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.FrameRateRange.Max;
    pGetVideoEncoderConfigurationOptionsResponse->H264Options.FrameRateRange.Min =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.FrameRateRange.Min;


    pGetVideoEncoderConfigurationOptionsResponse->H264Options.EncodingIntervalRange.Max =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.EncodingIntervalRange.Max;
    pGetVideoEncoderConfigurationOptionsResponse->H264Options.EncodingIntervalRange.Min =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.EncodingIntervalRange.Min;

    pGetVideoEncoderConfigurationOptionsResponse->H264Options.GovLengthRange.Max =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.GovLengthRange.Max;
    pGetVideoEncoderConfigurationOptionsResponse->H264Options.GovLengthRange.Min =
            g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.GovLengthRange.Min;

    sizeOfCodecProfile = g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.__sizeH264ProfilesSupported;
    sizeOfCodecProfile = sizeOfCodecProfile > MAX_H264_PROFILES -1 ? MAX_H264_PROFILES -1 : sizeOfCodecProfile;

    for ( k = 0; k < sizeOfCodecProfile; k++ )
    {
        pGetVideoEncoderConfigurationOptionsResponse->H264Options.H264ProfilesSupported[k] =
                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.H264ProfilesSupported[k];
    }

    sizeOfResolution = g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.__sizeResolutionsAvailable;
    sizeOfResolution = sizeOfResolution > MAX_RESOLUTIONS_ITEMS - 1 ? MAX_RESOLUTIONS_ITEMS - 1 : sizeOfResolution;

    pGetVideoEncoderConfigurationOptionsResponse->H264Options.__sizeResolutionsAvailable = sizeOfResolution;

    for ( k = 0; k < sizeOfResolution; k++ )
    {
        pGetVideoEncoderConfigurationOptionsResponse->H264Options.ResolutionsAvailable[k].Width =
                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Width;
        pGetVideoEncoderConfigurationOptionsResponse->H264Options.ResolutionsAvailable[k].Height =
                g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Height;

        ONVIF_DBP("*****************************%ld,%ld",g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Width,
                  g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.H264Options.ResolutionsAvailable[k].Height);

    }

    result = GetVideoEncoderConfiguration(pDeviceInfo, videoEncodertoken);
    if ( 1 != result )
    {
        return -3;
    }
    memset(pGetVideoEncoderConfigurationResponse, 0, sizeof(Onvif_Video_Config));

    pGetVideoEncoderConfigurationResponse->Quality =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.Quality;
    pGetVideoEncoderConfigurationResponse->FrameRateLimit =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.FrameRateLimit;
    pGetVideoEncoderConfigurationResponse->BitrateLimit =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.BitrateLimit;
    pGetVideoEncoderConfigurationResponse->EncodingInterval =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.EncodingInterval;
    pGetVideoEncoderConfigurationResponse->height =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.VideoResolution.Height;
    pGetVideoEncoderConfigurationResponse->width =
            g_onvifCameras.onvif_info[index].videoEncoderConfiguration.VideoResolution.Width;


    if (1 == g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasH264 )
    {
        pGetVideoEncoderConfigurationResponse->videoEncoding = 2;
        pGetVideoEncoderConfigurationResponse->h264.GovLength =
                g_onvifCameras.onvif_info[index].videoEncoderConfiguration.h264.GovLength;
        pGetVideoEncoderConfigurationResponse->h264.H264Profile =
                g_onvifCameras.onvif_info[index].videoEncoderConfiguration.h264.H264Profile;

    }
    if (1== g_onvifCameras.onvif_info[index].videoEncoderConfigurationOptions.hasMpeg4  )
    {
        pGetVideoEncoderConfigurationResponse->videoEncoding = 1;

        pGetVideoEncoderConfigurationResponse->mpeg4.GovLength =
                g_onvifCameras.onvif_info[index].videoEncoderConfiguration.mpeg4.GovLength;
        pGetVideoEncoderConfigurationResponse->mpeg4.Mpeg4Profile =
                g_onvifCameras.onvif_info[index].videoEncoderConfiguration.mpeg4.Mpeg4Profile;

    }
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return result;

}

int ONVIF_SetVideoEncoderConfiguration(Remote_Device_Info *pDeviceInfo, Onvif_Video_Config *pSetVideoEncoderConfig)
{

    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );


    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);


    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _trt__GetVideoEncoderConfiguration *p_trt__GetVideoEncoderConfiguration = (struct _trt__GetVideoEncoderConfiguration *)soap_malloc(soap, sizeof( struct _trt__GetVideoEncoderConfiguration ));

    p_trt__GetVideoEncoderConfiguration->ConfigurationToken = (char*)soap_malloc(soap, 128);
    memset(p_trt__GetVideoEncoderConfiguration->ConfigurationToken, 0, 128);


    if ( pSetVideoEncoderConfig->token )
    {
        strncpy(p_trt__GetVideoEncoderConfiguration->ConfigurationToken, pSetVideoEncoderConfig->token, MAX_PROF_TOKEN - 1 );
    }

    struct _trt__GetVideoEncoderConfigurationResponse *p_trt__GetVideoEncoderConfigurationResponse =
            soap_malloc(soap, sizeof(struct _trt__GetVideoEncoderConfigurationResponse));
    memset(p_trt__GetVideoEncoderConfigurationResponse, 0, sizeof( struct _trt__GetVideoEncoderConfigurationResponse ));

    soap_call___trt__GetVideoEncoderConfiguration(soap, ServerAddr, NULL,p_trt__GetVideoEncoderConfiguration, p_trt__GetVideoEncoderConfigurationResponse );

    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    struct _trt__SetVideoEncoderConfiguration *p_trt__SetVideoEncoderConfiguration =(struct _trt__SetVideoEncoderConfiguration *)soap_malloc(soap, sizeof(struct _trt__SetVideoEncoderConfiguration));
    //memset(p_trt__SetVideoEncoderConfiguration, 0, sizeof(struct _trt__SetVideoEncoderConfiguration));
    struct _trt__SetVideoEncoderConfigurationResponse * p_trt__SetVideoEncoderConfigurationResponse =(struct _trt__SetVideoEncoderConfigurationResponse *)soap_malloc(soap, sizeof(struct _trt__SetVideoEncoderConfigurationResponse));
    //memset(p_trt__SetVideoEncoderConfigurationResponse, 0, sizeof(struct _trt__SetVideoEncoderConfigurationResponse));


    p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->BitrateLimit =  pSetVideoEncoderConfig->BitrateLimit;
    p_trt__GetVideoEncoderConfigurationResponse->Configuration->RateControl->FrameRateLimit = pSetVideoEncoderConfig->FrameRateLimit;

    p_trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Width = pSetVideoEncoderConfig->width;
    p_trt__GetVideoEncoderConfigurationResponse->Configuration->Resolution->Height = pSetVideoEncoderConfig->height;
#if 0
    p_trt__GetVideoEncoderConfigurationResponse->Configuration->Quality = pSetVideoEncoderConfig->Quality;

    if ( tt__VideoEncoding__H264 == pSetVideoEncoderConfig->videoEncoding )
    {
        p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264->GovLength = pSetVideoEncoderConfig->h264.GovLength;
        p_trt__GetVideoEncoderConfigurationResponse->Configuration->H264->H264Profile = pSetVideoEncoderConfig->h264.H264Profile;
    }

#endif
    p_trt__SetVideoEncoderConfiguration->ForcePersistence = xsd__boolean__true_;
    p_trt__SetVideoEncoderConfiguration->Configuration = p_trt__GetVideoEncoderConfigurationResponse->Configuration;

    soap_call___trt__SetVideoEncoderConfiguration(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr, NULL, p_trt__SetVideoEncoderConfiguration, p_trt__SetVideoEncoderConfigurationResponse);
    if (soap->error)
    {
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;


}



//***************************PTZ模块***********************


#define YEAR_IN_SEC (31556926)
#define MONTH_IN_SEC (2629744)
#define DAY_IN_SEC (86400)
#define HOURS_IN_SEC (3600)
#define MIN_IN_SEC (60)

long epoch_convert_switch(int value, char convert, int time)
{
    long seconds = 0;
    switch(convert)
    {
    case 'Y': seconds = value * YEAR_IN_SEC ;
        break;
    case 'M':
        if(time == 1)
        {
            seconds = value * MIN_IN_SEC;
        }
        else
        {
            seconds = value * MONTH_IN_SEC;
        }
        break;
    case 'D': seconds = value * DAY_IN_SEC;
        break;
    case 'H': seconds = value * HOURS_IN_SEC;
        break;
    case 'S': seconds = value;
        break;
    }
    return seconds;
}



long periodtol(char *ptr)
{
    char buff[10] = "0";
    char convert;
    int i = 0;
    int value = 0;
    int time = 0;
    int minus = 0;
    long cumulative = 0;
    if(*ptr == '-')
    {
        ptr++;
        minus = 1;
    }
    while(*ptr != '\0')
    {

        if(*ptr == 'P' || *ptr == 'T')
        {
            ptr++;
            if(*ptr == 'T')
            {
                time = 1;
                ptr++;
            }
        }
        else
        {
            if(*ptr >= '0' && *ptr <= '9')
            {
                buff[i] = *ptr;
                i++;
                ptr++;
            }
            else
            {
                buff[i] = 0;
                value = atoi(buff);
                memset(buff, 0, sizeof(buff));
                i = 0;
                convert = *ptr;
                ptr++;
                cumulative = cumulative + epoch_convert_switch(value, convert, time);
            }
        }
    }
    if(minus == 1)
    {
        return -cumulative;
    }
    else
    {
        return cumulative;
    }
}



int ONVIF_PtzGetConfigurationOptions(Remote_Device_Info *pDeviceInfo, const char *ConfigurationToken, Onvif_Ptz_ConfigOtions *pGetPtzConfigOtionsReponse)
{

    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pGetPtzConfigOtionsReponse ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {

        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }


    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;

#endif
    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    struct _tptz__GetConfigurationOptions *p_tptz__GetConfigurationOptions =
            (struct _tptz__GetConfigurationOptions *)soap_malloc(soap, sizeof(struct _tptz__GetConfigurationOptions ));
            p_tptz__GetConfigurationOptions->ConfigurationToken = (char*)soap_malloc(soap, 64);
            memset(p_tptz__GetConfigurationOptions->ConfigurationToken, 0, 64);

            if ( ConfigurationToken ) {
                strncpy(p_tptz__GetConfigurationOptions->ConfigurationToken, ConfigurationToken, 63);
                p_tptz__GetConfigurationOptions->ConfigurationToken[63] = '\0';
            }

            struct _tptz__GetConfigurationOptionsResponse *p_tptz__GetConfigurationOptionsResponse =
                    (struct _tptz__GetConfigurationOptionsResponse *)soap_malloc(soap, sizeof(struct _tptz__GetConfigurationOptionsResponse));
                    memset(p_tptz__GetConfigurationOptionsResponse, 0, sizeof( p_tptz__GetConfigurationOptionsResponse ));

                    soap_call___tptz__GetConfigurationOptions(soap, ServerAddr, NULL, p_tptz__GetConfigurationOptions, p_tptz__GetConfigurationOptionsResponse);

                    if ( !soap->error) {

                        if ( p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions) {

                            if ( p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->PTZTimeout ) {
                                pGetPtzConfigOtionsReponse->TimeOutRange.Max = p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->PTZTimeout->Max;
                                pGetPtzConfigOtionsReponse->TimeOutRange.Min = p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->PTZTimeout->Min;

                            }
                            if ( p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces) {

                                if ( p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->__sizeContinuousPanTiltVelocitySpace ) {
                                    pGetPtzConfigOtionsReponse->PanTiltVelocityRangeX.Min =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace[0].XRange->Min;
                                    pGetPtzConfigOtionsReponse->PanTiltVelocityRangeX.Max =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace[0].XRange->Max;

                                    pGetPtzConfigOtionsReponse->PanTiltVelocityRangeY.Min =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace[0].YRange->Min;
                                    pGetPtzConfigOtionsReponse->PanTiltVelocityRangeY.Max =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousPanTiltVelocitySpace[0].YRange->Max;


                                    pGetPtzConfigOtionsReponse->ZoomVelocityRangeX.Min =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousZoomVelocitySpace[0].XRange->Min;
                                    pGetPtzConfigOtionsReponse->ZoomVelocityRangeX.Max =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->ContinuousZoomVelocitySpace[0].XRange->Max;
                                }
                                if ( p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->__sizePanTiltSpeedSpace ) {
                                    pGetPtzConfigOtionsReponse->PanTiltSpeedRangeX.Min =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->PanTiltSpeedSpace[0].XRange->Min;
                                    pGetPtzConfigOtionsReponse->PanTiltSpeedRangeX.Max =
                                            p_tptz__GetConfigurationOptionsResponse->PTZConfigurationOptions->Spaces->PanTiltSpeedSpace[0].XRange->Max;
                                }
                            }
                        }
                    }

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                    return soap->error;
}


int ONVIF_PtzGetConfiguration(Remote_Device_Info *pDeviceInfo, Onvif_Ptz_Config *pGetPtzConfig)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pGetPtzConfig ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif
    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _tptz__GetConfiguration *p_tptz__GetConfiguration =
            (struct _tptz__GetConfiguration *)soap_malloc(soap, sizeof( struct _tptz__GetConfiguration ));
            memset(p_tptz__GetConfiguration, 0, sizeof(struct _tptz__GetConfiguration ));
            p_tptz__GetConfiguration->PTZConfigurationToken =
                    (struct tt__ReferenceToken *)soap_malloc(soap, sizeof(tt__ReferenceToken )*64);


            strncpy(p_tptz__GetConfiguration->PTZConfigurationToken,pGetPtzConfig->token,MAX_LEN -1);
            struct _tptz__GetConfigurationResponse *p_tptz__GetConfigurationResponse =
                    (struct _tptz__GetConfigurationResponse *)soap_malloc(soap, sizeof(struct _tptz__GetConfigurationResponse));
                    soap_call___tptz__GetConfiguration(soap, ServerAddr, NULL, p_tptz__GetConfiguration, p_tptz__GetConfigurationResponse);
                    if ( !soap->error ) {
                        if ( p_tptz__GetConfigurationResponse->PTZConfiguration->token ) {
                            strncpy(pGetPtzConfig->token,
                                    p_tptz__GetConfigurationResponse->PTZConfiguration->token, MAX_LEN -1 );
                        }

                        if ( p_tptz__GetConfigurationResponse->PTZConfiguration->Name) {
                            strncpy(pGetPtzConfig->name,
                                    p_tptz__GetConfigurationResponse->PTZConfiguration->Name, MAX_LEN -1 );
                        }

                        if ( p_tptz__GetConfigurationResponse->PTZConfiguration->NodeToken) {
                            strncpy(pGetPtzConfig->NodeToken,
                                    p_tptz__GetConfigurationResponse->PTZConfiguration->NodeToken, MAX_LEN -1 );
                        }

                        if ( p_tptz__GetConfigurationResponse->PTZConfiguration->DefaultPTZSpeed ) {
                            if ( p_tptz__GetConfigurationResponse->PTZConfiguration->DefaultPTZSpeed->PanTilt ) {
                                pGetPtzConfig->DefaultPanTiltSpeedX =
                                        p_tptz__GetConfigurationResponse->PTZConfiguration->DefaultPTZSpeed->PanTilt->x;
                                pGetPtzConfig->DefaultPanTiltSpeedY =
                                        p_tptz__GetConfigurationResponse->PTZConfiguration->DefaultPTZSpeed->PanTilt->y;
                            }
                        }

                        ONVIF_DBP(" &&&&&&&&&&&&&timeout=%ld \n",*p_tptz__GetConfigurationResponse->PTZConfiguration->DefaultPTZTimeout);
                    }

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                    return soap->error;

}

#define  MIN_SPEED  0.125

int ONVIF_PtzGotoPreset(Remote_Device_Info *pDeviceInfo, const char *profileToken, const char *presetToken,unsigned int speed)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !presetToken) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;
#endif
    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _tptz__GotoPreset * p_tptz__GotoPreset = ( struct _tptz__GotoPreset *)soap_malloc(soap, sizeof(struct _tptz__GotoPreset));
    memset(p_tptz__GotoPreset, 0, sizeof(struct _tptz__GotoPreset));

    p_tptz__GotoPreset->ProfileToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__GotoPreset->ProfileToken, 0, 64);
    p_tptz__GotoPreset->PresetToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__GotoPreset->PresetToken, 0, 64);
    if(0!= speed)
    {
        p_tptz__GotoPreset->Speed = (struct tt__PTZSpeed *)soap_malloc(soap,sizeof(struct tt__PTZSpeed));
        p_tptz__GotoPreset->Speed->PanTilt=(struct tt__Vector2D *)soap_malloc(soap,sizeof(struct tt__Vector2D ));
        p_tptz__GotoPreset->Speed->PanTilt->space = (char*)soap_malloc(soap, 128);
        memset(p_tptz__GotoPreset->Speed->PanTilt->space, 0, 128);
        p_tptz__GotoPreset->Speed->Zoom = (struct tt__Vector1D *)soap_malloc(soap,sizeof(struct tt__Vector1D ));
        p_tptz__GotoPreset->Speed->Zoom->space =  (char*)soap_malloc(soap, 128);
        memset(p_tptz__GotoPreset->Speed->Zoom->space, 0, 128);
        p_tptz__GotoPreset->Speed->Zoom->space =  "http://www.onvif.org/ver10/tptz/PanTiltSpaces/ZoomGenericSpeedSpace";
        //ptz speed space
        p_tptz__GotoPreset->Speed->PanTilt->space = "http://www.onvif.org/ver10/tptz/PanTiltSpaces/GenericSpeedSpace";
        //  0-8
        p_tptz__GotoPreset->Speed->PanTilt->y = ((float)(speed * MIN_SPEED));
        p_tptz__GotoPreset->Speed->PanTilt->x = ((float)(speed * MIN_SPEED));

    }
    if ( profileToken[0])
    {
        strncpy(p_tptz__GotoPreset->ProfileToken, profileToken, 63);
    }
    strncpy(p_tptz__GotoPreset->PresetToken, presetToken, 63);
    soap_call___tptz__GotoPreset(soap, ServerAddr, NULL, p_tptz__GotoPreset, NULL);
    if (soap->error)
    {
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}

int ONVIF_PtzSetPreset(Remote_Device_Info *pDeviceInfo, const char * profileToken ,const char *presetName, const char *presetToken )
{

    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !presetToken) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {

        return -1;
    }

    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;
#endif
    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tptz__SetPreset p_tptz__SetPreset ;
    struct _tptz__SetPresetResponse p_tptz__SetPresetResponse;
    p_tptz__SetPreset.ProfileToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__SetPreset.ProfileToken, 0,  64 );

    p_tptz__SetPreset.PresetName = (char*)soap_malloc(soap, 64);
    memset(p_tptz__SetPreset.PresetName, 0,  64 );

    p_tptz__SetPreset.PresetToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__SetPreset.PresetToken, 0,  64 );

    if (presetName[0])
    {
        strncpy(p_tptz__SetPreset.PresetName, presetName, 63);
    }

    strncpy(p_tptz__SetPreset.ProfileToken, profileToken, 63);
    strncpy(p_tptz__SetPreset.PresetToken, presetToken, 63);


    soap_call___tptz__SetPreset(soap, ServerAddr, NULL,&p_tptz__SetPreset, &p_tptz__SetPresetResponse);
    if ( p_tptz__SetPresetResponse.PresetToken )
    {
        fprintf(stderr,"respose presetToken %s \n", p_tptz__SetPresetResponse.PresetToken);
    }
    if (soap->error) {
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}


int ONVIF_PtzGetPresets(Remote_Device_Info *pDeviceInfo, const char *profileToken, Onvif_Ptz_Presets *pGetPtzPresetsResponse )
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pGetPtzPresetsResponse || !profileToken ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0 || index > MAX_CAMERA - 1 )
    {
        return -1;
    }

    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _tptz__GetPresets *p__tptz__GetPresets =
            (struct _tptz__GetPresets *)soap_malloc(soap, sizeof(struct _tptz__GetPresets));

            memset(p__tptz__GetPresets, 0, sizeof(struct _tptz__GetPresets));

            p__tptz__GetPresets->ProfileToken = (char*)soap_malloc(soap, 128);
            memset(p__tptz__GetPresets->ProfileToken, 0, 128);

            if ( profileToken[0] ) {
                strncpy(p__tptz__GetPresets->ProfileToken, profileToken, 127);
            }

            struct _tptz__GetPresetsResponse *p_tptz__GetPresetsResponse =
                    (struct _tptz__GetPresetsResponse *)soap_malloc(soap, sizeof(struct _tptz__GetPresetsResponse ));

                    soap_call___tptz__GetPresets(soap, ServerAddr, NULL, p__tptz__GetPresets, p_tptz__GetPresetsResponse);

                    if (!soap->error )
                    {

                        int k = 0;
                        int sizeOfPresets = p_tptz__GetPresetsResponse->__sizePreset;
                        sizeOfPresets = sizeOfPresets > MAX_PTZ_PRESETS - 1 ? MAX_PTZ_PRESETS - 1 : sizeOfPresets;

                        memset(pGetPtzPresetsResponse, 0, sizeof( Onvif_Ptz_Presets ) );
                        pGetPtzPresetsResponse->sizeOfPresets = sizeOfPresets;

                        for ( k = 0; k < sizeOfPresets; k++ ) {

                            if(p_tptz__GetPresetsResponse->Preset[k].Name)
                            {
                                strncpy(pGetPtzPresetsResponse->presets[k].presetName, p_tptz__GetPresetsResponse->Preset[k].Name, MAX_LEN - 1);
                            }
                            if(p_tptz__GetPresetsResponse->Preset[k].token)
                            {
                                strncpy(pGetPtzPresetsResponse->presets[k].presetToken, p_tptz__GetPresetsResponse->Preset[k].token, MAX_LEN - 1);
                            }
#if 0
                            if(p_tptz__GetPresetsResponse->Preset[k].PTZPosition)
                            {
                                fprintf(stderr,"x =%lf,y =%lf ,z=%lf \n",p_tptz__GetPresetsResponse->Preset[k].PTZPosition->PanTilt->x,p_tptz__GetPresetsResponse->Preset[k].PTZPosition->PanTilt->y,p_tptz__GetPresetsResponse->Preset[k].PTZPosition->Zoom->x);
                                pGetPtzPresetsResponse->presets[k].PanTilt_x = p_tptz__GetPresetsResponse->Preset[k].PTZPosition->PanTilt->x;
                                pGetPtzPresetsResponse->presets[k].PanTilt_y = p_tptz__GetPresetsResponse->Preset[k].PTZPosition->PanTilt->y;
                                pGetPtzPresetsResponse->presets[k].zoom = p_tptz__GetPresetsResponse->Preset[k].PTZPosition->Zoom->x;
                            }
#endif

                        }
                    }

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                    return soap->error;

}

int ONVIF_PtzRemovePreset( Remote_Device_Info *pDeviceInfo, const char *profileToken, const char *presetToken )
{
    ONVIF_DBP("=============%s: %s: %d  \n",pDeviceInfo->ip ,__func__,__LINE__ );
    if ( !pDeviceInfo || !presetToken) {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;

#endif
    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tptz__RemovePreset p_tptz__RemovePreset ;
    struct _tptz__RemovePresetResponse  p_tptz__RemovePresetResponse ;

    p_tptz__RemovePreset.ProfileToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__RemovePreset.ProfileToken, 0, 64);
    p_tptz__RemovePreset.PresetToken = (char*)soap_malloc(soap, 64);
    memset(p_tptz__RemovePreset.PresetToken, 0, 64);

    if ( profileToken ) {
        strncpy(p_tptz__RemovePreset.ProfileToken, profileToken, 63);
    }

    strncpy(p_tptz__RemovePreset.PresetToken, presetToken, 63);
    soap_call___tptz__RemovePreset(soap, ServerAddr, NULL, &p_tptz__RemovePreset, &p_tptz__RemovePresetResponse);

    if (soap->error)
    {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("=============%s: %s: %d  \n",pDeviceInfo->ip ,__func__,__LINE__ );
    return soap->error;

}



//#define  MIN_SPEED  0.05
int ONVIF_PtzContinueMove(Remote_Device_Info *pDeviceInfo, ONVIF_PTZ__ContinuousMove *pPtzContinuousMove )
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    struct timeval v1;
    gettimeofday(&v1, NULL);
    //fprintf(stderr, "\n##############################Enter ONVIF_PtzContinueMove time %d", v1.tv_sec * 1000 + v1.tv_usec / 1000);
    if ( !pDeviceInfo || !pPtzContinuousMove ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] || !pPtzContinuousMove->ProfileToken[0] ) {
        return -1;
    }
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr,255);
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tptz__ContinuousMove *p_tptz__ContinuousMove =
            (struct _tptz__ContinuousMove *)soap_malloc(soap, sizeof( struct _tptz__ContinuousMove ) );
            memset(p_tptz__ContinuousMove, 0, sizeof( struct _tptz__ContinuousMove ));
            struct _tptz__ContinuousMoveResponse *p_tptz__ContinuousMoveResponse =
                    (struct _tptz__ContinuousMoveResponse *)soap_malloc(soap, sizeof( struct _tptz__ContinuousMoveResponse) );
                    memset(p_tptz__ContinuousMoveResponse, 0, sizeof( struct _tptz__ContinuousMoveResponse ) );

                    p_tptz__ContinuousMove->ProfileToken= pPtzContinuousMove->ProfileToken;

                    //p_tptz__ContinuousMove->Timeout =   ( xsd__duration *)soap_malloc(soap, sizeof( xsd__duration) );
                    // *p_tptz__ContinuousMove->Timeout =pPtzContinuousMove->Timeout;
                    // fprintf(stderr,"%ld\n",*p_tptz__ContinuousMove->Timeout);

                    p_tptz__ContinuousMove->Velocity =
                            (struct tt__PTZSpeed *)soap_malloc(soap, sizeof( struct tt__PTZSpeed ) );
                    memset(p_tptz__ContinuousMove->Velocity, 0, sizeof( struct tt__PTZSpeed ) );

                    // speed 0 - 7
                    int speed = pPtzContinuousMove->Speed % 8;

                    if ( pPtzContinuousMove->isPan )
                    {
                        p_tptz__ContinuousMove->Velocity->PanTilt =
                                (struct tt__Vector2D *)soap_malloc(soap, sizeof( struct tt__Vector2D ) );
                        memset(p_tptz__ContinuousMove->Velocity->PanTilt, 0, sizeof( struct tt__Vector2D ));


                        p_tptz__ContinuousMove->Velocity->PanTilt->space= "http://www.onvif.org/ver10/tptz/PanTiltSpaces/VelocityGenericSpace";


                        if ( PTZ_UP == pPtzContinuousMove->direction )
                        {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = 0.0;
                        }

                        if ( PTZ_DOWN == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = - fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = 0.0;
                        }

                        if ( PTZ_LEFT == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = 0.0;
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = - fabs((float)(speed * MIN_SPEED));
                        }
                        if ( PTZ_RIGHT == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = 0.0;
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = fabs((float)(speed * MIN_SPEED));
                        }

                        if ( PTZ_LEFT_UP == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = - fabs((float)(speed * MIN_SPEED));

                        }

                        if ( PTZ_LEFT_DOWN == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y = - fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x = - fabs((float)(speed * MIN_SPEED));
                        }

                        if ( PTZ_RIGHT_UP == pPtzContinuousMove->direction ) {
                            p_tptz__ContinuousMove->Velocity->PanTilt->y =  fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x =  fabs((float)(speed * MIN_SPEED));
                        }

                        if ( PTZ_RIGHT_DOWN == pPtzContinuousMove->direction ) {

                            p_tptz__ContinuousMove->Velocity->PanTilt->y =  - fabs((float)(speed * MIN_SPEED));
                            p_tptz__ContinuousMove->Velocity->PanTilt->x =  fabs((float)(speed * MIN_SPEED));
                            //  ONVIF_DBP("%lf,%lf",p_tptz__ContinuousMove->Velocity->PanTilt->x,p_tptz__ContinuousMove->Velocity->PanTilt->y);
                        }
                    }

                    if ( pPtzContinuousMove->isZoom )
                    {
                        p_tptz__ContinuousMove->Velocity->Zoom =
                                (struct tt__Vector1D *)soap_malloc(soap, sizeof(struct tt__Vector1D));
                        memset(p_tptz__ContinuousMove->Velocity->Zoom, 0, sizeof(struct tt__Vector1D));
                        p_tptz__ContinuousMove->Velocity->Zoom->space = (char*)soap_malloc(soap, 128);
                        p_tptz__ContinuousMove->Velocity->Zoom->space = "http://www.onvif.org/ver10/tptz/ZoomSpaces/VelocityGenericSpace";
                        p_tptz__ContinuousMove->Velocity->Zoom->x = (float)(speed * 0.125);
                    }

                    soap_call___tptz__ContinuousMove(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr, NULL, p_tptz__ContinuousMove, p_tptz__ContinuousMoveResponse);
                    if (soap->error)
                    {
                        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                    return soap->error;
}



int ONVIF_PtzGetNodes(Remote_Device_Info *pDeviceInfo, ONVIF_GetNodesResponse *pGetNodesResponse)
{
    ONVIF_DBP("come here \n");
    if ( !pDeviceInfo || !pGetNodesResponse ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&PTZ服务地址为空 \n");
        return -1;
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tptz__GetNodes *p_tptz__GetNodes  =
            (struct _tptz__GetNodes *)soap_malloc(soap, sizeof(struct _tptz__GetNodes));
            memset(p_tptz__GetNodes, 0, sizeof(struct _tptz__GetNodes));

            struct _tptz__GetNodesResponse *p_tptz__GetNodesResponse =
                    (struct _tptz__GetNodesResponse *)soap_malloc(soap, sizeof(struct _tptz__GetNodesResponse ));

                    soap_call___tptz__GetNodes(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr, NULL, p_tptz__GetNodes, p_tptz__GetNodesResponse);

                    if ( !soap->error )
                    {
                        int k = 0;
                        int sizeOfNodes = p_tptz__GetNodesResponse->__sizePTZNode;

                        sizeOfNodes = sizeOfNodes > MAX_PTZ_PRESETS - 1 ? MAX_PTZ_PRESETS - 1 : sizeOfNodes;
                        ONVIF_DBP(" &&&&&&&&&&&&&sizeOfNodes ==%d \n",sizeOfNodes);
                        memset(pGetNodesResponse, 0, sizeof( ONVIF_GetNodesResponse ) );
                        pGetNodesResponse->__sizePTZNode = sizeOfNodes;

                        for ( k = 0; k < sizeOfNodes; k++ )
                        {

                            strncpy(pGetNodesResponse->PTZNode[k].PTZNodetoken,p_tptz__GetNodesResponse->PTZNode[k].token,63);
                            ONVIF_DBP(" &&&&&&&&&&&&&PTZNodetoken ==%s \n",pGetNodesResponse->PTZNode[k].PTZNodetoken);
                            if(p_tptz__GetNodesResponse->PTZNode[k].Name)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].PTZNodeName,p_tptz__GetNodesResponse->PTZNode[k].Name,63);
                            }

                            //1
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeAbsolutePanTiltPositionSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeAbsolutePanTiltPositionSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->URI,MAX_URL_LEN);
                                ONVIF_DBP(" &&&&&&&&&&&&&AbsolutePanTiltPositionSpace ==%s \n",pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.URI);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->XRange->Min;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->YRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsolutePanTiltPositionSpace->YRange->Min;
                            ONVIF_DBP("xmax= %d ,%d, ymax =%d,%d \n",pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Max,
                                      pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.XRange.Min,
                                      pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Max,
                                      pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsolutePanTiltPositionSpace.YRange.Min);
                            //2
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeAbsoluteZoomPositionSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeAbsoluteZoomPositionSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsoluteZoomPositionSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsoluteZoomPositionSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsoluteZoomPositionSpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Max =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsoluteZoomPositionSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsoluteZoomPositionSpace.XRange.Min =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsoluteZoomPositionSpace->XRange->Min;


                            //pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.AbsoluteZoomPositionSpace.YRange = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->AbsoluteZoomPositionSpace.YRange;

                            //3
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeRelativePanTiltTranslationSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeRelativePanTiltTranslationSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativePanTiltTranslationSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativePanTiltTranslationSpace.XRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->XRange->Min;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->YRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativePanTiltTranslationSpace.YRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativePanTiltTranslationSpace->YRange->Min;
                            //4
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeRelativeZoomTranslationSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeRelativeZoomTranslationSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativeZoomTranslationSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativeZoomTranslationSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativeZoomTranslationSpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Max =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativeZoomTranslationSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativeZoomTranslationSpace.XRange.Min =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativeZoomTranslationSpace->XRange->Min;
                            //pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.RelativeZoomTranslationSpace.YRange = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->RelativeZoomTranslationSpace.YRange;

                            //5
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeContinuousPanTiltVelocitySpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeContinuousPanTiltVelocitySpace;
                            // ONVIF_DBP(" &&&&&&&&&&&&&PTZNodetoken ==%d \n",p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeContinuousPanTiltVelocitySpace);
                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->URI,MAX_URL_LEN);
                                //  ONVIF_DBP(" &&&&&&&&&&&&&PTZNodeturi ==%s \n",p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->URI);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.XRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->XRange->Min;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Max = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->YRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousPanTiltVelocitySpace.YRange.Min = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousPanTiltVelocitySpace->YRange->Min;
                            //6

                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeContinuousZoomVelocitySpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeContinuousZoomVelocitySpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousZoomVelocitySpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousZoomVelocitySpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousZoomVelocitySpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Max =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousZoomVelocitySpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousZoomVelocitySpace.XRange.Min =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousZoomVelocitySpace->XRange->Min;
                            //pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ContinuousZoomVelocitySpace.YRange = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ContinuousZoomVelocitySpace.YRange;

                            //7
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizePanTiltSpeedSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizePanTiltSpeedSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->PanTiltSpeedSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.PanTiltSpeedSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->PanTiltSpeedSpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Max =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->PanTiltSpeedSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Min =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->PanTiltSpeedSpace->XRange->Min;
                            ONVIF_DBP("xmax= %d ,min= %d \n",pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Max,
                                      pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.PanTiltSpeedSpace.XRange.Min);


                            //8
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.__sizeZoomSpeedSpace =
                                    p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->__sizeZoomSpeedSpace;

                            if(p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ZoomSpeedSpace->URI)
                            {
                                strncpy(pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ZoomSpeedSpace.URI, p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ZoomSpeedSpace->URI,MAX_URL_LEN);
                            }
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ZoomSpeedSpace.XRange.Max =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ZoomSpeedSpace->XRange->Max;
                            pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ZoomSpeedSpace.XRange.Min =p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ZoomSpeedSpace->XRange->Min;
                            //pGetNodesResponse->PTZNode[k].SupportedPTZSpaces.ZoomSpeedSpace.YRange = p_tptz__GetNodesResponse->PTZNode[k].SupportedPTZSpaces->ZoomSpeedSpace.YRange;

                            pGetNodesResponse->PTZNode[k].MaximumNumberOfPresets = p_tptz__GetNodesResponse->PTZNode[k].MaximumNumberOfPresets;
                            pGetNodesResponse->PTZNode[k].HomeSupported = p_tptz__GetNodesResponse->PTZNode[k].HomeSupported;
                            pGetNodesResponse->PTZNode[k].__sizeAuxiliaryCommands =p_tptz__GetNodesResponse->PTZNode[k].__sizeAuxiliaryCommands;
                        }
                    }

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("come here \n");
                    return soap->error;

}




int ONVIF_PtzStop(Remote_Device_Info *pDeviceInfo, const char *profileToken )
{

#if 1
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !profileToken ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&PTZ服务地址为空 \n");
        return -1;
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    bool pantilt = true;
    bool zoom = false ;
    struct _tptz__Stop *p_tptz__Stop =
            (struct _tptz__Stop *)soap_malloc(soap, sizeof(struct _tptz__Stop));
            memset(p_tptz__Stop, 0, sizeof( struct _tptz__Stop));

            p_tptz__Stop->PanTilt = (enum xsd__boolean * )soap_malloc(soap ,sizeof (enum xsd__boolean));
            p_tptz__Stop->Zoom = (enum xsd__boolean * )soap_malloc(soap ,sizeof (enum xsd__boolean));
            *p_tptz__Stop->PanTilt = pantilt ;
            *p_tptz__Stop->Zoom  = zoom;

            p_tptz__Stop->ProfileToken = (char*)soap_malloc(soap, 64);
            memset(p_tptz__Stop->ProfileToken, 0, 63);
            strncpy(p_tptz__Stop->ProfileToken, profileToken, 63);

            struct _tptz__StopResponse *p_tptz__StopResponse =
                    (struct _tptz__StopResponse *)soap_malloc(soap, sizeof(struct _tptz__StopResponse));

                    soap_call___tptz__Stop(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.pTZCapabilities.XAddr, NULL, p_tptz__Stop, p_tptz__StopResponse);

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }

                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
                    return soap->error;

#endif
}





/***********************************************/

/****************IMAGE模块********************/

/***********************************************/



int ONVIF_GetImageOptions(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Options *pGetImgOptionsResponse)
{

    if ( !pDeviceInfo || !pGetImgOptionsResponse || !videoSourceToken ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.mediaCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&IMage服务地址为空 \n");
        return -1;
    }
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);

    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _timg__GetOptions p_timg__GetOptions ;
    p_timg__GetOptions.VideoSourceToken = (char*)soap_malloc(soap, 64);
    strncpy(p_timg__GetOptions.VideoSourceToken, videoSourceToken,63);
    struct _timg__GetOptionsResponse p_timg__GetOptionsResponse =  {0};

    soap_call___timg__GetOptions(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr, NULL, &p_timg__GetOptions, &p_timg__GetOptionsResponse);

    if ( !soap->error )
    {
        if ( p_timg__GetOptionsResponse.ImagingOptions )
        {

            if ( p_timg__GetOptionsResponse.ImagingOptions->Brightness )
            {
                pGetImgOptionsResponse->brightness.Max = p_timg__GetOptionsResponse.ImagingOptions->Brightness->Max;
                pGetImgOptionsResponse->brightness.Min = p_timg__GetOptionsResponse.ImagingOptions->Brightness->Min;
            }

            if ( p_timg__GetOptionsResponse.ImagingOptions->Contrast )
            {
                pGetImgOptionsResponse->Contrast.Max = p_timg__GetOptionsResponse.ImagingOptions->Contrast->Max;
                pGetImgOptionsResponse->Contrast.Min = p_timg__GetOptionsResponse.ImagingOptions->Contrast->Min;
            }

            if ( p_timg__GetOptionsResponse.ImagingOptions->ColorSaturation )
            {
                pGetImgOptionsResponse->ColorSaturation.Max = p_timg__GetOptionsResponse.ImagingOptions->ColorSaturation->Max;
                pGetImgOptionsResponse->ColorSaturation.Min = p_timg__GetOptionsResponse.ImagingOptions->ColorSaturation->Min;
            }
            if ( p_timg__GetOptionsResponse.ImagingOptions->Sharpness )
            {
                pGetImgOptionsResponse->Sharpness.Max = p_timg__GetOptionsResponse.ImagingOptions->Sharpness->Max;
                pGetImgOptionsResponse->Sharpness.Min = p_timg__GetOptionsResponse.ImagingOptions->Sharpness->Min;
            }
        }
    }
    if (soap->error) {
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);

    return soap->error;

}


int ONVIF_GetImageConfiguration(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Config *pGetImgConfigurationResponse)
{

    if ( !pDeviceInfo || !pGetImgConfigurationResponse || !videoSourceToken ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr[0])
    {
        ONVIF_DBP(" &&&&&&&&&&&&&媒体服务地址为空 \n");
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr,255);
    }


    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;
#endif

    soap_set_namespaces(soap, namespaces);

    soap_header(soap);


    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _timg__GetImagingSettings *p_timg__GetImagingSettings =
            (struct _timg__GetImagingSettings *)soap_malloc(soap, sizeof( struct _timg__GetImagingSettings));
            memset(p_timg__GetImagingSettings, 0, sizeof( struct _timg__GetImagingSettings ));

            p_timg__GetImagingSettings->VideoSourceToken = (char*)soap_malloc(soap, 64);
            memset(p_timg__GetImagingSettings->VideoSourceToken, 0, 64);
            strncpy(p_timg__GetImagingSettings->VideoSourceToken, videoSourceToken, 63);

            struct _timg__GetImagingSettingsResponse *p_timg__GetImagingSettingsResponse =
                    (struct _timg__GetImagingSettingsResponse *)soap_malloc(soap, sizeof( struct _timg__GetImagingSettingsResponse));
                    memset(p_timg__GetImagingSettingsResponse, 0, sizeof(struct _timg__GetImagingSettingsResponse));

                    soap_call___timg__GetImagingSettings(soap, ServerAddr, NULL, p_timg__GetImagingSettings, p_timg__GetImagingSettingsResponse);

                    if (!soap->error) {

                        if ( p_timg__GetImagingSettingsResponse->ImagingSettings ) {

                            if ( p_timg__GetImagingSettingsResponse->ImagingSettings->Brightness ) {
                                pGetImgConfigurationResponse->brightness = *p_timg__GetImagingSettingsResponse->ImagingSettings->Brightness;
                            }

                            if ( p_timg__GetImagingSettingsResponse->ImagingSettings->Contrast ) {
                                pGetImgConfigurationResponse->Contrast = *p_timg__GetImagingSettingsResponse->ImagingSettings->Contrast;
                            }

                            if ( p_timg__GetImagingSettingsResponse->ImagingSettings->ColorSaturation ) {
                                pGetImgConfigurationResponse->ColorSaturation = *p_timg__GetImagingSettingsResponse->ImagingSettings->ColorSaturation;
                            }

                            if ( p_timg__GetImagingSettingsResponse->ImagingSettings->Sharpness ) {
                                pGetImgConfigurationResponse->Sharpness = *p_timg__GetImagingSettingsResponse->ImagingSettings->Sharpness;
                                ONVIF_DBP("%f,%f,%f,%f",*p_timg__GetImagingSettingsResponse->ImagingSettings->Brightness,
                                          *p_timg__GetImagingSettingsResponse->ImagingSettings->Contrast,pGetImgConfigurationResponse->ColorSaturation,pGetImgConfigurationResponse->Sharpness);

                            }
                            ONVIF_DBP("%f,%f,%f,%f",*p_timg__GetImagingSettingsResponse->ImagingSettings->Brightness,
                                      *p_timg__GetImagingSettingsResponse->ImagingSettings->Contrast, *p_timg__GetImagingSettingsResponse->ImagingSettings->ColorSaturation, *p_timg__GetImagingSettingsResponse->ImagingSettings->Sharpness);
                        }
                    }

                    if (soap->error)
                    {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }


                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);

                    return soap->error;

}


int ONVIF_SetImageConfiguration(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Config *pSetImgConfiguration)
{
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    if ( !pDeviceInfo || !pSetImgConfiguration || !videoSourceToken ) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if(index < 0 || index > MAX_CAMERA - 1)
    {
        return -1;
    }
    char ServerAddr[256];
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr[0])
    {
        sprintf(ServerAddr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
    }
    else
    {
        strncpy(ServerAddr,g_onvifCameras.onvif_info[index].tdsCapabilities.imagingCapabilities.XAddr,255);
    }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = 2;
    soap->recv_timeout = 2;
    soap->send_timeout = 2;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    ONVIF_DBP("token : %s, brightness=%f,Contrast=%f,ColorSaturation=%f,Sharpness=%f \n", videoSourceToken,pSetImgConfiguration->brightness,pSetImgConfiguration->Contrast,pSetImgConfiguration->ColorSaturation,pSetImgConfiguration->Sharpness);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
    struct _timg__SetImagingSettings *p_timg__SetImagingSettings =(struct _timg__SetImagingSettings *)soap_malloc(soap, sizeof( struct _timg__SetImagingSettings ));

    memset(p_timg__SetImagingSettings, 0, sizeof(struct _timg__SetImagingSettings));

    p_timg__SetImagingSettings->VideoSourceToken = (char*)soap_malloc(soap, 64);
    memset(p_timg__SetImagingSettings->VideoSourceToken, 0, 64);
    strncpy(p_timg__SetImagingSettings->VideoSourceToken, videoSourceToken, 63);

    p_timg__SetImagingSettings->ForcePersistence =
            (enum xsd__boolean*)soap_malloc(soap, sizeof( enum xsd__boolean));
    *p_timg__SetImagingSettings->ForcePersistence = true ;

    p_timg__SetImagingSettings->ImagingSettings =
            (struct tt__ImagingSettings20*)soap_malloc(soap, sizeof( struct tt__ImagingSettings20));
    memset(p_timg__SetImagingSettings->ImagingSettings, 0, sizeof(struct tt__ImagingSettings20));

    p_timg__SetImagingSettings->ImagingSettings->Brightness = (float*)soap_malloc(soap, sizeof(float));
    p_timg__SetImagingSettings->ImagingSettings->Contrast = (float*)soap_malloc(soap, sizeof(float));
    p_timg__SetImagingSettings->ImagingSettings->ColorSaturation = (float*)soap_malloc(soap, sizeof(float));
    p_timg__SetImagingSettings->ImagingSettings->Sharpness = (float*)soap_malloc(soap, sizeof(float));

    *p_timg__SetImagingSettings->ImagingSettings->Brightness = pSetImgConfiguration->brightness;
    *p_timg__SetImagingSettings->ImagingSettings->Contrast = pSetImgConfiguration->Contrast;
    *p_timg__SetImagingSettings->ImagingSettings->ColorSaturation = pSetImgConfiguration->ColorSaturation;
    *p_timg__SetImagingSettings->ImagingSettings->Sharpness = pSetImgConfiguration->Sharpness;

    struct _timg__SetImagingSettingsResponse *p_timg__SetImagingSettingsResponse =(struct _timg__SetImagingSettingsResponse *)soap_malloc(soap, sizeof(struct _timg__SetImagingSettingsResponse));

    soap_call___timg__SetImagingSettings(soap,ServerAddr, NULL, p_timg__SetImagingSettings, p_timg__SetImagingSettingsResponse);

    if (soap->error)
    {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }

    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);
    soap->error = (soap->error == 0 ? 1 : -1);
    ONVIF_DBP("################%s\n",pDeviceInfo->ip);
    return soap->error;

}


int msSleep(long ms)
{
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = ms * 1000;
    return select(0, NULL, NULL, NULL, &tv);
}


pthread_mutex_t g_eventLock;



#if 1

/*********************************************/
/**************事件模块************************/
/*********************************************/


int ONVIF_GetLocalIP(char *ifname, char * ip_buf)
{

    // fprintf(stderr,"@@@@@@@@@@@@@@get ip  %s :%d \n",__func__,__LINE__);
    struct ifreq ifr;
    int skfd;
    struct sockaddr_in *saddr;

    if ( (skfd = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        // fprintf(stderr,"socket error\n");
        return -1;
    }
    memset(&ifr, 0, sizeof( struct ifreq));
    strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name) - 1);
    if (ioctl(skfd, SIOCGIFADDR, &ifr) < 0) {
        // fprintf(stderr,"net_get_ifaddr: ioctl SIOCGIFADDR\n");
        close(skfd);
        return -1 ;
    }

    saddr = (struct sockaddr_in *) &ifr.ifr_addr;
    strcpy(ip_buf, inet_ntoa(saddr->sin_addr));
    // fprintf(stderr,"@@@@@@@@@@@@@@%s\n",ip_buf);
    close(skfd);
    return 0;
}



typedef union __NET_IPV4
{
    int  int32;
    char str[4];
} NET_IPV4;



int ONVIF_EventSubscribe(Remote_Device_Info *pDeviceInfo)
{
#if 1
    if ( !pDeviceInfo) {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    int error = GetCapabilities(pDeviceInfo);
    if ( 1 != error )
    {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0  || index >= MAX_CAMERA ) {
        return -1;
    }
    if(!g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr)
    {
        ONVIF_DBP(" &&&&&&&&&&&&&IMage服务地址为空 \n");
        return -1;
    }

    ONVIF_DBP("endpoint %s \n", g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr);
    char eventAddr[MAX_STRING_LENGTH] = { 0 };

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _tev__GetEventPropertiesResponse *p_tev__GetEventPropertiesResponse =
            ( struct _tev__GetEventPropertiesResponse * )soap_malloc(soap, sizeof(struct _tev__GetEventPropertiesResponse ) );
            memset(p_tev__GetEventPropertiesResponse, 0, sizeof(  struct _tev__GetEventPropertiesResponse ) );
            struct _tev__GetEventProperties *p_tev__GetEventProperties =
                    (struct _tev__GetEventProperties *)soap_malloc(soap, sizeof(struct _tev__GetEventProperties));
                    memset(p_tev__GetEventProperties, 0, sizeof(struct _tev__GetEventProperties));

                    soap_call___ns2__GetEventProperties(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr, NULL, p_tev__GetEventProperties, p_tev__GetEventPropertiesResponse);

                    int size = 0;
                    int k = 0;

                    if (!soap->error) {
                        if ( soap->header ) {
                            if ( soap->header->wsa__To ) {
                                strncpy(eventAddr, soap->header->wsa__To, MAX_STRING_LENGTH - 1 );
                                ONVIF_DBP("eventAddr %s \n", eventAddr);
                            }
                        }

                        if ( p_tev__GetEventPropertiesResponse->wstop__TopicSet) {
                            size = p_tev__GetEventPropertiesResponse->wstop__TopicSet->__size;
                            if ( size > 0 ) {
                                for (k = 0; k < size; k++ ) {
                                    //ONVIF_DBP(" any[%d] %s \n", k, p_tev__GetEventPropertiesResponse->wstop__TopicSet->__any[k] );
                                }
                            }
                        }
                    }

                    if (soap->error) {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    if ( soap->error )
                    {
                        ONVIF_DBP("!!!!!!!!!!!!!!!!不支持事件订阅！！！！！！！！！！！！！！！！！", eventAddr);
                        return -1;
                    }

                    ONVIF_DBP("!!!!!!!!!!!!!!!!开始事件订阅！！！！！！！！！！！！！！！！！", eventAddr);

                    soap_init(soap);

#ifdef ONVIF_TIMEOUT
                    soap->connect_timeout = g_onvif_time_out;
                    soap->recv_timeout = g_onvif_time_out;
                    soap->send_timeout = g_onvif_time_out;
#endif

                    soap_set_namespaces(soap, namespaces);
                    soap_header(soap);
                    //权限认证
                    if(pDeviceInfo->userName[0])
                    {
                        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
                    }


                    struct _wsnt__Subscribe *p_wsnt__Subscribe =
                            (struct _wsnt__Subscribe *)soap_malloc(soap, sizeof(struct _wsnt__Subscribe ));
                            memset(p_wsnt__Subscribe, 0, sizeof(struct _wsnt__Subscribe ));
                            //memset(&p_wsnt__Subscribe->ConsumerReference, 0, sizeof(struct wsa__EndpointReferenceType));
                            p_wsnt__Subscribe->ConsumerReference.Address = (char*)soap_malloc(soap, MAX_STRING_LENGTH);
                            memset(p_wsnt__Subscribe->ConsumerReference.Address, 0, MAX_STRING_LENGTH - 1);
                            char ip_buff[32];

                            ONVIF_GetLocalIP("eth0", ip_buff);
                            sprintf(p_wsnt__Subscribe->ConsumerReference.Address, "http://%s:8181/",
                                    ip_buff);
                            // fprintf(stderr,"p_wsnt__Subscribe->ConsumerReference.Address=%s\n",p_wsnt__Subscribe->ConsumerReference.Address);

                            ONVIF_DBP("!!!!!!!!!!!!!!!!开始事件订阅！！！！！！！！！！！！！！！！！%s\n", p_wsnt__Subscribe->ConsumerReference.Address);

                            p_wsnt__Subscribe->InitialTerminationTime = (char*)soap_malloc(soap, MAX_STRING_LENGTH);
                            strcpy(p_wsnt__Subscribe->InitialTerminationTime, "PT60S");

                            struct _wsnt__SubscribeResponse *p_wsnt__SubscribeResponse =
                                    (struct _wsnt__SubscribeResponse *)soap_malloc(soap, sizeof(struct _wsnt__SubscribeResponse ));
                                    memset(p_wsnt__SubscribeResponse, 0, sizeof(struct _wsnt__SubscribeResponse));

                                    soap_call___ns4__Subscribe(soap, g_onvifCameras.onvif_info[index].tdsCapabilities.eventCapabilities.XAddr, NULL, p_wsnt__Subscribe, p_wsnt__SubscribeResponse);

                                    if ( !soap->error)
                                    {
                                        int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );

                                        if ( (-1 != index) && index < MAX_CAMERA )
                                        {
                                            if ( p_wsnt__SubscribeResponse->TerminationTime )
                                            {
                                                ONVIF_DBP("TerminationTime %d \n", *p_wsnt__SubscribeResponse->TerminationTime);
                                                g_onvifCameras.CameraEvents[index].endTime =
                                                        *p_wsnt__SubscribeResponse->TerminationTime;
                                            }

                                            if ( p_wsnt__SubscribeResponse->CurrentTime ) {
                                                ONVIF_DBP("wsnt__CurrentTime %d \n", *p_wsnt__SubscribeResponse->CurrentTime);

                                                g_onvifCameras.CameraEvents[index].currentTime =
                                                        *p_wsnt__SubscribeResponse->CurrentTime;
                                                g_onvifCameras.CameraEvents[index].startTime =
                                                        *p_wsnt__SubscribeResponse->CurrentTime;

                                            }

                                            if ( p_wsnt__SubscribeResponse->SubscriptionReference.Address ) {

                                                strncpy(g_onvifCameras.CameraEvents[index].subscribeUrl,
                                                        p_wsnt__SubscribeResponse->SubscriptionReference.Address,
                                                        MAX_STRING_LENGTH - 1);
                                                ONVIF_DBP("SubscriptionReference->Address %s\n", p_wsnt__SubscribeResponse->SubscriptionReference.Address);
                                                // ONVIF_DBP("SubscriptionReference->Address %s", g_onvifCameras.CameraEvents[index].subscribeUrl);

                                                g_onvifCameras.CameraEvents[index].subscribeUrl[MAX_STRING_LENGTH - 1] = '\0';
                                            }
                                        }
                                    }

                                    if (soap->error) {
                                        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                                    }
                                    soap_destroy(soap);
                                    soap_end(soap);
                                    soap_done(soap);

                                    soap->error = (soap->error == 0 ? 1 : -1);

                                    return soap->error;
#endif

}


int ONVIF_EventUnSubscribe(Remote_Device_Info *pDeviceInfo)
{

    if ( !pDeviceInfo)
    {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    char endpoint[MAX_STRING_LENGTH];
    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );
    if ( index < 0  || index >= MAX_CAMERA ) {
        return -1;
    }
    ONVIF_DBP("@@@@@@@@@@@@@%s:%d:%s\n",__func__,__LINE__,g_onvifCameras.CameraEvents[index].subscribeUrl);
    if ( !g_onvifCameras.CameraEvents[index].subscribeUrl[0])
    {
        return -1;
    }

    ONVIF_DBP("endpoint %s \n", g_onvifCameras.CameraEvents[index].subscribeUrl);

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif
    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }



    struct _wsnt__Unsubscribe *p_wsnt__Unsubscribe =
            (struct _wsnt__Unsubscribe *)soap_malloc(soap, sizeof(struct _wsnt__Unsubscribe ));
            memset(p_wsnt__Unsubscribe, 0, sizeof(struct _wsnt__Unsubscribe));

            struct _wsnt__UnsubscribeResponse *p_wsnt__UnsubscribeResponse =
                    (struct _wsnt__UnsubscribeResponse *)soap_malloc(soap, sizeof(struct _wsnt__UnsubscribeResponse ));
                    memset(p_wsnt__UnsubscribeResponse, 0, sizeof(struct _wsnt__UnsubscribeResponse));


                    soap_call___ns8__Unsubscribe(soap, g_onvifCameras.CameraEvents[index].subscribeUrl, NULL, p_wsnt__Unsubscribe, p_wsnt__UnsubscribeResponse);

                    if (soap->error)
                    {
                        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);

                    return soap->error;
}



int ONVIF_EventRenew(Remote_Device_Info *pDeviceInfo)
{
#if 1
    if ( !pDeviceInfo) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );

    if ( index < 0  || index >= MAX_CAMERA ) {
        return -1;
    }

    if ( !g_onvifCameras.CameraEvents[index].subscribeUrl )
    {
        return -1;
    }
    ONVIF_DBP("endpoint %s \n", g_onvifCameras.CameraEvents[index].subscribeUrl);

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);
    //权限认证
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }


    struct _wsnt__Renew  * p_wsnt__Renew  = (struct _wsnt__Renew  *) soap_malloc(soap,sizeof(struct _wsnt__Renew));
    memset(p_wsnt__Renew,0,sizeof(struct _wsnt__Renew));
    p_wsnt__Renew->TerminationTime = (char*)soap_malloc(soap, MAX_STRING_LENGTH);
    strcpy(p_wsnt__Renew->TerminationTime, "PT240S");


    struct _wsnt__RenewResponse  * p_wsnt__RenewResponse  = (struct _wsnt__RenewResponse  *) soap_malloc(soap,sizeof(struct _wsnt__RenewResponse));
    memset(p_wsnt__RenewResponse,0,sizeof(struct _wsnt__RenewResponse));
    soap_call___ns3__Renew(soap, g_onvifCameras.CameraEvents[index].subscribeUrl, NULL, p_wsnt__Renew, p_wsnt__RenewResponse);

    if (!soap->error)
    {
        ONVIF_DBP("p_wsnt__RenewResponse end time %d \n", p_wsnt__RenewResponse->TerminationTime );
        pthread_mutex_lock(&g_eventLock);

        if ( p_wsnt__RenewResponse->CurrentTime )
        {
            g_onvifCameras.CameraEvents[index].startTime =  *p_wsnt__RenewResponse->CurrentTime ;
            ONVIF_DBP("p_wsnt__RenewResponse curent time %d \n", *p_wsnt__RenewResponse->CurrentTime);
        }
        g_onvifCameras.CameraEvents[index].endTime = p_wsnt__RenewResponse->TerminationTime ;
        pthread_mutex_unlock(&g_eventLock);

        ONVIF_DBP("%s renew get endTime %d \n", g_onvifCameras.CameraEvents[index].subscribeUrl,
                  p_wsnt__RenewResponse->TerminationTime);

    }

    if (soap->error) {
        ONVIF_DBP("soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
    }
    soap_destroy(soap);
    soap_end(soap);
    soap_done(soap);

    soap->error = (soap->error == 0 ? 1 : -1);

    return soap->error;

#endif
}




int g_threadEventExit = 0;
pthread_t g_threadEventM = -1;
pthread_t g_threadEventServer = -1;

ONVIF_EventCallback g_EventCallback = NULL;


#if 0
void *thread_EventRenew( void *arg )
{
    Remote_Device_Info *pDeviceInfo = (Remote_Device_Info*)arg;

    ONVIF_DBP(" Enter\n");

    if ( !pDeviceInfo ) {
        return;
    }

    ONVIF_DBP(" ip %s port %d\n", pDeviceInfo->ip, pDeviceInfo->port);

    ONVIF_EventRenew(pDeviceInfo);

    ONVIF_DBP(" exit \n");

}
#endif

void *thread_EventManager( void* args )
{
    int i = 0;
    Remote_Device_Info pDeviceInfo;

    while ( !g_threadEventExit )
    {
        // fprintf(stderr,"%s:%d \n",__func__,__LINE__);

        for (i = 0; i < MAX_CAMERA - 1; i++ )
        {

            if ( g_onvifCameras.CameraEvents[i].subscribeUrl[0] )
            {

                pthread_mutex_lock(&g_eventLock);
                g_onvifCameras.CameraEvents[i].startTime++;
                pthread_mutex_unlock(&g_eventLock);
                // fprintf(stderr,"%s:%d :%d  %d\n",__func__,__LINE__,g_onvifCameras.CameraEvents[i].startTime,g_onvifCameras.CameraEvents[i].endTime);
                if ( g_onvifCameras.CameraEvents[i].startTime + 60 > g_onvifCameras.CameraEvents[i].endTime)
                {
                    //fprintf(stderr,"%s:%d :%s\n",__func__,__LINE__,g_onvifCameras.CameraEvents[i].subscribeUrl);
                    ONVIF_DBP(" url %s renew \n", g_onvifCameras.CameraEvents[i].subscribeUrl );
                    // renew
                    // pthread_t th;
                    //pthread_create(&th, NULL, thread_EventRenew, (void*)&g_onvifCameras.cameraIpInfo[i]);
                    strcpy(pDeviceInfo.ip, g_onvifCameras.cameraIpInfo[i].ip);
                    strcpy(pDeviceInfo.userName, g_onvifCameras.cameraIpInfo[i].User);
                    strcpy(pDeviceInfo.password, g_onvifCameras.cameraIpInfo[i].Password);
                    pDeviceInfo.port =g_onvifCameras.cameraIpInfo[i].port;
                    ONVIF_EventRenew(&pDeviceInfo);
                    // fprintf(stderr,"%s:%d :%s :%s\n",__func__,__LINE__,pDeviceInfo.userName,pDeviceInfo.password);
                }
            }
        }
        usleep(800 *1000*2);
    }
    g_threadEventM = -1;
    pthread_mutex_destroy(&g_eventLock);
    ONVIF_DBP(" EXIT thread_EventManager \n");

}



int Onvif_EventListen( )
{
    int sockOptVal = 1;
    struct sockaddr_in svrAddr;
    int serverSocket = -1;

    serverSocket = socket(PF_INET, SOCK_STREAM, 0);
    if(-1 == serverSocket){
        return -1;
    }
    if (-1 == setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &sockOptVal, sizeof(int))) {
        close(serverSocket);
        return -1;
    }

    memset(&svrAddr, 0, sizeof(svrAddr));
    svrAddr.sin_addr.s_addr 	= htonl(INADDR_ANY);
    svrAddr.sin_family 		= AF_INET;
    svrAddr.sin_port  		= htons(8181);

    if (-1 == bind(serverSocket, (struct sockaddr *)&svrAddr, sizeof(struct sockaddr))){
        close(serverSocket);
        return -1;
    }

    if (-1 == listen(serverSocket, SOMAXCONN )){
        close(serverSocket);
        return -1;
    }

    return serverSocket;

}



int EventHandle( char* pBuf )
{
    if ( !pBuf ) {
        return -1;
    }

    char *pStart = pBuf;
    char *pAction = NULL;

    char *space = "http://docs.oasis-open.org/wsn/bw-2/NotificationConsumer/Notify";

#if  0
    pStart = (char*)strcasestr(pBuf, "action");

    if ( pStart ) {
        pAction = pStart + strlen("action") + 1;
        pStart = pAction;
        pStart += strlen(space);
        *pStart = '\0';
    }

    //printf("action %s \n", pAction);

    // Name="AlarmType" Value="AlarmLocal


    pStart += 1;



    fprintf(stderr,"alarm value \n" );
    pStart = (char*)strcasestr(pBuf, "ismotion");

    if ( pStart )
    {
        pStart = strstr(pStart, "Value");

        if ( pStart )
        {
            pStart += strlen("Value=\"");

            char *pValue = pStart;

            pStart = strstr(pStart, "\"");
            *pStart = '\0';

            fprintf(stderr,"alarm value %s \n", pValue);

            if ( strcmp(pValue, "true" ) == 0 )
            {
                return 1;
            }
            if ( strcmp( pValue, "false" ) == 0 )
            {
                return 2;
            }

        }
    }
#endif
    pStart = (char*)strcasestr(pBuf, "ismotion");
    if (pStart)
    {
        pAction = (char*)strcasestr(pStart, "value=\"true\"");
        if(pAction)
        {
            return  2;
        }
        pAction = (char*)strcasestr(pStart, "value=\"false\"");
        if(pAction)
        {
            return  1;
        }
    }

    return 0;

}

char recvBuf[1024*20];

void *thread_EventServer( void* args  )
{

    int svrSockFd, cliSockFd;
    int maxFd = 0;
    int ret, sockOptVal = 1;
    socklen_t addrLen;

    int recvLen = 0;

    struct sockaddr_in cliAddr;
    struct linger  socklinger;
    struct timeval timeout;
     AlarmType pAlarmType;
     AlarmStatus pAlarmStatus;
    svrSockFd = Onvif_EventListen();

    if ( -1 == svrSockFd ) {
        return;
    }
    char *pStart = NULL;
    char *pAction = NULL;
    fd_set sockFds;

    while ( !g_threadEventExit )
    {
        FD_ZERO(&sockFds);
        FD_SET(svrSockFd, &sockFds);

        maxFd = svrSockFd;
        pAlarmType = NO_ALARM;
        timeout.tv_sec  = 2;
        timeout.tv_usec = 0;	// 2 s timeout
        // fprintf(stderr,"%s:%d\r\n ",__func__,__LINE__);
        ret = select(maxFd + 1, &sockFds, NULL, NULL, &timeout);
        // fprintf(stderr,"%s:%d\r\n ",__func__,__LINE__);
        if(ret < 0){
            if(errno == EINTR)
            {
                continue;
            }

            break;
        } else if(ret == 0)
        {
            continue;
            sleep(2);

        } else {

            if (FD_ISSET(svrSockFd, &sockFds))
            {
                addrLen = sizeof(cliAddr);
                cliSockFd = accept(svrSockFd, (struct sockaddr *)&cliAddr,&addrLen);
                if(cliSockFd <= 0){
                    // fprintf(stderr, "%s :%d: accept \r\n", __FILE__, __LINE__);
                    continue;
                }
                //fprintf(stderr, "%s :%d: accept \r\n", __FILE__, __LINE__);
                //dbg(Dbg, DbgNoPerror, "accept a client\n");
                socklinger.l_onoff	= 1;
                socklinger.l_linger = 0;
                timeout.tv_sec  = 2;  // 3
                timeout.tv_usec = 0;
                if ( -1 == setsockopt(cliSockFd, SOL_SOCKET, SO_RCVTIMEO,(char*)&timeout, sizeof(timeout))){
                    //dbg(Err, DbgPerror, "setsockopt cliSockFd = %d error\n",cliSockFd);
                }
                if ( -1 == setsockopt(cliSockFd, SOL_SOCKET, SO_LINGER,  &socklinger, sizeof(socklinger))){
                    //dbg(Err, DbgPerror, "setsockopt cliSockFd = %d error\n",cliSockFd);
                }
                if ( -1 == setsockopt(cliSockFd, IPPROTO_TCP, TCP_NODELAY, &sockOptVal, sizeof(int))){
                    //dbg(Err, DbgPerror, "setsockopt cliSockFd = %d error\n",cliSockFd);
                }


                int ret = 0;
                recvLen = 0;
                int leftLen = 10240;
                // fprintf(stderr, "%s :%d: accept \r\n", __FILE__, __LINE__);
                while ( leftLen > 0 ) {

                    ret = recv(cliSockFd, recvBuf+recvLen, leftLen, 0);
                    if (ret <= 0) {

                        break;
                    }

                    // fprintf(stderr, "%s :%d: recv %d  \r\n", __FILE__, __LINE__,ret);
                    recvLen += ret;
                    leftLen -= ret;
                    usleep(100*1000);
                }

                // fprintf(stderr, "%s :%d: recvLen %d \r\n recvBuf: %s \r\n", __FILE__, __LINE__, recvLen,recvBuf);
                // fprintf(stderr, "%s :%d: recv \r\n", __FILE__, __LINE__);
                if ( recvLen > 0 )
                {

                 //   int eventType = EventHandle(recvBuf);
                    pStart = (char*)strcasestr(recvBuf, "ismotion");
                    if (pStart)
                    {
                        pAlarmType = MOTION_DETECTION_ALARM;
                        pAction = (char*)strcasestr(pStart, "value=\"true\"");
                        if(pAction)
                        {
                            pAlarmStatus = MOTION_DETECTION_START;
                        }
                        pAction = (char*)strcasestr(pStart, "value=\"false\"");
                        if(pAction)
                        {
                           pAlarmStatus = MOTION_DETECTION_END;
                        }
                    }


                    if ( pAlarmType >= MOTION_DETECTION_ALARM  && g_EventCallback )
                    {
                        char ipaddr[MAX_IP_LEN] = { 0 };
                        // fprintf(stderr,"==================got motiondetect  , ip %s , type =%d \n", inet_ntoa(cliAddr.sin_addr),eventType );
                        sprintf(ipaddr, "%s", inet_ntoa(cliAddr.sin_addr));
                        g_EventCallback(ipaddr, pAlarmType,pAlarmStatus);
                    }
                }

                close(cliSockFd);
                cliSockFd = -1;

            }
        }
    }

    g_threadEventServer = -1;

    ONVIF_DBP(" EXIT thread_EventServer \n");


}

int ONVIF_EventServerStart( ONVIF_EventCallback pEventCallback )
{
#if  1
    if ( pEventCallback ) {
        g_EventCallback = pEventCallback;
    }

    if ( -1 == g_threadEventM )
    {
        pthread_mutex_init(&g_eventLock, NULL);

        pthread_create(&g_threadEventM, NULL, thread_EventManager, NULL);

        pthread_create(&g_threadEventServer, NULL, thread_EventServer, NULL);
    }

#endif
}

int ONVIF_EventServerStop()
{
    g_threadEventExit = 1;

    sleep(3);

    if ( -1 != g_threadEventM) {
        pthread_join(g_threadEventM, NULL);
        g_threadEventM = -1;
    }

    if ( -1 != g_threadEventServer ) {
        pthread_join(g_threadEventServer, NULL);
        g_threadEventServer = -1;
    }
    pthread_mutex_destroy (&g_eventLock);
}



#endif

u_int32_t dton(u_int32_t  mask)
{ //转换函数3
    u_int32_t i, c;
    int bits = sizeof(u_int32_t) * 8;
    i = ~0;
    bits -= mask;
    /* 让32位全是1的无符号数与左移bits位(右移位用0填充)得出子网掩码 */
    i <<= bits;
    return htonl(i);
}




int ONVIF_GetNetworkInterfaces(Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface)
{
    if ( !pDeviceInfo) {
        return -1;
    }
    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
   // int index = ONVIF_GetCameraIndex( pDeviceInfo->ip );

   // if ( index < 0  || index >= MAX_CAMERA ) {
   //     return -1;
   // }

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    struct in_addr len;
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
   }

    struct _tds__GetNetworkInterfaces * tds__GetNetworkInterfaces =
            ( struct _tds__GetNetworkInterfaces *)soap_malloc(soap,sizeof( struct _tds__GetNetworkInterfaces ));

            struct _tds__GetNetworkInterfacesResponse * tds__GetNetworkInterfacesResponse =
                    ( struct _tds__GetNetworkInterfacesResponse *)soap_malloc(soap,sizeof( struct _tds__GetNetworkInterfacesResponse ));

                    char pEndPointStr[MAX_STRING_LENGTH];
                    sprintf(pEndPointStr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
                    fprintf(stderr,"%s \r\n",pEndPointStr);

                    soap_call___tds__GetNetworkInterfaces(soap, pEndPointStr , NULL, tds__GetNetworkInterfaces, tds__GetNetworkInterfacesResponse);
                    if (!soap->error)
                    {
                        fprintf(stderr,"%s \r\n",pEndPointStr);
                        AllInterface->__sizeNetworkInterfaces =tds__GetNetworkInterfacesResponse->__sizeNetworkInterfaces;

                        int i ;
                        for(i=0 ; i!= tds__GetNetworkInterfacesResponse->__sizeNetworkInterfaces;++i)
                        {
                            strncpy(AllInterface->SingleNetworkInterface[i].token , tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].token,MAX_STRING_LENGTH);
                            AllInterface->SingleNetworkInterface[i].Enable = tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].Enabled;
                            strncpy(AllInterface->SingleNetworkInterface[i].InterfaceName , tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].Info->Name,MAX_STRING_LENGTH);
                            strncpy(AllInterface->SingleNetworkInterface[i].HwAddress , tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].Info->HwAddress,MAX_STRING_LENGTH);
                            strncpy(AllInterface->SingleNetworkInterface[i].IpAddress ,tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].IPv4->Config->Manual->Address,MAX_STRING_LENGTH);
                            AllInterface->SingleNetworkInterface[i].DHCP =tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].IPv4->Config->DHCP;
                            AllInterface->SingleNetworkInterface[i].NetMaskLength =tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].IPv4->Config->Manual->PrefixLength;
                            len.s_addr = dton(tds__GetNetworkInterfacesResponse->NetworkInterfaces[i].IPv4->Config->Manual->PrefixLength);
                            strncpy(AllInterface->SingleNetworkInterface[i].NetMask,inet_ntoa(len),MAX_STRING_LENGTH);
                            fprintf(stderr,"name=%s,ip=%s,netmask=%s\r\n",AllInterface->SingleNetworkInterface[i].InterfaceName,AllInterface->SingleNetworkInterface[i].IpAddress,AllInterface->SingleNetworkInterface[i].NetMask);
                        }
                    }

                    if (soap->error) {
                        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);

                    soap->error = (soap->error == 0 ? 1 : -1);

                    return soap->error;
}

int vMaskToPrefix(char * p_strXAddr, int *p_iPrefixLength)
{
    in_addr_t       netMask;
    unsigned char   ucBuff;
    int   len;
    int   i;

    if((0==p_strXAddr) || (0==p_iPrefixLength)) return -1;
    netMask = inet_addr(p_strXAddr);

    do{
        len = 0;
        for(i=0;i<4;i++){
            ucBuff = *(i+((unsigned char*)&netMask));
            if(0==ucBuff) continue;
            if(len!=(i*8)){
                len = -1;
                break;
            }
            while(ucBuff>0){
                if(0==(ucBuff & 0x80)){
                    len = -1;
                    break;
                }
                len ++;
                ucBuff = ucBuff << 1;
            }
            if(0>len) break;
        }
        if(0>len) break;

        *p_iPrefixLength = len;
        return 0;
    }while(0);
    *p_iPrefixLength = 0;
    return -1;
}

#if 0
void vMaskToPrefix( char * p_strXAddr, int *p_iPrefixLength)
{
    int l_iPrefixLength = 0;
    in_addr_t netMask = inet_addr(p_strXAddr);

    while(netMask>0){
        if(netMask%2) l_iPrefixLength++;
        netMask = netMask >> 1;
    }
    *p_iPrefixLength = l_iPrefixLength;
}



void vMaskToPrefix( char * p_strXAddr, int *p_iPrefixLength)
{
    int l_iPrefixLength = 0;
    in_addr_t netMask = inet_addr("255.255.248.0");
    fprintf(stderr,"netmask =%ld\r\n",netMask);
    while(netMask>0){
        netMask = netMask >> 1;
        l_iPrefixLength++;
    }
    fprintf(stderr,"%s : %d %d\r\n",p_strXAddr,netMask,l_iPrefixLength);
    *p_iPrefixLength = l_iPrefixLength;


    int l_iPrefixLength = 0;
    int l_iTemp = 0;
    int i ;
    for(  i  = 0; i != strlen(p_strXAddr); ++i )
    {
        if( isdigit(p_strXAddr[i]))
        {

            l_iTemp = l_iTemp * 10 + (p_strXAddr[i] - '0');
        }
        else
        {

            switch(l_iTemp)
            {
            case 255:
                l_iPrefixLength += 8;
                break;
            case 254:
                l_iPrefixLength += 7;
                break;
            case 252:
                l_iPrefixLength += 6;
                break;
            case 248:
                l_iPrefixLength += 5;
                break;
            case 240:
                l_iPrefixLength += 4;
                break;
            case 224:
                l_iPrefixLength += 3;
                break;
            case 192:
                l_iPrefixLength += 2;
                break;
            case 128:
                l_iPrefixLength += 1;
                break;
            case 0:
                l_iPrefixLength += 0;
                break;
            default:
                break;
            }
            l_iTemp = 0;
        }
    }


    p_iPrefixLength = l_iPrefixLength;
}

#endif


int ONVIF_SetNetworkInterfaces(Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface)
{
    if ( !pDeviceInfo) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }

    fprintf(stderr,"endpoint %s  %s\n", pDeviceInfo->userName,pDeviceInfo->password);

    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

#if 1
    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }
#endif
    struct _tds__SetNetworkInterfaces * tds__SetNetworkInterfaces =
            ( struct _tds__SetNetworkInterfaces *)soap_malloc(soap,sizeof( struct _tds__SetNetworkInterfaces ));

            tds__SetNetworkInterfaces->InterfaceToken =
                    ( char *)soap_malloc(soap,MAX_STRING_LENGTH);
            tds__SetNetworkInterfaces->NetworkInterface =
                    ( struct tt__NetworkInterfaceSetConfiguration *)soap_malloc(soap,sizeof( struct tt__NetworkInterfaceSetConfiguration ));
            memset(tds__SetNetworkInterfaces->NetworkInterface,0,sizeof(struct tt__NetworkInterfaceSetConfiguration));

            tds__SetNetworkInterfaces->NetworkInterface->Link = ( struct tt__NetworkInterfaceConnectionSetting *)soap_malloc(soap,sizeof( struct tt__NetworkInterfaceConnectionSetting ));

            tds__SetNetworkInterfaces->NetworkInterface->Link->AutoNegotiation = xsd__boolean__false_;
            tds__SetNetworkInterfaces->NetworkInterface->Link->Duplex= tt__Duplex__Full;
            tds__SetNetworkInterfaces->NetworkInterface->Link->Speed = 10;
            int  MTU = 1500;
            tds__SetNetworkInterfaces->NetworkInterface->MTU = ( int *)soap_malloc(soap,sizeof(int));

            *tds__SetNetworkInterfaces->NetworkInterface->MTU = MTU;

            tds__SetNetworkInterfaces->NetworkInterface->Enabled =  ( enum xsd__boolean *)soap_malloc(soap,sizeof(enum xsd__boolean));
            tds__SetNetworkInterfaces->NetworkInterface->IPv4 = ( struct tt__IPv4NetworkInterfaceSetConfiguration *)soap_malloc(soap,sizeof(struct tt__IPv4NetworkInterfaceSetConfiguration));

#if 1
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->Enabled = ( enum xsd__boolean *)soap_malloc(soap,sizeof(enum xsd__boolean));
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual = ( struct tt__PrefixedIPv4Address *)soap_malloc(soap,sizeof(struct tt__PrefixedIPv4Address));
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address = (char *)soap_malloc(soap,MAX_STRING_LENGTH);
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address[0] = 0;
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->DHCP = ( enum xsd__boolean *)soap_malloc(soap,sizeof(enum xsd__boolean));

            enum xsd__boolean InterfaceEnable = true ;
            int PrefixLength = 0 ;
            fprintf(stderr,"PrefixLength =%s\r\n",AllInterface->SingleNetworkInterface[0].NetMask);

            int ret = vMaskToPrefix(AllInterface->SingleNetworkInterface[0].NetMask,&PrefixLength);
            if(ret == -1)
            {
                fprintf(stderr,"子网掩码有误!\r\n");
                return -1;
            }
            strncpy( tds__SetNetworkInterfaces->InterfaceToken,AllInterface->SingleNetworkInterface[0].token,MAX_STRING_LENGTH);

            *tds__SetNetworkInterfaces->NetworkInterface->Enabled = InterfaceEnable;
            *tds__SetNetworkInterfaces->NetworkInterface->IPv4->Enabled = InterfaceEnable;
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->__sizeManual = 1;

            strncpy(tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->Address,AllInterface->SingleNetworkInterface[0].IpAddress,MAX_STRING_LENGTH);
            tds__SetNetworkInterfaces->NetworkInterface->IPv4->Manual->PrefixLength   = PrefixLength ;
            fprintf(stderr,"PrefixLength =%d\r\n",PrefixLength);
            bool dhcp = false;
            *tds__SetNetworkInterfaces->NetworkInterface->IPv4->DHCP = false;
#endif

            char pEndPointStr[MAX_STRING_LENGTH];
            sprintf(pEndPointStr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);

            struct _tds__SetNetworkInterfacesResponse * tds__SetNetworkInterfacesResponse =
                    ( struct _tds__SetNetworkInterfacesResponse *)soap_malloc(soap,sizeof( struct _tds__SetNetworkInterfacesResponse ));

                    soap_call___tds__SetNetworkInterfaces(soap,pEndPointStr, NULL, tds__SetNetworkInterfaces, tds__SetNetworkInterfacesResponse);

                    if (soap->error)
                    {
                        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    return soap->error;

}


int ONVIF_SetDefaultGateway(Remote_Device_Info *pDeviceInfo ,char * gateway)
{
    if ( !pDeviceInfo) {
        return -1;
    }

    if ( !pDeviceInfo->ip[0] ) {
        return -1;
    }
    fprintf(stderr,"endpoint %s  %s\n", pDeviceInfo->userName,pDeviceInfo->password);
    struct soap  soap_client;
    struct soap *soap = &soap_client;
    soap_init(soap);

#ifdef ONVIF_TIMEOUT
    soap->connect_timeout = g_onvif_time_out;
    soap->recv_timeout = g_onvif_time_out;
    soap->send_timeout = g_onvif_time_out;
#endif

    soap_set_namespaces(soap, namespaces);
    soap_header(soap);

    if(pDeviceInfo->userName[0])
    {
        ONVIF_SetAuthenticationInformation(soap,pDeviceInfo->userName,pDeviceInfo->password);
    }

    struct _tds__SetNetworkDefaultGateway *tds__SetNetworkDefaultGateway =
            ( struct _tds__SetNetworkDefaultGateway *)soap_malloc(soap,sizeof( struct _tds__SetNetworkDefaultGateway ));


            tds__SetNetworkDefaultGateway->IPv4Address =(char **)soap_malloc(soap,sizeof(char*));
            *(tds__SetNetworkDefaultGateway->IPv4Address) = (char *)soap_malloc(soap,MAX_STRING_LENGTH);;

            strncpy(*(tds__SetNetworkDefaultGateway->IPv4Address) ,gateway,MAX_STRING_LENGTH);
            tds__SetNetworkDefaultGateway->__sizeIPv4Address = 1;
            struct _tds__SetNetworkDefaultGatewayResponse *tds__SetNetworkDefaultGatewayResponse   =
                    ( struct _tds__SetNetworkDefaultGatewayResponse *)soap_malloc(soap,sizeof( struct _tds__SetNetworkDefaultGatewayResponse ));

                    char pEndPointStr[MAX_STRING_LENGTH];
                    sprintf(pEndPointStr,"http://%s:%d/onvif/device_service",pDeviceInfo->ip,pDeviceInfo->port);
                    fprintf(stderr,"%s\r\n",pEndPointStr);
                    soap_call___tds__SetNetworkDefaultGateway(soap,pEndPointStr, NULL,tds__SetNetworkDefaultGateway,tds__SetNetworkDefaultGatewayResponse);
                    if (soap->error)
                    {
                        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(soap), *soap_faultstring(soap));
                    }
                    fprintf(stderr,"!!!!!!!!!!!!!gatewayaddr end \r\n");
                    soap_destroy(soap);
                    soap_end(soap);
                    soap_done(soap);
                    soap->error = (soap->error == 0 ? 1 : -1);
                    return soap->error;
}



int GetNetInterfaceName(char *pInerfaceName)
{
    int ret = -1;

    if(NULL == pInerfaceName)
        return ret;

    FILE* f = fopen("/proc/net/dev", "r");
    if (!f)
    {
        fprintf(stderr, "Open /proc/net/dev failed! errno:%d\n", errno);
        return ret;
    }

    char szLine[512];
    fgets(szLine, sizeof(szLine), f);    /* eat line */
    fgets(szLine, sizeof(szLine), f);
    while(fgets(szLine, sizeof(szLine), f))
    {
        char szName[128] = {0};
        sscanf(szLine, "%s", szName);
        int nLen = strlen(szName);
        if (nLen <= 0) continue;
        if (szName[nLen - 1] == ':') szName[nLen - 1] = 0;
        if (strcmp(szName, "lo") != 0)
        {
            strcpy(pInerfaceName, szName);
            ret = 0;
        }
    }

    fclose(f);
    f = NULL;

    return ret;
}

int GetGateway(unsigned int *pGateway)
{
    char szLine[512];
    char szName[128] = {0};

    FILE* f = fopen("/proc/net/route", "r");
    if (!f)
    {
        fprintf(stderr, "Open /proc/net/route failed! errno:%d\n", errno);
        return -1;
    }

    fgets(szLine, sizeof(szLine), f);    /* eat line */
    fgets(szLine, sizeof(szLine), f);

    sscanf(szLine, "%*s\t%*s\t%s", szName);

    *pGateway = strtol(szName, NULL, 16);

    fclose(f);
    f = NULL;

    return 0;
}


int GetNetInterfaceInfo(const char *pInerfaceName, IPAddress *pIpAddress, char *pMac)
{
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0)
    {
        fprintf(stderr, "Create socket failed!errno=%d", errno);
        return -1;
    }

    struct ifreq ifr;
    unsigned char mac[6];
    unsigned int nIP, nNetmask, nGateway;

    printf("%s:\n", pInerfaceName);

    strcpy(ifr.ifr_name, pInerfaceName);
    if (ioctl(s, SIOCGIFHWADDR, &ifr) < 0)
    {
        return -1;
    }
    memcpy(mac, ifr.ifr_hwaddr.sa_data, sizeof(mac));
    printf("MAC: %02x-%02x-%02x-%02x-%02x-%02x\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    strcpy(ifr.ifr_name, pInerfaceName);
    if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
    {
        nIP = 0;
    }
    else
    {
        nIP = *(unsigned int*)&ifr.ifr_broadaddr.sa_data[2];
    }
    printf("IP: %s\n", inet_ntoa(*(struct in_addr*)&nIP));

    strcpy(ifr.ifr_name, pInerfaceName);
    if (ioctl(s, SIOCGIFNETMASK, &ifr) < 0)
    {
        nNetmask = 0;
    }
    else
    {
        nNetmask = *(unsigned int*)&ifr.ifr_netmask.sa_data[2];
    }
    printf("Netmask: %s\n", inet_ntoa(*(struct in_addr*)&nNetmask));

    if(GetGateway(&nGateway) < 0)
    {
        nGateway = 0;
    }
    printf("Gateway: %s\n", inet_ntoa(*(struct in_addr*)&nGateway));

    close(s);

    strcpy(pIpAddress->ip, inet_ntoa(*(struct in_addr*)&nIP));
    strcpy(pIpAddress->ip, inet_ntoa(*(struct in_addr*)&nIP));
    strcpy(pIpAddress->ip, inet_ntoa(*(struct in_addr*)&nIP));
    memcpy(pMac, mac, 6);

    return 0;
}


//arp包的格式
struct arpMsg {
    /* Ethernet header */
    uint8_t  h_dest[6];     /* 00 destination ether addr */
    uint8_t  h_source[6];   /* 06 source ether addr */
    uint16_t h_proto;       /* 0c packet type ID field */

    /* ARP packet */
    uint16_t htype;         /* 0e hardware type (must be ARPHRD_ETHER) */
    uint16_t ptype;         /* 10 protocol type (must be ETH_P_IP) */
    uint8_t  hlen;          /* 12 hardware address length (must be 6) */
    uint8_t  plen;          /* 13 protocol address length (must be 4) */
    uint16_t operation;     /* 14 ARP opcode */
    uint8_t  sHaddr[6];     /* 16 sender's hardware address */
    uint8_t  sInaddr[4];    /* 1c sender's IP address */
    uint8_t  tHaddr[6];     /* 20 target's hardware address */
    uint8_t  tInaddr[4];    /* 26 target's IP address */
    uint8_t  pad[18];       /* 2a pad for min. ethernet payload (60 bytes) */
} PACKED;


#define ARP_MSG_SIZE 0x2a


int safe_poll(struct pollfd *ufds, nfds_t nfds, int timeout)
{
    while (1) {
        int n = poll(ufds, nfds, timeout);
        if (n >= 0)
            return n;
        /* Make sure we inch towards completion */
        if (timeout > 0)
            timeout--;
        /* E.g. strace causes poll to return this */
        if (errno == EINTR)
            continue;
        /* Kernel is very low on memory. Retry. */
        /* I doubt many callers would handle this correctly! */
        if (errno == ENOMEM)
            continue;
        //bb_perror_msg("poll");
        return n;
    }
}


/* Like strncpy but make sure the resulting string is always 0 terminated. */
char* safe_strncpy(char *dst, const char *src, size_t size)
{
    if (!size) return dst;
    dst[--size] = '\0';
    return strncpy(dst, src, size);
}

unsigned long long monotonic_us(void)
{
   struct timeval tv;
   gettimeofday(&tv, NULL);
   return tv.tv_sec * 1000000ULL + tv.tv_usec;
}




int Arpping(unsigned int test_ip, unsigned int from_ip, unsigned char *from_mac, const char *pInterfaceName)
{

    int timeout_ms;
    struct pollfd pfd[1];

    int  s     ;      /* socket */

    int ret = -1;             /* "no reply received" yet */
    struct sockaddr addr;   /* for interface name */

    struct arpMsg arp;

    s = socket(PF_PACKET, SOCK_PACKET, htons(ETH_P_ARP));
    if (s == -1) {
        return ret;
    }

    const int one = 1;
    if (setsockopt(s, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) == -1) {
        goto end;
    }

    /* send arp request */
    memset(&arp, 0, sizeof(arp));
    memset(arp.h_dest, 0xff, 6);                    /* MAC DA */
    memcpy(arp.h_source, from_mac, 6);              /* MAC SA */
    arp.h_proto = htons(ETH_P_ARP);                 /* protocol type (Ethernet) */
    arp.htype = htons(ARPHRD_ETHER);                /* hardware type */
    arp.ptype = htons(ETH_P_IP);                    /* protocol type (ARP message) */
    arp.hlen = 6;                                   /* hardware address length */
    arp.plen = 4;                                   /* protocol address length */
    arp.operation = htons(ARPOP_REQUEST);           /* ARP op code */
    memcpy(arp.sHaddr, from_mac, 6);                /* source hardware address */
    memcpy(arp.sInaddr, &from_ip, sizeof(from_ip)); /* source IP address */
    /* tHaddr is zero-fiiled */                     /* target hardware address */
    memcpy(arp.tInaddr, &test_ip, sizeof(test_ip)); /* target IP address */

    memset(&addr, 0, sizeof(addr));
    safe_strncpy(addr.sa_data, pInterfaceName, sizeof(addr.sa_data));

    if (sendto(s, &arp, sizeof(arp), 0, &addr, sizeof(addr)) < 0) {
        // TODO: error message? caller didn't expect us to fail,
        goto end;
    }
    pfd[0].fd = s;
    ret = 0;
    /* wait for arp reply, and check it */
    timeout_ms = 2000;
    do {
        int r;
        unsigned prevTime = monotonic_us();

        pfd[0].events = POLLIN;
        r = safe_poll(pfd, 1, timeout_ms);
        if (r < 0)
            break;
        if (r) {
            r = read(s, &arp, sizeof(arp));
            if (r < 0)
                break;
            if (r >= ARP_MSG_SIZE
                    && arp.operation == htons(ARPOP_REPLY)
                    /* don't check it: Linux doesn't return proper tHaddr (fixed in 2.6.24?) */
                    /* && memcmp(arp.tHaddr, from_mac, 6) == 0 */
                    && *((uint32_t *) arp.sInaddr) == test_ip
                    ) {
                //说明ip地址已被使用
                ret = 1;
                break;
            }
        }
        timeout_ms -= ((unsigned)monotonic_us() - prevTime) / 1000;
    } while (timeout_ms > 0);

end:
    close(s);

    return ret;

}



int DelSecondIp(const char* pInterfaceName)
{
    int sock_set_ip;

    struct ifreq ifr_set_ip;

    if((sock_set_ip = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket create failse...SetLocalIp!/n");
        return -1;
    }

    bzero( &ifr_set_ip,sizeof(ifr_set_ip));
    strncpy(ifr_set_ip.ifr_name, pInterfaceName, sizeof(ifr_set_ip.ifr_name)-1);

     //设置激活标志
     //ifr_set_ip.ifr_flags |= IFF_UP |IFF_RUNNING;

     //get the status of the device
     if( ioctl( sock_set_ip, SIOCSIFFLAGS, &ifr_set_ip ) < 0 )
     {
          perror("SIOCSIFFLAGS");
          close( sock_set_ip );
          return -1;
     }

    close( sock_set_ip );
    return 0;
}

int SetSecondIp(const char* pInterfaceName, const IPAddress *pIpAddress)
{
    int sock_set_ip;

    struct sockaddr_in sin_set_ip;
    struct ifreq ifr_set_ip;

    if( pIpAddress == NULL )
        return -1;

    if((sock_set_ip = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket create failse...SetLocalIp!/n");
        return -1;
    }

    bzero( &sin_set_ip, sizeof(sin_set_ip));
    bzero( &ifr_set_ip, sizeof(ifr_set_ip));

    sin_set_ip.sin_family = AF_INET;
    sin_set_ip.sin_addr.s_addr = inet_addr(pIpAddress->ip);
    memcpy( &ifr_set_ip.ifr_addr, &sin_set_ip, sizeof(sin_set_ip));
    strncpy(ifr_set_ip.ifr_name, pInterfaceName, sizeof(ifr_set_ip.ifr_name)-1);

    if( ioctl( sock_set_ip, SIOCSIFADDR, &ifr_set_ip) < 0 )
    {
        perror( "SIOCSIFADDR");
        close( sock_set_ip );
        return -1;
    }

    sin_set_ip.sin_addr.s_addr = inet_addr(pIpAddress->mask);
    memcpy( &ifr_set_ip.ifr_netmask, &sin_set_ip, sizeof(sin_set_ip));
    if( ioctl( sock_set_ip, SIOCSIFNETMASK, &ifr_set_ip) < 0 )
    {
        perror( "SIOCSIFNETMASK");
        close( sock_set_ip );
        return -1;
    }

    //设置激活标志
    ifr_set_ip.ifr_flags |= IFF_UP |IFF_RUNNING;

    //get the status of the device
    if( ioctl( sock_set_ip, SIOCSIFFLAGS, &ifr_set_ip ) < 0 )
    {
        perror("SIOCSIFFLAGS");
        close( sock_set_ip );
        return -1;
    }

    close( sock_set_ip );
    return 0;
}

int IsSameSubnet(const char *ip1, const char *mask1, const char *ip2, const char *mask2)
{
    bool ret = -1;

    struct in_addr addr_ip1;
    struct in_addr addr_netmask1;
    struct in_addr addr_ipseg1;
    memset(&addr_ip1, 0, sizeof(struct in_addr));
    memset(&addr_netmask1, 0, sizeof(struct in_addr));
    memset(&addr_ipseg1, 0, sizeof(struct in_addr));

    struct in_addr addr_ip2;
    struct in_addr addr_netmask2;
    struct in_addr addr_ipseg2;
    memset(&addr_ip2, 0, sizeof(struct in_addr));
    memset(&addr_netmask2, 0, sizeof(struct in_addr));
    memset(&addr_ipseg2, 0, sizeof(struct in_addr));

    addr_ip1.s_addr = inet_addr(ip1);
    addr_netmask1.s_addr = inet_addr(mask1);
    addr_ipseg1.s_addr = addr_ip1.s_addr & addr_netmask1.s_addr;

    addr_ip2.s_addr = inet_addr(ip2);
    addr_netmask2.s_addr = inet_addr(mask2);
    addr_ipseg2.s_addr = addr_ip2.s_addr & addr_netmask2.s_addr;

    if (addr_ipseg1.s_addr == addr_ipseg2.s_addr)
    {
        ret = 0;
    }

    return ret;
}

int ONVIF_SetNetworkInterfaces2(int nIndex, Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface)
{
    int ret = -1;
    char szName[128] ="eth0";
    char szSecondName[128];
    IPAddress localAddress;
    char szMac[6];
    unsigned int iaIP;
    IPAddress tempIP;

    if(pDeviceInfo == NULL || AllInterface == NULL)
    {
        return ret;
    }

    if(nIndex >= 0)
    {
#if 0
        if(GetNetInterfaceName(szName) == -1)
        {
            return ret;
        }
#endif

        if(GetNetInterfaceInfo(szName, &localAddress, szMac) == -1)
        {
            return ret;
        }

        snprintf(szSecondName, 128, "%s:0", szName);
        iaIP = inet_addr(defaultSearchIPAddress[nIndex].ip);
        if(FindNotExistIP(&iaIP, szMac, szName) == -1)
        {
            return ret;
        }

        memcpy(&tempIP, &defaultSearchIPAddress[nIndex], sizeof(IPAddress));
        strcpy(tempIP.ip, inet_ntoa(*(struct in_addr*)&iaIP));
        if(SetSecondIp(szSecondName, &tempIP) == -1)
        {
            return ret;
        }
    }

    ret = ONVIF_SetNetworkInterfaces(pDeviceInfo, AllInterface);

    if(nIndex >= 0)
    {
        DelSecondIp(szSecondName);
    }

    return ret;
}

int ONVIF_GetNetworkInterfaces2(int nIndex, Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface)
{
    int ret = -1;
    char szName[128];
    char szSecondName[128];
    IPAddress localAddress;
    char szMac[6];
    unsigned int iaIP;
    IPAddress tempIP;

    if(pDeviceInfo == NULL || AllInterface == NULL)
    {
        return ret;
    }

    if(nIndex >= 0)
    {
        if(GetNetInterfaceName(szName) == -1)
        {
            return ret;
        }

        if(GetNetInterfaceInfo(szName, &localAddress, szMac) == -1)
        {
            return ret;
        }

        snprintf(szSecondName, 128, "%s:0", szName);
        iaIP = inet_addr(defaultSearchIPAddress[nIndex].ip);
        if(FindNotExistIP(&iaIP, szMac, szName) == -1)
        {
            return ret;
        }

        memcpy(&tempIP, &defaultSearchIPAddress[nIndex], sizeof(IPAddress));
        strcpy(tempIP.ip, inet_ntoa(*(struct in_addr*)&iaIP));
        if(SetSecondIp(szSecondName, &tempIP) == -1)
        {
            return ret;
        }
    }

    ret = ONVIF_GetNetworkInterfaces(pDeviceInfo, AllInterface);

    if(nIndex >= 0)
    {
        DelSecondIp(szSecondName);
    }

    return ret;
}




int ONVIF_SetMotionDetectionSensitivity(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__ModifyAnalyticsModules *pReq = 0;
    struct _tan__ModifyAnalyticsModulesResponse *pRsp = 0;
    struct _tt__ItemList_SimpleItem *pSimpleItem;
    struct _tt__ItemList_ElementItem *pElementItem;


    char  *pEndPointStr;
    int    rsl;

    if(0==pDevInfo || 0==pToken || 0==pAnalyticsInfo) return -1;
    if(0==pDevInfo->ip[0] || 0==pDevInfo->port) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;

        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);
        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__ModifyAnalyticsModules);
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);

        /*AnalyticsModule*/
        pReq->__sizeAnalyticsModule = 1;
        SOAP_MALLOC_STRUCT_NUM(pReq->AnalyticsModule, tt__Config, pReq->__sizeAnalyticsModule);
        /*struct tt__ItemList *Parameters;*/
        SOAP_MALLOC_STRUCT(pReq->AnalyticsModule->Parameters,tt__ItemList);
        /*struct _tt__ItemList_SimpleItem *SimpleItem;*/
        pReq->AnalyticsModule->Parameters->__sizeSimpleItem = 1;
        SOAP_MALLOC_STRUCT_NUM(pReq->AnalyticsModule->Parameters->SimpleItem, _tt__ItemList_SimpleItem, pReq->AnalyticsModule->Parameters->__sizeSimpleItem);
        /*struct _tt__ItemList_ElementItem *ElementItem;*/
        pReq->AnalyticsModule->Parameters->__sizeElementItem = 1;
        SOAP_MALLOC_STRUCT_NUM(pReq->AnalyticsModule->Parameters->ElementItem,_tt__ItemList_ElementItem,pReq->AnalyticsModule->Parameters->__sizeElementItem);

        /*************************    init value    **********************************/
        SOAP_CLONE_STR(pReq->AnalyticsModule->Name,pAnalyticsInfo->name);
        pReq->AnalyticsModule->Type = "tt:CellMotionEngine";

        pSimpleItem = pReq->AnalyticsModule->Parameters->SimpleItem;
        pSimpleItem->Name = "Sensitivity";
        SOAP_PRINTF_INT(pSimpleItem->Value,pAnalyticsInfo->Sensitivity);

        pElementItem = pReq->AnalyticsModule->Parameters->ElementItem;
        pElementItem->Name = "Layout";
        pElementItem->__any = "<tt:CellLayout Columns=\"22\" Rows=\"18\"><tt:Transformation><tt:Translate x=\"-1.0\" y=\"-1.0\" /><tt:Scale x=\"0.00625\" y=\"0.00834\" /></tt:Transformation></tt:CellLayout>";
        /*************************    end           **********************************/

        /*pRsp init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__ModifyAnalyticsModulesResponse);
        /*call*/
        rsl = soap_call___ns12__ModifyAnalyticsModules(&soapObj,pEndPointStr,0,pReq,pRsp);
        if(0!=rsl) break;

        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);
    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}

int ONVIF_SetMotionDetectionSensitivity_S(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo)
{
    MotionDetectionAnalytics  analyticsInfoTmp;
    int rsl;

    if(0==pDevInfo || 0==pToken || 0==pAnalyticsInfo ) return -1;

    memset(&analyticsInfoTmp,0,sizeof(MotionDetectionAnalytics));

    do{
        rsl = ONVIF_GetMotionDetectionSensitivity(pDevInfo,pToken,&analyticsInfoTmp);
        if(0!=rsl) break;
        printf("ONVIF_GetMotionDetectionSensitivity success; name=%s-%d-%d-%d\n",analyticsInfoTmp.name,analyticsInfoTmp.Sensitivity,analyticsInfoTmp.Column,analyticsInfoTmp.Row);
        analyticsInfoTmp.Sensitivity = pAnalyticsInfo->Sensitivity;
        analyticsInfoTmp.Row = pAnalyticsInfo->Row;
        analyticsInfoTmp.Column = pAnalyticsInfo->Column;

        rsl = ONVIF_SetMotionDetectionSensitivity(pDevInfo,pToken, &analyticsInfoTmp);
        if(0!=rsl) break;
        return 0;
    }while(0);
    return -1;
}



int ONVIF_GetMotionDetectionSensitivity(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__GetAnalyticsModules *pReq = 0;
    struct _tan__GetAnalyticsModulesResponse *pRsp = 0;
    struct  tt__Config  *pModule;
    struct _tt__ItemList_SimpleItem *pSimpleItem;
    struct _tt__ItemList_ElementItem *pElementItem;

    char  *pEndPointStr;
    char  *pFrom;
    int    Sensitivity;
    int    row,column;
    int    rsl,i,j;

    if(0==pDevInfo || 0==pToken || 0==pAnalyticsInfo) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;

        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);

        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__GetAnalyticsModules);
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);
        /*response init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__GetAnalyticsModulesResponse);

        /*call*/
        rsl = soap_call___ns12__GetAnalyticsModules(&soapObj,pEndPointStr, NULL,pReq,pRsp);
        if(0!=rsl) break;

        printf("get analytics modules success!\n");

        /*analysis*/
        if(0==pRsp->AnalyticsModule) break;

        /*找到 type = tt:CellMotionEngine 的 AnalyticsModule*/
        if(0==pRsp->AnalyticsModule->Parameters) break;
        for(i=0;i<pRsp->__sizeAnalyticsModule;i++){
            pModule = i + pRsp->AnalyticsModule;
            if(0==strcasecmp(pModule->Type,"tt:CellMotionEngine")){
                snprintf(pAnalyticsInfo->name,SOAP_STR_LEN,"%s",pModule->Name);

                if(0==pModule->Parameters) break;
                if(0!=pModule->Parameters->SimpleItem){
                    /*找到 simpleItem name = Sensitivity*/
                    for(j=0;j<pModule->Parameters->__sizeSimpleItem;j++){
                        pSimpleItem = j + pModule->Parameters->SimpleItem;
                        if(0==strcasecmp(pSimpleItem->Name,"Sensitivity")){
                            if(1==sscanf(pSimpleItem->Value,"%d",&Sensitivity)){
                                pAnalyticsInfo->Sensitivity = Sensitivity;
                            }
                        }
                    }
                }
                if(0!=pModule->Parameters->ElementItem){
                    /*找到 ElementItem name = Layout*/
                    for(j=0;j<pModule->Parameters->__sizeElementItem;j++){
                        pElementItem = j + pModule->Parameters->ElementItem;
                        if(0==strcasecmp(pElementItem->Name,"Layout")){
                            if(0==pElementItem->__any) break;
                            pFrom = strcasestr(pElementItem->__any,"Columns");
                            if(0!=pFrom){
                                if(1==sscanf(pFrom,"Columns = \"%d",&column)){
                                    pAnalyticsInfo->Column = column;
                                }
                            }
                            pFrom = strcasestr(pElementItem->__any,"Rows");
                            if(0!=pFrom){
                                if(1==sscanf(pFrom,"Rows = \"%d",&row)){
                                    pAnalyticsInfo->Row = row;
                                }
                            }
                        }
                    }
                }
            }
        }
        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);

    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}



/*SOAP_FMAC5 int SOAP_FMAC6 soap_call___ns11__GetRules(struct soap *soap, const char *soap_endpoint, const char *soap_action,
 * struct _tan__GetRules *tan__GetRules,
 * struct _tan__GetRulesResponse *tan__GetRulesResponse)*/


int ONVIF_GetMotionDetectionRule(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule *pRuleInfo)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__GetRules *pReq = 0;
    struct _tan__GetRulesResponse *pRsp = 0;
    struct  tt__Config *pRule=0;
    struct _tt__ItemList_SimpleItem *pSimpleItem=0;
    char  *pEndPointStr = 0;
    int    rsl,i,j;

    if(0==pDevInfo || 0==pToken || 0==pRuleInfo) return -1;
    if(0==pDevInfo->ip[0] || 0==pDevInfo->port) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        /*用户验证*/
        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;
        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);

        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__GetRules);
        /*token*/
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);

        /*pRsp init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__GetRulesResponse);
        /*call*/
        rsl = soap_call___ns11__GetRules(&soapObj,pEndPointStr,0,pReq,pRsp);
        if(0!=rsl) break;

        /*analysis*/
        if(1>pRsp->__sizeRule || 0==pRsp->Rule
           || 0==pRsp->Rule->Parameters
           || 1>pRsp->Rule->Parameters->__sizeSimpleItem || 0==pRsp->Rule->Parameters->SimpleItem)
            break;

        /*寻找 type = tt:CellMotionDetector 的 rule*/
        if(0==pRsp->Rule) break;
        for(i=0;i<pRsp->__sizeRule;i++){
            pRule = i+pRsp->Rule;
            if(0==strcasecmp(pRule->Type,"tt:CellMotionDetector")){
                printf("%s",pRule->Name);
                snprintf(pRuleInfo->name,SOAP_STR_LEN,"%s",pRule->Name);
                for(i=0;i<pRule->Parameters->__sizeSimpleItem;i++){
                    pSimpleItem = pRule->Parameters->SimpleItem+i;
                    if(0==strcmp(pSimpleItem->Name,"AlarmOnDelay")){
                        sscanf(pSimpleItem->Value,"%d",&pRuleInfo->ararmOnDelay);
                    }else if(0==strcmp(pSimpleItem->Name,"AlarmOffDelay")){
                        sscanf(pSimpleItem->Value,"%d",&pRuleInfo->ararmOffDelay);
                    }else if(0==strcmp(pSimpleItem->Name,"ActiveCells")){
                        snprintf(pRuleInfo->activeCell,SOAP_STR_LEN,"%s",pSimpleItem->Value);

//                        int len = sizeof(pRuleInfo->activeCell);
//                        Base64StrToLocalData(pSimpleItem->Value,pRuleInfo->activeCell,len,22,18);
                    }
                }
            }
        }
        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);
    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}

/*SOAP_FMAC5 int SOAP_FMAC6 soap_call___ns11__GetSupportedRules(
 * struct soap *soap, const char *soap_endpoint, const char *soap_action,
 * struct _tan__GetSupportedRules *tan__GetSupportedRules,
 * struct _tan__GetSupportedRulesResponse *tan__GetSupportedRulesResponse)
*/
int ONVIF_GetSupportedRules(Remote_Device_Info *pDevInfo, unsigned char *pToken)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__GetSupportedRules *pReq = 0;
    struct _tan__GetSupportedRulesResponse *pRsp = 0;

    char  *pEndPointStr = 0;
    int    rsl,i;


    if(0==pDevInfo || 0==pToken) return -1;
    if(0==pDevInfo->ip[0] || 0==pDevInfo->port) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        /*用户验证*/
        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;
        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);

        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__GetSupportedRules);
        /*token*/
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);

        /*pRsp init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__GetSupportedRulesResponse);
        /*call*/
        rsl = soap_call___ns11__GetSupportedRules(&soapObj,pEndPointStr,0,pReq,pRsp);
        if(0!=rsl) break;

        /*Analysis*/
        if(0==pRsp->SupportedRules ||
           1>pRsp->SupportedRules->__sizeRuleContentSchemaLocation || 0==pRsp->SupportedRules->RuleContentSchemaLocation)
            break;


        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);

    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}

/*SOAP_FMAC5 int SOAP_FMAC6 soap_call___ns12__GetSupportedAnalyticsModules(
 * struct soap *soap, const char *soap_endpoint, const char *soap_action,
 * struct _tan__GetSupportedAnalyticsModules *tan__GetSupportedAnalyticsModules,
 * struct _tan__GetSupportedAnalyticsModulesResponse *tan__GetSupportedAnalyticsModulesResponse)
*/

int ONVIF_GetSupportedAnalyticsModules(Remote_Device_Info *pDevInfo, unsigned char *pToken)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__GetSupportedAnalyticsModules *pReq = 0;
    struct _tan__GetSupportedAnalyticsModulesResponse *pRsp = 0;

    char  *pEndPointStr = 0;
    int    rsl,i;


    if(0==pDevInfo || 0==pToken) return -1;
    if(0==pDevInfo->ip[0] || 0==pDevInfo->port) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        /*用户验证*/
        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;
        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);

        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__GetSupportedAnalyticsModules);
        /*token*/
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);

        /*pRsp init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__GetSupportedAnalyticsModulesResponse);
        /*call*/
        rsl = soap_call___ns12__GetSupportedAnalyticsModules(&soapObj,pEndPointStr,0,pReq,pRsp);
        if(0!=rsl) break;

        /*Analysis*/
        if(0==pRsp->SupportedAnalyticsModules ||
           1>pRsp->SupportedAnalyticsModules->__sizeAnalyticsModuleContentSchemaLocation
                || 0==pRsp->SupportedAnalyticsModules->AnalyticsModuleContentSchemaLocation)
            break;



        printf("ONVIF_GetSupportedAnalyticsModules success!!!!!!!!\n");

        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);

    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}

//int soap_call___ns11__ModifyRules(struct soap *soap, const char *soap_endpoint, const char *soap_action,
//struct _tan__ModifyRules *tan__ModifyRules,
//struct _tan__ModifyRulesResponse *tan__ModifyRulesResponse)
int ONVIF_SetMotionDetectionRule(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule *pRule)
{
    struct soap  soapObj;
    SOAP_MALLOC_VARIABLE(&soapObj);

    struct _tan__ModifyRules *pReq = 0;
    struct _tan__ModifyRulesResponse *pRsp = 0;
    struct _tt__ItemList_SimpleItem *pSimpleItem;


    char  *pEndPointStr = 0;
    int    rsl;

    if(0==pDevInfo || 0==pToken || 0==pRule) return -1;
    if(0==pDevInfo->ip[0] || 0==pDevInfo->port) return -1;

    do{
        soap_init(&soapObj);
        soap_set_namespaces(&soapObj,namespaces);
        soap_header(&soapObj);

        /*用户验证*/
        rsl = ONVIF_SetAuthenticationInformation(&soapObj,pDevInfo->userName,pDevInfo->password);
        if(0!=rsl) break;
        /*end point init*/
        SOAP_ENDPOINTSTR_GENERATE(pEndPointStr,pDevInfo->ip,pDevInfo->port);

        /*require init*/
        SOAP_MALLOC_STRUCT(pReq,_tan__ModifyRules);
        SOAP_CLONE_STR(pReq->ConfigurationToken,pToken);

        /*struct tt__Config *Rule;*/
        pReq->__sizeRule =1;
        SOAP_MALLOC_STRUCT_NUM(pReq->Rule,tt__Config,pReq->__sizeRule);
        SOAP_CLONE_STR(pReq->Rule->Name,pRule->name);
        pReq->Rule->Type = "tt:CellMotionDetector";
        /*struct tt__ItemList*/
        SOAP_MALLOC_STRUCT(pReq->Rule->Parameters,tt__ItemList);
        /*struct _tt__ItemList_SimpleItem*/
        pReq->Rule->Parameters->__sizeSimpleItem = 3;
        SOAP_MALLOC_STRUCT_NUM(pReq->Rule->Parameters->SimpleItem,_tt__ItemList_SimpleItem,pReq->Rule->Parameters->__sizeSimpleItem);

        pSimpleItem = pReq->Rule->Parameters->SimpleItem;
        /*AlarmOnDelay*/
        pSimpleItem[0].Name = "AlarmOnDelay";
        SOAP_PRINTF_INT(pSimpleItem[0].Value,pRule->ararmOnDelay);
        /*AlarmOffDelay*/
        pSimpleItem[1].Name = "AlarmOffDelay";
        SOAP_PRINTF_INT(pSimpleItem[1].Value,pRule->ararmOffDelay);
        /*ActiveCells*/
        pSimpleItem[2].Name = "ActiveCells";
        SOAP_CLONE_STR(pSimpleItem[2].Value,pRule->activeCell);

//        unsigned char dedata[50] = {0};
//        int len = 50;
//        local2Net(pRule->activeCell,18*4,dedata,&len);
//        SOAP_MALLOC_STR(pSimpleItem[2].Value,128);
//        getcells(dedata,pSimpleItem[2].Value);

        /*response init*/
        SOAP_MALLOC_STRUCT(pRsp,_tan__ModifyRulesResponse);
        /*call*/
        rsl = soap_call___ns11__ModifyRules(&soapObj,pEndPointStr,0,pReq,pRsp);
        if(0!=rsl) break;

        soap_destroy(&soapObj);
        soap_end(&soapObj);
        soap_done(&soapObj);
        return 0;
    }while(0);
    if(0!=soapObj.error){
        fprintf(stderr,"soap error: %s, %s\n", *soap_faultcode(&soapObj), *soap_faultstring(&soapObj));
    }
    soap_destroy(&soapObj);
    soap_end(&soapObj);
    soap_done(&soapObj);
    return -1;
}


int ONVIF_SetMotionDetectionRule_S(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule_S *pRuleSInfo)
{
    MotionDetectionAnalytics   analyticsInfo;
    MotionDetectionRule        ruleInfo;
    int rsl;

    if(0==pDevInfo || 0==pToken || 0==pRuleSInfo) return -1;

    memset(&analyticsInfo,0,sizeof(MotionDetectionAnalytics));
    memset(&ruleInfo,0,sizeof(MotionDetectionRule));


    do{
        rsl = ONVIF_GetMotionDetectionSensitivity(pDevInfo,pToken,&analyticsInfo);
        if(0!=rsl) break;
        printf("ONVIF_GetMotionDetectionSensitivity success; name=%s-%d-%d-%d\n",analyticsInfo.name,analyticsInfo.Sensitivity,analyticsInfo.Column,analyticsInfo.Row);

        rsl = ONVIF_GetMotionDetectionRule(pDevInfo,pToken,&ruleInfo);
        if(0!=rsl) break;
        printf("ONVIF_GetMotionDetectionRule success!\n");
        printf("AlarmOnDelay=%d\n",ruleInfo.ararmOnDelay);
        printf("AlarmOffDelay=%d\n",ruleInfo.ararmOffDelay);

        ruleInfo.ararmOnDelay = pRuleSInfo->ararmOnDelay;
        ruleInfo.ararmOffDelay = pRuleSInfo->ararmOffDelay;
        memset(ruleInfo.activeCell,0,sizeof(ruleInfo.activeCell));
        rsl = LocalDataToBase64Str(pRuleSInfo->activeCell,sizeof(pRuleSInfo->activeCell),
                             ruleInfo.activeCell,sizeof(ruleInfo.activeCell),
                             analyticsInfo.Column,analyticsInfo.Row);
        if(0!=rsl) break;

        rsl = ONVIF_SetMotionDetectionRule(pDevInfo,pToken,&ruleInfo);
        if(0!=rsl) break;
        return 0;
    }while(0);
    return -1;
}


/********************************************************************************************/
/*******************************  base64,pack        ***************************************/
/********************************************************************************************/
  const char base64i[81] = "\76XXX\77\64\65\66\67\70\71\72\73\74\75XXXXXXX\00\01\02\03\04\05\06\07\10\11\12\13\14\15\16\17\20\21\22\23\24\25\26\27\30\31XXXXXX\32\33\34\35\36\37\40\41\42\43\44\45\46\47\50\51\52\53\54\55\56\57\60\61\62\63";
  const char base64o[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  #define blank(c)		((c)+1 > 0 && (c) <= 32)

  int net2Local(char* InData, int InLen, char* OutData, int* OutLen)
  {
      int i = 0;
      int j = 0;
      int line_index = 0;
      int index = 0;
      unsigned long* IpcBlock = (unsigned long *)OutData;

      IpcBlock[index] = 0;
      for(i = 0; i < InLen * 8; i++)
      {
          if(line_index >= 22)
          {
              //printf("\n");
              index++;
              IpcBlock[index] = 0;
              line_index = 0;
          }

          unsigned long temp = ((InData[i / 8] & (1 << (7 - (i % 8)))) >> (7 - (i % 8)));
          //printf("temp = 0x%08X\n", temp);
          IpcBlock[index] = IpcBlock[index] + (temp << line_index);

  #if 0
          printf("i = %02d InData[i / 8] = 0x%02X IpcBlock[index] = 0x%08X\n", \
              i, (unsigned char)InData[i / 8], IpcBlock[index]);
  #endif

          line_index++;

      }

      *OutLen = index;

      return index;
  }

  int local2Net(char* InData, int InLen, char* OutData, int* OutLen)
  {
      int i = 0;
      int j = 0;
      int index = 0;
      unsigned long* IpcBlock = (unsigned long *)InData;
      unsigned long TrueBlock[18][22];
      unsigned long pTrueBlock[400];
      unsigned long NvrBlock[50];

      memset(TrueBlock, 0, sizeof(unsigned long) * 18 * 22);
      memset(pTrueBlock, 0, sizeof(unsigned long) * 400);
      memset(NvrBlock, 0, sizeof(unsigned long) * 50);

      for(i = 0; i < InLen/sizeof(unsigned long); i++)
      {
          IpcBlock[i] = IpcBlock[i] & 0x003FFFFF;
      }

      for(i = 0; i < 18; i++)
      {
          for(j = 0; j < 22; j++)
          {
              TrueBlock[i][j] = (IpcBlock[i] & (1 << (j))) >> (j) ;
          }
      }

      memcpy(pTrueBlock, TrueBlock, sizeof(unsigned long) * 18 * 22);

      memset(OutData, 0, sizeof(char) * 50);

      for(i = 0; i < 400; i += 8)
      {
          if(i < 22 * 18)
          {
              NvrBlock[index] = (pTrueBlock[i] << 7) + (pTrueBlock[i + 1] << 6) + \
                              (pTrueBlock[i + 2] << 5) + (pTrueBlock[i + 3] << 4) + \
                              (pTrueBlock[i + 4] << 3) + (pTrueBlock[i + 5] << 2) + \
                              (pTrueBlock[i + 6] << 1) + (pTrueBlock[i + 7]);
              index++;
          }

      }

      for(i = 0; i < 50; i++)
      {
          OutData[i] = (char)NvrBlock[i];
      }

      *OutLen = index;

      return index;
  }

  unsigned int unpackbits(unsigned char *outp, unsigned char *inp,
              unsigned int outlen, unsigned int inlen)
  {
      unsigned int i, len;
      int val;

      /* i counts output bytes; outlen = expected output size */
      for(i = 0; inlen > 1 && i < outlen;){
          /* get flag byte */
          len = *inp++;
          --inlen;

          if(len == 128) /* ignore this flag value */
              ; // warn_msg("RLE flag byte=128 ignored");
          else{
              if(len > 128){
                  len = 1+256-len;

                  /* get value to repeat */
                  val = *inp++;
                  --inlen;

                  if((i+len) <= outlen)
                      memset(outp, val, len);
                  else{
                      memset(outp, val, outlen-i); // fill enough to complete row
                      printf("unpacked RLE data would overflow row (run)\n");
                      len = 0; // effectively ignore this run, probably corrupt flag byte
                  }
              }else{
                  ++len;
                  if((i+len) <= outlen){
                      if(len > inlen)
                          break; // abort - ran out of input data
                      /* copy verbatim run */
                      memcpy(outp, inp, len);
                      inp += len;
                      inlen -= len;
                  }else{
                      memcpy(outp, inp, outlen-i); // copy enough to complete row
                      printf("unpacked RLE data would overflow row (copy)\n");
                      len = 0; // effectively ignore
                  }
              }
              outp += len;
              i += len;
          }
      }
      if(i < outlen)
          printf("not enough RLE data for row\n");
      return i;
  }

  unsigned int packbits(unsigned char *src, unsigned char *dst, unsigned int n){
      unsigned char *p, *q, *run, *dataend;
      int count, maxrun;

      dataend = src + n;
      for( run = src, q = dst; n > 0; run = p, n -= count ){
          // A run cannot be longer than 128 bytes.
          maxrun = n < 128 ? n : 128;
          if(run <= (dataend-3) && run[1] == run[0] && run[2] == run[0]){
              // 'run' points to at least three duplicated values.
              // Step forward until run length limit, end of input,
              // or a non matching byte:
              for( p = run+3; p < (run+maxrun) && *p == run[0]; )
                  ++p;
              count = p - run;

              // replace this run in output with two bytes:
              *q++ = 1+256-count; /* flag byte, which encodes count (129..254) */

              *q++ = run[0];      /* byte value that is duplicated */

          }else{
              // If the input doesn't begin with at least 3 duplicated values,
              // then copy the input block, up to the run length limit,
              // end of input, or until we see three duplicated values:
              for( p = run; p < (run+maxrun); )
                  if(p <= (dataend-3) && p[1] == p[0] && p[2] == p[0])
                      break; // 3 bytes repeated end verbatim run
                  else
                      ++p;
              count = p - run;
              *q++ = count-1;        /* flag byte, which encodes count (0..127) */
              memcpy(q, run, count); /* followed by the bytes in the run */
              q += count;
          }
      }
      return q - dst;
  }

  int s2base64(const unsigned char *s, char *t, int n)
  {
      int i;
      unsigned long m;
      char *p;
  #if 0
      if (!t)
      t = (char*)soap_malloc(soap, (n + 2) / 3 * 4 + 1);
      if (!t)
      return NULL;
  #endif
      p = t;
      t[0] = '\0';
      if (!s)
          return 0;
      for (; n > 2; n -= 3, s += 3)
      {
          m = s[0];
          m = (m << 8) | s[1];
          m = (m << 8) | s[2];
          for (i = 4; i > 0; m >>= 6)
          t[--i] = base64o[m & 0x3F];
          t += 4;
      }
      t[0] = '\0';
      if (n > 0) /* 0 < n <= 2 implies that t[0..4] is allocated (base64 scaling formula) */
      {
          m = 0;
          for (i = 0; i < n; i++)
              m = (m << 8) | *s++;
          for (; i < 3; i++)
              m <<= 8;
          for (i = 4; i > 0; m >>= 6)
              t[--i] = base64o[m & 0x3F];
          for (i = 3; i > n; i--)
              t[i] = '=';
          t[4] = '\0';
      }
      return ((n + 2) / 3 * 4 + 1);
  }


  char * base642s(const char *s, char *t, size_t l, int *n)
  {
      size_t i, j;
      char c;
      unsigned long m;
      char *p;
      if (!s || !*s)
      {
          if (n)
          *n = 0;

          return "\0\0\0";
      }

      if (!t)
      {
          l = (strlen(s) + 3) / 4 * 3 + 1;	/* make sure enough space for \0 */
      }

      if (!t)
          return NULL;

      p = t;

      if (n)
          *n = 0;

      for (i = 0; ; i += 3, l -= 3)
      {
          m = 0;
          j = 0;

          while (j < 4)
          {
              c = *s++;
              if (c == '=' || !c)
              {
                  if (l >= j - 1)
                  {
                      switch (j)
                      {
                          case 2:
                              *t++ = (char)((m >> 4) & 0xFF);
                              i++;
                              l--;
                              break;
                          case 3:
                              *t++ = (char)((m >> 10) & 0xFF);
                              *t++ = (char)((m >> 2) & 0xFF);
                              i += 2;
                              l -= 2;
                      }
                  }

                  if (n)
                      *n = (int)i;

                  if (l)
                      *t = '\0';

                  return p;
              }

              c -= '+';
              if (c >= 0 && c <= 79)
              {
                  int b = base64i[c];
  #if 0
                  if (b >= 64)
                  { soap->error = SOAP_TYPE;
                  return NULL;
                  }
  #endif
                  m = (m << 6) + b;
                  j++;
              }
              else if (!blank(c + '+'))
              {
              return NULL;
              }
          }

          if (l < 3)
          {
              if (n)
                  *n = (int)i;
              if (l)
                  *t = '\0';
              return p;
          }
          *t++ = (char)((m >> 16) & 0xFF);
          *t++ = (char)((m >> 8) & 0xFF);
          *t++ = (char)(m & 0xFF);
      }
  }

  int getdecells(char base64str[128],unsigned char dedata[50])
  {
      int ret = 0;
      int i = 0;
      char endata[2048] = {0};

      if(!base64str)
          return SOAP_ERR;

      base642s(base64str, endata, strlen(base64str), &ret);//解码


      int outlen = 22 * 18 / 8 + (((22 * 18) % 8) ? 1 : 0);

      unpackbits(dedata, endata,  outlen, ret);


      return SOAP_OK;
  }



  int getcells(unsigned char ucBlock[50],char base64[128])
  {
      int i;
      int ret;
      char str[2048]={0};
      char dst[2048]={0};
      //unsigned char dst[18]={0};

      strcpy(str ,(char *)ucBlock );

      packbits(ucBlock,dst,50);

      int dstlen = 22 * 18 /8 + (((22 * 18) % 8) ? 1 : 0);

      for(i = 0; i<dstlen; i++)
      {
          if(i % 16 == 0)
          {
              //printf("\n");
          }

      //	LogTrace("packbits=0x%02X \n", (unsigned char)dst[i]);
      }

      s2base64(dst, base64, dstlen);

      return SOAP_OK;
  }

#define BASE64_BUFF_LEN      2 * 1024
int Base64StrToLocalData(char *pStr, char* pData, int *pDataLen, int w, int h)
{
  char   pBuff1[BASE64_BUFF_LEN] = {0};
  char   pBuff2[BASE64_BUFF_LEN] = {0};
  int    len,outLen;

  w = 22;
  h = 18;

  if(0==pStr || 0==pData || 0==pDataLen) return -1;

  do{
      len = BASE64_BUFF_LEN;
      base642s(pStr, pBuff1, strlen(pStr), &len);

      outLen = w * h / 8 + (((w * h) % 8) ? 1 : 0);
      unpackbits(pBuff2,pBuff1,outLen,BASE64_BUFF_LEN);

      net2Local(pBuff2,50,pData,&pDataLen);
      return 0;
  }while(0);
  return -1;
}

int LocalDataToBase64Str(char* pData, int dataLen, char *pStr, int *pStrLen, int w, int h)
{
    char   pBuff1[BASE64_BUFF_LEN] = {0};
    char   pBuff2[BASE64_BUFF_LEN] = {0};
    int    len,outLen;
    int    i;
    int    c,b;

    if(0==pData || 0==pStr || 0==pStrLen) return -1;

    do{
//        for(i=0;i<h;i++){
//            c = (w*i)/8;
//            b = w*i%8;
//            ((int*)(pBuff1))[i] |= (((int*)(pData))[i]>>b) & (0xffffffff << 32-w-b);
//        }

        outLen = BASE64_BUFF_LEN;
        local2Net(pData,dataLen,pBuff1,&outLen);

        outLen = w * h / 8 + (((w * h) % 8) ? 1 : 0);
        packbits(pBuff1,pBuff2,outLen);

        outLen = w * h / 8 + (((w * h) % 8) ? 1 : 0);
        s2base64(pBuff2, pStr, outLen);

        return 0;
    }while(0);
    return -1;
}





