
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "soapH.h"
#include "soapStub.h"
#include <string.h>
#include "onvif_client.h"
#include "onvif_header.h"

void printfbit(unsigned int val)
{
    int i;
    for(i=0;i<32;i++){
        if(i%8==0) printf(" ");
        printf("%d", ((val&(1<<i))==0)?0:1);

    }
    printf("\n");
}



int main()
{
    Remote_Device_Info           devInfo;
    int  rsl;

    memset(&devInfo,0,sizeof(Remote_Device_Info));
//    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.196.25");
//    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.196.101");
    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.195.18");  /*dahua*/
//    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.192.174");  /*中维*/
//    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.192.212");  /*熊迈*/
//    snprintf(&devInfo.ip,MAX_LEN,"%s","172.18.192.173");  /*天视通*/

    devInfo.port = 80;
    snprintf(&devInfo.userName,MAX_LEN,"%s","admin");
    snprintf(&devInfo.password,MAX_LEN,"%s","admin");

//    const char *pToken = "Maintenance";
//    const char *pToken = "VideoAnalyticsToken";
    const char *pToken = "000";
#if 0
    MotionDetectionAnalytics AnalyticsInfo;
    AnalyticsInfo.Sensitivity = 10;
    AnalyticsInfo.Column = 22;
    AnalyticsInfo.Row = 18;
    ONVIF_SetMotionDetectionSensitivity_S(&devInfo,pToken,&AnalyticsInfo);

#elif 0
    MotionDetectionAnalytics   AnalyticsInfo;
    rsl = ONVIF_GetMotionDetectionSensitivity(&devInfo,pToken,&AnalyticsInfo);
    if(0==rsl){
        printf("ONVIF_GetMotionDetectionSensitivity success; name=%s-%d-%d-%d\n",AnalyticsInfo.name,AnalyticsInfo.Sensitivity,AnalyticsInfo.Column,AnalyticsInfo.Row);
    }else{
        printf("ONVIF_GetMotionDetectionSensitivity failed\n");
    }

    AnalyticsInfo.Sensitivity = 32;
    rsl = ONVIF_SetMotionDetectionSensitivity(&devInfo, pToken, &AnalyticsInfo);
    if(0==rsl){
        printf("ONVIF_SetMotionDetectionSensitivity success!\n");
    }else{
        printf("ONVIF_SetMotionDetectionSensitivity failed!\n");
    }

#elif 0
    MotionDetectionRule ruleInfo;
    rsl = ONVIF_GetMotionDetectionRule(&devInfo,pToken,&ruleInfo);
    if(0==rsl){
        printf("ONVIF_GetMotionDetectionRule success!\n");
        printf("AlarmOnDelay=%d\n",ruleInfo.ararmOnDelay);
        printf("AlarmOffDelay=%d\n",ruleInfo.ararmOffDelay);
    }
    else printf("ONVIF_GetMotionDetectionRule failed!\n");


    unsigned int cell[18]={0};
    int cellLen = sizeof(cell);
    Base64StrToLocalData(ruleInfo.activeCell,cell,&cellLen,4,4);
    int h;
    for(h=0;h<18;h++){
        printf("%2d-%8x\n",h,cell[h]);
    }

    ruleInfo.ararmOffDelay = 51;
    ruleInfo.ararmOnDelay = 51;

    memset(cell,0xff,sizeof(cell));
    cellLen = sizeof(ruleInfo.activeCell);
    LocalDataToBase64Str(cell,sizeof(cell),ruleInfo.activeCell,&cellLen,22,18);
    printf("activeCell str = %s\n",ruleInfo.activeCell);

    memset(cell,0x00,sizeof(cell));
    cell[0] = 0xffffffff;
//    cell[1] = 0xffffffff;
//    cell[2] = 0xffffffff;
    cell[17] = 0xffffffff;

//    unsigned char dedata[50] = {0};
//    int len = 50;
//    local2Net(cell,18*4,dedata,&len);
//    getcells(dedata,ruleInfo.activeCell);

    int strLen = sizeof(ruleInfo.activeCell);
    LocalDataToBase64Str(cell,sizeof(cell),ruleInfo.activeCell,strLen,22,18);

    printf("activeCell str = %s\n",ruleInfo.activeCell);
    rsl = ONVIF_SetMotionDetectionRule(&devInfo,pToken,&ruleInfo);

    if(0==rsl){
        printf("ONVIF_SetMotionDetectionRule success!\n");
    }else{
        printf("ONVIF_SetMotionDetectionRule failed!\n");
    }

#elif 1
    MotionDetectionRule_S ruleSInfo;
    memset(&ruleSInfo,0,sizeof(MotionDetectionRule_S));

    ruleSInfo.activeCell[0] = 0xffffff;
    ruleSInfo.activeCell[4] = 0xffffff;
    ruleSInfo.activeCell[9] = 0xffffff;

    ruleSInfo.ararmOnDelay = 12;
    ruleSInfo.ararmOffDelay = 12;
    ONVIF_SetMotionDetectionRule_S(&devInfo,pToken,&ruleSInfo);

    int i;
    for(i=0;i<18;i++){
        printf("%08x\n",ruleSInfo.activeCell[i]);
    }

#elif  0
    ONVIF_GetSupportedRules(&devInfo,pToken);

#endif
}









