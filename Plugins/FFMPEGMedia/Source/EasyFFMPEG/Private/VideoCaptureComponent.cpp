// Fill out your copyright notice in the Description page of Project Settings.


#include "VideoCaptureComponent.h"

#include "EasyFFMPEG.h"
#include "Engine/GameEngine.h"
#include "HAL/FileManager.h"

//#include "Android/AndroidPlatformFile.h"

#include "Kismet/GameplayStatics.h"

#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#endif

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/opt.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
#include "libavutil/error.h"

#if PLATFORM_ANDROID
#include "libavcodec/jni.h"
#endif
}

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "IAssetViewport.h"
#endif
#include <Core.h>

char* err1 = new char[50];
// Sets default values for this component's properties
UVideoCaptureComponent::UVideoCaptureComponent()
	: CaptureState(EMovieCaptureState::NotInit)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	// ...
}


void UVideoCaptureComponent::StartCapture(const FString& InVideoFilename)
{
	ShouldCutFrameCount = 0;
	CapturedFrameNumber = 0;


	//--------------------------------------my add code-------------------------------------
	//av_register_all();
	//avcodec_register_all();
	VideoFilename = InVideoFilename;

#if PLATFORM_ANDROID

	FString fileName = "";
	VideoFilename.Split("../", NULL, &fileName, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	VideoFilename = FPaths::ProjectPersistentDownloadDir() + "/UnrealGame/" + FApp::GetProjectName() + "/" + fileName;//可以返回根目录！！！
	// = FPaths::ConvertRelativePathToFull(VideoFilename);//返回原本的相对路径
	//tempStr1 = FPaths::ProjectSavedDir()+"videos/"+ fileName;//返回相对路径
	//tempStr1 = FPaths::ProjectContentDir() + "videos/" + fileName;//返回相对路径
#endif

	//--------------------------------------my add code end---------------------------------

	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC == nullptr) {
		UE_LOG(LogFFmpeg, Warning, TEXT("Can not found player controller."));
		return;
	}

	if (IsInitialized()) {
		UE_LOG(LogFFmpeg, Warning, TEXT("Please uninitialize capture component before call InitCapture()."));
		return;
	}

	FIntPoint ViewportSize;
	PC->GetViewportSize(ViewportSize.X, ViewportSize.Y);

	if (!InitFrameGrabber(ViewportSize)) {
		UE_LOG(LogFFmpeg, Error, TEXT("Init frame grabber failed."));
		return;
	}

	

	if (!CreateVideoFileWriter()) {
		UE_LOG(LogFFmpeg, Error, TEXT("Cant create the video file '%s'."), *VideoFilename);
		StopCapture();
		return;
	}

	DestroyVideoFileWriter();

	//-----add
	//FString f3 = "file";
	//char * c1 = "file";
	//FormatCtx->protocol_whitelist = c1; //TCHAR_TO_ANSI(*f3);
	//----------

	int32 result = avformat_alloc_output_context2(&FormatCtx, nullptr, nullptr, TCHAR_TO_UTF8(*VideoFilename));
	if (result < 0) {
		UE_LOG(LogFFmpeg, Error, TEXT("Can not allocate format context."));
		StopCapture();
		return;
	}
	

	Codec = avcodec_find_encoder(FormatCtx->oformat->video_codec);
	if (Codec == nullptr) {
		UE_LOG(LogFFmpeg, Error, TEXT("Codec not found."));
		StopCapture();
		return;
	}
	//
	Stream = avformat_new_stream(FormatCtx, Codec);
	if (Stream == nullptr) {
		UE_LOG(LogFFmpeg, Error, TEXT("Can not allocate a new stream."));
		StopCapture();
		return;
	}

	CodecCtx = avcodec_alloc_context3(Codec);
	if (Codec == nullptr) {
		UE_LOG(LogFFmpeg, Error, TEXT("InitCapture() failed: Cloud not allocate video codec context."));
		StopCapture();
		return;
	}


	Stream->codecpar->codec_id = FormatCtx->oformat->video_codec;
	Stream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
	Stream->codecpar->width = ViewportSize.X;
	Stream->codecpar->height = ViewportSize.Y;
	Stream->codecpar->format =  AV_PIX_FMT_YUV420P;//AV_PIX_FMT_BGRA
	Stream->codecpar->bit_rate = (int64_t)CaptureConfigs.BitRate * (int64_t)800;
	
	avcodec_parameters_to_context(CodecCtx, Stream->codecpar);
	
	CodecCtx->time_base = { CaptureConfigs.FrameRate.Y, CaptureConfigs.FrameRate.X };
	CodecCtx->framerate = { CaptureConfigs.FrameRate.X, CaptureConfigs.FrameRate.Y };
	CodecCtx->gop_size = CaptureConfigs.GopSize;
	CodecCtx->max_b_frames = CaptureConfigs.MaxBFrames;
	CodecCtx->bit_rate = (int64_t)CaptureConfigs.BitRate * (int64_t)800;

	if (Stream->codecpar->codec_id == AV_CODEC_ID_H264) {//AV_CODEC_ID_MPEG4
		av_opt_set(CodecCtx, "preset", "slow", 0);
	}
	
	//FString f2 = FString:: *Stream->codecpar->codec_id;
	//UE_LOG(LogFFmpeg, Error, TEXT("Stream->codecpar->codec_id: '%s'."), (Stream->codecpar->codec_id));


	result = avcodec_open2(CodecCtx, Codec, nullptr);
	if (result < 0) {
		UE_LOG(LogFFmpeg, Error, TEXT("Could not open codec."));
		av_make_error_string(err1, 50, result);
		FString tempStr2(err1);
		UE_LOG(LogFFmpeg, Error, TEXT("avcodec_open2 err: '%s'."), *tempStr2);
		StopCapture();
		return;
	}

	avcodec_parameters_from_context(Stream->codecpar, CodecCtx);

	av_dump_format(FormatCtx, 0, TCHAR_TO_UTF8(*VideoFilename), 1);
	
	//-----------------------start my test code------------------------------------
	//视频文件是否存在
	f1 = (FPaths::FileExists(VideoFilename) ? TEXT("true") : TEXT("false"));
	UE_LOG(LogFFmpeg, Error, TEXT("mp4 File exist? '%s'."),*f1);
	//-----------------------my test code end--------------------------------------


	
	
	UE_LOG(LogFFmpeg, Error, TEXT("Full video path: '%s'."), *VideoFilename);

	//transpose
	//result = av_dict_set_int(&Stream->metadata,"rotate",180,0);
	//result=av_dict_set(&Stream->metadata, "rotate", "180", 0);
	result = av_dict_set_int(&Stream->metadata, "rotate", 0, 0);
	result = av_dict_set_int(&Stream->metadata, "transpose", 0, 0);
	if(result<0){ 
		UE_LOG(LogFFmpeg, Error, TEXT("SetVideoRotateErr: '%s'."), *(FString::FromInt(result))); 
	}
	

	result = avio_open(&FormatCtx->pb, TCHAR_TO_UTF8(*(VideoFilename)), AVIO_FLAG_WRITE);
	if (result < 0) {

		//char* err1 = new char[50];
		av_make_error_string(err1, 50, result);
		FString tempStr2(err1);
		UE_LOG(LogFFmpeg, Error, TEXT("avio_open err: '%s'."), *tempStr2);
		UE_LOG(LogFFmpeg, Error, TEXT("avio_open Return: '%s'."), *(FString::FromInt(result)));
		UE_LOG(LogFFmpeg, Error, TEXT("Cant open the file '%s'."), *VideoFilename);
		
		StopCapture();
		return;
	}
	//打印metadata
	AVDictionaryEntry* m = NULL;
	while ((m = av_dict_get(Stream->metadata, "", m, AV_DICT_IGNORE_SUFFIX)) != NULL)
	{
		UE_LOG(LogFFmpeg, Error, TEXT("22222222====Key:%s ===value:%s"), *FString(m->key), *FString(m->value));
	}
	//AVDictionaryEntry* m = NULL;
	//while ((m = av_dict_get(Stream->metadata, "", m, AV_DICT_IGNORE_SUFFIX)) != NULL) {

	//		printf("22222222====Key:%s ===value:%s\n", m->key, m->value);

	//	}

	result = avformat_write_header(FormatCtx, nullptr);
	if (result < 0) {
		UE_LOG(LogFFmpeg, Error, TEXT("Error ocurred when write header into file."));
		StopCapture();
		return;
	}

	Packet = av_packet_alloc();
	if (Packet == nullptr) {
		UE_LOG(LogFFmpeg, Error, TEXT("InitCapture() failed: Cloud not allocate packet."));
		StopCapture();
		return;
	}

	Frame = av_frame_alloc();
	if (Frame == nullptr) {
		UE_LOG(LogFFmpeg, Error, TEXT("Cloud not allocate video frame."));
		StopCapture();
		return;
	}

	Frame->format = CodecCtx->pix_fmt;
	Frame->width = CodecCtx->width;
	Frame->height = CodecCtx->height;

	result = av_frame_get_buffer(Frame, 0);
	if (result < 0) {
		UE_LOG(LogFFmpeg, Error, TEXT("Cloud not allocate the video frame data."));
		StopCapture();
		return;
	}

	FrameTimeForCapture = FTimespan::FromSeconds(CaptureConfigs.FrameRate.Y / (CaptureConfigs.FrameRate.X * 1.0f));
	PassedTime = FrameTimeForCapture;
	//打印metadata
	//AVDictionaryEntry* m = NULL;
	while ((m = av_dict_get(FormatCtx->metadata, "", m, AV_DICT_IGNORE_SUFFIX)) != NULL)
	{
		UE_LOG(LogFFmpeg, Error, TEXT("333333==FormatCtx====Key:%s ===value:%s"), *FString(m->key), *FString(m->value));
	}
	CaptureState = EMovieCaptureState::Initialized;
}

