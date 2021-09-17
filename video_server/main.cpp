extern "C"
{
	#include "libavcodec/avcodec.h"
	#include "libavformat/avformat.h"
	#include "libswscale/swscale.h"
	#include "libswresample/swresample.h"
	#include "libavutil/imgutils.h"
	#include "libavutil/pixfmt.h"
};

#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <mutex>
#include <unistd.h>

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

//define ROI
#define w_min 200
#define h_min 200
#define w_max 3839
#define h_max 1959

using namespace std;
mutex mtx;
int frame_count[6];

//get the ROI area of a given frame.
//Input
//iframe: the input frame
//_w_min, _h_min, _w_max, _h_max: the ROI region
//fmt: Pixel format of the input frame
//Output
//oframe: the output frame
void framecut(AVFrame * oframe , AVFrame * iframe, int _w_min, int _h_min, int _w_max, int _h_max, AVPixelFormat fmt)
{
	int width = _w_max - _w_min + 1;  
    	int height = _h_max - _h_min + 1;
	if (oframe == NULL) {  
		oframe = av_frame_alloc();  
        	av_image_alloc(oframe->data, oframe->linesize, width, height, fmt, 1);  
    	}
	int h[3], w[3], h_offset[3], w_offset[3];
	unsigned char* pnt;
      	switch (fmt) {
		//for yuv420p, output a yuv420p frame
		case AVPixelFormat::AV_PIX_FMT_YUV420P:
    			h[0] = height, h[1] = h[2] = height >> 1;
    			w[0] = width, w[1] = w[2] = width >> 1;
    			h_offset[0] = _h_min, h_offset[1] = h_offset[2] = _h_min >> 1;
    			w_offset[0] = _w_min, w_offset[1] = w_offset[2] = _w_min >> 1;
			for (int i = 0; i < 3; ++i) {
				pnt = oframe->data[i];
				for (int j = 0; j < h[i]; ++j)
					memcpy(pnt + oframe->linesize[i] * j , iframe->data[i] + iframe->linesize[i] * (j+ h_offset[i]) + w_offset[i], w[i] * sizeof(char));
			}
			break;
		//for nv12, output an nv12 frame
		case AVPixelFormat::AV_PIX_FMT_NV12:
                        h[0] = height, h[1] = height >> 1;
                        w[0] = width, w[1] = width;
                        h_offset[0] = _h_min, h_offset[1] = _h_min >> 1;
                        w_offset[0] = _w_min, w_offset[1] = _w_min;
                        for (int i = 0; i < 2; ++i) {
                                pnt = oframe->data[i];
                                for (int j = 0; j < h[i]; ++j)
                                        memcpy(pnt + oframe->linesize[i] * j , iframe->data[i] + iframe->linesize[i] * (j+ h_offset[i]) + w_offset[i], w[i] * sizeof(char));
			}
			break;
		//warning: for yuvj422p, output a yuv420p frame, to make the frame acceptable for nvenc codec
                case AVPixelFormat::AV_PIX_FMT_YUVJ422P:
                        h[0] = h[1] = h[2] = height;
                        w[0] = width, w[1] = w[2] = width >> 1;
                        h_offset[0] = h_offset[1] = h_offset[2] = _h_min;
                        w_offset[0] = _w_min, w_offset[1] = w_offset[2] = _w_min >> 1;
                        pnt = oframe->data[0];
                        for (int j = 0; j < h[0]; j++)
                        	memcpy(pnt + oframe->linesize[0] * j , iframe->data[0] + iframe->linesize[0] * (j+ h_offset[0]) + w_offset[0], w[0] * sizeof(char));
			for (int i = 1; i < 3; ++i) {
                                pnt = oframe->data[i];
                                for (int j = 0; j < h[i]; j+=2)
                                        memcpy(pnt + oframe->linesize[i] * (j >> 1) , iframe->data[i] + iframe->linesize[i] * (j+ h_offset[i]) + w_offset[i], w[i] * sizeof(char));

                        }
                        break;
		default:
			puts("error: invalid pixel format");
			break;
	}
}

