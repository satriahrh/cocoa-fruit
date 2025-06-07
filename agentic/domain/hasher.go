package domain

// Hasher is the core port for any hashing strategy.
type Hasher interface {
	Hash(data []byte) string
}
