package log

import (
	"context"
	"os"

	"go.uber.org/zap"
)

var logger *zap.Logger

func init() {
	if os.Getenv("DEBUG") == "true" {
		logger, _ = zap.NewDevelopment()
	} else {
		logger, _ = zap.NewProduction()
	}
}

func WithCtx(ctx context.Context) *zap.Logger {
	fields := []zap.Field{}

	if v := ctx.Value("device_version"); v != nil {
		fields = append(fields, zap.Any("device_version", v))
	}
	if v := ctx.Value("device_id"); v != nil {
		fields = append(fields, zap.Any("device_id", v))
	}
	if v := ctx.Value("user_id"); v != nil {
		fields = append(fields, zap.Any("user_id", v))
	}

	return logger.With(fields...)
}

func With(fields ...zap.Field) *zap.Logger {
	return logger.With(fields...)
}
