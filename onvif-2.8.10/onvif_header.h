#ifndef _ONVIF_HEADER_H
#define _ONVIF_HEADER_H


#define MAX_PROF_TOKEN 64
#define MAX_STRING_LENGTH 128
#define MAX_CAMERA  32

#define MAX_URL_LEN 128

#define EACH_CAMERA_MAX_PROFILES  10

#define MAX_ONVIF_VERSION_SUPPORT 6

#define MAX_RESOLUTIONS_AVAILABLE 5

#define MAX_H264_PROFILES 6

#define MAX_IP_LEN 64


typedef struct
{
    char                                   Vname[MAX_PROF_TOKEN];               //video source configuration name
    int                                    Vcount;                              // count for number of users
    char                                   Vtoken[MAX_PROF_TOKEN];             //video source configuration token
    char                                   Vsourcetoken[MAX_PROF_TOKEN];
    int                                    windowx;                                //x cordinate of window
    int                                    windowy;             //y cordinate of window
    int                                    windowwidth;          //width
    int                                    windowheight;        //height
}Add_VideoSource_configuration;

typedef struct
{
    char                                   Aname[MAX_PROF_TOKEN];                 //audio name
    int                                    Ausecount;                //audio user count
    char                                   Atoken[MAX_PROF_TOKEN];                //audio token
    char                                   Asourcetoken[MAX_PROF_TOKEN];          //audio source token
}Add_AudioSource_Configuration;

typedef struct
{
    char                                   VEname[MAX_PROF_TOKEN];                 //video encoder name
    char                                   VEtoken[MAX_PROF_TOKEN];                 //video encoder token
    int                                    VEusercount;            //video encoder user count
    float                                  VEquality;              //video encoder quality
    int                                    Vencoder;             //video encoder for jpeg, mpeg, h.264
    int                                    Rwidth;               //resolution width
    int                                    Rheight;                //resolution height
    int                                    frameratelimit;        //frameratelimit
    int                                    encodinginterval;       //encodinginterval
    int                                    bitratelimit;            //bitratelimit
    int                                    Mpeggovlength;              //mpeg GoVLength
    int                                    Mpegprofile;                 //mpegprofile SP/ASP
    int                                    H264govlength;               //H264 govlength
    int                                    H264profile;                 //H264 profile  baseline/main/extended/high
    long                                   sessiontimeout;


}Add_VideoEncoder_configuration;

typedef struct
{
    char                       AEname[MAX_PROF_TOKEN];                     //audio encoder name
    char                       AEtoken[MAX_PROF_TOKEN];                   //audio encoder token
    int                        AEusercount;               //audio encoder user count
    int                        bitrate;                    //Bitrate
    int                        samplerate;                //Samplerate
    int 				       AEencoding;

}Add_AudioEncoder_Configuration;

/* Onvif - Profile Structure*/
typedef struct
{

    char                                profilename[MAX_PROF_TOKEN];       //name of profile
    char                                profiletoken[MAX_PROF_TOKEN];      //token of profile
    int                                 fixed;             //profile fixed or not
    Add_VideoSource_configuration 		AVSC;

    Add_VideoEncoder_configuration 		AESC;

    float                               PTZspeedx;                                    //pan tilt speed
    float                               PTZspeedy;                                    // pan tilt speed
    char                                PTZtoken[MAX_PROF_TOKEN];                 //ptz token
    float                               PTZzoom;                                      //ptz zoom
      char                                PTZname[MAX_PROF_TOKEN];                  //ptz name
#if 0
     Add_AudioEncoder_Configuration 		AAEC;
     Add_AudioSource_Configuration 		AASC;
    /* PTZ configuration */


    int                                 PTZusercount;             //ptz user count
    char                                PTZnodetoken[MAX_PROF_TOKEN];             //ptz nade token
    char                                DefaultAbsolutePantTiltPositionSpace[MAX_STRING_LENGTH];         //default absolute pant tilt position space
    char                                DefaultAbsoluteZoomPositionSpace[MAX_STRING_LENGTH];             //default absolute zoom position space
    char                                DefaultRelativePanTiltTranslationSpace[MAX_STRING_LENGTH];       //default relative pan tilt translation space
    char                                DefaultRelativeZoomTranslationSpace[MAX_STRING_LENGTH];          //default relative zoom translation space
    char                                DefaultContinuousPanTiltVelocitySpace[MAX_STRING_LENGTH];         //Default Continuous PanTilt Velocity Space
    char                                DefaultContinuousZoomVelocitySpace[MAX_STRING_LENGTH];            //Default Continuous Zoom Velocity Space

    char                                PTZspeedspace[MAX_STRING_LENGTH];                                // pan tilt speed

    char                                PTZzoomspce[MAX_STRING_LENGTH];                                  //ptz zoom space
    char                                DefaultPTZTimeout[MAX_PROF_TOKEN];                            //default time out for ptz
    char                                PANURI[MAX_STRING_LENGTH];                                        //pan tilt limit uri
    float                               PTZrangeminx;                                     //ptz min x  range
    float                               PTZrangemaxx;                                     //ptz max x range
    float                               PTZrangeminy;                                     //ptz min y range
    float                               PTZrangemaxy;                                      //ptz max y range
    char                                PTZzoomURI[MAX_STRING_LENGTH];                                        //zoom uri
    char                                PTZMname[MAX_STRING_LENGTH];                                           //meta data name
    char                                PTZMtoken[MAX_STRING_LENGTH];                                         //meta data token
    int                                 PTZMusercount;                                      //meta data user count
    int                                 PTZfilterstatus;                                    //ptz filter status
    int                                 PTZfilterposition;                                  //ptz filter position
    int                                 PTZManalytics;                                      //analytics
#endif
    /* Extension */
    char                                AExname[MAX_PROF_TOKEN];                           //extension audio output configuration name
    int                                 AExusecount;                       //extension audio output configuration user count
    char                                AExtoken[MAX_PROF_TOKEN];                          //extension audio output configuration token
    char                                AExoutputtoken[MAX_PROF_TOKEN];                    //extension audio output configuration otput token
    char                                AExsendprimacy[MAX_STRING_LENGTH];                    //extension audio output configuration primacy
    int                                 AExoutputlevel;                      //extension audio output configuration level
    char                                AExDname[MAX_STRING_LENGTH];                            //audio decoder name
    int                                 AExDusecount;                       //audio decoder user count
    char                                AExDtoken[MAX_STRING_LENGTH];                         //audio decoder token

    char							    StreamUrl[MAX_STRING_LENGTH];
    int									streamProtocol;   // udp, rtsp rtp over tcp
    char                                VAtoken[MAX_PROF_TOKEN];

}Onvif_profile;


