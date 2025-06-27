package speech

import (
	"context"
	"encoding/base64"
	"fmt"

	speech "cloud.google.com/go/speech/apiv1"
	"cloud.google.com/go/speech/apiv1/speechpb"
	"github.com/satriahrh/cocoa-fruit/agentic/utils/log"
	"go.uber.org/zap"
)

type GoogleSpeech struct {
	client *speech.Client
}

func NewGoogleSpeech() *GoogleSpeech {
	client, err := speech.NewClient(context.Background())
	if err != nil {
		panic(fmt.Errorf("creating Google speech client: %w", err))
	}
	return &GoogleSpeech{
		client: client,
	}
}

// TranscribeAudio converts base64 audio data to text
func (g *GoogleSpeech) TranscribeAudio(ctx context.Context, base64Audio string) (string, error) {
	log.WithCtx(ctx).Info("🎤 Starting audio transcription",
		zap.Int("base64_length", len(base64Audio)))

	// Decode base64 audio
	audioData, err := base64.StdEncoding.DecodeString(base64Audio)
	if err != nil {
		log.WithCtx(ctx).Error("❌ Failed to decode base64 audio", zap.Error(err))
		return "", fmt.Errorf("decoding base64 audio: %w", err)
	}

	log.WithCtx(ctx).Info("🔍 Base64 decoded successfully",
		zap.Int("audio_bytes", len(audioData)))

	// Create recognition config
	config := &speechpb.RecognitionConfig{
		Encoding:          speechpb.RecognitionConfig_LINEAR16,
		SampleRateHertz:   16000,
		LanguageCode:      "id-ID",
		AudioChannelCount: 1, // Mono
	}

	log.WithCtx(ctx).Info("⚙️ Created recognition config",
		zap.String("encoding", "LINEAR16"),
		zap.Int("sample_rate", 16000),
		zap.String("language", "id-ID"),
		zap.Int("channels", 1))

	// Create recognition audio
	audio := &speechpb.RecognitionAudio{
		AudioSource: &speechpb.RecognitionAudio_Content{
			Content: audioData,
		},
	}

	// Perform recognition
	log.WithCtx(ctx).Info("🚀 Sending request to Google Speech-to-Text API")
	resp, err := g.client.Recognize(ctx, &speechpb.RecognizeRequest{
		Config: config,
		Audio:  audio,
	})
	if err != nil {
		log.WithCtx(ctx).Error("❌ Google Speech-to-Text API error", zap.Error(err))
		return "", fmt.Errorf("recognizing audio: %w", err)
	}

	log.WithCtx(ctx).Info("✅ Google Speech-to-Text API response received",
		zap.Int("results_count", len(resp.Results)), zap.Any("resp", resp))

	// Extract transcript
	var transcript string
	for i, result := range resp.Results {
		log.WithCtx(ctx).Info("🔍 Processing result",
			zap.Int("result_index", i),
			zap.Int("alternatives_count", len(result.Alternatives)))

		for j, alt := range result.Alternatives {
			if alt.Transcript != "" {
				transcript = alt.Transcript
				log.WithCtx(ctx).Info("✅ Found transcript",
					zap.Int("result_index", i),
					zap.Int("alternative_index", j),
					zap.String("transcript", transcript))
				break
			}
		}
		if transcript != "" {
			break
		}
	}

	if transcript == "" {
		log.WithCtx(ctx).Warn("⚠️ No transcript found in API response")
	} else {
		log.WithCtx(ctx).Info("🎉 Transcription completed successfully",
			zap.String("transcript", transcript))
	}

	return transcript, nil
}

func (g *GoogleSpeech) TranscribeStreaming(ctx context.Context) error {
	streamingClient, err := g.client.StreamingRecognize(ctx)
	if err != nil {
		return fmt.Errorf("creating streaming client: %w", err)
	}

	err = streamingClient.Send(&speechpb.StreamingRecognizeRequest{
		StreamingRequest: &speechpb.StreamingRecognizeRequest_StreamingConfig{
			StreamingConfig: &speechpb.StreamingRecognitionConfig{
				Config: &speechpb.RecognitionConfig{
					Encoding:        speechpb.RecognitionConfig_LINEAR16,
					SampleRateHertz: 16000,
					LanguageCode:    "id-ID",
				},
			},
		},
	})
	if err != nil {
		return fmt.Errorf("sending streaming config: %w", err)
	}

	panic("not implemented")
}
