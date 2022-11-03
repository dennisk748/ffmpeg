#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <stdio.h>

//COMPATIBILITY WITH NEWER API
#if LIBAVCODEC_VERSION_INT < AV_VERSION_INT(55, 28, 1)
#define av_frame_alloc avcodec_alloc_frame
#define av_frame_free avcodec_free_frame
#endif
 
void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame){
    FILE *pFile;
    char szFIlename[32];
    int y;


    //open file
    sprintf(szFIlename, "/home/Documents/frame%d.ppm", iFrame);
    pFile = fopen(szFIlename, "wb");
    if(pFile == NULL)
        return;
    
    //write header
    fprintf(pFile, "p6\n%d %d\n255\n", width, height);

    //write pixel data
    for(y=0; y<height; y++)
        fwrite(pFrame->data[0]+y*pFrame->linesize[0], 1, width*3, pFile);
    
    //close file
    fclose(pFile);
    }

int main(int argc, char *argv[])
{
    AVFormatContext *pFormatCtx = NULL;
    int i, videoStream, numBytes, frameFinished;
    struct SwsContext *sws_ctx = NULL;
    AVCodecContext *pCodecCtxOrig = NULL;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec = NULL;
    AVFrame *pFrame = NULL;
    AVFrame *pFrameRGB = NULL;
    uint8_t *buffer = NULL;
    AVPacket packet;

    // register all available file formats and codecs
    av_register_all();

    //OPEN VIDEO FILE
    if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0)
    {
        return -1; // couldnt open file
    }

    //RETRIEVE STREAM INFORMATION AND PUT THEM IN pFormatCtx->streams
    if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
        return -1; // COuldnt find stream information
    }
    
    // Dump information about file onto standard error
    av_dump_format(pFormatCtx, 0, argv[1], 0);

    // at this point pFormatCtx->streams is an array of pointers of size pFormatCtx->nb_streams
    // so lets walk through it until we find a video stream
    
    //lets find the first video stream
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {   
        if(pFormatCtx->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_VIDEO){
            videoStream = i;
            break;
        }
        if(videoStream == -1){
            return -1; //Didnt find a video stream
        }
    }
    // get a pointer to the codec context for the video stream
    pCodecCtxOrig = pFormatCtx->streams[videoStream]->codecpar;

    // the streams information about the codec is in what we call the "codec context"
    // this contains all the information about the codec that the stream is using
    //and now we have a pointer to it. we still have to find the actual codec and open it

    // next lets find a decoder for the video stream codec
    pCodec = avcodec_find_decoder(pCodecCtxOrig->codec_id);
    if(pCodec == NULL){
        fprintf(stderr, "Unsupported codec!\n");
        return -1; //Codec not found
    } 

    // copy context
    pCodecCtx = avcodec_alloc_context3(pCodec);
    if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0){
        fprintf(stderr, "Couldn't copy codec context");
        return -1; //Error copying codec context
    }
    // we dont use the avcodecctx from the video stream directly, we have
    // to use avcodec_copy_context() to copy the ctx to a new location(after allocating memory for it)

    //open codec
    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0 ){
        return -1; // could not open codec
    }
    
    // STORING THE DATA

    //Allocate video frame
    pFrame = av_frame_alloc();

    //well output to PPM FIles, which are stored in 24-bit RGB,
    //well convert our frame from its native format to RGB.ffmpeg will do the conversion for us
    //we are going to convert our initial frame to a specific format

    //Allocate an avframe structure for converted frame now
    pFrameRGB = av_frame_alloc();
    if(pFrameRGB == NULL){
        return -1;
    }

    // even though weve allocated the frame, we still need a place to put the
    // raw data when we convert it.We use avpicture_get_size to get the size 
    // we need and allocate the space manually

    //Determine required buffer size and allocate buffer
    numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
    buffer = (uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

    //Assign appropriate parts of buffer to image planes in pFrameRGB
    //ABout avpicture cast:the avpicture struct is a subset of the avframe
    //struct
    avpicture_fill((AVFrame *)pFrameRGB, buffer, AV_PIX_FMT_RGB24,
                    pCodecCtx->width,pCodecCtx->height);
    
    // now we are ready to read form the stream
    //READING THE DATA

    // what we are going to do is read through the entire video stream
    //by reading in the packet,decoding it into our frame, and once our frame
    //is complete, we convert and save it.

    //initialize SWS context for software scaling
    sws_ctx = sws_getContext(pCodecCtx->width,
                pCodecCtx->height,
                pCodecCtx->pix_fmt,
                pCodecCtx->width,
                pCodecCtx->height,
                AV_PIX_FMT_RGB24,
                SWS_BILINEAR,
                NULL,
                NULL,
                NULL);
    i=0;
    while (av_read_frame(pFormatCtx, &packet) >= 0)
    {
        // is this a packet from the video stream
        if(packet.stream_index == videoStream){
            // decode video frame
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            //did we get a video frame
            if(frameFinished){
                //convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                            pFrame->linesize, 0, pCodecCtx->height,
                            pFrameRGB->data, pFrameRGB->linesize);

                // save the frame to disk
                if(++i <= 5)
                    SaveFrame(pFrameRGB,pCodecCtx->width,pCodecCtx->height, i);
            }

        }
        // free the packet that was allocated by av_read_frame
        av_packet_free(&packet);
    }
    
    //free the RGB image
    av_free(buffer);
    av_free(pFrameRGB);

    //free the YUV frame
    av_free(pFrame);

    //close the codecs
    avcodec_close(pCodecCtx);
    avcodec_close(pCodecCtxOrig);

    //close the video file
    avformat_close_input(&pFormatCtx);

    return 0;

}