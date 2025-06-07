package hasher

import (
	"crypto/sha256"
	"encoding/hex"

	"github.com/satriahrh/cocoa-fruit/agentic/domain"
)

// New returns a domain.Hasher backed by SHA‑256.
func New() domain.Hasher { return sha256Hasher{} }

type sha256Hasher struct{}

func (h sha256Hasher) Hash(data []byte) string {
	sum := sha256.Sum256(data)
	return hex.EncodeToString(sum[:])
}
