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

#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

#define MAX_AUDIO_FRAME_SIZE 192000  // 1 second of 48KHZ 32bit audio -> 32bit / 8 * 48000 * 1 = 192000 Byte  

#define screens 6

using namespace std;

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
                        break;
        }
}

int main()
{
	AVFormatContext* ifmt_ctx = NULL;
	const char*   input_filepath = NULL;

	AVFormatContext* ofmt_ctx[screens];
	char output_filepath[screens][255];

	int idx_video = -1;
	int output_idx_video = -1;

	AVStream* istream_video = NULL;
	AVCodecContext*  codec_ctx_video = NULL;
	AVCodec* codec_video = NULL;

	AVStream* ostream_video[screens];

	AVCodecContext*  codec_ctx_2h264[screens];
	AVCodec* codec_2h264[screens];

	struct SwsContext* img_convert_ctx_2yuv = NULL;  

	uint8_t* buffer_yuv = NULL;

	av_register_all();
	avformat_network_init();

	string input_stream = "http://127.0.0.1:8000/stream";
	cout << "Thread " << input_stream << endl;
	string output_stream = "rtmp://127.0.0.1/videotest/test";
	input_filepath = input_stream.c_str();

	AVDictionary* options = NULL;
	//av_dict_set(&options, "rtmp_transport", "tcp", 0);
	av_dict_set(&options, "input_format", "flv", 0);
	int ret;
while (1) {
	if (avformat_open_input(&ifmt_ctx, input_filepath, NULL, &options) < 0)
	{
		printf("error: avformat_open_input %d\n", ret);
		break;
	}

	if (avformat_find_stream_info(ifmt_ctx, NULL) < 0)
	{
		printf("error: avformat_find_stream_info\n");
		break;
	}
	
	string tmp;
	for (int i = 0; i < screens; ++i) {
		tmp = output_stream + char('0' + i);
		tmp += "_out";
		cout << tmp << endl;
		strcpy(output_filepath[i], tmp.c_str());
		if (avformat_alloc_output_context2(&ofmt_ctx[i], NULL, "flv", output_filepath[i]) < 0)
		{
			printf("error: avformat_alloc_output_context2\n");
			break;
		}
	}

	for (int i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		if (AVMEDIA_TYPE_VIDEO == ifmt_ctx->streams[i]->codec->codec_type)
		{
			idx_video = i;

			istream_video = ifmt_ctx->streams[idx_video];
			codec_ctx_video = istream_video->codec;

			for (int j = 0; j < screens; ++j) {
				ostream_video[j] = avformat_new_stream(ofmt_ctx[j], NULL);
				output_idx_video = ofmt_ctx[j]->nb_streams - 1;
			}

			// 以下设置必须有，否则网络推流会出错。有时间再研究。
			for (int j = 0; j < screens; ++j) {
				ostream_video[j]->codec->codec_tag = 0;
				if (ofmt_ctx[j]->oformat->flags & AVFMT_GLOBALHEADER)
					ostream_video[j]->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}
		}
		else
		{
			;
		}
	}

	if (idx_video < 0)
	{
		printf("error: can not find any video stream\n");
		break;
	}

	codec_video = avcodec_find_decoder(codec_ctx_video->codec_id);
	//codec_video = avcodec_find_decoder_by_name("h264_cuvid");
	if (NULL == codec_video)
	{
		printf("error: avcodec_find_decoder, input video\n");
		break;
	}
		
	if (avcodec_open2(codec_ctx_video, codec_video, NULL) < 0)  // 打开输入视频流的解码器
	{
		printf("error: avcodec_open2, input video\n");
		break;
	}

	for (int i = 0; i < screens; ++i) {
		codec_ctx_2h264[i] = ostream_video[i]->codec;
		codec_ctx_2h264[i]->codec_type = AVMEDIA_TYPE_VIDEO;
		codec_ctx_2h264[i]->pix_fmt = AV_PIX_FMT_YUV420P;
		//codec_ctx_2h264[i]->pix_fmt = codec_ctx_video->pix_fmt;
		//codec_ctx_2h264->width = codec_ctx_video->width;
		//codec_ctx_2h264->height = codec_ctx_video->height;
		codec_ctx_2h264[i]->width = ((codec_ctx_video->width / screens) >> 1 ) << 1;
		codec_ctx_2h264[i]->height = ((codec_ctx_video->width / screens / 16 * 9) >> 1) << 1;
		codec_ctx_2h264[i]->bit_rate = 4000000;
		codec_ctx_2h264[i]->gop_size = 250;
		codec_ctx_2h264[i]->time_base.num = 1;
		codec_ctx_2h264[i]->time_base.den = 30;
		codec_ctx_2h264[i]->qmin = 10;
		codec_ctx_2h264[i]->qmax = 51;
		codec_ctx_2h264[i]->max_b_frames = 0;
	}

	AVDictionary* param[screens];
	for (int i = 0; i < screens; ++i) {
		param[i] = NULL;
		//av_dict_set(&param[i], "preset", "veryfast", 0);
		av_dict_set(&param[i], "tune", "zerolatency", 0);
		//av_dict_set(&param, "profile", "main", 0);
	}

	for (int i = 0; i < screens; ++i) {
		codec_2h264[i] = avcodec_find_encoder_by_name("h264_nvenc");
		//codec_2h264[i] = avcodec_find_encoder(AV_CODEC_ID_H264);
		if (avcodec_open2(codec_ctx_2h264[i], codec_2h264[i], &param[i]) < 0)
		{
			printf("error: avcodec_open2, output video\n");
			break;
		}
	}
	for (int i = 0; i < screens; ++i) {
		if (avio_open(&ofmt_ctx[i]->pb, output_filepath[i], AVIO_FLAG_WRITE) < 0)
		{
			printf("error: avio_open\n");
			break;
		}

		if (avformat_write_header(ofmt_ctx[i], NULL) < 0)
		{
			printf("error: avformat_write_header\n");
			break;
		}
	}

	av_dump_format(ifmt_ctx, -1, input_filepath, 0);
	for (int i = 0; i < screens; ++i)
		av_dump_format(ofmt_ctx[i], -1, output_filepath[i], 1);

	printf("fmt: %d %d\n", codec_ctx_video->pix_fmt, AV_PIX_FMT_YUV420P);

	AVFrame  oframe_yuv = { 0 };
	//oframe_yuv.width = codec_ctx_video->width;
	//oframe_yuv.height = codec_ctx_video->height;
	//oframe_yuv.width = w_max - w_min + 1;
	//oframe_yuv.height = h_max - h_min + 1;
	oframe_yuv.width = codec_ctx_2h264[0]->width;
	oframe_yuv.height = codec_ctx_2h264[0]->height;
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

		//if (0 == ipkt.pts)
		//{
		//  continue;
		//}

		if (ipkt.stream_index == idx_video)
		{
			ret = avcodec_send_packet(codec_ctx_video, &ipkt);
			if (ret < 0) {
				puts("avcodec_send_packet ERROR");
				av_packet_unref(&ipkt);
				break;
			}
			ret = avcodec_receive_frame(codec_ctx_video, &iframe);
			if (ret == AVERROR(EAGAIN)) {
				av_packet_unref(&ipkt);
				continue;
			}
			if (ret < 0) {
				puts("avcodec_receive_frame ERROR");
				av_packet_unref(&ipkt);
				break;
			}
			
			for (int i = 0; i < screens; ++i) {
				framecut(&oframe_yuv , &iframe, codec_ctx_video->width / screens * i, (codec_ctx_video->height >> 1) - (codec_ctx_2h264[i]->height >> 1), codec_ctx_video->width / screens * (i+1)-1, (codec_ctx_video->height >> 1) + (codec_ctx_2h264[i]->height >> 1) - 1, codec_ctx_video->pix_fmt);
				
				oframe_yuv.pts = ipkt.pts;
				oframe_yuv.width = codec_ctx_2h264[i]->width;
				oframe_yuv.height = codec_ctx_2h264[i]->height;
				while (1) {
				ret = avcodec_send_frame(codec_ctx_2h264[i], &oframe_yuv);
				if (ret < 0) {
					printf("send_frame failed %d %d %d %d %d\n", ret, AVERROR(EAGAIN), AVERROR_EOF, AVERROR(EINVAL), AVERROR(ENOMEM));
					av_packet_unref(&ipkt);
					break;
				}
				ret = avcodec_receive_packet(codec_ctx_2h264[i], &opkt);
				if (ret == AVERROR(EAGAIN)) {
					//av_packet_unref(&ipkt);
					continue;
				};
				break;
				
				if (ret < 0) {
					printf("receive_packet failed %d\n", ret);
					av_packet_unref(&ipkt);
					break;
				}
				}

				opkt.pts = av_rescale_q_rnd(ipkt.pts, istream_video->time_base, ostream_video[i]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				opkt.dts = av_rescale_q_rnd(ipkt.dts, istream_video->time_base, ostream_video[i]->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				//printf("PTS: %lld   DTS: %lld\n", opkt.pts, opkt.dts);
				opkt.duration = av_rescale_q(ipkt.duration, istream_video->time_base, ostream_video[i]->time_base);
			
				opkt.pos = -1;
				opkt.stream_index = output_idx_video;

				av_interleaved_write_frame(ofmt_ctx[i], &opkt);
				av_packet_unref(&opkt);
			}
			
			++video_count;
			if ((video_count & 15) == 0) printf("video_count = %d\n", video_count);
		}
		else
		{
			;
		}
		av_packet_unref(&ipkt);
	}

	for (int i = 0; i < screens; ++i)
		if (0 != av_write_trailer(ofmt_ctx[i]))
		{
			printf("error: av_write_trailer\n");
			break;
		}		
}
	avformat_close_input(&ifmt_ctx);

	av_free(buffer_yuv);
	sws_freeContext(img_convert_ctx_2yuv);

	for (int i = 0; i < screens; ++i) {
		avcodec_free_context(&codec_ctx_2h264[i]);

		if (NULL != ofmt_ctx[i])
		avio_close(ofmt_ctx[i]->pb);

		avformat_free_context(ofmt_ctx[i]);
	}
    	
	return 0;
}