bool UVideoCaptureComponent::IsInitialized()
{
	return CaptureState >= EMovieCaptureState::Initialized;
}

//截取帧
bool UVideoCaptureComponent::CaptureThisFrame(int32 CurrentFrame)
{
	if (CaptureState == EMovieCaptureState::NotInit || !FrameGrabber.IsValid()) {
		return false;
	}

	FrameGrabber->CaptureThisFrame(FFramePayloadPtr());
	TArray<FCapturedFrameData> frames = FrameGrabber->GetCapturedFrames();
	if (!frames.IsValidIndex(0)) {
		if (CurrentFrame == 0) {
			ShouldCutFrameCount++;
		}
		return false;
	}

	if (ShouldCutFrameCount > 1) {
		ShouldCutFrameCount--;
		return true;
	}

	FCapturedFrameData& lastFrame = frames.Last();

	WriteFrameToFile(lastFrame.ColorBuffer, CurrentFrame);

	return true;
}

void UVideoCaptureComponent::StopCapture()
{

	ReleaseFrameGrabber();
	DestroyVideoFileWriter();
	ReleaseContext();


	CaptureState = EMovieCaptureState::NotInit;
}

// Called when the game starts
void UVideoCaptureComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void UVideoCaptureComponent::BeginDestroy()
{
	Super::BeginDestroy();
	StopCapture();
}