int DevPort = 0;
char DevIp[MAX_LEN] ={0};
extern struct soap * soap_overall;
void OnvifCallback( char *pDeviceIp, unsigned short devicePort, char *pDeviceUuid)
{
    printf("port : %d\n", devicePort);
    printf("ip Address  : %s\n", pDeviceIp);
    printf("uuid  : %s\n", pDeviceUuid);

    DevPort = devicePort ;
    strncpy(DevIp,pDeviceIp,127);
}

#if 0

//测试函数
int main()
{

    extern ONVIF_CAMERAS g_onvifCameras  ;
    int result = 0;
    char urlBuf[256] = { 0 };
    //StartDiscovery(OnvifCallback);

   // while(1)

    {

        time_t t;
        struct tm * area;
        TimeAndDate  pTimeandDate,pTZ;
        int ret = 0;
        int type = 0;
        int ipc_hour= -1;
        int ipc_minute= 0;
        char * ptr = NULL;

        Remote_Device_Info DeviceInfo = { 0 };
        strcpy(DeviceInfo.ip, "172.18.192.171");
        strcpy(DeviceInfo.userName, "admin");
        strcpy(DeviceInfo.password, "admin");
        DeviceInfo.port = 80;

        t = time(NULL);

        //从数据库获取时区数据
        //sprintf(pTZ.TZ,"%s","GMT+08:00");

        ret = ONVIF_GetSystemTimeAndDate(&DeviceInfo,&pTimeandDate);
        fprintf(stderr,"  get tz from ipc %s  \r\n",pTimeandDate.TZ);
    #if 0
        if(ret==0)
        {
            fp
            //china standard  time
            ptr=strstr(pTimeandDate.TZ,"CST");
            if(ptr)
            {
                fprintf(stderr,"%s:%d  \r\n",__func__,__LINE__);
                ptr=strchr(pTimeandDate.TZ,'-');
                if(ptr)
                {
                  sscanf(ptr,"-%d",&ipc_hour);
                   fprintf(stderr,"%s:%d :%d \r\n",__func__,__LINE__,ipc_hour);
                  if(ipc_hour == 8)
                  {
                     type  = 0;
                    fprintf(stderr,"%s type =0 \r\n",pTimeandDate.TZ);
                  }
                  else if(ipc_hour <8)
                  {
                    type = 1;
                  }
                  else if(ipc_hour >8)
                  {
                    type = -1;
                  }
                }
                else
                {
                 fprintf(stderr,"%s:%d  \r\n",__func__,__LINE__);
                 ipc_hour = 8;
                 type  = 1;
                }
            }

        if(strstr(pTimeandDate.TZ,"GMT"))
        {

            ptr=strchr(pTimeandDate.TZ,'+');
            if(ptr)
            {
              sscanf(ptr,"+%d:%d",&ipc_hour,&ipc_minute);
              fprintf(stderr,"hour =%d \r\n",ipc_hour);
              type  = 1;
            }
            else
            {
                     ptr =strchr(pTimeandDate.TZ,'-');
                    if(ptr)
                    {
                     sscanf(ptr,"-%d:%d",&ipc_hour,&ipc_minute);
                     type  = -1;
                    }
                    else
                    {
                        /* 0时区*/
                        ipc_hour = 0;
                        ipc_minute = 0;
                        type = 0;
                    }

            }

        }

        }



        switch(type)
        {
            case -1:
            t += (ipc_hour*3600 + ipc_minute *60 );
               break;
            case 1:
            t -= ( ipc_hour*3600 + ipc_minute *60 );
                break;
            case  0 :
               break;
            default:
                break;
        }
#endif

          t -= ( 8*3600 + 0 *60 );
        sprintf(pTimeandDate.TZ,"%s","GMT+09:00");
        area = localtime(&t);
        pTimeandDate.type = 0;
        pTimeandDate.Year = area->tm_year + 1900;
        pTimeandDate.Month = area->tm_mon +1;
        pTimeandDate.Day = area->tm_mday;
        pTimeandDate.Second = area->tm_sec ;
        pTimeandDate.Hour = area->tm_hour ;
        pTimeandDate.Minute= area->tm_min ;
        pTimeandDate.isUTC = true;


        ret = ONVIF_SetSystemTimeAndDate(&DeviceInfo,&pTimeandDate);






















         #if 0
        Remote_Device_Info DeviceInfo = { 0 };
        strcpy(DeviceInfo.ip, "172.18.195.66");
        strcpy(DeviceInfo.userName, "abc");
        strcpy(DeviceInfo.password, "123");
        DeviceInfo.port = 80;

          TimeAndDate pTimeandDate;

          ONVIF_GetSystemTimeAndDate(&DeviceInfo,&pTimeandDate);
          fprintf(stderr,"timezone = %s \r\n",pTimeandDate.TZ);

          time_t t ;
           t = time(NULL);
           t -= ( 7*3600 + 0 *60 );
           struct tm* area = localtime(&t);
           pTimeandDate.Year = area->tm_year + 1900;
               pTimeandDate.Month = area->tm_mon +1;
               pTimeandDate.Day = area->tm_mday;
               pTimeandDate.Second = area->tm_sec ;
               pTimeandDate.isUTC = true;
               pTimeandDate.Hour= area->tm_hour ;
               pTimeandDate.Minute= area->tm_min ;
                strcpy(pTimeandDate.TZ , "GMT+08:00") ;

          ONVIF_SetSystemTimeAndDate(&DeviceInfo,&pTimeandDate);



        NetworkInterfaces AllInterface;
       // ONVIF_GetNetworkInterfaces(&DeviceInfo , &AllInterface);
       // fprintf(stderr,"size=%d,token=%s,\r\n",AllInterface.__sizeNetworkInterfaces,AllInterface.SingleNetworkInterface[0].token);

        //char * ipaddr = "172.18.195.254";
      //  char* netmask = "255.255.248.0" ;

        strcpy(AllInterface.SingleNetworkInterface[0].InterfaceName,"eth0");
         strcpy(AllInterface.SingleNetworkInterface[0].NetMask,"255.255.248.0");
          strcpy(AllInterface.SingleNetworkInterface[0].IpAddress,"172.18.195.253");
           strcpy(AllInterface.SingleNetworkInterface[0].token,"eth0");
       ONVIF_SetNetworkInterfaces(&DeviceInfo ,&AllInterface);
       sleep(1);
          strcpy(DeviceInfo.ip, "172.18.195.253");
       ONVIF_SetDefaultGateway(&DeviceInfo ,"172.18.195.2");



        int index =ONVIF_AddRemoteDevice(1,&DeviceInfo);


        ONVIF_GetSystemTimeAndDate(&DeviceInfo);

        Onvif_Profiles getProfilesRespone = { 0 };
        ONVIF_GetProfiles(&DeviceInfo, &getProfilesRespone);

        printf("\n\n########################ONVIF_GetProfiles\n");
        int sizeUri = getProfilesRespone.sizeOfProfiles;
        int k = 0;

        Onvif_Img_Options pGetImgOptionsResponse;
        Onvif_Img_Config pSetImgConfiguration;

        pSetImgConfiguration.brightness = 80.0;
        pSetImgConfiguration.Contrast = 80.0;
        pSetImgConfiguration.ColorSaturation = 80.0;
        pSetImgConfiguration.Sharpness = 80.0;
        // printf("@@@@@@@@@@@@@@@@@@@@@@@@@@内存大小为：%ld",sizeof(g_onvifCameras) );
        Onvif_Video_Config pGetvideoConfigurationResponse ;

        Onvif_VideoEncoderConfigurationOptions pGetVideoEncoderConfigurationOptionsResponse;

        for ( k = 0; k < sizeUri; k++ )
        {
            ONVIF_GetStreamUri(&DeviceInfo, getProfilesRespone.Onvif_Profiles[k].profileToken, urlBuf );
            printf("         %s \n", urlBuf);
            printf("profile=%s,token=%s\n",getProfilesRespone.Onvif_Profiles[k].profileToken,g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEtoken);

            ONVIF_GetVideoEncoderConfiguration(&DeviceInfo,getProfilesRespone.Onvif_Profiles[k].profileToken,g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEtoken,
                                               &pGetvideoConfigurationResponse,&pGetVideoEncoderConfigurationOptionsResponse);



            int i =0;
            for(i=0;i!=pGetVideoEncoderConfigurationOptionsResponse.H264Options.__sizeResolutionsAvailable;++i)
            {
                fprintf(stderr,"resolution%d  , h =%d ,w =%d\r\n ",i,pGetVideoEncoderConfigurationOptionsResponse.H264Options.ResolutionsAvailable[i].Height,
                        pGetVideoEncoderConfigurationOptionsResponse.H264Options.ResolutionsAvailable[i].Width);



            }




            //ONVIF_GetVideoEncoderConfiguration(&DeviceInfo,g_onvifCameras.onvif_info[index].Profiles[k].profiletoken ,g_onvifCameras.onvif_info[index].Profiles[k].AESC.VEtoken, &pGetVideoEncoderConfigurationOptionsResponse);
            printf("    hasH264 = %d \n, FrameRateRangeMAX = %d\n,FrameRateRangemin = %d\n,EncodingIntervalRangemax = %d\n,EncodingIntervalRangemin =%d\n,size =%d\n ",
                   pGetVideoEncoderConfigurationOptionsResponse.hasH264,

                   pGetVideoEncoderConfigurationOptionsResponse.H264Options.FrameRateRange.Max,
                   pGetVideoEncoderConfigurationOptionsResponse.H264Options.FrameRateRange.Min,
                   pGetVideoEncoderConfigurationOptionsResponse.H264Options.EncodingIntervalRange.Max,
                   pGetVideoEncoderConfigurationOptionsResponse.H264Options.EncodingIntervalRange.Min,
                   pGetVideoEncoderConfigurationOptionsResponse.H264Options.__sizeResolutionsAvailable);


#if 0
            ONVIF_GetImageOptions(&DeviceInfo,  g_onvifCameras.onvif_info[index].Profiles[k].AVSC.Vsourcetoken, &pGetImgOptionsResponse);
            printf("brightness.min = %lf\n,Contrast.min =%lf\n,ColorSaturation.min = %lf \n ,Sharpness.min =%lf \n ",pGetImgOptionsResponse.brightness.Min, pGetImgOptionsResponse.Contrast.Min ,
                   pGetImgOptionsResponse.ColorSaturation.Min , pGetImgOptionsResponse.Sharpness.Min);
            printf("brightness.max = %lf\n,Contrast.max =%lf\n,ColorSaturation.max = %lf \n ,Sharpness.max =%lf \n ",pGetImgOptionsResponse.brightness.Max, pGetImgOptionsResponse.Contrast.Max ,
                   pGetImgOptionsResponse.ColorSaturation.Max , pGetImgOptionsResponse.Sharpness.Max);

            ONVIF_GetImageConfiguration(&DeviceInfo,  g_onvifCameras.onvif_info[index].Profiles[k].AVSC.Vsourcetoken, &pGetImgConfigurationResponse);
            //printf("brightness = %lf\n,Contrast =%lf\n,ColorSaturation = %lf \n ,Sharpness =%lf \n ",pGetImgConfigurationResponse.brightness, pGetImgConfigurationResponse.Contrast ,
            //pGetImgConfigurationResponse.ColorSaturation, pGetImgConfigurationResponse.Sharpness);

#endif
        }


        ONVIF_SetImageConfiguration(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].AVSC.Vsourcetoken,&pSetImgConfiguration);

        //ONVIF_EventSubscribe(&DeviceInfo);

        //printf("\n\n########################PTZ\n");
        ONVIF_GetNodesResponse pGetNodesResponse;

        ONVIF_PtzGetNodes(&DeviceInfo, &pGetNodesResponse);

#endif
#if 0


        Onvif_Ptz_Presets ptz_presets;
        #if 1
        ONVIF_PtzGetPresets(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken,  &ptz_presets);
        printf("	first presets= %d \n", ptz_presets.sizeOfPresets);
        for ( k = 0; k < ptz_presets.sizeOfPresets; k++ )
        {
            printf("	%d preset Name =%s  token= %s  pan_x =%lf, pan_y =%lf ,zoom =%lf\n", k,
                   ptz_presets.presets[k].presetName,
                   ptz_presets.presets[k].presetToken,
                   ptz_presets.presets[k].PanTilt_x,
                   ptz_presets.presets[k].PanTilt_y,
                   ptz_presets.presets[k].zoom
                   );
        }

        Onvif_Ptz_ConfigOtions ptzOtions;
        ONVIF_PtzGetConfigurationOptions(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].PTZtoken, &ptzOtions);
        printf("	vec X %f - %f \n", ptzOtions.PanTiltVelocityRangeX.Min, ptzOtions.PanTiltVelocityRangeX.Max);
        printf("	vec Y %f - %f \n", ptzOtions.PanTiltVelocityRangeY.Min, ptzOtions.PanTiltVelocityRangeY.Max);
        printf("	zoom min %f - max%f \n", ptzOtions.ZoomVelocityRangeX.Min, ptzOtions.ZoomVelocityRangeX.Max);
        printf("	zoomspeend Y %f - %f \n", ptzOtions.ZoomSpeedRangeX.Min, ptzOtions.ZoomSpeedRangeX.Max);
        printf("	speed X %f - %f \n", ptzOtions.PanTiltSpeedRangeX.Min, ptzOtions.PanTiltSpeedRangeX.Max);
        printf("	timeOut  %d - %d /s\n", ptzOtions.TimeOutRange.Min, ptzOtions.TimeOutRange.Max);
#endif
        Onvif_Ptz_Config pGetPtzConfig;

        strcpy(pGetPtzConfig.token,g_onvifCameras.onvif_info[index].Profiles[0].PTZtoken);
        ONVIF_PtzGetConfiguration(&DeviceInfo,  &pGetPtzConfig);

        ONVIF_PtzSetPreset  (&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken,"feifei", "6" );
        printf("############token =%s \r\n",g_onvifCameras.onvif_info[index].Profiles[0].profiletoken);
           printf("name =%s \n  token =%s \n ,speed = %lf\n",pGetPtzConfig.name,pGetPtzConfig.token,pGetPtzConfig.DefaultZoomSpeedX);
        //sleep(2);
        ONVIF_PtzGotoPreset(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken, "6",0);
        sleep(1);
       // ONVIF_PtzRemovePreset(&DeviceInfo,g_onvifCameras.onvif_info[index].Profiles[0].profiletoken,"6");


        ONVIF_PtzGetPresets(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken,  &ptz_presets);
        //printf("	second  presets= %d \n", ptz_presets.sizeOfPresets);
      //  for ( k = 0; k < ptz_presets.sizeOfPresets; k++ )
      //  {
          //  printf("	%d preset Name =%s  token= %s  pan_x =%lf, pan_y =%lf ,zoom =%lf\n", k,
          //         ptz_presets.presets[k].presetName,
         //          ptz_presets.presets[k].presetToken,
         //          ptz_presets.presets[k].PanTilt_x,
         //          ptz_presets.presets[k].PanTilt_y,
         //          ptz_presets.presets[k].zoom       ) ;
    //   }

        ONVIF_PTZ__ContinuousMove ptzMove = { 0 };
        strcpy(ptzMove.ProfileToken, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken);
        ptzMove.isPan = 1;
        ptzMove.Speed = 6;
        ptzMove.direction = PTZ_LEFT_DOWN;
        ptzMove.Timeout =10000;

        ptzMove.isZoom = 0;

        //ONVIF_PtzContinueMove(&DeviceInfo, &ptzMove);
          sleep(5);
        //	ONVIF_PtzStop(&DeviceInfo, g_onvifCameras.onvif_info[index].Profiles[0].profiletoken );
#endif

    }
    return 0 ;


}



#endif