int work(int thread_id, int thread_sum)
{
	AVFormatContext* ifmt_ctx = NULL;
	const char*   input_filepath = NULL;

	AVFormatContext* ofmt_ctx = NULL;
	const char* output_filepath = NULL;

	int idx_video = -1;
	int output_idx_video = -1;

	AVStream* istream_video = NULL;
	AVCodecContext*  codec_ctx_video = NULL;
	AVCodec* codec_video = NULL;

	AVStream* ostream_video = NULL;

	AVCodecContext*  codec_ctx_2h264 = NULL;
	AVCodec* codec_2h264 = NULL;

	uint8_t* buffer_yuv = NULL;

	av_register_all();
	avformat_network_init();
	
	//generate input / output filepath based on thread id
	string input_stream = "http://127.0.0.1:8000/stream";
	input_stream[20] += thread_id;
	cout << "Thread " << input_stream << endl;
	string output_stream = "rtmp://127.0.0.1/videotest/test";
	output_stream += char('0' + thread_id);
	output_stream += "_out";
	input_filepath = input_stream.c_str();
	output_filepath = output_stream.c_str();

	AVDictionary* options = NULL;
	//av_dict_set(&options, "rtmp_transport", "tcp", 0);
	av_dict_set(&options, "input_format", "mjpeg", 0);
	
	if (avformat_open_input(&ifmt_ctx, input_filepath, NULL, &options) < 0)
	{
		printf("error: avformat_open_input\n");
		return -1;
	}

	if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
	{
		printf("error: avformat_find_stream_info\n");
		return -1;
	}

	if (avformat_alloc_output_context2(&ofmt_ctx, NULL, "flv", output_filepath) < 0)
	{
		printf("error: avformat_alloc_output_context2\n");
		return -1;
	}
	
	for (int i = 0; i < ifmt_ctx->nb_streams; i++)
		if (AVMEDIA_TYPE_VIDEO == ifmt_ctx->streams[i]->codec->codec_type)
		{
			//get the video channel ID.
			idx_video = i;

			istream_video = ifmt_ctx->streams[idx_video];
			codec_ctx_video = istream_video->codec;

			ostream_video = avformat_new_stream(ofmt_ctx, NULL);
			output_idx_video = ofmt_ctx->nb_streams - 1;

			// necessary settings for video streaming.
			ostream_video->codec->codec_tag = 0;
			if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				ostream_video->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		}

	if (idx_video < 0)
	{
		printf("error: can not find any video stream\n");
		return -1;
	}

	codec_video = avcodec_find_decoder(codec_ctx_video->codec_id);
	//codec_video = avcodec_find_decoder_by_name("h264_cuvid");
	if (NULL == codec_video)
	{
		printf("error: avcodec_find_decoder, input video\n");
		return -1;
	}

	//open the video stream decoder
	if (avcodec_open2(codec_ctx_video, codec_video, NULL) < 0)
	{
		printf("error: avcodec_open2, input video\n");
		return -1;
	}

	codec_ctx_2h264 = ostream_video->codec;
        codec_ctx_2h264->pix_fmt = AV_PIX_FMT_YUV420P;
        codec_ctx_2h264->width = w_max - w_min + 1;
        codec_ctx_2h264->height = h_max - h_min + 1;
        codec_ctx_2h264->bit_rate = 4000000;
        codec_ctx_2h264->gop_size = 250;
        codec_ctx_2h264->time_base.num = 1;
        codec_ctx_2h264->time_base.den = 30;
        codec_ctx_2h264->qmin = 10;
        codec_ctx_2h264->qmax = 51;
        codec_ctx_2h264->max_b_frames = 0;

        AVDictionary* param = NULL;
        av_dict_set(&param, "tune", "zerolatency", 0);

        codec_2h264 = avcodec_find_encoder_by_name("h264_nvenc");
        if (avcodec_open2(codec_ctx_2h264, codec_2h264, &param) < 0)
        {
                printf("error: avcodec_open2, output video\n");
                return -1;
        }

	if (avio_open(&ofmt_ctx->pb, output_filepath, AVIO_FLAG_WRITE) < 0)
	{
		printf("error: avio_open\n");
		return -1;
	}

	if (avformat_write_header(ofmt_ctx, NULL) < 0)
	{
		printf("error: avformat_write_header\n");
		return -1;
	}

	av_dump_format(ifmt_ctx, -1, input_filepath, 0);
	av_dump_format(ofmt_ctx, -1, output_filepath, 1);

	AVFrame  oframe_yuv = { 0 };
	oframe_yuv.width = w_max - w_min + 1;
	oframe_yuv.height = h_max - h_min + 1;
	oframe_yuv.format = AV_PIX_FMT_YUV420P;

	int pic_size_yuv = av_image_get_buffer_size((AVPixelFormat)oframe_yuv.format, oframe_yuv.width, oframe_yuv.height, 1);
	buffer_yuv = (uint8_t*)av_malloc(pic_size_yuv);
	av_image_fill_arrays(oframe_yuv.data, oframe_yuv.linesize, buffer_yuv, (AVPixelFormat)oframe_yuv.format, oframe_yuv.width, oframe_yuv.height, 1);

	AVPacket ipkt = { 0 };
	av_init_packet(&ipkt);
	ipkt.data = NULL;
	ipkt.size = 0;

	AVFrame  iframe = { 0 };

	AVPacket opkt = { 0 };
	av_init_packet(&opkt);
	opkt.data = NULL;
	opkt.size = 0;

	int ret;
	int video_count = 0;

	while (1)
	{	
		if (av_read_frame(ifmt_ctx, &ipkt) < 0)  
		break;

		if (ipkt.stream_index == idx_video)
		{
			ret = avcodec_send_packet(codec_ctx_video, &ipkt);
			if (ret < 0) {
				puts("avcodec_send_packet ERROR");
				av_packet_unref(&ipkt);
				return -1;
			}
			ret = avcodec_receive_frame(codec_ctx_video, &iframe);
			if (ret == AVERROR(EAGAIN)) {//need to send more data to the decoder to get the decoded frame.
				av_packet_unref(&ipkt);
				continue;
			}
			if (ret < 0) {
				puts("avcodec_receive_frame ERROR");
				av_packet_unref(&ipkt);
				return -1;
			}

			framecut(&oframe_yuv , &iframe, w_min, h_min, w_max, h_max, codec_ctx_video->pix_fmt);

			oframe_yuv.pts = ipkt.pts;
			oframe_yuv.width = codec_ctx_2h264->width;
			oframe_yuv.height = codec_ctx_2h264->height;
			
			ret = avcodec_send_frame(codec_ctx_2h264, &oframe_yuv);
			if (ret < 0) {
				printf("send_frame failed %d\n", ret);
				av_packet_unref(&ipkt);
				return -1;
			}
			while (1) {
				ret = avcodec_receive_packet(codec_ctx_2h264, &opkt);
				if (ret == AVERROR(EAGAIN)) {
					av_packet_unref(&ipkt);
					break;
				};
				if (ret < 0) {
					printf("receive_packet failed %d\n", ret);
					av_packet_unref(&ipkt);
					return -1;
				}

				opkt.pts = av_rescale_q_rnd(ipkt.pts, istream_video->time_base, ostream_video->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				opkt.dts = av_rescale_q_rnd(ipkt.dts, istream_video->time_base, ostream_video->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				opkt.duration = av_rescale_q(ipkt.duration, istream_video->time_base, ostream_video->time_base);
			
				opkt.pos = -1;
				opkt.stream_index = output_idx_video;

				av_interleaved_write_frame(ofmt_ctx, &opkt);
				av_packet_unref(&opkt);
			
				++video_count;
				if ((video_count & 3) == 0) {
					frame_count[thread_id] = video_count;
					while (1) {
						if (mtx.try_lock()) {
							ret = 0;
							for (int i = 0; i < thread_sum; ++i)
								if (frame_count[i] < video_count) ret = 1;
							mtx.unlock();
						}
						if (!ret) break;
						//if the thread is ahead of another thread, sleep 1ms.
						usleep(1000);
					}
				}
			}
		}
		av_packet_unref(&ipkt);
	}

	if (0 != av_write_trailer(ofmt_ctx))
	{
		printf("error: av_write_trailer\n");
		return -1;
	}

	avformat_close_input(&ifmt_ctx);
	av_free(buffer_yuv);
	avcodec_free_context(&codec_ctx_2h264);
	if (NULL != ofmt_ctx) avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
    	
	return 0;
}

int main()
{
	for (uint8_t i = 0; i < 2; i++)
	{
		thread t(work, i, 2);
		t.detach();
	}
	getchar();
	return 0;
}