typedef struct _TtAnalyticsCapabilities_
{
    char XAddr[MAX_URL_LEN];
    int  RuleSupport;
    int  AnalyticsModuleSupport;
    char token[MAX_PROF_TOKEN];

}AnalyticsCapabilities; 

typedef struct _onvifVersion_
{
    int major;
    int minor;
}OnvifVersion;


typedef struct _TtDeviceCapabilities_
{
    char XAddr[MAX_URL_LEN];
    int sizeOfVersion;
    OnvifVersion onvifVersion[MAX_ONVIF_VERSION_SUPPORT];

    int InputConnectors;
    int RelayOutputs;

}DeviceCapabilities;

typedef struct _TtEventCapabilities_
{
    char XAddr[MAX_URL_LEN];
    int WSSubscriptionPolicySupport;
    int WSPullPointSupport;
    int WSPausableSubscriptionManagerInterfaceSupport;

}EventCapabilities;

typedef struct _TtImagingCapabilities_
{
    char XAddr[MAX_URL_LEN];

}ImagingCapabilities;


typedef struct _TtRealTimeStreamingCapabilities_
{
    int RTPMulticast;
    int RTP_USCORETCP;
    int RTP_USCORERTSP_USCORETCP;

}RealTimeStreamingCapabilities;

typedef struct _TtMediaCapabilities_
{

    char XAddr[MAX_URL_LEN];
    RealTimeStreamingCapabilities StreamingCapabilities;


}MediaCapabilities;

typedef struct _TtPTZCapabilities_
{
    char XAddr[MAX_URL_LEN];

}PTZCapabilities;

typedef struct _TtDeviceIOCapabilities_
{
    char XAddr[MAX_URL_LEN];	/* required element of type xsd:anyURI */
    int VideoSources;	/* required element of type xsd:int */
    int VideoOutputs;	/* required element of type xsd:int */
    int AudioSources;	/* required element of type xsd:int */
    int AudioOutputs;	/* required element of type xsd:int */
    int RelayOutputs;	/* required element of type xsd:int */
}DeviceIOCapabilities;

typedef struct _TtCapabilitiesExtension_
{
    DeviceIOCapabilities  ExtDevIOCapabilities;

}CapabilitiesExtension;

typedef struct _TdsCapabilities_
{
    int hasAnalyticsCapabilities;
    AnalyticsCapabilities analyticsCapabilities;

    int hasDeviceCapabilities;
    DeviceCapabilities    deviceCapabilities;

    int hasEventCapabilities;
    EventCapabilities	  eventCapabilities;

    int hasImagingCapabilities;
    ImagingCapabilities   imagingCapabilities;

    int hasMediaCapabilities;
    MediaCapabilities	  mediaCapabilities;

    int hasPTZCapabilities;
    PTZCapabilities		  pTZCapabilities;

    int hasCapabilitiesExtension;
    CapabilitiesExtension capabilitiesExtension;

}TdsCapabilities;


