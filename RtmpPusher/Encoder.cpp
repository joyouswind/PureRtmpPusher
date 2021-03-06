#include "Encoder.h"


Encoder::Encoder(int width, int height, int frameRate, H264EncodedCallBack callBack)
{
	m_h264CallBack = callBack;
	m_avCodecContext = get_qsv_context(width, height, frameRate);
}

Encoder::~Encoder()
{
	if (m_avCodecContext != nullptr)
	{
		avcodec_free_context(&m_avCodecContext);
		m_avCodecContext = nullptr;
	}
}

void Encoder::encode_frame(AVFrame* frame)
{
	// send frame to encode queue;
	if (avcodec_send_frame(m_avCodecContext, frame) == 0)
	{
		int eof = AVERROR(AVERROR_EOF);
		while (true)
		{
			AVPacket* avPacket = ImgUtility::create_packet();
			//receive packet from encode queue.
			int ret = avcodec_receive_packet(m_avCodecContext, avPacket);
			if (ret == eof || ret < 0)
			{
				//编码器可能会接收多个输入帧/数据包而不返回帧，直到其内部缓冲区被填充为止。
				av_packet_unref(avPacket);
				break;
			}
			else
			{
				m_h264CallBack(avPacket);
				av_packet_unref(avPacket);
			}
		}
	}
}

AVCodecContext* Encoder::get_qsv_context(int width, int height, int frameRate)
{
	avdevice_register_all();
	AVCodec* codec = avcodec_find_encoder_by_name("h264_qsv");
	if (codec == nullptr)
	{
		return nullptr;
	}
	AVCodecContext* codecContext = avcodec_alloc_context3(codec);
	if (codecContext == nullptr)
	{
		return nullptr;
	}

	codecContext->width = width; //Width
	codecContext->height = height; //Height
	codecContext->time_base = { 1, frameRate };  //frames per second
	codecContext->pix_fmt = *codec->pix_fmts;
	//av_opt_set(codecContext->priv_data, "profile", "main", 0);
	codecContext->bit_rate = get_bitrate(width, height, frameRate);// put sample parameters   
	codecContext->gop_size = frameRate * 2; //   
	codecContext->max_b_frames = 1; //B frames

	//av_dict_set(&options, "profile", "baseline", 0);

	if (avcodec_open2(codecContext, codec, nullptr) < 0)
	{
		avcodec_free_context(&codecContext);
		return nullptr;
	}
	return codecContext;
}

AVCodecContext* Encoder::get_h264_context(int width, int height, int frameRate)
{
	AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (codec == nullptr)
	{
		return nullptr;
	}
	AVCodecContext* codecContext = avcodec_alloc_context3(codec);
	if (codecContext == nullptr)
	{
		return nullptr;
	}

	codecContext->width = width; //Width
	codecContext->height = height; //Height
	codecContext->time_base = { 1, frameRate };  //frames per second
	codecContext->thread_count = 2; //Thread used.
	codecContext->pix_fmt = *codec->pix_fmts;//AV_PIX_FMT_YUV420P; //YUV420 for H264
	codecContext->bit_rate = get_bitrate(width, height, frameRate);// put sample parameters   
	codecContext->gop_size = frameRate * 2; // emit one intra frame every ten frames   
	codecContext->max_b_frames = 1; //B frames
	//Set parameters
	AVDictionary* options = nullptr;
	//基本画质。支持I/P 帧，只支持无交错（Progressive）和CAVLC
	av_dict_set(&options, "profile", "baseline", 0);
	//低质量，快编码速度
	av_dict_set(&options, "preset", "fast", 0);

	if (avcodec_open2(codecContext, codec, &options) < 0)
	{
		av_dict_free(&options);
		avcodec_free_context(&codecContext);
		return nullptr;
	}
	return codecContext;

}

int Encoder::get_bitrate(int width, int height, int frameRate)
{
	if (width == 0 || height == 0 || frameRate == 0)
	{
		return  LIVESTREAM_BITRATE_720P;
	}

	int bitrate = LIVESTREAM_BITRATE_720P * width / 1280;
	bitrate = bitrate * height / 720;
	if (frameRate >= 15)
	{
		bitrate = bitrate * frameRate / 20;
	}
	else if (frameRate < 15)
	{
		bitrate = bitrate * 15 / 20;
	}

	return bitrate;
}