bool UVideoCaptureComponent::CreateVideoFileWriter()
{
	if (IFileManager::Get().FileExists(*VideoFilename)) {
		IFileManager::Get().Delete(*VideoFilename);
	}

	Writer = IFileManager::Get().CreateFileWriter(*VideoFilename, EFileWrite::FILEWRITE_Append);
	if (Writer == nullptr) {
		return false;
	}

	return true;
}

void UVideoCaptureComponent::DestroyVideoFileWriter()
{
	if (Writer == nullptr) {
		return;
	}

	Writer->Flush();
	Writer->Close();

	delete Writer;
	Writer = nullptr;
}

bool UVideoCaptureComponent::InitFrameGrabber(const FIntPoint& ViewportSize)
{
	if (FrameGrabber.IsValid()) {
		return true;
	}

	TSharedPtr<FSceneViewport> sceneViewport;

#if WITH_EDITOR
	if (GIsEditor)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE)
			{
				FSlatePlayInEditorInfo* SlatePlayInEditorSession = GEditor->SlatePlayInEditorMap.Find(Context.ContextHandle);
				if (SlatePlayInEditorSession)
				{
					if (SlatePlayInEditorSession->DestinationSlateViewport.IsValid())
					{
						TSharedPtr<IAssetViewport> DestinationLevelViewport = SlatePlayInEditorSession->DestinationSlateViewport.Pin();
						sceneViewport = DestinationLevelViewport->GetSharedActiveViewport();
					}
					else if (SlatePlayInEditorSession->SlatePlayInEditorWindowViewport.IsValid())
					{
						sceneViewport = SlatePlayInEditorSession->SlatePlayInEditorWindowViewport;
					}
				}
			}
		}
	}
	else
#endif
	{
		UGameEngine* gameEngine = Cast<UGameEngine>(GEngine);
		if (gameEngine == nullptr || !gameEngine->SceneViewport.IsValid()) {
			return false;
		}

		sceneViewport = gameEngine->SceneViewport;
	}
	
	if (!sceneViewport) {
		return false;
	}
	
	FrameGrabber = MakeShareable(new FFrameGrabber(sceneViewport.ToSharedRef(), ViewportSize));
	FrameGrabber->StartCapturingFrames();

	return true;
}

void UVideoCaptureComponent::ReleaseFrameGrabber()
{
	if (FrameGrabber.IsValid()){
		FrameGrabber->StopCapturingFrames();
		FrameGrabber->Shutdown();
		FrameGrabber.Reset();
	}
}


void UVideoCaptureComponent::ReleaseContext()
{
	if (CaptureState != EMovieCaptureState::NotInit) {
		EncodeVideoFrame(CodecCtx, nullptr, Packet);

		av_write_trailer(FormatCtx);
	}

	if (CodecCtx != nullptr) {
		avcodec_free_context(&CodecCtx);
		CodecCtx = nullptr;
	}

	if (FormatCtx != nullptr) {
		if (FormatCtx->pb != nullptr) {
			avio_close(FormatCtx->pb);
		}
		avformat_free_context(FormatCtx);
		FormatCtx = nullptr;
	}

	if (Frame != nullptr) {
		av_frame_free(&Frame);
		Frame = nullptr;
	}

	if (Packet != nullptr) {
		av_packet_free(&Packet);
		Packet = nullptr;
	}
}