typedef struct _TtIntRange_
{
    int Min;
    int Max;
}TtIntRange;

typedef struct _TtFloatRange_
{
    float Min;
    float Max;
}TtFloatRange;

typedef struct _TtVideoResolution_
{
    int Width;
    int Height;
}TtVideoResolution;

typedef struct _TtJpegOptions_
{
    int __sizeResolutionsAvailable;
    TtVideoResolution ResolutionsAvailable[MAX_RESOLUTIONS_AVAILABLE];
    TtIntRange FrameRateRange;
    TtIntRange EncodingIntervalRange;
}TtJpegOptions;


typedef struct _TtH264Options_
{
    int __sizeResolutionsAvailable;

    TtVideoResolution ResolutionsAvailable[MAX_RESOLUTIONS_AVAILABLE];
    TtIntRange GovLengthRange;
    TtIntRange FrameRateRange;
    TtIntRange EncodingIntervalRange;
    TtIntRange BitrateRange;
    int __sizeH264ProfilesSupported;
    int H264ProfilesSupported[MAX_H264_PROFILES];

}TtH264Options;

typedef struct _TtMpeg4Options_
{
    int __sizeResolutionsAvailable;
    TtVideoResolution ResolutionsAvailable[MAX_RESOLUTIONS_AVAILABLE];
    TtIntRange GovLengthRange;
    TtIntRange FrameRateRange;
    TtIntRange EncodingIntervalRange;
    int __sizeMpeg4ProfilesSupported;
    int Mpeg4ProfilesSupported[MAX_H264_PROFILES];

}TtMpeg4Options;


typedef struct _VideoEncoderConfigurationOptions_
{
    TtIntRange QualityRange;

    int hasJPEG;
    TtJpegOptions JpegOptions;

    int hasMpeg4;
    TtMpeg4Options Mpeg4Options;

    int hasH264;
    TtH264Options H264Options;
}VideoEncoderConfigurationOptions;

typedef struct _Mpeg4Conf_
{
    int GovLength;
    int Mpeg4Profile;

}Mpeg4Conf;

typedef struct _H264Conf_
{
    int GovLength;
    int H264Profile;

}H264Conf;

typedef struct 	_VideoEncoderConfiguration_
{
    char Name[MAX_PROF_TOKEN];
    char token[MAX_PROF_TOKEN];

    int videoEncoding;
    TtVideoResolution  VideoResolution;

    float Quality;
    int FrameRateLimit;
    int EncodingInterval;
    int BitrateLimit;
    int EncodeType;                  // 0 jpeg   1 mpeg   2 H264
    H264Conf h264;
    Mpeg4Conf mpeg4;

}VideoEncoderConfiguration;


typedef struct _ONVIF_DEVINFO_
{
   bool Status;
   char Manufacturer[MAX_STRING_LEN];
   char Model[MAX_STRING_LEN];
   char FirmwareVersion[MAX_STRING_LEN];
   char SerialNumber[MAX_STRING_LEN];
   char HardwareId[MAX_STRING_LEN];

}Onvif_DevInfo;



typedef struct _ONVIF_INFO_
{

    Onvif_DevInfo onvif_deviceinfo;
    TdsCapabilities tdsCapabilities;

    VideoEncoderConfigurationOptions videoEncoderConfigurationOptions;

    VideoEncoderConfiguration videoEncoderConfiguration;


    int profileSize;
    Onvif_profile Profiles[EACH_CAMERA_MAX_PROFILES];

}ONVIF_INFO;

typedef struct _CameraIpInfo_
{
    char ip[MAX_IP_LEN];
    unsigned int port;
    char User[MAX_IP_LEN];
    char Password[MAX_IP_LEN];
}CameraIpInfo;

struct in_addr;

typedef struct _ONVIF_EVENT_ITEM_
{
    char Xaddr[MAX_STRING_LENGTH];
    char subscribeUrl[MAX_STRING_LENGTH];
    time_t startTime;
    time_t currentTime;
    time_t endTime;
}Onvif_Event_Item;


typedef struct _ONVIF_CAMERAS_
{	
    CameraIpInfo  cameraIpInfo[MAX_CAMERA];
    ONVIF_INFO   onvif_info[MAX_CAMERA];
    Onvif_Event_Item CameraEvents[MAX_CAMERA];
    int init;

}ONVIF_CAMERAS;


#if 0

typedef struct _ONVIF_EVENT_MANAGER_
{
    CameraIpInfo cameraIpInfo[MAX_CAMERA];
    Onvif_Event_Item CameraEvents[MAX_CAMERA];

}Onvif_Event_Manager;


#endif



#endif
