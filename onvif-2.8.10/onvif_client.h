#ifndef ONVIF_CLIENT
#define ONVFI_CLIENT


#ifdef __cplusplus
extern "C"{
#endif /* __cplusplus */

#include <stdbool.h>
#define IP_LEN 32
#define MAX_LEN 64
#define MAX_PROFILES 20
#define MAX_STRING_LEN 128
#define MAX_RESOLUTIONS_ITEMS 10

#define MAX_H264_PROFILES 6 // 0 baseline ...

#define MAX_PTZ_PRESETS 128

#define MAX_NETWORK_INTERFACE 2

enum PTZ_CONTROL_CODE
{
    PTZ_STOP = 0,               // 0
    PTZ_UP,		              // 1
    PTZ_DOWN,		              // 2
    PTZ_LEFT,		              // 3
    PTZ_RIGHT,		              // 4
    PTZ_LEFT_UP,                // 5
    PTZ_LEFT_DOWN,              // 6
    PTZ_RIGHT_UP,               // 7
    PTZ_RIGHT_DOWN,             // 8
    PTZ_ZOOM_IN,                // 9
    PTZ_ZOOM_OUT,               // 10
};


typedef struct _ONVIF_Mpeg4Config_
{
    int GovLength;
    int Mpeg4Profile;

}Onvif_Mpeg4_Config;






typedef struct _ONVIF_H264Config_
{
    int GovLength;
    int H264Profile;

}Onvif_H264_Config;

typedef struct 	_ONVIF_VideoConfig_
{
    char Name[MAX_LEN];              //
    char token[MAX_LEN];             //

    int videoEncoding;               // 0 JPEG, 1 MPEG4, 2 H264
    int width;                        //
    int height;                      //

    float Quality;                  //

    int FrameRateLimit;	          //
    int EncodingInterval;	      // I ?
    int BitrateLimit;	          //

    Onvif_H264_Config h264;       // videoEncoding  H264
    Onvif_Mpeg4_Config mpeg4;	  // videoEncoding  MPEG4

}Onvif_Video_Config;



typedef struct _Onvif_VideoAnalytics_Config_
{
   char token[MAX_LEN];
}Onvif_VideoAnalytics_Config;


typedef struct _ONVIF_IntRange_
{
    int Min;
    int Max;
}Onvif_Int_Range;

typedef struct _ONVIF_FloatRange_
{
    float Min;
    float Max;
}Onvif_Float_Range;

typedef struct _ONVIF_Resolution_
{
    int Width;
    int Height;
}Onvif_Resolution;

typedef struct _ONVIF_JpegOptions_
{
    int __sizeResolutionsAvailable;	                              //
    Onvif_Resolution ResolutionsAvailable[MAX_RESOLUTIONS_ITEMS]; //
    Onvif_Int_Range FrameRateRange;	                              //
    Onvif_Int_Range EncodingIntervalRange;
}Onvif_Jpeg_Options;


typedef struct _ONVIF_H264Options_
{
    int __sizeResolutionsAvailable;								  //

    Onvif_Resolution ResolutionsAvailable[MAX_RESOLUTIONS_ITEMS]; //
    Onvif_Int_Range GovLengthRange;
    Onvif_Int_Range FrameRateRange;	                              //
    Onvif_Int_Range EncodingIntervalRange;
    Onvif_Int_Range BitrateRange;
    int __sizeH264ProfilesSupported;	                          //
    int H264ProfilesSupported[MAX_H264_PROFILES];	             // 0 baseline, 1 main, 2 high

}Onvif_H264_Options;

typedef struct _ONVIF_Mpeg4Options_
{
    int __sizeResolutionsAvailable;	                                //
    Onvif_Resolution ResolutionsAvailable[MAX_RESOLUTIONS_ITEMS];	//
    Onvif_Int_Range GovLengthRange;
    Onvif_Int_Range FrameRateRange;	                                //
    Onvif_Int_Range EncodingIntervalRange;
    int __sizeMpeg4ProfilesSupported;
    int Mpeg4ProfilesSupported[MAX_H264_PROFILES];	                // 0

}Onvif_Mpeg4_Options;


typedef struct _ONVIF_VideoEncoderConfigurationOptions_
{
    Onvif_Int_Range QualityRange;                             //

    int hasJPEG;                                              // 0  1
    Onvif_Jpeg_Options JpegOptions;                           //  hasJPEG 1

    int hasMpeg4;	                                          // 0  1
    Onvif_Mpeg4_Options Mpeg4Options;                         //  hasMpeg4 1

    int hasH264;
    Onvif_H264_Options H264Options;		                       // 0  1
}Onvif_VideoEncoderConfigurationOptions;             // hasH264 1














/**************************added  by cq 2014.4.4************/

typedef struct _ONVIF_Space2DDescription_
{
    char  URI[MAX_LEN] ;
    Onvif_Float_Range   XRange  ;
    Onvif_Float_Range    YRange  ;
}ONVIF_Space2DDescription;

typedef struct _ONVIF_Space1DDescription_
{
    char  URI[MAX_LEN] ;
    Onvif_Float_Range   XRange  ;
}ONVIF_Space1DDescription;


typedef struct _ONVIF_PTZSpaces_
{
    int     __sizeAbsolutePanTiltPositionSpace ;
    ONVIF_Space2DDescription      AbsolutePanTiltPositionSpace   ;
    int             __sizeAbsoluteZoomPositionSpace ;
    ONVIF_Space1DDescription  AbsoluteZoomPositionSpace      ;
    int              __sizeRelativePanTiltTranslationSpace ;
    ONVIF_Space2DDescription     RelativePanTiltTranslationSpace ;
    int    __sizeRelativeZoomTranslationSpace ;
    ONVIF_Space1DDescription    RelativeZoomTranslationSpace   ;
    int           __sizeContinuousPanTiltVelocitySpace ;
    ONVIF_Space2DDescription    ContinuousPanTiltVelocitySpace ;
    int   __sizeContinuousZoomVelocitySpace ;
    ONVIF_Space1DDescription       ContinuousZoomVelocitySpace    ;
    int       __sizePanTiltSpeedSpace        ;
    ONVIF_Space1DDescription   PanTiltSpeedSpace    ;
    int   __sizeZoomSpeedSpace   ;
    ONVIF_Space1DDescription   ZoomSpeedSpace    ;

}ONVIF_PTZSpaces;

typedef struct _ONVIF_PTZNode_
{

    char PTZNodetoken[MAX_LEN]  ;
    char  PTZNodeName[MAX_LEN]  ;
    ONVIF_PTZSpaces  SupportedPTZSpaces   ;
    int     MaximumNumberOfPresets      ;
    bool     HomeSupported     ;
    int     __sizeAuxiliaryCommands  ;

}ONVIF_PTZNode;

typedef struct _ONVIF_GetNodesResponse_
{
    int     __sizePTZNode     ;
    ONVIF_PTZNode PTZNode[MAX_PTZ_PRESETS];
}ONVIF_GetNodesResponse;





typedef struct _ONVIF_PTZ_ConfigOptions_
{

    Onvif_Float_Range PanTiltVelocityRangeX;
    Onvif_Float_Range PanTiltVelocityRangeY;
    Onvif_Float_Range PanTiltSpeedRangeX;

    Onvif_Float_Range ZoomVelocityRangeX;
    Onvif_Float_Range ZoomSpeedRangeX;

    Onvif_Int_Range TimeOutRange;  // s

}Onvif_Ptz_ConfigOtions;





typedef struct _ONVIF_PTZ_Config_
{

    char name[MAX_LEN];
    char token[MAX_LEN];
    char NodeToken[MAX_LEN];

    float DefaultPanTiltSpeedX;
    float DefaultPanTiltSpeedY;

    float DefaultZoomSpeedX;

    int timeOut;


}Onvif_Ptz_Config;


typedef struct _ONVIF_PresetItem_
{
    char presetName[MAX_LEN];
    char presetToken[MAX_LEN];
    float PanTilt_x;
    float PanTilt_y;
    float zoom;

} Onvif_presetItem;

typedef struct _ONVIF_PTZ_Presets_
{
    int sizeOfPresets;
    Onvif_presetItem presets[MAX_PTZ_PRESETS];

}Onvif_Ptz_Presets;




typedef struct _ONVIF_PTZ__ContinuousMove_
{
    char ProfileToken[MAX_LEN];	          //     与media profile相关的令牌
    int   isPan;                          //     是否进行转动
    int   Speed;                          //     [-8 , 8]
    int   direction;	                  //     查看PTZ_CONTROL_CODE 枚举变量
    int   isZoom;	                      //     是否进行放大或者缩小
    int   Timeout;	                      //     时限
}ONVIF_PTZ__ContinuousMove;





typedef struct _ONVIF_PROFILE_
{


    char profieName[MAX_LEN];                  //

    char profileToken[MAX_LEN];                //

    Onvif_Video_Config videoConfig;            //

    int hasAudio;
    Onvif_VideoAnalytics_Config videoAnalyticsConfig;

    char videoSourceToken[MAX_LEN];

    Onvif_Ptz_Config ptzConfig;





}Onvif_Profile;




typedef struct _ONVIF_PROFILES_
{

    int sizeOfProfiles;							//
    Onvif_Profile Onvif_Profiles[MAX_PROFILES];

}Onvif_Profiles;


typedef struct _ONVIF_IMG_CONFIG_
{
    float brightness;
    float Contrast;
    float ColorSaturation;
    float Sharpness;

}Onvif_Img_Config;

typedef struct _ONVIF_IMG_OPTIONS_
{
    Onvif_Float_Range brightness;
    Onvif_Float_Range Contrast;
    Onvif_Float_Range ColorSaturation;
    Onvif_Float_Range Sharpness;

}Onvif_Img_Options;

typedef struct _ONVIF_DEV_INFO_
{
    bool Status;
   char Manufacturer[MAX_STRING_LEN];
   char Model[MAX_STRING_LEN];
   char FirmwareVersion[MAX_STRING_LEN];
   char SerialNumber[MAX_STRING_LEN];
   char HardwareId[MAX_STRING_LEN];

}Onvif_DeviceInfo;

// ip
typedef struct _REMOTE_DEVICE_INFO_
{
    char ip[MAX_LEN];
    unsigned int port;

    char userName[MAX_LEN];
    char password[MAX_LEN];

}Remote_Device_Info;

typedef struct _NetworkInterface_
{
    char token[MAX_LEN];
    char InterfaceName[MAX_LEN];
    char HwAddress[MAX_LEN];
    char IpAddress[MAX_LEN];
    char NetMask[MAX_LEN];
    bool DHCP;
    int NetMaskLength;
    bool Enable;

}NetworkInterface;


typedef struct _NetworkInterfaces_
{
    int __sizeNetworkInterfaces;                                //网卡个数
    NetworkInterface  SingleNetworkInterface[MAX_NETWORK_INTERFACE];

}NetworkInterfaces;


typedef struct _TimeAndDate_
{
  int type ;                  //manual  0,   ntp  1
  int Hour;
  int Minute;
  int Second;
  int Year;
  int Month;
  int Day;
  bool isUTC;
  char TZ[MAX_STRING_LEN];
}TimeAndDate;




typedef enum _AlarmType_
{
  NO_ALARM = 0X00,
  MOTION_DETECTION_ALARM,
  COVER_ALARM ,
  OTHER_ALARM,
}AlarmType;

typedef enum _AlarmStatus_
{
  MOTION_DETECTION_START = 0x00,
  MOTION_DETECTION_END

}AlarmStatus;


typedef struct _Point_
{
    int x;                                  // 坐标点x
    int y;                                  // 坐标点y
}Point;

//一块事件敏感的区域
typedef struct _MotionDetectionArea_
{
    Point point1;                          // 左上角的坐标
    Point point2;                          // 右下角的坐标
}MotionDetectionArea;


#define SOAP_STR_LEN    1*1024

#define SOAP_MALLOC_VARIABLE(soapObjPtr)                   \
    struct soap *soap_malloc_pSoapObj = soapObjPtr;        \
    int          soap_malloc_size;

#define SOAP_MALLOC_STRUCT_NUM(dstPtr, stype, num)         \
    soap_malloc_size = (sizeof(struct stype)*(num));       \
    dstPtr = (struct stype*)soap_malloc(soap_malloc_pSoapObj,soap_malloc_size); \
    if(0==dstPtr) break;                                   \
    memset(dstPtr,0,soap_malloc_size);
#define SOAP_MALLOC_STRUCT(dstPtr, stype)  SOAP_MALLOC_STRUCT_NUM(dstPtr,stype,1)

#define SOAP_MALLOC_STR(dstPrt, len)                       \
    dstPrt = (char*)soap_malloc(soap_malloc_pSoapObj,len+1);                    \
    if(0==dstPrt) break;                                   \
    *((char*) (dstPrt)) = '\0';

#define SOAP_CLONE_STR(dstPrt,srcPtr)                      \
    SOAP_MALLOC_STR(dstPrt,strlen(srcPtr))                 \
    sprintf(dstPrt,"%s",srcPtr)

#define SOAP_PRINTF_INT(dstPrt,val)                        \
    SOAP_MALLOC_STR(dstPrt, 20)                            \
    sprintf(dstPrt,"%d",val);


#define SOAP_ENDPOINTSTR_FORMAT "http://%s:%d/onvif/device_service"
#define SOAP_ENDPOINTSTR_GENERATE(strPtr, ip, port)                             \
    SOAP_MALLOC_STR(strPtr,(strlen(SOAP_ENDPOINTSTR_FORMAT)+strlen(ip)+20));    \
    sprintf(strPtr,SOAP_ENDPOINTSTR_FORMAT,ip,port);



/*获取支持的 AnalyticsModules*/
int ONVIF_GetSupportedAnalyticsModules(Remote_Device_Info *pDevInfo, unsigned char *pToken);
/*获取支持的Rules*/
int ONVIF_GetSupportedRules(Remote_Device_Info *pDevInfo, unsigned char *pToken);


/*移动侦测灵敏度*/
typedef struct _MotionDetectionAnalytics_{
    int    Sensitivity;
    int    Row;
    int    Column;
    char   name[SOAP_STR_LEN];
}MotionDetectionAnalytics;

/*获取移动侦测灵敏度*/
int ONVIF_GetMotionDetectionSensitivity(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo);
/*设置移动侦测灵敏度基础*/
int ONVIF_SetMotionDetectionSensitivity(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo);
/*设置移动侦测灵敏度基础*/
int ONVIF_SetMotionDetectionSensitivity_S(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionAnalytics *pAnalyticsInfo);

int Base64StrToLocalData(char *pStr, char* pData, int *pDataLen, int w, int h);

int LocalDataToBase64Str(char* pData, int dataLen, char *pStr, int *pStrLen, int w, int h);

typedef struct _MotionDetectionRule_{
    int              ararmOnDelay;
    int              ararmOffDelay;
    char             activeCell[SOAP_STR_LEN];
    char             name[SOAP_STR_LEN];
}MotionDetectionRule;
/*获取移动侦测区域*/
int ONVIF_GetMotionDetectionRule(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule *pRule);
/*设置移动侦测区域*/
int ONVIF_SetMotionDetectionRule(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule *pRule);

typedef struct _MotionDetectionRule_S{
    int              ararmOnDelay;
    int              ararmOffDelay;
    int              activeCell[32];
}MotionDetectionRule_S;

int ONVIF_SetMotionDetectionRule_S(Remote_Device_Info *pDevInfo, unsigned char *pToken, MotionDetectionRule_S *pRuleSInfo);


int ONVIF_AddRemoteDevice(unsigned int channel ,Remote_Device_Info *pDeviceInfo);
int ONVIF_DeleteRemoteDevice(int  channel ,Remote_Device_Info *pDeviceInfo);

//  -1 1
int ONVIF_GetProfiles(Remote_Device_Info *pDeviceInfo, Onvif_Profiles *pGetProfilesRespone);

int ONVIF_GetProfilesFromCache( Remote_Device_Info *pDeviceInfo, Onvif_Profiles *pGetProfilesRespone);

int ONVIF_GetStreamUri(Remote_Device_Info *pDeviceInfo, const char *pProfileToken, char *pUrlBuf );





/**********************************/
//函数名   ：  ONVIF_GetVideoEncoderConfigurationOptions
//用途    ：   获取系统支持的所有视频编码配置选项
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//            profileToken：与 mediaprofile有关的令牌；
//			  videoEncodertoken：  与视频编码有关的令牌
//
//输出参数  ：  pGetVideoEncoderConfigurationOptionsResponse
//返回值   ：
/**********************************/
int ONVIF_GetVideoEncoderConfigurationOptions(Remote_Device_Info *pDeviceInfo, const char *profileToken,const char * videoEncodertoken , Onvif_VideoEncoderConfigurationOptions *pGetVideoEncoderConfigurationOptionsResponse);
//接口有问题,应加上视频编码的令牌
//int ONVIF_GetVideoEncoderConfigurationOptions(Remote_Device_Info *pDeviceInfo, const char *profileToken,Onvif_VideoEncoderConfigurationOptions *pGetVideoEncoderConfigurationOptionsResponse);
int ONVIF_GetVideoEncoderConfiguration(Remote_Device_Info *pDeviceInfo,  const char* ProfileToekn, const char * videoEncodertoken, Onvif_Video_Config *pGetVideoEncoderConfigurationResponse,Onvif_VideoEncoderConfigurationOptions * pGetVideoEncoderConfigurationOptionsResponse);



int ONVIF_SetVideoEncoderConfiguration(Remote_Device_Info *pDeviceInfo, Onvif_Video_Config *pSetVideoEncoderConfig);

//---------------------------PTZ------------------------------------//
/**********************************/
//函数名   ：  ONVIF_PtzGetNodes
//用途    ：   获取系统支持的所有Nodes
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//
//输出参数  ：  pGetNodesResponse
//返回值   ：
/**********************************/
int ONVIF_PtzGetNodes(Remote_Device_Info *pDeviceInfo, ONVIF_GetNodesResponse *pGetNodesResponse);

/**********************************/
//函数名   ：  ONVIF_PtzSetPreset
//用途    ：   设置ptz的预设位置
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//            profileToken：与 mediaprofile有关的令牌；
//            presetName： 预设位置名；
//		      presetToken ： 预设位置的令牌
//输出参数  ：  无
//返回值   ：
/**********************************/
int ONVIF_PtzSetPreset(Remote_Device_Info *pDeviceInfo, const char * profileToken ,const char *presetName, const char *presetToken );
//int ONVIF_PtzSetPreset(Remote_Device_Info *pDeviceInfo, const char *presetName, const char *presetToken );

/**********************************/
//函数名   ：  ONVIF_PtzGetConfigurationOptions
//用途    ：   获取系统支持的所有ptz配置选项
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//            ConfigurationToken： 与ptz configuration 有关的令牌；
//
//输出参数 ：  pGetPtzConfigOtionsReponse
//返回值   ：
/**********************************/
int ONVIF_PtzGetConfigurationOptions(Remote_Device_Info *pDeviceInfo, const char *ConfigurationToken, Onvif_Ptz_ConfigOtions *pGetPtzConfigOtionsReponse);
int ONVIF_PtzGetConfiguration(Remote_Device_Info *pDeviceInfo, Onvif_Ptz_Config *pGetPtzConfig);


/**********************************/
//函数名   ：  ONVIF_PtzGetPresets
//用途    ：   获取系统支持的所有预设位置信息
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//             profileToken：与 mediaprofile有关的令牌；
//
//输出参数 ：  pGetPtzPresetsResponse
//返回值   ：
/**********************************/
int ONVIF_PtzGetPresets(Remote_Device_Info *pDeviceInfo, const char *profileToken, Onvif_Ptz_Presets *pGetPtzPresetsResponse );

/**********************************/
//函数名   ：  ONVIF_PtzGotoPreset
//用途    ：   回到预先设置的位置
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//             profileToken：与 mediaprofile有关的令牌；
//             presetToken ： 预设位置的令牌
//输出参数 ：
//返回值   ：
/**********************************/
int ONVIF_PtzGotoPreset(Remote_Device_Info *pDeviceInfo, const char *profileToken, const char *presetToken,unsigned int speed);


/**********************************/
//函数名   ：  ONVIF_PtzRemovePreset
//用途    ：   删除预先设置的位置
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//             profileToken：与 mediaprofile有关的令牌；
//             presetToken ： 预设位置的令牌
//输出参数 ：
//返回值   ：
/**********************************/
int ONVIF_PtzRemovePreset( Remote_Device_Info *pDeviceInfo, const char *profileToken, const char *presetToken );

/**********************************/
//函数名   ：  ONVIF_PtzRemovePreset
//用途    ：   ptz连续性移动
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//            pPtzContinuousMove ： 具体信息察看ONVIF_PTZ__ContinuousMove 结构体
//输出参数 ：
//返回值   ：
/**********************************/
int ONVIF_PtzContinueMove(Remote_Device_Info *pDeviceInfo, ONVIF_PTZ__ContinuousMove *pPtzContinuousMove );
/**********************************/
//函数名   ：  ONVIF_PtzStop
//用途    ：   ptz停止移动
//输入参数 ：   pDeviceInfo： Remote_Device_Info 结构体；
//            profileToken：与 mediaprofile有关的令牌；
//输出参数 ：
//返回值   ：
/**********************************/
int ONVIF_PtzStop(Remote_Device_Info *pDeviceInfo, const char *profileToken );


int ONVIF_GetDeviceInformation( Remote_Device_Info *pDeviceInfo );

//--------------------------------IMG
int ONVIF_GetImageOptions(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Options *pGetImgOptionsResponse);
int ONVIF_GetImageConfiguration(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Config *pGetImgConfigurationResponse);
int ONVIF_SetImageConfiguration(Remote_Device_Info *pDeviceInfo, const char* videoSourceToken, Onvif_Img_Config *pSetImgConfiguration);

//-----------------------Event
typedef void (*ONVIF_EventCallback) (char *pRemoteIp, AlarmType pAlarmType, AlarmStatus pAlarmStatus );
int ONVIF_EventSubscribe(Remote_Device_Info *pDeviceInfo);
int ONVIF_EventUnSubscribe(Remote_Device_Info *pDeviceInfo);
int ONVIF_EventRenew(Remote_Device_Info *pDeviceInfo);

int ONVIF_EventServerStart( ONVIF_EventCallback pEventCallback );
int ONVIF_EventServerStop();

int ONVIF_SetTimeOut(unsigned int ms);

int ONVIF_SetDisplayNum(unsigned int num);
typedef void (*Discovery_Callback) (char *pDeviceIp, unsigned short devicePort, char *pDeviceUuid, int nIndex);


int StartDiscovery(Discovery_Callback pCallback, bool bMultiSegment);
int StopDiscovery();

int ONVIF_SetNetworkInterfaces2(int nIndex, Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface);
int ONVIF_GetNetworkInterfaces2(int nIndex, Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface);

int ONVIF_SetSystemTimeAndDate(Remote_Device_Info *pDeviceInfo,TimeAndDate * pTimeandDate);
int ONVIF_GetSystemTimeAndDate(Remote_Device_Info *pDeviceInfo,TimeAndDate * pTimeandDate);
int ONVIF_GetDeviceInformationFromCache(Remote_Device_Info *pDeviceInfo, Onvif_DeviceInfo *pGetDevinfoRespone);
int ONVIF_SetNetworkInterfaces(Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface);
int ONVIF_GetNetworkInterfaces(Remote_Device_Info *pDeviceInfo ,NetworkInterfaces * AllInterface);

int ONVIF_SetDefaultGateway(Remote_Device_Info *pDeviceInfo ,char * gateway);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif
