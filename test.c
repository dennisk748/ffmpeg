#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    if (argc < 2){
        printf("you need to specify a media file");
        return -1;
    }

    AVFormatContext *pFormatCtx = avformat_alloc_context();

    if(avformat_open_input(&pFormatCtx, argv[1],NULL,NULL) != 0){
        printf("could not open file");
        return -1;
    }

    printf("Format %s, duration %ld us", pFormatCtx->iformat->long_name,pFormatCtx->duration);

    return 0;
}