void UVideoCaptureComponent::WriteFrameToFile(const TArray<FColor>& ColorBuffer, int32 CurrentFrame)
{
	AVFrame* rgbFrame = av_frame_alloc();
	if (rgbFrame == nullptr) {
		return;
	}
	
	
	//UE_LOG(LogFFmpeg, Error, TEXT("=====ColorBuffer长度:%s =====width:%s =====height:%s"), 
	//	*FString::FromInt(ColorBuffer.Num()), 
	//	*FString::FromInt(CodecCtx->width),
	//	*FString::FromInt(CodecCtx->height));

	TArray <FColor> TempBuffer = ColorBuffer;
	TArray <FColor> TempBuffer2 = TempBuffer;
	enum AVPixelFormat pf = AV_PIX_FMT_BGRA;
#if PLATFORM_ANDROID

	//修复android中录制后的镜像反转异常
	size_t count = TempBuffer.Num();
	size_t w = CodecCtx->width;
	size_t h = CodecCtx->height;
	for (size_t i = 0; i < TempBuffer.Num(); i++)
	{
		TempBuffer[i] = ColorBuffer[count - i-1];
	}
	
	for (size_t n = 0; n < w; n++)
	{
		for (size_t m = 0; m < h; m++)
		{
			TempBuffer2[w * m + n] = TempBuffer[w * m - n + w - 1];
		}
	}


	//修复android中录制后的颜色R，B通道反转异常
	pf = AV_PIX_FMT_RGBA;
	//pf = AV_PIX_FMT_YUV420P;
	
#endif // PLATFORM_ANDROID


	avpicture_fill((AVPicture*)rgbFrame, (uint8_t*)TempBuffer2.GetData(), pf, CodecCtx->width, CodecCtx->height);

	SwsContext* scaleCtx = sws_getContext(CodecCtx->width, CodecCtx->height, pf,
										  CodecCtx->width, CodecCtx->height, CodecCtx->pix_fmt, SWS_BILINEAR, nullptr, nullptr, nullptr);
	if (scaleCtx == nullptr) {
		av_frame_free(&rgbFrame);
		return;
	}

	int32 result = sws_scale(scaleCtx, rgbFrame->data, rgbFrame->linesize, 0, CodecCtx->height, Frame->data, Frame->linesize);
	//av_free(&pBGRFrame);
	av_frame_free(&rgbFrame);
	sws_freeContext(scaleCtx);

	Frame->pts = CurrentFrame;

	if (result != CodecCtx->height) {
		return;
	}

	EncodeVideoFrame(CodecCtx, Frame, Packet);
}

void UVideoCaptureComponent::EncodeVideoFrame(struct AVCodecContext* InCodecCtx, struct AVFrame* InFrame, struct AVPacket* InPacket)
{
	//avcodec_parameters_to_context(InCodecCtx, st->codecpar);
	int32 result = avcodec_send_frame(InCodecCtx, InFrame);
	if (result < 0) {
		UE_LOG(LogFFmpeg, Error, TEXT("Error sending a frame for encoding."));
		
		av_make_error_string(err1, 50, result);
		FString tempStr2(err1);
		UE_LOG(LogFFmpeg, Error, TEXT("avcodec_send_frame Return: '%s'."), *tempStr2);
		return;
	}

	while(result >= 0)
	{

		InPacket->data = nullptr;
		InPacket->size = 0;

		result = avcodec_receive_packet(InCodecCtx, InPacket);
		if (result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
			return;
		}
		else if (result < 0) {
			UE_LOG(LogFFmpeg, Error, TEXT("Error during encoding."));
			return;
		}

		AVRational tempRational;
		tempRational.num = CaptureConfigs.FrameRate.Y;
		tempRational.den = CaptureConfigs.FrameRate.X;

		av_packet_rescale_ts(InPacket, tempRational, Stream->time_base);

		InPacket->stream_index = Stream->index;

		result = av_interleaved_write_frame(FormatCtx, InPacket);
		if (result < 0) {
			UE_LOG(LogFFmpeg, Error, TEXT("Error during interleaved write frame."));
			return;
		}

		av_packet_unref(InPacket);
	}
}

void UVideoCaptureComponent::ReadTextFile()
{
}

// Called every frame
void UVideoCaptureComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	if (!IsInitialized()) {
		return;
	}

	PassedTime += FTimespan::FromSeconds(DeltaTime);
	
	if (PassedTime >= FrameTimeForCapture)
	{
		CaptureThisFrame(CapturedFrameNumber++);
		PassedTime = FTimespan::Zero();
	}
}
