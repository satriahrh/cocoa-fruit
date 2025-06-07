package tts

import (
	"context"
	"fmt"

	texttospeech "cloud.google.com/go/texttospeech/apiv1"
	"cloud.google.com/go/texttospeech/apiv1/texttospeechpb"
)

type GoogleTTS struct {
	client *texttospeech.Client
}

func NewGoogleTTS() *GoogleTTS {
	client, err := texttospeech.NewClient(context.Background())
	if err != nil {
		panic(fmt.Errorf("creating Google tts client: %w", err))
	}
	return &GoogleTTS{
		client: client,
	}
}

func (g *GoogleTTS) Synthesize(ctx context.Context, text string) ([]byte, error) {
	req := texttospeechpb.SynthesizeSpeechRequest{
		Input: &texttospeechpb.SynthesisInput{
			InputSource: &texttospeechpb.SynthesisInput_Text{
				Text: text,
			},
		},
		Voice: &texttospeechpb.VoiceSelectionParams{
			LanguageCode: "id-ID",
			SsmlGender:   texttospeechpb.SsmlVoiceGender_MALE,
		},
		AudioConfig: &texttospeechpb.AudioConfig{
			AudioEncoding: texttospeechpb.AudioEncoding_MP3,
		},
	}
	resp, err := g.client.SynthesizeSpeech(ctx, &req)
	if err != nil {
		return nil, fmt.Errorf("synthesizing speech: %w", err)
	}

	return resp.GetAudioContent(), nil
}
