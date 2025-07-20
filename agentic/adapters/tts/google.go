package tts

import (
	"context"
	"fmt"
	"os"
	"text/tabwriter"

	texttospeech "cloud.google.com/go/texttospeech/apiv1"
	"cloud.google.com/go/texttospeech/apiv1/texttospeechpb"
)

type GoogleTTS struct {
	client        *texttospeech.Client
	VoiceName     string
	LanguageCode  string
	AudioEncoding texttospeechpb.AudioEncoding
	SampleRate    int32
}

type GoogleTTSConfig struct {
	VoiceName     string
	LanguageCode  string
	AudioEncoding string // now a string, not pb type
	SampleRate    int32
}

func mapAudioEncoding(enc string) texttospeechpb.AudioEncoding {
	switch enc {
	case "LINEAR16":
		return texttospeechpb.AudioEncoding_LINEAR16
		// ...existing code...
	case "MULAW":
		return texttospeechpb.AudioEncoding_MULAW
	case "ALAW":
		return texttospeechpb.AudioEncoding_ALAW
	default:
		return texttospeechpb.AudioEncoding_LINEAR16
	}
}

func NewGoogleTTS(cfg GoogleTTSConfig) *GoogleTTS {
	client, err := texttospeech.NewClient(context.Background())
	if err != nil {
		panic(fmt.Errorf("creating Google tts client: %w", err))
	}

	resp, err := client.ListVoices(context.Background(), &texttospeechpb.ListVoicesRequest{
		LanguageCode: cfg.LanguageCode,
	})
	if err != nil {
		panic(fmt.Errorf("listing voices: %w", err))
	}

	w := tabwriter.NewWriter(os.Stdout, 0, 0, 1, ' ', 0)
	for _, voice := range resp.Voices {
		// Display the voice's name. Example: tpc-vocoded
		fmt.Fprintf(w, "Name: %v\n", voice.Name)

		for _, languageCode := range voice.LanguageCodes {
			fmt.Fprintf(w, "  Supported language: %v\n", languageCode)
		}
		fmt.Fprintf(w, "  SSML Voice Gender: %v\n", voice.SsmlGender.String())
		fmt.Fprintf(w, "  Natural Sample Rate Hertz: %v\n",
			voice.NaturalSampleRateHertz)
	}
	w.Flush()

	return &GoogleTTS{
		client:        client,
		VoiceName:     cfg.VoiceName,
		LanguageCode:  cfg.LanguageCode,
		AudioEncoding: mapAudioEncoding(cfg.AudioEncoding),
		SampleRate:    cfg.SampleRate,
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
			LanguageCode: g.LanguageCode,
			Name:         g.VoiceName,
			SsmlGender:   texttospeechpb.SsmlVoiceGender_MALE,
		},
		AudioConfig: &texttospeechpb.AudioConfig{
			AudioEncoding:   g.AudioEncoding,
			SampleRateHertz: g.SampleRate,
		},
	}
	resp, err := g.client.SynthesizeSpeech(ctx, &req)
	if err != nil {
		return nil, fmt.Errorf("synthesizing speech: %w", err)
	}

	return resp.GetAudioContent(), nil
}

// StreamingSynthesize uses Google Cloud TTS streaming synthesis for real-time audio streaming
func (g *GoogleTTS) StreamingSynthesize(ctx context.Context, text string) (<-chan []byte, error) {
	audioChan := make(chan []byte, 10)

	go func() {
		defer close(audioChan)

		// Start streaming synthesis
		stream, err := g.client.StreamingSynthesize(ctx)
		if err != nil {
			fmt.Printf("❌ Failed to start streaming synthesis: %v\n", err)
			select {
			case audioChan <- nil:
			case <-ctx.Done():
				return
			}
			return
		}
		defer stream.CloseSend()

		// Try Chirp3-HD with ALAW first, fallback to Standard with LINEAR16 if needed
		config := texttospeechpb.StreamingSynthesizeRequest{
			StreamingRequest: &texttospeechpb.StreamingSynthesizeRequest_StreamingConfig{
				StreamingConfig: &texttospeechpb.StreamingSynthesizeConfig{
					Voice: &texttospeechpb.VoiceSelectionParams{
						LanguageCode: g.LanguageCode,
						Name:         g.VoiceName,
						SsmlGender:   texttospeechpb.SsmlVoiceGender_FEMALE,
					},
					StreamingAudioConfig: &texttospeechpb.StreamingAudioConfig{
						AudioEncoding:   g.AudioEncoding,
						SampleRateHertz: g.SampleRate,
					},
				},
			},
		}

		if err := stream.Send(&config); err != nil {
			fmt.Printf("❌ Failed to send fallback config: %v\n", err)
			select {
			case audioChan <- nil:
			case <-ctx.Done():
				return
			}
			return
		}

		// Send the input text
		inputReq := texttospeechpb.StreamingSynthesizeRequest{
			StreamingRequest: &texttospeechpb.StreamingSynthesizeRequest_Input{
				Input: &texttospeechpb.StreamingSynthesisInput{
					InputSource: &texttospeechpb.StreamingSynthesisInput_Text{
						Text: text,
					},
				},
			},
		}

		if err := stream.Send(&inputReq); err != nil {
			fmt.Printf("❌ Failed to send input: %v\n", err)
			select {
			case audioChan <- nil:
			case <-ctx.Done():
				return
			}
			return
		}

		// Receive streaming audio chunks
		for {
			resp, err := stream.Recv()
			if err != nil {
				fmt.Printf("❌ Failed to receive audio chunk: %v\n", err)
				// Check if it's end of stream
				if err.Error() == "rpc error: code = Unknown desc = EOF" {
					break
				}
				// Send error indicator
				select {
				case audioChan <- nil:
				case <-ctx.Done():
					return
				}
				return
			}

			// Send audio chunk through channel
			audioContent := resp.GetAudioContent()
			if len(audioContent) > 0 {
				select {
				case audioChan <- audioContent:
				case <-ctx.Done():
					return
				}
			}
		}
	}()

	return audioChan, nil
}

