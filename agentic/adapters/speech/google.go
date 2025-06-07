package speech

import (
	"context"
	"fmt"

	speech "cloud.google.com/go/speech/apiv1"
	"cloud.google.com/go/speech/apiv1/speechpb"
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
