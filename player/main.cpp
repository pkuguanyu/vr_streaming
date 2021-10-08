#include <stdio.h>
#include "peer_management.hpp" 
#define __STDC_CONSTANT_MACROS
 
#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "SDL/SDL.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL/SDL.h>
#include <unistd.h>
#ifdef __cplusplus
};
#endif
#endif
 
//Refresh
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define players 1 //the number of players
#define gap_limit 30 //if bar for synchronization
 
int thread_exit=0;
//Thread
int sfp_refresh_thread(void *opaque)
{
	SDL_Event event;
	while (thread_exit==0) {
		event.type = SFM_REFRESH_EVENT;
		SDL_PushEvent(&event);
		//Wait 40 ms
		SDL_Delay(40);
	}
	return 0;
}
 
int main(int argc, char* argv[])
{
	int player_id = argv[1][0] - '0';

	peer_management group;
	if (players > 1) group.init(player_id, players);
	printf("player id: %d\n", player_id);

	AVFormatContext	*pFormatCtx;
	int				i, videoindex;
	AVCodecContext	*pCodecCtx;
	AVCodec			*pCodec;
	AVFrame	*pFrame,*pFrameYUV;
	AVPacket *packet;
	struct SwsContext *img_convert_ctx;
	//SDL
	int ret, got_picture;
	int screen_w=0,screen_h=0;
	SDL_Surface *screen; 
	SDL_Overlay *bmp; 
	SDL_Rect rect;
	SDL_Thread *video_tid;
	SDL_Event event;
 
	char filepath[]="rtmp://127.0.0.1/videotest/test0_out";
	int len = strlen(filepath);
	filepath[len - 5] += argv[1][0] - '0';
	puts(filepath);
	av_register_all();
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	
	if(avformat_open_input(&pFormatCtx,filepath,NULL,NULL)!=0){
		printf("Couldn't open input stream.\n");
		return -1;
	}
	if(avformat_find_stream_info(pFormatCtx,NULL)<0){
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) 
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO){
			videoindex=i;
			break;
		}
	if(videoindex==-1){
		printf("Didn't find a video stream.\n");
		return -1;
	}
	pCodecCtx=pFormatCtx->streams[videoindex]->codec;
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL)
	{
		printf("Codec not found.\n");
		return -1;
	}
	if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
	{
		printf("Could not open codec.\n");
		return -1;
	}
 
	pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();
//------------SDL----------------
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {  
		printf( "Could not initialize SDL - %s\n", SDL_GetError()); 
		return -1;
	} 
 
	
	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	screen = SDL_SetVideoMode(screen_w, screen_h, 0,0);
 
	if(!screen) {  
		printf("SDL: could not set video mode - exiting:%s\n",SDL_GetError());  
		return -1;
	}
	
	bmp = SDL_CreateYUVOverlay(pCodecCtx->width, pCodecCtx->height,SDL_YV12_OVERLAY, screen); 
	
	rect.x = 0;    
	rect.y = 0;    
	rect.w = screen_w;    
	rect.h = screen_h;  
 
	packet=(AVPacket *)av_malloc(sizeof(AVPacket));
 
	printf("---------------File Information------------------\n");
	av_dump_format(pFormatCtx,0,filepath,0);
	printf("-------------------------------------------------\n");
	
	
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL); 
	//--------------
	video_tid = SDL_CreateThread(sfp_refresh_thread,NULL);
	//
	SDL_WM_SetCaption("Simple FFmpeg Player (SDL Update)",NULL);
 
	//Event Loop
	
	int max_gap = 0;
	
	for (;;) {
		//Wait
		SDL_WaitEvent(&event);
		if(event.type==SFM_REFRESH_EVENT){
			//------------------------------
			while(av_read_frame(pFormatCtx, packet)>=0){
				if(packet->stream_index==videoindex){
					ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
					if(ret < 0){
						printf("Decode Error.\n");
						return -1;
					}
					if(got_picture){
						if (players > 1) {
							//check whether the current player is behind others		
							group.update_peers();
							group.update_current_frame(packet->pts);
							group.broadcast();
							max_gap = group.gap();
							//If behind, discard this frame without display, in order to get the next frame immediately. 
							if (max_gap > gap_limit) {
								av_free_packet(packet);
								continue;
							}
						}
						SDL_LockYUVOverlay(bmp);
						pFrameYUV->data[0]=bmp->pixels[0];
						pFrameYUV->data[1]=bmp->pixels[2];
						pFrameYUV->data[2]=bmp->pixels[1];     
						pFrameYUV->linesize[0]=bmp->pitches[0];
						pFrameYUV->linesize[1]=bmp->pitches[2];   
						pFrameYUV->linesize[2]=bmp->pitches[1];
						sws_scale(img_convert_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height, pFrameYUV->data, pFrameYUV->linesize);
 
						SDL_UnlockYUVOverlay(bmp); 
						
						SDL_DisplayYUVOverlay(bmp, &rect); 
						if (packet->pts % 640 == 0) printf("frame %d\n", packet->pts);
						av_free_packet(packet);
 						break;
					}					
				}
				av_free_packet(packet);
			}
		}
 
	}
	
	SDL_Quit();
 
	sws_freeContext(img_convert_ctx);
 
	//--------------
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
 
	return 0;
}