// TestChirpHDStreamingConfig tests different configurations for Chirp3-HD streaming
func (g *GoogleTTS) TestChirpHDStreamingConfig(ctx context.Context) {
	voiceName := "id-ID-Chirp3-HD-Achernar"
	testText := "Halo, ini adalah tes suara."

	// Test configurations
	configs := []struct {
		name          string
		audioEncoding texttospeechpb.AudioEncoding
		sampleRate    int32
		description   string
	}{
		{
			name:          "LINEAR16-24000",
			audioEncoding: texttospeechpb.AudioEncoding_LINEAR16,
			sampleRate:    24000,
			description:   "LINEAR16 at natural sample rate",
		},
		{
			name:          "LINEAR16-16000",
			audioEncoding: texttospeechpb.AudioEncoding_LINEAR16,
			sampleRate:    16000,
			description:   "LINEAR16 at 16kHz",
		},
		{
			name:          "MULAW-8000",
			audioEncoding: texttospeechpb.AudioEncoding_MULAW,
			sampleRate:    8000,
			description:   "MULAW at 8kHz",
		},
		{
			name:          "ALAW-8000",
			audioEncoding: texttospeechpb.AudioEncoding_ALAW,
			sampleRate:    8000,
			description:   "ALAW at 8kHz",
		},
	}

	fmt.Printf("🧪 Testing Chirp3-HD streaming configurations for voice: %s\n", voiceName)
	fmt.Printf("📝 Test text: %s\n\n", testText)

	for _, config := range configs {
		fmt.Printf("🔬 Testing: %s (%s)\n", config.name, config.description)

		// Test streaming synthesis
		stream, err := g.client.StreamingSynthesize(ctx)
		if err != nil {
			fmt.Printf("❌ Failed to create streaming client: %v\n", err)
			continue
		}

		// Send configuration
		streamConfig := texttospeechpb.StreamingSynthesizeRequest{
			StreamingRequest: &texttospeechpb.StreamingSynthesizeRequest_StreamingConfig{
				StreamingConfig: &texttospeechpb.StreamingSynthesizeConfig{
					Voice: &texttospeechpb.VoiceSelectionParams{
						LanguageCode: "id-ID",
						Name:         voiceName,
						SsmlGender:   texttospeechpb.SsmlVoiceGender_FEMALE,
					},
					StreamingAudioConfig: &texttospeechpb.StreamingAudioConfig{
						AudioEncoding:   config.audioEncoding,
						SampleRateHertz: config.sampleRate,
					},
				},
			},
		}

		if err := stream.Send(&streamConfig); err != nil {
			fmt.Printf("❌ Failed to send config: %v\n", err)
			stream.CloseSend()
			continue
		}

		// Send input text
		inputReq := texttospeechpb.StreamingSynthesizeRequest{
			StreamingRequest: &texttospeechpb.StreamingSynthesizeRequest_Input{
				Input: &texttospeechpb.StreamingSynthesisInput{
					InputSource: &texttospeechpb.StreamingSynthesisInput_Text{
						Text: testText,
					},
				},
			},
		}

		if err := stream.Send(&inputReq); err != nil {
			fmt.Printf("❌ Failed to send input: %v\n", err)
			stream.CloseSend()
			continue
		}

		// Try to receive response
		resp, err := stream.Recv()
		if err != nil {
			fmt.Printf("❌ Failed: %v\n", err)
		} else {
			audioContent := resp.GetAudioContent()
			fmt.Printf("✅ SUCCESS! Received %d bytes of audio\n", len(audioContent))
		}

		stream.CloseSend()
		fmt.Println()
	}
}
