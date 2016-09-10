#include "AugmentedReality.h"
#include "WebcamReader.h"

// Конструктор
AWebcamReader::AWebcamReader()
{
	// Tick будет вызывать каждый кадр
	PrimaryActorTick.bCanEverTick = true;

	// Инициализация Opencv и настроек камеры
	CameraID = 0;
	RefreshRate = 15;
	isStreamOpen = false;
	VideoSize = FVector2D(0, 0);
	ShouldResize = false;
	ResizeDeminsions = FVector2D(1280, 720);
	stream = new cv::VideoCapture();
	frame = new cv::Mat();
	RefreshTimer = 1000000.0f;
	
	//Переменные для BlurDetection
	threshold = 100;
}

AWebcamReader::~AWebcamReader()
{
	FMemory::Free(stream);
	FMemory::Free(frame);
	FMemory::Free(size);
}

// Вызывается в начале игры
void AWebcamReader::BeginPlay()
{
	Super::BeginPlay();

	stream->open(CameraID);
	if (stream->isOpened())
	{
		isStreamOpen = true;
		UpdateFrame();
		VideoSize = FVector2D(frame->cols, frame->rows);
		ResizeDeminsions = GetGameViewportSize();
		size = new cv::Size(ResizeDeminsions.X, ResizeDeminsions.Y);
		VideoTexture = UTexture2D::CreateTransient(VideoSize.X, VideoSize.Y);
		VideoTexture->UpdateResource();
		VideoUpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, VideoSize.X, VideoSize.Y);

		Data.Init(FColor(0, 0, 0, 255), VideoSize.X * VideoSize.Y);
	}
}

FVector2D AWebcamReader::GetGameViewportSize()
{
	FVector2D Result = FVector2D(1, 1);

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize( /*out*/Result);
	}
	UE_LOG(LogTemp, Warning, TEXT("My Viewport: %s"), *Result.ToString());
	return Result;
}

// Вызывается каждый кадр
void AWebcamReader::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshTimer += DeltaTime;
	if (isStreamOpen && RefreshTimer >= 1.0f / RefreshRate)
	{
		RefreshTimer -= 1.0f / RefreshRate;
		UpdateFrame();
		handleFrame();
		UpdateTexture();
		OnNextVideoFrame();
	}
}

//Фильтр размытых кадров
bool AWebcamReader::blurDetection(cv::Mat image, int threshold) 
{	
	cv::Mat laplacianImage;
	cv::Laplacian(image, laplacianImage, CV_64F);
	cv::Scalar mu, sigma;
	cv::meanStdDev(laplacianImage, mu, sigma);
	double focusMeasure = sigma.val[0] * sigma.val[0];

	bool detected = false;
	if (focusMeasure < threshold) {
		std::string text = "Image is blurred (value: " + std::to_string(focusMeasure) + " )";
		cv::putText(image, text, cvPoint(10, 30), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cvScalar(255, 255, 255), 1, CV_AA);
		detected = true;
	}
	return detected;
}

//LSD
double* AWebcamReader::DoLSD(cv::Mat image)
{
	//Конвертация изображения для последующего применения алгоритма LSD
	cv::Mat grayscaleMat(image.size(), CV_8U);
	cv::cvtColor(image, grayscaleMat, CV_BGR2GRAY);
	double* pgm_image;
	pgm_image = (double *)malloc(image.cols * image.rows * sizeof(double));
	for (int y = 0; y < image.rows; y++)
	{
		for (int x = 0; x < image.cols; x++)
		{
			int i = x + (y * image.cols);
			pgm_image[i] = double(image.data[i]);
		}
	}
	//Применение LSD
	int numLinesDetected;
	double* outArray;
	outArray = lsd(&numLinesDetected, pgm_image, image.cols, image.rows);
	return outArray;
}

//Вызывается каждый кадр после UpdateFrame()
void AWebcamReader::handleFrame() 
{
	if (!blurDetection(*frame, threshold)) 
	{	
		double* out = DoLSD(*frame);
	}
}

void AWebcamReader::UpdateFrame()
{
	if (isStreamOpen)
	{
		stream->read(*frame);
		if (ShouldResize)
		{
			cv::resize(*frame, *frame, *size);
		}
	}
}

void AWebcamReader::UpdateTexture()
{
	if (isStreamOpen && frame->data)
	{
		for (int y = 0; y < VideoSize.Y; y++)
		{
			for (int x = 0; x < VideoSize.X; x++)
			{
				int i = x + (y * VideoSize.X);
				Data[i].B = frame->data[i * 3 + 0];
				Data[i].G = frame->data[i * 3 + 1];
				Data[i].R = frame->data[i * 3 + 2];
			}
		}

		UpdateTextureRegions(VideoTexture, (int32)0, (uint32)1, VideoUpdateTextureRegion, (uint32)(4 * VideoSize.X), (uint32)4, (uint8*)Data.GetData(), false);
	}
}

void AWebcamReader::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
	if (Texture->Resource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*, RegionData, RegionData,
			bool, bFreeData, bFreeData,
			{
			for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
			{
				int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
				if (RegionData->MipIndex >= CurrentFirstMip)
				{
					RHIUpdateTexture2D(
						RegionData->Texture2DResource->GetTexture2DRHI(),
						RegionData->MipIndex - CurrentFirstMip,
						RegionData->Regions[RegionIndex],
						RegionData->SrcPitch,
						RegionData->SrcData
						+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
						+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
						);
				}
			}
			if (bFreeData)
			{
				FMemory::Free(RegionData->Regions);
				FMemory::Free(RegionData->SrcData);
			}
			delete RegionData;
		});
	}
}

void AWebcamReader::OnNextVideoFrame_Implementation()
{
	// No default implementation
}
