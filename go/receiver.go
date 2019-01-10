package osp

// TODO:
// - Send a response message
// - Make a nice object API with methods
// - Make it possible to have a presentation receiver that is a client

import (
  "context"
	"crypto/tls"
)

func RunPresentationReceiver(ctx context.Context, mdnsInstanceName string, port int, cert tls.Certificate, presentUrl func(string)) {
	messages := make(chan interface{})
	go ReadMessagesAsServer(ctx, mdnsInstanceName, port, cert, messages)
	for msg := range messages {
		switch m := msg.(type) {
		case PresentationStartRequest:
		  presentUrl(m.URL)
		}
	}
}