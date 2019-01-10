package osp

// TODO: 
// - Read messages as well, and more than one

import (
	"context"
)

func SendMessageAsClient(ctx context.Context, hostname string, port int, msg interface{}) error {
	session, err := DialAsQuicClient(ctx, hostname, port)
	if err != nil {
		return err
	}
	stream, err := session.OpenStreamSync()
	if err != nil {
		return err
	}
	err = WriteMessage(msg, stream)
	if err != nil {
		return err
	}
	return nil
}